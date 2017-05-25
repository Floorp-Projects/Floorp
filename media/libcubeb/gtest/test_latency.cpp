#include "gtest/gtest.h"
#include <stdlib.h>
#include <memory>
#include "cubeb/cubeb.h"
#include "common.h"

TEST(cubeb, latency)
{
  cubeb * ctx = NULL;
  int r;
  uint32_t max_channels;
  uint32_t preferred_rate;
  uint32_t latency_frames;

  r = common_init(&ctx, "Cubeb audio test");
  ASSERT_EQ(r, CUBEB_OK);

  std::unique_ptr<cubeb, decltype(&cubeb_destroy)>
    cleanup_cubeb_at_exit(ctx, cubeb_destroy);

  r = cubeb_get_max_channel_count(ctx, &max_channels);
  ASSERT_TRUE(r == CUBEB_OK || r == CUBEB_ERROR_NOT_SUPPORTED);
  if (r == CUBEB_OK) {
    ASSERT_GT(max_channels, 0u);
  }

  r = cubeb_get_preferred_sample_rate(ctx, &preferred_rate);
  ASSERT_TRUE(r == CUBEB_OK || r == CUBEB_ERROR_NOT_SUPPORTED);
  if (r == CUBEB_OK) {
    ASSERT_GT(preferred_rate, 0u);
  }

  cubeb_stream_params params = {
    CUBEB_SAMPLE_FLOAT32NE,
    preferred_rate,
    max_channels,
    CUBEB_LAYOUT_UNDEFINED
#if defined(__ANDROID__)
    , CUBEB_STREAM_TYPE_MUSIC
#endif
  };
  r = cubeb_get_min_latency(ctx, params, &latency_frames);
  ASSERT_TRUE(r == CUBEB_OK || r == CUBEB_ERROR_NOT_SUPPORTED);
  if (r == CUBEB_OK) {
    ASSERT_GT(latency_frames, 0u);
  }
}
