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

static void list_dicts(const std::list<std::string> &dicts_dir_list, bool use_json);
static std::unique_ptr<Library> prepare(Param_config &param);

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

    std::unique_ptr<Library>lib = prepare(param);
    if (lib == nullptr) {
        return EXIT_SUCCESS;
    }

    if (param.listen_port > 0) {
        httplib::Server serv;
        serv.set_base_dir(param.opt_data_dir);
        serv.get("/", [&](const httplib::Request &req, httplib::Response &res) {
            bool all_data = true;
            if (req.has_param("co")) {//content only. partial html
                all_data = false;
            }
            std::string result = lib->process_phrase(req.get_param_value("w").c_str(), all_data);
            res.set_content(result, "text/html");
        });
        serv.get("/neigh", [&](const httplib::Request &req, httplib::Response &res) {
            int offset;
            uint32_t length;
            char *pch;
            offset = strtol(req.get_param_value("off").c_str(), &pch, 10);
            if (*pch) {
                offset = 0;
            }
            length = strtoul(req.get_param_value("len").c_str(), &pch, 10);
            if (*pch) {
                length = 10;
            }
            std::string result = lib->get_neighbour(req.get_param_value("w").c_str(), offset, length);
            res.set_content(result, "text/plain");
        });
        if (!serv.listen("0.0.0.0", param.listen_port)) {
            puts("start HTTP failed!");
        }
    } else if (optind < argc) {
        for (int i = optind; i < argc; ++i)
            if (lib->process_phrase(argv[i], true).length() <= 0) {
                return 3;
            }
    } else {
        printf("There is no word.\n");
        return 4;
    }
    return EXIT_SUCCESS;
} catch (const std::exception &ex) {
    exit(10);
}
static std::unique_ptr<Library> prepare(Param_config &param)
{
    const char *stardict_data_dir = getenv("STARDICT_DATA_DIR");
    const char *data_dir;
    if (param.opt_data_dir) {
        data_dir = param.opt_data_dir;
    } else {
        if (stardict_data_dir) {
            data_dir = stardict_data_dir;
        } else {
//            data_dir = "/storage/sdcard1/download/dict";
            data_dir = "/usr/share/stardict/dic";
//            data_dir = "/mnt/code/dic";
        }
        param.opt_data_dir = data_dir;
    }

    const char *homedir = getenv("HOME");
    if (!homedir)
        homedir = "/tmp/";

    std::list<std::string> dicts_dir_list;
    if (!param.only_data_dir)
        dicts_dir_list.push_back(std::string(homedir) + G_DIR_SEPARATOR ".stardict" G_DIR_SEPARATOR "dic");
    dicts_dir_list.push_back(param.opt_data_dir);
    if (param.show_list_dicts) {
        list_dicts(dicts_dir_list, param.json_output);
        return nullptr;
    }

    std::list<std::string> order_list;
    std::list<std::string> disable_list;
    std::map<std::string, std::string> bookname_to_ifo;
    for_each_file(dicts_dir_list, ".ifo", order_list, disable_list,
                  [&bookname_to_ifo](const std::string &fname, bool) {
                      const auto &&ifo = load_from_ifo_file(fname, false);
                      if (ifo.size() < 1)
                          return;
                      bookname_to_ifo[ifo.at("bookname")] = ifo.at("ifo_file_name");
                  });

    if (param.use_dict_list) {
        char *dict_list_str = strdup(param.use_dict_list);
        char *p = dict_list_str;
        char *t = strtok_r(p, "\r\n", &p);
        while (t) {
            const auto found = bookname_to_ifo.find(t);
            if (found != bookname_to_ifo.end())
                order_list.push_back(found->second);
            t = strtok_r(nullptr, "\r\n", &p);
        }
        free(dict_list_str);
        for (const auto &x : bookname_to_ifo) {
            const auto found = std::find(order_list.begin(), order_list.end(), x.second);
            if (found == order_list.end()) {
                disable_list.push_back(x.second);
            }
        }
    } else {
        const std::string odering_cfg_file = std::string(homedir) + G_DIR_SEPARATOR ".sdwv_ordering";
        char *ordering_str = g_file_get_contents(odering_cfg_file.c_str());
        if (ordering_str) {
            char *p = ordering_str;
            char *t = strtok_r(p, "\r\n", &p);
            while (t) {
                order_list.push_back(bookname_to_ifo.at(t));
                t = strtok_r(nullptr, "\r\n", &p);
            }
            free(ordering_str);
        }
    }

    std::unique_ptr<Library> lib(new Library(param, bookname_to_ifo));
    lib->load(dicts_dir_list, order_list, disable_list);
    return lib;
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
                      const auto &&ifo = load_from_ifo_file(filename, false);
                      if (ifo.size() > 1) {
                          const std::string &bookname = ifo.at("bookname");
                          if (use_json) {
                              if (first_entry) {
                                  first_entry = false;
                              } else {
                                  putchar(','); // comma between entries
                              }
                              printf("{\"name\": \"%s\", \"wordcount\": \"%s\"}", json_escape_string(bookname).c_str(), ifo.at("wordcount").c_str());
                          } else {
                              printf("%s    %s\n", bookname.c_str(), ifo.at("wordcount").c_str());
                          }
                      }
                  });
    if (use_json)
        printf("]\n");
}
