#include "toast/crash.hpp"
#include "toast/logger.hpp"
#include "toast/splash.hpp"
#include "toast/environment.hpp"
#include "toast/memory.hpp"

#ifdef OS_WIN
    #include "compat/win32/backtrace.hpp"
    #include <windows.h>
#else
    #include "compat/unix/backtrace.hpp"
    #include <typeinfo>
#endif

using namespace Toast;
using namespace std;

static Logger __logger("Toast-Crash");
static void (*shutdown_ptr)() = NULL;

static void shutdown() {
    if (shutdown_ptr != NULL) shutdown_ptr();
    // abort();
    exit(-1);
}

static void catch_fatal_signal(int sig) {
    Crash::on_signal(sig);
}

void Crash::initialize() {
    signal(SIGFPE, catch_fatal_signal);
    signal(SIGILL, catch_fatal_signal);
    signal(SIGSEGV, catch_fatal_signal);
    #ifndef OS_WIN
        signal(SIGBUS, catch_fatal_signal);
        signal(SIGSYS, catch_fatal_signal);
    #endif
}

void Crash::on_known(std::exception e) {
    string type(typeid(e).name());
    string msg(e.what());
    
    Crash::handle_exception(type, msg);
}

void Crash::on_known(const char *e) {
    string type = "<string>";
    string msg(e);
    
    Crash::handle_exception(type, msg);
}

void Crash::on_unknown() {
    string type = "<unknown type>";
    string msg = "<no message>";
    
    Crash::handle_exception(type, msg);
}

void Crash::on_signal(int sigid) {
    string type = "<signal>";
    string msg;
    
    if (sigid == SIGFPE) msg = "SIGFPE (Fatal Arithmetic Error)";
    else if (sigid == SIGILL) msg = "SIGILL (Illegal Instruction)";
    else if (sigid == SIGSEGV) msg = "SIGSEGV (Segmentation Violation)";
    #ifndef OS_WIN
        else if (sigid == SIGBUS) msg = "SIGBUS (Invalid Dereference)";
        else if (sigid == SIGSYS) msg = "SIGSYS (Bad System Call)";
    #endif
    
    Crash::handle_exception(type, msg);
}

void Crash::on_shutdown(void (*arg)()) {
    shutdown_ptr = arg;
}

void Crash::handle_exception(string type, string msg) {
    __logger.raw("\n**** CRASH LOG ****");
    __logger.raw(Splash::get_error_splash() + "\n");
    __logger.raw("Your robot has crashed. Following is a crash log and more details.");
    
    __logger.raw("\nException Info: ");
    __logger.raw("\tType: " + type);
    __logger.raw("\tMessage: " + msg);
    
    __logger.raw("\nBacktrace:");
    vector<string> backtrace = backtrace_get(2);
    for (auto i : backtrace) {
        __logger.raw("\t" + i);
    }
    
    // __logger.raw("\nModules:");
    // for (auto module : Loader::modules()) {
    //     __logger.raw("\t" + module->info->name);
    //     __logger.raw("\t\tName: " + module->info->name);
    //     __logger.raw("\t\tUnique: " + module->info->unique);
    //     __logger.raw("\t\tFile: " + module->filepath);
    // }
    
    // __logger.raw("\nModule Files:");
    // for (auto module : Loader::module_files()) {
    //     __logger.raw("\t" + module);
    // }
    
    // __logger.raw("\nLibrary Files:");
    // for (auto library : Loader::library_files()) {
    //     __logger.raw("\t" + library);
    // }
    
    // TODO file coredump of shared pool
    // TODO contain module unique IDs in the shared pool, then use that here
    // to get module information.
    
    __logger.raw("\n*******************");
    shutdown();
}