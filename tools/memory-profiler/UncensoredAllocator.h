/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef memory_profiler_UncensoredAllocator_h
#define memory_profiler_UncensoredAllocator_h

#include "mozilla/Compiler.h"

#include <string>
#include <unordered_map>
#include <vector>

class NativeProfiler;

#if MOZ_USING_STLPORT
namespace std {
using tr1::unordered_map;
} // namespace std
#endif

namespace mozilla {

void InitializeMallocHook();
void EnableMallocHook(NativeProfiler* aNativeProfiler);
void DisableMallocHook();
void* u_malloc(size_t size);
void u_free(void* ptr);

#ifdef MOZ_REPLACE_MALLOC
template<class Tp>
struct UncensoredAllocator
{
  typedef size_t size_type;
  typedef ptrdiff_t difference_type;
  typedef Tp* pointer;
  typedef const Tp* const_pointer;
  typedef Tp& reference;
  typedef const Tp& const_reference;
  typedef Tp value_type;

  UncensoredAllocator() {}

  template<class T>
  UncensoredAllocator(const UncensoredAllocator<T>&) {}

  template<class Other>
  struct rebind
  {
    typedef UncensoredAllocator<Other> other;
  };
  Tp* allocate(size_t n)
  {
    return reinterpret_cast<Tp*>(u_malloc(n * sizeof(Tp)));
  }
  void deallocate(Tp* p, size_t n)
  {
    u_free(reinterpret_cast<void*>(p));
  }
  void construct(Tp* p, const Tp& val)
  {
    new ((void*)p) Tp(val);
  }
  void destroy(Tp* p)
  {
    p->Tp::~Tp();
  }
  bool operator==(const UncensoredAllocator& rhs) const
  {
    return true;
  }
  bool operator!=(const UncensoredAllocator& rhs) const
  {
    return false;
  }
  size_type max_size() const
  {
    return static_cast<size_type>(-1) / sizeof(Tp);
  }
};

using u_string =
  std::basic_string<char, std::char_traits<char>, UncensoredAllocator<char>>;

template<typename T>
using u_vector = std::vector<T, UncensoredAllocator<T>>;

template<typename K, typename V, typename H = std::hash<K>>
using u_unordered_map =
  std::unordered_map<K, V, H, std::equal_to<K>, UncensoredAllocator<std::pair<K, V>>>;

#else

using u_string = std::string;
template<typename T>
using u_vector = std::vector<T>;
template<typename K, typename V, typename H = std::hash<K>>
using u_unordered_map =
  std::unordered_map<K, V, H>;

#endif
} // namespace mozilla

#endif // memory_profiler_UncensoredAllocator_h
