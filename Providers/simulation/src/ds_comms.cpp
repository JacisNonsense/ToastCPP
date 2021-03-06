#include "toast/net/socket.hpp"
#include "toast/net/util.hpp"

#include "thp/sim/ds_comms.hpp"
#include "thp/sim/sim_provider.hpp"

#include "toast/memory.hpp"
#include "toast/state.hpp"
#include "toast/concurrent/mutex.hpp"
#include "toast/util.hpp"
#include "toast/crash.hpp"
#include "toast/logger.hpp"

#include <thread>
#include <iostream>
#include <stdio.h>

using namespace Sim;
using namespace Toast::Memory;

static char encode_buffer[8];
static char decode_buffer[1024];
static char tcp_buffer[1024];
static char tcp_write_buffer[1024];
static char mdns_payload[1024];
static bool mdns_payload_init = false;
static int mdns_payload_len;

static Toast::Concurrent::Mutex mtx;
static bool server_running;

static std::thread udp_thread, tcp_thread, mdns_thread;

static Toast::Logger logger("Driver Station Comms");

static void init_mdns_payload() {
	if (mdns_payload_init) return;
	std::string service_name = get_simulation_config()->mdns.service_name;
	int snl = service_name.size();

	std::string target_host_name = get_simulation_config()->mdns.host_name;
	int thnl = target_host_name.size();

	std::string ip_address = get_simulation_config()->mdns.ip_address;
	unsigned char *ip = new unsigned char[4];
	Toast::Net::Util::get_ip(ip_address, ip);

	unsigned char payload_1[] = {
		0x00, 0x00, 0x84, 0x00,		// ID, Response Query
		0x00, 0x00, 0x00, 0x03,		// No Question, 3 Answers
		0x00, 0x00, 0x00, 0x01,		// No Authority, 1 Additional RR
	};
	unsigned char payload_2[] = {
		// Record 1: PTR
		0x03,							// Len: 3
		0x5f, 0x6e, 0x69,				// _ni
		0x04,							// Len: 4
		0x5f, 0x74, 0x63, 0x70,			// _tcp
		0x05,							// Len: 5
		0x6c, 0x6f, 0x63, 0x61, 0x6c,	// local
		0x00,							// end of string

		0x00, 0x0c, 0x80, 0x01,			// Type: PTR (domain name PoinTeR), Class: IN, Cache flush: true
		0x00, 0x00, 0x00, 0x3C,			// TTL: 60 Sec
		0x00, (unsigned char)(0x03 + snl),		// Data Length: 3 + snl
		(unsigned char)snl						// Name Length: snl
	};
	char *payload_3 = (char *)service_name.c_str();

	unsigned char payload_4[] = {
		0xc0, 0x0c,					// Name Offset (0xc0, 0x0c => 12 =>._ni._tcp.local)

		// Record 2: SRV
		0xc0, 0x26, 0x00, 0x21,		// Name Offset (mdns.service_name), Type: SRV (Server Selection)
		0x80, 0x01,					// Class: IN, Cache flush: true
		0x00, 0x00, 0x00, 0x3C,		// TTL: 60 sec
		0x00, (unsigned char)(0xE + thnl),	// Data Length: 14 + thnl
		0x00, 0x00, 0x00, 0x00,		// Priority: 0, Weight: 0
		0x0d, 0xfc,					// Port: 3580
		(unsigned char)thnl					// Len: thnl
	};
	char *payload_5 = (char *)target_host_name.c_str();

	unsigned char payload_6[] = {
		0x05,							// Len: 5
		0x6c, 0x6f, 0x63, 0x61, 0x6c,	// local
		0x00,							// end of string

		// Record 3: TXT
		0xc0, 0x26, 0x00, 0x10,		// Name Offset (mdns.service_name), Type: TXT
		0x80, 0x01,					// Class: IN, Cache flush: true
		0x00, 0x00, 0x00, 0x3C,		// TTL: 60 sec
		0x00, 0x01, 0x00,			// Data Length: 1, TXT Length: 0

		// Additional Record: A
		0xc0, (unsigned char)(0x3b + snl),	// Name Offset (mdns.target_host_name)
		0x00, 0x01, 0x80, 0x01,		// Type: A, Class: IN, Cache flush: true
		0x00, 0x00, 0x00, 0x3C,		// TTL: 60 sec
		0x00, 0x04,					// Data Length: 4
		ip[0], ip[1], ip[2], ip[3]	// IP Bytes
	};

	memcpy(&mdns_payload[0], payload_1, 12);
	memcpy(&mdns_payload[12], payload_2, 27);
	memcpy(&mdns_payload[39], payload_3, snl);
	memcpy(&mdns_payload[39 + snl], payload_4, 21);
	memcpy(&mdns_payload[60 + snl], payload_5, thnl);
	memcpy(&mdns_payload[60 + snl + thnl], payload_6, 36);

	mdns_payload_len = 96 + snl + thnl;
	mdns_payload_init = true;

	logger << "Initialized mDNS. IP: " + Toast::Net::Util::ip_to_string(ip);

	delete[] ip;
}

