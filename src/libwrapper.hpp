#pragma once

#include <string>
#include <vector>

#include "stardict_lib.hpp"
#include "utils.hpp"

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
    Library(const Param_config &param)
        : Libs(param)
    {
    }

    const std::string process_phrase(const char *loc_str, bool buffer_out);

private:
    class response_out final
    {
    public:
        explicit response_out(const char *str, const Param_config &param, bool bufferout);
        response_out(const response_out &) = delete;
        response_out &operator=(const response_out &) = delete;
        ~response_out();
        response_out &operator <<(const std::string &content);
        std::string get_content();
        void print_search_result(TSearchResultList &res_list);
    private:
        const Param_config &param_;
        bool bufferout_;
        std::string buffer;
    };
    void SimpleLookup(const std::string &str, TSearchResultList &res_list);
    void LookupWithFuzzy(const std::string &str, TSearchResultList &res_list);
    void LookupWithRule(const std::string &str, TSearchResultList &res_lsit);
    void LookupData(const std::string &str, TSearchResultList &res_list);
};

