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

/* === NOTE === NOTE === NOTE === NOTE === NOTE === NOTE === NOTE === NOTE ===
 *
 * THIS IS A COPY OF libcore/include/UniquePtr.h AND AS SUCH THAT IS THE
 * CANONICAL SOURCE OF THIS FILE. PLEASE KEEP THEM IN SYNC.
 *
 * === NOTE === NOTE === NOTE === NOTE === NOTE === NOTE === NOTE === NOTE ===
 */

#ifndef UNIQUE_PTR_H_included
#define UNIQUE_PTR_H_included

#include <cstdlib> // For NULL.

// Default deleter for pointer types.
template <typename T>
struct DefaultDelete {
    enum { type_must_be_complete = sizeof(T) };
    DefaultDelete() {}
    void operator()(T* p) const {
        delete p;
    }
};

// Default deleter for array types.
template <typename T>
struct DefaultDelete<T[]> {
    enum { type_must_be_complete = sizeof(T) };
    void operator()(T* p) const {
        delete[] p;
    }
};

// A smart pointer that deletes the given pointer on destruction.
// Equivalent to C++0x's std::unique_ptr (a combination of boost::scoped_ptr
// and boost::scoped_array).
// Named to be in keeping with Android style but also to avoid
// collision with any other implementation, until we can switch over
// to unique_ptr.
// Use thus:
//   UniquePtr<C> c(new C);
template <typename T, typename D = DefaultDelete<T> >
class UniquePtr {
public:
    // Construct a new UniquePtr, taking ownership of the given raw pointer.
    explicit UniquePtr(T* ptr = NULL) : mPtr(ptr) {
    }

    ~UniquePtr() {
        reset();
    }

    // Accessors.
    T& operator*() const { return *mPtr; }
    T* operator->() const { return mPtr; }
    T* get() const { return mPtr; }

    // Returns the raw pointer and hands over ownership to the caller.
    // The pointer will not be deleted by UniquePtr.
    T* release() __attribute__((warn_unused_result)) {
        T* result = mPtr;
        mPtr = NULL;
        return result;
    }

    // Takes ownership of the given raw pointer.
    // If this smart pointer previously owned a different raw pointer, that
    // raw pointer will be freed.
    void reset(T* ptr = NULL) {
        if (ptr != mPtr) {
            D()(mPtr);
            mPtr = ptr;
        }
    }

private:
    // The raw pointer.
    T* mPtr;

    // Comparing unique pointers is probably a mistake, since they're unique.
    template <typename T2> bool operator==(const UniquePtr<T2>& p) const;
    template <typename T2> bool operator!=(const UniquePtr<T2>& p) const;

    // Disallow copy and assignment.
    UniquePtr(const UniquePtr&);
    void operator=(const UniquePtr&);
};

// Partial specialization for array types. Like std::unique_ptr, this removes
// operator* and operator-> but adds operator[].
template <typename T, typename D>
class UniquePtr<T[], D> {
public:
    explicit UniquePtr(T* ptr = NULL) : mPtr(ptr) {
    }

    ~UniquePtr() {
        reset();
    }

    T& operator[](size_t i) const {
        return mPtr[i];
    }
    T* get() const { return mPtr; }

    T* release() __attribute__((warn_unused_result)) {
        T* result = mPtr;
        mPtr = NULL;
        return result;
    }

    void reset(T* ptr = NULL) {
        if (ptr != mPtr) {
            D()(mPtr);
            mPtr = ptr;
        }
    }

private:
    T* mPtr;

    // Disallow copy and assignment.
    UniquePtr(const UniquePtr&);
    void operator=(const UniquePtr&);
};

#if UNIQUE_PTR_TESTS

// Run these tests with:
// g++ -g -DUNIQUE_PTR_TESTS -x c++ UniquePtr.h && ./a.out

#include <stdio.h>

static void assert(bool b) {
    if (!b) {
        fprintf(stderr, "FAIL\n");
        abort();
    }
    fprintf(stderr, "OK\n");
}
static int cCount = 0;
struct C {
    C() { ++cCount; }
    ~C() { --cCount; }
};
static bool freed = false;
struct Freer {
    void operator()(int* p) {
        assert(*p == 123);
        free(p);
        freed = true;
    }
};

int main(int argc, char* argv[]) {
    //
    // UniquePtr<T> tests...
    //

    // Can we free a single object?
    {
        UniquePtr<C> c(new C);
        assert(cCount == 1);
    }
    assert(cCount == 0);
    // Does release work?
    C* rawC;
    {
        UniquePtr<C> c(new C);
        assert(cCount == 1);
        rawC = c.release();
    }
    assert(cCount == 1);
    delete rawC;
    // Does reset work?
    {
        UniquePtr<C> c(new C);
        assert(cCount == 1);
        c.reset(new C);
        assert(cCount == 1);
    }
    assert(cCount == 0);

    //
    // UniquePtr<T[]> tests...
    //

    // Can we free an array?
    {
        UniquePtr<C[]> cs(new C[4]);
        assert(cCount == 4);
    }
    assert(cCount == 0);
    // Does release work?
    {
        UniquePtr<C[]> c(new C[4]);
        assert(cCount == 4);
        rawC = c.release();
    }
    assert(cCount == 4);
    delete[] rawC;
    // Does reset work?
    {
        UniquePtr<C[]> c(new C[4]);
        assert(cCount == 4);
        c.reset(new C[2]);
        assert(cCount == 2);
    }
    assert(cCount == 0);

    //
    // Custom deleter tests...
    //
    assert(!freed);
    {
        UniquePtr<int, Freer> i(reinterpret_cast<int*>(malloc(sizeof(int))));
        *i = 123;
    }
    assert(freed);
    return 0;
}
#endif

#endif  // UNIQUE_PTR_H_included
