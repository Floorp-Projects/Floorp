/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZPNGCONF_H
#define MOZPNGCONF_H
#define PNGLCONF_H /* So we don't try to use libpng's pnglibconf.h */

#define PNG_API_RULE 0
#define PNG_COST_SHIFT 3
#define PNG_GAMMA_THRESHOLD_FIXED 5000
#define PNG_MAX_GAMMA_8 11
#define PNG_USER_CHUNK_CACHE_MAX 128
#define PNG_USER_CHUNK_MALLOC_MAX 4000000L
#define PNG_USER_HEIGHT_MAX 1000000
#define PNG_USER_WIDTH_MAX 1000000
#define PNG_WEIGHT_SHIFT 8
#define PNG_ZBUF_SIZE 8192
#define PNG_IDAT_READ_SIZE PNG_ZBUF_SIZE
#define PNG_INFLATE_BUF_SIZE 1024
#define PNG_Z_DEFAULT_COMPRESSION (-1)
#define PNG_Z_DEFAULT_NOFILTER_STRATEGY 0
#define PNG_Z_DEFAULT_STRATEGY 1

#ifdef _MSC_VER
/* The PNG_PEDANTIC_WARNINGS (attributes) fail to build with some MSC
 * compilers; we'll play it safe and disable them for all MSC compilers.
 */
#define PNG_NO_PEDANTIC_WARNINGS
#endif

#undef PNG_ARM_NEON_OPT /* This may have been defined in pngpriv.h */
#define PNG_ARM_NEON_OPT 0

#define PNG_BENIGN_READ_ERRORS_SUPPORTED
#define PNG_READ_SUPPORTED
#define PNG_READ_APNG_SUPPORTED
#define PNG_READ_cHRM_SUPPORTED
#define PNG_READ_gAMA_SUPPORTED
#define PNG_READ_iCCP_SUPPORTED
#define PNG_READ_sRGB_SUPPORTED
#define PNG_READ_tRNS_SUPPORTED
#define PNG_READ_16BIT_SUPPORTED
#define PNG_READ_ANCILLARY_CHUNKS_SUPPORTED
#define PNG_READ_COMPOSITE_NODIV_SUPPORTED
#define PNG_READ_COMPRESSED_TEXT_SUPPORTED
#define PNG_READ_EXPAND_SUPPORTED
#define PNG_READ_GAMMA_SUPPORTED
#define PNG_READ_GRAY_TO_RGB_SUPPORTED
#define PNG_READ_INTERLACING_SUPPORTED
#define PNG_READ_SCALE_16_TO_8_SUPPORTED
#define PNG_READ_TRANSFORMS_SUPPORTED

/* necessary for boot animation code */
#ifdef MOZ_WIDGET_GONK
#define PNG_UNKNOWN_CHUNKS_SUPPORTED
#define PNG_SET_UNKNOWN_CHUNKS_SUPPORTED
#define PNG_HANDLE_AS_UNKNOWN_SUPPORTED
#define PNG_EASY_ACCESS_SUPPORTED
#define PNG_READ_BGR_SUPPORTED
#define PNG_BENIGN_READ_ERRORS_SUPPORTED
#define PNG_READ_EXPAND_SUPPORTED
#define PNG_READ_FILLER_SUPPORTED
#define PNG_READ_GRAY_TO_RGB_SUPPORTED
#define PNG_READ_STRIP_16_TO_8_SUPPORTED
#define PNG_READ_STRIP_ALPHA_SUPPORTED
#define PNG_READ_USER_TRANSFORM_SUPPORTED
#define PNG_SEQUENTIAL_READ_SUPPORTED
#endif

#define PNG_WRITE_SUPPORTED
#define PNG_WRITE_APNG_SUPPORTED
#define PNG_WRITE_tRNS_SUPPORTED
#define PNG_WRITE_16BIT_SUPPORTED
#define PNG_WRITE_ANCILLARY_CHUNKS_SUPPORTED
#define PNG_WRITE_FLUSH_SUPPORTED
#define PNG_WRITE_OPTIMIZE_CMF_SUPPORTED
#define PNG_WRITE_INT_FUNCTIONS_SUPPORTED

#define PNG_APNG_SUPPORTED
#define PNG_BENIGN_ERRORS_SUPPORTED
#define PNG_cHRM_SUPPORTED
#define PNG_COLORSPACE_SUPPORTED
#define PNG_gAMA_SUPPORTED
#define PNG_GAMMA_SUPPORTED
#define PNG_iCCP_SUPPORTED
#define PNG_sRGB_SUPPORTED
#define PNG_tRNS_SUPPORTED
#define PNG_16BIT_SUPPORTED
#define PNG_CHECK_cHRM_SUPPORTED
#define PNG_FIXED_POINT_SUPPORTED
#define PNG_FLOATING_ARITHMETIC_SUPPORTED
#define PNG_FLOATING_POINT_SUPPORTED
#define PNG_POINTER_INDEXING_SUPPORTED
#define PNG_PROGRESSIVE_READ_SUPPORTED
#define PNG_SETJMP_SUPPORTED
#define PNG_STDIO_SUPPORTED
#define PNG_TEXT_SUPPORTED

#ifdef PR_LOGGING
#define PNG_ERROR_TEXT_SUPPORTED
#define PNG_WARNINGS_SUPPORTED
#endif

/* Mangle names of exported libpng functions so different libpng versions
   can coexist. It is recommended that if you do this, you give your
   library a different name such as "mozlibpng" instead of "libpng". */

