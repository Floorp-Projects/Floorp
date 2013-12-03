/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include "vpx_scale/vpxscale.h"
#include "vpx_mem/vpx_mem.h"
/****************************************************************************
*  Imports
****************************************************************************/

/****************************************************************************
 *
 *  ROUTINE       : vp8_horizontal_line_4_5_scale_c
 *
 *  INPUTS        : const unsigned char *source : Pointer to source data.
 *                  unsigned int source_width    : Stride of source.
 *                  unsigned char *dest         : Pointer to destination data.
 *                  unsigned int dest_width      : Stride of destination (NOT USED).
 *
 *  OUTPUTS       : None.
 *
 *  RETURNS       : void
 *
 *  FUNCTION      : Copies horizontal line of pixels from source to
 *                  destination scaling up by 4 to 5.
 *
 *  SPECIAL NOTES : None.
 *
 ****************************************************************************/
void vp8_horizontal_line_4_5_scale_c(const unsigned char *source,
                                     unsigned int source_width,
                                     unsigned char *dest,
                                     unsigned int dest_width) {
  unsigned i;
  unsigned int a, b, c;
  unsigned char *des = dest;
  const unsigned char *src = source;

  (void) dest_width;

  for (i = 0; i < source_width - 4; i += 4) {
    a = src[0];
    b = src[1];
    des [0] = (unsigned char) a;
    des [1] = (unsigned char)((a * 51 + 205 * b + 128) >> 8);
    c = src[2] * 154;
    a = src[3];
    des [2] = (unsigned char)((b * 102 + c + 128) >> 8);
    des [3] = (unsigned char)((c + 102 * a + 128) >> 8);
    b = src[4];
    des [4] = (unsigned char)((a * 205 + 51 * b + 128) >> 8);

    src += 4;
    des += 5;
  }

  a = src[0];
  b = src[1];
  des [0] = (unsigned char)(a);
  des [1] = (unsigned char)((a * 51 + 205 * b + 128) >> 8);
  c = src[2] * 154;
  a = src[3];
  des [2] = (unsigned char)((b * 102 + c + 128) >> 8);
  des [3] = (unsigned char)((c + 102 * a + 128) >> 8);
  des [4] = (unsigned char)(a);

}

/****************************************************************************
 *
 *  ROUTINE       : vp8_vertical_band_4_5_scale_c
 *
 *  INPUTS        : unsigned char *dest    : Pointer to destination data.
 *                  unsigned int dest_pitch : Stride of destination data.
 *                  unsigned int dest_width : Width of destination data.
 *
 *  OUTPUTS       : None.
 *
 *  RETURNS       : void
 *
 *  FUNCTION      : Scales vertical band of pixels by scale 4 to 5. The
 *                  height of the band scaled is 4-pixels.
 *
 *  SPECIAL NOTES : The routine uses the first line of the band below
 *                  the current band.
 *
 ****************************************************************************/
void vp8_vertical_band_4_5_scale_c(unsigned char *dest,
                                   unsigned int dest_pitch,
                                   unsigned int dest_width) {
  unsigned int i;
  unsigned int a, b, c, d;
  unsigned char *des = dest;

  for (i = 0; i < dest_width; i++) {
    a = des [0];
    b = des [dest_pitch];

    des[dest_pitch] = (unsigned char)((a * 51 + 205 * b + 128) >> 8);

    c = des[dest_pitch * 2] * 154;
    d = des[dest_pitch * 3];

    des [dest_pitch * 2] = (unsigned char)((b * 102 + c + 128) >> 8);
    des [dest_pitch * 3] = (unsigned char)((c + 102 * d + 128) >> 8);

    /* First line in next band */
    a = des [dest_pitch * 5];
    des [dest_pitch * 4] = (unsigned char)((d * 205 + 51 * a + 128) >> 8);

    des++;
  }
}

/****************************************************************************
 *
 *  ROUTINE       : vp8_last_vertical_band_4_5_scale_c
 *
 *  INPUTS        : unsigned char *dest    : Pointer to destination data.
 *                  unsigned int dest_pitch : Stride of destination data.
 *                  unsigned int dest_width : Width of destination data.
 *
 *  OUTPUTS       : None.
 *
 *  RETURNS       : void
 *
 *  FUNCTION      : Scales last vertical band of pixels by scale 4 to 5. The
 *                  height of the band scaled is 4-pixels.
 *
 *  SPECIAL NOTES : The routine does not have available the first line of
 *                  the band below the current band, since this is the
 *                  last band.
 *
 ****************************************************************************/