static void mdns_broadcast_func() {
	init_mdns_payload();

	Toast::Net::Socket::DatagramSocket sock(5353);
	sock.bind();

	Toast::Net::Socket::SocketAddress addr(get_simulation_config()->mdns.broadcast_ip, 5353);
	while (true) {
		mtx.lock();
		if (!server_running)
			break;
		mtx.unlock();

		sock.send(mdns_payload, mdns_payload_len, &addr);

		sleep_ms(5000);
	}
	sock.close();
}

static void tcp_thread_func() {
	Toast::Net::Socket::ServerSocket sock(1740);
	sock.open();
	
	while (true) {
		Toast::Net::Socket::ClientSocket client = sock.accept();
		int ret = 1;
		while (ret > 0) {
			ret = client.read(tcp_buffer, 8192);
			if (ret > 0) {
				DriverStationComms::decode_ds_tcp_packet(tcp_buffer, ret);
			}
		}
		client.close();
		sleep_ms(100);
	}

	sock.close();
}

static void udp_thread_func() {
	Toast::Net::Socket::DatagramSocket sock(1110);
	sock.bind();
	
	Toast::Net::Socket::SocketAddress addr;
	while (true) {
		mtx.lock();
		if (!server_running)
			break;
		mtx.unlock();

		int len = sock.read(decode_buffer, 1024, &addr);
		DriverStationComms::decode_ds_packet(decode_buffer, len);
		
		DriverStationComms::encode_ds_packet(encode_buffer);
		addr.set_port(1150);
		sock.send(encode_buffer, 8, &addr);
	}
	sock.close();
}

void DriverStationComms::start() {
	if (!server_running) {
		Toast::Net::Socket::socket_init();

		mtx.lock();
		server_running = true;
		mtx.unlock();
		
		udp_thread = std::thread(udp_thread_func);
		udp_thread.detach();

		mdns_thread = std::thread(mdns_broadcast_func);
		mdns_thread.detach();

		tcp_thread = std::thread(tcp_thread_func);
		tcp_thread.detach();
	}
}

void DriverStationComms::stop() {
	mtx.lock();
	server_running = false;
	mtx.unlock();
}

typedef struct {
	uint8_t axis_count, pov_count, button_count;
	uint16_t pov[4];
	uint8_t axis[16];
	uint32_t button_mask;
	bool has_update;
} _TempJoyData;

static uint8_t sq_1 = 0, sq_2 = 0, control = 0, req = 0;
static _TempJoyData joys[6];
static long long last_decode_time;
static double bat_voltage;

void DriverStationComms::decode_ds_packet(char *data, int length) {
	sq_1 = data[0]; sq_2 = data[1];
	if (data[2] != 0) {
		control = data[3];
		RobotState state = RobotState::DISABLED;
		bool estop = FWI_IS_BIT_SET(control, 7);	// TODO Write Estop
		bool fms = FWI_IS_BIT_SET(control, 3);

		if (FWI_IS_BIT_SET(control, 2)) {
			if (FWI_IS_BIT_SET(control, 0)) {
				state = RobotState::TEST;
			} else {
				if (FWI_IS_BIT_SET(control, 1)) {
					state = RobotState::AUTO;
				} else {
					state = RobotState::TELEOP;
				}
			}
		}

		req = data[4];
		bool reboot = FWI_IS_BIT_SET(req, 3);
		bool restart = FWI_IS_BIT_SET(req, 2);

		if (reboot || restart) {
			logger << "NOTICE: Driver Station Requested Code Restart";
			Toast::Crash::shutdown();
		}

		int alliance_position = data[5] % 3 + 1;
		Shared::DS::Alliance alliance = data[5] < 3 ? Shared::DS::Alliance::Red : Shared::DS::Alliance::Blue;

		int i = 6;
		bool search = true;
		int joy_id = 0;

		while (i < length && search) {
			int struct_size = data[i];
			search = data[i + 1] == 0x0c;
			if (!search) continue;

			_TempJoyData *joy = &joys[joy_id];

			joy->axis_count = data[i + 2];
			for (int ax = 0; ax < joy->axis_count; ax++) {
				joy->axis[ax] = data[i + 2 + ax + 1];
			}

			int b = i + 2 + joy->axis_count + 1;

			joy->button_count = data[b];
			int button_delta = (joy->button_count / 8 + ((joy->button_count % 8 == 0) ? 0 : 1));
			uint32_t total_mask = 0;
			for (int bm = 0; bm < button_delta; bm++) {
				uint8_t m = data[b + bm + 1];
				total_mask = (total_mask << (bm * 8)) | m;
			}
			joy->button_mask = total_mask;

			b += button_delta + 1;

			joy->pov_count = data[b];
			for (int pv = 0; pv < joy->pov_count; pv++) {
				uint8_t a1 = data[b + 1 + (pv * 2)];
				uint8_t a2 = data[b + 1 + (pv * 2) + 1];
				if (a2 < 0) a2 = 256 + a2;
				joy->pov[pv] = (uint16_t)(a1 << 8 | a2);
			}

			joy->has_update = true;
			joy_id++;
			i += struct_size + 1;
		}

		// Write it all to the shared pool
		Toast::States::Internal::set_state(state);

		MTX_LOCK(shared_mutex()->ds, 0);
		shared()->ds_info()->set_alliance(alliance);
		shared()->ds_info()->set_alliance_station(alliance_position);
		shared()->ds_info()->set_fms_attached(fms);
		MTX_UNLOCK(shared_mutex()->ds, 0);

		last_decode_time = current_time_millis();
	}
}

