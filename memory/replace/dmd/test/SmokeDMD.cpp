/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// This program is used by the DMD xpcshell test. It is run under DMD and
// produces some output. The xpcshell test then post-processes and checks this
// output.
//
// Note that this file does not have "Test" or "test" in its name, because that
// will cause the build system to not record breakpad symbols for it, which
// will stop the post-processing (which includes stack fixing) from working
// correctly.

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "mozilla/Assertions.h"
#include "mozilla/JSONWriter.h"
#include "mozilla/UniquePtr.h"
#include "DMD.h"

using mozilla::JSONWriter;
using mozilla::MakeUnique;
using namespace mozilla::dmd;

DMDFuncs::Singleton DMDFuncs::sSingleton;

class FpWriteFunc : public mozilla::JSONWriteFunc
{
public:
  explicit FpWriteFunc(const char* aFilename)
  {
    mFp = fopen(aFilename, "w");
    if (!mFp) {
      fprintf(stderr, "SmokeDMD: can't create %s file: %s\n",
              aFilename, strerror(errno));
      exit(1);
    }
  }

  ~FpWriteFunc() { fclose(mFp); }

  void Write(const char* aStr) { fputs(aStr, mFp); }

private:
  FILE* mFp;
};

// This stops otherwise-unused variables from being optimized away.
static void
UseItOrLoseIt(void* aPtr, int aSeven)
{
  char buf[64];
  int n = sprintf(buf, "%p\n", aPtr);
  if (n == 20 + aSeven) {
    fprintf(stderr, "well, that is surprising");
  }
}

// This function checks that heap blocks that have the same stack trace but
// different (or no) reporters get aggregated separately.
void Foo(int aSeven)
{
  char* a[6];
  for (int i = 0; i < aSeven - 1; i++) {
    a[i] = (char*) malloc(128 - 16*i);
  }

  // Oddly, some versions of clang will cause identical stack traces to be
  // generated for adjacent calls to Report(), which breaks the test. Inserting
  // the UseItOrLoseIt() calls in between is enough to prevent this.

  Report(a[2]);                     // reported

  UseItOrLoseIt(a[2], aSeven);

  for (int i = 0; i < aSeven - 5; i++) {
    Report(a[i]);                   // reported
  }

  UseItOrLoseIt(a[2], aSeven);

  Report(a[3]);                     // reported

  // a[4], a[5] unreported
}

void
TestEmpty(const char* aTestName, const char* aMode)
{
  char filename[128];
  sprintf(filename, "complete-%s-%s.json", aTestName, aMode);
  auto f = MakeUnique<FpWriteFunc>(filename);

  char options[128];
  sprintf(options, "--mode=%s --stacks=full", aMode);
  ResetEverything(options);

  // Zero for everything.
  Analyze(Move(f));
}

