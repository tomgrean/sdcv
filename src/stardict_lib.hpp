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
    uint32_t offset;
    char *data;
    //write code here to make it inline
    cacheItem() { data = nullptr; }
    ~cacheItem() { free(data); }
};

const int WORDDATA_CACHE_NUM = 10;
const int INVALID_INDEX = -100;

class DictBase
{
public:
    DictBase() {}
    ~DictBase()
    {
        if (dictfile)
            fclose(dictfile);
    }
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
    std::string sametypesequence;
    FILE *dictfile = nullptr;
    std::unique_ptr<DictData> dictdzfile;

private:
    cacheItem cache[WORDDATA_CACHE_NUM];
    int cache_cur = 0;
};

//this structure contain all information about dictionary
struct DictInfo {
    std::string ifo_file_name;
    uint32_t wordcount;
    uint32_t syn_wordcount;
    std::string bookname;
    std::string author;
    std::string email;
    std::string website;
    std::string date;
    std::string description;
    uint32_t index_file_size;
    uint32_t syn_file_size;
    std::string sametypesequence;

    bool load_from_ifo_file(const std::string &ifofilename, bool istreedict);
};

class IIndexFile
{
public:
    uint32_t wordentry_offset;
    uint32_t wordentry_size;

    virtual ~IIndexFile() {}
    virtual bool load(const std::string &url, uint64_t wc, uint64_t fsize, bool verbose) = 0;
    virtual const char *get_key(int64_t idx) = 0;
    virtual void get_data(int64_t idx) = 0;
    virtual const char *get_key_and_data(int64_t idx) = 0;
    virtual bool lookup(const char *str, int64_t &idx) = 0;
};

class SynFile
{
public:
    bool load(const std::string &url, uint64_t wc);
    bool lookup(const char *str, int64_t &idx);

private:
    std::map<std::string, uint64_t> synonyms;
};

class Dict : public DictBase
{
public:
    Dict() {}
    Dict(const Dict &) = delete;
    Dict &operator=(const Dict &) = delete;
    bool load(const std::string &ifofilename, bool verbose);

    uint64_t narticles() const { return wordcount; }
    const std::string &dict_name() const { return bookname; }
    const std::string &ifofilename() const { return ifo_file_name; }

    const char *get_key(int64_t index) { return idx_file->get_key(index); }
    char *get_data(int64_t index)
    {
        idx_file->get_data(index);
        return DictBase::GetWordData(idx_file->wordentry_offset, idx_file->wordentry_size);
    }
    void get_key_and_data(int64_t index, const char **key, uint32_t *offset, uint32_t *size)
    {
        *key = idx_file->get_key_and_data(index);
        *offset = idx_file->wordentry_offset;
        *size = idx_file->wordentry_size;
    }
    bool Lookup(const char *str, int64_t &idx);
    bool LookupWithRule(const std::regex &spec, int64_t *aIndex, int iBuffLen);

private:
    std::string ifo_file_name;
    uint64_t wordcount;
    uint64_t syn_wordcount;
    std::string bookname;

    std::unique_ptr<IIndexFile> idx_file;
    std::unique_ptr<SynFile> syn_file;

    bool load_ifofile(const std::string &ifofilename, uint64_t &idxfilesize);
};

class Libs
{
public:
    Libs(std::function<void(void)> f = std::function<void(void)>())
    {
        progress_func = f;
        iMaxFuzzyDistance = MAX_FUZZY_DISTANCE; //need to read from cfg.
    }
    void setVerbose(bool verbose) { verbose_ = verbose; }
    void setFuzzy(bool fuzzy) { fuzzy_ = fuzzy; }
    ~Libs();
    Libs(const Libs &) = delete;
    Libs &operator=(const Libs &) = delete;

    bool load_dict(const std::string &url);
    void load(const std::list<std::string> &dicts_dirs,
              const std::list<std::string> &order_list,
              const std::list<std::string> &disable_list);
    int64_t narticles(int idict) const { return oLib[idict]->narticles(); }
    const std::string &dict_name(int idict) const { return oLib[idict]->dict_name(); }
    int ndicts() const { return oLib.size(); }

    const char *poGetWord(int64_t iIndex, int iLib)
    {
        return oLib[iLib]->get_key(iIndex);
    }
    char *poGetWordData(int64_t iIndex, int iLib)
    {
        if (iIndex == INVALID_INDEX)
            return nullptr;
        return oLib[iLib]->get_data(iIndex);
    }
    const char *poGetCurrentWord(int64_t *iCurrent);
    const char *poGetNextWord(const char *word, int64_t *iCurrent);
    const char *poGetPreWord(int64_t *iCurrent);
    bool LookupWord(const char *sWord, int64_t &iWordIndex, int iLib)
    {
        return oLib[iLib]->Lookup(sWord, iWordIndex);
    }
    bool SimpleLookupWord(const char *sWord, int64_t &iWordIndex, int iLib);

    bool LookupSimilarWord(const char *sWord, int64_t &iWordIndex, int iLib);
    bool LookupWithFuzzy(const char *sWord, char *reslist[], int reslist_size);
    int LookupWithRule(const char *sWord, char *reslist[]);
    bool LookupData(const char *sWord, std::vector<char *> *reslist);

protected:
    bool fuzzy_;

private:
    std::vector<Dict *> oLib; // word Libs.
    int iMaxFuzzyDistance;
    std::function<void(void)> progress_func;
    bool verbose_;
};

enum query_t {
    qtSIMPLE,
    qtREGEXP,
    qtFUZZY,
    qtDATA
};

extern query_t analyze_query(const char *s, std::string &res);
