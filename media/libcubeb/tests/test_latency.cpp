#ifdef NDEBUG
#undef NDEBUG
#endif
#include <stdlib.h>
#include "cubeb/cubeb.h"
#include <assert.h>
#include <stdio.h>
#ifdef CUBEB_GECKO_BUILD
#include "TestHarness.h"
#endif

#define LOG(msg) fprintf(stderr, "%s\n", msg);

int main(int argc, char * argv[])
{
#ifdef CUBEB_GECKO_BUILD
  ScopedXPCOM xpcom("test_latency");
#endif

  cubeb * ctx = NULL;
  int r;
  uint32_t max_channels;
  uint32_t preferred_rate;
  uint32_t latency_frames;

  LOG("latency_test start");
  r = cubeb_init(&ctx, "Cubeb audio test");
  assert(r == CUBEB_OK && "Cubeb init failed.");
  LOG("cubeb_init ok");

  r = cubeb_get_max_channel_count(ctx, &max_channels);
  assert(r == CUBEB_OK || r == CUBEB_ERROR_NOT_SUPPORTED);
  if (r == CUBEB_OK) {
    assert(max_channels > 0 && "Invalid max channel count.");
    LOG("cubeb_get_max_channel_count ok");
  }

  r = cubeb_get_preferred_sample_rate(ctx, &preferred_rate);
  assert(r == CUBEB_OK || r == CUBEB_ERROR_NOT_SUPPORTED);
  if (r == CUBEB_OK) {
    assert(preferred_rate > 0 && "Invalid preferred sample rate.");
    LOG("cubeb_get_preferred_sample_rate ok");
  }

  cubeb_stream_params params = {
    CUBEB_SAMPLE_FLOAT32NE,
    preferred_rate,
    max_channels
  };
  r = cubeb_get_min_latency(ctx, params, &latency_frames);
  assert(r == CUBEB_OK || r == CUBEB_ERROR_NOT_SUPPORTED);
  if (r == CUBEB_OK) {
    assert(latency_frames > 0 && "Invalid minimal latency.");
    LOG("cubeb_get_min_latency ok");
  }

  cubeb_destroy(ctx);
  LOG("cubeb_destroy ok");
  return EXIT_SUCCESS;
}
