/* ************************************************************************** */
/* *             For conditions of distribution and use,                    * */
/* *                see copyright notice in libmng.h                        * */
/* ************************************************************************** */
/* *                                                                        * */
/* * project   : libmng                                                     * */
/* * file      : libmng_pixels.h           copyright (c) 2000 G.Juyn        * */
/* * version   : 1.0.1                                                      * */
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

mng_retcode display_progressive_check (mng_datap pData);

/* ************************************************************************** */
/* *                                                                        * */
/* * Display routines - convert rowdata (which is already color-corrected)  * */
/* * to the output canvas, respecting any transparency information          * */
/* *                                                                        * */
/* ************************************************************************** */

mng_retcode display_rgb8           (mng_datap  pData);
mng_retcode display_rgba8          (mng_datap  pData);
mng_retcode display_argb8          (mng_datap  pData);
mng_retcode display_rgb8_a8        (mng_datap  pData);
mng_retcode display_bgr8           (mng_datap  pData);
mng_retcode display_bgra8          (mng_datap  pData);
mng_retcode display_bgra8_pm       (mng_datap  pData);
mng_retcode display_abgr8          (mng_datap  pData);

/* ************************************************************************** */
/* *                                                                        * */
/* * Background restore routines - restore the background with info from    * */
/* * the BACK and/or bKGD chunk and/or the app's background canvas          * */
/* *                                                                        * */
/* ************************************************************************** */

mng_retcode restore_bkgd_backimage (mng_datap  pData);
mng_retcode restore_bkgd_backcolor (mng_datap  pData);
mng_retcode restore_bkgd_bkgd      (mng_datap  pData);
mng_retcode restore_bkgd_bgcolor   (mng_datap  pData);
mng_retcode restore_bkgd_rgb8      (mng_datap  pData);
mng_retcode restore_bkgd_bgr8      (mng_datap  pData);

/* ************************************************************************** */
/* *                                                                        * */
/* * Row retrieval routines - retrieve processed & uncompressed row-data    * */
/* * from the current "object"                                              * */
/* *                                                                        * */
/* ************************************************************************** */

mng_retcode retrieve_g8            (mng_datap  pData);
mng_retcode retrieve_g16           (mng_datap  pData);
mng_retcode retrieve_rgb8          (mng_datap  pData);
mng_retcode retrieve_rgb16         (mng_datap  pData);
mng_retcode retrieve_idx8          (mng_datap  pData);
mng_retcode retrieve_ga8           (mng_datap  pData);
mng_retcode retrieve_ga16          (mng_datap  pData);
mng_retcode retrieve_rgba8         (mng_datap  pData);
mng_retcode retrieve_rgba16        (mng_datap  pData);

/* ************************************************************************** */
/* *                                                                        * */
/* * Row storage routines - store processed & uncompressed row-data         * */
/* * into the current "object"                                              * */
/* *                                                                        * */
/* ************************************************************************** */

mng_retcode store_g1               (mng_datap  pData);
mng_retcode store_g2               (mng_datap  pData);
mng_retcode store_g4               (mng_datap  pData);
mng_retcode store_g8               (mng_datap  pData);
mng_retcode store_g16              (mng_datap  pData);
mng_retcode store_rgb8             (mng_datap  pData);
mng_retcode store_rgb16            (mng_datap  pData);
mng_retcode store_idx1             (mng_datap  pData);
mng_retcode store_idx2             (mng_datap  pData);
mng_retcode store_idx4             (mng_datap  pData);
mng_retcode store_idx8             (mng_datap  pData);
mng_retcode store_ga8              (mng_datap  pData);
mng_retcode store_ga16             (mng_datap  pData);
mng_retcode store_rgba8            (mng_datap  pData);
mng_retcode store_rgba16           (mng_datap  pData);

/* ************************************************************************** */
/* *                                                                        * */
/* * Row storage routines (JPEG) - store processed & uncompressed row-data  * */
/* * into the current "object"                                              * */
/* *                                                                        * */
/* ************************************************************************** */

