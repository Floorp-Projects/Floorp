/*
 * Copyright © 2013 Sebastien Alaiwan
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */
#if !defined(TEST_COMMON)
#define TEST_COMMON

#if defined( _WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <cstdarg>
#include "cubeb/cubeb.h"
#include "cubeb_mixer.h"

template<typename T, size_t N>
constexpr size_t
ARRAY_LENGTH(T(&)[N])
{
  return N;
}

void delay(unsigned int ms)
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
  cubeb_channel_layout const layout;
} layout_info;

layout_info const layout_infos[CUBEB_LAYOUT_MAX] = {
  { "undefined",      0,  CUBEB_LAYOUT_UNDEFINED },
  { "dual mono",      2,  CUBEB_LAYOUT_DUAL_MONO },
  { "dual mono lfe",  3,  CUBEB_LAYOUT_DUAL_MONO_LFE },
  { "mono",           1,  CUBEB_LAYOUT_MONO },
  { "mono lfe",       2,  CUBEB_LAYOUT_MONO_LFE },
  { "stereo",         2,  CUBEB_LAYOUT_STEREO },
  { "stereo lfe",     3,  CUBEB_LAYOUT_STEREO_LFE },
  { "3f",             3,  CUBEB_LAYOUT_3F },
  { "3f lfe",         4,  CUBEB_LAYOUT_3F_LFE },
  { "2f1",            3,  CUBEB_LAYOUT_2F1 },
  { "2f1 lfe",        4,  CUBEB_LAYOUT_2F1_LFE },
  { "3f1",            4,  CUBEB_LAYOUT_3F1 },
  { "3f1 lfe",        5,  CUBEB_LAYOUT_3F1_LFE },
  { "2f2",            4,  CUBEB_LAYOUT_2F2 },
  { "2f2 lfe",        5,  CUBEB_LAYOUT_2F2_LFE },
  { "3f2",            5,  CUBEB_LAYOUT_3F2 },
  { "3f2 lfe",        6,  CUBEB_LAYOUT_3F2_LFE },
  { "3f3r lfe",       7,  CUBEB_LAYOUT_3F3R_LFE },
  { "3f4 lfe",        8,  CUBEB_LAYOUT_3F4_LFE }
};

int has_available_input_device(cubeb * ctx)
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
    input_device_available |= (devices.device[i].state ==
                               CUBEB_DEVICE_STATE_ENABLED);
  }

  if (!input_device_available) {
    fprintf(stderr, "there are input devices, but they are not "
        "available, skipping\n");
  }

  cubeb_device_collection_destroy(ctx, &devices);
  return !!input_device_available;
}

void print_log(const char * msg, ...)
{
  va_list args;
  va_start(args, msg);
  vprintf(msg, args);
  va_end(args);
}

/** Initialize cubeb with backend override.
 *  Create call cubeb_init passing value for CUBEB_BACKEND env var as
 *  override. */
int common_init(cubeb ** ctx, char const * ctx_name)
{
  int r;
  char const * backend;
  char const * ctx_backend;

  backend = getenv("CUBEB_BACKEND");
  r = cubeb_init(ctx, ctx_name, backend);
  if (r == CUBEB_OK && backend) {
    ctx_backend = cubeb_get_backend_id(*ctx);
    if (strcmp(backend, ctx_backend) != 0) {
      fprintf(stderr, "Requested backend `%s', got `%s'\n",
              backend, ctx_backend);
    }
  }

  return r;
}

#endif /* TEST_COMMON */
