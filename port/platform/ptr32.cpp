// Handle table behind ptr32<T> (see port/compat/port_ptr32.hpp)
#include "port_prelude.h"
#include "port_ptr32.hpp"
#include <unordered_map>
#include <vector>

void** gPortHandleTable = NULL;
uint32_t gPortHandleCount = 0;

namespace
{
std::vector<void*>& Table()
{
    static std::vector<void*> t(1, nullptr); // handle 0 == NULL
    return t;
}
std::unordered_map<const void*, uint32_t>& Index()
{
    static std::unordered_map<const void*, uint32_t> m;
    return m;
}
} // namespace

uint32_t port_ptr_to_handle(const void* p)
{
    if (!p)
    {
        return 0;
    }
    auto& idx = Index();
    auto it = idx.find(p);
    if (it != idx.end())
    {
        return it->second;
    }
    auto& t = Table();
    uint32_t h = static_cast<uint32_t>(t.size());
    t.push_back(const_cast<void*>(p));
    idx.emplace(p, h);
    gPortHandleTable = t.data();
    gPortHandleCount = static_cast<uint32_t>(t.size());
    return h;
}
