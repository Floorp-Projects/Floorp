// Copyright (c) 2006, Google Inc.
// All rights reserved.
//
// Author: Li Liu
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <pthread.h>
#include <unistd.h>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "client/linux/handler/minidump_generator.h"

using namespace google_breakpad;

// Thread use this to see if it should stop working.
static bool should_exit = false;

static void foo2(int arg) {
  // Stack variable, used for debugging stack dumps.
  int c = arg;
  c = 0xcccccccc;
  while (!should_exit)
    sleep(1);
}

static void foo(int arg) {
  // Stack variable, used for debugging stack dumps.
  int b = arg;
  b = 0xbbbbbbbb;
  foo2(b);
}

static void *thread_main(void *) {
  // Stack variable, used for debugging stack dumps.
  int a = 0xaaaaaaaa;
  foo(a);
  return NULL;
}

static void CreateThread(int num) {
  pthread_t h;
  for (int i = 0; i < num; ++i) {
    pthread_create(&h, NULL, thread_main, NULL);
    pthread_detach(h);
  }
}

int main(int argc, char *argv[]) {
  CreateThread(10);
  google_breakpad::MinidumpGenerator mg;
  if (mg.WriteMinidumpToFile("minidump_test.out", -1, NULL))
    printf("Succeeded written minidump\n");
  else
    printf("Failed to write minidump\n");
  should_exit = true;
  return 0;
}
