#include "toast/net/socket.hpp"
#include "toast/bootstrap.hpp"
#include "toast/bootstrap/loader.hpp"
#include "toast/bootstrap/driver.hpp"
#include "toast/bootstrap/state.hpp"
#include "toast/bootstrap/server.hpp"
#include "toast/memory.hpp"
#include "toast/filesystem.hpp"
#include "toast/logger.hpp"
#include "toast/splash.hpp"
#include "toast/environment.hpp"
#include "toast/state.hpp"
#include "toast/util.hpp"
#include "toast/crash.hpp"
#include "toast/config.hpp"
#include "toast/ipc.hpp"

#include "toast/bootstrap/utils/log.hpp"

#include "toast/provider.hpp"

using namespace Toast;

Logger _b_log("Toast");
Bootstrap::BootConfig _b_cfg;

static DYNAMIC dyn;
static std::string provider = "toast_hardware_provider";

template <class ret = void>
static ret thp_dynamic_call(DYNAMIC dyn, std::string name) {
	if (dyn != NULL) {
		SYMBOL sym = Internal::Loader::get_symbol(dyn, name);
		if (sym == NULL) return static_cast<ret>(NULL);
		return reinterpret_cast<ret(*)()>(sym)();
	}
	// This has to be a static cast, or else when ret=void, this does `return NULL` on a void function,
	// resulting in a compilation error.
	return static_cast<ret>(NULL);
}

void bootstrap_shutdown() {
	_b_log << "Bootstrap Stopped. Freeing Resources.";

	thp_dynamic_call(dyn, "provider_free");
	Log::flush();
	Log::close();
	Toast::Bootstrap::Loader::free();
	Toast::Memory::free_memory(true);

	// Just kill it. WPILib holds on too long.
	#ifndef ROBORIO
		exit(Crash::has_crashed() ? 1 : 0);
	#else
		std::quick_exit(Crash::has_crashed() ? 1 : 0);
	#endif
}

void init_toast_bootstrap(int argc, char *argv[]) {
	// Argument Parsing
	bool loop = true, load = true, load_driver = true;
	for (int i = 0; i < argc; i++) {
		char *a = argv[i];
		char *b = (i != argc - 1) ? b = argv[i + 1] : NULL;

		if (strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0) {
			cout << "toast_launcher [options]\n\n"
				"\t--no-loop\t Exit Toast immediately after initialization\n"
				"\t--no-driver\t Do not load Toast Bootstrap Drivers\n"
				"\t--no-load\t Do not load Toast Modules\n"
				"\t--provider <provider name>\t Select a hardware provider to use\n"
				"\n"
				"\t--util <utility> [utility options]\t Load a Toast Utility (does not load robot code)\n"
				"\t\tlog2csv \t<files> [-o OUTPUT]\tConvert .tlog (or a directory of .tlog) files to CSV files. Default output to directory containing log file.\n"
				"\t\tlog_combine \t<files> [-o OUTPUT]\tCombine .tlog (or a directory of .tlog) files into one file, sorting messages by time. Default output to ./combined.csv\n";
			return;
		}
		else if ((strcmp(a, "--provider") == 0 || strcmp(a, "-p") == 0) && b != NULL) {
			i++;
			provider = std::string(b);
		}
		else if (strcmp(a, "--no-loop") == 0) loop = false;
		else if (strcmp(a, "--no-driver") == 0) load_driver = false;
		else if (strcmp(a, "--no-load") == 0) load = false;
		else if (strcmp(a, "--util") == 0 && b != nullptr) {
			// Toast Runtime Utility, skip normal loading.
			i++;
			if (strncmp(b, "log2", 4) == 0) {
				BootstrapUtil::Log::log2something(argc, argv, i);
			} else if (strcmp(b, "log_combine") == 0) {
				BootstrapUtil::Log::combine_logs(argc, argv, i);
			} else {
				cerr << "[ERR] You wanted to use a utility, but we can't find the one you're looking for!" << endl;
			}
			return;
		}
	}
	// End Argument Parsing

    long long start_time = current_time_millis();
	std::string prov_library = Internal::Loader::library_name(provider);
	dyn = Internal::Loader::load_dynamic_library(prov_library);
	if (dyn == NULL) {
		if (Filesystem::exists(prov_library)) {
			cerr << "Toast Hardware Provider Library " << prov_library << " could not be loaded! Falling back..." << endl;
			cerr << Internal::Loader::get_dynamic_error() << endl;
		}
		for (std::string s : Filesystem::ls_local("./")) {
			std::string name = Filesystem::name(s);
			if (starts_with(name, "provider_") || starts_with(name, "libprovider_")) {
				dyn = Internal::Loader::load_dynamic_library(name);
				if (dyn != NULL) break;
			}
		}

		if (dyn == NULL) {
			cerr << "ERROR: Toast Hardware Provider not present! Toast Cannot Run!" << endl;
			exit(-1);
		}
	}

    ProviderInfo *info = thp_dynamic_call<ProviderInfo *>(dyn, "provider_info");
	Crash::on_shutdown(bootstrap_shutdown);
    
    thp_dynamic_call(dyn, "provider_preinit");
    
    Net::Socket::socket_init();
    Filesystem::initialize();
    Memory::initialize_bootstrap();
	IPC::start(-1);
    
    Log::initialize("Bootstrap");
    _b_log.raw(Splash::get_startup_splash() + "\n");
    _b_log.raw("Toast Loaded on OS: [" + Environment::OS::to_string() + "] with Process ID [" + std::to_string(get_pid()) + "]");
    _b_log.raw("Hardware Provider: " + std::string(info->name));
	_b_cfg.load();
	Bootstrap::Web::prepare();
    
    // State Tick Timing (100Hz by default)
    int tick_frequency = (int)(1000.0 / _b_cfg.timings.states.frequency);
    States::Internal::set_tick_timing(tick_frequency);

	thp_dynamic_call(dyn, "provider_predriver");
	if (load_driver) {
		_b_log << "Initializing Driver Loader";
		Bootstrap::Driver::initialize();
		Bootstrap::Driver::preinit_drivers();
	}
    
	thp_dynamic_call(dyn, "provider_preload");
	if (load) {
		_b_log << "Initializing Loader";
		Bootstrap::Loader::initialize();
	}

    Bootstrap::States::init();
	Bootstrap::Web::start();

	thp_dynamic_call(dyn, "provider_init");

	if (load_driver) {
		Bootstrap::Driver::init_drivers();
	}
    long long end_time = current_time_millis();
    _b_log << "Total Bootstrap Startup Time: " + to_string(end_time - start_time) + "ms";

    if (loop) thp_dynamic_call(dyn, "provider_loop");
	bootstrap_shutdown();
}

Bootstrap::BootConfig *Bootstrap::get_config() {
	return &_b_cfg;
}

Toast::Logger *Bootstrap::get_logger() {
	return &_b_log;
}

DYNAMIC *Bootstrap::get_provider() {
	return &dyn;
}
