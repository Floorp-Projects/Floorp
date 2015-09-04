/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define MOZ_MEMORY_IMPL
#include "mozmemory_wrap.h"

#ifdef _WIN32
#include <windows.h>
#include <io.h>
typedef int ssize_t;
#else
#include <sys/mman.h>
#include <unistd.h>
#endif
#include <algorithm>
#include <cstdio>
#include <cstring>

#include "mozilla/Assertions.h"
#include "FdPrintf.h"

static void
die(const char* message)
{
  /* Here, it doesn't matter that fprintf may allocate memory. */
  fprintf(stderr, "%s\n", message);
  exit(1);
}

/* We don't want to be using malloc() to allocate our internal tracking
 * data, because that would change the parameters of what is being measured,
 * so we want to use data types that directly use mmap/VirtualAlloc. */
template <typename T, size_t Len>
class MappedArray
{
public:
  MappedArray(): mPtr(nullptr) {}

  ~MappedArray()
  {
    if (mPtr) {
#ifdef _WIN32
      VirtualFree(mPtr, sizeof(T) * Len, MEM_RELEASE);
#else
      munmap(mPtr, sizeof(T) * Len);
#endif
    }
  }

  T& operator[] (size_t aIndex) const
  {
    if (mPtr) {
      return mPtr[aIndex];
    }

#ifdef _WIN32
    mPtr = reinterpret_cast<T*>(VirtualAlloc(nullptr, sizeof(T) * Len,
             MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (mPtr == nullptr) {
      die("VirtualAlloc error");
    }
#else
    mPtr = reinterpret_cast<T*>(mmap(nullptr, sizeof(T) * Len,
             PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0));
    if (mPtr == MAP_FAILED) {
      die("Mmap error");
    }
#endif
    return mPtr[aIndex];
  }

private:
  mutable T* mPtr;
};

/* Type for records of allocations. */
struct MemSlot
{
  void* mPtr;
  size_t mSize;
};

/* An almost infinite list of slots.
 * In essence, this is a linked list of arrays of groups of slots.
 * Each group is 1MB. On 64-bits, one group allows to store 64k allocations.
 * Each MemSlotList instance can store 1023 such groups, which means more
 * than 65M allocations. In case more would be needed, we chain to another
 * MemSlotList, and so on.
 * Using 1023 groups makes the MemSlotList itself page sized on 32-bits
 * and 2 pages-sized on 64-bits.
 */
class MemSlotList
{
  static const size_t kGroups = 1024 - 1;
  static const size_t kGroupSize = (1024 * 1024) / sizeof(MemSlot);

  MappedArray<MemSlot, kGroupSize> mSlots[kGroups];
  MappedArray<MemSlotList, 1> mNext;

public:
  MemSlot& operator[] (size_t aIndex) const
  {
    if (aIndex < kGroupSize * kGroups) {
      return mSlots[aIndex / kGroupSize][aIndex % kGroupSize];
    }
    aIndex -= kGroupSize * kGroups;
    return mNext[0][aIndex];
  }
};

/* Helper class for memory buffers */
class Buffer
{
public:
  Buffer() : mBuf(nullptr), mLength(0) {}

  Buffer(const void* aBuf, size_t aLength)
    : mBuf(reinterpret_cast<const char*>(aBuf)), mLength(aLength)
  {}

  /* Constructor for string literals. */
  template <size_t Size>
  explicit Buffer(const char (&aStr)[Size])
    : mBuf(aStr), mLength(Size - 1)
  {}

  /* Returns a sub-buffer up-to but not including the given aNeedle character.
   * The "parent" buffer itself is altered to begin after the aNeedle
   * character.
   * If the aNeedle character is not found, return the entire buffer, and empty
   * the "parent" buffer. */
  Buffer SplitChar(char aNeedle)
  {
    char* buf = const_cast<char*>(mBuf);
    char* c = reinterpret_cast<char*>(memchr(buf, aNeedle, mLength));
    if (!c) {
      return Split(mLength);
    }

    Buffer result = Split(c - buf);
    // Remove the aNeedle character itself.
    Split(1);
    return result;
  }

  /* Returns a sub-buffer of at most aLength characters. The "parent" buffer is
   * amputated of those aLength characters. If the "parent" buffer is smaller
   * than aLength, then its length is used instead. */
  Buffer Split(size_t aLength)
  {
    Buffer result(mBuf, std::min(aLength, mLength));
    mLength -= result.mLength;
    mBuf += result.mLength;
    return result;
  }

  /* Move the buffer (including its content) to the memory address of the aOther
   * buffer. */
  void Slide(Buffer aOther)
  {
    memmove(const_cast<char*>(aOther.mBuf), mBuf, mLength);
    mBuf = aOther.mBuf;
  }

  /* Returns whether the two involved buffers have the same content. */
  bool operator ==(Buffer aOther)
  {
    return mLength == aOther.mLength && (mBuf == aOther.mBuf ||
                                         !strncmp(mBuf, aOther.mBuf, mLength));
  }

  /* Returns whether the buffer is empty. */
  explicit operator bool() { return mLength; }

  /* Returns the memory location of the buffer. */
  const char* get() { return mBuf; }

  /* Returns the memory location of the end of the buffer (technically, the
   * first byte after the buffer). */
  const char* GetEnd() { return mBuf + mLength; }

  /* Extend the buffer over the content of the other buffer, assuming it is
   * adjacent. */
  void Extend(Buffer aOther)
  {
    MOZ_ASSERT(aOther.mBuf == GetEnd());
    mLength += aOther.mLength;
  }

private:
  const char* mBuf;
  size_t mLength;
};

/* Helper class to read from a file descriptor line by line. */
class FdReader {
public:
  explicit FdReader(int aFd)
    : mFd(aFd)
    , mData(&mRawBuf, 0)
    , mBuf(&mRawBuf, sizeof(mRawBuf))
  {}

  /* Read a line from the file descriptor and returns it as a Buffer instance */
  Buffer ReadLine()
  {
    while (true) {
      Buffer result = mData.SplitChar('\n');

      /* There are essentially three different cases here:
       * - '\n' was found "early". In this case, the end of the result buffer
       *   is before the beginning of the mData buffer (since SplitChar
       *   amputated it).
       * - '\n' was found as the last character of mData. In this case, mData
       *   is empty, but still points at the end of mBuf. result points to what
       *   used to be in mData, without the last character.
       * - '\n' was not found. In this case too, mData is empty and points at
       *   the end of mBuf. But result points to the entire buffer that used to
       *   be pointed by mData.
       * Only in the latter case do both result and mData's end match, and it's
       * the only case where we need to refill the buffer.
       */
      if (result.GetEnd() != mData.GetEnd()) {
        return result;
      }

      /* Since SplitChar emptied mData, make it point to what it had before. */
      mData = result;

      /* And move it to the beginning of the read buffer. */
      mData.Slide(mBuf);

      FillBuffer();

      if (!mData) {
        return Buffer();
      }
    }
  }

private:
  /* Fill the read buffer. */
  void FillBuffer()
  {
    size_t size = mBuf.GetEnd() - mData.GetEnd();
    Buffer remainder(mData.GetEnd(), size);

    ssize_t len = 1;
    while (remainder && len > 0) {
      len = ::read(mFd, const_cast<char*>(remainder.get()), size);
      if (len < 0) {
        die("Read error");
      }
      size -= len;
      mData.Extend(remainder.Split(len));
    }
  }

  /* File descriptor to read from. */
  int mFd;
  /* Part of data that was read from the file descriptor but not returned with
   * ReadLine yet. */
  Buffer mData;
  /* Buffer representation of mRawBuf */
  Buffer mBuf;
  /* read() buffer */
  char mRawBuf[4096];
};

MOZ_BEGIN_EXTERN_C

/* Function declarations for all the replace_malloc _impl functions.
 * See memory/build/replace_malloc.c */
#define MALLOC_DECL(name, return_type, ...) \
  return_type name ## _impl(__VA_ARGS__);
#define MALLOC_FUNCS MALLOC_FUNCS_MALLOC
#include "malloc_decls.h"

#define MALLOC_DECL(name, return_type, ...) \
  return_type name ## _impl(__VA_ARGS__);
#define MALLOC_FUNCS MALLOC_FUNCS_JEMALLOC
#include "malloc_decls.h"

/* mozjemalloc relies on DllMain to initialize, but DllMain is not invoked
 * for executables, so manually invoke mozjemalloc initialization. */
#if defined(_WIN32) && !defined(MOZ_JEMALLOC4)
void malloc_init_hard(void);
#endif

#ifdef ANDROID
/* mozjemalloc uses MozTagAnonymousMemory, which doesn't have an inline
 * implementation on Android */
void
MozTagAnonymousMemory(const void* aPtr, size_t aLength, const char* aTag) {}

/* mozjemalloc and jemalloc use pthread_atfork, which Android doesn't have.
 * While gecko has one in libmozglue, the replay program can't use that.
 * Since we're not going to fork anyways, make it a dummy function. */
int
pthread_atfork(void (*aPrepare)(void), void (*aParent)(void),
               void (*aChild)(void))
{
  return 0;
}
#endif

#ifdef MOZ_NUWA_PROCESS
#include <pthread.h>

/* NUWA builds have jemalloc built with
 * -Dpthread_mutex_lock=__real_pthread_mutex_lock */
int
__real_pthread_mutex_lock(pthread_mutex_t* aMutex)
{
  return pthread_mutex_lock(aMutex);
}
#endif

MOZ_END_EXTERN_C

size_t parseNumber(Buffer aBuf)
{
  if (!aBuf) {
    die("Malformed input");
  }

  size_t result = 0;
  for (const char* c = aBuf.get(), *end = aBuf.GetEnd(); c < end; c++) {
    if (*c < '0' || *c > '9') {
      die("Malformed input");
    }
    result *= 10;
    result += *c - '0';
  }
  return result;
}

/* Class to handle dispatching the replay function calls to replace-malloc. */
class Replay
{
public:
  Replay(): mOps(0) {
#ifdef _WIN32
    // See comment in FdPrintf.h as to why native win32 handles are used.
    mStdErr = reinterpret_cast<intptr_t>(GetStdHandle(STD_ERROR_HANDLE));
#else
    mStdErr = fileno(stderr);
#endif
  }

  MemSlot& operator[] (size_t index) const
  {
    return mSlots[index];
  }

  void malloc(MemSlot& aSlot, Buffer& aArgs)
  {
    mOps++;
    size_t size = parseNumber(aArgs);
    aSlot.mPtr = ::malloc_impl(size);
    aSlot.mSize = size;
    Commit(aSlot);
  }

  void posix_memalign(MemSlot& aSlot, Buffer& aArgs)
  {
    mOps++;
    size_t alignment = parseNumber(aArgs.SplitChar(','));
    size_t size = parseNumber(aArgs);
    void* ptr;
    if (::posix_memalign_impl(&ptr, alignment, size) == 0) {
      aSlot.mPtr = ptr;
      aSlot.mSize = size;
    } else {
      aSlot.mPtr = nullptr;
      aSlot.mSize = 0;
    }
    Commit(aSlot);
  }

  void aligned_alloc(MemSlot& aSlot, Buffer& aArgs)
  {
    mOps++;
    size_t alignment = parseNumber(aArgs.SplitChar(','));
    size_t size = parseNumber(aArgs);
    aSlot.mPtr = ::aligned_alloc_impl(alignment, size);
    aSlot.mSize = size;
    Commit(aSlot);
  }

  void calloc(MemSlot& aSlot, Buffer& aArgs)
  {
    mOps++;
    size_t num = parseNumber(aArgs.SplitChar(','));
    size_t size = parseNumber(aArgs);
    aSlot.mPtr = ::calloc_impl(num, size);
    aSlot.mSize = size * num;
    Commit(aSlot);
  }

  void realloc(MemSlot& aSlot, Buffer& aArgs)
  {
    mOps++;
    Buffer dummy = aArgs.SplitChar('#');
    if (dummy) {
      die("Malformed input");
    }
    size_t slot_id = parseNumber(aArgs.SplitChar(','));
    size_t size = parseNumber(aArgs);
    MemSlot& old_slot = (*this)[slot_id];
    void* old_ptr = old_slot.mPtr;
    old_slot.mPtr = nullptr;
    old_slot.mSize = 0;
    aSlot.mPtr = ::realloc_impl(old_ptr, size);
    aSlot.mSize = size;
    Commit(aSlot);
  }

  void free(Buffer& aArgs)
  {
    mOps++;
    Buffer dummy = aArgs.SplitChar('#');
    if (dummy) {
      die("Malformed input");
    }
    size_t slot_id = parseNumber(aArgs);
    MemSlot& slot = (*this)[slot_id];
    ::free_impl(slot.mPtr);
    slot.mPtr = nullptr;
    slot.mSize = 0;
  }

  void memalign(MemSlot& aSlot, Buffer& aArgs)
  {
    mOps++;
    size_t alignment = parseNumber(aArgs.SplitChar(','));
    size_t size = parseNumber(aArgs);
    aSlot.mPtr = ::memalign_impl(alignment, size);
    aSlot.mSize = size;
    Commit(aSlot);
  }

  void valloc(MemSlot& aSlot, Buffer& aArgs)
  {
    mOps++;
    size_t size = parseNumber(aArgs);
    aSlot.mPtr = ::valloc_impl(size);
    aSlot.mSize = size;
    Commit(aSlot);
  }

  void jemalloc_stats(Buffer& aArgs)
  {
    if (aArgs) {
      die("Malformed input");
    }
    jemalloc_stats_t stats;
    ::jemalloc_stats_impl(&stats);
    FdPrintf(mStdErr,
             "#%zu mapped: %zu; allocated: %zu; waste: %zu; dirty: %zu; "
             "bookkeep: %zu; binunused: %zu\n", mOps, stats.mapped,
             stats.allocated, stats.waste, stats.page_cache,
             stats.bookkeeping, stats.bin_unused);
    /* TODO: Add more data, like actual RSS as measured by OS, but compensated
     * for the replay internal data. */
  }

private:
  void Commit(MemSlot& aSlot)
  {
    memset(aSlot.mPtr, 0x5a, aSlot.mSize);
  }

  intptr_t mStdErr;
  size_t mOps;
  MemSlotList mSlots;
};


int
main()
{
  size_t first_pid = 0;
  FdReader reader(0);
  Replay replay;

#if defined(_WIN32) && !defined(MOZ_JEMALLOC4)
  malloc_init_hard();
#endif

  /* Read log from stdin and dispatch function calls to the Replay instance.
   * The log format is essentially:
   *   <pid> <function>([<args>])[=<result>]
   * <args> is a comma separated list of arguments.
   *
   * The logs are expected to be preprocessed so that allocations are
   * attributed a tracking slot. The input is trusted not to have crazy
   * values for these slot numbers.
   *
   * <result>, as well as some of the args to some of the function calls are
   * such slot numbers.
   */
  while (true) {
    Buffer line = reader.ReadLine();

    if (!line) {
      break;
    }

    size_t pid = parseNumber(line.SplitChar(' '));
    if (!first_pid) {
      first_pid = pid;
    }

    /* The log may contain data for several processes, only entries for the
     * very first that appears are treated. */
    if (first_pid != pid) {
      continue;
    }

    Buffer func = line.SplitChar('(');
    Buffer args = line.SplitChar(')');

    /* jemalloc_stats and free are functions with no result. */
    if (func == Buffer("jemalloc_stats")) {
      replay.jemalloc_stats(args);
      continue;
    } else if (func == Buffer("free")) {
      replay.free(args);
      continue;
    }

    /* Parse result value and get the corresponding slot. */
    Buffer dummy = line.SplitChar('=');
    Buffer dummy2 = line.SplitChar('#');
    if (dummy || dummy2) {
      die("Malformed input");
    }

    size_t slot_id = parseNumber(line);
    MemSlot& slot = replay[slot_id];

    if (func == Buffer("malloc")) {
      replay.malloc(slot, args);
    } else if (func == Buffer("posix_memalign")) {
      replay.posix_memalign(slot, args);
    } else if (func == Buffer("aligned_alloc")) {
      replay.aligned_alloc(slot, args);
    } else if (func == Buffer("calloc")) {
      replay.calloc(slot, args);
    } else if (func == Buffer("realloc")) {
      replay.realloc(slot, args);
    } else if (func == Buffer("memalign")) {
      replay.memalign(slot, args);
    } else if (func == Buffer("valloc")) {
      replay.valloc(slot, args);
    } else {
      die("Malformed input");
    }
  }

  return 0;
}
