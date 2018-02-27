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

#include <cstring>
#include <map>
#include <memory>

#include "utils.hpp"

#include "libwrapper.hpp"

//static const char ESC_BLUE[] = "<font color='blue'>";
static const char ESC_END[] = "</font>";
static const char ESC_END_B[] = "</b>";
static const char ESC_END_I[] = "</i>";
//static const char ESC_END_U[] = "</u>";
static const char ESC_BOLD[] = "<b>";
//static const char ESC_UNDERLINE[] = "<u>";
static const char ESC_ITALIC[] = "<i>";
static const char ESC_LIGHT_GRAY[] = "<font color='gray'>";
static const char ESC_BROWN[] = "<font color='brown'>";
static const char ESC_GREEN[] = "<font color='green'>";

static const char *TRANSCRIPTION_VISFMT = ESC_BROWN;
static const char *EXAMPLE_VISFMT = ESC_LIGHT_GRAY;
static const char *ABR_VISFMT = ESC_GREEN;

std::map<std::string, std::string> *Library::pbookname_to_ifo = nullptr;

static std::string text2simplehtml(const char *str, uint32_t &sec_size)
{
    std::string res;
    const char *p = str;
    for (; *p; ++p) {
        switch (*p) {
        case '\n':
            res += "<br>\n";
            break;
        case '<':
            res += "&lt;";
            break;
        case '>':
            res += "&gt;";
            break;
        default:
            res += *p;
            break;
        }
    }
    sec_size = p - str;
    return res;
}

static std::string xdxf2text(const std::string &dictname, const char *xstr, bool colorize_output, uint32_t &sec_size)
{
    std::string res;
    const char *p = xstr;
    int tagFlag = 0;//1: kref. 2: rref.
    for (; *p; ++p) {
        if (*p != '<') {
            if (tagFlag == 1) {//kref | a href
                tagFlag = 0;

                const char *end_p = strchr(p, '<');
                if (end_p) {
                    const std::string targetRef(p, end_p - p);
                    res += targetRef;
                }
                res += "'>";
            } else if (tagFlag == 2) {//rref | img or wav
                tagFlag = 0;

                const char *end_p = strchr(p, '<');
                if (end_p) {
                    const std::string targetRef(p, end_p - p);
                    std::string file(Library::pbookname_to_ifo->at(dictname));
                    std::string::size_type end = file.rfind(G_DIR_SEPARATOR);
                    if (end != std::string::npos) {
                        std::string::size_type begin = file.rfind(G_DIR_SEPARATOR, end - 1);
                        if (begin == std::string::npos)
                            begin = 0;
                        file = file.substr(begin + 1, end - begin - 1);
                    }
                    if (file.length() <= 0 || file[0] == '/')
                        file = ".";

                    if (targetRef.find(".wav") != std::string::npos) {
                        // sound.
#if 0
                        res += "<a href='";
                        res += file;
                        res += "/res/";
                        res += targetRef;
                        res += "'>&#128266;</a>";//emoji of loudspeaker
#else
                        res += "&#128266";//emoji of loudspeaker
#endif
                        p = end_p - 1;
                        continue;
                    }
                    // assume image
                    res += "<img src='";
                    res += file;
                    res += "/res/";
                    res += targetRef;
                    res += "'>";
                    p = end_p - 1;
                    continue;
                }
            }
            if (colorize_output && *p == '\n') {
                res += "<br>";
            }
            res += *p;
            continue;
        }

        const char *next = strchr(p, '>');
        if (!next)
            continue;

        const std::string name(p + 1, next - p - 1);

        if (name == "k") {
            const char *begin = next;
            if ((next = strstr(begin, "</k>")) != nullptr)
                next += sizeof("</k>") - 1 - 1;
            else
                next = begin;
        }
        if (colorize_output) {
            if (name == "abr") {
                res += ABR_VISFMT;
            } else if (name == "/abr") {
                res += ESC_END;
            } else if (name == "kref") {
                    res += "<a href='?w=";
                    tagFlag = 1;//kref | a href
            } else if (name == "/kref") {
                    res += "</a>";
            } else if (name == "rref") {
                    tagFlag = 2;//rref | image, sound
            } else if (name == "/rref") {
                    //nothing here.
            } else if (name == "b") {
                res += ESC_BOLD;
            } else if (name == "/b") {
                res += ESC_END_B;
            } else if (name == "i") {
                res += ESC_ITALIC;
            } else if (name == "/i") {
                res += ESC_END_I;
            } else if (name == "tr") {
                res += TRANSCRIPTION_VISFMT;
                res += '[';
            } else if (name == "/tr") {
                res += ']';
                res += ESC_END;
            } else if (name == "ex") {
                res += EXAMPLE_VISFMT;
            } else if (name == "/ex") {
                res += ESC_END;
            } else if (!name.empty() && name[0] == 'c' && name != "co") {
                std::string::size_type pos = name.find('=');
                if (pos != std::string::npos) {
                    pos += 2;
                    std::string::size_type end_pos = name.find('\"', pos);
                    const std::string color(name, pos, end_pos - pos);
                    res += "<font color='" + color + "'>";
                } else {
                    res += ESC_GREEN;
                }
            } else if (name == "/c") {
                res += ESC_END;
            }
        } else {
            if (name == "tr") {
                res += '[';
            } else if (name == "/tr") {
                res += ']';
            }
        }

        p = next;
    }
    sec_size = p - xstr;
    return res;
}

