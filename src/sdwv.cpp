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
    int show_v1_h2 = 0;
    bool show_list_dicts = false;
    std::string use_dict_list;
    bool json_output = false;
    bool no_fuzzy = false;
    std::string opt_data_dir;
    bool only_data_dir = false;
    bool colorize = false;
    int listen_port = -1;

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
                {0, 0, 0, 0 }
            };

            c = getopt_long(argc, argv, "hvlu:je2:xcp",
                     long_options, &option_index);
            if (c == -1)
                break;

            switch (c) {
            case 'h':
            	show_v1_h2 = 2;
            	break;
            case 'v':
            	show_v1_h2 = 1;
            	break;
            case 'l':
                show_list_dicts = true;
                break;
            case 'u':
            	arg = 1;
            	if (optarg)
            		use_dict_list = optarg;
            	else
            		fprintf(stderr, "Omitting arg to '-%c'.\n", c);
                break;
            case 'j':
                json_output = true;
                break;
            case 'e':
                no_fuzzy = true;
                break;
            case '2':
            	arg = 1;
            	if (optarg)
            		opt_data_dir = optarg;
            	else
            		fprintf(stderr, "Omitting arg to '-%c'.\n", c);
                break;
            case 'x':
            	only_data_dir = true;
            	break;
            case 'c':
            	colorize = true;
            	break;
            case 'p':
            	arg = 1;
            	if (optarg)
            		listen_port = (int)strtol(optarg, NULL, 10);
            	else
            		listen_port = 8888;
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

		if (listen_port > 0 && !colorize) {
			fprintf(stderr, "-p implies -c.\n");
			colorize = true;
		}
    } else {
    	show_v1_h2 = 2;
    }
    if (show_v1_h2 == 1) {
        printf("Web version of Stardict, version %s\n", gVersion);
        return EXIT_SUCCESS;
    } else if (show_v1_h2 == 2) {
    	puts(
    			"Usage:\n"
    			"  sdwv [OPTION...]  words\n"
    			"\n"
    			"Help Options:\n"
    			"  -h, --help                     Show help options\n"
    			"\n"
    			"Application Options:\n"
    			"  -v, --version                  display version information and exit\n"
    			"  -l, --list-dicts               display list of available dictionaries and exit\n"
    			"  -u, --use-dict                 for search use only dictionary with this bookname\n"
    			"  -j, --json-output              print the result formatted as JSON\n"
    			"  -e, --exact-search             do not fuzzy-search for similar words, only return exact matches\n"
    			"  -0, --utf8-output              output must be in utf8\n"
    			"  -1, --utf8-input               input of sdwv in utf8\n"
    			"  -2, --data-dir                 use this directory as path to stardict data directory\n"
    			"  -x, --only-data-dir            only use the dictionaries in data-dir, do not search in user and system directories\n"
    			"  -c, --color                    colorize the output\n"
    			"  -p, --port                     the port to listen\n"
    			"\n");
        return EXIT_SUCCESS;
    }

    const char *stardict_data_dir = getenv("STARDICT_DATA_DIR");
    std::string data_dir;
    if (opt_data_dir.length() <= 0) {
        if (!only_data_dir) {
            if (stardict_data_dir)
                data_dir = stardict_data_dir;
            else
                data_dir = "/usr/share/stardict/dic";
        }
    } else {
        data_dir = (opt_data_dir);
    }

    const char *homedir = getenv("HOME");
//    if (!homedir)
//        homedir = g_get_home_dir();

    std::list<std::string> dicts_dir_list;
    if (!only_data_dir)
        dicts_dir_list.push_back(std::string(homedir) + G_DIR_SEPARATOR + ".stardict" + G_DIR_SEPARATOR + "dic");
    dicts_dir_list.push_back(data_dir);
    if (show_list_dicts) {
        list_dicts(dicts_dir_list, json_output);
        return EXIT_SUCCESS;
    }

    std::list<std::string> disable_list;

    std::map<std::string, std::string> bookname_to_ifo;
    for_each_file(dicts_dir_list, ".ifo", std::list<std::string>(), std::list<std::string>(),
                  [&bookname_to_ifo](const std::string &fname, bool) {
                      DictInfo dict_info;
                      const bool load_ok = dict_info.load_from_ifo_file(fname, false);
                      if (!load_ok)
                          return;
                      bookname_to_ifo[dict_info.bookname] = dict_info.ifo_file_name;
                  });

    std::list<std::string> order_list;
    if (use_dict_list.length() > 0) {
        for (auto &&x : bookname_to_ifo) {
            const char *p = (use_dict_list.c_str());
            for (; p != nullptr; ++p)
                if (x.first.compare(p) == 0) {
                    break;
                }
            if (p == nullptr) {
                disable_list.push_back(x.second);
            }
        }

        // add bookname to list
        const char *p = (use_dict_list.c_str());
        while (p) {
            order_list.push_back(bookname_to_ifo.at(p));
            ++p;
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
        fprintf(stderr, "g_mkdir failed: %s\n", strerror(errno));
    }

    Library::pbookname_to_ifo = &bookname_to_ifo;
    Library lib(colorize, json_output, no_fuzzy);
    lib.load(dicts_dir_list, order_list, disable_list);

    if (optind < argc) {
        for (int i = optind; i < argc; ++i)
            if (lib.process_phrase(argv[i], false).length() <= 0) {
                return EXIT_FAILURE;
            }
    } else if (listen_port > 0) {
        httplib::Server serv;
        serv.set_base_dir(data_dir.c_str());
        serv.get("/", [&](const httplib::Request &req, httplib::Response &res) {
            std::string result = lib.process_phrase(req.get_param_value("w").c_str(), true);
            res.set_content(result, "text/html");
        });
        //serv.get("/.*/res/.*", Ser);
        serv.listen("127.0.0.1", (int)listen_port);
    } else {
        fprintf(stderr, "There is no word.\n");
    }
    return EXIT_SUCCESS;
} catch (const std::exception &ex) {
    fprintf(stderr, "Internal error: %s\n", ex.what());
    exit(EXIT_FAILURE);
}

static void list_dicts(const std::list<std::string> &dicts_dir_list, bool use_json)
{
    bool first_entry = true;
    if (!use_json)
        printf("Dictionary's name   Word count\n");
    else
        fputc('[', stdout);
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
                                  fputc(',', stdout); // comma between entries
                              }
                              printf("{\"name\": \"%s\", \"wordcount\": \"%d\"}", json_escape_string(bookname).c_str(), dict_info.wordcount);
                          } else {
                              printf("%s    %d\n", bookname.c_str(), dict_info.wordcount);
                          }
                      }
                  });
    if (use_json)
        fputs("]\n", stdout);
}
