#ifdef NDEBUG
#undef NDEBUG
#endif
#include <stdlib.h>
#include <cubeb/cubeb.h>
#include <assert.h>
#include <stdio.h>

#define LOG(msg) fprintf(stderr, "%s\n", msg);

int main(int argc, char * argv[])
{
  cubeb * ctx = NULL;
  int rv;
  uint32_t max_channels;
  uint32_t preferred_rate;
  uint32_t latency_ms;

  LOG("latency_test start");
  rv = cubeb_init(&ctx, "Cubeb audio test");
  assert(rv == CUBEB_OK && "Cubeb init failed.");
  LOG("cubeb_init ok");

  rv = cubeb_get_max_channel_count(ctx, &max_channels);
  assert(rv == CUBEB_OK && "Could not query the max channe count.");
  assert(max_channels > 0 && "Invalid max channel count.");
  LOG("cubeb_get_max_channel_count ok");

  rv = cubeb_get_preferred_sample_rate(ctx, &preferred_rate);
  assert(rv == CUBEB_OK && "Could not query the preferred sample rate.");
  assert(preferred_rate && "Invalid preferred sample rate.");
  LOG("cubeb_get_preferred_sample_rate ok");

  cubeb_stream_params params = {
    CUBEB_SAMPLE_FLOAT32NE,
    preferred_rate,
    max_channels
  };
  rv = cubeb_get_min_latency(ctx, params, &latency_ms);
  assert(rv == CUBEB_OK && "Could not query the minimal latency.");
  assert(latency_ms && "Invalid minimal latency.");
  LOG("cubeb_get_min_latency ok");

  cubeb_destroy(ctx);
  LOG("cubeb_destroy ok");

  return EXIT_SUCCESS;
}
