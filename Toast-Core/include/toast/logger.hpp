#pragma once

#include "toast/library.hpp"
#include "toast/util.hpp"
#include "toast/net/transport.hpp"

#include <string>
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <time.h>

namespace Toast {
    namespace Log {
        API typedef struct {
            std::string name;
            bool is_error;
            bool is_debug;
        } Level;
        
        API Level INFO();
        API Level WARN();
        API Level ERR();
        API Level SEVERE();
        API Level DEBUG();
        
        API void initialize(std::string process_name);
        API void set_debug(bool is_debug);
        API void flush();
        API void close();
        
        API std::string file();
        API void copyTo(std::string path);
        
        API void log(std::string name, std::string msg, Level level);
		API void log_raw(std::string sender, std::string level, std::string raw, std::string console, bool error, bool debug);
        API void info(std::string name, std::string msg);
        API void debug(std::string name, std::string msg);
    }
    
    class Logger {
    public:
        API Logger(std::string name);
        
        API void log(std::string msg, Log::Level level);
        API void info(std::string msg);
        API void debug(std::string msg);
        API void warn(std::string msg);
        API void error(std::string msg);
        API void severe(std::string msg);
        API void raw(std::string msg, bool error, bool debug);
        API void raw(std::string msg);
        
        API Logger& operator<<(std::string msg);

        API void set_name(std::string name);
        API std::string get_name();
    
    private:
        std::string name;
    };
}