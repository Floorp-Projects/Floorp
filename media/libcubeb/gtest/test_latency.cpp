#include "gtest/gtest.h"
#include <stdlib.h>
#include <memory>
#include "cubeb/cubeb.h"

TEST(cubeb, latency)
{
  cubeb * ctx = NULL;
  int r;
  uint32_t max_channels;
  uint32_t preferred_rate;
  uint32_t latency_frames;
  cubeb_channel_layout layout;

  r = cubeb_init(&ctx, "Cubeb audio test", NULL);
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

  r = cubeb_get_preferred_channel_layout(ctx, &layout);
  ASSERT_TRUE(r == CUBEB_OK || r == CUBEB_ERROR_NOT_SUPPORTED ||
              (r == CUBEB_ERROR && layout == CUBEB_LAYOUT_UNDEFINED));

  cubeb_stream_params params = {
    CUBEB_SAMPLE_FLOAT32NE,
    preferred_rate,
    max_channels,
    (r == CUBEB_OK) ? layout : CUBEB_LAYOUT_UNDEFINED
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
