/* vim: se cin sw=2 ts=2 et : */
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WIDGET_GTK_GFXINFO_UTILS_h__
#define WIDGET_GTK_GFXINFO_UTILS_h__

// An alternative to mozilla::Unused for use in (a) C code and (b) code where
// linking with unused.o is difficult.
#define MOZ_UNUSED(expr) \
  do {                   \
    if (expr) {          \
      (void)0;           \
    }                    \
  } while (0)

#define LOG_PIPE 2

static bool enable_logging = false;
static void log(const char* format, ...) {
  if (!enable_logging) {
    return;
  }
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
}

static int output_pipe = 1;
static void close_logging() {
  // we want to redirect to /dev/null stdout, stderr, and while we're at it,
  // any PR logging file descriptors. To that effect, we redirect all positive
  // file descriptors up to what open() returns here. In particular, 1 is stdout
  // and 2 is stderr.
  int fd = open("/dev/null", O_WRONLY);
  for (int i = 1; i < fd; i++) {
    if (output_pipe != i) {
      dup2(fd, i);
    }
  }
  close(fd);
}

// C++ standard collides with C standard in that it doesn't allow casting void*
// to function pointer types. So the work-around is to convert first to size_t.
// http://www.trilithium.com/johan/2004/12/problem-with-dlsym/
template <typename func_ptr_type>
static func_ptr_type cast(void* ptr) {
  return reinterpret_cast<func_ptr_type>(reinterpret_cast<size_t>(ptr));
}

#define BUFFER_SIZE_STEP 4000

static char* test_buf = nullptr;
static int test_bufsize = 0;
static int test_length = 0;

static void record_value(const char* format, ...) {
  if (!test_buf || test_length + BUFFER_SIZE_STEP / 2 > test_bufsize) {
    test_bufsize += BUFFER_SIZE_STEP;
    test_buf = (char*)realloc(test_buf, test_bufsize);
  }
  int remaining = test_bufsize - test_length;

  // Append the new values to the buffer, not to exceed the remaining space.
  va_list args;
  va_start(args, format);
  int max_added = vsnprintf(test_buf + test_length, remaining, format, args);
  va_end(args);

  if (max_added >= remaining) {
    test_length += remaining;
  } else {
    test_length += max_added;
  }
}

#define record_error(str_, ...) record_value("ERROR\n" str_ "\n", ##__VA_ARGS__)
#define record_warning(str_, ...) \
  record_value("WARNING\n" str_ "\n", ##__VA_ARGS__)

static void record_flush() {
  if (!test_buf) {
    return;
  }
  MOZ_UNUSED(write(output_pipe, test_buf, test_length));
  if (enable_logging) {
    MOZ_UNUSED(write(LOG_PIPE, test_buf, test_length));
  }
  free(test_buf);
  test_buf = nullptr;
}

#endif /* WIDGET_GTK_GFXINFO_h__ */
