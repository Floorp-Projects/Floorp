/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZZCONF_H
#define MOZZCONF_H

#include "mozilla/Types.h"

#if defined(ZLIB_IN_MOZGLUE)
#define ZEXTERN MFBT_API
#endif

/* Exported Symbols */
#define zlibVersion MOZ_Z_zlibVersion
#define deflate MOZ_Z_deflate
#define deflateEnd MOZ_Z_deflateEnd
#define inflate MOZ_Z_inflate
#define inflateEnd MOZ_Z_inflateEnd
#define deflateSetDictionary MOZ_Z_deflateSetDictionary
#define deflateCopy MOZ_Z_deflateCopy
#define deflateReset MOZ_Z_deflateReset
#define deflateParams MOZ_Z_deflateParams
#define deflateBound MOZ_Z_deflateBound
#define deflatePrime MOZ_Z_deflatePrime
#define inflateSetDictionary MOZ_Z_inflateSetDictionary
#define inflateSync MOZ_Z_inflateSync
#define inflateCopy MOZ_Z_inflateCopy
#define inflateReset MOZ_Z_inflateReset
#define inflateBack MOZ_Z_inflateBack
#define inflateBackEnd MOZ_Z_inflateBackEnd
#define zlibCompileFlags MOZ_Z_zlibCompileFlags
#define compress MOZ_Z_compress
#define compress2 MOZ_Z_compress2
#define compressBound MOZ_Z_compressBound
#define uncompress MOZ_Z_uncompress
#define gzopen MOZ_Z_gzopen
#define gzdopen MOZ_Z_gzdopen
#define gzsetparams MOZ_Z_gzsetparams
#define gzread MOZ_Z_gzread
#define gzwrite MOZ_Z_gzwrite
#define gzprintf MOZ_Z_gzprintf
#define gzputs MOZ_Z_gzputs
#define gzgets MOZ_Z_gzgets
#define gzputc MOZ_Z_gzputc
#define gzungetc MOZ_Z_gzungetc
#define gzflush MOZ_Z_gzflush
#define gzseek MOZ_Z_gzseek
#define gzrewind MOZ_Z_gzrewind
#define gztell MOZ_Z_gztell
#define gzeof MOZ_Z_gzeof
#define gzclose MOZ_Z_gzclose
#define gzerror MOZ_Z_gzerror
#define gzclearerr MOZ_Z_gzclearerr
#define adler32 MOZ_Z_adler32
#define crc32 MOZ_Z_crc32
#define deflateInit_ MOZ_Z_deflateInit_
#define deflateInit2_ MOZ_Z_deflateInit2_
#define inflateInit_ MOZ_Z_inflateInit_
#define inflateInit2_ MOZ_Z_inflateInit2_
#define inflateBackInit_ MOZ_Z_inflateBackInit_
#define inflateSyncPoint MOZ_Z_inflateSyncPoint
#define get_crc_table MOZ_Z_get_crc_table
#define zError MOZ_Z_zError

/* Extra global symbols */
#define _dist_code MOZ_Z__dist_code
#define _length_code MOZ_Z__length_code
#define _tr_align MOZ_Z__tr_align
#define _tr_flush_block MOZ_Z__tr_flush_block
#define _tr_init MOZ_Z__tr_init
#define _tr_stored_block MOZ_Z__tr_stored_block
#define _tr_tally MOZ_Z__tr_tally
#define deflate_copyright MOZ_Z_deflate_copyright
#define inflate_copyright MOZ_Z_inflate_copyright
#define inflate_fast MOZ_Z_inflate_fast
#define inflate_table MOZ_Z_inflate_table
#define z_errmsg MOZ_Z_z_errmsg
#define zcalloc MOZ_Z_zcalloc
#define zcfree MOZ_Z_zcfree
#define alloc_func MOZ_Z_alloc_func
#define free_func MOZ_Z_free_func
#define in_func MOZ_Z_in_func
#define out_func MOZ_Z_out_func

