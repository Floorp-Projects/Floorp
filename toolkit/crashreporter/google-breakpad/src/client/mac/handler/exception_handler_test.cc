// Copyright (c) 2006, Google Inc.
// All rights reserved.
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

/*
g++ -framework CoreFoundation -I../../.. \
	../../minidump_file_writer.cc \
	../../../common/convert_UTF.c \
	../../../common/string_conversion.cc \
	../../../common/mac/string_utilities.cc \
	exception_handler.cc \
	minidump_generator.cc \
	exception_handler_test.cc \
	-o exception_handler_test
*/

#include <pthread.h>
#include <pwd.h>
#include <unistd.h>

#include <CoreFoundation/CoreFoundation.h>

#include "exception_handler.h"
#include "minidump_generator.h"

using std::string;
using google_breakpad::ExceptionHandler;

static void *SleepyFunction(void *) {
  while (1) {
    sleep(10000);
  }
}

static void Crasher() {
  int *a = NULL;

	fprintf(stdout, "Going to crash...\n");
  fprintf(stdout, "A = %d", *a);
}

static void SoonToCrash() {
  Crasher();
}

bool MDCallback(const char *dump_dir, const char *file_name,
                void *context, bool success) {
  string path(dump_dir);
  string dest(dump_dir);
  path.append(file_name);
  path.append(".dmp");

  fprintf(stdout, "Minidump: %s\n", path.c_str());
  // Indicate that we've handled the callback
  return true;
}

int main(int argc, char * const argv[]) {
  char buffer[PATH_MAX];
  struct passwd *user = getpwuid(getuid());

  // Home dir
  snprintf(buffer, sizeof(buffer), "/Users/%s/Desktop/", user->pw_name);

  string path(buffer);
  ExceptionHandler eh(path, NULL, MDCallback, NULL, true);
  pthread_t t;

  if (pthread_create(&t, NULL, SleepyFunction, NULL) == 0) {
    pthread_detach(t);
  } else {
    perror("pthread_create");
  }

  // Dump a test
  eh.WriteMinidump();

	// Test the handler
  SoonToCrash();

  return 0;
}