void vp8_last_vertical_band_4_5_scale_c(unsigned char *dest,
                                        unsigned int dest_pitch,
                                        unsigned int dest_width) {
  unsigned int i;
  unsigned int a, b, c, d;
  unsigned char *des = dest;

  for (i = 0; i < dest_width; ++i) {
    a = des[0];
    b = des[dest_pitch];

    des[dest_pitch] = (unsigned char)((a * 51 + 205 * b + 128) >> 8);

    c = des[dest_pitch * 2] * 154;
    d = des[dest_pitch * 3];

    des [dest_pitch * 2] = (unsigned char)((b * 102 + c + 128) >> 8);
    des [dest_pitch * 3] = (unsigned char)((c + 102 * d + 128) >> 8);

    /* No other line for interplation of this line, so .. */
    des[dest_pitch * 4] = (unsigned char) d;

    des++;
  }
}

/****************************************************************************
 *
 *  ROUTINE       : vp8_horizontal_line_2_3_scale_c
 *
 *  INPUTS        : const unsigned char *source : Pointer to source data.
 *                  unsigned int source_width    : Stride of source.
 *                  unsigned char *dest         : Pointer to destination data.
 *                  unsigned int dest_width      : Stride of destination (NOT USED).
 *
 *  OUTPUTS       : None.
 *
 *  RETURNS       : void
 *
 *  FUNCTION      : Copies horizontal line of pixels from source to
 *                  destination scaling up by 2 to 3.
 *
 *  SPECIAL NOTES : None.
 *
 *
 ****************************************************************************/
void vp8_horizontal_line_2_3_scale_c(const unsigned char *source,
                                     unsigned int source_width,
                                     unsigned char *dest,
                                     unsigned int dest_width) {
  unsigned int i;
  unsigned int a, b, c;
  unsigned char *des = dest;
  const unsigned char *src = source;

  (void) dest_width;

  for (i = 0; i < source_width - 2; i += 2) {
    a = src[0];
    b = src[1];
    c = src[2];

    des [0] = (unsigned char)(a);
    des [1] = (unsigned char)((a * 85 + 171 * b + 128) >> 8);
    des [2] = (unsigned char)((b * 171 + 85 * c + 128) >> 8);

    src += 2;
    des += 3;
  }

  a = src[0];
  b = src[1];
  des [0] = (unsigned char)(a);
  des [1] = (unsigned char)((a * 85 + 171 * b + 128) >> 8);
  des [2] = (unsigned char)(b);
}


/****************************************************************************
 *
 *  ROUTINE       : vp8_vertical_band_2_3_scale_c
 *
 *  INPUTS        : unsigned char *dest    : Pointer to destination data.
 *                  unsigned int dest_pitch : Stride of destination data.
 *                  unsigned int dest_width : Width of destination data.
 *
 *  OUTPUTS       : None.
 *
 *  RETURNS       : void
 *
 *  FUNCTION      : Scales vertical band of pixels by scale 2 to 3. The
 *                  height of the band scaled is 2-pixels.
 *
 *  SPECIAL NOTES : The routine uses the first line of the band below
 *                  the current band.
 *
 ****************************************************************************/
void vp8_vertical_band_2_3_scale_c(unsigned char *dest,
                                   unsigned int dest_pitch,
                                   unsigned int dest_width) {
  unsigned int i;
  unsigned int a, b, c;
  unsigned char *des = dest;

  for (i = 0; i < dest_width; i++) {
    a = des [0];
    b = des [dest_pitch];
    c = des[dest_pitch * 3];
    des [dest_pitch  ] = (unsigned char)((a * 85 + 171 * b + 128) >> 8);
    des [dest_pitch * 2] = (unsigned char)((b * 171 + 85 * c + 128) >> 8);

    des++;
  }
}