static std::string parse_data(const std::string &dictname, const char *data, bool colorize_output)
{
    if (!data)
        return "";

    std::string res;
    uint32_t data_size, sec_size;
    const char *p = data;
    data_size = get_uint32(p);
    p += sizeof(uint32_t);
    while (uint32_t(p - data) < data_size) {
        sec_size = 0;
        switch (*p++) {
        case 'h': // HTML data
            if (*p) {
                //res += '\n';
                sec_size = res.length();
                res += p;
                sec_size = res.length() - sec_size;
            }
            sec_size++;
            break;
        case 'w': // WikiMedia markup data
        case 'm': // plain text, utf-8
        case 'l': // not utf-8, some other locale encoding, discouraged, need more work...
            if (*p) {
                if (colorize_output) {
                    res += text2simplehtml(p, sec_size);
                } else {
                    sec_size = res.length();
                    res += p;
                    sec_size = res.length() - sec_size;
                }
            }
            sec_size++;
            break;
        case 'g': // pango markup data
        case 'x': // xdxf
            if (*p) {
                res += xdxf2text(dictname, p, colorize_output, sec_size);
            }
            sec_size++;
            break;
        case 't': // english phonetic string
            if (*p) {
                if (colorize_output)
                    res += TRANSCRIPTION_VISFMT;
                res += '[';
                sec_size = res.length();
                res += p;
                sec_size = res.length() - sec_size;
                res += ']';
                if (colorize_output)
                    res += ESC_END;
            }
            sec_size++;
            break;
        case 'k': // KingSoft PowerWord data
        case 'y': // chinese YinBiao or japanese kana, utf-8
            if (*p) {
                sec_size = res.length();
                res += p;
                sec_size -= res.length();
            }
            sec_size++;
            break;
        case 'W': // wav file
        case 'P': // picture data
            sec_size = get_uint32(p);
            sec_size += sizeof(uint32_t);
            break;
        }
        p += sec_size;
    }

    return res;
}

void Library::SimpleLookup(const std::string &str, TSearchResultList &res_list)
{
    int64_t ind;
    res_list.reserve(ndicts());
    for (int idict = 0; idict < ndicts(); ++idict)
        if (SimpleLookupWord(str.c_str(), ind, idict))
            res_list.push_back(
                TSearchResult(dict_name(idict),
                              poGetWord(ind, idict),
                              parse_data(dict_name(idict), poGetWordData(ind, idict), colorize_output_)));
}
#if abcdef
void Library::LookupWithFuzzy(const std::string &str, TSearchResultList &res_list)
{
    static const int MAXFUZZY = 10;

    char *fuzzy_res[MAXFUZZY];
    if (!Libs::LookupWithFuzzy(str.c_str(), fuzzy_res, MAXFUZZY))
        return;

    for (char **p = fuzzy_res, **end = (fuzzy_res + MAXFUZZY); p != end && *p; ++p) {
        SimpleLookup(*p, res_list);
        free(*p);
    }
}

void Library::LookupWithRule(const std::string &str, TSearchResultList &res_list)
{
    std::vector<char *> match_res((MAX_MATCH_ITEM_PER_LIB)*ndicts());

    const int nfound = Libs::LookupWithRule(str.c_str(), &match_res[0]);
    if (nfound == 0)
        return;

    for (int i = 0; i < nfound; ++i) {
        SimpleLookup(match_res[i], res_list);
        free(match_res[i]);
    }
}
#endif
void Library::LookupData(const std::string &str, TSearchResultList &res_list)
{
    std::vector<std::vector<char *>> drl(ndicts());
    if (!Libs::LookupData(str.c_str(), &drl[0]))
        return;
    for (int idict = 0; idict < ndicts(); ++idict)
        for (char *res : drl[idict]) {
            SimpleLookup(res, res_list);
            free(res);
        }
}

