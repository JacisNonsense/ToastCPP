#pragma once

#include <string>
#include <vector>
#include "toast/library.hpp"
#include <string.h>

namespace Toast {
	namespace Filesystem {
		API std::string toast_dir();
		API std::string user_dir();
		API std::string join(std::string path1, std::string path2);
		API std::string path(std::string path);
		API std::string path_mkdir(std::string path);
		API std::string absolute(std::string path);
		API void rmdir(std::string path);
		API void mkdir(std::string path);
		API bool exists(std::string path);
		API bool is_directory(std::string path);
		API void rm(std::string path);

		API std::string extension(std::string path);
		API std::vector<std::string> split_path(std::string path);
		API std::string name(std::string path);
		API std::string basename(std::string path);
		API std::string parent(std::string path);
		
		API std::vector<std::string> ls(std::string path);
		API std::vector<std::string> ls_local(std::string path);

		API void initialize();
	}
}