/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <windows.h>
#include <psapi.h>

#include <stdio.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "../crash_generation/temporary_stack.cc"

static bool failed = false;

namespace metrics {

// A single statistic we measure involving memory use, associated with a
// human-readable name.
struct MemoryStat {
  const char* name;
  SIZE_T PROCESS_MEMORY_COUNTERS::*ptr;
};

// relevant_stats: A list of all memory stats in MemoryRecord (or, equivalently,
// PROCESS_MEMORY_COUNTERS).
#define DECL_MEMORY_STAT(name) \
  { #name, &PROCESS_MEMORY_COUNTERS::name }
// Conveniently, all of these are of type SIZE_T, and in bytes.
// https://docs.microsoft.com/en-us/windows/win32/api/psapi/ns-psapi-process_memory_counters
constexpr MemoryStat relevant_stats[] = {
    DECL_MEMORY_STAT(PeakWorkingSetSize),
    DECL_MEMORY_STAT(WorkingSetSize),
    DECL_MEMORY_STAT(QuotaPeakPagedPoolUsage),
    DECL_MEMORY_STAT(QuotaPagedPoolUsage),
    DECL_MEMORY_STAT(QuotaPeakNonPagedPoolUsage),
    DECL_MEMORY_STAT(QuotaNonPagedPoolUsage),
    DECL_MEMORY_STAT(PagefileUsage),
    DECL_MEMORY_STAT(PeakPagefileUsage),
};
#undef DECL_MEMORY_STAT

// A record of this program's memory use at a given time.
struct MemoryRecord {
  const char* name;  // must be a static constant
  PROCESS_MEMORY_COUNTERS stats;
};

// Get a record of the current memory use of this process.
MemoryRecord get_mem_stats(const char* name) {
  MemoryRecord record{.name = name, .stats = {sizeof(PROCESS_MEMORY_COUNTERS)}};
  ::GetProcessMemoryInfo(::GetCurrentProcess(), &record.stats,
                         sizeof(record.stats));
  return record;
}

// Check that memory use is low (for some definition of "low").
void assert_mem_use_is_reasonable(MemoryRecord const& mem) {
  for (auto const& stat : relevant_stats) {
    size_t const val = mem.stats.*stat.ptr;
    // 10 MB.
    if (val > 10 * 1024 * 1024) {
      failed = true;
      ::fprintf(stderr, "unexpectedly large %30s at %s (%zu)\n", stat.name,
                mem.name, val);
    }
  }
}

// Check that memory use has not more than doubled between two snapshots.
void assert_compare_mem_stats(MemoryRecord const& mem_a,
                              MemoryRecord const& mem_b) {
  for (auto const& stat : relevant_stats) {
    size_t const a = mem_a.stats.*stat.ptr;
    size_t const b = mem_b.stats.*stat.ptr;
    bool const ok = (a <= b && b <= 2 * a);

    if (!ok) {
      failed = true;
      ::fprintf(
          stderr,
          "unusual growth in %30s between '%10s' (%10zu) and '%10s' (%10zu)\n",
          stat.name, mem_a.name, a, mem_b.name, b);
    }
  }
}
}  // namespace metrics

// Test function: compute Fibonacci "locally".
size_t fibonacci(size_t val) {
  if (val == 0 || val == 1) return val;
  return fibonacci(val - 1) + fibonacci(val - 2);
}

// Test function: compute Fibonacci on another stack.
size_t fiberborne_fibonacci(size_t val, size_t stack_size) {
  auto const adaptor = [](void* arg) {
    size_t* valptr = (size_t*)arg;
    *valptr = fibonacci(*valptr);
  };

  ::RunOnTemporaryStack(adaptor, &val, stack_size);
  return val;
}

// Test that we actually call the fiber procedure.
void test_small() {
  auto const initial = metrics::get_mem_stats("initial");

  constexpr size_t expected_values[10] = {0, 1, 1, 2, 3, 5, 8, 13, 21, 34};
  for (size_t i = 0; i < ARRAYSIZE(expected_values); ++i) {
    auto const actual = ::fiberborne_fibonacci(i, 1024);
    auto const expected = ::fibonacci(i);
    if (actual != expected) {
      failed = true;
      ::fprintf(stderr,
                "Fib(%zu) not computed correctly?! got %zu, expected %zu\n", i,
                actual, expected);
    }
  }

  auto const after_small = metrics::get_mem_stats("after_small");

  assert_compare_mem_stats(initial, after_small);
}

void test_large() {
  // Test that _reserving_ ridiculous amounts of memory doesn't actually result
  // in ridiculous amounts of memory being _used_.
#ifdef _WIN64
  // 1TB is probably more than the testing machine will actually permit to be
  // allocated.
  constexpr size_t very_large_stack_size = size_t(1024) * 1024 * 1024 * 1024;
#else
  // On 32-bit systems, we can't even pretend to allocate 1TB -- it won't fit in
  // a `size_t`, which tops out at 4GB. In reality, process address spaces top
  // out at 2GB (ignoring PAE), and even 1GB might plausibly cause address-space
  // issues. 256MB should be fine, though.
  constexpr size_t very_large_stack_size = size_t(256) * 1024 * 1024;
#endif

  // We do expect a fairly large increase after the first run, presumably due to
  // initialization of some internal structures; but further calls should not
  // increase that.
  //
  // Note that this is an ideal case; repeated reservation and release of large
  // stacks in small environments may fragment memory space.
  ::fiberborne_fibonacci(0, very_large_stack_size);
  auto const after_once = metrics::get_mem_stats("after once");
  assert_mem_use_is_reasonable(after_once);

  for (size_t i = 0; i < 10; ++i) {
    ::fiberborne_fibonacci(i, very_large_stack_size);
    assert_mem_use_is_reasonable(
        metrics::get_mem_stats("somewhere in the loop"));
  }
  auto const after_many = metrics::get_mem_stats("after many");

  assert_compare_mem_stats(after_once, after_many);
}

void test_too_large() {
  bool callback_has_run = false;
  auto const callback = [](void* arg) { *((bool*)arg) = true; };

  const size_t too_large = (size_t(-1));
  auto const ret =
      ::RunOnTemporaryStack(callback, &callback_has_run, too_large);
  if (SUCCEEDED(ret)) {
    ::fprintf(stderr, "Unexpectedly reported success in test_too_large");
    failed = true;
  }
  if (callback_has_run) {
    ::fprintf(stderr, "Unexpectedly ran callback in test_too_large");
    failed = true;
  }
}

// Test that the program exits correctly (rather than by falling off the end of
// a fiber procedure).
struct confirm_normal_exit {
  // When a fiber procedure exits, its host thread also exits _immediately_. If
  // that was the last active thread, then the process also exits, ordinarily
  // with a return code of 0. Since this occurs at the Win32 API level, CRT
  // facilities (such as `atexit` and `at_quick_exit` handlers) are not invoked;
  // to react to the destruction of a thread, we must use a separate thread.
  //
  // (Or a separate process. But let's not go that far.)

