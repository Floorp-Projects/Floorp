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
  sprintf(filename, "full-%s-%s.json", aTestName, aMode);
  auto f = MakeUnique<FpWriteFunc>(filename);

  char options[128];
  sprintf(options, "--mode=%s --sample-below=1", aMode);
  ResetEverything(options);

  // Zero for everything.
  Analyze(Move(f));
}

void
TestUnsampled(const char* aTestName, int aNum, const char* aMode, int aSeven)
{
  char filename[128];
  sprintf(filename, "full-%s%d-%s.json", aTestName, aNum, aMode);
  auto f = MakeUnique<FpWriteFunc>(filename);

  // The --show-dump-stats=yes is there just to give that option some basic
  // testing, e.g. ensure it doesn't crash. It's hard to test much beyond that.
  char options[128];
  sprintf(options, "--mode=%s --sample-below=1 --show-dump-stats=yes", aMode);
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

  // Note: 8 bytes is the smallest requested size that gives consistent
  // behaviour across all platforms with jemalloc.
  // Analyze 1: reported.
  // Analyze 2: thrice-reported.
  char* a2 = (char*) malloc(8);
  Report(a2);

  // Analyze 1: reported.
  // Analyze 2: reportedness carries over, due to ReportOnAlloc.
  char* b = (char*) malloc(10);
  ReportOnAlloc(b);

  // ReportOnAlloc, then freed.
  // Analyze 1: freed, irrelevant.
  // Analyze 2: freed, irrelevant.
  char* b2 = (char*) malloc(8);
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
TestSampled(const char* aTestName, const char* aMode, int aSeven)
{
  char filename[128];
  sprintf(filename, "full-%s-%s.json", aTestName, aMode);
  auto f = MakeUnique<FpWriteFunc>(filename);

  char options[128];
  sprintf(options, "--mode=%s --sample-below=128", aMode);
  ResetEverything(options);

  char* s;

  // This equals the sample size, and so is reported exactly.  It should be
  // listed before records of the same size that are sampled.
  s = (char*) malloc(128);
  UseItOrLoseIt(s, aSeven);

  // This exceeds the sample size, and so is reported exactly.
  s = (char*) malloc(160);
  UseItOrLoseIt(s, aSeven);

  // These together constitute exactly one sample.
  for (int i = 0; i < aSeven + 9; i++) {
    s = (char*) malloc(8);
    UseItOrLoseIt(s, aSeven);
  }

  // These fall 8 bytes short of a full sample.
  for (int i = 0; i < aSeven + 8; i++) {
    s = (char*) malloc(8);
    UseItOrLoseIt(s, aSeven);
  }

  // This exceeds the sample size, and so is recorded exactly.
  s = (char*) malloc(256);
  UseItOrLoseIt(s, aSeven);

  // This gets more than to a full sample from the |i < aSeven + 8| loop above.
  s = (char*) malloc(96);
  UseItOrLoseIt(s, aSeven);

  // This gets to another full sample.
  for (int i = 0; i < aSeven - 2; i++) {
    s = (char*) malloc(8);
    UseItOrLoseIt(s, aSeven);
  }

  // This allocates 16, 32, ..., 128 bytes, which results in a heap block
  // record that contains a mix of sample and non-sampled blocks, and so should
  // be printed with '~' signs.
  for (int i = 1; i <= aSeven + 1; i++) {
    s = (char*) malloc(i * 16);
    UseItOrLoseIt(s, aSeven);
  }

  // At the end we're 64 bytes into the current sample so we report ~1,424
  // bytes of allocation overall, which is 64 less than the real value 1,488.

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

  TestUnsampled("unsampled", 1, "live",        seven);
  TestUnsampled("unsampled", 1, "dark-matter", seven);

  TestUnsampled("unsampled", 2, "dark-matter", seven);
  TestUnsampled("unsampled", 2, "cumulative",  seven);

  TestSampled("sampled", "live", seven);
}

int main()
{
  RunTests();

  return 0;
}
