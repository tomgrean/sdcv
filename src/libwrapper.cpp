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

#include <cstdio>
#include <cstring>
#include <map>
#include <unordered_map>
#include <memory>

#include "utils.hpp"
#include "libwrapper.hpp"

void TransAction::constructString(char *str, bool isFrom)
{
    std::list<VarString> *target;
    char *varstart, *varend;

    if (isFrom) {
        target = &from;
    } else {
        target = &to;
    }
    while (str && *str) {
        varstart = strstr(str, "{{");
        if (nullptr == varstart) {
            target->push_back(VarString(str, 0));
            return;
        }
        *varstart = '\0';
        varstart += 2;
        if (varstart > str) {
            target->push_back(VarString(str, 0));
        }
        varend = strstr(varstart, "}}");
        if (nullptr == varend) {
            printf("ERROR parsing %s\n", varstart);
            return;
        }
        *varend = '\0';
        target->push_back(VarString(varstart, 1));
        str = varend + 2;
    }
}
std::string TransformatTemplate::generate(const CBook_it &dictname, const char *xstr, char sametypesequence, uint32_t &sec_size)
{
    std::string res(xstr);
    sec_size = res.length();
    const std::map<char, CustomType>::const_iterator rep = customRep.find(sametypesequence);
    if (rep != customRep.end()) {
        const auto &getter = [&dictname](const std::string &name)->std::string {
            if (name == "DICT_PATH") {
                return dictname->second;
            } else if (name == "DICT_NAME") {
                return dictname->first;
            }
            return "";
        };
        for (const auto &it : rep->second) {
            it->replaceAll(res, getter);
        }
    }
    return res;
}
TransformatTemplate::TransformatTemplate(const char *fileName)
{
    char *content = g_file_get_contents(fileName);
    // file format
    // dict type declaration:
    // :{{sametypesequence}}
    // plain text replace:
    // x=y
    // regular expression replace:
    // x~y
    // both x and y can contain VARIABLEs
    if (content) {
        char sametypesequence = 'm';
        char *savep;
        char *p;

        for (p = strtok_r(content, "\n\r", &savep); p; p = strtok_r(NULL, "\n\r", &savep)) {
            if ('#' == *p) {
                continue;
            } else if (':' == *p) {
                sametypesequence = *++p;
                continue;
            }
            char *eq = nullptr;
            char exprFlag = 0;//the operator char.
            // first get '=' position
            char *pstart = p, *pend = p;
            for (; *pend; ++pstart, ++pend) {
                if (*pend == '\\') {
                    ++pend;
                    switch (*pend) {
                    case 'n':
                        *pstart = '\n';
                        break;
                    case 't':
                        *pstart = '\t';
                        break;
                    case 'r':
                        *pstart = '\r';
                        break;
                    default:
                        *pstart = *pend;
                        break;
                    }
                } else if (!eq) {
                    switch (*pend) {
                    case '=':
                        exprFlag = *pend;
                        *pstart = '\0';
                        eq = pstart + 1;
                        break;
                    case '~':
                        exprFlag = *pend;
                        *pstart = '\0';
                        eq = pstart + 1;
                        break;
                    default:
                        *pstart = *pend;
                        break;
                    }
                } else if (pstart != pend)
                    *pstart = *pend;
            }
            if (pstart != pend)
                *pstart = *pend;

            if (eq) {
                auto it = customRep.find(sametypesequence);
                if (it == customRep.end()) {
                    auto pair = customRep.emplace(sametypesequence, CustomType());
                    it = pair.first;
                }
                TransAction *transact(nullptr);
                switch (exprFlag) {
                case '=':
                    transact = (new TransActText(p, eq));
                    break;
                case '~':
                    //regex
                    transact = (new TransActRegex(p, eq));
                    break;
                }
                if (transact != nullptr)
                    it->second.push_back(std::unique_ptr<TransAction>(transact));
            }
        }
    }
    free(content);
}