void DriverStationComms::encode_ds_packet(char *data) {
	data[0] = sq_1; data[1] = sq_2;
	data[2] = 0x01;
	data[3] = control;
	data[4] = 0x10 | 0x20;
	
	double voltage = bat_voltage;

	data[5] = (uint8_t)(voltage);
	data[6] = (uint8_t)((voltage * 100 - ((uint8_t)voltage) * 100) * 2.5);
	data[7] = 0;
}

void DriverStationComms::decode_ds_tcp_packet(char *data, int length) {
	if (data[2] == 0x02) {
		// Joystick Descriptor
		int i = 3;
		while (i < length) {
			uint8_t joyid = data[i++];
			bool xbox = data[i++] == 1;
			uint8_t type = data[i++];
			uint8_t name_length = data[i++];
			int nb_i = i;
			i += name_length;
			uint8_t axis_count = data[i++];
			uint8_t axis_types[16];
			int at_i = i;
			i += axis_count;
			uint8_t button_count = data[i++];
			uint8_t pov_count = data[i++];

			if (type != 255 && axis_count != 255) {
				MTX_LOCK(shared_mutex()->joy, joyid);
				Shared::DS::JoystickDescriptor *j = shared()->joystick(joyid)->get_descriptor();
				j->set_is_xbox(xbox);
				j->set_type((Shared::DS::JoystickType) type);
				if (name_length > 60) name_length = 60;
				j->set_name_length(name_length);
				memcpy(j->get_name(), &data[nb_i], name_length);
				j->set_axis_count(axis_count);
				for (int x = 0; x < axis_count; x++) {
					j->set_axis_type(x, (Shared::DS::JoystickAxisType) data[at_i + x]);
				}
				MTX_UNLOCK(shared_mutex()->joy, joyid);
			}
		}
	}
}
bool last = false;

void DriverStationComms::periodic_update() {
	MTX_LOCK(shared_mutex()->ds, 0);
	if (current_time_millis() - last_decode_time > 1000) {
		// DS Disconnected
		shared()->ds_info()->set_ds_attached(false);
	} else {
		// DS Connected
		shared()->ds_info()->set_ds_attached(true);
	}
	MTX_UNLOCK(shared_mutex()->ds, 0);

	for (int i = 0; i < 6; i++) {
		MTX_LOCK(shared_mutex()->joy, i);
		Shared::DS::Joystick *j = shared()->joystick(i);
		_TempJoyData *t = &joys[i];

		j->set_num_axis(t->axis_count);
		j->set_num_button(t->button_count);
		j->set_num_pov(t->pov_count);

		j->set_button_mask(t->button_mask);

		for (int x = 0; x < j->get_num_axis(); x++) {
			j->set_axis(x, t->axis[x]);
		}

		for (int x = 0; x < j->get_num_pov(); x++) {
			j->set_pov(x, t->pov[x]);
		}

		t->has_update = false;
		MTX_UNLOCK(shared_mutex()->joy, i);
	}

	MTX_LOCK(shared_mutex()->power, 0);
	bat_voltage = shared()->power()->get_pdp_voltage();
	MTX_UNLOCK(shared_mutex()->power, 0);
}
