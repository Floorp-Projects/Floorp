/* ************************************************************************** */
/* *             For conditions of distribution and use,                    * */
/* *                see copyright notice in libmng.h                        * */
/* ************************************************************************** */
/* *                                                                        * */
/* * project   : libmng                                                     * */
/* * file      : libmng_pixels.h           copyright (c) 2000-2002 G.Juyn   * */
/* * version   : 1.0.5                                                      * */
/* *                                                                        * */
/* * purpose   : Pixel-row management routines (definition)                 * */
/* *                                                                        * */
/* * author    : G.Juyn                                                     * */
/* * web       : http://www.3-t.com                                         * */
/* * email     : mailto:info@3-t.com                                        * */
/* *                                                                        * */
/* * comment   : Definition of the pixel-row management routines            * */
/* *                                                                        * */
/* * changes   : 0.5.1 - 05/08/2000 - G.Juyn                                * */
/* *             - changed strict-ANSI stuff                                * */
/* *                                                                        * */
/* *             0.5.2 - 05/22/2000 - G.Juyn                                * */
/* *             - added some JNG definitions                               * */
/* *             - added delta-image row-processing routines                * */
/* *             0.5.2 - 06/05/2000 - G.Juyn                                * */
/* *             - added support for RGB8_A8 canvasstyle                    * */
/* *                                                                        * */
/* *             0.5.3 - 06/16/2000 - G.Juyn                                * */
/* *             - changed progressive-display processing                   * */
/* *                                                                        * */
/* *             0.9.2 - 08/05/2000 - G.Juyn                                * */
/* *             - changed file-prefixes                                    * */
/* *                                                                        * */
/* *             0.9.3 - 08/26/2000 - G.Juyn                                * */
/* *             - added MAGN support                                       * */
/* *             0.9.3 - 09/07/2000 - G.Juyn                                * */
/* *             - added support for new filter_types                       * */
/* *             0.9.3 - 10/16/2000 - G.Juyn                                * */
/* *             - added optional support for bKGD for PNG images           * */
/* *             - added support for JDAA                                   * */
/* *             0.9.3 - 10/19/2000 - G.Juyn                                * */
/* *             - implemented delayed delta-processing                     * */
/* *                                                                        * */
/* *             0.9.4 -  1/18/2001 - G.Juyn                                * */
/* *             - added "new" MAGN methods 3, 4 & 5                        * */
/* *                                                                        * */
/* *             1.0.1 - 04/21/2001 - G.Juyn (code by G.Kelly)              * */
/* *             - added BGRA8 canvas with premultiplied alpha              * */
/* *                                                                        * */
/* *             1.0.5 - 08/15/2002 - G.Juyn                                * */
/* *             - completed PROM support                                   * */
/* *             - completed delta-image support                            * */
/* *             1.0.5 - 08/16/2002 - G.Juyn                                * */
/* *             - completed MAGN support (16-bit functions)                * */
/* *             1.0.5 - 08/19/2002 - G.Juyn                                * */
/* *             - B597134 - libmng pollutes the linker namespace           * */
/* *             1.0.5 - 09/22/2002 - G.Juyn                                * */
/* *             - added bgrx8 canvas (filler byte)                         * */
/* *             1.0.5 - 09/23/2002 - G.Juyn                                * */
/* *             - added compose over/under routines for PAST processing    * */
/* *             - added flip & tile routines for PAST processing           * */
/* *                                                                        * */
/* ************************************************************************** */

#if defined(__BORLANDC__) && defined(MNG_STRICT_ANSI)
#pragma option -A                      /* force ANSI-C */
#endif

#ifndef _libmng_pixels_h_
#define _libmng_pixels_h_

/* ************************************************************************** */
/* *                                                                        * */
/* * Progressive display check - checks to see if progressive display is    * */
/* * in order & indicates so                                                * */
/* *                                                                        * */
/* * The routine is called after a call to one of the display_xxx routines  * */
/* * if appropriate                                                         * */
/* *                                                                        * */
/* * The refresh is warrented in the read_chunk routine (mng_read.c)        * */
/* * and only during read&display processing, since there's not much point  * */
/* * doing it from memory!                                                  * */
/* *                                                                        * */
/* ************************************************************************** */

mng_retcode mng_display_progressive_check (mng_datap pData);

/* ************************************************************************** */
/* *                                                                        * */
/* * Display routines - convert rowdata (which is already color-corrected)  * */
/* * to the output canvas, respecting any transparency information          * */
/* *                                                                        * */
/* ************************************************************************** */