void
TestFull(const char* aTestName, int aNum, const char* aMode, int aSeven)
{
  char filename[128];
  sprintf(filename, "complete-%s%d-%s.json", aTestName, aNum, aMode);
  auto f = MakeUnique<FpWriteFunc>(filename);

  // The --show-dump-stats=yes is there just to give that option some basic
  // testing, e.g. ensure it doesn't crash. It's hard to test much beyond that.
  char options[128];
  sprintf(options, "--mode=%s --stacks=full --show-dump-stats=yes", aMode);
  ResetEverything(options);

  // Analyze 1: 1 freed, 9 out of 10 unreported.
  // Analyze 2: still present and unreported.
  int i;
  char* a = nullptr;
  for (i = 0; i < aSeven + 3; i++) {
      a = (char*) malloc(100);
      UseItOrLoseIt(a, aSeven);
  }
  free(a);

  // A no-op.
  free(nullptr);

  // Note: 16 bytes is the smallest requested size that gives consistent
  // behaviour across all platforms with jemalloc.
  // Analyze 1: reported.
  // Analyze 2: thrice-reported.
  char* a2 = (char*) malloc(16);
  Report(a2);

  // Analyze 1: reported.
  // Analyze 2: reportedness carries over, due to ReportOnAlloc.
  char* b = (char*) malloc(10);
  ReportOnAlloc(b);

  // ReportOnAlloc, then freed.
  // Analyze 1: freed, irrelevant.
  // Analyze 2: freed, irrelevant.
  char* b2 = (char*) malloc(16);
  ReportOnAlloc(b2);
  free(b2);

  // Analyze 1: reported 4 times.
  // Analyze 2: freed, irrelevant.
  char* c = (char*) calloc(10, 3);
  Report(c);
  for (int i = 0; i < aSeven - 4; i++) {
    Report(c);
  }

  // Analyze 1: ignored.
  // Analyze 2: irrelevant.
  Report((void*)(intptr_t)i);

  // jemalloc rounds this up to 8192.
  // Analyze 1: reported.
  // Analyze 2: freed.
  char* e = (char*) malloc(4096);
  e = (char*) realloc(e, 7169);
  Report(e);

  // First realloc is like malloc;  second realloc is shrinking.
  // Analyze 1: reported.
  // Analyze 2: re-reported.
  char* e2 = (char*) realloc(nullptr, 1024);
  e2 = (char*) realloc(e2, 512);
  Report(e2);

  // First realloc is like malloc;  second realloc creates a min-sized block.
  // XXX: on Windows, second realloc frees the block.
  // Analyze 1: reported.
  // Analyze 2: freed, irrelevant.
  char* e3 = (char*) realloc(nullptr, 1023);
//e3 = (char*) realloc(e3, 0);
  MOZ_ASSERT(e3);
  Report(e3);

  // Analyze 1: freed, irrelevant.
  // Analyze 2: freed, irrelevant.
  char* f1 = (char*) malloc(64);
  free(f1);

  // Analyze 1: ignored.
  // Analyze 2: irrelevant.
  Report((void*)(intptr_t)0x0);

  // Analyze 1: mixture of reported and unreported.
  // Analyze 2: all unreported.
  Foo(aSeven);

  // Analyze 1: twice-reported.
  // Analyze 2: twice-reported.
  char* g1 = (char*) malloc(77);
  ReportOnAlloc(g1);
  ReportOnAlloc(g1);

  // Analyze 1: mixture of reported and unreported.
  // Analyze 2: all unreported.
  // Nb: this Foo() call is deliberately not adjacent to the previous one. See
  // the comment about adjacent calls in Foo() for more details.
  Foo(aSeven);

  // Analyze 1: twice-reported.
  // Analyze 2: once-reported.
  char* g2 = (char*) malloc(78);
  Report(g2);
  ReportOnAlloc(g2);

  // Analyze 1: twice-reported.
  // Analyze 2: once-reported.
  char* g3 = (char*) malloc(79);
  ReportOnAlloc(g3);
  Report(g3);

  // All the odd-ball ones.
  // Analyze 1: all unreported.
  // Analyze 2: all freed, irrelevant.
  // XXX: no memalign on Mac
//void* w = memalign(64, 65);           // rounds up to 128
//UseItOrLoseIt(w, aSeven);

  // XXX: posix_memalign doesn't work on B2G
//void* x;
//posix_memalign(&y, 128, 129);         // rounds up to 256
//UseItOrLoseIt(x, aSeven);

  // XXX: valloc doesn't work on Windows.
//void* y = valloc(1);                  // rounds up to 4096
//UseItOrLoseIt(y, aSeven);

  // XXX: C11 only
//void* z = aligned_alloc(64, 256);
//UseItOrLoseIt(z, aSeven);

  if (aNum == 1) {
    // Analyze 1.
    Analyze(Move(f));
  }

  ClearReports();

  //---------

  Report(a2);
  Report(a2);
  free(c);
  free(e);
  Report(e2);
  free(e3);
//free(w);
//free(x);
//free(y);
//free(z);

  // Do some allocations that will only show up in cumulative mode.
  for (int i = 0; i < 100; i++) {
    free(malloc(128));
  }

  if (aNum == 2) {
    // Analyze 2.
    Analyze(Move(f));
  }
}