mng_retcode store_jpeg_g8          (mng_datap  pData);
mng_retcode store_jpeg_rgb8        (mng_datap  pData);
mng_retcode store_jpeg_ga8         (mng_datap  pData);
mng_retcode store_jpeg_rgba8       (mng_datap  pData);

mng_retcode store_jpeg_g12         (mng_datap  pData);
mng_retcode store_jpeg_rgb12       (mng_datap  pData);
mng_retcode store_jpeg_ga12        (mng_datap  pData);
mng_retcode store_jpeg_rgba12      (mng_datap  pData);

mng_retcode store_jpeg_g8_alpha    (mng_datap  pData);
mng_retcode store_jpeg_rgb8_alpha  (mng_datap  pData);

mng_retcode store_jpeg_g8_a1       (mng_datap  pData);
mng_retcode store_jpeg_g8_a2       (mng_datap  pData);
mng_retcode store_jpeg_g8_a4       (mng_datap  pData);
mng_retcode store_jpeg_g8_a8       (mng_datap  pData);
mng_retcode store_jpeg_g8_a16      (mng_datap  pData);

mng_retcode store_jpeg_rgb8_a1     (mng_datap  pData);
mng_retcode store_jpeg_rgb8_a2     (mng_datap  pData);
mng_retcode store_jpeg_rgb8_a4     (mng_datap  pData);
mng_retcode store_jpeg_rgb8_a8     (mng_datap  pData);
mng_retcode store_jpeg_rgb8_a16    (mng_datap  pData);

mng_retcode store_jpeg_g12_a1      (mng_datap  pData);
mng_retcode store_jpeg_g12_a2      (mng_datap  pData);
mng_retcode store_jpeg_g12_a4      (mng_datap  pData);
mng_retcode store_jpeg_g12_a8      (mng_datap  pData);
mng_retcode store_jpeg_g12_a16     (mng_datap  pData);

mng_retcode store_jpeg_rgb12_a1    (mng_datap  pData);
mng_retcode store_jpeg_rgb12_a2    (mng_datap  pData);
mng_retcode store_jpeg_rgb12_a4    (mng_datap  pData);
mng_retcode store_jpeg_rgb12_a8    (mng_datap  pData);
mng_retcode store_jpeg_rgb12_a16   (mng_datap  pData);

/* ************************************************************************** */
/* *                                                                        * */
/* * Delta-image row routines - apply the processed & uncompressed row-data * */
/* * onto the target "object"                                               * */
/* *                                                                        * */
/* ************************************************************************** */

mng_retcode delta_g1               (mng_datap  pData);
mng_retcode delta_g2               (mng_datap  pData);
mng_retcode delta_g4               (mng_datap  pData);
mng_retcode delta_g8               (mng_datap  pData);
mng_retcode delta_g16              (mng_datap  pData);
mng_retcode delta_rgb8             (mng_datap  pData);
mng_retcode delta_rgb16            (mng_datap  pData);
mng_retcode delta_idx1             (mng_datap  pData);
mng_retcode delta_idx2             (mng_datap  pData);
mng_retcode delta_idx4             (mng_datap  pData);
mng_retcode delta_idx8             (mng_datap  pData);
mng_retcode delta_ga8              (mng_datap  pData);
mng_retcode delta_ga16             (mng_datap  pData);
mng_retcode delta_rgba8            (mng_datap  pData);
mng_retcode delta_rgba16           (mng_datap  pData);

/* ************************************************************************** */
/* *                                                                        * */
/* * Delta-image row routines - apply the source row onto the target        * */
/* *                                                                        * */
/* ************************************************************************** */

