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

using TSearchResultList = std::vector<TSearchResult>;
using CBook_it = std::map<std::string, std::string>::const_iterator;

//variables:
//{{DICT_NAME}} CBook_it->first
//{{DICT_PATH}} CBook_it->second
class VarString {
public:
	VarString(std::string s, char f):str(s),flag(f) {
		//printf("creating varStr: %d, %s\n", flag, str.c_str());
	}
	VarString(VarString &&other):str(std::move(other.str)),flag(other.flag) {
		//printf("CREATING varStr: %d, %s\n", flag, str.c_str());
	}
	const std::string toString(const std::map<std::string, std::string>&params) const {
		if (flag) {
			const auto it = params.find(str);
			return it != params.end() ? it->second : "";
		}
		return str;
	}
private:
	std::string str;
	char flag;//0:string, 1:variable
};
class CustomAction {
public:
	virtual ~CustomAction(){}
	virtual void replaceAll(std::string &input, const std::map<std::string, std::string>&params) = 0;
protected:
	inline std::string getToText(const std::map<std::string, std::string>&params) {
		return genFormatText(to, params);
	}
	inline std::string getFromText(const std::map<std::string, std::string>&params) {
		return genFormatText(from, params);
	}
	void constructString(char *str, bool isFrom);
private:
	static std::string genFormatText(const std::list<VarString> &strs, const std::map<std::string, std::string>&params) {
		std::string result;
		for (const auto &it : strs) {
			result += it.toString(params);
		}
		return result;
	}
	std::list<VarString> from, to;
};
using CustomType = std::vector<std::unique_ptr<CustomAction>>;

class CustomActText: public CustomAction {
public:
	CustomActText(char *f, char *t) {
		constructString(f, true);
		constructString(t, false);
	}
	void replaceAll(std::string &input, const std::map<std::string, std::string>&params) override {
		std::string f = getFromText(params);
		std::string t = getToText(params);
		std::string::size_type fpos;
		while ((fpos = input.find(f)) != std::string::npos) {
			input.replace(fpos, f.length(), t);
		}
	}
};
class CustomActRegex: public CustomAction {
public:
	CustomActRegex(char *f, char *t) {
		constructString(f, true);
		constructString(t, false);
	}
	void replaceAll(std::string &input, const std::map<std::string, std::string>&params) override {
		std::string f = getFromText(params);
		std::string t = getToText(params);
		input = std::regex_replace(input, std::regex(f), t);
	}
};

class CustomTemplate {
public:
	explicit CustomTemplate(const char *fileName);
	CustomTemplate(CustomTemplate&&other):customRep(std::move(other.customRep)) {}
	std::string generate(const CBook_it &dictname, const char *xstr, char sametypesequence, uint32_t &sec_size);
private:
	CustomTemplate(CustomTemplate&) = delete;
	std::map<char, CustomType> customRep;
};

//this class is wrapper around Dicts class for easy use
//of it
class Library : public Libs
{
public:
    Library(const Param_config &param, const std::map<std::string, std::string> & bookname2ifo, const char *tconf)
        : Libs(param), bookname_to_ifo(bookname2ifo), outputTemplate(tconf)
    {
    }

    const std::string process_phrase(const char *loc_str, bool all_data);
    const std::string get_neighbour(const char *str, int offset, uint32_t length);
    std::string parse_data(const CBook_it &dictname, const char *data, bool colorize_output);
private:
    class response_out final
    {
    public:
        explicit response_out(const char *str, const Param_config &param, bool all_data);
        response_out(const response_out &) = delete;
        response_out &operator=(const response_out &) = delete;
        response_out &operator <<(const std::string &content);
        std::string get_content();
        void print_search_result(TSearchResultList &res_list);
    private:
        const Param_config &param_;
        std::string buffer;
        bool all_data;
    };
    const std::map<std::string, std::string> bookname_to_ifo;
    CustomTemplate outputTemplate;

    void SimpleLookup(const std::string &str, TSearchResultList &res_list);
    void LookupWithFuzzy(const std::string &str, TSearchResultList &res_list);
    void LookupWithRule(const std::string &str, TSearchResultList &res_lsit);
    void LookupData(const std::string &str, TSearchResultList &res_list);
};