/* The following has been present since libpng-0.88, has never changed, and
   is unaffected by conditional compilation macros.  It will not be mangled
   and it is the only choice for use in configure scripts for detecting the
   presence of any libpng version since 0.88.

   png_get_io_ptr
*/

/* Mozilla: mangle it anyway. */
#define png_get_io_ptr                  MOZ_PNG_get_io_ptr

/* The following weren't present in libpng-0.88 but have never changed since
   they were first introduced and are not affected by any conditional compile
   choices and therefore don't need to be mangled.  We'll mangle them anyway. */
#define png_sig_cmp                     MOZ_PNG_sig_cmp
#define png_access_version_number       MOZ_PNG_access_vn

/* These have never changed since they were first introduced but they
   make direct reference to members of png_ptr that might have been moved,
   so they will be mangled. */

#define png_set_sig_bytes               MOZ_PNG_set_sig_b
#define png_reset_zstream               MOZ_PNG_reset_zs

/* The following may have changed, or can be affected by conditional compilation
   choices, and will be mangled. */
#define png_64bit_product               MOZ_PNG_64bit_product
#define png_build_gamma_table           MOZ_PNG_build_gamma_tab
#define png_build_grayscale_palette     MOZ_PNG_build_g_p
#define png_calculate_crc               MOZ_PNG_calc_crc
#define png_calloc                      MOZ_PNG_calloc
#define png_check_cHRM_fixed            MOZ_PNG_ck_cHRM_fixed
#define png_check_chunk_name            MOZ_PNG_ck_chunk_name
#define png_check_IHDR                  MOZ_PNG_ck_IHDR
#define png_check_keyword               MOZ_PNG_ck_keyword
#define png_combine_row                 MOZ_PNG_combine_row
#define png_convert_from_struct_tm      MOZ_PNG_cv_from_struct_tm
#define png_convert_from_time_t         MOZ_PNG_cv_from_time_t
#define png_convert_to_rfc1123          MOZ_PNG_cv_to_rfc1123
#define png_correct_palette             MOZ_PNG_correct_palette
#define png_crc_error                   MOZ_PNG_crc_error
#define png_crc_finish                  MOZ_PNG_crc_finish
#define png_crc_read                    MOZ_PNG_crc_read
#define png_create_info_struct          MOZ_PNG_cr_info_str
#define png_create_read_struct          MOZ_PNG_cr_read_str
#define png_create_read_struct_2        MOZ_PNG_cr_read_str_2
#define png_create_struct               MOZ_PNG_create_st
#define png_create_struct_2             MOZ_PNG_create_s2
#define png_create_write_struct         MOZ_PNG_cr_write_str
#define png_create_write_struct_2       MOZ_PNG_cr_write_str_2
#define png_data_freer                  MOZ_PNG_data_freer
#define png_decompress_chunk            MOZ_PNG_decomp_chunk
#define png_default_error               MOZ_PNG_def_error
#define png_default_flush               MOZ_PNG_def_flush
#define png_default_read_data           MOZ_PNG_def_read_data
#define png_default_warning             MOZ_PNG_def_warning
#define png_default_write_data          MOZ_PNG_default_write_data
#define png_destroy_info_struct         MOZ_PNG_dest_info_str
#define png_destroy_read_struct         MOZ_PNG_dest_read_str
#define png_destroy_struct              MOZ_PNG_dest_str
#define png_destroy_struct_2            MOZ_PNG_dest_str_2
#define png_destroy_write_struct        MOZ_PNG_dest_write_str
#define png_digit                       MOZ_PNG_digit
#define png_do_bgr                      MOZ_PNG_do_bgr
#define png_do_chop                     MOZ_PNG_do_chop
#define png_do_expand                   MOZ_PNG_do_expand
#define png_do_expand_palette           MOZ_PNG_do_expand_plte
#define png_do_gamma                    MOZ_PNG_do_gamma
#define png_do_gray_to_rgb              MOZ_PNG_do_g_to_rgb
#define png_do_invert                   MOZ_PNG_do_invert
#define png_do_packswap                 MOZ_PNG_do_packswap
#define png_do_read_filler              MOZ_PNG_do_read_fill
#define png_do_read_interlace           MOZ_PNG_do_read_int
#define png_do_read_intrapixel          MOZ_PNG_do_read_intra
#define png_do_read_invert_alpha        MOZ_PNG_do_read_inv_alph
#define png_do_read_swap_alpha          MOZ_PNG_do_read_swap_alph
#define png_do_read_transformations     MOZ_PNG_do_read_trans
#define png_do_rgb_to_gray              MOZ_PNG_do_rgb_to_g
#define png_do_shift                    MOZ_PNG_do_shift
#define png_do_swap                     MOZ_PNG_do_swap
#define png_do_unpack                   MOZ_PNG_do_unpack
#define png_do_unshift                  MOZ_PNG_do_unshift
#define png_do_write_interlace          MOZ_PNG_do_write_interlace
#define png_do_write_intrapixel         MOZ_PNG_do_write_intrapixel
#define png_do_write_invert_alpha       MOZ_PNG_do_write_invert_alpha
#define png_do_write_swap_alpha         MOZ_PNG_do_write_swap_alpha
#define png_do_write_transformations    MOZ_PNG_do_write_trans
#define png_err                         MOZ_PNG_err
#define png_far_to_near                 MOZ_PNG_far_to_near
#define png_fixed                       MOZ_PNG_fixed
#define png_flush                       MOZ_PNG_flush
#define png_format_buffer               MOZ_PNG_format_buf
#define png_free                        MOZ_PNG_free
#define png_free_data                   MOZ_PNG_free_data
#define png_free_default                MOZ_PNG_free_def
#define png_get_bit_depth               MOZ_PNG_get_bit_depth
#define png_get_bKGD                    MOZ_PNG_get_bKGD
#define png_get_channels                MOZ_PNG_get_channels
#define png_get_cHRM                    MOZ_PNG_get_cHRM
#define png_get_cHRM_fixed              MOZ_PNG_get_cHRM_fixed
#define png_get_color_type              MOZ_PNG_get_color_type
#define png_get_compression_buffer_size MOZ_PNG_get_comp_buf_siz
#define png_get_compression_type        MOZ_PNG_get_comp_type
#define png_get_copyright               MOZ_PNG_get_copyright
#define png_get_error_ptr               MOZ_PNG_get_error_ptr
#define png_get_filter_type             MOZ_PNG_get_filter_type
#define png_get_gAMA                    MOZ_PNG_get_gAMA
#define png_get_gAMA_fixed              MOZ_PNG_get_gAMA_fixed
#define png_get_header_ver              MOZ_PNG_get_hdr_ver
#define png_get_header_version          MOZ_PNG_get_hdr_vn
#define png_get_hIST                    MOZ_PNG_get_hIST
#define png_get_iCCP                    MOZ_PNG_get_iCCP
#define png_get_IHDR                    MOZ_PNG_get_IHDR
#define png_get_image_height            MOZ_PNG_get_image_h
#define png_get_image_width             MOZ_PNG_get_image_w
#define png_get_interlace_type          MOZ_PNG_get_interlace_type
#define png_get_libpng_ver              MOZ_PNG_get_libpng_ver
#define png_get_mem_ptr                 MOZ_PNG_get_mem_ptr
#define png_get_oFFs                    MOZ_PNG_get_oFFs
#define png_get_pCAL                    MOZ_PNG_get_pCAL
#define png_get_pHYs                    MOZ_PNG_get_pHYs
#define png_get_pHYs_dpi                MOZ_PNG_get_pHYs_dpi
#define png_get_pixel_aspect_ratio      MOZ_PNG_get_pixel_aspect_ratio
#define png_get_pixels_per_inch         MOZ_PNG_get_pixels_per_inch
#define png_get_pixels_per_meter        MOZ_PNG_get_pixels_p_m
#define png_get_PLTE                    MOZ_PNG_get_PLTE
#define png_get_progressive_ptr         MOZ_PNG_get_progressive_ptr
#define png_get_rgb_to_gray_status      MOZ_PNG_get_rgb_to_gray_status
#define png_get_rowbytes                MOZ_PNG_get_rowbytes
#define png_get_rows                    MOZ_PNG_get_rows
#define png_get_sBIT                    MOZ_PNG_get_sBIT
#define png_get_sCAL                    MOZ_PNG_get_sCAL
#define png_get_sCAL_s                  MOZ_PNG_get_sCAL_s
#define png_get_signature               MOZ_PNG_get_signature
#define png_get_sPLT                    MOZ_PNG_get_sPLT
#define png_get_sRGB                    MOZ_PNG_get_sRGB
#define png_get_text                    MOZ_PNG_get_text
#define png_get_tIME                    MOZ_PNG_get_tIME
#define png_get_tRNS                    MOZ_PNG_get_tRNS
#define png_get_unknown_chunks          MOZ_PNG_get_unk_chunks
#define png_get_user_chunk_ptr          MOZ_PNG_get_user_chunk_ptr
#define png_get_user_height_max         MOZ_PNG_get_user_height_max
#define png_get_user_transform_ptr      MOZ_PNG_get_user_transform_ptr
#define png_get_user_width_max          MOZ_PNG_get_user_width_max
#define png_get_valid                   MOZ_PNG_get_valid
#define png_get_x_offset_inches         MOZ_PNG_get_x_offset_inches
#define png_get_x_offset_microns        MOZ_PNG_get_x_offs_microns
#define png_get_x_offset_pixels         MOZ_PNG_get_x_offs_pixels
#define png_get_x_pixels_per_inch       MOZ_PNG_get_x_pixels_per_inch
#define png_get_x_pixels_per_meter      MOZ_PNG_get_x_pix_per_meter
#define png_get_y_offset_inches         MOZ_PNG_get_y_offset_inches
#define png_get_y_offset_microns        MOZ_PNG_get_y_offs_microns
#define png_get_y_offset_pixels         MOZ_PNG_get_y_offs_pixels
#define png_get_y_pixels_per_inch       MOZ_PNG_get_y_pixels_per_inch
#define png_get_y_pixels_per_meter      MOZ_PNG_get_y_pix_per_meter
#define png_handle_as_unknown           MOZ_PNG_handle_as_unknown
#define png_handle_bKGD                 MOZ_PNG_handle_bKGD
#define png_handle_cHRM                 MOZ_PNG_handle_cHRM
#define png_handle_gAMA                 MOZ_PNG_handle_gAMA
#define png_handle_hIST                 MOZ_PNG_handle_hIST
#define png_handle_iCCP                 MOZ_PNG_handle_iCCP
#define png_handle_IEND                 MOZ_PNG_handle_IEND
#define png_handle_IHDR                 MOZ_PNG_handle_IHDR
#define png_handle_iTXt                 MOZ_PNG_handle_iTXt
#define png_handle_oFFs                 MOZ_PNG_handle_oFFs
#define png_handle_pCAL                 MOZ_PNG_handle_pCAL
#define png_handle_pHYs                 MOZ_PNG_handle_pHYs
#define png_handle_PLTE                 MOZ_PNG_handle_PLTE
#define png_handle_sBIT                 MOZ_PNG_handle_sBIT
#define png_handle_sCAL                 MOZ_PNG_handle_sCAL
#define png_handle_sPLT                 MOZ_PNG_handle_sPLT
#define png_handle_sRGB                 MOZ_PNG_handle_sRGB
#define png_handle_tEXt                 MOZ_PNG_handle_tEXt
#define png_handle_tIME                 MOZ_PNG_handle_tIME
#define png_handle_tRNS                 MOZ_PNG_handle_tRNS
#define png_handle_unknown              MOZ_PNG_handle_unknown
#define png_handle_zTXt                 MOZ_PNG_handle_zTXt
#define png_inflate                     MOZ_PNG_inflate
#define png_info_destroy                MOZ_PNG_info_dest
#define png_info_init_3                 MOZ_PNG_info_init_3
#define png_init_io                     MOZ_PNG_init_io
#define png_init_read_transformations   MOZ_PNG_init_read_transf
#define png_malloc                      MOZ_PNG_malloc
#define png_malloc_default              MOZ_PNG_malloc_default
#define png_malloc_warn                 MOZ_PNG_malloc_warn
#define png_permit_mng_features         MOZ_PNG_permit_mng_features
#define png_process_data                MOZ_PNG_process_data
#define png_process_IDAT_data           MOZ_PNG_proc_IDAT_data
#define png_process_some_data           MOZ_PNG_proc_some_data
#define png_progressive_combine_row     MOZ_PNG_progressive_combine_row
#define png_push_check_crc              MOZ_PNG_push_check_crc
#define png_push_crc_finish             MOZ_PNG_push_crc_finish
#define png_push_crc_skip               MOZ_PNG_push_crc_skip
#define png_push_fill_buffer            MOZ_PNG_push_fill_buffer
#define png_push_handle_iTXt            MOZ_PNG_push_handle_iTXt
#define png_push_handle_tEXt            MOZ_PNG_push_handle_tEXt
#define png_push_handle_unknown         MOZ_PNG_push_handle_unk
#define png_push_handle_zTXt            MOZ_PNG_push_handle_ztXt
#define png_push_have_end               MOZ_PNG_push_have_end
#define png_push_have_info              MOZ_PNG_push_have_info
#define png_push_have_row               MOZ_PNG_push_have_row
#define png_push_process_row            MOZ_PNG_push_proc_row
#define png_push_read_chunk             MOZ_PNG_push_read_chunk
#define png_push_read_end               MOZ_PNG_push_read_end
#define png_push_read_IDAT              MOZ_PNG_push_read_IDAT
#define png_push_read_iTXt              MOZ_PNG_push_read_iTXt
#define png_push_read_sig               MOZ_PNG_push_read_sig
#define png_push_read_tEXt              MOZ_PNG_push_read_tEXt
#define png_push_read_zTXt              MOZ_PNG_push_read_zTXt
#define png_push_restore_buffer         MOZ_PNG_push_rest_buf
#define png_push_save_buffer            MOZ_PNG_push_save_buf
#define png_read_chunk_header           MOZ_PNG_read_chunk_header
#define png_read_data                   MOZ_PNG_read_data
#define png_read_destroy                MOZ_PNG_read_dest
#define png_read_end                    MOZ_PNG_read_end
#define png_read_filter_row             MOZ_PNG_read_filt_row
#define png_read_finish_row             MOZ_PNG_read_finish_row
#define png_read_image                  MOZ_PNG_read_image
#define png_read_info                   MOZ_PNG_read_info
#define png_read_png                    MOZ_PNG_read_png
#define png_read_push_finish_row        MOZ_PNG_read_push_finish_row
#define png_read_row                    MOZ_PNG_read_row
#define png_read_rows                   MOZ_PNG_read_rows
#define png_read_start_row              MOZ_PNG_read_start_row
#define png_read_transform_info         MOZ_PNG_read_transform_info
#define png_read_update_info            MOZ_PNG_read_update_info
#define png_reset_crc                   MOZ_PNG_reset_crc
#define png_save_int_32                 MOZ_PNG_save_int_32
#define png_save_uint_16                MOZ_PNG_save_uint_16
#define png_save_uint_32                MOZ_PNG_save_uint_32
#define png_set_add_alpha               MOZ_PNG_set_add_alpha
#define png_set_background              MOZ_PNG_set_background
#define png_set_benign_errors           MOZ_PNG_set_benign_errors
#define png_set_bgr                     MOZ_PNG_set_bgr
#define png_set_bKGD                    MOZ_PNG_set_bKGD
#define png_set_cHRM                    MOZ_PNG_set_cHRM
#define png_set_cHRM_fixed              MOZ_PNG_set_cHRM_fixed
#define png_set_compression_buffer_size MOZ_PNG_set_comp_buf_siz
#define png_set_compression_level       MOZ_PNG_set_comp_level
#define png_set_compression_mem_level   MOZ_PNG_set_comp_mem_lev
#define png_set_compression_method      MOZ_PNG_set_comp_method
#define png_set_compression_strategy    MOZ_PNG_set_comp_strategy
#define png_set_compression_window_bits MOZ_PNG_set_comp_win_bits
#define png_set_crc_action              MOZ_PNG_set_crc_action
#define png_set_error_fn                MOZ_PNG_set_error_fn
#define png_set_expand                  MOZ_PNG_set_expand
#define png_set_expand_gray_1_2_4_to_8  MOZ_PNG_set_x_g_124_to_8
#define png_set_filler                  MOZ_PNG_set_filler
#define png_set_filter                  MOZ_PNG_set_filter
#define png_set_filter_heuristics       MOZ_PNG_set_filter_heur
#define png_set_flush                   MOZ_PNG_set_flush
#define png_set_gAMA                    MOZ_PNG_set_gAMA
#define png_set_gAMA_fixed              MOZ_PNG_set_gAMA_fixed
#define png_set_gamma                   MOZ_PNG_set_gamma
#define png_set_gray_to_rgb             MOZ_PNG_set_gray_to_rgb
#define png_set_hIST                    MOZ_PNG_set_hIST
#define png_set_iCCP                    MOZ_PNG_set_iCCP
#define png_set_IHDR                    MOZ_PNG_set_IHDR
#define png_set_interlace_handling      MOZ_PNG_set_interlace_handling
#define png_set_invalid                 MOZ_PNG_set_invalid
#define png_set_invert_alpha            MOZ_PNG_set_invert_alpha
#define png_set_invert_mono             MOZ_PNG_set_invert_mono
#define png_set_keep_unknown_chunks     MOZ_PNG_set_keep_unknown_chunks
#define png_set_mem_fn                  MOZ_PNG_set_mem_fn
#define png_set_oFFs                    MOZ_PNG_set_oFFs
#define png_set_packing                 MOZ_PNG_set_packing
#define png_set_packswap                MOZ_PNG_set_packswap
#define png_set_palette_to_rgb          MOZ_PNG_set_palette_to_rgb
#define png_set_pCAL                    MOZ_PNG_set_pCAL
#define png_set_pHYs                    MOZ_PNG_set_pHYs
#define png_set_PLTE                    MOZ_PNG_set_PLTE
#define png_set_progressive_read_fn     MOZ_PNG_set_progressive_read_fn
#define png_set_read_fn                 MOZ_PNG_set_read_fn
#define png_set_read_status_fn          MOZ_PNG_set_read_status_fn
#define png_set_read_user_chunk_fn      MOZ_PNG_set_read_user_chunk_fn
#define png_set_read_user_transform_fn  MOZ_PNG_set_read_user_trans_fn
#define png_set_rgb_to_gray             MOZ_PNG_set_rgb_to_gray
#define png_set_rgb_to_gray_fixed       MOZ_PNG_set_rgb_to_gray_fixed
#define png_set_rows                    MOZ_PNG_set_rows
#define png_set_sBIT                    MOZ_PNG_set_sBIT
#define png_set_sCAL                    MOZ_PNG_set_sCAL
#define png_set_sCAL_s                  MOZ_PNG_set_sCAL_s
#define png_set_shift                   MOZ_PNG_set_shift
#define png_set_sPLT                    MOZ_PNG_set_sPLT
#define png_set_sRGB                    MOZ_PNG_set_sRGB
#define png_set_sRGB_gAMA_and_cHRM      MOZ_PNG_set_sRGB_gAMA_and_cHRM
#define png_set_strip_16                MOZ_PNG_set_strip_16
#define png_set_strip_alpha             MOZ_PNG_set_strip_alpha
#define png_set_strip_error_numbers     MOZ_PNG_set_strip_err_nums
#define png_set_swap                    MOZ_PNG_set_swap
#define png_set_swap_alpha              MOZ_PNG_set_swap_alpha
#define png_set_text                    MOZ_PNG_set_text
#define png_set_text_2                  MOZ_PNG_set_text_2
#define png_set_tIME                    MOZ_PNG_set_tIME
#define png_set_tRNS                    MOZ_PNG_set_tRNS
#define png_set_tRNS_to_alpha           MOZ_PNG_set_tRNS_to_alpha
#define png_set_unknown_chunk_location  MOZ_PNG_set_unknown_chunk_loc
#define png_set_unknown_chunks          MOZ_PNG_set_unknown_chunks
#define png_set_user_limits             MOZ_PNG_set_user_limits
#define png_set_user_transform_info     MOZ_PNG_set_user_transform_info
#define png_set_write_fn                MOZ_PNG_set_write_fn
#define png_set_write_status_fn         MOZ_PNG_set_write_status_fn
#define png_set_write_user_transform_fn MOZ_PNG_set_write_user_trans_fn
#define png_start_read_image            MOZ_PNG_start_read_image
#define png_text_compress               MOZ_PNG_text_compress
#define png_write_bKGD                  MOZ_PNG_write_bKGD
#define png_write_cHRM                  MOZ_PNG_write_cHRM
#define png_write_cHRM_fixed            MOZ_PNG_write_cHRM_fixed
#define png_write_chunk                 MOZ_PNG_write_chunk
#define png_write_chunk_data            MOZ_PNG_write_chunk_data
#define png_write_chunk_end             MOZ_PNG_write_chunk_end
#define png_write_chunk_start           MOZ_PNG_write_chunk_start
#define png_write_compressed_data_out   MOZ_PNG_write_compressed_data_out
#define png_write_data                  MOZ_PNG_write_data
#define png_write_destroy               MOZ_PNG_write_destroy
#define png_write_end                   MOZ_PNG_write_end
#define png_write_filtered_row          MOZ_PNG_write_filtered_row
#define png_write_find_filter           MOZ_PNG_write_find_filter
#define png_write_finish_row            MOZ_PNG_write_finish_row
#define png_write_flush                 MOZ_PNG_write_flush
#define png_write_gAMA                  MOZ_PNG_write_gAMA
#define png_write_gAMA_fixed            MOZ_PNG_write_gAMA_fixed
#define png_write_hIST                  MOZ_PNG_write_hIST
#define png_write_iCCP                  MOZ_PNG_write_iCCP
#define png_write_IDAT                  MOZ_PNG_write_IDAT
#define png_write_IEND                  MOZ_PNG_write_IEND
#define png_write_IHDR                  MOZ_PNG_write_IHDR
#define png_write_image                 MOZ_PNG_write_image
#define png_write_info                  MOZ_PNG_write_info
#define png_write_info_before_PLTE      MOZ_PNG_write_info_before_PLTE
#define png_write_iTXt                  MOZ_PNG_write_iTXt
#define png_write_oFFs                  MOZ_PNG_write_oFFs
#define png_write_pCAL                  MOZ_PNG_write_pCAL
#define png_write_pHYs                  MOZ_PNG_write_pHYs
#define png_write_PLTE                  MOZ_PNG_write_PLTE
#define png_write_png                   MOZ_PNG_write_png
#define png_write_row                   MOZ_PNG_write_row
#define png_write_rows                  MOZ_PNG_write_rows
#define png_write_sBIT                  MOZ_PNG_write_sBIT
#define png_write_sCAL_s                MOZ_PNG_write_sCAL_s
#define png_write_sig                   MOZ_PNG_write_sig
#define png_write_sPLT                  MOZ_PNG_write_sPLT
#define png_write_sRGB                  MOZ_PNG_write_sRGB
#define png_write_start_row             MOZ_PNG_write_trans
#define png_write_tEXt                  MOZ_PNG_write_tEXt
#define png_write_tIME                  MOZ_PNG_write_tIME
#define png_write_tRNS                  MOZ_PNG_write_tRNS
#define png_write_zTXt                  MOZ_PNG_write_zTXt
#define png_zalloc                      MOZ_PNG_zalloc
#define png_zfree                       MOZ_PNG_zfree
#define onebppswaptable                 MOZ_onebppswaptable
#define twobppswaptable                 MOZ_twobppswaptable
#define fourbppswaptable                MOZ_fourbppswaptable