mng_retcode delta_g1_g1            (mng_datap  pData);
mng_retcode delta_g2_g2            (mng_datap  pData);
mng_retcode delta_g4_g4            (mng_datap  pData);
mng_retcode delta_g8_g8            (mng_datap  pData);
mng_retcode delta_g16_g16          (mng_datap  pData);
mng_retcode delta_rgb8_rgb8        (mng_datap  pData);
mng_retcode delta_rgb16_rgb16      (mng_datap  pData);
mng_retcode delta_ga8_ga8          (mng_datap  pData);
mng_retcode delta_ga8_g8           (mng_datap  pData);
mng_retcode delta_ga8_a8           (mng_datap  pData);
mng_retcode delta_ga16_ga16        (mng_datap  pData);
mng_retcode delta_ga16_g16         (mng_datap  pData);
mng_retcode delta_ga16_a16         (mng_datap  pData);
mng_retcode delta_rgba8_rgba8      (mng_datap  pData);
mng_retcode delta_rgba8_rgb8       (mng_datap  pData);
mng_retcode delta_rgba8_a8         (mng_datap  pData);
mng_retcode delta_rgba16_rgba16    (mng_datap  pData);
mng_retcode delta_rgba16_rgb16     (mng_datap  pData);
mng_retcode delta_rgba16_a16       (mng_datap  pData);

/* ************************************************************************** */
/* *                                                                        * */
/* * Delta-image row routines - scale the source to bitdepth of target      * */
/* *                                                                        * */
/* ************************************************************************** */



/* ************************************************************************** */
/* *                                                                        * */
/* * Row processing routines - convert uncompressed data from zlib to       * */
/* * managable row-data which serves as input to the color-management       * */
/* * routines                                                               * */
/* *                                                                        * */
/* ************************************************************************** */

mng_retcode process_g1             (mng_datap  pData);
mng_retcode process_g2             (mng_datap  pData);
mng_retcode process_g4             (mng_datap  pData);
mng_retcode process_g8             (mng_datap  pData);
mng_retcode process_g16            (mng_datap  pData);
mng_retcode process_rgb8           (mng_datap  pData);
mng_retcode process_rgb16          (mng_datap  pData);
mng_retcode process_idx1           (mng_datap  pData);
mng_retcode process_idx2           (mng_datap  pData);
mng_retcode process_idx4           (mng_datap  pData);
mng_retcode process_idx8           (mng_datap  pData);
mng_retcode process_ga8            (mng_datap  pData);
mng_retcode process_ga16           (mng_datap  pData);
mng_retcode process_rgba8          (mng_datap  pData);
mng_retcode process_rgba16         (mng_datap  pData);

/* ************************************************************************** */
/* *                                                                        * */
/* * Row processing initialization routines - set up the variables needed   * */
/* * to process uncompressed row-data                                       * */
/* *                                                                        * */
/* ************************************************************************** */

mng_retcode init_g1_ni             (mng_datap  pData);
mng_retcode init_g1_i              (mng_datap  pData);
mng_retcode init_g2_ni             (mng_datap  pData);
mng_retcode init_g2_i              (mng_datap  pData);
mng_retcode init_g4_ni             (mng_datap  pData);
mng_retcode init_g4_i              (mng_datap  pData);
mng_retcode init_g8_ni             (mng_datap  pData);
mng_retcode init_g8_i              (mng_datap  pData);
mng_retcode init_g16_ni            (mng_datap  pData);
mng_retcode init_g16_i             (mng_datap  pData);
mng_retcode init_rgb8_ni           (mng_datap  pData);
mng_retcode init_rgb8_i            (mng_datap  pData);
mng_retcode init_rgb16_ni          (mng_datap  pData);
mng_retcode init_rgb16_i           (mng_datap  pData);
mng_retcode init_idx1_ni           (mng_datap  pData);
mng_retcode init_idx1_i            (mng_datap  pData);
mng_retcode init_idx2_ni           (mng_datap  pData);
mng_retcode init_idx2_i            (mng_datap  pData);
mng_retcode init_idx4_ni           (mng_datap  pData);
mng_retcode init_idx4_i            (mng_datap  pData);
mng_retcode init_idx8_ni           (mng_datap  pData);
mng_retcode init_idx8_i            (mng_datap  pData);
mng_retcode init_ga8_ni            (mng_datap  pData);
mng_retcode init_ga8_i             (mng_datap  pData);
mng_retcode init_ga16_ni           (mng_datap  pData);
mng_retcode init_ga16_i            (mng_datap  pData);
mng_retcode init_rgba8_ni          (mng_datap  pData);
mng_retcode init_rgba8_i           (mng_datap  pData);
mng_retcode init_rgba16_ni         (mng_datap  pData);
mng_retcode init_rgba16_i          (mng_datap  pData);

