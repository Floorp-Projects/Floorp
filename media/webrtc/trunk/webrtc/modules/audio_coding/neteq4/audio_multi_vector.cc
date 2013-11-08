/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/neteq4/audio_multi_vector.h"

#include <assert.h>

#include <algorithm>

#include "webrtc/typedefs.h"

namespace webrtc {

template<typename T>
AudioMultiVector<T>::AudioMultiVector(size_t N) {
  assert(N > 0);
  if (N < 1) N = 1;
  for (size_t n = 0; n < N; ++n) {
    channels_.push_back(new AudioVector<T>);
  }
}

template<typename T>
AudioMultiVector<T>::AudioMultiVector(size_t N, size_t initial_size) {
  assert(N > 0);
  if (N < 1) N = 1;
  for (size_t n = 0; n < N; ++n) {
    channels_.push_back(new AudioVector<T>(initial_size));
  }
}

template<typename T>
AudioMultiVector<T>::~AudioMultiVector() {
  typename std::vector<AudioVector<T>*>::iterator it = channels_.begin();
  while (it != channels_.end()) {
    delete (*it);
    ++it;
  }
}

template<typename T>
void AudioMultiVector<T>::Clear() {
  for (size_t i = 0; i < Channels(); ++i) {
    channels_[i]->Clear();
  }
}

template<typename T>
void AudioMultiVector<T>::Zeros(size_t length) {
  for (size_t i = 0; i < Channels(); ++i) {
    channels_[i]->Clear();
    channels_[i]->Extend(length);
  }
}

template<typename T>
void AudioMultiVector<T>::CopyFrom(AudioMultiVector<T>* copy_to) const {
  if (copy_to) {
    for (size_t i = 0; i < Channels(); ++i) {
      channels_[i]->CopyFrom(&(*copy_to)[i]);
    }
  }
}

template<typename T>
void AudioMultiVector<T>::PushBackInterleaved(const T* append_this,
                                              size_t length) {
  assert(length % Channels() == 0);
  if (Channels() == 1) {
    // Special case to avoid extra allocation and data shuffling.
    channels_[0]->PushBack(append_this, length);
    return;
  }
  size_t length_per_channel = length / Channels();
  T* temp_array = new T[length_per_channel];  // Intermediate storage.
  for (size_t channel = 0; channel < Channels(); ++channel) {
    // Copy elements to |temp_array|.
    // Set |source_ptr| to first element of this channel.
    const T* source_ptr = &append_this[channel];
    for (size_t i = 0; i < length_per_channel; ++i) {
      temp_array[i] = *source_ptr;
      source_ptr += Channels();  // Jump to next element of this channel.
    }
    channels_[channel]->PushBack(temp_array, length_per_channel);
  }
  delete [] temp_array;
}

template<typename T>
void AudioMultiVector<T>::PushBack(const AudioMultiVector<T>& append_this) {
  assert(Channels() == append_this.Channels());
  if (Channels() == append_this.Channels()) {
    for (size_t i = 0; i < Channels(); ++i) {
      channels_[i]->PushBack(append_this[i]);
    }
  }
}

template<typename T>
void AudioMultiVector<T>::PushBackFromIndex(
    const AudioMultiVector<T>& append_this,
    size_t index) {
  assert(index < append_this.Size());
  index = std::min(index, append_this.Size() - 1);
  size_t length = append_this.Size() - index;
  assert(Channels() == append_this.Channels());
  if (Channels() == append_this.Channels()) {
    for (size_t i = 0; i < Channels(); ++i) {
      channels_[i]->PushBack(&append_this[i][index], length);
    }
  }
}

template<typename T>
void AudioMultiVector<T>::PopFront(size_t length) {
  for (size_t i = 0; i < Channels(); ++i) {
    channels_[i]->PopFront(length);
  }
}

template<typename T>
void AudioMultiVector<T>::PopBack(size_t length) {
  for (size_t i = 0; i < Channels(); ++i) {
    channels_[i]->PopBack(length);
  }
}

template<typename T>
size_t AudioMultiVector<T>::ReadInterleaved(size_t length,
                                            T* destination) const {
  return ReadInterleavedFromIndex(0, length, destination);
}

template<typename T>
size_t AudioMultiVector<T>::ReadInterleavedFromIndex(size_t start_index,
                                                     size_t length,
                                                     T* destination) const {
  if (!destination) {
    return 0;
  }
  size_t index = 0;  // Number of elements written to |destination| so far.
  assert(start_index <= Size());
  start_index = std::min(start_index, Size());
  if (length + start_index > Size()) {
    length = Size() - start_index;
  }
  for (size_t i = 0; i < length; ++i) {
    for (size_t channel = 0; channel < Channels(); ++channel) {
      destination[index] = (*this)[channel][i + start_index];
      ++index;
    }
  }
  return index;
}

template<typename T>
size_t AudioMultiVector<T>::ReadInterleavedFromEnd(size_t length,
                                                   T* destination) const {
  length = std::min(length, Size());  // Cannot read more than Size() elements.
  return ReadInterleavedFromIndex(Size() - length, length, destination);
}

template<typename T>
void AudioMultiVector<T>::OverwriteAt(const AudioMultiVector<T>& insert_this,
                                      size_t length,
                                      size_t position) {
  assert(Channels() == insert_this.Channels());
  // Cap |length| at the length of |insert_this|.
  assert(length <= insert_this.Size());
  length = std::min(length, insert_this.Size());
  if (Channels() == insert_this.Channels()) {
    for (size_t i = 0; i < Channels(); ++i) {
      channels_[i]->OverwriteAt(&insert_this[i][0], length, position);
    }
  }
}

template<typename T>
void AudioMultiVector<T>::CrossFade(const AudioMultiVector<T>& append_this,
                                    size_t fade_length) {
  assert(Channels() == append_this.Channels());
  if (Channels() == append_this.Channels()) {
    for (size_t i = 0; i < Channels(); ++i) {
      channels_[i]->CrossFade(append_this[i], fade_length);
    }
  }
}

template<typename T>
size_t AudioMultiVector<T>::Size() const {
  assert(channels_[0]);
  return channels_[0]->Size();
}

template<typename T>
void AudioMultiVector<T>::AssertSize(size_t required_size) {
  if (Size() < required_size) {
    size_t extend_length = required_size - Size();
    for (size_t channel = 0; channel < Channels(); ++channel) {
      channels_[channel]->Extend(extend_length);
    }
  }
}

template<typename T>
bool AudioMultiVector<T>::Empty() const {
  assert(channels_[0]);
  return channels_[0]->Empty();
}

template<typename T>
const AudioVector<T>& AudioMultiVector<T>::operator[](size_t index) const {
  return *(channels_[index]);
}

template<typename T>
AudioVector<T>& AudioMultiVector<T>::operator[](size_t index) {
  return *(channels_[index]);
}

// Instantiate the template for a few types.
template class AudioMultiVector<int16_t>;
template class AudioMultiVector<int32_t>;
template class AudioMultiVector<double>;

}  // namespace webrtc
