/*
 * Copyright © 2013 Sebastien Alaiwan
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */
#if !defined(TEST_COMMON)
#define TEST_COMMON

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <objbase.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "cubeb/cubeb.h"
#include "cubeb_mixer.h"
#include "gtest/gtest.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>

template <typename T, size_t N>
constexpr size_t
ARRAY_LENGTH(T (&)[N])
{
  return N;
}

inline void
delay(unsigned int ms)
{
#if defined(_WIN32)
  Sleep(ms);
#else
  sleep(ms / 1000);
  usleep(ms % 1000 * 1000);
#endif
}

#if !defined(M_PI)
#define M_PI 3.14159265358979323846
#endif

typedef struct {
  char const * name;
  unsigned int const channels;
  uint32_t const layout;
} layout_info;

struct backend_caps {
  const char * id;
  const int input_capabilities;
};

// This static table allows knowing if a backend has audio input capabilities.
// We don't rely on opening a stream and checking if it works, because this
// would make the test skip the tests that make use of audio input, if a
// particular backend has a bug that causes a failure during audio input stream
// creation
static backend_caps backend_capabilities[] = {
    {"sun", 1},        {"wasapi", 1}, {"kai", 1},    {"audiounit", 1},
    {"audiotrack", 0}, {"opensl", 1}, {"aaudio", 1}, {"jack", 1},
    {"pulse", 1},      {"sndio", 1},  {"oss", 1},    {"winmm", 0},
    {"alsa", 1},
};

inline int
can_run_audio_input_test(cubeb * ctx)
{
  cubeb_device_collection devices;
  int input_device_available = 0;
  int r;
  /* Bail out early if the host does not have input devices. */
  r = cubeb_enumerate_devices(ctx, CUBEB_DEVICE_TYPE_INPUT, &devices);
  if (r != CUBEB_OK) {
    fprintf(stderr, "error enumerating devices.");
    return 0;
  }

  if (devices.count == 0) {
    fprintf(stderr, "no input device available, skipping test.\n");
    cubeb_device_collection_destroy(ctx, &devices);
    return 0;
  }

  for (uint32_t i = 0; i < devices.count; i++) {
    input_device_available |=
        (devices.device[i].state == CUBEB_DEVICE_STATE_ENABLED);
  }

  if (!input_device_available) {
    fprintf(stderr, "there are input devices, but they are not "
                    "available, skipping\n");
  }

  cubeb_device_collection_destroy(ctx, &devices);

  int backend_has_input_capabilities;
  const char * backend_id = cubeb_get_backend_id(ctx);
  for (uint32_t i = 0; i < sizeof(backend_capabilities) / sizeof(backend_caps);
       i++) {
    if (strcmp(backend_capabilities[i].id, backend_id) == 0) {
      backend_has_input_capabilities =
          backend_capabilities[i].input_capabilities;
    }
  }

  return !!input_device_available && !!backend_has_input_capabilities;
}

inline void
print_log(const char * msg, ...)
{
  va_list args;
  va_start(args, msg);
  vprintf(msg, args);
  va_end(args);
}

/** Initialize cubeb with backend override.
 *  Create call cubeb_init passing value for CUBEB_BACKEND env var as
 *  override. */
inline int
common_init(cubeb ** ctx, char const * ctx_name)
{
#ifdef ENABLE_NORMAL_LOG
  if (cubeb_set_log_callback(CUBEB_LOG_NORMAL, print_log) != CUBEB_OK) {
    fprintf(stderr, "Set normal log callback failed\n");
  }
#endif

#ifdef ENABLE_VERBOSE_LOG
  if (cubeb_set_log_callback(CUBEB_LOG_VERBOSE, print_log) != CUBEB_OK) {
    fprintf(stderr, "Set verbose log callback failed\n");
  }
#endif

  int r;
  char const * backend;
  char const * ctx_backend;

  backend = getenv("CUBEB_BACKEND");
  r = cubeb_init(ctx, ctx_name, backend);
  if (r == CUBEB_OK && backend) {
    ctx_backend = cubeb_get_backend_id(*ctx);
    if (strcmp(backend, ctx_backend) != 0) {
      fprintf(stderr, "Requested backend `%s', got `%s'\n", backend,
              ctx_backend);
    }
  }

  return r;
}

#if defined(_WIN32)
class TestEnvironment : public ::testing::Environment {
public:
  void SetUp() override { hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED); }

  void TearDown() override
  {
    if (SUCCEEDED(hr)) {
      CoUninitialize();
    }
  }

private:
  HRESULT hr;
};

::testing::Environment * const foo_env =
    ::testing::AddGlobalTestEnvironment(new TestEnvironment);
#endif

#endif /* TEST_COMMON */
