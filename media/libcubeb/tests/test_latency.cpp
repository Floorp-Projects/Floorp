#ifdef NDEBUG
#undef NDEBUG
#endif
#include <stdlib.h>
#include <cubeb/cubeb.h>
#include <assert.h>

int main(int argc, char * argv[])
{
  cubeb * ctx = NULL;
  int rv;
  uint32_t max_channels;
  uint32_t preferred_rate;
  uint32_t latency_ms;

  rv = cubeb_init(&ctx, "Cubeb audio test");
  assert(rv == CUBEB_OK && "Cubeb init failed.");

  rv = cubeb_get_max_channel_count(ctx, &max_channels);
  assert(rv == CUBEB_OK && "Could not query the max channe count.");
  assert(max_channels > 0 && "Invalid max channel count.");

  rv = cubeb_get_preferred_sample_rate(ctx, &preferred_rate);
  assert(rv == CUBEB_OK && "Could not query the preferred sample rate.");
  assert(preferred_rate && "Invalid preferred sample rate.");

  cubeb_stream_params params = {
    CUBEB_SAMPLE_FLOAT32NE,
    preferred_rate,
    max_channels
  };
  rv = cubeb_get_min_latency(ctx, params, &latency_ms);
  assert(rv == CUBEB_OK && "Could not query the minimal latency.");
  assert(latency_ms && "Invalid minimal latency.");

  cubeb_destroy(ctx);

  return EXIT_SUCCESS;
}
