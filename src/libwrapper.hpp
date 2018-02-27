#pragma once

#include <string>
#include <vector>

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
    Library(bool colorize_output, bool use_json, bool no_fuzzy)
        : colorize_output_(colorize_output)
        , json_(use_json)
    {
        setVerbose(!use_json);
        setFuzzy(!no_fuzzy);
    }

    const std::string process_phrase(const char *loc_str, bool buffer_out);

private:
    const bool colorize_output_;
    const bool json_;
    class response_out final
    {
    public:
        explicit response_out(const char *str, Library *lib, bool bufferout);
        response_out(const response_out &) = delete;
        response_out &operator=(const response_out &) = delete;
        ~response_out();
        response_out &operator <<(const std::string &content);
        std::string get_content();
        void print_search_result(TSearchResultList &res_list);
    private:
        Library *lib_;
        bool bufferout_;
        std::string buffer;
    };
    void SimpleLookup(const std::string &str, TSearchResultList &res_list);
#if abcdef
    void LookupWithFuzzy(const std::string &str, TSearchResultList &res_list);
    void LookupWithRule(const std::string &str, TSearchResultList &res_lsit);
#endif
    void LookupData(const std::string &str, TSearchResultList &res_list);
};