/* ************************************************************************** */
/* *                                                                        * */
/* * Row processing initialization routines (JPEG) - set up the variables   * */
/* * needed to process uncompressed row-data                                * */
/* *                                                                        * */
/* ************************************************************************** */

mng_retcode init_jpeg_a1_ni        (mng_datap  pData);
mng_retcode init_jpeg_a2_ni        (mng_datap  pData);
mng_retcode init_jpeg_a4_ni        (mng_datap  pData);
mng_retcode init_jpeg_a8_ni        (mng_datap  pData);
mng_retcode init_jpeg_a16_ni       (mng_datap  pData);

/* ************************************************************************** */
/* *                                                                        * */
/* * General row processing routines                                        * */
/* *                                                                        * */
/* ************************************************************************** */

mng_retcode init_rowproc           (mng_datap  pData);
mng_retcode next_row               (mng_datap  pData);
mng_retcode next_jpeg_alpharow     (mng_datap  pData);
mng_retcode next_jpeg_row          (mng_datap  pData);
mng_retcode cleanup_rowproc        (mng_datap  pData);

/* ************************************************************************** */
/* *                                                                        * */
/* * Magnification row routines - apply magnification transforms            * */
/* *                                                                        * */
/* ************************************************************************** */

mng_retcode magnify_g8_x1          (mng_datap  pData,
                                    mng_uint16 iMX,
                                    mng_uint16 iML,
                                    mng_uint16 iMR,
                                    mng_uint32 iWidth,
                                    mng_uint8p pSrcline,
                                    mng_uint8p pDstline);
mng_retcode magnify_g8_x2          (mng_datap  pData,
                                    mng_uint16 iMX,
                                    mng_uint16 iML,
                                    mng_uint16 iMR,
                                    mng_uint32 iWidth,
                                    mng_uint8p pSrcline,
                                    mng_uint8p pDstline);
mng_retcode magnify_g8_x3          (mng_datap  pData,
                                    mng_uint16 iMX,
                                    mng_uint16 iML,
                                    mng_uint16 iMR,
                                    mng_uint32 iWidth,
                                    mng_uint8p pSrcline,
                                    mng_uint8p pDstline);
mng_retcode magnify_rgb8_x1        (mng_datap  pData,
                                    mng_uint16 iMX,
                                    mng_uint16 iML,
                                    mng_uint16 iMR,
                                    mng_uint32 iWidth,
                                    mng_uint8p pSrcline,
                                    mng_uint8p pDstline);
mng_retcode magnify_rgb8_x2        (mng_datap  pData,
                                    mng_uint16 iMX,
                                    mng_uint16 iML,
                                    mng_uint16 iMR,
                                    mng_uint32 iWidth,
                                    mng_uint8p pSrcline,
                                    mng_uint8p pDstline);
mng_retcode magnify_rgb8_x3        (mng_datap  pData,
                                    mng_uint16 iMX,
                                    mng_uint16 iML,
                                    mng_uint16 iMR,
                                    mng_uint32 iWidth,
                                    mng_uint8p pSrcline,
                                    mng_uint8p pDstline);
mng_retcode magnify_ga8_x1         (mng_datap  pData,
                                    mng_uint16 iMX,
                                    mng_uint16 iML,
                                    mng_uint16 iMR,
                                    mng_uint32 iWidth,
                                    mng_uint8p pSrcline,
                                    mng_uint8p pDstline);