mng_retcode mng_display_rgb8           (mng_datap  pData);
mng_retcode mng_display_rgba8          (mng_datap  pData);
mng_retcode mng_display_argb8          (mng_datap  pData);
mng_retcode mng_display_rgb8_a8        (mng_datap  pData);
mng_retcode mng_display_bgr8           (mng_datap  pData);
mng_retcode mng_display_bgrx8          (mng_datap  pData);
mng_retcode mng_display_bgra8          (mng_datap  pData);
mng_retcode mng_display_bgra8_pm       (mng_datap  pData);
mng_retcode mng_display_abgr8          (mng_datap  pData);

/* ************************************************************************** */
/* *                                                                        * */
/* * Background restore routines - restore the background with info from    * */
/* * the BACK and/or bKGD chunk and/or the app's background canvas          * */
/* *                                                                        * */
/* ************************************************************************** */

mng_retcode mng_restore_bkgd_backimage (mng_datap  pData);
mng_retcode mng_restore_bkgd_backcolor (mng_datap  pData);
mng_retcode mng_restore_bkgd_bkgd      (mng_datap  pData);
mng_retcode mng_restore_bkgd_bgcolor   (mng_datap  pData);
mng_retcode mng_restore_bkgd_rgb8      (mng_datap  pData);
mng_retcode mng_restore_bkgd_bgr8      (mng_datap  pData);
mng_retcode mng_restore_bkgd_bgrx8     (mng_datap  pData);

/* ************************************************************************** */
/* *                                                                        * */
/* * Row retrieval routines - retrieve processed & uncompressed row-data    * */
/* * from the current "object"                                              * */
/* *                                                                        * */
/* ************************************************************************** */

mng_retcode mng_retrieve_g8            (mng_datap  pData);
mng_retcode mng_retrieve_g16           (mng_datap  pData);
mng_retcode mng_retrieve_rgb8          (mng_datap  pData);
mng_retcode mng_retrieve_rgb16         (mng_datap  pData);
mng_retcode mng_retrieve_idx8          (mng_datap  pData);
mng_retcode mng_retrieve_ga8           (mng_datap  pData);
mng_retcode mng_retrieve_ga16          (mng_datap  pData);
mng_retcode mng_retrieve_rgba8         (mng_datap  pData);
mng_retcode mng_retrieve_rgba16        (mng_datap  pData);

/* ************************************************************************** */
/* *                                                                        * */
/* * Row storage routines - store processed & uncompressed row-data         * */
/* * into the current "object"                                              * */
/* *                                                                        * */
/* ************************************************************************** */

mng_retcode mng_store_g1               (mng_datap  pData);
mng_retcode mng_store_g2               (mng_datap  pData);
mng_retcode mng_store_g4               (mng_datap  pData);
mng_retcode mng_store_g8               (mng_datap  pData);
mng_retcode mng_store_g16              (mng_datap  pData);
mng_retcode mng_store_rgb8             (mng_datap  pData);
mng_retcode mng_store_rgb16            (mng_datap  pData);
mng_retcode mng_store_idx1             (mng_datap  pData);
mng_retcode mng_store_idx2             (mng_datap  pData);
mng_retcode mng_store_idx4             (mng_datap  pData);
mng_retcode mng_store_idx8             (mng_datap  pData);
mng_retcode mng_store_ga8              (mng_datap  pData);
mng_retcode mng_store_ga16             (mng_datap  pData);
mng_retcode mng_store_rgba8            (mng_datap  pData);
mng_retcode mng_store_rgba16           (mng_datap  pData);

/* ************************************************************************** */
/* *                                                                        * */
/* * Row storage routines (JPEG) - store processed & uncompressed row-data  * */
/* * into the current "object"                                              * */
/* *                                                                        * */
/* ************************************************************************** */

mng_retcode mng_store_jpeg_g8          (mng_datap  pData);
mng_retcode mng_store_jpeg_rgb8        (mng_datap  pData);
mng_retcode mng_store_jpeg_ga8         (mng_datap  pData);
mng_retcode mng_store_jpeg_rgba8       (mng_datap  pData);

mng_retcode mng_store_jpeg_g12         (mng_datap  pData);
mng_retcode mng_store_jpeg_rgb12       (mng_datap  pData);
mng_retcode mng_store_jpeg_ga12        (mng_datap  pData);
mng_retcode mng_store_jpeg_rgba12      (mng_datap  pData);

mng_retcode mng_store_jpeg_g8_alpha    (mng_datap  pData);
mng_retcode mng_store_jpeg_rgb8_alpha  (mng_datap  pData);