/****************************************************************************
 *
 *  ROUTINE       : vp8_last_vertical_band_2_3_scale_c
 *
 *  INPUTS        : unsigned char *dest    : Pointer to destination data.
 *                  unsigned int dest_pitch : Stride of destination data.
 *                  unsigned int dest_width : Width of destination data.
 *
 *  OUTPUTS       : None.
 *
 *  RETURNS       : void
 *
 *  FUNCTION      : Scales last vertical band of pixels by scale 2 to 3. The
 *                  height of the band scaled is 2-pixels.
 *
 *  SPECIAL NOTES : The routine does not have available the first line of
 *                  the band below the current band, since this is the
 *                  last band.
 *
 ****************************************************************************/
void vp8_last_vertical_band_2_3_scale_c(unsigned char *dest,
                                        unsigned int dest_pitch,
                                        unsigned int dest_width) {
  unsigned int i;
  unsigned int a, b;
  unsigned char *des = dest;

  for (i = 0; i < dest_width; ++i) {
    a = des [0];
    b = des [dest_pitch];

    des [dest_pitch  ] = (unsigned char)((a * 85 + 171 * b + 128) >> 8);
    des [dest_pitch * 2] = (unsigned char)(b);
    des++;
  }
}

/****************************************************************************
 *
 *  ROUTINE       : vp8_horizontal_line_3_5_scale_c
 *
 *  INPUTS        : const unsigned char *source : Pointer to source data.
 *                  unsigned int source_width    : Stride of source.
 *                  unsigned char *dest         : Pointer to destination data.
 *                  unsigned int dest_width      : Stride of destination (NOT USED).
 *
 *  OUTPUTS       : None.
 *
 *  RETURNS       : void
 *
 *  FUNCTION      : Copies horizontal line of pixels from source to
 *                  destination scaling up by 3 to 5.
 *
 *  SPECIAL NOTES : None.
 *
 *
 ****************************************************************************/
void vp8_horizontal_line_3_5_scale_c(const unsigned char *source,
                                     unsigned int source_width,
                                     unsigned char *dest,
                                     unsigned int dest_width) {
  unsigned int i;
  unsigned int a, b, c;
  unsigned char *des = dest;
  const unsigned char *src = source;

  (void) dest_width;

  for (i = 0; i < source_width - 3; i += 3) {
    a = src[0];
    b = src[1];
    des [0] = (unsigned char)(a);
    des [1] = (unsigned char)((a * 102 + 154 * b + 128) >> 8);

    c = src[2];
    des [2] = (unsigned char)((b * 205 + c * 51 + 128) >> 8);
    des [3] = (unsigned char)((b * 51 + c * 205 + 128) >> 8);

    a = src[3];
    des [4] = (unsigned char)((c * 154 + a * 102 + 128) >> 8);

    src += 3;
    des += 5;
  }

  a = src[0];
  b = src[1];
  des [0] = (unsigned char)(a);

  des [1] = (unsigned char)((a * 102 + 154 * b + 128) >> 8);
  c = src[2];
  des [2] = (unsigned char)((b * 205 + c * 51 + 128) >> 8);
  des [3] = (unsigned char)((b * 51 + c * 205 + 128) >> 8);

  des [4] = (unsigned char)(c);
}

/****************************************************************************
 *
 *  ROUTINE       : vp8_vertical_band_3_5_scale_c
 *
 *  INPUTS        : unsigned char *dest    : Pointer to destination data.
 *                  unsigned int dest_pitch : Stride of destination data.
 *                  unsigned int dest_width : Width of destination data.
 *
 *  OUTPUTS       : None.
 *
 *  RETURNS       : void
 *
 *  FUNCTION      : Scales vertical band of pixels by scale 3 to 5. The
 *                  height of the band scaled is 3-pixels.
 *
 *  SPECIAL NOTES : The routine uses the first line of the band below
 *                  the current band.
 *
 ****************************************************************************/
void vp8_vertical_band_3_5_scale_c(unsigned char *dest,
                                   unsigned int dest_pitch,
                                   unsigned int dest_width) {
  unsigned int i;
  unsigned int a, b, c;
  unsigned char *des = dest;

  for (i = 0; i < dest_width; i++) {
    a = des [0];
    b = des [dest_pitch];
    des [dest_pitch] = (unsigned char)((a * 102 + 154 * b + 128) >> 8);

    c = des[dest_pitch * 2];
    des [dest_pitch * 2] = (unsigned char)((b * 205 + c * 51 + 128) >> 8);
    des [dest_pitch * 3] = (unsigned char)((b * 51 + c * 205 + 128) >> 8);

    /* First line in next band... */
    a = des [dest_pitch * 5];
    des [dest_pitch * 4] = (unsigned char)((c * 154 + a * 102 + 128) >> 8);

    des++;
  }
}

