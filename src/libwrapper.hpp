#pragma once

#include <sstream>
#include <string>
#include <vector>
#include <cctype>

#include "stardict_lib.hpp"
#include "utils.hpp"

//this structure is wrapper and it need for unification
//results of search whith return Dicts class
struct TSearchResult {
    std::string bookname;
    std::string word;
    std::string definition;

    TSearchResult(const std::string &name, const std::string &w, const std::string &&def):
      bookname(name)
    , word(w)
    , definition(def){}
};

using TSearchResultList = std::vector<TSearchResult>;
using CBook_it = std::map<std::string, std::string>::const_iterator;
using VMaper = std::function<std::string(const std::string&)>;

//variables:
//{{DICT_NAME}} CBook_it->first
//{{DICT_PATH}} CBook_it->second
class VarString {
public:
    explicit VarString(std::string s, char f):str(s),flag(f) {
        //printf("creating varStr: %d, %s\n", flag, str.c_str());
    }
    VarString(VarString &) = delete;
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
    const std::string toString(const VMaper &getter) const {
        if (flag) {
            return getter(str);
        }
        return str;
    }
    const std::string str;
    const char flag;//0:string, 1:variable
};
class TransAction {
public:
    explicit TransAction(){}
    TransAction(TransAction&) = delete;
    TransAction(TransAction&&) = delete;
    virtual ~TransAction(){}
    virtual void replaceAll(std::string &input, const VMaper &params) = 0;
protected:
    void constructString(char *str, bool isFrom);
    static std::string genFormatText(const std::list<VarString> &strs, const VMaper &params) {
        std::string result;
        for (const auto &it : strs) {
            result += it.toString(params);
        }
        return result;
    }
    std::list<VarString> from, to;
};
using CustomType = std::vector<std::unique_ptr<TransAction>>;

class TransActText: public TransAction {
public:
    TransActText(char *f, char *t) {
        constructString(f, true);
        constructString(t, false);
    }
    void replaceAll(std::string &input, const VMaper &params) override {
        std::string f = genFormatText(from, params);
        std::string t = genFormatText(to, params);
        std::string::size_type fpos = 0;
        while ((fpos = input.find(f, fpos)) != std::string::npos) {
            input.replace(fpos, f.length(), t);
            fpos += t.length();
        }
    }
};
class TransActRegex: public TransAction {
public:
    TransActRegex(char *f, char *t) {
        constructString(f, true);
        constructString(t, false);
        if (from.size() == 1 && from.front().flag == 0) {
            const std::string &str = from.front().str;
            try {
                re.reset(new std::regex(str, std::regex::optimize));
            } catch (const std::regex_error &) {
                printf("Regex error1:%s\n", str.c_str());
                exit(2);
            }
        }
    }
    void replaceAll(std::string &input, const VMaper &params) override {
        std::string f = genFormatText(from, params);
        std::string t = genFormatText(to, params);
        if (re != nullptr)
            input = std::regex_replace(input, *re, t);
        else try {
            input = std::regex_replace(input, std::regex(f), t);
        } catch (const std::regex_error &) {
            printf("Regex error2:%s\n", f.c_str());
            // do not exit when running.
        }
    }
private:
    std::unique_ptr<std::regex> re;
};

class TransformatTemplate {
public:
    explicit TransformatTemplate(const char *fileName);
    TransformatTemplate(TransformatTemplate&) = delete;
    TransformatTemplate(TransformatTemplate&&other):customRep(std::move(other.customRep)) {}
    std::string generate(const CBook_it &dictname, const char *xstr, char sametypesequence, uint32_t &sec_size);
private:
    std::map<char, CustomType> customRep;
};
//----------------------------------------
class TemplateHolder {
public:
    TemplateHolder(char htype):holderType(htype){}
    TemplateHolder(TemplateHolder&) = delete;
    virtual ~TemplateHolder(){}
    virtual std::string toString(const VMaper &) const {return "";}
    const char holderType;
};
class MarkerHolder: public TemplateHolder {
    // do mark on header and footer.
public:
    MarkerHolder(char f):TemplateHolder('M'), flag(f){}
    MarkerHolder(MarkerHolder&) = delete;
    const char flag;//'h' for header, 'b' for body, 'f' for footer.
};
class TextHolder: public TemplateHolder {
    // actual text or variable
public:
    TextHolder(const char *s, char f, char fl): TemplateHolder('T'), flag(fl), vstr(s, f) {}
    TextHolder(TextHolder&) = delete;
    std::string toString(const VMaper &getter) const override {
        if (flag == 'j' && vstr.flag) {
            //json format.
            return json_escape_string(vstr.toString(getter));
        }
        return vstr.toString(getter);
    }
    const char flag;//same as in MarkerHolder.
private:
    VarString vstr;
};
template<class ContainerObj, class ObjIt>
class ForHolder: public TemplateHolder {
    // word loops.
public:
    ForHolder(VMaper (*fg)(const ObjIt &,int,const VMaper &)):TemplateHolder('F'),obj(nullptr),funcgetter(fg) {}
    ForHolder(ForHolder&) = delete;
    ~ForHolder() {
        for (const auto t: innerHolder) {
            delete(t);
        }
    }
    std::string toString(const VMaper &getter) const override {
        std::string result;
        if (innerHolder.size() <= 0 || obj == nullptr) {
            return "";
        }
        int idx = 0;
        for (ObjIt it = obj->begin(); it != obj->end(); ++it) {
            ++idx;
            const auto &xg = (*funcgetter)(it, idx, getter);
            for (const auto &vs: innerHolder) {
                result += vs->toString(xg);
            }
        }
        return result;
    }
    void addHolder(TemplateHolder *th){
        innerHolder.push_back(th);
    }
    ContainerObj *obj;
private:
    VMaper (*funcgetter)(const ObjIt &it,int,const VMaper &);
    std::list<TemplateHolder*> innerHolder;
};
class ResponseOut {
public:
    explicit ResponseOut(const char *fileName);
    ResponseOut(const ResponseOut &) = delete;
    ResponseOut(const ResponseOut &&o):buffer(std::move(o.buffer)), elements(std::move(o.elements)){}
    ResponseOut &operator=(const ResponseOut &) = delete;
    ~ResponseOut() {
        for (const auto t: elements) {
            delete(t);
        }
    }
    inline const std::string &get_content() const {return buffer;}
    inline void reset() {buffer.clear();}
    void make_content(bool isWrap, TSearchResultList &res_list, const char *str);
protected:
    std::string buffer;
    std::list<TemplateHolder*> elements;
};
//----------------------------------------
//this class is wrapper around Dicts class for easy use
//of it
class Library : public Libs {
public:
    Library(const Param_config &param, const std::map<std::string, std::string> &&bookname2path)
        : Libs(param), bookname_to_path(bookname2path), transformatter(param.transformat), rout(param.output_temp)
    {
    }

    const std::string process_phrase(const char *loc_str, bool all_data);
    const std::string get_neighbour(const char *str, int offset, uint32_t length);
    std::string parse_data(const CBook_it &dictname, const char *data);
private:
    const std::map<std::string, std::string> bookname_to_path;
    TransformatTemplate transformatter;
    ResponseOut rout;

    void SimpleLookup(const std::string &str, TSearchResultList &res_list);
    void LookupWithFuzzy(const std::string &str, TSearchResultList &res_list);
    void LookupWithRule(const std::string &str, TSearchResultList &res_lsit);
    void LookupData(const std::string &str, TSearchResultList &res_list);
};