mng_retcode mng_store_jpeg_g8_a1       (mng_datap  pData);
mng_retcode mng_store_jpeg_g8_a2       (mng_datap  pData);
mng_retcode mng_store_jpeg_g8_a4       (mng_datap  pData);
mng_retcode mng_store_jpeg_g8_a8       (mng_datap  pData);
mng_retcode mng_store_jpeg_g8_a16      (mng_datap  pData);

mng_retcode mng_store_jpeg_rgb8_a1     (mng_datap  pData);
mng_retcode mng_store_jpeg_rgb8_a2     (mng_datap  pData);
mng_retcode mng_store_jpeg_rgb8_a4     (mng_datap  pData);
mng_retcode mng_store_jpeg_rgb8_a8     (mng_datap  pData);
mng_retcode mng_store_jpeg_rgb8_a16    (mng_datap  pData);

mng_retcode mng_store_jpeg_g12_a1      (mng_datap  pData);
mng_retcode mng_store_jpeg_g12_a2      (mng_datap  pData);
mng_retcode mng_store_jpeg_g12_a4      (mng_datap  pData);
mng_retcode mng_store_jpeg_g12_a8      (mng_datap  pData);
mng_retcode mng_store_jpeg_g12_a16     (mng_datap  pData);

mng_retcode mng_store_jpeg_rgb12_a1    (mng_datap  pData);
mng_retcode mng_store_jpeg_rgb12_a2    (mng_datap  pData);
mng_retcode mng_store_jpeg_rgb12_a4    (mng_datap  pData);
mng_retcode mng_store_jpeg_rgb12_a8    (mng_datap  pData);
mng_retcode mng_store_jpeg_rgb12_a16   (mng_datap  pData);

/* ************************************************************************** */
/* *                                                                        * */
/* * Delta-image row routines - apply the processed & uncompressed row-data * */
/* * onto the target "object"                                               * */
/* *                                                                        * */
/* ************************************************************************** */

mng_retcode mng_delta_g1               (mng_datap  pData);
mng_retcode mng_delta_g2               (mng_datap  pData);
mng_retcode mng_delta_g4               (mng_datap  pData);
mng_retcode mng_delta_g8               (mng_datap  pData);
mng_retcode mng_delta_g16              (mng_datap  pData);
mng_retcode mng_delta_rgb8             (mng_datap  pData);
mng_retcode mng_delta_rgb16            (mng_datap  pData);
mng_retcode mng_delta_idx1             (mng_datap  pData);
mng_retcode mng_delta_idx2             (mng_datap  pData);
mng_retcode mng_delta_idx4             (mng_datap  pData);
mng_retcode mng_delta_idx8             (mng_datap  pData);
mng_retcode mng_delta_ga8              (mng_datap  pData);
mng_retcode mng_delta_ga16             (mng_datap  pData);
mng_retcode mng_delta_rgba8            (mng_datap  pData);
mng_retcode mng_delta_rgba16           (mng_datap  pData);

/* ************************************************************************** */
/* *                                                                        * */
/* * Delta-image row routines - apply the source row onto the target        * */
/* *                                                                        * */
/* ************************************************************************** */

mng_retcode mng_delta_g1_g1            (mng_datap  pData);
mng_retcode mng_delta_g2_g2            (mng_datap  pData);
mng_retcode mng_delta_g4_g4            (mng_datap  pData);
mng_retcode mng_delta_g8_g8            (mng_datap  pData);
mng_retcode mng_delta_g16_g16          (mng_datap  pData);
mng_retcode mng_delta_rgb8_rgb8        (mng_datap  pData);
mng_retcode mng_delta_rgb16_rgb16      (mng_datap  pData);
mng_retcode mng_delta_ga8_ga8          (mng_datap  pData);
mng_retcode mng_delta_ga8_g8           (mng_datap  pData);
mng_retcode mng_delta_ga8_a8           (mng_datap  pData);
mng_retcode mng_delta_ga16_ga16        (mng_datap  pData);
mng_retcode mng_delta_ga16_g16         (mng_datap  pData);
mng_retcode mng_delta_ga16_a16         (mng_datap  pData);
mng_retcode mng_delta_rgba8_rgba8      (mng_datap  pData);
mng_retcode mng_delta_rgba8_rgb8       (mng_datap  pData);
mng_retcode mng_delta_rgba8_a8         (mng_datap  pData);
mng_retcode mng_delta_rgba16_rgba16    (mng_datap  pData);
mng_retcode mng_delta_rgba16_rgb16     (mng_datap  pData);
mng_retcode mng_delta_rgba16_a16       (mng_datap  pData);

/* ************************************************************************** */
/* *                                                                        * */
/* * Delta-image row routines - scale the delta to bitdepth of target       * */
/* *                                                                        * */
/* ************************************************************************** */

