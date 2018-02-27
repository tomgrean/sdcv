#pragma once

#include <cassert>
#include <cstddef>
#include <functional>
#include <list>
#include <cstring>
#include <string>

#ifndef G_DIR_SEPARATOR
#ifdef _WIN32
#define G_DIR_SEPARATOR "\\"
#else
#define G_DIR_SEPARATOR "/"
#endif
#endif

#if abcdef
template <typename T, typename unref_res_t, void (*unref_res)(unref_res_t *)>
class ResourceWrapper
{
public:
    ResourceWrapper(T *p = nullptr)
        : p_(p)
    {
    }
    ~ResourceWrapper() { free_resource(); }
    ResourceWrapper(const ResourceWrapper &) = delete;
    ResourceWrapper &operator=(const ResourceWrapper &) = delete;
    T *operator->() const { return p_; }
    bool operator!() const { return p_ == nullptr; }
    const T &operator[](size_t idx) const
    {
        assert(p_ != nullptr);
        return p_[idx];
    }

    void reset(T *newp)
    {
        if (p_ != newp) {
            free_resource();
            p_ = newp;
        }
    }

    friend inline bool operator==(const ResourceWrapper &lhs, std::nullptr_t) noexcept
    {
        return !lhs.p_;
    }

    friend inline bool operator!=(const ResourceWrapper &lhs, std::nullptr_t) noexcept
    {
        return !!lhs.p_;
    }

    friend inline T *get_impl(const ResourceWrapper &rw)
    {
        return rw.p_;
    }

    friend inline T **get_addr(ResourceWrapper &rw)
    {
        return &rw.p_;
    }

private:
    T *p_;

    void free_resource()
    {
        if (p_)
            unref_res(p_);
    }
};
#endif

extern void for_each_file(const std::list<std::string> &dirs_list, const std::string &suff,
                          const std::list<std::string> &order_list, const std::list<std::string> &disable_list,
                          const std::function<void(const std::string &, bool)> &f);
extern std::string json_escape_string(const std::string &str);