void
TestPartial(const char* aTestName, const char* aMode, int aSeven)
{
  char filename[128];
  sprintf(filename, "complete-%s-%s.json", aTestName, aMode);
  auto f = MakeUnique<FpWriteFunc>(filename);

  char options[128];
  sprintf(options, "--mode=%s", aMode);
  ResetEverything(options);

  int kTenThousand = aSeven + 9993;
  char* s;

  // The output of this function is deterministic but it relies on the
  // probability and seeds given to the FastBernoulliTrial instance in
  // ResetBernoulli(). If they change, the output will change too.

  // Expected fraction with stacks: (1 - (1 - 0.003) ** 16) = 0.0469.
  // So we expect about 0.0469 * 10000 == 469.
  // We actually get 511.
  for (int i = 0; i < kTenThousand; i++) {
    s = (char*) malloc(16);
    UseItOrLoseIt(s, aSeven);
  }

  // Expected fraction with stacks: (1 - (1 - 0.003) ** 128) = 0.3193.
  // So we expect about 0.3193 * 10000 == 3193.
  // We actually get 3136.
  for (int i = 0; i < kTenThousand; i++) {
    s = (char*) malloc(128);
    UseItOrLoseIt(s, aSeven);
  }

  // Expected fraction with stacks: (1 - (1 - 0.003) ** 1024) = 0.9539.
  // So we expect about 0.9539 * 10000 == 9539.
  // We actually get 9531.
  for (int i = 0; i < kTenThousand; i++) {
    s = (char*) malloc(1024);
    UseItOrLoseIt(s, aSeven);
  }

  Analyze(Move(f));
}

void
TestScan(int aSeven)
{
  auto f = MakeUnique<FpWriteFunc>("basic-scan.json");

  ResetEverything("--mode=scan");

  uintptr_t* p = (uintptr_t*) malloc(6 * sizeof(uintptr_t*));
  UseItOrLoseIt(p, aSeven);

  // Hard-coded values checked by scan-test.py
  p[0] = 0x123; // outside a block, small value
  p[1] = 0x0; // null
  p[2] = (uintptr_t)((uint8_t*)p - 1); // pointer outside a block, but nearby
  p[3] = (uintptr_t)p; // pointer to start of a block
  p[4] = (uintptr_t)((uint8_t*)p + 1); // pointer into a block
  p[5] = 0x0; // trailing null

  Analyze(Move(f));
}

void
RunTests()
{
  // This test relies on the compiler not doing various optimizations, such as
  // eliding unused malloc() calls or unrolling loops with fixed iteration
  // counts. So we compile it with -O0 (or equivalent), which probably prevents
  // that. We also use the following variable for various loop iteration
  // counts, just in case compilers might unroll very small loops even with
  // -O0.
  int seven = 7;

  // Make sure that DMD is actually running; it is initialized on the first
  // allocation.
  int *x = (int*)malloc(100);
  UseItOrLoseIt(x, seven);
  MOZ_RELEASE_ASSERT(IsRunning());

  // Please keep this in sync with run_test in test_dmd.js.

  TestEmpty("empty", "live");
  TestEmpty("empty", "dark-matter");
  TestEmpty("empty", "cumulative");

  TestFull("full", 1, "live",        seven);
  TestFull("full", 1, "dark-matter", seven);

  TestFull("full", 2, "dark-matter", seven);
  TestFull("full", 2, "cumulative",  seven);

  TestPartial("partial", "live", seven);

  TestScan(seven);
}

int main()
{
  RunTests();

  return 0;
}
