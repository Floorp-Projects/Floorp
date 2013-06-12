/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/neteq4/audio_vector.h"

#include <assert.h>

#include <algorithm>

#include "webrtc/typedefs.h"

namespace webrtc {

template<typename T>
void AudioVector<T>::Clear() {
  vector_.clear();
}

template<typename T>
void AudioVector<T>::CopyFrom(AudioVector<T>* copy_to) const {
  if (copy_to) {
    copy_to->vector_.assign(vector_.begin(), vector_.end());
  }
}

template<typename T>
void AudioVector<T>::PushFront(const AudioVector<T>& prepend_this) {
  vector_.insert(vector_.begin(), prepend_this.vector_.begin(),
                 prepend_this.vector_.end());
}

template<typename T>
void AudioVector<T>::PushFront(const T* prepend_this, size_t length) {
  // Same operation as InsertAt beginning.
  InsertAt(prepend_this, length, 0);
}

template<typename T>
void AudioVector<T>::PushBack(const AudioVector<T>& append_this) {
  vector_.reserve(vector_.size() + append_this.Size());
  for (size_t i = 0; i < append_this.Size(); ++i) {
    vector_.push_back(append_this[i]);
  }
}

template<typename T>
void AudioVector<T>::PushBack(const T* append_this, size_t length) {
  vector_.reserve(vector_.size() + length);
  for (size_t i = 0; i < length; ++i) {
    vector_.push_back(append_this[i]);
  }
}

template<typename T>
void AudioVector<T>::PopFront(size_t length) {
  if (length >= vector_.size()) {
    // Remove all elements.
    vector_.clear();
  } else {
    typename std::vector<T>::iterator end_range = vector_.begin();
    end_range += length;
    // Erase all elements in range vector_.begin() and |end_range| (not
    // including |end_range|).
    vector_.erase(vector_.begin(), end_range);
  }
}

template<typename T>
void AudioVector<T>::PopBack(size_t length) {
  // Make sure that new_size is never negative (which causes wrap-around).
  size_t new_size = vector_.size() - std::min(length, vector_.size());
  vector_.resize(new_size);
}

template<typename T>
void AudioVector<T>::Extend(size_t extra_length) {
  vector_.insert(vector_.end(), extra_length, 0);
}

template<typename T>
void AudioVector<T>::InsertAt(const T* insert_this,
                              size_t length,
                              size_t position) {
  typename std::vector<T>::iterator insert_position = vector_.begin();
  // Cap the position at the current vector length, to be sure the iterator
  // does not extend beyond the end of the vector.
  position = std::min(vector_.size(), position);
  insert_position += position;
  // First, insert zeros at the position. This makes the vector longer (and
  // invalidates the iterator |insert_position|.
  vector_.insert(insert_position, length, 0);
  // Write the new values into the vector.
  for (size_t i = 0; i < length; ++i) {
    vector_[position + i] = insert_this[i];
  }
}

template<typename T>
void AudioVector<T>::InsertZerosAt(size_t length,
                                   size_t position) {
  typename std::vector<T>::iterator insert_position = vector_.begin();
  // Cap the position at the current vector length, to be sure the iterator
  // does not extend beyond the end of the vector.
  position = std::min(vector_.size(), position);
  insert_position += position;
  // Insert zeros at the position. This makes the vector longer (and
  // invalidates the iterator |insert_position|.
  vector_.insert(insert_position, length, 0);
}

template<typename T>
void AudioVector<T>::OverwriteAt(const T* insert_this,
                                 size_t length,
                                 size_t position) {
  // Cap the insert position at the current vector length.
  position = std::min(vector_.size(), position);
  // Extend the vector if needed. (It is valid to overwrite beyond the current
  // end of the vector.)
  if (position + length > vector_.size()) {
    Extend(position + length - vector_.size());
  }
  for (size_t i = 0; i < length; ++i) {
    vector_[position + i] = insert_this[i];
  }
}

template<typename T>
void AudioVector<T>::CrossFade(const AudioVector<T>& append_this,
                               size_t fade_length) {
  // Fade length cannot be longer than the current vector or |append_this|.
  assert(fade_length <= Size());
  assert(fade_length <= append_this.Size());
  fade_length = std::min(fade_length, Size());
  fade_length = std::min(fade_length, append_this.Size());
  size_t position = Size() - fade_length;
  // Cross fade the overlapping regions.
  // |alpha| is the mixing factor in Q14.
  // TODO(hlundin): Consider skipping +1 in the denominator to produce a
  // smoother cross-fade, in particular at the end of the fade.
  int alpha_step = 16384 / (fade_length + 1);
  int alpha = 16384;
  for (size_t i = 0; i < fade_length; ++i) {
    alpha -= alpha_step;
    vector_[position + i] = (alpha * vector_[position + i] +
        (16384 - alpha) * append_this[i] + 8192) >> 14;
  }
  assert(alpha >= 0);  // Verify that the slope was correct.
  // Append what is left of |append_this|.
  size_t samples_to_push_back = append_this.Size() - fade_length;
  if (samples_to_push_back > 0)
    PushBack(&append_this[fade_length], samples_to_push_back);
}

// Template specialization for double. The only difference is in the calculation
// of the cross-faded value, where we divide by 16384 instead of shifting with
// 14 steps, and also not adding 8192 before scaling.
template<>
void AudioVector<double>::CrossFade(const AudioVector<double>& append_this,
                                    size_t fade_length) {
  // Fade length cannot be longer than the current vector or |append_this|.
  assert(fade_length <= Size());
  assert(fade_length <= append_this.Size());
  fade_length = std::min(fade_length, Size());
  fade_length = std::min(fade_length, append_this.Size());
  size_t position = Size() - fade_length;
  // Cross fade the overlapping regions.
  // |alpha| is the mixing factor in Q14.
  // TODO(hlundin): Consider skipping +1 in the denominator to produce a
  // smoother cross-fade, in particular at the end of the fade.
  int alpha_step = 16384 / (fade_length + 1);
  int alpha = 16384;
  for (size_t i = 0; i < fade_length; ++i) {
    alpha -= alpha_step;
    vector_[position + i] = (alpha * vector_[position + i] +
        (16384 - alpha) * append_this[i]) / 16384;
  }
  assert(alpha >= 0);  // Verify that the slope was correct.
  // Append what is left of |append_this|.
  size_t samples_to_push_back = append_this.Size() - fade_length;
  if (samples_to_push_back > 0)
    PushBack(&append_this[fade_length], samples_to_push_back);
}

template<typename T>
const T& AudioVector<T>::operator[](size_t index) const {
  return vector_[index];
}

template<typename T>
T& AudioVector<T>::operator[](size_t index) {
  return vector_[index];
}

// Instantiate the template for a few types.
template class AudioVector<int16_t>;
template class AudioVector<int32_t>;
template class AudioVector<double>;

}  // namespace webrtc
