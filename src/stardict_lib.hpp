#pragma once

#include <cstdio>
#include <cstring>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <regex>

#include "dictziplib.hpp"
#include "utils.hpp"

const int MAX_MATCH_ITEM_PER_LIB = 100;
const int MAX_FUZZY_DISTANCE = 3; // at most MAX_FUZZY_DISTANCE-1 differences allowed when find similar words

inline uint32_t get_uint32(const char *addr)
{
    uint32_t result;
    memcpy(&result, addr, sizeof(uint32_t));
    return result;
}

inline void set_uint32(char *addr, uint32_t val)
{
    memcpy(addr, &val, sizeof(uint32_t));
}

struct cacheItem {
    uint32_t offset = 0;
    char *data = nullptr;
    //write code here to make it inline
    cacheItem() {}
    ~cacheItem() { free(data); }
};

const int WORDDATA_CACHE_NUM = 10;
const int INVALID_INDEX = -100;

class DictBase
{
public:
    DictBase() {}
    DictBase(const DictBase &) = delete;
    DictBase &operator=(const DictBase &) = delete;
    char *GetWordData(uint32_t idxitem_offset, uint32_t idxitem_size);
    bool containSearchData() const
    {
        if (sametypesequence.empty())
            return true;
        return sametypesequence.find_first_of("mlgxty") != std::string::npos;
    }
    bool SearchData(std::vector<std::string> &SearchWords, uint32_t idxitem_offset, uint32_t idxitem_size, char *origin_data);

protected:
    ~DictBase()
    {
        if (dictfile)
            fclose(dictfile);
    }
    std::string sametypesequence;
    FILE *dictfile = nullptr;
    std::unique_ptr<DictData> dictdzfile;

private:
    cacheItem cache[WORDDATA_CACHE_NUM];
    int cache_cur = 0;
};

class IIndexFile
{
public:
    uint32_t wordentry_offset;
    uint32_t wordentry_size;

    virtual ~IIndexFile() {}
    virtual bool load(const std::string &url, uint32_t wc, uint32_t fsize, bool verbose) = 0;
    virtual const char *get_key(int32_t idx) = 0;
    virtual void get_data(int32_t idx) = 0;
    virtual const char *get_key_and_data(int32_t idx) = 0;
    virtual bool lookup(const char *str, int32_t &idx, std::function<int(const char*,const char*)> cmp) = 0;
};

class SynFile
{
public:
    bool load(const std::string &url, uint32_t wc);
    bool lookup(const char *str, int32_t &idx);

private:
    std::map<std::string, uint32_t> synonyms;
};

class Dict : public DictBase
{
public:
    Dict(): wordcount(0), syn_wordcount(0) {}
    Dict(const Dict &) = delete;
    Dict &operator=(const Dict &) = delete;
    bool load(const std::string &ifofilename, bool verbose);

    uint32_t narticles() const { return wordcount; }
    const std::string &dict_name() const { return bookname; }
    const std::string &ifofilename() const { return ifo_file_name; }

    const char *get_key(int32_t index) { return idx_file->get_key(index); }
    char *get_data(int32_t index)
    {
        idx_file->get_data(index);
        return DictBase::GetWordData(idx_file->wordentry_offset, idx_file->wordentry_size);
    }
    void get_key_and_data(int32_t index, const char **key, uint32_t *offset, uint32_t *size)
    {
        *key = idx_file->get_key_and_data(index);
        *offset = idx_file->wordentry_offset;
        *size = idx_file->wordentry_size;
    }
    bool Lookup(const char *str, int32_t &idx, bool ignorecase);
    bool LookupWithRule(const std::regex &spec, int32_t *aIndex, int iBuffLen);

private:
    std::string ifo_file_name;
    uint32_t wordcount;
    uint32_t syn_wordcount;
    std::string bookname;

    std::unique_ptr<IIndexFile> idx_file;
    std::unique_ptr<SynFile> syn_file;

    bool load_ifofile(const std::string &ifofilename, uint32_t &idxfilesize);
};

class Libs
{
public:
    Libs(const Param_config &param, std::function<void(void)> f = std::function<void(void)>())
        : param_(param)
        , progress_func(f)
    {
        iMaxFuzzyDistance = MAX_FUZZY_DISTANCE; //need to read from cfg.
    }
    Libs(const Libs &) = delete;
    Libs &operator=(const Libs &) = delete;

    bool load_dict(const std::string &url);
    void load(const std::list<std::string> &dicts_dirs,
              const std::list<std::string> &order_list,
              const std::list<std::string> &disable_list);
    int32_t narticles(int idict) const { return oLib[idict]->narticles(); }
    const std::string &dict_name(int idict) const { return oLib[idict]->dict_name(); }
    int ndicts() const { return oLib.size(); }

    const char *poGetWord(int32_t iIndex, int iLib)
    {
        return oLib[iLib]->get_key(iIndex);
    }
    char *poGetWordData(int32_t iIndex, int iLib)
    {
        if (iIndex == INVALID_INDEX)
            return nullptr;
        return oLib[iLib]->get_data(iIndex);
    }
    const char *poGetCurrentWord(int32_t *iCurrent);
    const char *poGetNextWord(const char *word, int32_t *iCurrent);
    const char *poGetPreWord(int32_t *iCurrent);
    bool LookupWord(const char *sWord, int32_t &iWordIndex, int iLib)
    {
        return oLib[iLib]->Lookup(sWord, iWordIndex, false);
    }
    bool SimpleLookupWord(const char *sWord, int32_t &iWordIndex, int iLib);

    bool LookupSimilarWord(const char *sWord, int32_t &iWordIndex, int iLib);
    bool LookupWithFuzzy(const char *sWord, char *reslist[], int reslist_size);
    int LookupWithRule(const char *sWord, char *reslist[]);
    bool LookupData(const char *sWord, std::vector<char *> *reslist);

protected:
    ~Libs();
    const Param_config &param_;

private:
    std::vector<Dict *> oLib; // word Libs.
    int iMaxFuzzyDistance;
    std::function<void(void)> progress_func;
};

enum query_t {
    qtSIMPLE,
    qtREGEXP,
    qtFUZZY,
    qtDATA
};

extern std::map<std::string, std::string> load_from_ifo_file(const std::string &ifofilename, bool istreedict);
extern query_t analyze_query(const char *s, std::string &res);
