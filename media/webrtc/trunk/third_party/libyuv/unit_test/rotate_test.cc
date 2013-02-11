/*
 *  Copyright 2011 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdlib.h>
#include <time.h>

#include "libyuv/rotate.h"
#include "../unit_test/unit_test.h"

namespace libyuv {

void PrintArray(uint8 *array, int w, int h) {
  for (int i = 0; i < h; ++i) {
    for (int j = 0; j < w; ++j) {
      printf("%4d", (signed char)array[i * w + j]);
    }
    printf("\n");
  }
}

TEST_F(libyuvTest, Transpose) {
  int iw, ih, ow, oh;
  int err = 0;

  for (iw = 8; iw < rotate_max_w_ && !err; ++iw) {
    for (ih = 8; ih < rotate_max_h_ && !err; ++ih) {
      int i;
      ow = ih;
      oh = iw;

      align_buffer_16(input, iw * ih)
      align_buffer_16(output_1, ow * oh)
      align_buffer_16(output_2, iw * ih)

      for (i = 0; i < iw * ih; ++i) {
        input[i] = i;
      }

      TransposePlane(input,    iw, output_1, ow, iw, ih);
      TransposePlane(output_1, ow, output_2, oh, ow, oh);

      for (i = 0; i < iw * ih; ++i) {
        if (input[i] != output_2[i]) {
          err++;
        }
      }

      if (err) {
        printf("input %dx%d \n", iw, ih);
        PrintArray(input, iw, ih);

        printf("transpose 1\n");
        PrintArray(output_1, ow, oh);

        printf("transpose 2\n");
        PrintArray(output_2, iw, ih);
      }

      free_aligned_buffer_16(input)
      free_aligned_buffer_16(output_1)
      free_aligned_buffer_16(output_2)
    }
  }

  EXPECT_EQ(0, err);
}

TEST_F(libyuvTest, TransposeUV) {
  int iw, ih, ow, oh;
  int err = 0;

  for (iw = 16; iw < rotate_max_w_ && !err; iw += 2) {
    for (ih = 8; ih < rotate_max_h_ && !err; ++ih) {
      int i;

      ow = ih;
      oh = iw >> 1;

      align_buffer_16(input, iw * ih)
      align_buffer_16(output_a1, ow * oh)
      align_buffer_16(output_b1, ow * oh)
      align_buffer_16(output_a2, iw * ih)
      align_buffer_16(output_b2, iw * ih)

      for (i = 0; i < iw * ih; i += 2) {
        input[i] = i >> 1;
        input[i + 1] = -(i >> 1);
      }

      TransposeUV(input, iw, output_a1, ow, output_b1, ow, iw >> 1, ih);

      TransposePlane(output_a1, ow, output_a2, oh, ow, oh);
      TransposePlane(output_b1, ow, output_b2, oh, ow, oh);

      for (i = 0; i < iw * ih; i += 2) {
        if (input[i] != output_a2[i >> 1]) {
          err++;
        }
        if (input[i + 1] != output_b2[i >> 1]) {
          err++;
        }
      }

      if (err) {
        printf("input %dx%d \n", iw, ih);
        PrintArray(input, iw, ih);

        printf("transpose 1\n");
        PrintArray(output_a1, ow, oh);
        PrintArray(output_b1, ow, oh);

        printf("transpose 2\n");
        PrintArray(output_a2, oh, ow);
        PrintArray(output_b2, oh, ow);
      }

      free_aligned_buffer_16(input)
      free_aligned_buffer_16(output_a1)
      free_aligned_buffer_16(output_b1)
      free_aligned_buffer_16(output_a2)
      free_aligned_buffer_16(output_b2)
    }
  }

  EXPECT_EQ(0, err);
}

TEST_F(libyuvTest, RotatePlane90) {
  int iw, ih, ow, oh;
  int err = 0;

  for (iw = 8; iw < rotate_max_w_ && !err; ++iw) {
    for (ih = 8; ih < rotate_max_h_ && !err; ++ih) {
      int i;

      ow = ih;
      oh = iw;

      align_buffer_16(input, iw * ih)
      align_buffer_16(output_0, iw * ih)
      align_buffer_16(output_90, ow * oh)
      align_buffer_16(output_180, iw * ih)
      align_buffer_16(output_270, ow * oh)

      for (i = 0; i < iw * ih; ++i) {
        input[i] = i;
      }

      RotatePlane90(input,      iw, output_90,  ow, iw, ih);
      RotatePlane90(output_90,  ow, output_180, oh, ow, oh);
      RotatePlane90(output_180, oh, output_270, ow, oh, ow);
      RotatePlane90(output_270, ow, output_0,   iw, ow, oh);

      for (i = 0; i < iw * ih; ++i) {
        if (input[i] != output_0[i]) {
          err++;
        }
      }

      if (err) {
        printf("input %dx%d \n", iw, ih);
        PrintArray(input, iw, ih);

        printf("output 90\n");
        PrintArray(output_90, ow, oh);

        printf("output 180\n");
        PrintArray(output_180, iw, ih);

        printf("output 270\n");
        PrintArray(output_270, ow, oh);

        printf("output 0\n");
        PrintArray(output_0, iw, ih);
      }

      free_aligned_buffer_16(input)
      free_aligned_buffer_16(output_0)
      free_aligned_buffer_16(output_90)
      free_aligned_buffer_16(output_180)
      free_aligned_buffer_16(output_270)
    }
  }

  EXPECT_EQ(0, err);
}

TEST_F(libyuvTest, RotateUV90) {
  int iw, ih, ow, oh;
  int err = 0;

  for (iw = 16; iw < rotate_max_w_ && !err; iw += 2) {
    for (ih = 8; ih < rotate_max_h_ && !err; ++ih) {
      int i;

      ow = ih;
      oh = iw >> 1;

      align_buffer_16(input, iw * ih)
      align_buffer_16(output_0_u, ow * oh)
      align_buffer_16(output_0_v, ow * oh)
      align_buffer_16(output_90_u, ow * oh)
      align_buffer_16(output_90_v, ow * oh)
      align_buffer_16(output_180_u, ow * oh)
      align_buffer_16(output_180_v, ow * oh)

      for (i = 0; i < iw * ih; i += 2) {
        input[i] = i >> 1;
        input[i + 1] = -(i >> 1);
      }

      RotateUV90(input, iw, output_90_u, ow, output_90_v, ow, iw >> 1, ih);

      RotatePlane90(output_90_u, ow, output_180_u, oh, ow, oh);
      RotatePlane90(output_90_v, ow, output_180_v, oh, ow, oh);

      RotatePlane180(output_180_u, ow, output_0_u, ow, ow, oh);
      RotatePlane180(output_180_v, ow, output_0_v, ow, ow, oh);

      for (i = 0; i < (ow * oh); ++i) {
        if (output_0_u[i] != (uint8)i) {
          err++;
        }
        if (output_0_v[i] != (uint8)(-i)) {
          err++;
        }
      }

      if (err) {
        printf("input %dx%d \n", iw, ih);
        PrintArray(input, iw, ih);

        printf("output 90_u\n");
        PrintArray(output_90_u, ow, oh);

        printf("output 90_v\n");
        PrintArray(output_90_v, ow, oh);

        printf("output 180_u\n");
        PrintArray(output_180_u, oh, ow);

        printf("output 180_v\n");
        PrintArray(output_180_v, oh, ow);

        printf("output 0_u\n");
        PrintArray(output_0_u, oh, ow);

        printf("output 0_v\n");
        PrintArray(output_0_v, oh, ow);
      }

      free_aligned_buffer_16(input)
      free_aligned_buffer_16(output_0_u)
      free_aligned_buffer_16(output_0_v)
      free_aligned_buffer_16(output_90_u)
      free_aligned_buffer_16(output_90_v)
      free_aligned_buffer_16(output_180_u)
      free_aligned_buffer_16(output_180_v)
    }
  }

  EXPECT_EQ(0, err);
}

TEST_F(libyuvTest, RotateUV180) {
  int iw, ih, ow, oh;
  int err = 0;

  for (iw = 16; iw < rotate_max_w_ && !err; iw += 2) {
    for (ih = 8; ih < rotate_max_h_ && !err; ++ih) {
      int i;

      ow = iw >> 1;
      oh = ih;

      align_buffer_16(input, iw * ih)
      align_buffer_16(output_0_u, ow * oh)
      align_buffer_16(output_0_v, ow * oh)
      align_buffer_16(output_90_u, ow * oh)
      align_buffer_16(output_90_v, ow * oh)
      align_buffer_16(output_180_u, ow * oh)
      align_buffer_16(output_180_v, ow * oh)

      for (i = 0; i < iw * ih; i += 2) {
        input[i] = i >> 1;
        input[i + 1] = -(i >> 1);
      }

      RotateUV180(input, iw, output_180_u, ow, output_180_v, ow, iw >> 1, ih);

      RotatePlane90(output_180_u, ow, output_90_u, oh, ow, oh);
      RotatePlane90(output_180_v, ow, output_90_v, oh, ow, oh);

      RotatePlane90(output_90_u, oh, output_0_u, ow, oh, ow);
      RotatePlane90(output_90_v, oh, output_0_v, ow, oh, ow);

      for (i = 0; i < (ow * oh); ++i) {
        if (output_0_u[i] != (uint8)i) {
          err++;
        }
        if (output_0_v[i] != (uint8)(-i)) {
          err++;
        }
      }

      if (err) {
        printf("input %dx%d \n", iw, ih);
        PrintArray(input, iw, ih);

        printf("output 180_u\n");
        PrintArray(output_180_u, oh, ow);

        printf("output 180_v\n");
        PrintArray(output_180_v, oh, ow);

        printf("output 90_u\n");
        PrintArray(output_90_u, oh, ow);

        printf("output 90_v\n");
        PrintArray(output_90_v, oh, ow);

        printf("output 0_u\n");
        PrintArray(output_0_u, ow, oh);

        printf("output 0_v\n");
        PrintArray(output_0_v, ow, oh);
      }

      free_aligned_buffer_16(input)
      free_aligned_buffer_16(output_0_u)
      free_aligned_buffer_16(output_0_v)
      free_aligned_buffer_16(output_90_u)
      free_aligned_buffer_16(output_90_v)
      free_aligned_buffer_16(output_180_u)
      free_aligned_buffer_16(output_180_v)
    }
  }

  EXPECT_EQ(0, err);
}

TEST_F(libyuvTest, RotateUV270) {
  int iw, ih, ow, oh;
  int err = 0;

  for (iw = 16; iw < rotate_max_w_ && !err; iw += 2) {
    for (ih = 8; ih < rotate_max_h_ && !err; ++ih) {
      int i;

      ow = ih;
      oh = iw >> 1;

      align_buffer_16(input, iw * ih)
      align_buffer_16(output_0_u, ow * oh)
      align_buffer_16(output_0_v, ow * oh)
      align_buffer_16(output_270_u, ow * oh)
      align_buffer_16(output_270_v, ow * oh)
      align_buffer_16(output_180_u, ow * oh)
      align_buffer_16(output_180_v, ow * oh)

      for (i = 0; i < iw * ih; i += 2) {
        input[i] = i >> 1;
        input[i + 1] = -(i >> 1);
      }

      RotateUV270(input, iw, output_270_u, ow, output_270_v, ow,
                       iw >> 1, ih);

      RotatePlane270(output_270_u, ow, output_180_u, oh, ow, oh);
      RotatePlane270(output_270_v, ow, output_180_v, oh, ow, oh);

      RotatePlane180(output_180_u, ow, output_0_u, ow, ow, oh);
      RotatePlane180(output_180_v, ow, output_0_v, ow, ow, oh);

      for (i = 0; i < (ow * oh); ++i) {
        if (output_0_u[i] != (uint8)i) {
          err++;
        }
        if (output_0_v[i] != (uint8)(-i)) {
          err++;
        }
      }

      if (err) {
        printf("input %dx%d \n", iw, ih);
        PrintArray(input, iw, ih);

        printf("output 270_u\n");
        PrintArray(output_270_u, ow, oh);

        printf("output 270_v\n");
        PrintArray(output_270_v, ow, oh);

        printf("output 180_u\n");
        PrintArray(output_180_u, oh, ow);

        printf("output 180_v\n");
        PrintArray(output_180_v, oh, ow);

        printf("output 0_u\n");
        PrintArray(output_0_u, oh, ow);

        printf("output 0_v\n");
        PrintArray(output_0_v, oh, ow);
      }

      free_aligned_buffer_16(input)
      free_aligned_buffer_16(output_0_u)
      free_aligned_buffer_16(output_0_v)
      free_aligned_buffer_16(output_270_u)
      free_aligned_buffer_16(output_270_v)
      free_aligned_buffer_16(output_180_u)
      free_aligned_buffer_16(output_180_v)
    }
  }

  EXPECT_EQ(0, err);
}

TEST_F(libyuvTest, RotatePlane180) {
  int iw, ih, ow, oh;
  int err = 0;

  for (iw = 8; iw < rotate_max_w_ && !err; ++iw)
    for (ih = 8; ih < rotate_max_h_ && !err; ++ih) {
      int i;

      ow = iw;
      oh = ih;

      align_buffer_16(input, iw * ih)
      align_buffer_16(output_0, iw * ih)
      align_buffer_16(output_180, iw * ih)

      for (i = 0; i < iw * ih; ++i) {
        input[i] = i;
      }

      RotatePlane180(input,      iw, output_180, ow, iw, ih);
      RotatePlane180(output_180, ow, output_0,   iw, ow, oh);

      for (i = 0; i < iw * ih; ++i) {
        if (input[i] != output_0[i]) {
          err++;
        }
      }

      if (err) {
        printf("input %dx%d \n", iw, ih);
        PrintArray(input, iw, ih);

        printf("output 180\n");
        PrintArray(output_180, iw, ih);

        printf("output 0\n");
        PrintArray(output_0, iw, ih);
      }

      free_aligned_buffer_16(input)
      free_aligned_buffer_16(output_0)
      free_aligned_buffer_16(output_180)
    }

  EXPECT_EQ(0, err);
}

TEST_F(libyuvTest, RotatePlane270) {
  int iw, ih, ow, oh;
  int err = 0;

  for (iw = 8; iw < rotate_max_w_ && !err; ++iw) {
    for (ih = 8; ih < rotate_max_h_ && !err; ++ih) {
      int i;

      ow = ih;
      oh = iw;

      align_buffer_16(input, iw * ih)
      align_buffer_16(output_0, iw * ih)
      align_buffer_16(output_90, ow * oh)
      align_buffer_16(output_180, iw * ih)
      align_buffer_16(output_270, ow * oh)

      for (i = 0; i < iw * ih; ++i)
        input[i] = i;

      RotatePlane270(input,      iw, output_270, ow, iw, ih);
      RotatePlane270(output_270, ow, output_180, oh, ow, oh);
      RotatePlane270(output_180, oh, output_90,  ow, oh, ow);
      RotatePlane270(output_90,  ow, output_0,   iw, ow, oh);

      for (i = 0; i < iw * ih; ++i) {
        if (input[i] != output_0[i]) {
          err++;
        }
      }

      if (err) {
        printf("input %dx%d \n", iw, ih);
        PrintArray(input, iw, ih);

        printf("output 270\n");
        PrintArray(output_270, ow, oh);

        printf("output 180\n");
        PrintArray(output_180, iw, ih);

        printf("output 90\n");
        PrintArray(output_90, ow, oh);

        printf("output 0\n");
        PrintArray(output_0, iw, ih);
      }

      free_aligned_buffer_16(input)
      free_aligned_buffer_16(output_0)
      free_aligned_buffer_16(output_90)
      free_aligned_buffer_16(output_180)
      free_aligned_buffer_16(output_270)
    }
  }

  EXPECT_EQ(0, err);
}

TEST_F(libyuvTest, RotatePlane90and270) {
  int iw, ih, ow, oh;
  int err = 0;

  for (iw = 16; iw < rotate_max_w_ && !err; iw += 4)
    for (ih = 16; ih < rotate_max_h_ && !err; ih += 4) {
      int i;

      ow = ih;
      oh = iw;

      align_buffer_16(input, iw * ih)
      align_buffer_16(output_0, iw * ih)
      align_buffer_16(output_90, ow * oh)

      for (i = 0; i < iw * ih; ++i) {
        input[i] = i;
      }

      RotatePlane90(input,      iw, output_90,  ow, iw, ih);
      RotatePlane270(output_90, ow, output_0,   iw, ow, oh);

      for (i = 0; i < iw * ih; ++i) {
        if (input[i] != output_0[i]) {
          err++;
        }
      }

      if (err) {
        printf("intput %dx%d\n", iw, ih);
        PrintArray(input, iw, ih);

        printf("output \n");
        PrintArray(output_90, ow, oh);

        printf("output \n");
        PrintArray(output_0, iw, ih);
      }

      free_aligned_buffer_16(input)
      free_aligned_buffer_16(output_0)
      free_aligned_buffer_16(output_90)
    }

  EXPECT_EQ(0, err);
}

TEST_F(libyuvTest, RotatePlane90Pitch) {
  int iw, ih;
  int err = 0;

  for (iw = 16; iw < rotate_max_w_ && !err; iw += 4)
    for (ih = 16; ih < rotate_max_h_ && !err; ih += 4) {
      int i;

      int ow = ih;
      int oh = iw;

      align_buffer_16(input, iw * ih)
      align_buffer_16(output_0, iw * ih)
      align_buffer_16(output_90, ow * oh)

      for (i = 0; i < iw * ih; ++i) {
        input[i] = i;
      }

      RotatePlane90(input, iw,
                    output_90 + (ow >> 1), ow,
                    iw >> 1, ih >> 1);
      RotatePlane90(input + (iw >> 1), iw,
                    output_90 + (ow >> 1) + ow * (oh >> 1), ow,
                    iw >> 1, ih >> 1);
      RotatePlane90(input + iw * (ih >> 1), iw,
                    output_90, ow,
                    iw >> 1, ih >> 1);
      RotatePlane90(input + (iw >> 1) + iw * (ih >> 1), iw,
                    output_90 + ow * (oh >> 1), ow,
                    iw >> 1, ih >> 1);

      RotatePlane270(output_90, ih, output_0,   iw, ow, oh);

      for (i = 0; i < iw * ih; ++i) {
        if (input[i] != output_0[i]) {
          err++;
        }
      }

      if (err) {
        printf("intput %dx%d\n", iw, ih);
        PrintArray(input, iw, ih);

        printf("output \n");
        PrintArray(output_90, ow, oh);

        printf("output \n");
        PrintArray(output_0, iw, ih);
      }

      free_aligned_buffer_16(input)
      free_aligned_buffer_16(output_0)
      free_aligned_buffer_16(output_90)
    }

  EXPECT_EQ(0, err);
}

TEST_F(libyuvTest, RotatePlane270Pitch) {
  int iw, ih, ow, oh;
  int err = 0;

  for (iw = 16; iw < rotate_max_w_ && !err; iw += 4) {
    for (ih = 16; ih < rotate_max_h_ && !err; ih += 4) {
      int i;

      ow = ih;
      oh = iw;

      align_buffer_16(input, iw * ih)
      align_buffer_16(output_0, iw * ih)
      align_buffer_16(output_270, ow * oh)

      for (i = 0; i < iw * ih; ++i) {
        input[i] = i;
      }

      RotatePlane270(input, iw,
                     output_270 + ow * (oh >> 1), ow,
                     iw >> 1, ih >> 1);
      RotatePlane270(input + (iw >> 1), iw,
                     output_270, ow,
                     iw >> 1, ih >> 1);
      RotatePlane270(input + iw * (ih >> 1), iw,
                     output_270 + (ow >> 1) + ow * (oh >> 1), ow,
                     iw >> 1, ih >> 1);
      RotatePlane270(input + (iw >> 1) + iw * (ih >> 1), iw,
                     output_270 + (ow >> 1), ow,
                     iw >> 1, ih >> 1);

      RotatePlane90(output_270, ih, output_0,   iw, ow, oh);

      for (i = 0; i < iw * ih; ++i) {
        if (input[i] != output_0[i]) {
          err++;
        }
      }

      if (err) {
        printf("intput %dx%d\n", iw, ih);
        PrintArray(input, iw, ih);

        printf("output \n");
        PrintArray(output_270, ow, oh);

        printf("output \n");
        PrintArray(output_0, iw, ih);
      }

      free_aligned_buffer_16(input)
      free_aligned_buffer_16(output_0)
      free_aligned_buffer_16(output_270)
    }
  }

  EXPECT_EQ(0, err);
}

TEST_F(libyuvTest, I420Rotate90) {
  int err = 0;

  int yw = 1024;
  int yh = 768;
  int b = 128;
  int uvw = (yw + 1) >> 1;
  int uvh = (yh + 1) >> 1;

  int i, j;

  int y_plane_size = (yw + b * 2) * (yh + b * 2);
  int uv_plane_size = (uvw + b * 2) * (uvh + b * 2);

  srandom(time(NULL));

  align_buffer_16(orig_y, y_plane_size)
  align_buffer_16(orig_u, uv_plane_size)
  align_buffer_16(orig_v, uv_plane_size)
  align_buffer_16(ro0_y, y_plane_size)
  align_buffer_16(ro0_u, uv_plane_size)
  align_buffer_16(ro0_v, uv_plane_size)
  align_buffer_16(ro90_y, y_plane_size)
  align_buffer_16(ro90_u, uv_plane_size)
  align_buffer_16(ro90_v, uv_plane_size)
  align_buffer_16(ro270_y, y_plane_size)
  align_buffer_16(ro270_u, uv_plane_size)
  align_buffer_16(ro270_v, uv_plane_size)
  memset(orig_y, 0, y_plane_size);
  memset(orig_u, 0, uv_plane_size);
  memset(orig_v, 0, uv_plane_size);
  memset(ro0_y, 0, y_plane_size);
  memset(ro0_u, 0, uv_plane_size);
  memset(ro0_v, 0, uv_plane_size);
  memset(ro90_y, 0, y_plane_size);
  memset(ro90_u, 0, uv_plane_size);
  memset(ro90_v, 0, uv_plane_size);
  memset(ro270_y, 0, y_plane_size);
  memset(ro270_u, 0, uv_plane_size);
  memset(ro270_v, 0, uv_plane_size);

  // fill image buffers with random data
  for (i = b; i < (yh + b); ++i) {
    for (j = b; j < (yw + b); ++j) {
      orig_y[i * (yw + b * 2) + j] = random() & 0xff;
    }
  }

  for (i = b; i < (uvh + b); ++i) {
    for (j = b; j < (uvw + b); ++j) {
      orig_u[i * (uvw + b * 2) + j] = random() & 0xff;
      orig_v[i * (uvw + b * 2) + j] = random() & 0xff;
    }
  }

  int y_off_0 = b * (yw + b * 2) + b;
  int uv_off_0 = b * (uvw + b * 2) + b;
  int y_off_90 = b * (yh + b * 2) + b;
  int uv_off_90 = b * (uvh + b * 2) + b;

  int y_st_0 = yw + b * 2;
  int uv_st_0 = uvw + b * 2;
  int y_st_90 = yh + b * 2;
  int uv_st_90 = uvh + b * 2;

  I420Rotate(orig_y+y_off_0, y_st_0,
             orig_u+uv_off_0, uv_st_0,
             orig_v+uv_off_0, uv_st_0,
             ro90_y+y_off_90, y_st_90,
             ro90_u+uv_off_90, uv_st_90,
             ro90_v+uv_off_90, uv_st_90,
             yw, yh,
             kRotateClockwise);

  I420Rotate(ro90_y+y_off_90, y_st_90,
             ro90_u+uv_off_90, uv_st_90,
             ro90_v+uv_off_90, uv_st_90,
             ro270_y+y_off_90, y_st_90,
             ro270_u+uv_off_90, uv_st_90,
             ro270_v+uv_off_90, uv_st_90,
             yh, yw,
             kRotate180);

  I420Rotate(ro270_y+y_off_90, y_st_90,
             ro270_u+uv_off_90, uv_st_90,
             ro270_v+uv_off_90, uv_st_90,
             ro0_y+y_off_0, y_st_0,
             ro0_u+uv_off_0, uv_st_0,
             ro0_v+uv_off_0, uv_st_0,
             yh, yw,
             kRotateClockwise);

  for (i = 0; i < y_plane_size; ++i) {
    if (orig_y[i] != ro0_y[i]) {
      ++err;
    }
  }

  for (i = 0; i < uv_plane_size; ++i) {
    if (orig_u[i] != ro0_u[i]) {
      ++err;
    }
    if (orig_v[i] != ro0_v[i]) {
      ++err;
    }
  }

  free_aligned_buffer_16(orig_y)
  free_aligned_buffer_16(orig_u)
  free_aligned_buffer_16(orig_v)
  free_aligned_buffer_16(ro0_y)
  free_aligned_buffer_16(ro0_u)
  free_aligned_buffer_16(ro0_v)
  free_aligned_buffer_16(ro90_y)
  free_aligned_buffer_16(ro90_u)
  free_aligned_buffer_16(ro90_v)
  free_aligned_buffer_16(ro270_y)
  free_aligned_buffer_16(ro270_u)
  free_aligned_buffer_16(ro270_v)

  EXPECT_EQ(0, err);
}

TEST_F(libyuvTest, I420Rotate270) {
  int err = 0;

  int yw = 1024;
  int yh = 768;
  int b = 128;
  int uvw = (yw + 1) >> 1;
  int uvh = (yh + 1) >> 1;

  int i, j;

  int y_plane_size = (yw + b * 2) * (yh + b * 2);
  int uv_plane_size = (uvw + b * 2) * (uvh + b * 2);

  srandom(time(NULL));

  align_buffer_16(orig_y, y_plane_size)
  align_buffer_16(orig_u, uv_plane_size)
  align_buffer_16(orig_v, uv_plane_size)
  align_buffer_16(ro0_y, y_plane_size)
  align_buffer_16(ro0_u, uv_plane_size)
  align_buffer_16(ro0_v, uv_plane_size)
  align_buffer_16(ro90_y, y_plane_size)
  align_buffer_16(ro90_u, uv_plane_size)
  align_buffer_16(ro90_v, uv_plane_size)
  align_buffer_16(ro270_y, y_plane_size)
  align_buffer_16(ro270_u, uv_plane_size)
  align_buffer_16(ro270_v, uv_plane_size)
  memset(orig_y, 0, y_plane_size);
  memset(orig_u, 0, uv_plane_size);
  memset(orig_v, 0, uv_plane_size);
  memset(ro0_y, 0, y_plane_size);
  memset(ro0_u, 0, uv_plane_size);
  memset(ro0_v, 0, uv_plane_size);
  memset(ro90_y, 0, y_plane_size);
  memset(ro90_u, 0, uv_plane_size);
  memset(ro90_v, 0, uv_plane_size);
  memset(ro270_y, 0, y_plane_size);
  memset(ro270_u, 0, uv_plane_size);
  memset(ro270_v, 0, uv_plane_size);

  // fill image buffers with random data
  for (i = b; i < (yh + b); ++i) {
    for (j = b; j < (yw + b); ++j) {
      orig_y[i * (yw + b * 2) + j] = random() & 0xff;
    }
  }

  for (i = b; i < (uvh + b); ++i) {
    for (j = b; j < (uvw + b); ++j) {
      orig_u[i * (uvw + b * 2) + j] = random() & 0xff;
      orig_v[i * (uvw + b * 2) + j] = random() & 0xff;
    }
  }

  int y_off_0 = b * (yw + b * 2) + b;
  int uv_off_0 = b * (uvw + b * 2) + b;
  int y_off_90 = b * (yh + b * 2) + b;
  int uv_off_90 = b * (uvh + b * 2) + b;

  int y_st_0 = yw + b * 2;
  int uv_st_0 = uvw + b * 2;
  int y_st_90 = yh + b * 2;
  int uv_st_90 = uvh + b * 2;

  I420Rotate(orig_y+y_off_0, y_st_0,
             orig_u+uv_off_0, uv_st_0,
             orig_v+uv_off_0, uv_st_0,
             ro270_y+y_off_90, y_st_90,
             ro270_u+uv_off_90, uv_st_90,
             ro270_v+uv_off_90, uv_st_90,
             yw, yh,
             kRotateCounterClockwise);

  I420Rotate(ro270_y+y_off_90, y_st_90,
             ro270_u+uv_off_90, uv_st_90,
             ro270_v+uv_off_90, uv_st_90,
             ro90_y+y_off_90, y_st_90,
             ro90_u+uv_off_90, uv_st_90,
             ro90_v+uv_off_90, uv_st_90,
             yh, yw,
             kRotate180);

  I420Rotate(ro90_y+y_off_90, y_st_90,
             ro90_u+uv_off_90, uv_st_90,
             ro90_v+uv_off_90, uv_st_90,
             ro0_y+y_off_0, y_st_0,
             ro0_u+uv_off_0, uv_st_0,
             ro0_v+uv_off_0, uv_st_0,
             yh, yw,
             kRotateCounterClockwise);

  for (i = 0; i < y_plane_size; ++i) {
    if (orig_y[i] != ro0_y[i]) {
      ++err;
    }
  }

  for (i = 0; i < uv_plane_size; ++i) {
    if (orig_u[i] != ro0_u[i]) {
      ++err;
    }
    if (orig_v[i] != ro0_v[i]) {
      ++err;
    }
  }

  free_aligned_buffer_16(orig_y)
  free_aligned_buffer_16(orig_u)
  free_aligned_buffer_16(orig_v)
  free_aligned_buffer_16(ro0_y)
  free_aligned_buffer_16(ro0_u)
  free_aligned_buffer_16(ro0_v)
  free_aligned_buffer_16(ro90_y)
  free_aligned_buffer_16(ro90_u)
  free_aligned_buffer_16(ro90_v)
  free_aligned_buffer_16(ro270_y)
  free_aligned_buffer_16(ro270_u)
  free_aligned_buffer_16(ro270_v)

  EXPECT_EQ(0, err);
}

TEST_F(libyuvTest, NV12ToI420Rotate90) {
  int err = 0;

  int yw = 1024;
  int yh = 768;
  int b = 128;
  int uvw = (yw + 1) >> 1;
  int uvh = (yh + 1) >> 1;
  int i, j;

  int y_plane_size = (yw + b * 2) * (yh + b * 2);
  int uv_plane_size = (uvw + b * 2) * (uvh + b * 2);
  int o_uv_plane_size = (uvw * 2 + b * 2) * (uvh + b * 2);

  srandom(time(NULL));

  align_buffer_16(orig_y, y_plane_size)
  align_buffer_16(orig_uv, o_uv_plane_size)
  align_buffer_16(ro0_y, y_plane_size)
  align_buffer_16(ro0_u, uv_plane_size)
  align_buffer_16(ro0_v, uv_plane_size)
  align_buffer_16(ro90_y, y_plane_size)
  align_buffer_16(ro90_u, uv_plane_size)
  align_buffer_16(ro90_v, uv_plane_size)
  memset(orig_y, 0, y_plane_size);
  memset(orig_uv, 0, uv_plane_size);
  memset(ro0_y, 0, y_plane_size);
  memset(ro0_u, 0, uv_plane_size);
  memset(ro0_v, 0, uv_plane_size);
  memset(ro90_y, 0, y_plane_size);
  memset(ro90_u, 0, uv_plane_size);
  memset(ro90_v, 0, uv_plane_size);

  // fill image buffers with random data
  for (i = b; i < (yh + b); ++i) {
    for (j = b; j < (yw + b); ++j) {
      orig_y[i * (yw + b * 2) + j] = random() & 0xff;
    }
  }

  for (i = b; i < (uvh + b); ++i) {
    for (j = b; j < (uvw * 2 + b); j += 2) {
      uint8 random_number = random() & 0x7f;
      orig_uv[i * (uvw * 2 + b * 2) + j] = random_number;
      orig_uv[i * (uvw * 2 + b * 2) + j + 1] = -random_number;
    }
  }

  int y_off_0 = b * (yw + b * 2) + b;
  int uv_off_0 = b * (uvw + b * 2) + b;
  int y_off_90 = b * (yh + b * 2) + b;
  int uv_off_90 = b * (uvh + b * 2) + b;

  int y_st_0 = yw + b * 2;
  int uv_st_0 = uvw + b * 2;
  int y_st_90 = yh + b * 2;
  int uv_st_90 = uvh + b * 2;

  NV12ToI420Rotate(orig_y+y_off_0, y_st_0,
                   orig_uv+y_off_0, y_st_0,
                   ro90_y+y_off_90, y_st_90,
                   ro90_u+uv_off_90, uv_st_90,
                   ro90_v+uv_off_90, uv_st_90,
                   yw, yh,
                   kRotateClockwise);

  I420Rotate(ro90_y+y_off_90, y_st_90,
             ro90_u+uv_off_90, uv_st_90,
             ro90_v+uv_off_90, uv_st_90,
             ro0_y+y_off_0, y_st_0,
             ro0_u+uv_off_0, uv_st_0,
             ro0_v+uv_off_0, uv_st_0,
             yh, yw,
             kRotateCounterClockwise);

  for (i = 0; i < y_plane_size; ++i) {
    if (orig_y[i] != ro0_y[i])
      ++err;
  }

  int zero_cnt = 0;

  for (i = 0; i < uv_plane_size; ++i) {
    if ((signed char)ro0_u[i] != -(signed char)ro0_v[i]) {
      ++err;
    }
    if (ro0_u[i] != 0) {
      ++zero_cnt;
    }
  }

  if (!zero_cnt) {
    ++err;
  }

  free_aligned_buffer_16(orig_y)
  free_aligned_buffer_16(orig_uv)
  free_aligned_buffer_16(ro0_y)
  free_aligned_buffer_16(ro0_u)
  free_aligned_buffer_16(ro0_v)
  free_aligned_buffer_16(ro90_y)
  free_aligned_buffer_16(ro90_u)
  free_aligned_buffer_16(ro90_v)

  EXPECT_EQ(0, err);
}

TEST_F(libyuvTest, NV12ToI420Rotate270) {
  int err = 0;

  int yw = 1024;
  int yh = 768;
  int b = 128;
  int uvw = (yw + 1) >> 1;
  int uvh = (yh + 1) >> 1;

  int i, j;

  int y_plane_size = (yw + b * 2) * (yh + b * 2);
  int uv_plane_size = (uvw + b * 2) * (uvh + b * 2);
  int o_uv_plane_size = (uvw * 2 + b * 2) * (uvh + b * 2);

  srandom(time(NULL));

  align_buffer_16(orig_y, y_plane_size)
  align_buffer_16(orig_uv, o_uv_plane_size)
  align_buffer_16(ro0_y, y_plane_size)
  align_buffer_16(ro0_u, uv_plane_size)
  align_buffer_16(ro0_v, uv_plane_size)
  align_buffer_16(ro270_y, y_plane_size)
  align_buffer_16(ro270_u, uv_plane_size)
  align_buffer_16(ro270_v, uv_plane_size)
  memset(orig_y, 0, y_plane_size);
  memset(orig_uv, 0, o_uv_plane_size);
  memset(ro0_y, 0, y_plane_size);
  memset(ro0_u, 0, uv_plane_size);
  memset(ro0_v, 0, uv_plane_size);
  memset(ro270_y, 0, y_plane_size);
  memset(ro270_u, 0, uv_plane_size);
  memset(ro270_v, 0, uv_plane_size);

  // fill image buffers with random data
  for (i = b; i < (yh + b); ++i) {
    for (j = b; j < (yw + b); ++j) {
      orig_y[i * (yw + b * 2) + j] = random() & 0xff;
    }
  }

  for (i = b; i < (uvh + b); ++i) {
    for (j = b; j < (uvw * 2 + b); j += 2) {
      uint8 random_number = random() & 0x7f;
      orig_uv[i * (uvw * 2 + b * 2) + j] = random_number;
      orig_uv[i * (uvw * 2 + b * 2) + j + 1] = -random_number;
    }
  }

  int y_off_0 = b * (yw + b * 2) + b;
  int uv_off_0 = b * (uvw + b * 2) + b;
  int y_off_270 = b * (yh + b * 2) + b;
  int uv_off_270 = b * (uvh + b * 2) + b;

  int y_st_0 = yw + b * 2;
  int uv_st_0 = uvw + b * 2;
  int y_st_270 = yh + b * 2;
  int uv_st_270 = uvh + b * 2;

  NV12ToI420Rotate(orig_y+y_off_0, y_st_0,
                   orig_uv+y_off_0, y_st_0,
                   ro270_y+y_off_270, y_st_270,
                   ro270_u+uv_off_270, uv_st_270,
                   ro270_v+uv_off_270, uv_st_270,
                   yw, yh,
                   kRotateCounterClockwise);

  I420Rotate(ro270_y+y_off_270, y_st_270,
             ro270_u+uv_off_270, uv_st_270,
             ro270_v+uv_off_270, uv_st_270,
             ro0_y+y_off_0, y_st_0,
             ro0_u+uv_off_0, uv_st_0,
             ro0_v+uv_off_0, uv_st_0,
             yh, yw,
             kRotateClockwise);

  for (i = 0; i < y_plane_size; ++i) {
    if (orig_y[i] != ro0_y[i])
      ++err;
  }

  int zero_cnt = 0;

  for (i = 0; i < uv_plane_size; ++i) {
    if ((signed char)ro0_u[i] != -(signed char)ro0_v[i]) {
      ++err;
    }
    if (ro0_u[i] != 0) {
      ++zero_cnt;
    }
  }

  if (!zero_cnt) {
    ++err;
  }

  free_aligned_buffer_16(orig_y)
  free_aligned_buffer_16(orig_uv)
  free_aligned_buffer_16(ro0_y)
  free_aligned_buffer_16(ro0_u)
  free_aligned_buffer_16(ro0_v)
  free_aligned_buffer_16(ro270_y)
  free_aligned_buffer_16(ro270_u)
  free_aligned_buffer_16(ro270_v)

  EXPECT_EQ(0, err);
}

TEST_F(libyuvTest, NV12ToI420Rotate180) {
  int err = 0;

  int yw = 1024;
  int yh = 768;
  int b = 128;
  int uvw = (yw + 1) >> 1;
  int uvh = (yh + 1) >> 1;

  int i, j;

  int y_plane_size = (yw + b * 2) * (yh + b * 2);
  int uv_plane_size = (uvw + b * 2) * (uvh + b * 2);
  int o_uv_plane_size = (uvw * 2 + b * 2) * (uvh + b * 2);

  srandom(time(NULL));

  align_buffer_16(orig_y, y_plane_size)
  align_buffer_16(orig_uv, o_uv_plane_size)
  align_buffer_16(ro0_y, y_plane_size)
  align_buffer_16(ro0_u, uv_plane_size)
  align_buffer_16(ro0_v, uv_plane_size)
  align_buffer_16(ro180_y, y_plane_size)
  align_buffer_16(ro180_u, uv_plane_size)
  align_buffer_16(ro180_v, uv_plane_size)
  memset(orig_y, 0, y_plane_size);
  memset(orig_uv, 0, o_uv_plane_size);
  memset(ro0_y, 0, y_plane_size);
  memset(ro0_u, 0, uv_plane_size);
  memset(ro0_v, 0, uv_plane_size);
  memset(ro180_y, 0, y_plane_size);
  memset(ro180_u, 0, uv_plane_size);
  memset(ro180_v, 0, uv_plane_size);

  // fill image buffers with random data
  for (i = b; i < (yh + b); ++i) {
    for (j = b; j < (yw + b); ++j) {
      orig_y[i * (yw + b * 2) + j] = random() & 0xff;
    }
  }

  for (i = b; i < (uvh + b); ++i) {
    for (j = b; j < (uvw * 2 + b); j += 2) {
      uint8 random_number = random() & 0x7f;
      orig_uv[i * (uvw * 2 + b * 2) + j] = random_number;
      orig_uv[i * (uvw * 2 + b * 2) + j + 1] = -random_number;
    }
  }

  int y_off = b * (yw + b * 2) + b;
  int uv_off = b * (uvw + b * 2) + b;

  int y_st = yw + b * 2;
  int uv_st = uvw + b * 2;

  NV12ToI420Rotate(orig_y+y_off, y_st,
                   orig_uv+y_off, y_st,
                   ro180_y+y_off, y_st,
                   ro180_u+uv_off, uv_st,
                   ro180_v+uv_off, uv_st,
                   yw, yh,
                   kRotate180);

  I420Rotate(ro180_y+y_off, y_st,
             ro180_u+uv_off, uv_st,
             ro180_v+uv_off, uv_st,
             ro0_y+y_off, y_st,
             ro0_u+uv_off, uv_st,
             ro0_v+uv_off, uv_st,
             yw, yh,
             kRotate180);

  for (i = 0; i < y_plane_size; ++i) {
    if (orig_y[i] != ro0_y[i]) {
      ++err;
    }
  }

  int zero_cnt = 0;

  for (i = 0; i < uv_plane_size; ++i) {
    if ((signed char)ro0_u[i] != -(signed char)ro0_v[i]) {
      ++err;
    }
    if (ro0_u[i] != 0) {
      ++zero_cnt;
    }
  }

  if (!zero_cnt) {
    ++err;
  }

  free_aligned_buffer_16(orig_y)
  free_aligned_buffer_16(orig_uv)
  free_aligned_buffer_16(ro0_y)
  free_aligned_buffer_16(ro0_u)
  free_aligned_buffer_16(ro0_v)
  free_aligned_buffer_16(ro180_y)
  free_aligned_buffer_16(ro180_u)
  free_aligned_buffer_16(ro180_v)

  EXPECT_EQ(0, err);
}

TEST_F(libyuvTest, NV12ToI420RotateNegHeight90) {
  int y_err = 0, uv_err = 0;

  int yw = 1024;
  int yh = 768;
  int b = 128;
  int uvw = (yw + 1) >> 1;
  int uvh = (yh + 1) >> 1;
  int i, j;

  int y_plane_size = (yw + b * 2) * (yh + b * 2);
  int uv_plane_size = (uvw + b * 2) * (uvh + b * 2);
  int o_uv_plane_size = (uvw * 2 + b * 2) * (uvh + b * 2);

  srandom(time(NULL));

  align_buffer_16(orig_y, y_plane_size)
  align_buffer_16(orig_uv, o_uv_plane_size)
  align_buffer_16(roa_y, y_plane_size)
  align_buffer_16(roa_u, uv_plane_size)
  align_buffer_16(roa_v, uv_plane_size)
  align_buffer_16(rob_y, y_plane_size)
  align_buffer_16(rob_u, uv_plane_size)
  align_buffer_16(rob_v, uv_plane_size)
  align_buffer_16(roc_y, y_plane_size)
  align_buffer_16(roc_u, uv_plane_size)
  align_buffer_16(roc_v, uv_plane_size)
  memset(orig_y, 0, y_plane_size);
  memset(orig_uv, 0, o_uv_plane_size);
  memset(roa_y, 0, y_plane_size);
  memset(roa_u, 0, uv_plane_size);
  memset(roa_v, 0, uv_plane_size);
  memset(rob_y, 0, y_plane_size);
  memset(rob_u, 0, uv_plane_size);
  memset(rob_v, 0, uv_plane_size);
  memset(roc_y, 0, y_plane_size);
  memset(roc_u, 0, uv_plane_size);
  memset(roc_v, 0, uv_plane_size);

  // fill image buffers with random data
  for (i = b; i < (yh + b); ++i) {
    for (j = b; j < (yw + b); ++j) {
      orig_y[i * (yw + b * 2) + j] = random() & 0xff;
    }
  }

  for (i = b; i < (uvh + b); ++i) {
    for (j = b; j < (uvw * 2 + b); j += 2) {
      uint8 random_number = random() & 0x7f;
      orig_uv[i * (uvw * 2 + b * 2) + j] = random_number;
      orig_uv[i * (uvw * 2 + b * 2) + j + 1] = -random_number;
    }
  }

  int y_off_0 = b * (yw + b * 2) + b;
  int uv_off_0 = b * (uvw + b * 2) + b;
  int y_off_90 = b * (yh + b * 2) + b;
  int uv_off_90 = b * (uvh + b * 2) + b;

  int y_st_0 = yw + b * 2;
  int uv_st_0 = uvw + b * 2;
  int y_st_90 = yh + b * 2;
  int uv_st_90 = uvh + b * 2;

  NV12ToI420Rotate(orig_y+y_off_0, y_st_0,
                   orig_uv+y_off_0, y_st_0,
                   roa_y+y_off_90, y_st_90,
                   roa_u+uv_off_90, uv_st_90,
                   roa_v+uv_off_90, uv_st_90,
                   yw, -yh,
                   kRotateClockwise);

  I420Rotate(roa_y+y_off_90, y_st_90,
             roa_u+uv_off_90, uv_st_90,
             roa_v+uv_off_90, uv_st_90,
             rob_y+y_off_0, y_st_0,
             rob_u+uv_off_0, uv_st_0,
             rob_v+uv_off_0, uv_st_0,
             yh, -yw,
             kRotateCounterClockwise);

  I420Rotate(rob_y+y_off_0, y_st_0,
             rob_u+uv_off_0, uv_st_0,
             rob_v+uv_off_0, uv_st_0,
             roc_y+y_off_0, y_st_0,
             roc_u+uv_off_0, uv_st_0,
             roc_v+uv_off_0, uv_st_0,
             yw, yh,
             kRotate180);

  for (i = 0; i < y_plane_size; ++i) {
    if (orig_y[i] != roc_y[i]) {
      ++y_err;
    }
  }

  if (y_err) {
    printf("input %dx%d \n", yw, yh);
    PrintArray(orig_y, y_st_0, yh + b * 2);

    printf("rotate a\n");
    PrintArray(roa_y, y_st_90, y_st_0);

    printf("rotate b\n");
    PrintArray(rob_y, y_st_90, y_st_0);

    printf("rotate c\n");
    PrintArray(roc_y, y_st_0, y_st_90);
  }

  int zero_cnt = 0;

  for (i = 0; i < uv_plane_size; ++i) {
    if ((signed char)roc_u[i] != -(signed char)roc_v[i]) {
      ++uv_err;
    }
    if (rob_u[i] != 0) {
      ++zero_cnt;
    }
  }

  if (!zero_cnt) {
    ++uv_err;
  }

  if (uv_err) {
    printf("input %dx%d \n", uvw * 2, uvh);
    PrintArray(orig_uv, y_st_0, uvh + b * 2);

    printf("rotate a\n");
    PrintArray(roa_u, uv_st_90, uv_st_0);
    PrintArray(roa_v, uv_st_90, uv_st_0);

    printf("rotate b\n");
    PrintArray(rob_u, uv_st_90, uv_st_0);
    PrintArray(rob_v, uv_st_90, uv_st_0);

    printf("rotate c\n");
    PrintArray(roc_u, uv_st_0, uv_st_90);
    PrintArray(roc_v, uv_st_0, uv_st_90);
  }

  free_aligned_buffer_16(orig_y)
  free_aligned_buffer_16(orig_uv)
  free_aligned_buffer_16(roa_y)
  free_aligned_buffer_16(roa_u)
  free_aligned_buffer_16(roa_v)
  free_aligned_buffer_16(rob_y)
  free_aligned_buffer_16(rob_u)
  free_aligned_buffer_16(rob_v)
  free_aligned_buffer_16(roc_y)
  free_aligned_buffer_16(roc_u)
  free_aligned_buffer_16(roc_v)

  EXPECT_EQ(0, y_err + uv_err);
}

TEST_F(libyuvTest, NV12ToI420RotateNegHeight180) {
  int y_err = 0, uv_err = 0;

  int yw = 1024;
  int yh = 768;
  int b = 128;
  int uvw = (yw + 1) >> 1;
  int uvh = (yh + 1) >> 1;
  int i, j;

  int y_plane_size = (yw + b * 2) * (yh + b * 2);
  int uv_plane_size = (uvw + b * 2) * (uvh + b * 2);
  int o_uv_plane_size = (uvw * 2 + b * 2) * (uvh + b * 2);

  srandom(time(NULL));

  align_buffer_16(orig_y, y_plane_size)
  align_buffer_16(orig_uv, o_uv_plane_size)
  align_buffer_16(roa_y, y_plane_size)
  align_buffer_16(roa_u, uv_plane_size)
  align_buffer_16(roa_v, uv_plane_size)
  align_buffer_16(rob_y, y_plane_size)
  align_buffer_16(rob_u, uv_plane_size)
  align_buffer_16(rob_v, uv_plane_size)
  memset(orig_y, 0, y_plane_size);
  memset(orig_uv, 0, o_uv_plane_size);
  memset(roa_y, 0, y_plane_size);
  memset(roa_u, 0, uv_plane_size);
  memset(roa_v, 0, uv_plane_size);
  memset(rob_y, 0, y_plane_size);
  memset(rob_u, 0, uv_plane_size);
  memset(rob_v, 0, uv_plane_size);

  // fill image buffers with random data
  for (i = b; i < (yh + b); ++i) {
    for (j = b; j < (yw + b); ++j) {
      orig_y[i * (yw + b * 2) + j] = random() & 0xff;
    }
  }

  for (i = b; i < (uvh + b); ++i) {
    for (j = b; j < (uvw * 2 + b); j += 2) {
      uint8 random_number = random() & 0x7f;
      orig_uv[i * (uvw * 2 + b * 2) + j] = random_number;
      orig_uv[i * (uvw * 2 + b * 2) + j + 1] = -random_number;
    }
  }

  int y_off = b * (yw + b * 2) + b;
  int uv_off = b * (uvw + b * 2) + b;

  int y_st = yw + b * 2;
  int uv_st = uvw + b * 2;

  NV12ToI420Rotate(orig_y+y_off, y_st,
                   orig_uv+y_off, y_st,
                   roa_y+y_off, y_st,
                   roa_u+uv_off, uv_st,
                   roa_v+uv_off, uv_st,
                   yw, -yh,
                   kRotate180);

  I420Rotate(roa_y+y_off, y_st,
             roa_u+uv_off, uv_st,
             roa_v+uv_off, uv_st,
             rob_y+y_off, y_st,
             rob_u+uv_off, uv_st,
             rob_v+uv_off, uv_st,
             yw, -yh,
             kRotate180);

  for (i = 0; i < y_plane_size; ++i) {
    if (orig_y[i] != rob_y[i])
      ++y_err;
  }

  if (y_err) {
    printf("input %dx%d \n", yw, yh);
    PrintArray(orig_y, y_st, yh + b * 2);

    printf("rotate a\n");
    PrintArray(roa_y, y_st, yh + b * 2);

    printf("rotate b\n");
    PrintArray(rob_y, y_st, yh + b * 2);
  }

  int zero_cnt = 0;

  for (i = 0; i < uv_plane_size; ++i) {
    if ((signed char)rob_u[i] != -(signed char)rob_v[i]) {
      ++uv_err;
    }
    if (rob_u[i] != 0) {
      ++zero_cnt;
    }
  }

  if (!zero_cnt) {
    ++uv_err;
  }

  if (uv_err) {
    printf("input %dx%d \n", uvw * 2, uvh);
    PrintArray(orig_uv, y_st, uvh + b * 2);

    printf("rotate a\n");
    PrintArray(roa_u, uv_st, uvh + b * 2);
    PrintArray(roa_v, uv_st, uvh + b * 2);

    printf("rotate b\n");
    PrintArray(rob_u, uv_st, uvh + b * 2);
    PrintArray(rob_v, uv_st, uvh + b * 2);
  }

  free_aligned_buffer_16(orig_y)
  free_aligned_buffer_16(orig_uv)
  free_aligned_buffer_16(roa_y)
  free_aligned_buffer_16(roa_u)
  free_aligned_buffer_16(roa_v)
  free_aligned_buffer_16(rob_y)
  free_aligned_buffer_16(rob_u)
  free_aligned_buffer_16(rob_v)

  EXPECT_EQ(0, y_err + uv_err);
}

}  // namespace libyuv
