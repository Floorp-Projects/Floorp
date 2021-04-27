#pragma once
// IWYU pragma: private, include "rlbox.hpp"
// IWYU pragma: friend "rlbox_.*\.hpp"

#include <map>
#ifndef RLBOX_USE_CUSTOM_SHARED_LOCK
#  include <shared_mutex>
#endif
#include <type_traits>

#include "rlbox_helpers.hpp"
#include "rlbox_type_traits.hpp"
#include "rlbox_types.hpp"

namespace rlbox {

template<typename T_PointerType>
class app_pointer_map
{

private:
  using T_PointerTypeUnsigned = detail::unsigned_int_of_size_t<T_PointerType>;

  std::map<T_PointerTypeUnsigned, void*> pointer_map;
  T_PointerTypeUnsigned counter = 1;
  RLBOX_SHARED_LOCK(map_mutex);

  T_PointerType get_unused_index()
  {
    constexpr auto max_val = std::numeric_limits<T_PointerTypeUnsigned>::max();
    constexpr auto min_val = std::numeric_limits<T_PointerTypeUnsigned>::min();
    for (T_PointerTypeUnsigned i = counter; i < max_val; i++) {
      if (pointer_map.find(i) == pointer_map.end()) {
        counter = i + 1;
        return reinterpret_cast<T_PointerType>(i);
      }
    }
    for (T_PointerTypeUnsigned i = min_val; i < counter; i++) {
      if (pointer_map.find(i) == pointer_map.end()) {
        counter = i + 1;
        return reinterpret_cast<T_PointerType>(i);
      }
    }
    detail::dynamic_check(false, "Could not find free app pointer slot");
    return 0;
  }

public:
  app_pointer_map()
  {
    // ensure we can't use app pointer 0 as this is sometimes confused as null
    // by the sandbox
    pointer_map[0] = nullptr;
  }

  T_PointerType get_app_pointer_idx(void* ptr)
  {
    RLBOX_ACQUIRE_UNIQUE_GUARD(lock, map_mutex);
    T_PointerType idx = get_unused_index();
    T_PointerTypeUnsigned idx_int =
      reinterpret_cast<T_PointerTypeUnsigned>(idx);
    pointer_map[idx_int] = ptr;
    return idx;
  }

  void remove_app_ptr(T_PointerType idx)
  {
    RLBOX_ACQUIRE_UNIQUE_GUARD(lock, map_mutex);
    T_PointerTypeUnsigned idx_int =
      reinterpret_cast<T_PointerTypeUnsigned>(idx);
    auto it = pointer_map.find(idx_int);
    detail::dynamic_check(it != pointer_map.end(),
                          "Error: removing a non-existing app pointer");
    pointer_map.erase(it);
  }

  void* lookup_index(T_PointerType idx)
  {
    RLBOX_ACQUIRE_SHARED_GUARD(lock, map_mutex);
    T_PointerTypeUnsigned idx_int =
      reinterpret_cast<T_PointerTypeUnsigned>(idx);
    auto it = pointer_map.find(idx_int);
    detail::dynamic_check(it != pointer_map.end(),
                          "Error: looking up a non-existing app pointer");
    return it->second;
  }
};

}