mng_retcode mng_scale_g1_g2            (mng_datap  pData);
mng_retcode mng_scale_g1_g4            (mng_datap  pData);
mng_retcode mng_scale_g1_g8            (mng_datap  pData);
mng_retcode mng_scale_g1_g16           (mng_datap  pData);
mng_retcode mng_scale_g2_g4            (mng_datap  pData);
mng_retcode mng_scale_g2_g8            (mng_datap  pData);
mng_retcode mng_scale_g2_g16           (mng_datap  pData);
mng_retcode mng_scale_g4_g8            (mng_datap  pData);
mng_retcode mng_scale_g4_g16           (mng_datap  pData);
mng_retcode mng_scale_g8_g16           (mng_datap  pData);
mng_retcode mng_scale_ga8_ga16         (mng_datap  pData);
mng_retcode mng_scale_rgb8_rgb16       (mng_datap  pData);
mng_retcode mng_scale_rgba8_rgba16     (mng_datap  pData);

mng_retcode mng_scale_g2_g1            (mng_datap  pData);
mng_retcode mng_scale_g4_g1            (mng_datap  pData);
mng_retcode mng_scale_g8_g1            (mng_datap  pData);
mng_retcode mng_scale_g16_g1           (mng_datap  pData);
mng_retcode mng_scale_g4_g2            (mng_datap  pData);
mng_retcode mng_scale_g8_g2            (mng_datap  pData);
mng_retcode mng_scale_g16_g2           (mng_datap  pData);
mng_retcode mng_scale_g8_g4            (mng_datap  pData);
mng_retcode mng_scale_g16_g4           (mng_datap  pData);
mng_retcode mng_scale_g16_g8           (mng_datap  pData);
mng_retcode mng_scale_ga16_ga8         (mng_datap  pData);
mng_retcode mng_scale_rgb16_rgb8       (mng_datap  pData);
mng_retcode mng_scale_rgba16_rgba8     (mng_datap  pData);

/* ************************************************************************** */
/* *                                                                        * */
/* * Delta-image bit routines - promote bit_depth                           * */
/* *                                                                        * */
/* ************************************************************************** */

mng_uint8   mng_promote_replicate_1_2  (mng_uint8  iB);
mng_uint8   mng_promote_replicate_1_4  (mng_uint8  iB);
mng_uint8   mng_promote_replicate_1_8  (mng_uint8  iB);
mng_uint16  mng_promote_replicate_1_16 (mng_uint8  iB);
mng_uint8   mng_promote_replicate_2_4  (mng_uint8  iB);
mng_uint8   mng_promote_replicate_2_8  (mng_uint8  iB);
mng_uint16  mng_promote_replicate_2_16 (mng_uint8  iB);
mng_uint8   mng_promote_replicate_4_8  (mng_uint8  iB);
mng_uint16  mng_promote_replicate_4_16 (mng_uint8  iB);
mng_uint16  mng_promote_replicate_8_16 (mng_uint8  iB);

/* ************************************************************************** */

mng_uint8   mng_promote_zerofill_1_2   (mng_uint8  iB);
mng_uint8   mng_promote_zerofill_1_4   (mng_uint8  iB);
mng_uint8   mng_promote_zerofill_1_8   (mng_uint8  iB);
mng_uint16  mng_promote_zerofill_1_16  (mng_uint8  iB);
mng_uint8   mng_promote_zerofill_2_4   (mng_uint8  iB);
mng_uint8   mng_promote_zerofill_2_8   (mng_uint8  iB);
mng_uint16  mng_promote_zerofill_2_16  (mng_uint8  iB);
mng_uint8   mng_promote_zerofill_4_8   (mng_uint8  iB);
mng_uint16  mng_promote_zerofill_4_16  (mng_uint8  iB);
mng_uint16  mng_promote_zerofill_8_16  (mng_uint8  iB);

/* ************************************************************************** */
/* *                                                                        * */
/* * Delta-image row routines - promote color_type                          * */
/* *                                                                        * */
/* ************************************************************************** */

mng_retcode mng_promote_g8_g8          (mng_datap  pData);
mng_retcode mng_promote_g8_g16         (mng_datap  pData);
mng_retcode mng_promote_g16_g16        (mng_datap  pData);

mng_retcode mng_promote_g8_ga8         (mng_datap  pData);
mng_retcode mng_promote_g8_ga16        (mng_datap  pData);
mng_retcode mng_promote_g16_ga16       (mng_datap  pData);