/****************************************************************************
 *
 *  ROUTINE       : vp8_last_vertical_band_3_5_scale_c
 *
 *  INPUTS        : unsigned char *dest    : Pointer to destination data.
 *                  unsigned int dest_pitch : Stride of destination data.
 *                  unsigned int dest_width : Width of destination data.
 *
 *  OUTPUTS       : None.
 *
 *  RETURNS       : void
 *
 *  FUNCTION      : Scales last vertical band of pixels by scale 3 to 5. The
 *                  height of the band scaled is 3-pixels.
 *
 *  SPECIAL NOTES : The routine does not have available the first line of
 *                  the band below the current band, since this is the
 *                  last band.
 *
 ****************************************************************************/
void vp8_last_vertical_band_3_5_scale_c(unsigned char *dest,
                                        unsigned int dest_pitch,
                                        unsigned int dest_width) {
  unsigned int i;
  unsigned int a, b, c;
  unsigned char *des = dest;

  for (i = 0; i < dest_width; ++i) {
    a = des [0];
    b = des [dest_pitch];

    des [ dest_pitch ] = (unsigned char)((a * 102 + 154 * b + 128) >> 8);

    c = des[dest_pitch * 2];
    des [dest_pitch * 2] = (unsigned char)((b * 205 + c * 51 + 128) >> 8);
    des [dest_pitch * 3] = (unsigned char)((b * 51 + c * 205 + 128) >> 8);

    /* No other line for interplation of this line, so .. */
    des [ dest_pitch * 4 ] = (unsigned char)(c);

    des++;
  }
}

/****************************************************************************
 *
 *  ROUTINE       : vp8_horizontal_line_3_4_scale_c
 *
 *  INPUTS        : const unsigned char *source : Pointer to source data.
 *                  unsigned int source_width    : Stride of source.
 *                  unsigned char *dest         : Pointer to destination data.
 *                  unsigned int dest_width      : Stride of destination (NOT USED).
 *
 *  OUTPUTS       : None.
 *
 *  RETURNS       : void
 *
 *  FUNCTION      : Copies horizontal line of pixels from source to
 *                  destination scaling up by 3 to 4.
 *
 *  SPECIAL NOTES : None.
 *
 *
 ****************************************************************************/
void vp8_horizontal_line_3_4_scale_c(const unsigned char *source,
                                     unsigned int source_width,
                                     unsigned char *dest,
                                     unsigned int dest_width) {
  unsigned int i;
  unsigned int a, b, c;
  unsigned char *des = dest;
  const unsigned char *src = source;

  (void) dest_width;

  for (i = 0; i < source_width - 3; i += 3) {
    a = src[0];
    b = src[1];
    des [0] = (unsigned char)(a);
    des [1] = (unsigned char)((a * 64 + b * 192 + 128) >> 8);

    c = src[2];
    des [2] = (unsigned char)((b + c + 1) >> 1);

    a = src[3];
    des [3] = (unsigned char)((c * 192 + a * 64 + 128) >> 8);

    src += 3;
    des += 4;
  }

  a = src[0];
  b = src[1];
  des [0] = (unsigned char)(a);
  des [1] = (unsigned char)((a * 64 + b * 192 + 128) >> 8);

  c = src[2];
  des [2] = (unsigned char)((b + c + 1) >> 1);
  des [3] = (unsigned char)(c);
}

/****************************************************************************
 *
 *  ROUTINE       : vp8_vertical_band_3_4_scale_c
 *
 *  INPUTS        : unsigned char *dest    : Pointer to destination data.
 *                  unsigned int dest_pitch : Stride of destination data.
 *                  unsigned int dest_width : Width of destination data.
 *
 *  OUTPUTS       : None.
 *
 *  RETURNS       : void
 *
 *  FUNCTION      : Scales vertical band of pixels by scale 3 to 4. The
 *                  height of the band scaled is 3-pixels.
 *
 *  SPECIAL NOTES : The routine uses the first line of the band below
 *                  the current band.
 *
 ****************************************************************************/
