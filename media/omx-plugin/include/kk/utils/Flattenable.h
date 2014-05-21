/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_UTILS_FLATTENABLE_H
#define ANDROID_UTILS_FLATTENABLE_H


#include <stdint.h>
#include <sys/types.h>
#include <utils/Errors.h>

namespace android {

/*
 * The Flattenable interface allows an object to serialize itself out
 * to a byte-buffer and an array of file descriptors.
 */

class Flattenable
{
public:
    // size in bytes of the flattened object
    virtual size_t getFlattenedSize() const = 0;

    // number of file descriptors to flatten
    virtual size_t getFdCount() const = 0;

    // flattens the object into buffer.
    // size should be at least of getFlattenedSize()
    // file descriptors are written in the fds[] array but ownership is
    // not transfered (ie: they must be dupped by the caller of
    // flatten() if needed).
    virtual status_t flatten(void* buffer, size_t size,
            int fds[], size_t count) const = 0;

    // unflattens the object from buffer.
    // size should be equal to the value of getFlattenedSize() when the
    // object was flattened.
    // unflattened file descriptors are found in the fds[] array and
    // don't need to be dupped(). ie: the caller of unflatten doesn't
    // keep ownership. If a fd is not retained by unflatten() it must be
    // explicitly closed.
    virtual status_t unflatten(void const* buffer, size_t size,
            int fds[], size_t count) = 0;

protected:
    virtual ~Flattenable() = 0;

};

/*
 * LightFlattenable is a protocol allowing object to serialize themselves out
 * to a byte-buffer.
 *
 * LightFlattenable objects must implement this protocol.
 *
 * LightFlattenable doesn't require the object to be virtual.
 */
template <typename T>
class LightFlattenable {
public:
    // returns whether this object always flatten into the same size.
    // for efficiency, this should always be inline.
    inline bool isFixedSize() const;

    // returns size in bytes of the flattened object. must be a constant.
    inline size_t getSize() const;

    // flattens the object into buffer.
    inline status_t flatten(void* buffer) const;

    // unflattens the object from buffer of given size.
    inline status_t unflatten(void const* buffer, size_t size);
};

template <typename T>
inline bool LightFlattenable<T>::isFixedSize() const {
    return static_cast<T const*>(this)->T::isFixedSize();
}
template <typename T>
inline size_t LightFlattenable<T>::getSize() const {
    return static_cast<T const*>(this)->T::getSize();
}
template <typename T>
inline status_t LightFlattenable<T>::flatten(void* buffer) const {
    return static_cast<T const*>(this)->T::flatten(buffer);
}
template <typename T>
inline status_t LightFlattenable<T>::unflatten(void const* buffer, size_t size) {
    return static_cast<T*>(this)->T::unflatten(buffer, size);
}

/*
 * LightFlattenablePod is an implementation of the LightFlattenable protocol
 * for POD (plain-old-data) objects.
 */
template <typename T>
class LightFlattenablePod : public LightFlattenable<T> {
public:
    inline bool isFixedSize() const {
        return true;
    }

    inline size_t getSize() const {
        return sizeof(T);
    }
    inline status_t flatten(void* buffer) const {
        *reinterpret_cast<T*>(buffer) = *static_cast<T const*>(this);
        return NO_ERROR;
    }
    inline status_t unflatten(void const* buffer, size_t) {
        *static_cast<T*>(this) = *reinterpret_cast<T const*>(buffer);
        return NO_ERROR;
    }
};


}; // namespace android


#endif /* ANDROID_UTILS_FLATTENABLE_H */