mng_retcode mng_promote_g8_rgb8        (mng_datap  pData);
mng_retcode mng_promote_g8_rgb16       (mng_datap  pData);
mng_retcode mng_promote_g16_rgb16      (mng_datap  pData);

mng_retcode mng_promote_g8_rgba8       (mng_datap  pData);
mng_retcode mng_promote_g8_rgba16      (mng_datap  pData);
mng_retcode mng_promote_g16_rgba16     (mng_datap  pData);

mng_retcode mng_promote_ga8_ga16       (mng_datap  pData);

mng_retcode mng_promote_ga8_rgba8      (mng_datap  pData);
mng_retcode mng_promote_ga8_rgba16     (mng_datap  pData);
mng_retcode mng_promote_ga16_rgba16    (mng_datap  pData);

mng_retcode mng_promote_rgb8_rgb16     (mng_datap  pData);

mng_retcode mng_promote_rgb8_rgba8     (mng_datap  pData);
mng_retcode mng_promote_rgb8_rgba16    (mng_datap  pData);
mng_retcode mng_promote_rgb16_rgba16   (mng_datap  pData);

mng_retcode mng_promote_idx8_rgb8      (mng_datap  pData);
mng_retcode mng_promote_idx8_rgb16     (mng_datap  pData);

mng_retcode mng_promote_idx8_rgba8     (mng_datap  pData);
mng_retcode mng_promote_idx8_rgba16    (mng_datap  pData);

mng_retcode mng_promote_rgba8_rgba16   (mng_datap  pData);

/* ************************************************************************** */
/* *                                                                        * */
/* * Row processing routines - convert uncompressed data from zlib to       * */
/* * managable row-data which serves as input to the color-management       * */
/* * routines                                                               * */
/* *                                                                        * */
/* ************************************************************************** */

mng_retcode mng_process_g1             (mng_datap  pData);
mng_retcode mng_process_g2             (mng_datap  pData);
mng_retcode mng_process_g4             (mng_datap  pData);
mng_retcode mng_process_g8             (mng_datap  pData);
mng_retcode mng_process_g16            (mng_datap  pData);
mng_retcode mng_process_rgb8           (mng_datap  pData);
mng_retcode mng_process_rgb16          (mng_datap  pData);
mng_retcode mng_process_idx1           (mng_datap  pData);
mng_retcode mng_process_idx2           (mng_datap  pData);
mng_retcode mng_process_idx4           (mng_datap  pData);
mng_retcode mng_process_idx8           (mng_datap  pData);
mng_retcode mng_process_ga8            (mng_datap  pData);
mng_retcode mng_process_ga16           (mng_datap  pData);
mng_retcode mng_process_rgba8          (mng_datap  pData);
mng_retcode mng_process_rgba16         (mng_datap  pData);

/* ************************************************************************** */
/* *                                                                        * */
/* * Row processing initialization routines - set up the variables needed   * */
/* * to process uncompressed row-data                                       * */
/* *                                                                        * */
/* ************************************************************************** */

mng_retcode mng_init_g1_ni             (mng_datap  pData);
mng_retcode mng_init_g1_i              (mng_datap  pData);
mng_retcode mng_init_g2_ni             (mng_datap  pData);
mng_retcode mng_init_g2_i              (mng_datap  pData);
mng_retcode mng_init_g4_ni             (mng_datap  pData);
mng_retcode mng_init_g4_i              (mng_datap  pData);
mng_retcode mng_init_g8_ni             (mng_datap  pData);
mng_retcode mng_init_g8_i              (mng_datap  pData);
mng_retcode mng_init_g16_ni            (mng_datap  pData);
mng_retcode mng_init_g16_i             (mng_datap  pData);
mng_retcode mng_init_rgb8_ni           (mng_datap  pData);
mng_retcode mng_init_rgb8_i            (mng_datap  pData);
mng_retcode mng_init_rgb16_ni          (mng_datap  pData);
mng_retcode mng_init_rgb16_i           (mng_datap  pData);
mng_retcode mng_init_idx1_ni           (mng_datap  pData);
mng_retcode mng_init_idx1_i            (mng_datap  pData);
mng_retcode mng_init_idx2_ni           (mng_datap  pData);
mng_retcode mng_init_idx2_i            (mng_datap  pData);
mng_retcode mng_init_idx4_ni           (mng_datap  pData);
mng_retcode mng_init_idx4_i            (mng_datap  pData);
mng_retcode mng_init_idx8_ni           (mng_datap  pData);
mng_retcode mng_init_idx8_i            (mng_datap  pData);
mng_retcode mng_init_ga8_ni            (mng_datap  pData);
mng_retcode mng_init_ga8_i             (mng_datap  pData);
mng_retcode mng_init_ga16_ni           (mng_datap  pData);
mng_retcode mng_init_ga16_i            (mng_datap  pData);
mng_retcode mng_init_rgba8_ni          (mng_datap  pData);
mng_retcode mng_init_rgba8_i           (mng_datap  pData);
mng_retcode mng_init_rgba16_ni         (mng_datap  pData);
mng_retcode mng_init_rgba16_i          (mng_datap  pData);