void vp8_vertical_band_3_4_scale_c(unsigned char *dest,
                                   unsigned int dest_pitch,
                                   unsigned int dest_width) {
  unsigned int i;
  unsigned int a, b, c;
  unsigned char *des = dest;

  for (i = 0; i < dest_width; i++) {
    a = des [0];
    b = des [dest_pitch];
    des [dest_pitch]   = (unsigned char)((a * 64 + b * 192 + 128) >> 8);

    c = des[dest_pitch * 2];
    des [dest_pitch * 2] = (unsigned char)((b + c + 1) >> 1);

    /* First line in next band... */
    a = des [dest_pitch * 4];
    des [dest_pitch * 3] = (unsigned char)((c * 192 + a * 64 + 128) >> 8);

    des++;
  }
}

/****************************************************************************
 *
 *  ROUTINE       : vp8_last_vertical_band_3_4_scale_c
 *
 *  INPUTS        : unsigned char *dest    : Pointer to destination data.
 *                  unsigned int dest_pitch : Stride of destination data.
 *                  unsigned int dest_width : Width of destination data.
 *
 *  OUTPUTS       : None.
 *
 *  RETURNS       : void
 *
 *  FUNCTION      : Scales last vertical band of pixels by scale 3 to 4. The
 *                  height of the band scaled is 3-pixels.
 *
 *  SPECIAL NOTES : The routine does not have available the first line of
 *                  the band below the current band, since this is the
 *                  last band.
 *
 ****************************************************************************/
void vp8_last_vertical_band_3_4_scale_c(unsigned char *dest,
                                        unsigned int dest_pitch,
                                        unsigned int dest_width) {
  unsigned int i;
  unsigned int a, b, c;
  unsigned char *des = dest;

  for (i = 0; i < dest_width; ++i) {
    a = des [0];
    b = des [dest_pitch];

    des [dest_pitch]   = (unsigned char)((a * 64 + b * 192 + 128) >> 8);

    c = des[dest_pitch * 2];
    des [dest_pitch * 2] = (unsigned char)((b + c + 1) >> 1);

    /* No other line for interplation of this line, so .. */
    des [dest_pitch * 3] = (unsigned char)(c);

    des++;
  }
}

/****************************************************************************
 *
 *  ROUTINE       : vp8_horizontal_line_1_2_scale_c
 *
 *  INPUTS        : const unsigned char *source : Pointer to source data.
 *                  unsigned int source_width    : Stride of source.
 *                  unsigned char *dest         : Pointer to destination data.
 *                  unsigned int dest_width      : Stride of destination (NOT USED).
 *
 *  OUTPUTS       : None.
 *
 *  RETURNS       : void
 *
 *  FUNCTION      : Copies horizontal line of pixels from source to
 *                  destination scaling up by 1 to 2.
 *
 *  SPECIAL NOTES : None.
 *
 ****************************************************************************/
void vp8_horizontal_line_1_2_scale_c(const unsigned char *source,
                                     unsigned int source_width,
                                     unsigned char *dest,
                                     unsigned int dest_width) {
  unsigned int i;
  unsigned int a, b;
  unsigned char *des = dest;
  const unsigned char *src = source;

  (void) dest_width;

  for (i = 0; i < source_width - 1; i += 1) {
    a = src[0];
    b = src[1];
    des [0] = (unsigned char)(a);
    des [1] = (unsigned char)((a + b + 1) >> 1);
    src += 1;
    des += 2;
  }

  a = src[0];
  des [0] = (unsigned char)(a);
  des [1] = (unsigned char)(a);
}

/****************************************************************************
 *
 *  ROUTINE       : vp8_vertical_band_1_2_scale_c
 *
 *  INPUTS        : unsigned char *dest    : Pointer to destination data.
 *                  unsigned int dest_pitch : Stride of destination data.
 *                  unsigned int dest_width : Width of destination data.
 *
 *  OUTPUTS       : None.
 *
 *  RETURNS       : void
 *
 *  FUNCTION      : Scales vertical band of pixels by scale 1 to 2. The
 *                  height of the band scaled is 1-pixel.
 *
 *  SPECIAL NOTES : The routine uses the first line of the band below
 *                  the current band.
 *
 ****************************************************************************/