mng_retcode magnify_ga8_x2         (mng_datap  pData,
                                    mng_uint16 iMX,
                                    mng_uint16 iML,
                                    mng_uint16 iMR,
                                    mng_uint32 iWidth,
                                    mng_uint8p pSrcline,
                                    mng_uint8p pDstline);
mng_retcode magnify_ga8_x3         (mng_datap  pData,
                                    mng_uint16 iMX,
                                    mng_uint16 iML,
                                    mng_uint16 iMR,
                                    mng_uint32 iWidth,
                                    mng_uint8p pSrcline,
                                    mng_uint8p pDstline);
mng_retcode magnify_ga8_x4         (mng_datap  pData,
                                    mng_uint16 iMX,
                                    mng_uint16 iML,
                                    mng_uint16 iMR,
                                    mng_uint32 iWidth,
                                    mng_uint8p pSrcline,
                                    mng_uint8p pDstline);
mng_retcode magnify_ga8_x5         (mng_datap  pData,
                                    mng_uint16 iMX,
                                    mng_uint16 iML,
                                    mng_uint16 iMR,
                                    mng_uint32 iWidth,
                                    mng_uint8p pSrcline,
                                    mng_uint8p pDstline);
mng_retcode magnify_rgba8_x1       (mng_datap  pData,
                                    mng_uint16 iMX,
                                    mng_uint16 iML,
                                    mng_uint16 iMR,
                                    mng_uint32 iWidth,
                                    mng_uint8p pSrcline,
                                    mng_uint8p pDstline);
mng_retcode magnify_rgba8_x2       (mng_datap  pData,
                                    mng_uint16 iMX,
                                    mng_uint16 iML,
                                    mng_uint16 iMR,
                                    mng_uint32 iWidth,
                                    mng_uint8p pSrcline,
                                    mng_uint8p pDstline);
mng_retcode magnify_rgba8_x3       (mng_datap  pData,
                                    mng_uint16 iMX,
                                    mng_uint16 iML,
                                    mng_uint16 iMR,
                                    mng_uint32 iWidth,
                                    mng_uint8p pSrcline,
                                    mng_uint8p pDstline);
mng_retcode magnify_rgba8_x4       (mng_datap  pData,
                                    mng_uint16 iMX,
                                    mng_uint16 iML,
                                    mng_uint16 iMR,
                                    mng_uint32 iWidth,
                                    mng_uint8p pSrcline,
                                    mng_uint8p pDstline);
mng_retcode magnify_rgba8_x5       (mng_datap  pData,
                                    mng_uint16 iMX,
                                    mng_uint16 iML,
                                    mng_uint16 iMR,
                                    mng_uint32 iWidth,
                                    mng_uint8p pSrcline,
                                    mng_uint8p pDstline);

mng_retcode magnify_g8_y1          (mng_datap  pData,
                                    mng_int32  iS,
                                    mng_int32  iM,
                                    mng_uint32 iWidth,
                                    mng_uint8p pSrcline1,
                                    mng_uint8p pSrcline2,
                                    mng_uint8p pDstline);
mng_retcode magnify_g8_y2          (mng_datap  pData,
                                    mng_int32  iS,
                                    mng_int32  iM,
                                    mng_uint32 iWidth,
                                    mng_uint8p pSrcline1,
                                    mng_uint8p pSrcline2,
                                    mng_uint8p pDstline);
mng_retcode magnify_g8_y3          (mng_datap  pData,
                                    mng_int32  iS,
                                    mng_int32  iM,
                                    mng_uint32 iWidth,
                                    mng_uint8p pSrcline1,
                                    mng_uint8p pSrcline2,
                                    mng_uint8p pDstline);
mng_retcode magnify_rgb8_y1        (mng_datap  pData,
                                    mng_int32  iS,
                                    mng_int32  iM,
                                    mng_uint32 iWidth,
                                    mng_uint8p pSrcline1,
                                    mng_uint8p pSrcline2,
                                    mng_uint8p pDstline);
