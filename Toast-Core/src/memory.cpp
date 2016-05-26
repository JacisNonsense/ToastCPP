#include "toast/memory.hpp"

using namespace Toast;
using namespace std;

static SHM_HANDLE __shm_handle_shared;
static char *__shared_block;

void Memory::initialize_bootstrap() {
    __shm_handle_shared = Internal::SHM::create_shm_file(TOAST_SHARED_MEMPOOL_NAME, TOAST_SHARED_MEMPOOL_SIZE);
    __shared_block = Internal::SHM::map_shm_file(__shm_handle_shared, TOAST_SHARED_MEMPOOL_SIZE);

    Memory::Shared::zero();
}

void Memory::initialize() {
    __shm_handle_shared = Toast::Internal::SHM::create_shm_file(TOAST_SHARED_MEMPOOL_NAME, TOAST_SHARED_MEMPOOL_SIZE);
    __shared_block = Toast::Internal::SHM::map_shm_file(__shm_handle_shared, TOAST_SHARED_MEMPOOL_SIZE);
}

// -- SHARED MEMORY BLOCK STUFF -- //

void Memory::Shared::zero() {
    memset(__shared_block, 0, TOAST_SHARED_MEMPOOL_SIZE);
}

char *Memory::Shared::get() {
    return __shared_block;
}

void Memory::Shared::set_debug(bool is_debug) {
    __shared_block[0x01] = is_debug ? 0x01 : 0x00;
}

bool Memory::Shared::get_debug() {
    return __shared_block[0x01] == 0x01 ? true : false;
}

// -- BRIDGED MEMORY STUFF -- //

Memory::Bridge::Bridge(string name, int size) {
    _name = name;
    _size = size;
}

void Memory::Bridge::create() {
    _handle = Toast::Internal::SHM::create_shm_file(_name, _size);
    _block = Toast::Internal::SHM::map_shm_file(_handle, _size);
}

void Memory::Bridge::open() {
    _handle = Toast::Internal::SHM::open_shm_file(_name);
    _block = Toast::Internal::SHM::map_shm_file(_handle, _size);
}

char *Memory::Bridge::get() {
    return _block;
}

void Memory::Bridge::destroy() {
    Toast::Internal::SHM::unmap_shm_file(_block, _size);
    Toast::Internal::SHM::close_shm_file(_name, _handle);
}

void Memory::Bridge::zero() {
    memset(_block, 0, _size);
}