void vp8_vertical_band_1_2_scale_c(unsigned char *dest,
                                   unsigned int dest_pitch,
                                   unsigned int dest_width) {
  unsigned int i;
  unsigned int a, b;
  unsigned char *des = dest;

  for (i = 0; i < dest_width; i++) {
    a = des [0];
    b = des [dest_pitch * 2];

    des[dest_pitch] = (unsigned char)((a + b + 1) >> 1);

    des++;
  }
}

/****************************************************************************
 *
 *  ROUTINE       : vp8_last_vertical_band_1_2_scale_c
 *
 *  INPUTS        : unsigned char *dest    : Pointer to destination data.
 *                  unsigned int dest_pitch : Stride of destination data.
 *                  unsigned int dest_width : Width of destination data.
 *
 *  OUTPUTS       : None.
 *
 *  RETURNS       : void
 *
 *  FUNCTION      : Scales last vertical band of pixels by scale 1 to 2. The
 *                  height of the band scaled is 1-pixel.
 *
 *  SPECIAL NOTES : The routine does not have available the first line of
 *                  the band below the current band, since this is the
 *                  last band.
 *
 ****************************************************************************/
void vp8_last_vertical_band_1_2_scale_c(unsigned char *dest,
                                        unsigned int dest_pitch,
                                        unsigned int dest_width) {
  unsigned int i;
  unsigned char *des = dest;

  for (i = 0; i < dest_width; ++i) {
    des[dest_pitch] = des[0];
    des++;
  }
}





/****************************************************************************
 *
 *  ROUTINE       : vp8_horizontal_line_4_5_scale_c
 *
 *  INPUTS        : const unsigned char *source : Pointer to source data.
 *                  unsigned int source_width    : Stride of source.
 *                  unsigned char *dest         : Pointer to destination data.
 *                  unsigned int dest_width      : Stride of destination (NOT USED).
 *
 *  OUTPUTS       : None.
 *
 *  RETURNS       : void
 *
 *  FUNCTION      : Copies horizontal line of pixels from source to
 *                  destination scaling up by 4 to 5.
 *
 *  SPECIAL NOTES : None.
 *
 ****************************************************************************/
void vp8_horizontal_line_5_4_scale_c(const unsigned char *source,
                                     unsigned int source_width,
                                     unsigned char *dest,
                                     unsigned int dest_width) {
  unsigned i;
  unsigned int a, b, c, d, e;
  unsigned char *des = dest;
  const unsigned char *src = source;

  (void) dest_width;

  for (i = 0; i < source_width; i += 5) {
    a = src[0];
    b = src[1];
    c = src[2];
    d = src[3];
    e = src[4];

    des[0] = (unsigned char) a;
    des[1] = (unsigned char)((b * 192 + c * 64 + 128) >> 8);
    des[2] = (unsigned char)((c * 128 + d * 128 + 128) >> 8);
    des[3] = (unsigned char)((d * 64 + e * 192 + 128) >> 8);

    src += 5;
    des += 4;
  }
}




void vp8_vertical_band_5_4_scale_c(unsigned char *source,
                                   unsigned int src_pitch,
                                   unsigned char *dest,
                                   unsigned int dest_pitch,
                                   unsigned int dest_width) {
  unsigned int i;
  unsigned int a, b, c, d, e;
  unsigned char *des = dest;
  unsigned char *src = source;

  for (i = 0; i < dest_width; i++) {

    a = src[0 * src_pitch];
    b = src[1 * src_pitch];
    c = src[2 * src_pitch];
    d = src[3 * src_pitch];
    e = src[4 * src_pitch];

    des[0 * dest_pitch] = (unsigned char) a;
    des[1 * dest_pitch] = (unsigned char)((b * 192 + c * 64 + 128) >> 8);
    des[2 * dest_pitch] = (unsigned char)((c * 128 + d * 128 + 128) >> 8);
    des[3 * dest_pitch] = (unsigned char)((d * 64 + e * 192 + 128) >> 8);

    src++;
    des++;

  }
}