/* New as of zlib-1.2.3 */
#define adler32_combine MOZ_Z_adler32_combine
#define crc32_combine MOZ_Z_crc32_combine
#define deflateSetHeader MOZ_Z_deflateSetHeader
#define deflateTune MOZ_Z_deflateTune
#define gzdirect MOZ_Z_gzdirect
#define inflatePrime MOZ_Z_inflatePrime
#define inflateGetHeader MOZ_Z_inflateGetHeader

/* New as of zlib-1.2.4 */
#define adler32_combine64 MOZ_Z_adler32_combine64
#define crc32_combine64 MOZ_Z_crc32_combine64
#define gz_error MOZ_Z_gz_error
#define gz_intmax MOZ_Z_gz_intmax
#define gz_strwinerror MOZ_Z_gz_strwinerror
#define gzbuffer MOZ_Z_gzbuffer
#define gzclose_r MOZ_Z_gzclose_r
#define gzclose_w MOZ_Z_gzclose_w
#define gzoffset MOZ_Z_gzoffset
#define gzoffset64 MOZ_Z_gzoffset64
#define gzopen64 MOZ_Z_gzopen64
#define gzseek64 MOZ_Z_gzseek64
#define gztell64 MOZ_Z_gztell64
#define inflateMark MOZ_Z_inflateMark
#define inflateReset2 MOZ_Z_inflateReset2
#define inflateUndermine MOZ_Z_inflateUndermine

/* New as of zlib-1.2.6 */
#define deflatePending MOZ_Z_deflatePending
#define deflateResetKeep MOZ_Z_deflateResetKeep
#define inflateResetKeep MOZ_Z_inflateResetKeep
#define gzgetc_ MOZ_Z_gzgetc_

/* New as of zlib-1.2.7 */
#define gzopen_w MOZ_Z_gzopen_w

/* New as of zlib-1.2.8 */
#define _tr_flush_bits MOZ_Z__tr_flush_bits
#define gzvprintf MOZ_Z_gzvprintf
#define inflateGetDictionary MOZ_Z_inflateGetDictionary

/* New as of zlib-1.2.11 */
#define adler32_combine_ MOZ_Z_adler32_combine_
#define crc32_combine_ MOZ_Z_crc32_combine_
#define deflate_fast MOZ_Z_deflate_fast
#define deflate_slow MOZ_Z_deflate_slow
#define deflateStateCheck MOZ_Z_deflateStateCheck
#define deflate_stored MOZ_Z_deflate_stored
#define fill_window MOZ_Z_fill_window
#define flush_pending MOZ_Z_flush_pending
#define longest_match MOZ_Z_longest_match
#define read_buf MOZ_Z_read_buf
#define slide_hash MOZ_Z_slide_hash
#define gz_open MOZ_Z_gz_open
#define gz_reset MOZ_Z_gz_reset
#define gz_avail MOZ_Z_gz_avail
#define gz_fetch MOZ_Z_gz_fetch
#define gz_decomp MOZ_Z_gz_decomp
#define gz_write MOZ_Z_gz_write
#define gz_comp MOZ_Z_gz_comp
#define gz_init MOZ_Z_gz_init
#define gz_write MOZ_Z_gz_write
#define gz_zero MOZ_Z_gz_zero
#define gz_load MOZ_Z_gz_load
#define gz_look MOZ_Z_gz_look
#define gz_read MOZ_Z_gz_read
#define gz_skip MOZ_Z_gz_skip
#define syncsearch MOZ_Z_syncsearch
#define updatewindow MOZ_Z_updatewindow
#define inflateStateCheck MOZ_Z_inflateStateCheck
#define bi_flush MOZ_Z_bi_flush
#define bi_windup MOZ_Z_bi_windup
#define bl_order MOZ_Z_bl_order
#define build_tree MOZ_Z_build_tree
#define compress_block MOZ_Z_compress_block
#define init_block MOZ_Z_init_block
#define pqdownheap MOZ_Z_pqdownheap
#define scan_tree MOZ_Z_scan_tree
#define send_tree MOZ_Z_send_tree
#define slide_hash MOZ_Z_slide_hash
#define uncompress2 MOZ_Z_uncompress2

#endif
