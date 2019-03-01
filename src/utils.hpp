#pragma once

#include <functional>
#include <list>
#include <string>
#include <cassert>

#ifndef G_DIR_SEPARATOR
#ifdef _WIN32
#define G_DIR_SEPARATOR "\\"
#else
#define G_DIR_SEPARATOR "/"
#endif
#endif

struct Param_config {
    int show_v1_h2 = 0;
    bool show_list_dicts = false;
    const char *use_dict_list = nullptr;
    const char *output_temp = nullptr;
    bool no_fuzzy = false;
    const char *opt_data_dir = nullptr;
    bool only_data_dir = false;
    const char *transformat = nullptr;
    bool daemonize = false;
    int listen_port = -1;
};

extern void for_each_file(const std::list<std::string> &dirs_list, const std::string &suff,
                          const std::list<std::string> &order_list, const std::list<std::string> &disable_list,
                          const std::function<void(const std::string &, bool)> &f);
extern std::string json_escape_string(const std::string &str);
extern char *g_file_get_contents(const char *filename);