mng_retcode magnify_rgb8_y2        (mng_datap  pData,
                                    mng_int32  iS,
                                    mng_int32  iM,
                                    mng_uint32 iWidth,
                                    mng_uint8p pSrcline1,
                                    mng_uint8p pSrcline2,
                                    mng_uint8p pDstline);
mng_retcode magnify_rgb8_y3        (mng_datap  pData,
                                    mng_int32  iS,
                                    mng_int32  iM,
                                    mng_uint32 iWidth,
                                    mng_uint8p pSrcline1,
                                    mng_uint8p pSrcline2,
                                    mng_uint8p pDstline);
mng_retcode magnify_ga8_y1         (mng_datap  pData,
                                    mng_int32  iS,
                                    mng_int32  iM,
                                    mng_uint32 iWidth,
                                    mng_uint8p pSrcline1,
                                    mng_uint8p pSrcline2,
                                    mng_uint8p pDstline);
mng_retcode magnify_ga8_y2         (mng_datap  pData,
                                    mng_int32  iS,
                                    mng_int32  iM,
                                    mng_uint32 iWidth,
                                    mng_uint8p pSrcline1,
                                    mng_uint8p pSrcline2,
                                    mng_uint8p pDstline);
mng_retcode magnify_ga8_y3         (mng_datap  pData,
                                    mng_int32  iS,
                                    mng_int32  iM,
                                    mng_uint32 iWidth,
                                    mng_uint8p pSrcline1,
                                    mng_uint8p pSrcline2,
                                    mng_uint8p pDstline);
mng_retcode magnify_ga8_y4         (mng_datap  pData,
                                    mng_int32  iS,
                                    mng_int32  iM,
                                    mng_uint32 iWidth,
                                    mng_uint8p pSrcline1,
                                    mng_uint8p pSrcline2,
                                    mng_uint8p pDstline);
mng_retcode magnify_ga8_y5         (mng_datap  pData,
                                    mng_int32  iS,
                                    mng_int32  iM,
                                    mng_uint32 iWidth,
                                    mng_uint8p pSrcline1,
                                    mng_uint8p pSrcline2,
                                    mng_uint8p pDstline);
mng_retcode magnify_rgba8_y1       (mng_datap  pData,
                                    mng_int32  iS,
                                    mng_int32  iM,
                                    mng_uint32 iWidth,
                                    mng_uint8p pSrcline1,
                                    mng_uint8p pSrcline2,
                                    mng_uint8p pDstline);
mng_retcode magnify_rgba8_y2       (mng_datap  pData,
                                    mng_int32  iS,
                                    mng_int32  iM,
                                    mng_uint32 iWidth,
                                    mng_uint8p pSrcline1,
                                    mng_uint8p pSrcline2,
                                    mng_uint8p pDstline);
mng_retcode magnify_rgba8_y3       (mng_datap  pData,
                                    mng_int32  iS,
                                    mng_int32  iM,
                                    mng_uint32 iWidth,
                                    mng_uint8p pSrcline1,
                                    mng_uint8p pSrcline2,
                                    mng_uint8p pDstline);
mng_retcode magnify_rgba8_y4       (mng_datap  pData,
                                    mng_int32  iS,
                                    mng_int32  iM,
                                    mng_uint32 iWidth,
                                    mng_uint8p pSrcline1,
                                    mng_uint8p pSrcline2,
                                    mng_uint8p pDstline);
mng_retcode magnify_rgba8_y5       (mng_datap  pData,
                                    mng_int32  iS,
                                    mng_int32  iM,
                                    mng_uint32 iWidth,
                                    mng_uint8p pSrcline1,
                                    mng_uint8p pSrcline2,
                                    mng_uint8p pDstline);

/* ************************************************************************** */

#endif /* _libmng_pixels_h_ */

/* ************************************************************************** */
/* * end of file                                                            * */
/* ************************************************************************** */