static VMaper forFunc(const TSearchResultList::iterator &it, int i,const VMaper &m)
{
    return [&it,i,&m](const std::string &key)->std::string {
        char num[12];
        switch (key[0]) {
        case 'i':
            snprintf(num, sizeof(num), "%d", i);
            return std::string(num);
        case 'w':
            return it->word;
        case 'd':
            return it->definition;
        case 'b':
            return it->bookname;
        }
        return m(key);
    };
}
ResponseOut::ResponseOut(const char *fileName)
{
    char *buffer = g_file_get_contents(fileName);
    char *content, *varstart, *varend, *varcol;
    char stateflag = 0, marker = 0;
    // file format
    // out type declaration:
    // {{m:h}}
    // header
    // {{m:f}}
    // footer
    // {{m:b}}
    // body, can be more than 1 :b
    // controls
    // {{for:}}...{{endfor:}}
    // variables
    // {{str}} the word that just looked up
    //
    // {{word}} actual word
    // {{bookname}} dictionary name
    // {{definition}} the definition in dictionary
    // {{idx}} the loop auto-increment index;
    if (!buffer) {
        printf("no output template\n");
        exit(3);
    }
    const auto &pusher = [this](TemplateHolder *th, char stateflag) {
        if (stateflag > 0) {
            static_cast<ForHolder<TSearchResultList, TSearchResultList::iterator>*>(elements.back())->addHolder(th);
        } else {
            elements.push_back(th);
        }
    };
    content = buffer;
    while (*content) {
        varstart = strstr(content, "{{");
        if (nullptr == varstart) {
            pusher(new TextHolder(content, 0, marker), stateflag);
            break;
        }
        *varstart = '\0';
        varstart += 2;
        if (varstart > content) {
            pusher(new TextHolder(content, 0, marker), stateflag);
        }
        varend = strstr(varstart, "}}");
        if (nullptr == varend) {
            printf("ERROR parsing %s\n", varstart);
            break;
        }
        *varend = '\0';
        //find ':'
        varcol = strchr(varstart, ':');
        if (varcol) {
            *varcol = '\0';
            ++varcol;
            if (*varstart == 'm') {
                marker = *varcol;
                pusher(new MarkerHolder(marker), stateflag);
            } else if (0 == strncmp(varstart, "for", 4)) {
                pusher(new ForHolder<TSearchResultList, TSearchResultList::iterator>(forFunc), stateflag);
                ++stateflag;
            } else if (0 == strncmp(varstart, "endfor", 7)) {
                --stateflag;
            }
        } else {
            pusher(new TextHolder(varstart, 1, marker), stateflag);
        }
        content = varend + 2;
    }
    free(buffer);
}
void ResponseOut::make_content(bool isWrap, TSearchResultList &res_list, const char *str)
{
    const auto &wrapgetter = [&str](const std::string &key)->std::string {
        if (str && key == "str")
            return std::string(str);
        return "";
    };
    bool outFlag = true;

    for (auto &elem: elements) {
        if (elem->holderType == 'M') {
            if (!isWrap && static_cast<const MarkerHolder*>(elem)->flag != 'b') {
                outFlag = false;
            } else {
                outFlag = true;
            }
        } else if (outFlag) {
            if (elem->holderType == 'T') {
                buffer += elem->toString(wrapgetter);
            } else if (elem->holderType == 'F') {
                auto *fh = static_cast<ForHolder<TSearchResultList, TSearchResultList::iterator>*>(elem);
                fh->obj = &res_list;
                buffer += elem->toString(wrapgetter);
            }
        }
    }
}

const std::string Library::process_phrase(const char *str, bool alldata)
{
    TSearchResultList res_list;
    rout.reset();
    if (nullptr == str || '\0' == str[0]) {
        rout.make_content(alldata, res_list, str);
        return rout.get_content();
    }

    std::string query;

    //analyze_query(str, query);

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


    switch (analyze_query(str, query)) {
    case qtFUZZY:
        LookupWithFuzzy(query, res_list);
        break;
    case qtREGEXP:
        LookupWithRule(query, res_list);
        break;
    case qtSIMPLE:
        SimpleLookup(query, res_list);
        if (res_list.empty() && !param_.no_fuzzy)
            LookupWithFuzzy(str, res_list);
        break;
    case qtDATA:
        LookupData(query, res_list);
        break;
    default:
        /*nothing*/;
    }

    rout.make_content(alldata, res_list, str);

    return rout.get_content();
}
const std::string Library::get_neighbour(const char *str, int offset, uint32_t length)
{
    std::list<std::string> neighbour;
    if (nullptr == str || '\0' == str[0])
        return "";

    int32_t *icurr = (int32_t*)malloc(sizeof(int32_t) * ndicts());
    const char *word;

    poGetNextWord(str, icurr);
    while (++offset <= 2) {
        word = poGetPreWord(icurr);
        if (!word)
            break;
    }
    while (neighbour.size() < length) {
        word = poGetNextWord(nullptr, icurr);
        if (word)
            neighbour.push_back(word);
        else
            break;
    }
    free(icurr);

    std::string result;
    for (const auto &e : neighbour) {
        result += e + "\n";
    }
    result.resize(result.length() - 1);//delete last "\n"
    return result;
}

std::string Library::parse_data(const CBook_it &dictname, const char *data)
{
    if (!data)
        return "";

    std::string res;
    uint32_t data_size, sec_size;
    const char *p = data;
    data_size = get_uint32(p);
    p += sizeof(uint32_t);
    while (uint32_t(p - data) < data_size) {
        const char t = *p++;
        sec_size = 0;
        switch (t) {
        case 'h': // HTML data
        case 'w': // WikiMedia markup data
        case 'm': // plain text, utf-8
        case 'l': // not utf-8, some other locale encoding, discouraged, need more work...
        case 'g': // pango markup data
        case 'x': // xdxf

        case 't': // english phonetic string

        case 'k': // KingSoft PowerWord data
        case 'y': // chinese YinBiao or japanese kana, utf-8

            if (*p) {
                res += transformatter.generate(dictname, p, t, sec_size);
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
    int32_t ind;
    res_list.reserve(ndicts());
    for (int idict = 0; idict < ndicts(); ++idict)
        if (SimpleLookupWord(str.c_str(), ind, idict))
            res_list.push_back(
                TSearchResult(dict_name(idict),
                              poGetWord(ind, idict),
                              parse_data(bookname_to_path.find(dict_name(idict)), poGetWordData(ind, idict))));
}

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