/*7***************************************************************************
 *
 *  ROUTINE       : vp8_horizontal_line_3_5_scale_c
 *
 *  INPUTS        : const unsigned char *source : Pointer to source data.
 *                  unsigned int source_width    : Stride of source.
 *                  unsigned char *dest         : Pointer to destination data.
 *                  unsigned int dest_width      : Stride of destination (NOT USED).
 *
 *  OUTPUTS       : None.
 *
 *  RETURNS       : void
 *
 *  FUNCTION      : Copies horizontal line of pixels from source to
 *                  destination scaling up by 3 to 5.
 *
 *  SPECIAL NOTES : None.
 *
 *
 ****************************************************************************/
void vp8_horizontal_line_5_3_scale_c(const unsigned char *source,
                                     unsigned int source_width,
                                     unsigned char *dest,
                                     unsigned int dest_width) {
  unsigned int i;
  unsigned int a, b, c, d, e;
  unsigned char *des = dest;
  const unsigned char *src = source;

  (void) dest_width;

  for (i = 0; i < source_width; i += 5) {
    a = src[0];
    b = src[1];
    c = src[2];
    d = src[3];
    e = src[4];

    des[0] = (unsigned char) a;
    des[1] = (unsigned char)((b * 85  + c * 171 + 128) >> 8);
    des[2] = (unsigned char)((d * 171 + e * 85 + 128) >> 8);

    src += 5;
    des += 3;
  }

}

void vp8_vertical_band_5_3_scale_c(unsigned char *source,
                                   unsigned int src_pitch,
                                   unsigned char *dest,
                                   unsigned int dest_pitch,
                                   unsigned int dest_width) {
  unsigned int i;
  unsigned int a, b, c, d, e;
  unsigned char *des = dest;
  unsigned char *src = source;

  for (i = 0; i < dest_width; i++) {

    a = src[0 * src_pitch];
    b = src[1 * src_pitch];
    c = src[2 * src_pitch];
    d = src[3 * src_pitch];
    e = src[4 * src_pitch];

    des[0 * dest_pitch] = (unsigned char) a;
    des[1 * dest_pitch] = (unsigned char)((b * 85 + c * 171 + 128) >> 8);
    des[2 * dest_pitch] = (unsigned char)((d * 171 + e * 85 + 128) >> 8);

    src++;
    des++;

  }
}

/****************************************************************************
 *
 *  ROUTINE       : vp8_horizontal_line_1_2_scale_c
 *
 *  INPUTS        : const unsigned char *source : Pointer to source data.
 *                  unsigned int source_width    : Stride of source.
 *                  unsigned char *dest         : Pointer to destination data.
 *                  unsigned int dest_width      : Stride of destination (NOT USED).
 *
 *  OUTPUTS       : None.
 *
 *  RETURNS       : void
 *
 *  FUNCTION      : Copies horizontal line of pixels from source to
 *                  destination scaling up by 1 to 2.
 *
 *  SPECIAL NOTES : None.
 *
 ****************************************************************************/
void vp8_horizontal_line_2_1_scale_c(const unsigned char *source,
                                     unsigned int source_width,
                                     unsigned char *dest,
                                     unsigned int dest_width) {
  unsigned int i;
  unsigned int a;
  unsigned char *des = dest;
  const unsigned char *src = source;

  (void) dest_width;

  for (i = 0; i < source_width; i += 2) {
    a = src[0];
    des [0] = (unsigned char)(a);
    src += 2;
    des += 1;
  }
}

void vp8_vertical_band_2_1_scale_c(unsigned char *source,
                                   unsigned int src_pitch,
                                   unsigned char *dest,
                                   unsigned int dest_pitch,
                                   unsigned int dest_width) {
  (void) dest_pitch;
  (void) src_pitch;
  vpx_memcpy(dest, source, dest_width);
}

void vp8_vertical_band_2_1_scale_i_c(unsigned char *source,
                                     unsigned int src_pitch,
                                     unsigned char *dest,
                                     unsigned int dest_pitch,
                                     unsigned int dest_width) {
  int i;
  int temp;
  int width = dest_width;

  (void) dest_pitch;

  for (i = 0; i < width; i++) {
    temp = 8;
    temp += source[i - (int)src_pitch] * 3;
    temp += source[i] * 10;
    temp += source[i + src_pitch] * 3;
    temp >>= 4;
    dest[i] = (unsigned char)(temp);
  }
}