/* ************************************************************************** */
/* *                                                                        * */
/* * Row processing initialization routines (JPEG) - set up the variables   * */
/* * needed to process uncompressed row-data                                * */
/* *                                                                        * */
/* ************************************************************************** */

mng_retcode mng_init_jpeg_a1_ni        (mng_datap  pData);
mng_retcode mng_init_jpeg_a2_ni        (mng_datap  pData);
mng_retcode mng_init_jpeg_a4_ni        (mng_datap  pData);
mng_retcode mng_init_jpeg_a8_ni        (mng_datap  pData);
mng_retcode mng_init_jpeg_a16_ni       (mng_datap  pData);

/* ************************************************************************** */
/* *                                                                        * */
/* * General row processing routines                                        * */
/* *                                                                        * */
/* ************************************************************************** */

mng_retcode mng_init_rowproc           (mng_datap  pData);
mng_retcode mng_next_row               (mng_datap  pData);
mng_retcode mng_next_jpeg_alpharow     (mng_datap  pData);
mng_retcode mng_next_jpeg_row          (mng_datap  pData);
mng_retcode mng_cleanup_rowproc        (mng_datap  pData);

/* ************************************************************************** */
/* *                                                                        * */
/* * Magnification row routines - apply magnification transforms            * */
/* *                                                                        * */
/* ************************************************************************** */