/* APNG additions */
#define png_ensure_fcTL_is_valid        MOZ_APNG_ensure_fcTL_is_valid
#define png_ensure_sequence_number      MOZ_APNG_ensure_seqno
#define png_get_acTL                    MOZ_APNG_get_acTL
#define png_get_first_frame_is_hidden   MOZ_APNG_get_first_frame_is_hidden
#define png_get_next_frame_blend_op     MOZ_APNG_get_next_frame_blend_op
#define png_get_next_frame_delay_den    MOZ_APNG_get_next_frame_delay_den
#define png_get_next_frame_delay_num    MOZ_APNG_get_next_frame_delay_num
#define png_get_next_frame_dispose_op   MOZ_APNG_get_next_frame_dispose_op
#define png_get_next_frame_fcTL         MOZ_APNG_get_next_frame_fcTL
#define png_get_next_frame_height       MOZ_APNG_get_next_frame_height
#define png_get_next_frame_width        MOZ_APNG_get_next_frame_width
#define png_get_next_frame_x_offset     MOZ_APNG_get_next_frame_x_offset
#define png_get_next_frame_y_offset     MOZ_APNG_get_next_frame_y_offset
#define png_get_num_frames              MOZ_APNG_set_num_frames
#define png_get_num_plays               MOZ_APNG_set_num_plays
#define png_handle_acTL                 MOZ_APNG_handle_acTL
#define png_handle_fcTL                 MOZ_APNG_handle_fcTL
#define png_handle_fdAT                 MOZ_APNG_handle_fdAT
#define png_have_info                   MOZ_APNG_have_info
#define png_progressive_read_reset      MOZ_APNG_prog_read_reset
#define png_read_frame_head             MOZ_APNG_read_frame_head
#define png_read_reinit                 MOZ_APNG_read_reinit
#define png_read_reset                  MOZ_APNG_read_reset
#define png_set_acTL                    MOZ_APNG_set_acTL
#define png_set_first_frame_is_hidden   MOZ_APNG_set_first_frame_is_hidden
#define png_set_next_frame_fcTL         MOZ_APNG_set_next_frame_fcTL
#define png_set_progressive_frame_fn    MOZ_APNG_set_prog_frame_fn
#define png_write_acTL                  MOZ_APNG_write_acTL
#define png_write_fcTL                  MOZ_APNG_write_fcTL
#define png_write_fdAT                  MOZ_APNG_write_fdAT
#define png_write_frame_head            MOZ_APNG_write_frame_head
#define png_write_frame_tail            MOZ_APNG_write_frame_tail
#define png_write_reinit                MOZ_APNG_write_reinit
#define png_write_reset                 MOZ_APNG_write_reset