 private:
  // WARNING: These handles must survive the struct which creates them!
  struct handles {
    // Handle to the main thread. Signaled when the thread exits.
    HANDLE const hMainThread;
    // Handle to an event which will be signaled by the main thread upon exit
    // from the containing function.
    HANDLE const hExitEvent;
    // Handle to an event which will be signaled by the auxiliary thread when
    // all necessary data has been transferred from the main thread.
    HANDLE const hInitializationEvent;

    handles()
        : hMainThread(confirm_normal_exit::get_current_thread_handle()),
          hExitEvent(::CreateEventW(NULL, TRUE, FALSE, NULL)),
          hInitializationEvent(::CreateEventW(NULL, TRUE, FALSE, NULL)) {}

    ~handles() {
      ::CloseHandle(hInitializationEvent);
      ::CloseHandle(hExitEvent);
      ::CloseHandle(hMainThread);
    }
  };

  // Data shared by the main thread and watcher thread.
  std::shared_ptr<handles> handles_ptr;

  // Listen for the destruction of the main thread (from which we were
  // launched).
  static DWORD ThreadFunc(std::shared_ptr<handles> handles_ptr) {
    HANDLE handles[2] = {handles_ptr->hExitEvent, handles_ptr->hMainThread};
    // Note that if both of these are signaled, we _are_ guaranteed to get
    // hExitEvent indicated as the reason -- not because it went off first
    // (although it did), but because it's the first one in the array.
    //
    // http://msdn.microsoft.com/en-us/library/windows/desktop/ms687025.aspx
    DWORD const ret = ::WaitForMultipleObjects(2, handles, FALSE, INFINITE);
    if (ret != WAIT_OBJECT_0 + 0) {
      failed = true;
      fprintf(stderr, "%s\n",
              "Unexpected exit of main thread: "
              "fiber did not relinquish control?");
      ::ExitProcess(1);
    }
    return 0;
  }

  // ::GetCurrentThread() returns only a context-dependent pseudohandle. Jump
  // through the MSDN-suggested hoops [0] to get a real handle for the current
  // thread.
  //
  // [0] https://msdn.microsoft.com/en-us/library/windows/desktop/ms683182.aspx
  static HANDLE get_current_thread_handle() {
    HANDLE handle;
    const auto ret = ::DuplicateHandle(
        ::GetCurrentProcess(), ::GetCurrentThread(), ::GetCurrentProcess(),
        &handle, SYNCHRONIZE, FALSE, 0);
    if (!ret) {
      const unsigned long err = ::GetLastError();
      ::fprintf(stderr, "DuplicateHandle failed with error code %lu\n", err);
      ::ExitProcess(1);
    }
    return handle;
  }

 public:
  confirm_normal_exit() : handles_ptr(std::make_shared<handles>()) {
    // See temporary_stack.cc for why this isn't just a stateless lambda.
    struct _ {
      static DWORD WINAPI thread_func(void* p) {
        confirm_normal_exit* this_ = ((confirm_normal_exit*)p);
        // Make our local copy...
        std::shared_ptr<handles> local_handles_ptr(this_->handles_ptr);
        // ... and signal the main thread to continue.
        ::SetEvent(local_handles_ptr->hInitializationEvent);
        return confirm_normal_exit::ThreadFunc(std::move(local_handles_ptr));
      };
    };

    ::CreateThread(NULL, 0, &_::thread_func, this, 0, NULL);

    // Block until the other thread signals that it has a copy of `handles_ptr`.
    // After this, neither thread will have access to the other's data, except
    // for the shared immutable data in `handles_ptr`.
    ::WaitForSingleObject(handles_ptr->hInitializationEvent, INFINITE);
  }
  ~confirm_normal_exit() { ::SetEvent(handles_ptr->hExitEvent); }
};

int main(int, char**) {
  confirm_normal_exit _cne;

  test_small();
  test_large();
  test_too_large();

  if (failed) {
    ::printf("%s\n", "failed (see above)");
    return 1;
  }
  ::printf("%s\n", "all ok");
  return 0;
}
