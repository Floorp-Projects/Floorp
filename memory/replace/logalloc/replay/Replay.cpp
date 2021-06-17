/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define MOZ_MEMORY_IMPL
#include "mozmemory_wrap.h"

#ifdef _WIN32
#  include <windows.h>
#  include <io.h>
typedef intptr_t ssize_t;
#else
#  include <sys/mman.h>
#  include <unistd.h>
#endif
#ifdef XP_LINUX
#  include <sys/types.h>
#  include <sys/stat.h>
#  include <fcntl.h>
#  include <stdlib.h>
#  include <ctype.h>
#endif
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "mozilla/Assertions.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/Maybe.h"
#include "FdPrintf.h"

using namespace mozilla;

static void die(const char* message) {
  /* Here, it doesn't matter that fprintf may allocate memory. */
  fprintf(stderr, "%s\n", message);
  exit(1);
}

#ifdef XP_LINUX
static size_t sPageSize = []() { return sysconf(_SC_PAGESIZE); }();
#endif

/* We don't want to be using malloc() to allocate our internal tracking
 * data, because that would change the parameters of what is being measured,
 * so we want to use data types that directly use mmap/VirtualAlloc. */
template <typename T, size_t Len>
class MappedArray {
 public:
  MappedArray() : mPtr(nullptr) {
#ifdef XP_LINUX
    MOZ_RELEASE_ASSERT(!((sizeof(T) * Len) & (sPageSize - 1)),
                       "MappedArray size must be a multiple of the page size");
#endif
  }

  ~MappedArray() {
    if (mPtr) {
#ifdef _WIN32
      VirtualFree(mPtr, sizeof(T) * Len, MEM_RELEASE);
#elif defined(XP_LINUX)
      munmap(reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(mPtr) -
                                     sPageSize),
             sizeof(T) * Len + sPageSize * 2);
#else
      munmap(mPtr, sizeof(T) * Len);
#endif
    }
  }

  T& operator[](size_t aIndex) const {
    if (mPtr) {
      return mPtr[aIndex];
    }

#ifdef _WIN32
    mPtr = reinterpret_cast<T*>(VirtualAlloc(
        nullptr, sizeof(T) * Len, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (mPtr == nullptr) {
      die("VirtualAlloc error");
    }
#else
    size_t data_size = sizeof(T) * Len;
    size_t size = data_size;
#  ifdef XP_LINUX
    // See below
    size += sPageSize * 2;
#  endif
    mPtr = reinterpret_cast<T*>(mmap(nullptr, size, PROT_READ | PROT_WRITE,
                                     MAP_ANON | MAP_PRIVATE, -1, 0));
    if (mPtr == MAP_FAILED) {
      die("Mmap error");
    }
#  ifdef XP_LINUX
    // On Linux we request a page on either side of the allocation and
    // mprotect them.  This prevents mappings in /proc/self/smaps from being
    // merged and allows us to parse this file to calculate the allocator's RSS.
    MOZ_ASSERT(0 == mprotect(mPtr, sPageSize, 0));
    MOZ_ASSERT(0 == mprotect(reinterpret_cast<void*>(
                                 reinterpret_cast<uintptr_t>(mPtr) + data_size +
                                 sPageSize),
                             sPageSize, 0));
    mPtr = reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(mPtr) + sPageSize);
#  endif
#endif
    return mPtr[aIndex];
  }

  bool ownsMapping(uintptr_t addr) const { return addr == (uintptr_t)mPtr; }

  bool allocated() const { return !!mPtr; }

 private:
  mutable T* mPtr;
};

/* Type for records of allocations. */
struct MemSlot {
  void* mPtr;

  // mRequest is only valid if mPtr is non-null.  It doesn't need to be cleared
  // when memory is freed or realloc()ed.
  size_t mRequest;
};

/* An almost infinite list of slots.
 * In essence, this is a linked list of arrays of groups of slots.
 * Each group is 1MB. On 64-bits, one group allows to store 64k allocations.
 * Each MemSlotList instance can store 1023 such groups, which means more
 * than 67M allocations. In case more would be needed, we chain to another
 * MemSlotList, and so on.
 * Using 1023 groups makes the MemSlotList itself page sized on 32-bits
 * and 2 pages-sized on 64-bits.
 */
class MemSlotList {
  static constexpr size_t kGroups = 1024 - 1;
  static constexpr size_t kGroupSize = (1024 * 1024) / sizeof(MemSlot);

