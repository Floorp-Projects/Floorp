/*
 * Copyright © 2016 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */

#if !defined(CUBEB_UTILS)
#define CUBEB_UTILS

#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <type_traits>
#if defined(WIN32)
#include "cubeb_utils_win.h"
#else
#include "cubeb_utils_unix.h"
#endif

/** Similar to memcpy, but accounts for the size of an element. */
template<typename T>
void PodCopy(T * destination, const T * source, size_t count)
{
  static_assert(std::is_trivial<T>::value, "Requires trivial type");
  memcpy(destination, source, count * sizeof(T));
}

/** Similar to memmove, but accounts for the size of an element. */
template<typename T>
void PodMove(T * destination, const T * source, size_t count)
{
  static_assert(std::is_trivial<T>::value, "Requires trivial type");
  memmove(destination, source, count * sizeof(T));
}

/** Similar to a memset to zero, but accounts for the size of an element. */
template<typename T>
void PodZero(T * destination, size_t count)
{
  static_assert(std::is_trivial<T>::value, "Requires trivial type");
  memset(destination, 0,  count * sizeof(T));
}

template<typename T>
class auto_array
{
public:
  explicit auto_array(uint32_t capacity = 0)
    : data_(capacity ? new T[capacity] : nullptr)
    , capacity_(capacity)
    , length_(0)
  {}

  ~auto_array()
  {
    delete [] data_;
  }

  /** Get a constant pointer to the underlying data. */
  T * data() const
  {
    return data_;
  }

  const T& at(size_t index) const
  {
    assert(index < length_ && "out of range");
    return data_[index];
  }

  T& at(size_t index)
  {
    assert(index < length_ && "out of range");
    return data_[index];
  }

  /** Get how much underlying storage this auto_array has. */
  size_t capacity() const
  {
    return capacity_;
  }

  /** Get how much elements this auto_array contains. */
  size_t length() const
  {
    return length_;
  }

  /** Keeps the storage, but removes all the elements from the array. */
  void clear()
  {
    length_ = 0;
  }

   /** Change the storage of this auto array, copying the elements to the new
    * storage.
    * @returns true in case of success
    * @returns false if the new capacity is not big enough to accomodate for the
    *                elements in the array.
    */
  bool reserve(size_t new_capacity)
  {
    if (new_capacity < length_) {
      return false;
    }
    T * new_data = new T[new_capacity];
    if (data_ && length_) {
      PodCopy(new_data, data_, length_);
    }
    capacity_ = new_capacity;
    delete [] data_;
    data_ = new_data;

    return true;
  }

   /** Append `length` elements to the end of the array, resizing the array if
    * needed.
    * @parameter elements the elements to append to the array.
    * @parameter length the number of elements to append to the array.
    */
  void push(const T * elements, size_t length)
  {
    if (length_ + length > capacity_) {
      reserve(length_ + length);
    }
    PodCopy(data_ + length_, elements, length);
    length_ += length;
  }

  /** Append `length` zero-ed elements to the end of the array, resizing the
   * array if needed.
   * @parameter length the number of elements to append to the array.
   */
  void push_silence(size_t length)
  {
    if (length_ + length > capacity_) {
      reserve(length + length_);
    }
    PodZero(data_ + length_, length);
    length_ += length;
  }

  /** Prepend `length` zero-ed elements to the end of the array, resizing the
   * array if needed.
   * @parameter length the number of elements to prepend to the array.
   */
  void push_front_silence(size_t length)
  {
    if (length_ + length > capacity_) {
      reserve(length + length_);
    }
    PodMove(data_ + length, data_, length_);
    PodZero(data_, length);
    length_ += length;
  }

  /** Return the number of free elements in the array. */
  size_t available() const
  {
    return capacity_ - length_;
  }

  /** Copies `length` elements to `elements` if it is not null, and shift
    * the remaining elements of the `auto_array` to the beginning.
    * @parameter elements a buffer to copy the elements to, or nullptr.
    * @parameter length the number of elements to copy.
    * @returns true in case of success.
    * @returns false if the auto_array contains less than `length` elements. */
  bool pop(T * elements, size_t length)
  {
    if (length > length_) {
      return false;
    }
    if (elements) {
      PodCopy(elements, data_, length);
    }
    PodMove(data_, data_ + length, length_ - length);

    length_ -= length;

    return true;
  }

  void set_length(size_t length)
  {
    assert(length <= capacity_);
    length_ = length;
  }

private:
  /** The underlying storage */
  T * data_;
  /** The size, in number of elements, of the storage. */
  size_t capacity_;
  /** The number of elements the array contains. */
  size_t length_;
};

struct auto_lock {
  explicit auto_lock(owned_critical_section & lock)
    : lock(lock)
  {
    lock.enter();
  }
  ~auto_lock()
  {
    lock.leave();
  }
private:
  owned_critical_section & lock;
};

#endif /* CUBEB_UTILS */
