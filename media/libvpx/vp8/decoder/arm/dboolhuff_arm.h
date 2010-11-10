#ifndef DBOOLHUFF_ARM_H
#define DBOOLHUFF_ARM_H

/* JLK
 * There are currently no arm-optimized versions of
 * these functions. As they are implemented, they
 * can be uncommented below and added to
 * arm/dsystemdependent.c
 *
 * The existing asm code is likely so different as
 * to be useless. However, its been left (for now)
 * for reference.
 */
#if 0
#if HAVE_ARMV6
#undef vp8_dbool_start
#define vp8_dbool_start vp8dx_start_decode_v6

#undef vp8_dbool_fill
#define vp8_dbool_fill vp8_bool_decoder_fill_v6

#undef vp8_dbool_debool
#define vp8_dbool_debool vp8_decode_bool_v6

#undef vp8_dbool_devalue
#define vp8_dbool_devalue vp8_decode_value_v6
#endif /* HAVE_ARMV6 */

#if HAVE_ARMV7
#undef vp8_dbool_start
#define vp8_dbool_start vp8dx_start_decode_neon

#undef vp8_dbool_fill
#define vp8_dbool_fill vp8_bool_decoder_fill_neon

#undef vp8_dbool_debool
#define vp8_dbool_debool vp8_decode_bool_neon

#undef vp8_dbool_devalue
#define vp8_dbool_devalue vp8_decode_value_neon
#endif /* HAVE_ARMV7 */
#endif
#endif /* DBOOLHUFF_ARM_H */
