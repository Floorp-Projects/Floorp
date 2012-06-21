/*
 * Written by Wilco Dijkstra, 1996. Refer to file LICENSE under
 * trunk/third_party_mods/sqrt_floor.
 *
 * Minor modifications in code style for WebRTC, 2012.
 */

#include "signal_processing_library.h"

/*
 * Algorithm:
 * Successive approximation of the equation (root + delta) ^ 2 = N
 * until delta < 1. If delta < 1 we have the integer part of SQRT (N).
 * Use delta = 2^i for i = 15 .. 0.
 *
 * Output precision is 16 bits. Note for large input values (close to
 * 0x7FFFFFFF), bit 15 (the highest bit of the low 16-bit half word)
 * contains the MSB information (a non-sign value). Do with caution
 * if you need to cast the output to int16_t type.
 *
 * If the input value is negative, it returns 0.
 */

#define WEBRTC_SPL_SQRT_ITER(N)                 \
  try1 = root + (1 << (N));                     \
  if (value >= try1 << (N))                     \
  {                                             \
    value -= try1 << (N);                       \
    root |= 2 << (N);                           \
  }

int32_t WebRtcSpl_SqrtFloor(int32_t value)
{
  int32_t root = 0, try1;

  WEBRTC_SPL_SQRT_ITER (15);
  WEBRTC_SPL_SQRT_ITER (14);
  WEBRTC_SPL_SQRT_ITER (13);
  WEBRTC_SPL_SQRT_ITER (12);
  WEBRTC_SPL_SQRT_ITER (11);
  WEBRTC_SPL_SQRT_ITER (10);
  WEBRTC_SPL_SQRT_ITER ( 9);
  WEBRTC_SPL_SQRT_ITER ( 8);
  WEBRTC_SPL_SQRT_ITER ( 7);
  WEBRTC_SPL_SQRT_ITER ( 6);
  WEBRTC_SPL_SQRT_ITER ( 5);
  WEBRTC_SPL_SQRT_ITER ( 4);
  WEBRTC_SPL_SQRT_ITER ( 3);
  WEBRTC_SPL_SQRT_ITER ( 2);
  WEBRTC_SPL_SQRT_ITER ( 1);
  WEBRTC_SPL_SQRT_ITER ( 0);

  return root >> 1;
}