  MappedArray<MemSlot, kGroupSize> mSlots[kGroups];
  MappedArray<MemSlotList, 1> mNext;

 public:
  MemSlot& operator[](size_t aIndex) const {
    if (aIndex < kGroupSize * kGroups) {
      return mSlots[aIndex / kGroupSize][aIndex % kGroupSize];
    }
    aIndex -= kGroupSize * kGroups;
    return mNext[0][aIndex];
  }

  // Ask if any of the memory-mapped buffers use this range.
  bool ownsMapping(uintptr_t aStart) const {
    for (const auto& slot : mSlots) {
      if (slot.allocated() && slot.ownsMapping(aStart)) {
        return true;
      }
    }
    return mNext.ownsMapping(aStart) ||
           (mNext.allocated() && mNext[0].ownsMapping(aStart));
  }
};

/* Helper class for memory buffers */
class Buffer {
 public:
  Buffer() : mBuf(nullptr), mLength(0) {}

  Buffer(const void* aBuf, size_t aLength)
      : mBuf(reinterpret_cast<const char*>(aBuf)), mLength(aLength) {}

  /* Constructor for string literals. */
  template <size_t Size>
  explicit Buffer(const char (&aStr)[Size]) : mBuf(aStr), mLength(Size - 1) {}

  /* Returns a sub-buffer up-to but not including the given aNeedle character.
   * The "parent" buffer itself is altered to begin after the aNeedle
   * character.
   * If the aNeedle character is not found, return the entire buffer, and empty
   * the "parent" buffer. */
  Buffer SplitChar(char aNeedle) {
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

  // Advance to the position after aNeedle.  This is like SplitChar but does not
  // return the skipped portion.
  void Skip(char aNeedle, unsigned nTimes = 1) {
    for (unsigned i = 0; i < nTimes; i++) {
      SplitChar(aNeedle);
    }
  }

  void SkipWhitespace() {
    while (mLength > 0) {
      if (!IsSpace(mBuf[0])) {
        break;
      }
      mBuf++;
      mLength--;
    }
  }

  static bool IsSpace(char c) {
    switch (c) {
      case ' ':
      case '\t':
      case '\n':
      case '\v':
      case '\f':
      case '\r':
        return true;
    }
    return false;
  }

  /* Returns a sub-buffer of at most aLength characters. The "parent" buffer is
   * amputated of those aLength characters. If the "parent" buffer is smaller
   * than aLength, then its length is used instead. */
  Buffer Split(size_t aLength) {
    Buffer result(mBuf, std::min(aLength, mLength));
    mLength -= result.mLength;
    mBuf += result.mLength;
    return result;
  }

  /* Move the buffer (including its content) to the memory address of the aOther
   * buffer. */
  void Slide(Buffer aOther) {
    memmove(const_cast<char*>(aOther.mBuf), mBuf, mLength);
    mBuf = aOther.mBuf;
  }

  /* Returns whether the two involved buffers have the same content. */
  bool operator==(Buffer aOther) {
    return mLength == aOther.mLength &&
           (mBuf == aOther.mBuf || !strncmp(mBuf, aOther.mBuf, mLength));
  }

  bool operator!=(Buffer aOther) { return !(*this == aOther); }

  /* Returns true if the buffer is not empty. */
  explicit operator bool() { return mLength; }

  char operator[](size_t n) const { return mBuf[n]; }

  /* Returns the memory location of the buffer. */
  const char* get() { return mBuf; }

  /* Returns the memory location of the end of the buffer (technically, the
   * first byte after the buffer). */
  const char* GetEnd() { return mBuf + mLength; }

  /* Extend the buffer over the content of the other buffer, assuming it is
   * adjacent. */
  void Extend(Buffer aOther) {
    MOZ_ASSERT(aOther.mBuf == GetEnd());
    mLength += aOther.mLength;
  }

  size_t Length() const { return mLength; }

 private:
  const char* mBuf;
  size_t mLength;
};

/* Helper class to read from a file descriptor line by line. */
class FdReader {
 public:
  explicit FdReader(int aFd, bool aNeedClose = false)
      : mFd(aFd),
        mNeedClose(aNeedClose),
        mData(&mRawBuf, 0),
        mBuf(&mRawBuf, sizeof(mRawBuf)) {}

  FdReader(FdReader&& aOther) noexcept
      : mFd(aOther.mFd),
        mNeedClose(aOther.mNeedClose),
        mData(&mRawBuf, 0),
        mBuf(&mRawBuf, sizeof(mRawBuf)) {
    memcpy(mRawBuf, aOther.mRawBuf, sizeof(mRawBuf));
    aOther.mFd = -1;
    aOther.mNeedClose = false;
    aOther.mData = Buffer();
    aOther.mBuf = Buffer();
  }