Library::response_out::response_out(const char *str, Library *lib, bool bufferout) : lib_(lib), bufferout_(bufferout)
{
    if (lib->json_) {
        if (bufferout_)
            buffer = "[";
        else
            fputc('[', stdout);
        return;
    }
    if (lib_->colorize_output_) {
        std::string headerhtml1 = "<html><head>"
                "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\" />"
                "<title>Star Dictionary</title>"
                "<style>\n"
                ".qw {\n"
                "    border: thin dashed grey;\n"
                "}\n"
                ".res_definition {\n"
                "    width:80%;\n"
                "    table-layout: fixed;\n"
                "    border-left: thin dashed black;\n"
                "    border-right: thin dashed black;\n"
                "    padding-left: 5px;\n"
                "    padding-right: 5px;\n"
                "    padding-bottom: 5px;\n"
                "}\n"
                ".res_word {\n"
                "    width:80%;\n"
                "    table-layout: fixed;\n"
                "    border: thin solid black;\n"
                "    padding-left: 5px;\n"
                "    padding-right: 5px;\n"
                "    padding-bottom: 5px;\n"
                "}\n"
                "</style>\n"
                "</head><body>"
                " <form action=\"/\" method=\"GET\">"
                "  word : <input class=\"qw\" id=\"qwt\" type=\"text\" name=\"w\" value=\"";
        std::string headerhtml2 = "\"/>"
                " <input type=\"submit\" value=\"GO\"/>"
                " </form>"
                "<hr/>";
        if (bufferout_) {
            buffer = headerhtml1 + str + headerhtml2;
        } else {
            fputs(headerhtml1.c_str(), stdout);
            fputs(str, stdout);
            fputs(headerhtml2.c_str(), stdout);
        }
    }
}
Library::response_out::~response_out()
{
    if (lib_->json_) {
        fputc(']', stdout);
    } else if (lib_->colorize_output_) {
        fputs("</body></html>", stdout);
    }
}
Library::response_out &Library::response_out::operator <<(const std::string &content) {
    if (bufferout_) {
        buffer += content;
    } else {
        fputs(content.c_str(), stdout);
    }
    return *this;
}
std::string Library::response_out::get_content() {
    std::string content;
    if (!bufferout_) {
        return content;
    }
    content = buffer;
    buffer = std::string();
    if (lib_->json_) {
        content += ']';
    } else if (lib_->colorize_output_) {
        content += "</body></html>";
    }
    return content;
}

void Library::response_out::print_search_result(TSearchResultList &res_list)
{
    bool first_result = true;
    response_out &out = *this;
    if (!lib_->json_ && lib_->colorize_output_)
        out << "<ol>";
    for (TSearchResult &res : res_list) {
        if (!lib_->json_ && lib_->colorize_output_) {
            // put list-of-contents
            out << "<li><a href='#" << res.idname << "'>"
                << res.def << " : " << res.bookname
                << "</a></li>\n";
        }
    }
    if (!lib_->json_ && lib_->colorize_output_)
        out << "</ol>";

    for (const TSearchResult &res : res_list) {
        if (lib_->json_) {
            if (!first_result) {
                out << ",";
            } else {
                first_result = false;
            }
            out << "{\"dict\": \"" << json_escape_string(res.bookname)
                    << "\",\"word\":\"" << json_escape_string(res.def)
                    << "\",\"definition\":\"" << json_escape_string(res.exp)
                    << "\"}";

        } else {
            if (lib_->colorize_output_) {//HTML <DIV> output
                out << "<div id='"<< res.idname << "' class='res_word'>"
                    << res.bookname << " ("<< res.def
                    << ")</div><div class='res_definition'>"
                    << res.exp << "</div>";
            } else {
                out << "-->" << res.bookname
                    << "\n-->" << res.def
                    << "\n"<< res.exp << "\n\n";
            }
        }
    }
}

const std::string Library::process_phrase(const char *str, bool buffer_out)
{
    response_out outputer(str, this, buffer_out);
    if (nullptr == str)
        return outputer.get_content();

    std::string query;

    analyze_query(str, query);

//    size_t bytes_read;
//    size_t bytes_written;
//    glib::Error err;
//    glib::CharStr str;
//    if (!utf8_input_)
//        str.reset(g_locale_to_utf8(loc_str, -1, &bytes_read, &bytes_written, get_addr(err)));
//    else
//        str.reset(g_strdup(loc_str));
//
//    if (nullptr == get_impl(str)) {
//        fprintf(stderr, _("Can not convert %s to utf8.\n"), loc_str);
//        fprintf(stderr, "%s\n", err->message);
//        return std::string();
//    }

    if (str[0] == '\0')
        return outputer.get_content();

    TSearchResultList res_list;

    switch (analyze_query(str, query)) {
#if abcdef
    case qtFUZZY:
        LookupWithFuzzy(query, res_list);
        break;
    case qtREGEXP:
        LookupWithRule(query, res_list);
        break;
    case qtSIMPLE:
        SimpleLookup(str, res_list);
        if (res_list.empty() && fuzzy_)
            LookupWithFuzzy(str, res_list);
        break;
#endif
    case qtDATA:
        LookupData(query, res_list);
        break;
    default:
        /*nothing*/;
    }

    if (!res_list.empty()) {
        outputer.print_search_result(res_list);
    } else {
        if (!json_) {
            outputer << ("Nothing similar to ") << (str);
        }
    }

    return outputer.get_content();;
}
