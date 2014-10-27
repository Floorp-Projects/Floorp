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
RunTests()
{
  // These files are written to $CWD.
  auto f1 = MakeUnique<FpWriteFunc>("full-empty.json");
  auto f2 = MakeUnique<FpWriteFunc>("full-unsampled1.json");
  auto f3 = MakeUnique<FpWriteFunc>("full-unsampled2.json");
  auto f4 = MakeUnique<FpWriteFunc>("full-sampled.json");

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

  // The first part of this test requires sampling to be disabled.
  SetSampleBelowSize(1);

  // The file manipulations above may have done some heap allocations.
  // Clear all knowledge of existing blocks to give us a clean slate.
  ClearBlocks();

  //---------

  // AnalyzeReports 1.  Zero for everything.
  JSONWriter writer1(Move(f1));
  AnalyzeReports(writer1);

  //---------

  // AnalyzeReports 2: 1 freed, 9 out of 10 unreported.
  // AnalyzeReports 3: still present and unreported.
  int i;
  char* a = nullptr;
  for (i = 0; i < seven + 3; i++) {
      a = (char*) malloc(100);
      UseItOrLoseIt(a, seven);
  }
  free(a);

  // Note: 8 bytes is the smallest requested size that gives consistent
  // behaviour across all platforms with jemalloc.
  // AnalyzeReports 2: reported.
  // AnalyzeReports 3: thrice-reported.
  char* a2 = (char*) malloc(8);
  Report(a2);

  // AnalyzeReports 2: reported.
  // AnalyzeReports 3: reportedness carries over, due to ReportOnAlloc.
  char* b = (char*) malloc(10);
  ReportOnAlloc(b);

  // ReportOnAlloc, then freed.
  // AnalyzeReports 2: freed, irrelevant.
  // AnalyzeReports 3: freed, irrelevant.
  char* b2 = (char*) malloc(1);
  ReportOnAlloc(b2);
  free(b2);

  // AnalyzeReports 2: reported 4 times.
  // AnalyzeReports 3: freed, irrelevant.
  char* c = (char*) calloc(10, 3);
  Report(c);
  for (int i = 0; i < seven - 4; i++) {
    Report(c);
  }

  // AnalyzeReports 2: ignored.
  // AnalyzeReports 3: irrelevant.
  Report((void*)(intptr_t)i);

  // jemalloc rounds this up to 8192.
  // AnalyzeReports 2: reported.
  // AnalyzeReports 3: freed.
  char* e = (char*) malloc(4096);
  e = (char*) realloc(e, 4097);
  Report(e);

  // First realloc is like malloc;  second realloc is shrinking.
  // AnalyzeReports 2: reported.
  // AnalyzeReports 3: re-reported.
  char* e2 = (char*) realloc(nullptr, 1024);
  e2 = (char*) realloc(e2, 512);
  Report(e2);

  // First realloc is like malloc;  second realloc creates a min-sized block.
  // XXX: on Windows, second realloc frees the block.
  // AnalyzeReports 2: reported.
  // AnalyzeReports 3: freed, irrelevant.
  char* e3 = (char*) realloc(nullptr, 1023);
//e3 = (char*) realloc(e3, 0);
  MOZ_ASSERT(e3);
  Report(e3);

  // AnalyzeReports 2: freed, irrelevant.
  // AnalyzeReports 3: freed, irrelevant.
  char* f = (char*) malloc(64);
  free(f);

  // AnalyzeReports 2: ignored.
  // AnalyzeReports 3: irrelevant.
  Report((void*)(intptr_t)0x0);

  // AnalyzeReports 2: mixture of reported and unreported.
  // AnalyzeReports 3: all unreported.
  Foo(seven);

  // AnalyzeReports 2: twice-reported.
  // AnalyzeReports 3: twice-reported.
  char* g1 = (char*) malloc(77);
  ReportOnAlloc(g1);
  ReportOnAlloc(g1);

  // AnalyzeReports 2: mixture of reported and unreported.
  // AnalyzeReports 3: all unreported.
  // Nb: this Foo() call is deliberately not adjacent to the previous one. See
  // the comment about adjacent calls in Foo() for more details.
  Foo(seven);

  // AnalyzeReports 2: twice-reported.
  // AnalyzeReports 3: once-reported.
  char* g2 = (char*) malloc(78);
  Report(g2);
  ReportOnAlloc(g2);

  // AnalyzeReports 2: twice-reported.
  // AnalyzeReports 3: once-reported.
  char* g3 = (char*) malloc(79);
  ReportOnAlloc(g3);
  Report(g3);

  // All the odd-ball ones.
  // AnalyzeReports 2: all unreported.
  // AnalyzeReports 3: all freed, irrelevant.
  // XXX: no memalign on Mac
//void* w = memalign(64, 65);           // rounds up to 128
//UseItOrLoseIt(w, seven);

  // XXX: posix_memalign doesn't work on B2G
//void* x;
//posix_memalign(&y, 128, 129);         // rounds up to 256
//UseItOrLoseIt(x, seven);

  // XXX: valloc doesn't work on Windows.
//void* y = valloc(1);                  // rounds up to 4096
//UseItOrLoseIt(y, seven);

  // XXX: C11 only
//void* z = aligned_alloc(64, 256);
//UseItOrLoseIt(z, seven);

  // AnalyzeReports 2.
  JSONWriter writer2(Move(f2));
  AnalyzeReports(writer2);

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

  // AnalyzeReports 3.
  JSONWriter writer3(Move(f3));
  AnalyzeReports(writer3);

  //---------

  // The first part of this test requires sampling to be disabled.
  SetSampleBelowSize(128);

  // Clear all knowledge of existing blocks to give us a clean slate.
  ClearBlocks();

  char* s;

  // This equals the sample size, and so is reported exactly.  It should be
  // listed before records of the same size that are sampled.
  s = (char*) malloc(128);
  UseItOrLoseIt(s, seven);

  // This exceeds the sample size, and so is reported exactly.
  s = (char*) malloc(144);
  UseItOrLoseIt(s, seven);

  // These together constitute exactly one sample.
  for (int i = 0; i < seven + 9; i++) {
    s = (char*) malloc(8);
    UseItOrLoseIt(s, seven);
  }

  // These fall 8 bytes short of a full sample.
  for (int i = 0; i < seven + 8; i++) {
    s = (char*) malloc(8);
    UseItOrLoseIt(s, seven);
  }

  // This exceeds the sample size, and so is recorded exactly.
  s = (char*) malloc(256);
  UseItOrLoseIt(s, seven);

  // This gets more than to a full sample from the |i < seven + 8| loop above.
  s = (char*) malloc(96);
  UseItOrLoseIt(s, seven);

  // This gets to another full sample.
  for (int i = 0; i < seven - 2; i++) {
    s = (char*) malloc(8);
    UseItOrLoseIt(s, seven);
  }

  // This allocates 16, 32, ..., 128 bytes, which results in a heap block
  // record that contains a mix of sample and non-sampled blocks, and so should
  // be printed with '~' signs.
  for (int i = 1; i <= seven + 1; i++) {
    s = (char*) malloc(i * 16);
    UseItOrLoseIt(s, seven);
  }

  // At the end we're 64 bytes into the current sample so we report ~1,424
  // bytes of allocation overall, which is 64 less than the real value 1,488.

  // AnalyzeReports 4.
  JSONWriter writer4(Move(f4));
  AnalyzeReports(writer4);
}

int main()
{
  RunTests();

  return 0;
}
