#ifndef ICU4XList_HPP
#define ICU4XList_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XList.h"

class ICU4XList;

/**
 * A destruction policy for using ICU4XList with std::unique_ptr.
 */
struct ICU4XListDeleter {
  void operator()(capi::ICU4XList* l) const noexcept {
    capi::ICU4XList_destroy(l);
  }
};

/**
 * A list of strings
 */
class ICU4XList {
 public:

  /**
   * Create a new list of strings
   */
  static ICU4XList create();

  /**
   * Create a new list of strings with preallocated space to hold
   * at least `capacity` elements
   */
  static ICU4XList create_with_capacity(size_t capacity);

  /**
   * Push a string to the list
   * 
   * For C++ users, potentially invalid UTF8 will be handled via
   * REPLACEMENT CHARACTERs
   */
  void push(const std::string_view val);

  /**
   * The number of elements in this list
   */
  size_t len() const;
  inline const capi::ICU4XList* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XList* AsFFIMut() { return this->inner.get(); }
  inline ICU4XList(capi::ICU4XList* i) : inner(i) {}
  ICU4XList() = default;
  ICU4XList(ICU4XList&&) noexcept = default;
  ICU4XList& operator=(ICU4XList&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XList, ICU4XListDeleter> inner;
};


inline ICU4XList ICU4XList::create() {
  return ICU4XList(capi::ICU4XList_create());
}
inline ICU4XList ICU4XList::create_with_capacity(size_t capacity) {
  return ICU4XList(capi::ICU4XList_create_with_capacity(capacity));
}
inline void ICU4XList::push(const std::string_view val) {
  capi::ICU4XList_push(this->inner.get(), val.data(), val.size());
}
inline size_t ICU4XList::len() const {
  return capi::ICU4XList_len(this->inner.get());
}
#endif