/* libpng-1.4.x additions */
#define png_do_quantize                 MOZ_PNG_do_quantize
#define png_get_chunk_cache_max         MOZ_PNG_get_chunk_cache_max
#define png_get_chunk_malloc_max        MOZ_PNG_get_chunk_malloc_max
#define png_get_io_chunk_name           MOZ_PNG_get_io_chunk_name
#define png_get_io_state                MOZ_PNG_get_io_state
#define png_longjmp                     MOZ_PNG_longjmp
#define png_read_sig                    MOZ_PNG_read_sig
#define png_set_chunk_cache_max         MOZ_PNG_set_chunk_cache_max
#define png_set_chunk_malloc_max        MOZ_PNG_set_chunk_malloc_max
#define png_set_longjmp_fn              MOZ_PNG_set_longjmp_fn
#define png_set_quantize                MOZ_PNG_set_quantize

/* libpng-1.5.x additions */
#define png_32bit_exp                             MOZ_PNG_32bit_exp
#define png_8bit_l2                               MOZ_PNG_8bit_l2
#define png_ascii_from_fixed                      MOZ_PNG_ascii_from_fixed
#define png_ascii_from_fp                         MOZ_PNG_ascii_from_fp
#define png_build_16bit_table                     MOZ_PNG_build_16bit_table
#define png_build_16to8_table                     MOZ_PNG_build_16to8_table
#define png_build_8bit_table                      MOZ_PNG_build_8bit_table
#define png_check_fp_number                       MOZ_PNG_check_fp_number
#define png_check_fp_string                       MOZ_PNG_check_fp_string
#define png_chunk_unknown_handling                MOZ_PNG_chunk_unk_handling
#define png_destroy_gamma_table                   MOZ_PNG_destroy_gamma_table
#define png_do_compose                            MOZ_PNG_do_compose
#define png_do_encode_alpha                       MOZ_PNG_do_encode_alpha
#define png_do_expand_16                          MOZ_PNG_do_expand_16
#define png_do_scale_16_to_8                      MOZ_PNG_do_scale_16_to_8
#define png_do_strip_channel                      MOZ_PNG_do_strip_channel
#define png_exp                                   MOZ_PNG_exp
#define png_exp16bit                              MOZ_PNG_exp16bit
#define png_exp8bit                               MOZ_PNG_exp8bit
#define png_fixed_inches_from_microns             MOZ_PNG_fixed_inch_from_micr
#define png_format_number                         MOZ_PNG_format_number
#define png_gamma_16bit_correct                   MOZ_PNG_gamma_16bit_correct
#define png_gamma_8bit_correct                    MOZ_PNG_gamma_8bit_correct
#define png_gamma_correct                         MOZ_PNG_gamma_correct
#define png_gamma_significant                     MOZ_PNG_gamma_significant
#define png_gamma_threshold                       MOZ_PNG_gamma_threshold
#define png_get_cHRM_XYZ                          MOZ_PNG_get_cHRM_XYZ
#define png_get_cHRM_XYZ_fixed                    MOZ_PNG_get_cHRM_XYZ_fixed
#define png_get_current_pass_number               MOZ_PNG_get_cur_pass_number
#define png_get_current_row_number                MOZ_PNG_get_cur_row_number
#define png_get_fixed_point                       MOZ_PNG_get_fixed_point
#define png_get_io_chunk_type                     MOZ_PNG_get_io_chunk_type
#define png_get_pixel_aspect_ratio_fixed          MOZ_PNG_get_pixel_aspect_fx
#define png_get_sCAL_fixed                        MOZ_PNG_get_sCAL_fixed
#define png_get_x_offset_inches_fixed             MOZ_PNG_get_x_offs_inches_fx
#define png_get_y_offset_inches_fixed             MOZ_PNG_get_y_offs_inches_fx
#define png_have_hwcap                            MOZ_PNG_have_hwcap
#define png_init_filter_functions                 MOZ_PNG_init_filt_func
#define png_init_filter_functions_neon            MOZ_PNG_init_filt_func_neon
#define png_init_filter_heuristics                MOZ_PNG_init_filt_heur
#define png_init_palette_transformations          MOZ_PNG_init_palette_transf
#define png_init_rgb_transformations              MOZ_PNG_init_rgb_transf
#define png_log16bit                              MOZ_PNG_log16bit
#define png_log8bit                               MOZ_PNG_log8bit
#define png_muldiv                                MOZ_PNG_muldiv
#define png_muldiv_warn                           MOZ_PNG_muldiv_warn
#define png_pow10                                 MOZ_PNG_pow10
#define png_process_data_pause                    MOZ_PNG_process_data_pause
#define png_process_data_skip                     MOZ_PNG_process_data_skip
#define png_product2                              MOZ_PNG_product2
#define png_read_filter_row_avg                   MOZ_PNG_read_filt_row_a
#define png_read_filter_row_avg3_neon             MOZ_PNG_read_filt_row_a3_neon
#define png_read_filter_row_avg4_neon             MOZ_PNG_read_filt_row_a4_neon
#define png_read_filter_row_paeth_1byte_pixel     MOZ_PNG_read_filt_row_p_1b_px
#define png_read_filter_row_paeth_multibyte_pixel MOZ_PNG_read_filt_row_p_mb_px
#define png_read_filter_row_paeth3_neon           MOZ_PNG_read_filt_row_p3_neon
#define png_read_filter_row_paeth4_neon           MOZ_PNG_read_filt_row_p4_neon
#define png_read_filter_row_sub                   MOZ_PNG_read_filt_row_s
#define png_read_filter_row_sub3_neon             MOZ_PNG_read_filt_row_s3_neon
#define png_read_filter_row_sub4_neon             MOZ_PNG_read_filt_row_s4_neon
#define png_read_filter_row_up                    MOZ_PNG_read_filt_row_up
#define png_read_filter_row_up_neon               MOZ_PNG_read_filt_row_up_neon
#define png_reciprocal                            MOZ_PNG_reciprocal
#define png_reciprocal2                           MOZ_PNG_reciprocal2
#define png_reset_filter_heuristics               MOZ_PNG_reset_filt_heur
#define png_safecat                               MOZ_PNG_safecat
#define png_set_alpha_mode                        MOZ_PNG_set_alpha_mode
#define png_set_alpha_mode_fixed                  MOZ_PNG_set_alpha_mode_fx
#define png_set_background_fixed                  MOZ_PNG_set_background_fx
#define png_set_cHRM_XYZ                          MOZ_PNG_set_cHRM_XYZ
#define png_set_cHRM_XYZ_fixed                    MOZ_PNG_set_cHRM_XYZ_fixed
#define png_set_expand_16                         MOZ_PNG_set_expand_16
#define png_set_filter_heuristics_fixed           MOZ_PNG_set_filter_heur_fx
#define png_set_gamma_fixed                       MOZ_PNG_set_gamma_fixed
#define png_set_sCAL_fixed                        MOZ_PNG_set_sCAL_fixed
#define png_set_scale_16                          MOZ_PNG_set_scale_16
#define png_set_text_compression_level            MOZ_PNG_set_text_c_level
#define png_set_text_compression_mem_level        MOZ_PNG_set_text_c_mem_level
#define png_set_text_compression_method           MOZ_PNG_set_text_c_method
#define png_set_text_compression_strategy         MOZ_PNG_set_text_c_strategy
#define png_set_text_compression_window_bits      MOZ_PNG_set_text_c_wnd_bits
#define png_user_version_check                    MOZ_PNG_user_version_check
#define png_write_chunk_header                    MOZ_PNG_write_chunk_header
#define png_write_complete_chunk                  MOZ_PNG_write_complete_chunk
#define png_xy_from_XYZ                           MOZ_PNG_xy_from_XYZ
#define png_XYZ_from_xy                           MOZ_PNG_XYZ_from_xy
#define png_XYZ_from_xy_checked                   MOZ_PNG_XYZ_from_xy_checked
#define png_zlib_claim                            MOZ_PNG_zlib_claim
#define png_zlib_release                          MOZ_PNG_zlib_release
#define convert_gamma_value                       MOZ_convert_gamma_value
#define ppi_from_ppm                              MOZ_ppi_from_ppm
#define translate_gamma_flags                     MOZ_translate_gamma_flags

