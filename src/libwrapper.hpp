#pragma once

#include <string>
#include <vector>

#include "stardict_lib.hpp"
#include "utils.hpp"

//this structure is wrapper and it need for unification
//results of search whith return Dicts class
struct TSearchResult {
    std::string bookname;
    std::string word;
    std::string definition;
    std::string idname;//TODO unique id.

    TSearchResult(const std::string &name, const std::string &w, const std::string &&def);
};

typedef std::vector<TSearchResult> TSearchResultList;

//this class is wrapper around Dicts class for easy use
//of it
class Library : public Libs
{
public:
    Library(const Param_config &param, const std::map<std::string, std::string> & bookname2ifo)
        : Libs(param), bookname_to_ifo(bookname2ifo)
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
    const std::map<std::string, std::string> bookname_to_ifo;

    void SimpleLookup(const std::string &str, TSearchResultList &res_list);
    void LookupWithFuzzy(const std::string &str, TSearchResultList &res_list);
    void LookupWithRule(const std::string &str, TSearchResultList &res_lsit);
    void LookupData(const std::string &str, TSearchResultList &res_list);
};
typedef std::map<std::string, std::string>::const_iterator CBook_it;
