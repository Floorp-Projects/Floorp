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
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "mozilla/Assertions.h"
#include "mozilla/MathAlgorithms.h"
#include "FdPrintf.h"

static void die(const char* message) {
  /* Here, it doesn't matter that fprintf may allocate memory. */
  fprintf(stderr, "%s\n", message);
  exit(1);
}

/* We don't want to be using malloc() to allocate our internal tracking
 * data, because that would change the parameters of what is being measured,
 * so we want to use data types that directly use mmap/VirtualAlloc. */
template <typename T, size_t Len>
class MappedArray {
 public:
  MappedArray() : mPtr(nullptr) {}

  ~MappedArray() {
    if (mPtr) {
#ifdef _WIN32
      VirtualFree(mPtr, sizeof(T) * Len, MEM_RELEASE);
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
    mPtr = reinterpret_cast<T*>(mmap(nullptr, sizeof(T) * Len,
                                     PROT_READ | PROT_WRITE,
                                     MAP_ANON | MAP_PRIVATE, -1, 0));
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
  static const size_t kGroups = 1024 - 1;
  static const size_t kGroupSize = (1024 * 1024) / sizeof(MemSlot);

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

  /* Returns whether the buffer is empty. */
  explicit operator bool() { return mLength; }

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

 private:
  const char* mBuf;
  size_t mLength;
};

/* Helper class to read from a file descriptor line by line. */
class FdReader {
 public:
  explicit FdReader(int aFd)
      : mFd(aFd), mData(&mRawBuf, 0), mBuf(&mRawBuf, sizeof(mRawBuf)) {}

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

size_t parseNumber(Buffer aBuf) {
  if (!aBuf) {
    die("Malformed input");
  }

  size_t result = 0;
  for (const char *c = aBuf.get(), *end = aBuf.GetEnd(); c < end; c++) {
    if (*c < '0' || *c > '9') {
      die("Malformed input");
    }
    result *= 10;
    result += *c - '0';
  }
  return result;
}

/* Class to handle dispatching the replay function calls to replace-malloc. */
class Replay {
 public:
  Replay()
      : mOps(0),
        mNumUsedSlots(0),
        mTotalRequestedSize(0),
        mTotalAllocatedSize(0),
        mCalculateSlop(false) {
#ifdef _WIN32
    // See comment in FdPrintf.h as to why native win32 handles are used.
    mStdErr = reinterpret_cast<intptr_t>(GetStdHandle(STD_ERROR_HANDLE));
#else
    mStdErr = fileno(stderr);
#endif
  }

  void enableSlopCalculation() { mCalculateSlop = true; }

  MemSlot& operator[](size_t index) const { return mSlots[index]; }

  void malloc(Buffer& aArgs, Buffer& aResult) {
    MemSlot& aSlot = SlotForResult(aResult);
    mOps++;
    size_t size = parseNumber(aArgs);
    aSlot.mPtr = ::malloc_impl(size);
    if (aSlot.mPtr) {
      aSlot.mRequest = size;
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

    FdPrintf(mStdErr, "\n");
    FdPrintf(mStdErr, "Objects:      %9zu\n", num_objects);
    FdPrintf(mStdErr, "Slots:        %9zu\n", mNumUsedSlots);
    FdPrintf(mStdErr, "Ops:          %9zu\n", mOps);
    FdPrintf(mStdErr, "mapped:       %9zu\n", stats.mapped);
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

    unsigned last_size = 0;
    for (auto& bin : bin_stats) {
      if (bin.size == 0) {
        break;
      }

      if (bin.size <= 16) {
        // 1 byte buckets.
        print_distribution(bin.size, last_size, 1);
      } else if (bin.size <= stats.quantum_max) {
        // 4 buckets, (4 bytes per bucket with a 16 byte quantum).
        print_distribution(bin.size, last_size, stats.quantum / 4);
      } else {
        // 16 buckets.
        print_distribution(bin.size, last_size, (bin.size - last_size) / 16);
      }

      last_size = bin.size;
    }

    // 16 buckets.
    print_distribution(stats.page_size, last_size,
                       (stats.page_size - last_size) / 16);

    // Buckets are 1/4 of the page size (12 buckets).
    print_distribution(stats.page_size * 4, stats.page_size,
                       stats.page_size / 4);

    /* TODO: Add more data, like actual RSS as measured by OS, but compensated
     * for the replay internal data. */
  }

 private:
  const size_t MAX_NUM_BUCKETS = 16;

  /*
   * Create and print frequency distributions of memory requests.
   */
  void print_distribution(size_t size, size_t next_smallest,
                          size_t bucket_size) {
    unsigned shift = mozilla::CeilingLog2(bucket_size);

    // The number of slots.
    const unsigned array_slots = (size - next_smallest) >> shift;

    // The translation to turn a slot index into a memory request size.
    const unsigned array_offset = 1 + next_smallest;
    const size_t array_offset_add = (1 << shift) + next_smallest;

    // Avoid a variable length array.
    MOZ_RELEASE_ASSERT(array_slots <= MAX_NUM_BUCKETS);
    size_t requests[MAX_NUM_BUCKETS];
    memset(requests, 0, sizeof(size_t) * array_slots);
    size_t total_requests = 0;

    for (size_t slot_id = 0; slot_id < mNumUsedSlots; slot_id++) {
      MemSlot& slot = mSlots[slot_id];
      if (slot.mPtr && slot.mRequest > next_smallest && slot.mRequest <= size) {
        requests[(slot.mRequest - array_offset) >> shift]++;
        total_requests++;
      }
    }

    FdPrintf(mStdErr, "\n%zu-bin Distribution:\n", size);
    FdPrintf(mStdErr, "   request   :  count percent\n");
    size_t range_start = next_smallest + 1;
    for (size_t j = 0; j < array_slots; j++) {
      size_t range_end = (j << shift) + array_offset_add;
      FdPrintf(mStdErr, "%5zu - %5zu: %6zu %6zu%%\n", range_start, range_end,
               requests[j], percent(requests[j], total_requests));
      range_start = range_end + 1;
    }
  }

  static size_t percent(size_t a, size_t b) {
    if (!b) {
      return 0;
    }
    return size_t(round(double(a) / double(b) * 100.0));
  }

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

  intptr_t mStdErr;
  size_t mOps;

  // The number of slots that have been used. It is used to iterate over slots
  // without accessing those we haven't initialised.
  size_t mNumUsedSlots;

  MemSlotList mSlots;
  size_t mTotalRequestedSize;
  size_t mTotalAllocatedSize;
  // Whether to calculate slop for all allocations over the runtime of a
  // process.
  bool mCalculateSlop;
};

int main(int argc, const char* argv[]) {
  size_t first_pid = 0;
  FdReader reader(0);
  Replay replay;

  for (int i = 1; i < argc; i++) {
    const char* option = argv[i];
    if (strcmp(option, "-s") == 0) {
      replay.enableSlopCalculation();
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