/* libpng-1.6.x additions */
#define png_app_error                             MOZ_PNG_app_err
#define png_app_warning                           MOZ_PNG_app_warn
#define png_benign_error                          MOZ_PNG_benign_err
#define png_chunk_benign_error                    MOZ_PNG_chunk_benign_err
#define png_chunk_report                          MOZ_PNG_chunk_report
#define png_colorspace_set_ICC                    MOZ_PNG_cs_set_ICC
#define png_colorspace_set_chromaticities         MOZ_PNG_cs_set_chromats
#define png_colorspace_set_endpoints              MOZ_PNG_cs_set_endpts
#define png_colorspace_set_gamma                  MOZ_PNG_cs_set_gamma
#define png_colorspace_set_sRGB                   MOZ_PNG_cs_set_sRGB
#define png_colorspace_sync                       MOZ_PNG_cs_sync
#define png_colorspace_sync_info                  MOZ_PNG_cs_sync_info
#define png_compress_IDAT                         MOZ_PNG_compress_IDAT
#define png_create_png_struct                     MOZ_PNG_create_png_struct
#define png_destroy_png_struct                    MOZ_PNG_destroy_png_struct
#define png_free_buffer_list                      MOZ_PNG_free_buffer_list
#define png_free_jmpbuf                           MOZ_PNG_free_jmpbuf
#define png_get_uint_31                           MOZ_PNG_get_uint_31
#define png_icc_check_header                      MOZ_PNG_icc_check_header
#define png_icc_check_length                      MOZ_PNG_icc_check_length
#define png_icc_check_tag_table                   MOZ_PNG_icc_check_tags
#define png_icc_set_sRGB                          MOZ_PNG_icc_set_sRGB
#define png_malloc_array                          MOZ_PNG_malloc_array
#define png_malloc_base                           MOZ_PNG_malloc_base
#define png_realloc_array                         MOZ_PNG_realloc_array
#define png_zstream_error                         MOZ_PNG_zstream_error

#if defined(PR_LOGGING) && defined(PNG_WARNINGS_SUPPORTED)
#define png_warning                     MOZ_PNG_warning
#define png_error                       MOZ_PNG_error
#define png_chunk_error                 MOZ_PNG_chunk_err
#define png_fixed_error                 MOZ_PNG_fixed_err
#define png_formatted_warning           MOZ_PNG_formatted_warning
#define png_chunk_warning               MOZ_PNG_chunk_warn
#define png_warning_parameter           MOZ_PNG_warn_param
#define png_warning_parameter_signed    MOZ_PNG_warn_param_signed
#define png_warning_parameter_unsigned  MOZ_PNG_warn_param_unsigned
#endif

#endif /* MOZPNGCONF_H */
