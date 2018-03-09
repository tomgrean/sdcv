/*
 * This file part of sdcv - console version of Stardict program
 * http://sdcv.sourceforge.net
 * Copyright (C) 2005-2006 Evgeniy <dushistov@mail.ru>
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
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <dirent.h>

#include "utils.hpp"

static void __for_each_file(const std::string &dirname, const std::string &suff,
                            const std::list<std::string> &order_list, const std::list<std::string> &disable_list,
                            const std::function<void(const std::string &, bool)> &f)
{
    DIR *dir = opendir(dirname.c_str());
    if (dir) {
        const struct dirent *entry;

        while ((entry = readdir(dir)) != nullptr) {
            if (entry->d_name[0] == '.')//skip all '.' started.
                continue;
            const std::string fullfilename(dirname + G_DIR_SEPARATOR + entry->d_name);
            if (entry->d_type == DT_DIR)
                __for_each_file(fullfilename, suff, order_list, disable_list, f);
            else if (strstr(entry->d_name, suff.c_str()) && std::find(order_list.begin(), order_list.end(), fullfilename) == order_list.end()) {
                const auto found = std::find(disable_list.begin(),
                                               disable_list.end(),
                                               fullfilename);
                if (found == disable_list.end())
                    f(fullfilename, false);
            }
        }
        closedir(dir);
    }
}

void for_each_file(const std::list<std::string> &dirs_list, const std::string &suff,
                   const std::list<std::string> &order_list, const std::list<std::string> &disable_list,
                   const std::function<void(const std::string &, bool)> &f)
{
    for (const std::string &item : order_list) {
        f(item, true);
    }
    for (const std::string &item : dirs_list)
        __for_each_file(item, suff, order_list, disable_list, f);
}

// based on https://stackoverflow.com/questions/7724448/simple-json-string-escape-for-c/33799784#33799784
std::string json_escape_string(const std::string &s)
{
    std::ostringstream o;
    for (auto c = s.cbegin(); c != s.cend(); c++) {
        switch (*c) {
        case '"':
            o << "\\\"";
            break;
        case '\\':
            o << "\\\\";
            break;
        case '\b':
            o << "\\b";
            break;
        case '\f':
            o << "\\f";
            break;
        case '\n':
            o << "\\n";
            break;
        case '\r':
            o << "\\r";
            break;
        case '\t':
            o << "\\t";
            break;
        default:
            if (static_cast<unsigned int>(*c) <= 0x1f) {
                o << "\\u"
                  << std::hex << std::setw(4) << std::setfill('0') << (int)*c;
            } else {
                o << *c;
            }
        }
    }
    return o.str();
}
