/*
 * This file part of sdwv - web version of Stardict program
 * https://github.com/tomgrean/sdwv
 * Copyright (C) 2003-2006 Evgeniy <dushistov@mail.ru>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <algorithm>
#include <cerrno>
#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <getopt.h>

#include "libwrapper.hpp"
#include "utils.hpp"
#include "httplib.h"

static const char gVersion[] = VERSION;

namespace
{
static bool stdio_getline(FILE *in, std::string &str)
{
    assert(in != nullptr);
    str.clear();
    int ch;
    while ((ch = fgetc(in)) != EOF && ch != '\n')
        str += ch;

    return EOF != ch;
}
}

static void list_dicts(const std::list<std::string> &dicts_dir_list, bool use_json);

int main(int argc, char *argv[]) try {
	Param_config param;

    if (argc > 1) {
        int c, arg;

        while (1) {
            int option_index = 0;
            arg = 0;
            static struct option long_options[] = {
                {"help",          no_argument,       0,  'h' },
                {"version",       no_argument,       0,  'v' },
                {"list-dicts",    no_argument,       0,  'l' },
                {"use-dict",      required_argument, 0,  'u' },
                {"json-output",   no_argument,       0,  'j' },
                {"exact-search",  no_argument,       0,  'e' },
                {"data-dir",      required_argument, 0,  '2' },
                {"only-data-dir", no_argument,       0,  'x' },
                {"color",         no_argument,       0,  'c' },
                {"port",          required_argument, 0,  'p' },
                {"daemon",        no_argument,       0,  'd' },
                {0, 0, 0, 0 }
            };

            c = getopt_long(argc, argv, "hvlu:je2:xcp:d",
                     long_options, &option_index);
            if (c == -1)
                break;

            switch (c) {
            case 'h':
            	param.show_v1_h2 = 2;
                break;
            case 'v':
            	param.show_v1_h2 = 1;
                break;
            case 'l':
            	param.show_list_dicts = true;
                break;
            case 'u':
                arg = 1;
                if (optarg)
                	param.use_dict_list = optarg;
                else
                    printf("Omitting arg to '-%c'.\n", c);
                break;
            case 'j':
            	param.json_output = true;
                break;
            case 'e':
            	param.no_fuzzy = true;
                break;
            case '2':
                arg = 1;
                if (optarg)
                	param.opt_data_dir = optarg;
                else
                    printf("Omitting arg to '-%c'.\n", c);
                break;
            case 'x':
            	param.only_data_dir = true;
                break;
            case 'c':
            	param.colorize = true;
                break;
            case 'p':
                arg = 1;
                if (optarg)
                	param.listen_port = (int)strtol(optarg, NULL, 10);
                else {
                    printf("no port, use default 8888\n");
                    param.listen_port = 8888;
                }
                break;
            case 'd':
            	param.daemonize = true;
                break;
            case '?':
                break;

            default:
                printf("?? getopt returned character code 0%o ??\n", c);
            }
        }

        if (arg) {
            optind++;
        }

        if (param.listen_port > 0 && !param.colorize) {
            printf("-p implies -c.\n");
            param.colorize = true;
        }
    } else {
    	param.show_v1_h2 = 2;
    }
    if (param.show_v1_h2 == 1) {
        printf("Web version of Stardict, version %s\n", gVersion);
        return EXIT_SUCCESS;
    } else if (param.show_v1_h2 == 2) {
        puts(
                "Usage:\n"
                "  sdwv [OPTION...]  words\n"
                "\n"
                "Help Options:\n"
                "  -h, --help             Show help options\n"
                "\n"
                "Application Options:\n"
                "  -v, --version          display version information and exit\n"
                "  -l, --list-dicts       display list of available dictionaries and exit\n"
                "  -u, --use-dict         for search use only dictionary with this bookname\n"
                "  -j, --json-output      print the result formatted as JSON\n"
                "  -e, --exact-search     do not fuzzy-search for similar words, only return exact matches\n"
                "  -0, --utf8-output      output must be in utf8\n"
                "  -1, --utf8-input       input of sdwv in utf8\n"
                "  -2, --data-dir         use this directory as path to stardict data directory\n"
                "  -x, --only-data-dir    only use the dictionaries in data-dir, do not search in user and system directories\n"
                "  -c, --color            colorize the output\n"
                "  -p, --port             the port to listen\n"
                "  -d, --daemon           run in daemon mode.\n"
                "\n");
        return EXIT_SUCCESS;
    }

    const char *stardict_data_dir = getenv("STARDICT_DATA_DIR");
    std::string data_dir;
    if (param.opt_data_dir) {
        data_dir = param.opt_data_dir;
    } else {
        if (!param.only_data_dir) {
            if (stardict_data_dir) {
                data_dir = stardict_data_dir;
            } else {
//                data_dir = "/storage/sdcard1/download/dict";
                data_dir = "/usr/share/stardict/dic";
//                data_dir = "/mnt/code/dic";
            }
        }
    }

    if (param.daemonize) {
    	param.show_v1_h2 = open("/dev/null", O_RDWR);
    	dup2(param.show_v1_h2, 0);
        dup2(param.show_v1_h2, 1);
        dup2(param.show_v1_h2, 2);
        close(param.show_v1_h2);
        param.show_v1_h2 = fork();
        if (param.show_v1_h2) {
            return EXIT_SUCCESS;
        }
    }
    const char *homedir = getenv("HOME");
    if (!homedir)
        homedir = "/tmp/";

    std::list<std::string> dicts_dir_list;
    if (!param.only_data_dir)
        dicts_dir_list.push_back(std::string(homedir) + G_DIR_SEPARATOR + ".stardict" + G_DIR_SEPARATOR + "dic");
    dicts_dir_list.push_back(data_dir);
    if (param.show_list_dicts) {
        list_dicts(dicts_dir_list, param.json_output);
        return EXIT_SUCCESS;
    }

    std::list<std::string> order_list;
    std::list<std::string> disable_list;
    std::map<std::string, std::string> bookname_to_ifo;
    for_each_file(dicts_dir_list, ".ifo", order_list, disable_list,
                  [&bookname_to_ifo](const std::string &fname, bool) {
                      DictInfo dict_info;
                      const bool load_ok = dict_info.load_from_ifo_file(fname, false);
                      if (!load_ok)
                          return;
                      bookname_to_ifo[dict_info.bookname] = dict_info.ifo_file_name;
                  });

    if (param.use_dict_list) {
        char *dict_list_str = strdup(param.use_dict_list);
        char *p = dict_list_str;
        while (true) {
            char *t = strsep(&p, ";\n");
            if (!t)
                break;
            auto found = bookname_to_ifo.find(t);
            if (found != bookname_to_ifo.end())
                order_list.push_back(found->second);
        }
        free(dict_list_str);
        for (auto &x : bookname_to_ifo) {
            const auto found = std::find(order_list.begin(), order_list.end(), x.second);
            if (found == order_list.end()) {
                disable_list.push_back(x.second);
            }
        }
    } else {
        const std::string odering_cfg_file = std::string(homedir) + G_DIR_SEPARATOR ".sdwv_ordering";
        FILE *ordering_file = fopen(odering_cfg_file.c_str(), "r");
        if (ordering_file != nullptr) {
            std::string line;
            while (stdio_getline(ordering_file, line)) {
                order_list.push_back(bookname_to_ifo.at(line));
            }
            fclose(ordering_file);
        }
    }

    const std::string conf_dir = std::string(homedir) + G_DIR_SEPARATOR + ".stardict";
    if (mkdir(conf_dir.c_str(), S_IRWXU) == -1 && errno != EEXIST) {
        printf("g_mkdir failed: %s\n", strerror(errno));
    }

    Library::pbookname_to_ifo = &bookname_to_ifo;
    Library lib(param);
    lib.load(dicts_dir_list, order_list, disable_list);

    if (param.listen_port > 0) {
        httplib::Server serv;
        serv.set_base_dir(data_dir.c_str());
        serv.get("/", [&](const httplib::Request &req, httplib::Response &res) {
            std::string result = lib.process_phrase(req.get_param_value("w").c_str(), true);
            res.set_content(result, "text/html");
        });
        //serv.get("/.*/res/.*", Ser);
        serv.listen("0.0.0.0", (int)param.listen_port);
    } else if (optind < argc) {
        for (int i = optind; i < argc; ++i)
            if (lib.process_phrase(argv[i], false).length() <= 0) {
                return 3;
            }
    } else {
        printf("There is no word.\n");
        return 4;
    }
    return EXIT_SUCCESS;
} catch (const std::exception &ex) {
    exit(120);
}

static void list_dicts(const std::list<std::string> &dicts_dir_list, bool use_json)
{
    bool first_entry = true;
    if (!use_json)
        printf("Dictionary's name   Word count\n");
    else
        putchar('[');
    std::list<std::string> order_list, disable_list;
    for_each_file(dicts_dir_list, ".ifo", order_list,
                  disable_list, [use_json, &first_entry](const std::string &filename, bool) -> void {
                      DictInfo dict_info;
                      if (dict_info.load_from_ifo_file(filename, false)) {
                          const std::string &bookname = dict_info.bookname;
                          if (use_json) {
                              if (first_entry) {
                                  first_entry = false;
                              } else {
                                  putchar(','); // comma between entries
                              }
                              printf("{\"name\": \"%s\", \"wordcount\": \"%d\"}", json_escape_string(bookname).c_str(), dict_info.wordcount);
                          } else {
                              printf("%s    %d\n", bookname.c_str(), dict_info.wordcount);
                          }
                      }
                  });
    if (use_json)
        printf("]\n");
}
