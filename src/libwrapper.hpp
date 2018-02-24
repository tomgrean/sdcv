#pragma once

#include <string>
#include <vector>

#include "readline.hpp"
#include "stardict_lib.hpp"

//this structure is wrapper and it need for unification
//results of search whith return Dicts class
struct TSearchResult {
    std::string bookname;
    std::string def;
    std::string exp;
    std::string idname;//TODO unique id.

    TSearchResult(const std::string &bookname_, const std::string &def_, const std::string &exp_)
        : bookname(bookname_)
        , def(def_)
        , exp(exp_)
    {
    	idname = bookname + ".." + def;
    	std::string::size_type index;
    	for (const char ch : {'\'', '\"', ' ', '\t'}) {
			while ((index = idname.find(ch)) != std::string::npos) {
				idname.replace(index, 1, 1, '_');
			}
		}
    }
};

typedef std::vector<TSearchResult> TSearchResultList;

//this class is wrapper around Dicts class for easy use
//of it
class Library : public Libs
{
public:
    static std::map<std::string, std::string> *pbookname_to_ifo;
    Library(bool uinput, bool uoutput, bool colorize_output, bool use_json, bool no_fuzzy)
        : utf8_input_(uinput)
        , utf8_output_(uoutput)
        , colorize_output_(colorize_output)
        , json_(use_json)
    {
        setVerbose(!use_json);
        setFuzzy(!no_fuzzy);
    }

    bool process_phrase(const char *loc_str, IReadLine &io, bool force = false);

private:
    bool utf8_input_;
    bool utf8_output_;
    bool colorize_output_;
    bool json_;

    void SimpleLookup(const std::string &str, TSearchResultList &res_list);
    void LookupWithFuzzy(const std::string &str, TSearchResultList &res_list);
    void LookupWithRule(const std::string &str, TSearchResultList &res_lsit);
    void LookupData(const std::string &str, TSearchResultList &res_list);
    void print_search_result(FILE *out, TSearchResultList &res_list, bool &first_result);
};

