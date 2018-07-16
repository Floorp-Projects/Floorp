#include "av1/common/common.h"
#include "av1/common/enums.h"

#define DWT_MAX_LENGTH 64

void av1_fdwt8x8(tran_low_t *input, tran_low_t *output, int stride);
void av1_fdwt8x8_uint8_input_c(uint8_t *input, tran_low_t *output, int stride,
                               int hbd);
int av1_haar_ac_sad_8x8_uint8_input(uint8_t *input, int stride, int hbd);
