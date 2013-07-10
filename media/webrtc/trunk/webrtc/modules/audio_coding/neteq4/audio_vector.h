/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_NETEQ4_AUDIO_VECTOR_H_
#define WEBRTC_MODULES_AUDIO_CODING_NETEQ4_AUDIO_VECTOR_H_

#include <cstring>  // Access to size_t.
#include <vector>

#include "webrtc/system_wrappers/interface/constructor_magic.h"

namespace webrtc {

template <typename T>
class AudioVector {
 public:
  // Creates an empty AudioVector.
  AudioVector() {}

  // Creates an AudioVector with an initial size.
  explicit AudioVector(size_t initial_size)
      : vector_(initial_size, 0) {}

  virtual ~AudioVector() {}

  // Deletes all values and make the vector empty.
  virtual void Clear();

  // Copies all values from this vector to |copy_to|. Any contents in |copy_to|
  // are deleted before the copy operation. After the operation is done,
  // |copy_to| will be an exact replica of this object.
  virtual void CopyFrom(AudioVector<T>* copy_to) const;

  // Prepends the contents of AudioVector |prepend_this| to this object. The
  // length of this object is increased with the length of |prepend_this|.
  virtual void PushFront(const AudioVector<T>& prepend_this);

  // Same as above, but with an array |prepend_this| with |length| elements as
  // source.
  virtual void PushFront(const T* prepend_this, size_t length);

  // Same as PushFront but will append to the end of this object.
  virtual void PushBack(const AudioVector<T>& append_this);

  // Same as PushFront but will append to the end of this object.
  virtual void PushBack(const T* append_this, size_t length);

  // Removes |length| elements from the beginning of this object.
  virtual void PopFront(size_t length);

  // Removes |length| elements from the end of this object.
  virtual void PopBack(size_t length);

  // Extends this object with |extra_length| elements at the end. The new
  // elements are initialized to zero.
  virtual void Extend(size_t extra_length);

  // Inserts |length| elements taken from the array |insert_this| and insert
  // them at |position|. The length of the AudioVector is increased by |length|.
  // |position| = 0 means that the new values are prepended to the vector.
  // |position| = Size() means that the new values are appended to the vector.
  virtual void InsertAt(const T* insert_this, size_t length, size_t position);

  // Like InsertAt, but inserts |length| zero elements at |position|.
  virtual void InsertZerosAt(size_t length, size_t position);

  // Overwrites |length| elements of this AudioVector with values taken from the
  // array |insert_this|, starting at |position|. The definition of |position|
  // is the same as for InsertAt(). If |length| and |position| are selected
  // such that the new data extends beyond the end of the current AudioVector,
  // the vector is extended to accommodate the new data.
  virtual void OverwriteAt(const T* insert_this,
                           size_t length,
                           size_t position);

  // Appends |append_this| to the end of the current vector. Lets the two
  // vectors overlap by |fade_length| samples, and cross-fade linearly in this
  // region.
  virtual void CrossFade(const AudioVector<T>& append_this, size_t fade_length);

  // Returns the number of elements in this AudioVector.
  virtual size_t Size() const { return vector_.size(); }

  // Returns true if this AudioVector is empty.
  virtual bool Empty() const { return vector_.empty(); }

  // Accesses and modifies an element of AudioVector.
  const T& operator[](size_t index) const;
  T& operator[](size_t index);

 private:
  std::vector<T> vector_;

  DISALLOW_COPY_AND_ASSIGN(AudioVector);
};

}  // namespace webrtc
#endif  // WEBRTC_MODULES_AUDIO_CODING_NETEQ4_AUDIO_VECTOR_H_
