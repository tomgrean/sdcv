#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <algorithm>
#include <cctype>
#include <cstring>
#include <stdexcept>

#include <unistd.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <zlib.h>
#include <libgen.h>

#include "distance.hpp"
#include "mapfile.hpp"
#include "utils.hpp"

#include "stardict_lib.hpp"

#define TO_STR2(xstr) #xstr
#define TO_STR1(xstr) TO_STR2(xstr)

#define THROW_IF_ERROR(expr)                                                                 \
    do {                                                                                     \
        assert((expr));                                                                      \
        if (!(expr))                                                                         \
            throw std::runtime_error(#expr " not true at " __FILE__ ": " TO_STR1(__LINE__)); \
    } while (false)

// Notice: read src/tools/DICTFILE_FORMAT for the dictionary
// file's format information!

namespace
{
struct Fuzzystruct {
    char *pMatchWord;
    int iMatchWordDistance;
};

static inline bool bIsVowel(char inputchar)
{
    char ch = toupper(inputchar);
    return (ch == 'A' || ch == 'E' || ch == 'I' || ch == 'O' || ch == 'U');
}

static bool bIsPureEnglish(const char *str)
{
    // i think this should work even when it is UTF8 string :).
    for (int i = 0; str[i] != 0; i++)
        //if(str[i]<0)
        //if(str[i]<32 || str[i]>126) // tab equal 9,so this is not OK.
        // Better use isascii() but not str[i]<0 while char is default unsigned in arm
        if (!isascii(str[i]))
            return false;
    return true;
}

static const std::string &g_get_user_cache_dir()
{
    static std::string user_cache;
    if (user_cache.length() <= 0) {
        if (getenv("HOME"))
            user_cache = std::string(getenv("HOME")) + G_DIR_SEPARATOR ".cache";
        else
            user_cache = "/tmp/sdwv_cache";
    }
    return user_cache;
}
static inline int stardict_strcmp(const char *s1, const char *s2)
{
    const int a = strcasecmp(s1, s2);
    if (a == 0)
        return strcmp(s1, s2);
    else
        return a;
}
#if 0
template <typename TC=uint32_t>
static void unicode_strdown(TC *str)
{
    while (*str) {
        *str = tolower(*str);
        ++str;
    }
}
#endif

inline static char *g_utf8_strdown(const char *p)
{
    if (!p)
        return nullptr;
    char *result;

    result = strdup(p);

    for (char *x = result; *x; ++x) {
        *x = tolower(*x);
    }
    return result;
}
#if 0
inline static void utf8_strup(char *p)
{
    if (!p)
        return;

    for (char *x = p; *x; ++x) {
        *x = toupper(*x);
    }
}
inline static void utf8_camelcase(char *p)
{
    if (!p)
        return;
    bool firstChar = true;

    for (char *x = p; *x; ++x) {
        if (isalpha(*x)) {
            if (firstChar) {
                firstChar = false;
                *x = toupper(*x);
            } else {
                *x = tolower(*x);
            }
        } else {
            firstChar = true;
        }
    }
}
#endif
}

char *DictBase::GetWordData(uint32_t idxitem_offset, uint32_t idxitem_size)
{
    for (int i = 0; i < WORDDATA_CACHE_NUM; i++)
        if (cache[i].data && cache[i].offset == idxitem_offset)
            return cache[i].data;

    if (dictfile)
        fseek(dictfile, idxitem_offset, SEEK_SET);

    char *data;
    if (!sametypesequence.empty()) {
        char *origin_data((char *)malloc(idxitem_size));

        if (dictfile) {
            const size_t nitems = fread((origin_data), idxitem_size, 1, dictfile);
            THROW_IF_ERROR(nitems == 1);
        } else
            dictdzfile->read((origin_data), idxitem_offset, idxitem_size);

        uint32_t data_size;
        int sametypesequence_len = sametypesequence.length();
        //there have sametypesequence_len char being omitted.
        data_size = idxitem_size + sizeof(uint32_t) + sametypesequence_len;
        //if the last item's size is determined by the end up '\0',then +=sizeof(char);
        //if the last item's size is determined by the head uint32_t type data,then +=sizeof(uint32_t);
        switch (sametypesequence[sametypesequence_len - 1]) {
        case 'm':
        case 't':
        case 'y':
        case 'l':
        case 'g':
        case 'x':
        case 'k':
            data_size += sizeof(char);
            break;
        case 'W':
        case 'P':
            data_size += sizeof(uint32_t);
            break;
        default:
            if (isupper(sametypesequence[sametypesequence_len - 1]))
                data_size += sizeof(uint32_t);
            else
                data_size += sizeof(char);
            break;
        }
        data = (char *)malloc(data_size);
        char *p1, *p2;
        p1 = data + sizeof(uint32_t);
        p2 = origin_data;
        uint32_t sec_size;
        //copy the head items.
        for (int i = 0; i < sametypesequence_len - 1; i++) {
            *p1 = sametypesequence[i];
            p1 += sizeof(char);
            switch (sametypesequence[i]) {
            case 'm':
            case 't':
            case 'y':
            case 'l':
            case 'g':
            case 'x':
            case 'k':
                sec_size = strlen(p2) + 1;
                memcpy(p1, p2, sec_size);
                p1 += sec_size;
                p2 += sec_size;
                break;
            case 'W':
            case 'P':
                sec_size = get_uint32(p2);
                sec_size += sizeof(uint32_t);
                memcpy(p1, p2, sec_size);
                p1 += sec_size;
                p2 += sec_size;
                break;
            default:
                if (isupper(sametypesequence[i])) {
                    sec_size = get_uint32(p2);
                    sec_size += sizeof(uint32_t);
                } else {
                    sec_size = strlen(p2) + 1;
                }
                memcpy(p1, p2, sec_size);
                p1 += sec_size;
                p2 += sec_size;
                break;
            }
        }
        //calculate the last item 's size.
        sec_size = idxitem_size - (p2 - (origin_data));
        *p1 = sametypesequence[sametypesequence_len - 1];
        p1 += sizeof(char);
        switch (sametypesequence[sametypesequence_len - 1]) {
        case 'm':
        case 't':
        case 'y':
        case 'l':
        case 'g':
        case 'x':
        case 'k':
            memcpy(p1, p2, sec_size);
            p1 += sec_size;
            *p1 = '\0'; //add the end up '\0';
            break;
        case 'W':
        case 'P':
            set_uint32(p1, sec_size);
            p1 += sizeof(uint32_t);
            memcpy(p1, p2, sec_size);
            break;
        default:
            if (isupper(sametypesequence[sametypesequence_len - 1])) {
                set_uint32(p1, sec_size);
                p1 += sizeof(uint32_t);
                memcpy(p1, p2, sec_size);
            } else {
                memcpy(p1, p2, sec_size);
                p1 += sec_size;
                *p1 = '\0';
            }
            break;
        }
        set_uint32(data, data_size);
        free(origin_data);
    } else {
        data = (char *)malloc(idxitem_size + sizeof(uint32_t));
        if (dictfile) {
            const size_t nitems = fread(data + sizeof(uint32_t), idxitem_size, 1, dictfile);
            THROW_IF_ERROR(nitems == 1);
        } else
            dictdzfile->read(data + sizeof(uint32_t), idxitem_offset, idxitem_size);
        set_uint32(data, idxitem_size + sizeof(uint32_t));
    }
    free(cache[cache_cur].data);

    cache[cache_cur].data = data;
    cache[cache_cur].offset = idxitem_offset;
    cache_cur++;
    if (cache_cur == WORDDATA_CACHE_NUM)
        cache_cur = 0;
    return data;
}

bool DictBase::SearchData(std::vector<std::string> &SearchWords, uint32_t idxitem_offset, uint32_t idxitem_size, char *origin_data)
{
    int nWord = SearchWords.size();
    std::vector<bool> WordFind(nWord, false);
    int nfound = 0;

    if (dictfile)
        fseek(dictfile, idxitem_offset, SEEK_SET);
    if (dictfile) {
        const size_t nitems = fread(origin_data, idxitem_size, 1, dictfile);
        THROW_IF_ERROR(nitems == 1);
    } else
        dictdzfile->read(origin_data, idxitem_offset, idxitem_size);
    char *p = origin_data;
    uint32_t sec_size;
    int j;
    if (!sametypesequence.empty()) {
        int sametypesequence_len = sametypesequence.length();
        for (int i = 0; i < sametypesequence_len - 1; i++) {
            switch (sametypesequence[i]) {
            case 'm':
            case 't':
            case 'y':
            case 'l':
            case 'g':
            case 'x':
            case 'k':
                for (j = 0; j < nWord; j++)
                    if (!WordFind[j] && strstr(p, SearchWords[j].c_str())) {
                        WordFind[j] = true;
                        ++nfound;
                    }

                if (nfound == nWord)
                    return true;
                sec_size = strlen(p) + 1;
                p += sec_size;
                break;
            default:
                if (isupper(sametypesequence[i])) {
                    sec_size = get_uint32(p);
                    sec_size += sizeof(uint32_t);
                } else {
                    sec_size = strlen(p) + 1;
                }
                p += sec_size;
            }
        }
        switch (sametypesequence[sametypesequence_len - 1]) {
        case 'm':
        case 't':
        case 'y':
        case 'l':
        case 'g':
        case 'x':
        case 'k':
            sec_size = idxitem_size - (p - origin_data);
            for (j = 0; j < nWord; j++) {
                char *t = strstr(p, SearchWords[j].c_str());
                if (!WordFind[j] && t && uint32_t(t - p) < sec_size) {
                    WordFind[j] = true;
                    ++nfound;
                }
            }

            if (nfound == nWord)
                return true;
            break;
        }
    } else {
        while (uint32_t(p - origin_data) < idxitem_size) {
            switch (*p) {
            case 'm':
            case 't':
            case 'y':
            case 'l':
            case 'g':
            case 'x':
            case 'k':
                for (j = 0; j < nWord; j++)
                    if (!WordFind[j] && strstr(p, SearchWords[j].c_str())) {
                        WordFind[j] = true;
                        ++nfound;
                    }

                if (nfound == nWord)
                    return true;
                sec_size = strlen(p) + 1;
                p += sec_size;
                break;
            default:
                if (isupper(*p)) {
                    sec_size = get_uint32(p);
                    sec_size += sizeof(uint32_t);
                } else {
                    sec_size = strlen(p) + 1;
                }
                p += sec_size;
            }
        }
    }
    return false;
}

namespace
{
class OffsetIndex : public IIndexFile
{
public:
    OffsetIndex()
        : idxfile(nullptr)
    {
    }
    ~OffsetIndex()
    {
        if (idxfile)
            fclose(idxfile);
    }
    bool load(const std::string &url, uint32_t wc, uint32_t fsize, bool verbose) override;
    const char *get_key(int32_t idx) override;
    void get_data(int32_t idx) override { get_key(idx); }
    const char *get_key_and_data(int32_t idx) override
    {
        return get_key(idx);
    }
    bool lookup(const char *str, int32_t &idx, std::function<int(const char*,const char*)> cmp) override;

private:
    static const int ENTR_PER_PAGE = 32;
    static const char *CACHE_MAGIC;

    std::vector<uint32_t> wordoffset;
    FILE *idxfile;
    uint32_t wordcount;

    char wordentry_buf[256 + sizeof(uint32_t) * 2]; // The length of "word_str" should be less than 256. See src/tools/DICTFILE_FORMAT.
    struct index_entry {
        int32_t idx;
        std::string keystr;
        void assign(int32_t i, const std::string &str)
        {
            idx = i;
            keystr.assign(str);
        }
    };
    index_entry first, last, middle, real_last;

    struct page_entry {
        char *keystr;
        uint32_t off, size;
    };
    std::vector<char> page_data;
    struct page_t {
        int32_t idx = -1;
        page_entry entries[ENTR_PER_PAGE];

        page_t() {}
        void fill(char *data, int nent, int32_t idx_);
    } page;
    uint32_t load_page(int32_t page_idx);
    const char *read_first_on_page_key(int32_t page_idx);
    const char *get_first_on_page_key(int32_t page_idx);
    bool load_cache(const std::string &url);
    bool save_cache(const std::string &url, bool verbose);
    static std::list<std::string> get_cache_variant(const std::string &url);
};

const char *OffsetIndex::CACHE_MAGIC = "StarDict's Cache, Version: 0.2";
#define CACHE_MAGIC_BYTES 0x51a4d1c1

class WordListIndex : public IIndexFile
{
public:
    WordListIndex()
        : idxdatabuf(nullptr)
    {
    }
    ~WordListIndex() { free(idxdatabuf); }
    bool load(const std::string &url, uint32_t wc, uint32_t fsize, bool verbose) override;
    const char *get_key(int32_t idx) override { return wordlist[idx]; }
    void get_data(int32_t idx) override;
    const char *get_key_and_data(int32_t idx) override
    {
        get_data(idx);
        return get_key(idx);
    }
    bool lookup(const char *str, int32_t &idx, std::function<int(const char*,const char*)> cmp) override;

private:
    char *idxdatabuf;
    std::vector<char *> wordlist;
};

void OffsetIndex::page_t::fill(char *data, int nent, int32_t idx_)
{
    idx = idx_;
    char *p = data;
    int32_t len;
    for (int i = 0; i < nent; ++i) {
        entries[i].keystr = p;
        len = strlen(p);
        p += len + 1;
        entries[i].off = ntohl(get_uint32(p));
        p += sizeof(uint32_t);
        entries[i].size = ntohl(get_uint32(p));
        p += sizeof(uint32_t);
    }
}

inline const char *OffsetIndex::read_first_on_page_key(int32_t page_idx)
{
    fseek(idxfile, wordoffset[page_idx], SEEK_SET);
    uint32_t page_size = wordoffset[page_idx + 1] - wordoffset[page_idx];
    const size_t nitems = fread(wordentry_buf,
                                std::min(sizeof(wordentry_buf), static_cast<size_t>(page_size)),
                                1, idxfile);
    THROW_IF_ERROR(nitems == 1);
    //TODO: check returned values, deal with word entry that strlen>255.
    return wordentry_buf;
}

inline const char *OffsetIndex::get_first_on_page_key(int32_t page_idx)
{
    if (page_idx < middle.idx) {
        if (page_idx == first.idx)
            return first.keystr.c_str();
        return read_first_on_page_key(page_idx);
    } else if (page_idx > middle.idx) {
        if (page_idx == last.idx)
            return last.keystr.c_str();
        return read_first_on_page_key(page_idx);
    } else
        return middle.keystr.c_str();
}

bool OffsetIndex::load_cache(const std::string &url)
{
    const std::list<std::string> vars = get_cache_variant(url);

    for (const std::string &item : vars) {
        struct ::stat idxstat, cachestat;
        if (stat(url.c_str(), &idxstat) != 0 || stat(item.c_str(), &cachestat) != 0)
            continue;
        if (cachestat.st_mtime < idxstat.st_mtime)
            continue;
        MapFile mf;
        if (!mf.open(item.c_str(), cachestat.st_size))
            continue;
        if (strncmp(mf.begin(), CACHE_MAGIC, strlen(CACHE_MAGIC)) != 0)
            continue;
        uint32_t tmp;
        memcpy(&tmp, mf.begin() + strlen(CACHE_MAGIC), sizeof(tmp));
        if (tmp != CACHE_MAGIC_BYTES)
            continue;
        memcpy(&wordoffset[0], mf.begin() + strlen(CACHE_MAGIC) + sizeof(uint32_t), wordoffset.size() * sizeof(wordoffset[0]));
        return true;
    }

    return false;
}

std::list<std::string> OffsetIndex::get_cache_variant(const std::string &url)
{
    std::list<std::string> res = { url + ".oft" };
    if (access(g_get_user_cache_dir().c_str(), R_OK|W_OK|X_OK) && mkdir(g_get_user_cache_dir().c_str(), 0700) == -1)
        return res;

    const std::string cache_dir(g_get_user_cache_dir() + G_DIR_SEPARATOR + "sdwv");

    if (access(cache_dir.c_str(), R_OK|W_OK|X_OK)) {
        if (mkdir(cache_dir.c_str(), 0700) == -1)
            return res;
    } else if (access(cache_dir.c_str(), R_OK|W_OK|X_OK))
        return res;

    char *u = strdup(url.c_str());
    const char *base = basename(u);
    res.push_back(cache_dir + G_DIR_SEPARATOR + base + ".oft");
    free(u);
    return res;
}

bool OffsetIndex::save_cache(const std::string &url, bool verbose)
{
    const std::list<std::string> vars = get_cache_variant(url);
    for (const std::string &item : vars) {
        FILE *out = fopen(item.c_str(), "wb");
        uint32_t magic = CACHE_MAGIC_BYTES;
        if (!out)
            continue;
        if (fwrite(CACHE_MAGIC, 1, strlen(CACHE_MAGIC), out) != strlen(CACHE_MAGIC))
            continue;
        if (fwrite(&magic, 1, sizeof(magic), out) != sizeof(magic))
            continue;
        if (fwrite(&wordoffset[0], sizeof(wordoffset[0]), wordoffset.size(), out) != wordoffset.size())
            continue;
        fclose(out);
        if (verbose) {
            printf("save to cache %s\n", url.c_str());
        }
        return true;
    }
    return false;
}

bool OffsetIndex::load(const std::string &url, uint32_t wc, uint32_t fsize, bool verbose)
{
    wordcount = wc;
    uint32_t npages = (wc - 1) / ENTR_PER_PAGE + 2;
    wordoffset.resize(npages);
    if (!load_cache(url)) { //map file will close after finish of block
        MapFile map_file;
        if (!map_file.open(url.c_str(), fsize))
            return false;
        const char *idxdatabuffer = map_file.begin();

        const char *p1 = idxdatabuffer;
        uint32_t index_size;
        uint32_t j = 0;
        for (uint32_t i = 0; i < wc; i++) {
            index_size = strlen(p1) + 1 + 2 * sizeof(uint32_t);
            if (i % ENTR_PER_PAGE == 0) {
                wordoffset[j] = p1 - idxdatabuffer;
                ++j;
            }
            p1 += index_size;
        }
        wordoffset[j] = p1 - idxdatabuffer;
        if (!save_cache(url, verbose))
            printf("cache update failed\n");
    }

    if (!(idxfile = fopen(url.c_str(), "rb"))) {
        wordoffset.resize(0);
        return false;
    }

    first.assign(0, read_first_on_page_key(0));
    last.assign(wordoffset.size() - 2, read_first_on_page_key(wordoffset.size() - 2));
    middle.assign((wordoffset.size() - 2) / 2, read_first_on_page_key((wordoffset.size() - 2) / 2));
    real_last.assign(wc - 1, get_key(wc - 1));

    return true;
}

inline uint32_t OffsetIndex::load_page(int32_t page_idx)
{
    uint32_t nentr = ENTR_PER_PAGE;
    if (page_idx == int32_t(wordoffset.size() - 2))
        if ((nentr = (wordcount % ENTR_PER_PAGE)) == 0)
            nentr = ENTR_PER_PAGE;

    if (page_idx != page.idx) {
        page_data.resize(wordoffset[page_idx + 1] - wordoffset[page_idx]);
        fseek(idxfile, wordoffset[page_idx], SEEK_SET);
        const size_t nitems = fread(&page_data[0], 1, page_data.size(), idxfile);
        THROW_IF_ERROR(nitems == page_data.size());

        page.fill(&page_data[0], nentr, page_idx);
    }

    return nentr;
}

const char *OffsetIndex::get_key(int32_t idx)
{
    load_page(idx / ENTR_PER_PAGE);
    int32_t idx_in_page = idx % ENTR_PER_PAGE;
    wordentry_offset = page.entries[idx_in_page].off;
    wordentry_size = page.entries[idx_in_page].size;

    return page.entries[idx_in_page].keystr;
}

bool OffsetIndex::lookup(const char *str, int32_t &idx, std::function<int(const char*,const char*)> cmp)
{
    bool bFound = false;
    int32_t iFrom;
    int32_t iTo = wordoffset.size() - 2;
    int cmpint;
    int32_t iThisIndex;
    if (cmp(str, first.keystr.c_str()) < 0) {
        idx = 0;
        return false;
    } else if (cmp(str, real_last.keystr.c_str()) > 0) {
        idx = INVALID_INDEX;
        return false;
    } else {
        iFrom = 0;
        iThisIndex = 0;
        while (iFrom <= iTo) {
            iThisIndex = (iFrom + iTo) / 2;
            cmpint = cmp(str, get_first_on_page_key(iThisIndex));
            if (cmpint > 0)
                iFrom = iThisIndex + 1;
            else if (cmpint < 0)
                iTo = iThisIndex - 1;
            else {
                bFound = true;
                break;
            }
        }
        if (!bFound)
            idx = iTo; //prev
        else
            idx = iThisIndex;
    }
    if (!bFound) {
        uint32_t netr = load_page(idx);
        iFrom = 1; // Needn't search the first word anymore.
        iTo = netr - 1;
        iThisIndex = 0;
        while (iFrom <= iTo) {
            iThisIndex = (iFrom + iTo) / 2;
            cmpint = cmp(str, page.entries[iThisIndex].keystr);
            if (cmpint > 0)
                iFrom = iThisIndex + 1;
            else if (cmpint < 0)
                iTo = iThisIndex - 1;
            else {
                bFound = true;
                break;
            }
        }
        idx *= ENTR_PER_PAGE;
        if (!bFound)
            idx += iFrom; //next
        else
            idx += iThisIndex;
    } else {
        idx *= ENTR_PER_PAGE;
    }
    return bFound;
}

bool WordListIndex::load(const std::string &url, uint32_t wc, uint32_t fsize, bool/* verbose*/)
{
    gzFile in = gzopen(url.c_str(), "rb");
    if (in == nullptr)
        return false;

    idxdatabuf = (char *)malloc(fsize);

    const int len = gzread(in, idxdatabuf, fsize);
    gzclose(in);
    if (len < 0)
        return false;

    if (uint32_t(len) != fsize)
        return false;

    wordlist.resize(wc + 1);
    char *p1 = idxdatabuf;
    uint32_t i;
    for (i = 0; i < wc; i++) {
        wordlist[i] = p1;
        p1 += strlen(p1) + 1 + 2 * sizeof(uint32_t);
    }
    wordlist[wc] = p1;

    return true;
}

void WordListIndex::get_data(int32_t idx)
{
    char *p1 = wordlist[idx] + strlen(wordlist[idx]) + sizeof(char);
    wordentry_offset = ntohl(get_uint32(p1));
    p1 += sizeof(uint32_t);
    wordentry_size = ntohl(get_uint32(p1));
}

bool WordListIndex::lookup(const char *str, int32_t &idx, std::function<int(const char*,const char*)> cmp)
{
    bool bFound = false;
    int32_t iTo = wordlist.size() - 2;

    if (cmp(str, get_key(0)) < 0) {
        idx = 0;
    } else if (cmp(str, get_key(iTo)) > 0) {
        idx = INVALID_INDEX;
    } else {
        int32_t iThisIndex = 0;
        int32_t iFrom = 0;
        int cmpint;
        while (iFrom <= iTo) {
            iThisIndex = (iFrom + iTo) / 2;
            cmpint = cmp(str, get_key(iThisIndex));
            if (cmpint > 0)
                iFrom = iThisIndex + 1;
            else if (cmpint < 0)
                iTo = iThisIndex - 1;
            else {
                bFound = true;
                break;
            }
        }
        if (!bFound)
            idx = iFrom; //next
        else
            idx = iThisIndex;
    }
    return bFound;
}
}

bool SynFile::load(const std::string &url, uint32_t wc)
{
    struct stat stat_buf;
    if (!stat(url.c_str(), &stat_buf)) {
        MapFile syn;
        if (!syn.open(url.c_str(), stat_buf.st_size))
            return false;
        const char *current = syn.begin();
        for (unsigned long i = 0; i < wc; i++) {
            // each entry in a syn-file is:
            // - 0-terminated string
            // 4-byte index into .dict file in network byte order
            std::string synonym(current);
            current += synonym.length() + 1;
            const uint32_t idx = ntohl(get_uint32(current));
            current += sizeof(idx);
            synonyms[synonym] = idx;
        }
        return true;
    } else {
        return false;
    }
}

bool SynFile::lookup(const char *str, int32_t &idx)
{
    char *lower_string = g_utf8_strdown(str);
    auto it = synonyms.find((lower_string));
    free(lower_string);
    if (it != synonyms.end()) {
        idx = it->second;
        return true;
    }
    return false;
}

bool Dict::Lookup(const char *str, int32_t &idx, bool ignorecase)
{
    return syn_file->lookup(str, idx) || idx_file->lookup(str, idx, ignorecase ? strcasecmp : stardict_strcmp);
}

bool Dict::load(const std::string &ifofilename, bool verbose)
{
    uint32_t idxfilesize;
    if (!load_ifofile(ifofilename, idxfilesize))
        return false;

    std::string basefilename(ifofilename, 0, ifofilename.size() - (sizeof("ifo") - 1));
    std::string fullfilename(basefilename + "dict.dz");

    if (!access(fullfilename.c_str(), R_OK)) {
        dictdzfile.reset(new DictData);
        if (!dictdzfile->open(fullfilename, 0)) {
            //g_print("open file %s failed!\n",fullfilename);
            return false;
        }
    } else {
        fullfilename = basefilename + "dict";
        dictfile = fopen(fullfilename.c_str(), "rb");
        if (!dictfile) {
            //g_print("open file %s failed!\n",fullfilename);
            return false;
        }
    }

    fullfilename = basefilename + "idx.gz";
    if (!access(fullfilename.c_str(), R_OK)) {
        idx_file.reset(new WordListIndex);
    } else {
        fullfilename = basefilename + "idx";
        idx_file.reset(new OffsetIndex);
    }

    if (!idx_file->load(fullfilename, wordcount, idxfilesize, verbose))
        return false;

    fullfilename = basefilename + "syn";
    syn_file.reset(new SynFile);
    syn_file->load(fullfilename, syn_wordcount);

    //g_print("bookname: %s , wordcount %lu\n", bookname.c_str(), narticles());
    return true;
}

bool Dict::load_ifofile(const std::string &ifofilename, uint32_t &idxfilesize)
{
    const auto &&ifo = load_from_ifo_file(ifofilename, false);
    if (ifo.size() < 1)
        return false;
    try {
        wordcount = strtoul(ifo.at("wordcount").c_str(), NULL, 10);
        if (wordcount == 0) {
            return false;
        }
        ifo_file_name = ifo.at("ifo_file_name");
        auto it = ifo.find("synwordcount");
        if (it != ifo.end())
            syn_wordcount = strtoul(it->second.c_str(), NULL, 10);
        else
            syn_wordcount = 0;
        bookname = ifo.at("bookname");

        idxfilesize = strtoul(ifo.at("idxfilesize").c_str(), NULL, 10);

        it = ifo.find("sametypesequence");
        if (it != ifo.end())
            sametypesequence = it->second;
    } catch (const std::out_of_range &) {
        return false;
    }
    return true;
}

bool Dict::LookupWithRule(const std::regex &spec, int32_t *aIndex, int iBuffLen)
{
    int iIndexCount = 0;

    for (uint32_t i = 0; i < narticles() && iIndexCount < (iBuffLen - 1); i++)
        if (std::regex_match(get_key(i), spec))
        //if (g_pattern_match_string(pspec, get_key(i)))
            aIndex[iIndexCount++] = i;

    aIndex[iIndexCount] = -1; // -1 is the end.
    return iIndexCount > 0;
}

Libs::~Libs()
{
    for (Dict *p : oLib)
        delete p;
}

bool Libs::load_dict(const std::string &url)
{
    Dict *lib = new Dict;
    if (lib->load(url, true)) {
        oLib.push_back(lib);
        return true;
    }
    delete lib;
    return false;
}

void Libs::load(const std::list<std::string> &dicts_dirs,
                const std::list<std::string> &order_list,
                const std::list<std::string> &disable_list)
{
    int order_cnt = 0;
    for_each_file(dicts_dirs, ".ifo", order_list, disable_list,
                  [this, &order_cnt](const std::string &url, bool ordered) {
                      if (load_dict(url) && ordered)
                          order_cnt++;
                  });
    auto i = oLib.begin();
    while (--order_cnt >= 0)
        ++i;
    std::sort(i, oLib.end(), [](Dict *l, Dict *r) -> bool {
        if (l && r) {
            return l->ifofilename() < r->ifofilename();
        }
        return false;
    });
}

const char *Libs::poGetCurrentWord(int32_t *iCurrent)
{
    const char *poCurrentWord = nullptr;
    const char *word;
    for (uint32_t iLib = 0; iLib < oLib.size(); iLib++) {
        if (iCurrent[iLib] == INVALID_INDEX)
            continue;
        if (iCurrent[iLib] >= narticles(iLib) || iCurrent[iLib] < 0)
            continue;
        if (poCurrentWord == nullptr) {
            poCurrentWord = poGetWord(iCurrent[iLib], iLib);
        } else {
            word = poGetWord(iCurrent[iLib], iLib);

            if (stardict_strcmp(poCurrentWord, word) > 0)
                poCurrentWord = word;
        }
    }
    return poCurrentWord;
}

const char *Libs::poGetNextWord(const char *sWord, int32_t *iCurrent)
{
    // the input can be:
    // (word,iCurrent),read word,write iNext to iCurrent,and return next word. used by TopWin::NextCallback();
    // (nullptr,iCurrent),read iCurrent,write iNext to iCurrent,and return next word. used by AppCore::ListWords();
    const char *poCurrentWord = nullptr;
    uint32_t iCurrentLib = 0;
    const char *word;

    for (uint32_t iLib = 0; iLib < oLib.size(); ++iLib) {
        if (sWord)
            oLib[iLib]->Lookup(sWord, iCurrent[iLib], false);
        if (iCurrent[iLib] == INVALID_INDEX)
            continue;
        if (iCurrent[iLib] >= narticles(iLib) || iCurrent[iLib] < 0)
            continue;
        if (poCurrentWord == nullptr) {
            poCurrentWord = poGetWord(iCurrent[iLib], iLib);
            iCurrentLib = iLib;
        } else {
            word = poGetWord(iCurrent[iLib], iLib);

            if (stardict_strcmp(poCurrentWord, word) > 0) {
                poCurrentWord = word;
                iCurrentLib = iLib;
            }
        }
    }
    if (poCurrentWord) {
        iCurrent[iCurrentLib]++;
        for (uint32_t iLib = 0; iLib < oLib.size(); iLib++) {
            if (iLib == iCurrentLib)
                continue;
            if (iCurrent[iLib] == INVALID_INDEX)
                continue;
            if (iCurrent[iLib] >= narticles(iLib) || iCurrent[iLib] < 0)
                continue;
            if (strcmp(poCurrentWord, poGetWord(iCurrent[iLib], iLib)) == 0)
                iCurrent[iLib]++;
        }
        poCurrentWord = poGetCurrentWord(iCurrent);
    }
    return poCurrentWord;
}

const char *
Libs::poGetPreWord(int32_t *iCurrent)
{
    // used by TopWin::PreviousCallback(); the iCurrent is cached by AppCore::TopWinWordChange();
    const char *poCurrentWord = nullptr;
    std::vector<Dict *>::size_type iCurrentLib = 0;
    const char *word;

    for (uint32_t iLib = 0; iLib < oLib.size(); iLib++) {
        if (iCurrent[iLib] == INVALID_INDEX)
            iCurrent[iLib] = narticles(iLib);
        else {
            if (iCurrent[iLib] > narticles(iLib) || iCurrent[iLib] <= 0)
                continue;
        }
        if (poCurrentWord == nullptr) {
            poCurrentWord = poGetWord(iCurrent[iLib] - 1, iLib);
            iCurrentLib = iLib;
        } else {
            word = poGetWord(iCurrent[iLib] - 1, iLib);
            if (stardict_strcmp(poCurrentWord, word) < 0) {
                poCurrentWord = word;
                iCurrentLib = iLib;
            }
        }
    }

    if (poCurrentWord) {
        iCurrent[iCurrentLib]--;
        for (uint32_t iLib = 0; iLib < oLib.size(); iLib++) {
            if (iLib == iCurrentLib)
                continue;
            if (iCurrent[iLib] > narticles(iLib) || iCurrent[iLib] <= 0)
                continue;
            if (strcmp(poCurrentWord, poGetWord(iCurrent[iLib] - 1, iLib)) == 0) {
                iCurrent[iLib]--;
            } else {
                if (iCurrent[iLib] == narticles(iLib))
                    iCurrent[iLib] = INVALID_INDEX;
            }
        }
    }
    return poCurrentWord;
}

bool Libs::LookupSimilarWord(const char *sWord, int32_t &iWordIndex, int iLib)
{
    int32_t iIndex;
    bool bFound;

    // to lower case.
    bFound = oLib[iLib]->Lookup(sWord, iIndex, true);

    if (bIsPureEnglish(sWord)) {
        // If not Found , try other status of sWord.
        int iWordLen = strlen(sWord);

        char *sNewWord = (char *)malloc(iWordLen + 1);

        //cut one char "s" or "d"
        if (!bFound && iWordLen > 1) {
            if (sWord[iWordLen - 1] == 'S' || sWord[iWordLen - 1] == 's'|| !strncasecmp(&sWord[iWordLen - 2], "ed", 2)) {
                strcpy(sNewWord, sWord);
                sNewWord[iWordLen - 1] = '\0'; // cut "s" or "d"
                bFound = oLib[iLib]->Lookup(sNewWord, iIndex, true);
            }
        }

        //cut "ly"
        if (!bFound && iWordLen > 2) {
            if (!strncasecmp(&sWord[iWordLen - 2], "ly", 2)) {
                strcpy(sNewWord, sWord);
                sNewWord[iWordLen - 2] = '\0'; // cut "ly"
                if (iWordLen > 5 && sNewWord[iWordLen - 3] == sNewWord[iWordLen - 4]
                    && !bIsVowel(sNewWord[iWordLen - 4]) && bIsVowel(sNewWord[iWordLen - 5])) { //doubled

                    sNewWord[iWordLen - 3] = '\0';
                    bFound = oLib[iLib]->Lookup(sNewWord, iIndex, true);
                    if (!bFound)
                        sNewWord[iWordLen - 3] = sNewWord[iWordLen - 4]; //restore
                }
                if (!bFound) {
                    bFound = oLib[iLib]->Lookup(sNewWord, iIndex, true);
                }
            }
        }

        //cut "ing"
        if (!bFound && iWordLen > 3) {
            if (!strncasecmp(&sWord[iWordLen - 3], "ing", 3)) {
                strcpy(sNewWord, sWord);
                sNewWord[iWordLen - 3] = '\0';
                if (iWordLen > 6 && (sNewWord[iWordLen - 4] == sNewWord[iWordLen - 5])
                    && !bIsVowel(sNewWord[iWordLen - 5]) && bIsVowel(sNewWord[iWordLen - 6])) { //doubled
                    sNewWord[iWordLen - 4] = '\0';
                    bFound = oLib[iLib]->Lookup(sNewWord, iIndex, true);
                    if (!bFound)
                        sNewWord[iWordLen - 4] = sNewWord[iWordLen - 5]; //restore
                }
                if (!bFound) {
                    bFound = oLib[iLib]->Lookup(sNewWord, iIndex, true);
                }
                if (!bFound) {
                    strcat(sNewWord, "e"); // add a char "e"
                    bFound = oLib[iLib]->Lookup(sNewWord, iIndex, true);
                }
            }
        }

        //cut two char "es"
        if (!bFound && iWordLen > 3) {
            if ((!strncasecmp(&sWord[iWordLen - 2], "es", 2) &&
                    (sWord[iWordLen - 3] == 's' || sWord[iWordLen - 3] == 'x' || sWord[iWordLen - 3] == 'o' ||
                    sWord[iWordLen - 3] == 'S' || sWord[iWordLen - 3] == 'X' || sWord[iWordLen - 3] == 'O' ||
                    (iWordLen > 4 && (sWord[iWordLen - 3] == 'h' || sWord[iWordLen - 3] == 'H') &&
                    (sWord[iWordLen - 4] == 'c' || sWord[iWordLen - 4] == 's' || sWord[iWordLen - 4] == 'C' || sWord[iWordLen - 4] == 'S'))))) {
                strcpy(sNewWord, sWord);
                sNewWord[iWordLen - 2] = '\0';
                bFound = oLib[iLib]->Lookup(sNewWord, iIndex, true);
            }
        }

        //cut "ed"
        if (!bFound && iWordLen > 3) {
            if (!strncasecmp(&sWord[iWordLen - 2], "ed", 2)) {
                strcpy(sNewWord, sWord);
                sNewWord[iWordLen - 2] = '\0';
                if (iWordLen > 5 && (sNewWord[iWordLen - 3] == sNewWord[iWordLen - 4])
                    && !bIsVowel(sNewWord[iWordLen - 4]) && bIsVowel(sNewWord[iWordLen - 5])) { //doubled
                    sNewWord[iWordLen - 3] = '\0';
                    bFound = oLib[iLib]->Lookup(sNewWord, iIndex, true);
                    if (!bFound)
                        sNewWord[iWordLen - 3] = sNewWord[iWordLen - 4]; //restore
                }
                if (!bFound) {
                    bFound = oLib[iLib]->Lookup(sNewWord, iIndex, true);
                }
            }
        }

        // cut "ied" , add "y".
        if (!bFound && iWordLen > 3) {
            if (!strncasecmp(&sWord[iWordLen - 3], "ied", 3)) {
                strcpy(sNewWord, sWord);
                sNewWord[iWordLen - 3] = '\0';
                strcat(sNewWord, "y"); // add a char "y"
                bFound = oLib[iLib]->Lookup(sNewWord, iIndex, true);
            }
        }

        // cut "ies" , add "y".
        if (!bFound && iWordLen > 3) {
            if (!strncasecmp(&sWord[iWordLen - 3], "ies", 3)) {
                strcpy(sNewWord, sWord);
                sNewWord[iWordLen - 3] = '\0';
                strcat(sNewWord, "y"); // add a char "y"
                bFound = oLib[iLib]->Lookup(sNewWord, iIndex, true);
            }
        }

        // cut "er".
        if (!bFound && iWordLen > 2) {
            if (!strncasecmp(&sWord[iWordLen - 2], "er", 2)) {
                strcpy(sNewWord, sWord);
                sNewWord[iWordLen - 2] = '\0';
                bFound = oLib[iLib]->Lookup(sNewWord, iIndex, true);
            }
        }

        // cut "est".
        if (!bFound && iWordLen > 3) {
            if (!strncasecmp(&sWord[iWordLen - 3], "est", 3)) {
                strcpy(sNewWord, sWord);
                sNewWord[iWordLen - 3] = '\0';
                bFound = oLib[iLib]->Lookup(sNewWord, iIndex, true);
            }
        }

        free(sNewWord);
    }

    if (bFound)
        iWordIndex = iIndex;
#if 0
    else {
        //don't change iWordIndex here.
        //when LookupSimilarWord all failed too, we want to use the old LookupWord index to list words.
        //iWordIndex = INVALID_INDEX;
    }
#endif
    return bFound;
}

bool Libs::SimpleLookupWord(const char *sWord, int32_t &iWordIndex, int iLib)
{
    bool bFound = oLib[iLib]->Lookup(sWord, iWordIndex, false);

    if (!bFound && !param_.no_fuzzy)
        bFound = LookupSimilarWord(sWord, iWordIndex, iLib);
    return bFound;
}

bool Libs::LookupWithFuzzy(const char *sWord, char *reslist[], int reslist_size)
{
#if 1
    if (sWord[0] == '\0')
        return false;
#define TCH char
    std::vector<Fuzzystruct> oFuzzystruct(reslist_size);
    //Fuzzystruct oFuzzystruct[reslist_size];

    for (int i = 0; i < reslist_size; i++) {
        oFuzzystruct[i].pMatchWord = nullptr;
        oFuzzystruct[i].iMatchWordDistance = iMaxFuzzyDistance;
    }
    int iMaxDistance = iMaxFuzzyDistance;
    int iDistance;
    bool Found = false;
    EditDistance oEditDistance;

    int32_t iCheckWordLen;
    const char *sCheck;
    TCH *ucs4_str1, *ucs4_str2;
    int32_t ucs4_str2_len;

#if 0
    ucs4_str2 = g_utf8_to_ucs4_fast(sWord, -1, &ucs4_str2_len);
    unicode_strdown(ucs4_str2);
#else
    // only do english...
    ucs4_str2 = g_utf8_strdown(sWord);
    ucs4_str2_len = strlen(ucs4_str2);
#endif

    for (size_t iLib = 0; iLib < oLib.size(); ++iLib) {
        if (progress_func)
            progress_func();

        //if (stardict_strcmp(sWord, poGetWord(0,iLib))>=0 && stardict_strcmp(sWord, poGetWord(narticles(iLib)-1,iLib))<=0) {
        //there are Chinese dicts and English dicts...

        const int iwords = narticles(iLib);
        for (int index = 0; index < iwords; index++) {
            sCheck = poGetWord(index, iLib);
            // tolower and skip too long or too short words
            iCheckWordLen = strlen(sCheck);
            if (iCheckWordLen - ucs4_str2_len >= iMaxDistance || ucs4_str2_len - iCheckWordLen >= iMaxDistance)
                continue;
#if 0
            ucs4_str1 = g_utf8_to_ucs4_fast(sCheck, -1, nullptr);
            if (iCheckWordLen > ucs4_str2_len)
                ucs4_str1[ucs4_str2_len] = 0;
            unicode_strdown(ucs4_str1);
#else
            // only do english...
            ucs4_str1 = g_utf8_strdown(sCheck);
            if (iCheckWordLen > ucs4_str2_len)
                ucs4_str1[ucs4_str2_len] = 0;
#endif
            iDistance = oEditDistance.CalEditDistance(ucs4_str1, ucs4_str2, iMaxDistance);
            free(ucs4_str1);
            if (iDistance < iMaxDistance && iDistance < ucs4_str2_len) {
                // when ucs4_str2_len=1,2 we need less fuzzy.
                Found = true;
                bool bAlreadyInList = false;
                int iMaxDistanceAt = 0;
                for (int j = 0; j < reslist_size; j++) {
                    if (oFuzzystruct[j].pMatchWord && strcmp(oFuzzystruct[j].pMatchWord, sCheck) == 0) { //already in list
                        bAlreadyInList = true;
                        break;
                    }
                    //find the position,it will certainly be found (include the first time) as iMaxDistance is set by last time.
                    if (oFuzzystruct[j].iMatchWordDistance == iMaxDistance) {
                        iMaxDistanceAt = j;
                    }
                }
                if (!bAlreadyInList) {
                    if (oFuzzystruct[iMaxDistanceAt].pMatchWord)
                        free(oFuzzystruct[iMaxDistanceAt].pMatchWord);
                    oFuzzystruct[iMaxDistanceAt].pMatchWord = strdup(sCheck);
                    oFuzzystruct[iMaxDistanceAt].iMatchWordDistance = iDistance;
                    // calc new iMaxDistance
                    iMaxDistance = iDistance;
                    for (int j = 0; j < reslist_size; j++) {
                        if (oFuzzystruct[j].iMatchWordDistance > iMaxDistance)
                            iMaxDistance = oFuzzystruct[j].iMatchWordDistance;
                    } // calc new iMaxDistance
                } // add to list
            } // find one
        } // each word

    } // each lib
    free(ucs4_str2);

    if (Found) // sort with distance
        std::sort(oFuzzystruct.begin(), oFuzzystruct.end(), [](const Fuzzystruct &lh, const Fuzzystruct &rh) -> bool {
            if (lh.iMatchWordDistance != rh.iMatchWordDistance)
                return lh.iMatchWordDistance < rh.iMatchWordDistance;

            if (lh.pMatchWord && rh.pMatchWord)
                return stardict_strcmp(lh.pMatchWord, rh.pMatchWord) < 0;

            return false;
        });

    for (int i = 0; i < reslist_size; ++i)
        reslist[i] = oFuzzystruct[i].pMatchWord;

    return Found;
#else
    return false;
#endif
}

int Libs::LookupWithRule(const char *word, char **ppMatchWord)
{
    int32_t aiIndex[MAX_MATCH_ITEM_PER_LIB + 1];
    int iMatchCount = 0;

    try {
        std::regex spec(word, std::regex::egrep | std::regex::icase | std::regex::nosubs);
        for (std::vector<Dict *>::size_type iLib = 0; iLib < oLib.size(); iLib++) {

            if (oLib[iLib]->LookupWithRule(spec, aiIndex, MAX_MATCH_ITEM_PER_LIB + 1)) {
                if (progress_func)
                    progress_func();
                for (int i = 0; aiIndex[i] != -1; i++) {
                    const char *sMatchWord = poGetWord(aiIndex[i], iLib);
                    bool bAlreadyInList = false;
                    for (int j = 0; j < iMatchCount; j++) {
                        if (strcmp(ppMatchWord[j], sMatchWord) == 0) { //already in list
                            bAlreadyInList = true;
                            break;
                        }
                    }
                    if (!bAlreadyInList)
                        ppMatchWord[iMatchCount++] = strdup(sMatchWord);
                }
            }
        }
        //g_pattern_spec_free(pspec);
    } catch (const std::regex_error &) {
        printf("Regex error:%s\n", word);
        return 0;
    }

    if (iMatchCount) // sort it.
        std::sort(ppMatchWord, ppMatchWord + iMatchCount, [](const char *lh, const char *rh) -> bool {
            return stardict_strcmp(lh, rh) < 0;
        });

    return iMatchCount;
}
bool Libs::LookupData(const char *sWord, std::vector<char *> *reslist)
{
    std::vector<std::string> SearchWords;
    std::string SearchWord;
    const char *p = sWord;
    while (*p) {
        if (*p == '\\') {
            p++;
            switch (*p) {
            case ' ':
                SearchWord += ' ';
                break;
            case '\\':
                SearchWord += '\\';
                break;
            case 't':
                SearchWord += '\t';
                break;
            case 'n':
                SearchWord += '\n';
                break;
            default:
                SearchWord += *p;
            }
        } else if (*p == ' ') {
            if (!SearchWord.empty()) {
                SearchWords.push_back(SearchWord);
                SearchWord.clear();
            }
        } else {
            SearchWord += *p;
        }
        p++;
    }
    if (!SearchWord.empty()) {
        SearchWords.push_back(SearchWord);
        SearchWord.clear();
    }
    if (SearchWords.empty())
        return false;

    uint32_t max_size = 0;
    char *origin_data = nullptr;
    for (std::vector<Dict *>::size_type i = 0; i < oLib.size(); ++i) {
        if (!oLib[i]->containSearchData())
            continue;
        if (progress_func)
            progress_func();
        const int iwords = narticles(i);
        const char *key;
        uint32_t offset, size;
        for (int j = 0; j < iwords; ++j) {
            oLib[i]->get_key_and_data(j, &key, &offset, &size);
            if (size > max_size) {
                origin_data = (char *)realloc(origin_data, size);
                max_size = size;
            }
            if (oLib[i]->SearchData(SearchWords, offset, size, origin_data))
                reslist[i].push_back(strdup(key));
        }
    }
    free(origin_data);

    std::vector<Dict *>::size_type i;
    for (i = 0; i < oLib.size(); ++i)
        if (!reslist[i].empty())
            break;

    return i != oLib.size();
}

/**************************************************/
std::map<std::string, std::string> load_from_ifo_file(const std::string &ifofilename, bool istreedict)
{
    std::map<std::string, std::string> result;
    char *buffer = g_file_get_contents(ifofilename.c_str());
    if (nullptr == buffer)
        return result;

    static const char TREEDICT_MAGIC_DATA[] = "StarDict's treedict ifo file";
    static const char DICT_MAGIC_DATA[] = "StarDict's dict ifo file";

    const char *magic_data = istreedict ? TREEDICT_MAGIC_DATA : DICT_MAGIC_DATA;
    const int magic_len = (istreedict ? (sizeof(TREEDICT_MAGIC_DATA) / sizeof(char))
            : (sizeof(DICT_MAGIC_DATA) / sizeof(char))) - 1;
    static const unsigned char utf8_bom[] = { 0xEF, 0xBB, 0xBF, '\0' };
    //if (!g_str_has_prefix(
    //        g_str_has_prefix((buffer), (const char *)(utf8_bom)) ? (buffer) + 3 : (buffer),
    //        magic_data)) {
    char *checker = buffer;
    if (!strncmp((const char*)utf8_bom, checker, 3))
        checker += 3;
    if (strncmp(magic_data, checker, magic_len)) {
        return result;
    }

    checker += magic_len;
    char *t = strtok_r(checker, "\r\n", &checker);
    while (t) {
        char *eq = strchr(t, '=');
        if (eq) {
            *eq++ = '\0';
            result[t] = eq;
        }
        t = strtok_r(nullptr, "\r\n", &checker);
    }
    free(buffer);
    result["ifo_file_name"] = ifofilename;
    return result;
}
query_t analyze_query(const char *s, std::string &res)
{
    if (!s || !*s) {
        res = s;
        return qtSIMPLE;
    }
    if (*s == '/') {
        res = s + 1;
        return qtFUZZY;
    }

    if (*s == '|') {
        res = s + 1;
        return qtDATA;
    }

    bool regexp = false;
    const char *p = s;
    res = "";
    for (; *p; res += *p, ++p) {
        if (*p == '\\') {
            ++p;
            if (!*p)
                break;
            continue;
        }
        if (*p == '*' || *p == '?')
            regexp = true;
    }
    if (regexp)
        return qtREGEXP;

    return qtSIMPLE;
}