  FdReader& operator=(const FdReader&) = delete;
  FdReader(const FdReader&) = delete;

  ~FdReader() {
    if (mNeedClose) {
      close(mFd);
    }
  }

  /* Read a line from the file descriptor and returns it as a Buffer instance */
  Buffer ReadLine() {
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
  void FillBuffer() {
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
  bool mNeedClose;

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
  return_type name##_impl(__VA_ARGS__);
#define MALLOC_FUNCS MALLOC_FUNCS_MALLOC
#include "malloc_decls.h"

#define MALLOC_DECL(name, return_type, ...) return_type name(__VA_ARGS__);
#define MALLOC_FUNCS MALLOC_FUNCS_JEMALLOC
#include "malloc_decls.h"

#ifdef ANDROID

/* mozjemalloc and jemalloc use pthread_atfork, which Android doesn't have.
 * While gecko has one in libmozglue, the replay program can't use that.
 * Since we're not going to fork anyways, make it a dummy function. */
int pthread_atfork(void (*aPrepare)(void), void (*aParent)(void),
                   void (*aChild)(void)) {
  return 0;
}
#endif

MOZ_END_EXTERN_C

template <unsigned Base = 10>
size_t parseNumber(Buffer aBuf) {
  if (!aBuf) {
    die("Malformed input");
  }

  size_t result = 0;
  for (const char *c = aBuf.get(), *end = aBuf.GetEnd(); c < end; c++) {
    result *= Base;
    if ((*c >= '0' && *c <= '9')) {
      result += *c - '0';
    } else if (Base == 16 && *c >= 'a' && *c <= 'f') {
      result += *c - 'a' + 10;
    } else if (Base == 16 && *c >= 'A' && *c <= 'F') {
      result += *c - 'A' + 10;
    } else {
      die("Malformed input");
    }
  }
  return result;
}

static size_t percent(size_t a, size_t b) {
  if (!b) {
    return 0;
  }
  return size_t(round(double(a) / double(b) * 100.0));
}

class Distribution {
 public:
  // Default constructor used for array initialisation.
  Distribution()
      : mMaxSize(0),
        mNextSmallest(0),
        mShift(0),
        mArrayOffset(0),
        mArraySlots(0),
        mTotalRequests(0),
        mRequests{0} {}

  Distribution(size_t max_size, size_t next_smallest, size_t bucket_size)
      : mMaxSize(max_size),
        mNextSmallest(next_smallest),
        mShift(CeilingLog2(bucket_size)),
        mArrayOffset(1 + next_smallest),
        mArraySlots((max_size - next_smallest) >> mShift),
        mTotalRequests(0),
        mRequests{
            0,
        } {
    MOZ_ASSERT(mMaxSize);
    MOZ_RELEASE_ASSERT(mArraySlots <= MAX_NUM_BUCKETS);
  }

  Distribution& operator=(const Distribution& aOther) = default;

  void addRequest(size_t request) {
    MOZ_ASSERT(mMaxSize);

    mRequests[(request - mArrayOffset) >> mShift]++;
    mTotalRequests++;
  }

  void printDist(intptr_t std_err) {
    MOZ_ASSERT(mMaxSize);

    // The translation to turn a slot index into a memory request size.
    const size_t array_offset_add = (1 << mShift) + mNextSmallest;

    FdPrintf(std_err, "\n%zu-bin Distribution:\n", mMaxSize);
    FdPrintf(std_err, "   request   :  count percent\n");
    size_t range_start = mNextSmallest + 1;
    for (size_t j = 0; j < mArraySlots; j++) {
      size_t range_end = (j << mShift) + array_offset_add;
      FdPrintf(std_err, "%5zu - %5zu: %6zu %6zu%%\n", range_start, range_end,
               mRequests[j], percent(mRequests[j], mTotalRequests));
      range_start = range_end + 1;
    }
  }

  size_t maxSize() const { return mMaxSize; }

 private:
  static constexpr size_t MAX_NUM_BUCKETS = 16;

  // If size is zero this distribution is uninitialised.
  size_t mMaxSize;
  size_t mNextSmallest;

  // Parameters to convert a size into a slot number.
  unsigned mShift;
  unsigned mArrayOffset;

  // The number of slots.
  unsigned mArraySlots;

  size_t mTotalRequests;
  size_t mRequests[MAX_NUM_BUCKETS];
};

#ifdef XP_LINUX
struct MemoryMap {
  uintptr_t mStart;
  uintptr_t mEnd;
  bool mReadable;
  bool mPrivate;
  bool mAnon;
  bool mIsStack;
  bool mIsSpecial;
  size_t mRSS;

  bool IsCandidate() const {
    // Candidates mappings are:
    //  * anonymous
    //  * they are private (not shared),
    //  * anonymous or "[heap]" (not another area such as stack),
    //
    // The only mappings we're falsely including are the .bss segments for
    // shared libraries.
    return mReadable && mPrivate && mAnon && !mIsStack && !mIsSpecial;
  }
};

class SMapsReader : private FdReader {
 private:
  explicit SMapsReader(FdReader&& reader) : FdReader(std::move(reader)) {}

 public:
  static Maybe<SMapsReader> open() {
    int fd = ::open(FILENAME, O_RDONLY);
    if (fd < 0) {
      perror(FILENAME);
      return mozilla::Nothing();
    }

    return Some(SMapsReader(FdReader(fd, true)));
  }

  Maybe<MemoryMap> readMap(intptr_t aStdErr) {
    // This is not very tolerant of format changes because things like
    // parseNumber will crash if they get a bad value.  TODO: make this
    // soft-fail.

    Buffer line = ReadLine();
    if (!line) {
      return Nothing();
    }

    // We're going to be at the start of an entry, start tokenising the first
    // line.

    // Range
    Buffer range = line.SplitChar(' ');
    uintptr_t range_start = parseNumber<16>(range.SplitChar('-'));
    uintptr_t range_end = parseNumber<16>(range);

    // Mode.
    Buffer mode = line.SplitChar(' ');
    if (mode.Length() != 4) {
      FdPrintf(aStdErr, "Couldn't parse SMAPS file\n");
      return Nothing();
    }
    bool readable = mode[0] == 'r';
    bool private_ = mode[3] == 'p';

    // Offset, device and inode.
    line.SkipWhitespace();
    bool zero_offset = !parseNumber<16>(line.SplitChar(' '));
    line.SkipWhitespace();
    bool no_device = line.SplitChar(' ') == Buffer("00:00");
    line.SkipWhitespace();
    bool zero_inode = !parseNumber(line.SplitChar(' '));
    bool is_anon = zero_offset && no_device && zero_inode;

    // Filename, or empty for anon mappings.
    line.SkipWhitespace();
    Buffer filename = line.SplitChar(' ');

    bool is_stack;
    bool is_special;
    if (filename && filename[0] == '[') {
      is_stack = filename == Buffer("[stack]");
      is_special = filename == Buffer("[vdso]") ||
                   filename == Buffer("[vvar]") ||
                   filename == Buffer("[vsyscall]");
    } else {
      is_stack = false;
      is_special = false;
    }

    size_t rss = 0;
    while ((line = ReadLine())) {
      Buffer field = line.SplitChar(':');
      if (field == Buffer("VmFlags")) {
        // This is the last field, at least in the current format. Break this
        // loop to read the next mapping.
        break;
      }

      if (field == Buffer("Rss")) {
        line.SkipWhitespace();
        Buffer value = line.SplitChar(' ');
        rss = parseNumber(value) * 1024;
      }
    }

    return Some(MemoryMap({range_start, range_end, readable, private_, is_anon,
                           is_stack, is_special, rss}));
  }

  static constexpr char FILENAME[] = "/proc/self/smaps";
};
#endif  // XP_LINUX

/* Class to handle dispatching the replay function calls to replace-malloc. */
class Replay {
 public:
  Replay() {
#ifdef _WIN32
    // See comment in FdPrintf.h as to why native win32 handles are used.
    mStdErr = reinterpret_cast<intptr_t>(GetStdHandle(STD_ERROR_HANDLE));
#else
    mStdErr = fileno(stderr);
#endif
#ifdef XP_LINUX
    BuildInitialMapInfo();
#endif
  }

  void enableSlopCalculation() { mCalculateSlop = true; }
  void enableMemset() { mDoMemset = true; }

  MemSlot& operator[](size_t index) const { return mSlots[index]; }

  void malloc(Buffer& aArgs, Buffer& aResult) {
    MemSlot& aSlot = SlotForResult(aResult);
    mOps++;
    size_t size = parseNumber(aArgs);
    aSlot.mPtr = ::malloc_impl(size);
    if (aSlot.mPtr) {
      aSlot.mRequest = size;
      MaybeCommit(aSlot);
      if (mCalculateSlop) {
        mTotalRequestedSize += size;
        mTotalAllocatedSize += ::malloc_usable_size_impl(aSlot.mPtr);
      }
    }
  }

  void posix_memalign(Buffer& aArgs, Buffer& aResult) {
    MemSlot& aSlot = SlotForResult(aResult);
    mOps++;
    size_t alignment = parseNumber(aArgs.SplitChar(','));
    size_t size = parseNumber(aArgs);
    void* ptr;
    if (::posix_memalign_impl(&ptr, alignment, size) == 0) {
      aSlot.mPtr = ptr;
      aSlot.mRequest = size;
      MaybeCommit(aSlot);
      if (mCalculateSlop) {
        mTotalRequestedSize += size;
        mTotalAllocatedSize += ::malloc_usable_size_impl(aSlot.mPtr);
      }
    } else {
      aSlot.mPtr = nullptr;
    }
  }

  void aligned_alloc(Buffer& aArgs, Buffer& aResult) {
    MemSlot& aSlot = SlotForResult(aResult);
    mOps++;
    size_t alignment = parseNumber(aArgs.SplitChar(','));
    size_t size = parseNumber(aArgs);
    aSlot.mPtr = ::aligned_alloc_impl(alignment, size);
    if (aSlot.mPtr) {
      aSlot.mRequest = size;
      MaybeCommit(aSlot);
      if (mCalculateSlop) {
        mTotalRequestedSize += size;
        mTotalAllocatedSize += ::malloc_usable_size_impl(aSlot.mPtr);
      }
    }
  }

  void calloc(Buffer& aArgs, Buffer& aResult) {
    MemSlot& aSlot = SlotForResult(aResult);
    mOps++;
    size_t num = parseNumber(aArgs.SplitChar(','));
    size_t size = parseNumber(aArgs);
    aSlot.mPtr = ::calloc_impl(num, size);
    if (aSlot.mPtr) {
      aSlot.mRequest = num * size;
      MaybeCommit(aSlot);
      if (mCalculateSlop) {
        mTotalRequestedSize += num * size;
        mTotalAllocatedSize += ::malloc_usable_size_impl(aSlot.mPtr);
      }
    }
  }

  void realloc(Buffer& aArgs, Buffer& aResult) {
    MemSlot& aSlot = SlotForResult(aResult);
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
    aSlot.mPtr = ::realloc_impl(old_ptr, size);
    if (aSlot.mPtr) {
      aSlot.mRequest = size;
      MaybeCommit(aSlot);
      if (mCalculateSlop) {
        mTotalRequestedSize += size;
        mTotalAllocatedSize += ::malloc_usable_size_impl(aSlot.mPtr);
      }
    }
  }

  void free(Buffer& aArgs, Buffer& aResult) {
    if (aResult) {
      die("Malformed input");
    }
    mOps++;
    Buffer dummy = aArgs.SplitChar('#');
    if (dummy) {
      die("Malformed input");
    }
    size_t slot_id = parseNumber(aArgs);
    MemSlot& slot = (*this)[slot_id];
    ::free_impl(slot.mPtr);
    slot.mPtr = nullptr;
  }

  void memalign(Buffer& aArgs, Buffer& aResult) {
    MemSlot& aSlot = SlotForResult(aResult);
    mOps++;
    size_t alignment = parseNumber(aArgs.SplitChar(','));
    size_t size = parseNumber(aArgs);
    aSlot.mPtr = ::memalign_impl(alignment, size);
    if (aSlot.mPtr) {
      aSlot.mRequest = size;
      MaybeCommit(aSlot);
      if (mCalculateSlop) {
        mTotalRequestedSize += size;
        mTotalAllocatedSize += ::malloc_usable_size_impl(aSlot.mPtr);
      }
    }
  }

  void valloc(Buffer& aArgs, Buffer& aResult) {
    MemSlot& aSlot = SlotForResult(aResult);
    mOps++;
    size_t size = parseNumber(aArgs);
    aSlot.mPtr = ::valloc_impl(size);
    if (aSlot.mPtr) {
      aSlot.mRequest = size;
      MaybeCommit(aSlot);
      if (mCalculateSlop) {
        mTotalRequestedSize += size;
        mTotalAllocatedSize += ::malloc_usable_size_impl(aSlot.mPtr);
      }
    }
  }

  void jemalloc_stats(Buffer& aArgs, Buffer& aResult) {
    if (aArgs || aResult) {
      die("Malformed input");
    }
    mOps++;
    jemalloc_stats_t stats;
    jemalloc_bin_stats_t bin_stats[JEMALLOC_MAX_STATS_BINS];
    ::jemalloc_stats_internal(&stats, bin_stats);

#ifdef XP_LINUX
    size_t rss = get_rss();
#endif

    size_t num_objects = 0;
    size_t num_sloppy_objects = 0;
    size_t total_allocated = 0;
    size_t total_slop = 0;
    size_t large_slop = 0;
    size_t large_used = 0;
    size_t huge_slop = 0;
    size_t huge_used = 0;
    size_t bin_slop[JEMALLOC_MAX_STATS_BINS] = {0};

    for (size_t slot_id = 0; slot_id < mNumUsedSlots; slot_id++) {
      MemSlot& slot = mSlots[slot_id];
      if (slot.mPtr) {
        size_t used = ::malloc_usable_size_impl(slot.mPtr);
        size_t slop = used - slot.mRequest;
        total_allocated += used;
        total_slop += slop;
        num_objects++;
        if (slop) {
          num_sloppy_objects++;
        }

        if (used <= stats.page_size / 2) {
          // We know that this is an inefficient linear search, but there's a
          // small number of bins and this is simple.
          for (unsigned i = 0; i < JEMALLOC_MAX_STATS_BINS; i++) {
            auto& bin = bin_stats[i];
            if (used == bin.size) {
              bin_slop[i] += slop;
              break;
            }
          }
        } else if (used <= stats.large_max) {
          large_slop += slop;
          large_used += used;
        } else {
          huge_slop += slop;
          huge_used += used;
        }
      }
    }

    // This formula corresponds to the calculation of wasted (from committed and
    // the other parameters) within jemalloc_stats()
    size_t committed = stats.allocated + stats.waste + stats.page_cache +
                       stats.bookkeeping + stats.bin_unused;

    FdPrintf(mStdErr, "\n");
    FdPrintf(mStdErr, "Objects:      %9zu\n", num_objects);
    FdPrintf(mStdErr, "Slots:        %9zu\n", mNumUsedSlots);
    FdPrintf(mStdErr, "Ops:          %9zu\n", mOps);
    FdPrintf(mStdErr, "mapped:       %9zu\n", stats.mapped);
    FdPrintf(mStdErr, "committed:    %9zu\n", committed);
#ifdef XP_LINUX
    if (rss) {
      FdPrintf(mStdErr, "rss:          %9zu\n", rss);
    }
#endif
    FdPrintf(mStdErr, "allocated:    %9zu\n", stats.allocated);
    FdPrintf(mStdErr, "waste:        %9zu\n", stats.waste);
    FdPrintf(mStdErr, "dirty:        %9zu\n", stats.page_cache);
    FdPrintf(mStdErr, "bookkeep:     %9zu\n", stats.bookkeeping);
    FdPrintf(mStdErr, "bin-unused:   %9zu\n", stats.bin_unused);
    FdPrintf(mStdErr, "quantum-max:  %9zu\n", stats.quantum_max);
    FdPrintf(mStdErr, "subpage-max:  %9zu\n", stats.page_size / 2);
    FdPrintf(mStdErr, "large-max:    %9zu\n", stats.large_max);
    if (mCalculateSlop) {
      size_t slop = mTotalAllocatedSize - mTotalRequestedSize;
      FdPrintf(mStdErr,
               "Total slop for all allocations: %zuKiB/%zuKiB (%zu%%)\n",
               slop / 1024, mTotalAllocatedSize / 1024,
               percent(slop, mTotalAllocatedSize));
    }
    FdPrintf(mStdErr, "Live sloppy objects: %zu/%zu (%zu%%)\n",
             num_sloppy_objects, num_objects,
             percent(num_sloppy_objects, num_objects));
    FdPrintf(mStdErr, "Live sloppy bytes: %zuKiB/%zuKiB (%zu%%)\n",
             total_slop / 1024, total_allocated / 1024,
             percent(total_slop, total_allocated));

    FdPrintf(mStdErr, "\n%8s %11s %10s %8s %9s %9s %8s\n", "bin-size",
             "unused (c)", "total (c)", "used (c)", "non-full (r)", "total (r)",
             "used (r)");
    for (auto& bin : bin_stats) {
      if (bin.size) {
        FdPrintf(mStdErr, "%8zu %8zuKiB %7zuKiB %7zu%% %12zu %9zu %7zu%%\n",
                 bin.size, bin.bytes_unused / 1024, bin.bytes_total / 1024,
                 percent(bin.bytes_total - bin.bytes_unused, bin.bytes_total),
                 bin.num_non_full_runs, bin.num_runs,
                 percent(bin.num_runs - bin.num_non_full_runs, bin.num_runs));
      }
    }

    FdPrintf(mStdErr, "\n%5s %8s %9s %7s\n", "bin", "slop", "used", "percent");
    for (unsigned i = 0; i < JEMALLOC_MAX_STATS_BINS; i++) {
      auto& bin = bin_stats[i];
      if (bin.size) {
        size_t used = bin.bytes_total - bin.bytes_unused;
        FdPrintf(mStdErr, "%5zu %8zu %9zu %6zu%%\n", bin.size, bin_slop[i],
                 used, percent(bin_slop[i], used));
      }
    }
    FdPrintf(mStdErr, "%5s %8zu %9zu %6zu%%\n", "large", large_slop, large_used,
             percent(large_slop, large_used));
    FdPrintf(mStdErr, "%5s %8zu %9zu %6zu%%\n", "huge", huge_slop, huge_used,
             percent(huge_slop, huge_used));

    print_distributions(stats, bin_stats);
  }

 private:
  /*
   * Create and print frequency distributions of memory requests.
   */
  void print_distributions(
      jemalloc_stats_t& stats,
      jemalloc_bin_stats_t (&bin_stats)[JEMALLOC_MAX_STATS_BINS]) {
    // We compute distributions for all of the bins for small allocations
    // (JEMALLOC_MAX_STATS_BINS) plus two more distributions for larger
    // allocations.
    Distribution dists[JEMALLOC_MAX_STATS_BINS + 2];

    unsigned last_size = 0;
    unsigned num_dists = 0;
    for (auto& bin : bin_stats) {
      if (bin.size == 0) {
        break;
      }
      auto& dist = dists[num_dists++];

      if (bin.size <= 16) {
        // 1 byte buckets.
        dist = Distribution(bin.size, last_size, 1);
      } else if (bin.size <= stats.quantum_max) {
        // 4 buckets, (4 bytes per bucket with a 16 byte quantum).
        dist = Distribution(bin.size, last_size, stats.quantum / 4);
      } else {
        // 16 buckets.
        dist = Distribution(bin.size, last_size, (bin.size - last_size) / 16);
      }
      last_size = bin.size;
    }

    // 16 buckets.
    dists[num_dists] = Distribution(stats.page_size, last_size,
                                    (stats.page_size - last_size) / 16);
    num_dists++;

    // Buckets are 1/4 of the page size (12 buckets).
    dists[num_dists] =
        Distribution(stats.page_size * 4, stats.page_size, stats.page_size / 4);
    num_dists++;

    MOZ_RELEASE_ASSERT(num_dists <= JEMALLOC_MAX_STATS_BINS + 2);

    for (size_t slot_id = 0; slot_id < mNumUsedSlots; slot_id++) {
      MemSlot& slot = mSlots[slot_id];
      if (slot.mPtr) {
        for (size_t i = 0; i < num_dists; i++) {
          if (slot.mRequest <= dists[i].maxSize()) {
            dists[i].addRequest(slot.mRequest);
            break;
          }
        }
      }
    }

    for (unsigned i = 0; i < num_dists; i++) {
      dists[i].printDist(mStdErr);
    }
  }

#ifdef XP_LINUX
  size_t get_rss() {
    if (mGetRSSFailed) {
      return 0;
    }

    // On Linux we can determine the RSS of the heap area by examining the
    // smaps file.
    mozilla::Maybe<SMapsReader> reader = SMapsReader::open();
    if (!reader) {
      mGetRSSFailed = true;
      return 0;
    }

    size_t rss = 0;
    while (Maybe<MemoryMap> map = reader->readMap(mStdErr)) {
      if (map->IsCandidate() && !mSlots.ownsMapping(map->mStart) &&
          !InitialMapsContains(map->mStart)) {
        rss += map->mRSS;
      }
    }

    return rss;
  }

  bool InitialMapsContains(uintptr_t aRangeStart) {
    for (unsigned i = 0; i < mNumInitialMaps; i++) {
      MOZ_ASSERT(i < MAX_INITIAL_MAPS);

      if (mInitialMaps[i] == aRangeStart) {
        return true;
      }
    }
    return false;
  }

 public:
  void BuildInitialMapInfo() {
    if (mGetRSSFailed) {
      return;
    }

    Maybe<SMapsReader> reader = SMapsReader::open();
    if (!reader) {
      mGetRSSFailed = true;
      return;
    }

    while (Maybe<MemoryMap> map = reader->readMap(mStdErr)) {
      if (map->IsCandidate()) {
        if (mNumInitialMaps >= MAX_INITIAL_MAPS) {
          FdPrintf(mStdErr, "Too many initial mappings, can't compute RSS\n");
          mGetRSSFailed = false;
          return;
        }

        mInitialMaps[mNumInitialMaps++] = map->mStart;
      }
    }
  }
#endif

 private:
  MemSlot& SlotForResult(Buffer& aResult) {
    /* Parse result value and get the corresponding slot. */
    Buffer dummy = aResult.SplitChar('=');
    Buffer dummy2 = aResult.SplitChar('#');
    if (dummy || dummy2) {
      die("Malformed input");
    }

    size_t slot_id = parseNumber(aResult);
    mNumUsedSlots = std::max(mNumUsedSlots, slot_id + 1);

    return mSlots[slot_id];
  }

  void MaybeCommit(MemSlot& aSlot) {
    if (mDoMemset) {
      // Write any byte, 0x55 isn't significant.
      memset(aSlot.mPtr, 0x55, aSlot.mRequest);
    }
  }

  intptr_t mStdErr;
  size_t mOps = 0;

  // The number of slots that have been used. It is used to iterate over slots
  // without accessing those we haven't initialised.
  size_t mNumUsedSlots = 0;

  MemSlotList mSlots;
  size_t mTotalRequestedSize = 0;
  size_t mTotalAllocatedSize = 0;
  // Whether to calculate slop for all allocations over the runtime of a
  // process.
  bool mCalculateSlop = false;
  bool mDoMemset = false;

#ifdef XP_LINUX
  // If we have a failure reading smaps info then this is used to disable that
  // feature.
  bool mGetRSSFailed = false;

  // The initial memory mappings are recorded here at start up.  We exclude
  // memory in these mappings when computing RSS.  We assume they do not grow
  // and that no regions are allocated near them, this is true because they'll
  // only record the .bss and .data segments from our binary and shared objects
  // or regions that logalloc-replay has created for MappedArrays.
  //
  // 64 should be enough for anybody.
  static constexpr unsigned MAX_INITIAL_MAPS = 64;
  uintptr_t mInitialMaps[MAX_INITIAL_MAPS];
  unsigned mNumInitialMaps = 0;
#endif  // XP_LINUX
};

static Replay replay;

int main(int argc, const char* argv[]) {
  size_t first_pid = 0;
  FdReader reader(0);

  for (int i = 1; i < argc; i++) {
    const char* option = argv[i];
    if (strcmp(option, "-s") == 0) {
      // Do accounting to calculate allocation slop.
      replay.enableSlopCalculation();
    } else if (strcmp(option, "-c") == 0) {
      // Touch memory as we allocate it.
      replay.enableMemset();
    } else {
      fprintf(stderr, "Unknown command line option: %s\n", option);
      return EXIT_FAILURE;
    }
  }

  /* Read log from stdin and dispatch function calls to the Replay instance.
   * The log format is essentially:
   *   <pid> <tid> <function>([<args>])[=<result>]
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

    /* The log contains thread ids for manual analysis, but we just ignore them
     * for now. */
    parseNumber(line.SplitChar(' '));

    Buffer func = line.SplitChar('(');
    Buffer args = line.SplitChar(')');

    if (func == Buffer("jemalloc_stats")) {
      replay.jemalloc_stats(args, line);
    } else if (func == Buffer("free")) {
      replay.free(args, line);
    } else if (func == Buffer("malloc")) {
      replay.malloc(args, line);
    } else if (func == Buffer("posix_memalign")) {
      replay.posix_memalign(args, line);
    } else if (func == Buffer("aligned_alloc")) {
      replay.aligned_alloc(args, line);
    } else if (func == Buffer("calloc")) {
      replay.calloc(args, line);
    } else if (func == Buffer("realloc")) {
      replay.realloc(args, line);
    } else if (func == Buffer("memalign")) {
      replay.memalign(args, line);
    } else if (func == Buffer("valloc")) {
      replay.valloc(args, line);
    } else {
      die("Malformed input");
    }
  }

  return 0;
}
