#pragma once

#include <string.h>
#include "toast/library.hpp"
#include "toast/state.hpp"
#include "toast/memory.hpp"

#include <vector>
#include <string>

#define MODULE_CLASS(NAME, RESTART, MODULE_CLASS)              \
        CAPI Toast::ModuleInfo *_get_module_information() {         \
            Toast::ModuleInfo *info = new Toast::ModuleInfo();      \
            strcpy(info->name, NAME);                               \
            info->restartable = RESTART;                            \
            return info;                                            \
        }                                                           \
                                                                    \
        CAPI Toast::Module *_allocate_module_instance() {           \
            return new MODULE_CLASS();                              \
        }                                                           \

namespace Toast {
    API typedef struct {
        char name[50];
        bool restartable;
    } ModuleInfo;
    
    API typedef struct {
        std::string name;
        std::string file;
        int module_idx;
        Memory::ModuleActState status;
    } ModuleData;
    
    class Module {
        public:
            API virtual void construct() {};
            API virtual void start() {};
    };
    
    class IterativeModule : public Module, public IterativeBase {
        public:
            API virtual void construct() {
                States::register_iterative((IterativeBase *)this);
            }
    };
    
    API std::vector<ModuleData> get_all_modules();
	API int module_id(std::string name);
}