mng_retcode mng_magnify_g8_x1          (mng_datap  pData,
                                        mng_uint16 iMX,
                                        mng_uint16 iML,
                                        mng_uint16 iMR,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_g8_x2          (mng_datap  pData,
                                        mng_uint16 iMX,
                                        mng_uint16 iML,
                                        mng_uint16 iMR,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_g8_x3          (mng_datap  pData,
                                        mng_uint16 iMX,
                                        mng_uint16 iML,
                                        mng_uint16 iMR,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_rgb8_x1        (mng_datap  pData,
                                        mng_uint16 iMX,
                                        mng_uint16 iML,
                                        mng_uint16 iMR,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_rgb8_x2        (mng_datap  pData,
                                        mng_uint16 iMX,
                                        mng_uint16 iML,
                                        mng_uint16 iMR,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_rgb8_x3        (mng_datap  pData,
                                        mng_uint16 iMX,
                                        mng_uint16 iML,
                                        mng_uint16 iMR,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_ga8_x1         (mng_datap  pData,
                                        mng_uint16 iMX,
                                        mng_uint16 iML,
                                        mng_uint16 iMR,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_ga8_x2         (mng_datap  pData,
                                        mng_uint16 iMX,
                                        mng_uint16 iML,
                                        mng_uint16 iMR,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_ga8_x3         (mng_datap  pData,
                                        mng_uint16 iMX,
                                        mng_uint16 iML,
                                        mng_uint16 iMR,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_ga8_x4         (mng_datap  pData,
                                        mng_uint16 iMX,
                                        mng_uint16 iML,
                                        mng_uint16 iMR,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_ga8_x5         (mng_datap  pData,
                                        mng_uint16 iMX,
                                        mng_uint16 iML,
                                        mng_uint16 iMR,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_rgba8_x1       (mng_datap  pData,
                                        mng_uint16 iMX,
                                        mng_uint16 iML,
                                        mng_uint16 iMR,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_rgba8_x2       (mng_datap  pData,
                                        mng_uint16 iMX,
                                        mng_uint16 iML,
                                        mng_uint16 iMR,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_rgba8_x3       (mng_datap  pData,
                                        mng_uint16 iMX,
                                        mng_uint16 iML,
                                        mng_uint16 iMR,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_rgba8_x4       (mng_datap  pData,
                                        mng_uint16 iMX,
                                        mng_uint16 iML,
                                        mng_uint16 iMR,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_rgba8_x5       (mng_datap  pData,
                                        mng_uint16 iMX,
                                        mng_uint16 iML,
                                        mng_uint16 iMR,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline,
                                        mng_uint8p pDstline);

mng_retcode mng_magnify_g8_y1          (mng_datap  pData,
                                        mng_int32  iS,
                                        mng_int32  iM,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline1,
                                        mng_uint8p pSrcline2,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_g8_y2          (mng_datap  pData,
                                        mng_int32  iS,
                                        mng_int32  iM,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline1,
                                        mng_uint8p pSrcline2,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_g8_y3          (mng_datap  pData,
                                        mng_int32  iS,
                                        mng_int32  iM,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline1,
                                        mng_uint8p pSrcline2,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_rgb8_y1        (mng_datap  pData,
                                        mng_int32  iS,
                                        mng_int32  iM,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline1,
                                        mng_uint8p pSrcline2,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_rgb8_y2        (mng_datap  pData,
                                        mng_int32  iS,
                                        mng_int32  iM,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline1,
                                        mng_uint8p pSrcline2,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_rgb8_y3        (mng_datap  pData,
                                        mng_int32  iS,
                                        mng_int32  iM,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline1,
                                        mng_uint8p pSrcline2,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_ga8_y1         (mng_datap  pData,
                                        mng_int32  iS,
                                        mng_int32  iM,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline1,
                                        mng_uint8p pSrcline2,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_ga8_y2         (mng_datap  pData,
                                        mng_int32  iS,
                                        mng_int32  iM,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline1,
                                        mng_uint8p pSrcline2,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_ga8_y3         (mng_datap  pData,
                                        mng_int32  iS,
                                        mng_int32  iM,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline1,
                                        mng_uint8p pSrcline2,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_ga8_y4         (mng_datap  pData,
                                        mng_int32  iS,
                                        mng_int32  iM,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline1,
                                        mng_uint8p pSrcline2,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_ga8_y5         (mng_datap  pData,
                                        mng_int32  iS,
                                        mng_int32  iM,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline1,
                                        mng_uint8p pSrcline2,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_rgba8_y1       (mng_datap  pData,
                                        mng_int32  iS,
                                        mng_int32  iM,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline1,
                                        mng_uint8p pSrcline2,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_rgba8_y2       (mng_datap  pData,
                                        mng_int32  iS,
                                        mng_int32  iM,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline1,
                                        mng_uint8p pSrcline2,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_rgba8_y3       (mng_datap  pData,
                                        mng_int32  iS,
                                        mng_int32  iM,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline1,
                                        mng_uint8p pSrcline2,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_rgba8_y4       (mng_datap  pData,
                                        mng_int32  iS,
                                        mng_int32  iM,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline1,
                                        mng_uint8p pSrcline2,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_rgba8_y5       (mng_datap  pData,
                                        mng_int32  iS,
                                        mng_int32  iM,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline1,
                                        mng_uint8p pSrcline2,
                                        mng_uint8p pDstline);

/* ************************************************************************** */

mng_retcode mng_magnify_g16_x1         (mng_datap  pData,
                                        mng_uint16 iMX,
                                        mng_uint16 iML,
                                        mng_uint16 iMR,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_g16_x2         (mng_datap  pData,
                                        mng_uint16 iMX,
                                        mng_uint16 iML,
                                        mng_uint16 iMR,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_g16_x3         (mng_datap  pData,
                                        mng_uint16 iMX,
                                        mng_uint16 iML,
                                        mng_uint16 iMR,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_rgb16_x1       (mng_datap  pData,
                                        mng_uint16 iMX,
                                        mng_uint16 iML,
                                        mng_uint16 iMR,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_rgb16_x2       (mng_datap  pData,
                                        mng_uint16 iMX,
                                        mng_uint16 iML,
                                        mng_uint16 iMR,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_rgb16_x3       (mng_datap  pData,
                                        mng_uint16 iMX,
                                        mng_uint16 iML,
                                        mng_uint16 iMR,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_ga16_x1        (mng_datap  pData,
                                        mng_uint16 iMX,
                                        mng_uint16 iML,
                                        mng_uint16 iMR,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_ga16_x2        (mng_datap  pData,
                                        mng_uint16 iMX,
                                        mng_uint16 iML,
                                        mng_uint16 iMR,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_ga16_x3        (mng_datap  pData,
                                        mng_uint16 iMX,
                                        mng_uint16 iML,
                                        mng_uint16 iMR,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_ga16_x4        (mng_datap  pData,
                                        mng_uint16 iMX,
                                        mng_uint16 iML,
                                        mng_uint16 iMR,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_ga16_x5        (mng_datap  pData,
                                        mng_uint16 iMX,
                                        mng_uint16 iML,
                                        mng_uint16 iMR,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_rgba16_x1      (mng_datap  pData,
                                        mng_uint16 iMX,
                                        mng_uint16 iML,
                                        mng_uint16 iMR,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_rgba16_x2      (mng_datap  pData,
                                        mng_uint16 iMX,
                                        mng_uint16 iML,
                                        mng_uint16 iMR,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_rgba16_x3      (mng_datap  pData,
                                        mng_uint16 iMX,
                                        mng_uint16 iML,
                                        mng_uint16 iMR,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_rgba16_x4      (mng_datap  pData,
                                        mng_uint16 iMX,
                                        mng_uint16 iML,
                                        mng_uint16 iMR,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_rgba16_x5      (mng_datap  pData,
                                        mng_uint16 iMX,
                                        mng_uint16 iML,
                                        mng_uint16 iMR,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline,
                                        mng_uint8p pDstline);

mng_retcode mng_magnify_g16_y1         (mng_datap  pData,
                                        mng_int32  iS,
                                        mng_int32  iM,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline1,
                                        mng_uint8p pSrcline2,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_g16_y2         (mng_datap  pData,
                                        mng_int32  iS,
                                        mng_int32  iM,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline1,
                                        mng_uint8p pSrcline2,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_g16_y3         (mng_datap  pData,
                                        mng_int32  iS,
                                        mng_int32  iM,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline1,
                                        mng_uint8p pSrcline2,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_rgb16_y1       (mng_datap  pData,
                                        mng_int32  iS,
                                        mng_int32  iM,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline1,
                                        mng_uint8p pSrcline2,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_rgb16_y2       (mng_datap  pData,
                                        mng_int32  iS,
                                        mng_int32  iM,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline1,
                                        mng_uint8p pSrcline2,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_rgb16_y3       (mng_datap  pData,
                                        mng_int32  iS,
                                        mng_int32  iM,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline1,
                                        mng_uint8p pSrcline2,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_ga16_y1        (mng_datap  pData,
                                        mng_int32  iS,
                                        mng_int32  iM,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline1,
                                        mng_uint8p pSrcline2,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_ga16_y2        (mng_datap  pData,
                                        mng_int32  iS,
                                        mng_int32  iM,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline1,
                                        mng_uint8p pSrcline2,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_ga16_y3        (mng_datap  pData,
                                        mng_int32  iS,
                                        mng_int32  iM,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline1,
                                        mng_uint8p pSrcline2,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_ga16_y4        (mng_datap  pData,
                                        mng_int32  iS,
                                        mng_int32  iM,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline1,
                                        mng_uint8p pSrcline2,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_ga16_y5        (mng_datap  pData,
                                        mng_int32  iS,
                                        mng_int32  iM,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline1,
                                        mng_uint8p pSrcline2,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_rgba16_y1      (mng_datap  pData,
                                        mng_int32  iS,
                                        mng_int32  iM,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline1,
                                        mng_uint8p pSrcline2,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_rgba16_y2      (mng_datap  pData,
                                        mng_int32  iS,
                                        mng_int32  iM,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline1,
                                        mng_uint8p pSrcline2,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_rgba16_y3      (mng_datap  pData,
                                        mng_int32  iS,
                                        mng_int32  iM,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline1,
                                        mng_uint8p pSrcline2,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_rgba16_y4      (mng_datap  pData,
                                        mng_int32  iS,
                                        mng_int32  iM,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline1,
                                        mng_uint8p pSrcline2,
                                        mng_uint8p pDstline);
mng_retcode mng_magnify_rgba16_y5      (mng_datap  pData,
                                        mng_int32  iS,
                                        mng_int32  iM,
                                        mng_uint32 iWidth,
                                        mng_uint8p pSrcline1,
                                        mng_uint8p pSrcline2,
                                        mng_uint8p pDstline);

/* ************************************************************************** */
/* *                                                                        * */
/* * PAST composition routines - compose over/under with a target object    * */
/* *                                                                        * */
/* ************************************************************************** */

mng_retcode mng_composeover_rgba8      (mng_datap  pData);
mng_retcode mng_composeover_rgba16     (mng_datap  pData);
mng_retcode mng_composeunder_rgba8     (mng_datap  pData);
mng_retcode mng_composeunder_rgba16    (mng_datap  pData);

/* ************************************************************************** */
/* *                                                                        * */
/* * PAST flip & tile routines - flip or tile a row of pixels               * */
/* *                                                                        * */
/* ************************************************************************** */

mng_retcode mng_flip_rgba8             (mng_datap  pData);
mng_retcode mng_flip_rgba16            (mng_datap  pData);
mng_retcode mng_tile_rgba8             (mng_datap  pData);
mng_retcode mng_tile_rgba16            (mng_datap  pData);

/* ************************************************************************** */

#endif /* _libmng_pixels_h_ */

/* ************************************************************************** */
/* * end of file                                                            * */
/* ************************************************************************** */
