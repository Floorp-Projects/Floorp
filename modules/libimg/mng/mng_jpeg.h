/* ************************************************************************** */
/* *             For conditions of distribution and use,                    * */
/* *                see copyright notice in libmng.h                        * */
/* ************************************************************************** */
/* *                                                                        * */
/* * project   : libmng                                                     * */
/* * file      : mng_jpeg.h                copyright (c) 2000 G.Juyn        * */
/* * version   : 0.9.0                                                      * */
/* *                                                                        * */
/* * purpose   : JPEG library interface (definition)                        * */
/* *                                                                        * */
/* * author    : G.Juyn                                                     * */
/* * web       : http://www.3-t.com                                         * */
/* * email     : mailto:info@3-t.com                                        * */
/* *                                                                        * */
/* * comment   : Definition of the JPEG library interface                   * */
/* *                                                                        * */
/* * changes   : 0.5.1 - 05/08/2000 - G.Juyn                                * */
/* *             - changed strict-ANSI stuff                                * */
/* *                                                                        * */
/* ************************************************************************** */

#if defined(__BORLANDC__) && defined(MNG_STRICT_ANSI)
#pragma option -A                      /* force ANSI-C */
#endif

#ifndef _mng_jpeg_h_
#define _mng_jpeg_h_

/* ************************************************************************** */

mng_retcode mngjpeg_initialize     (mng_datap  pData);
mng_retcode mngjpeg_cleanup        (mng_datap  pData);

mng_retcode mngjpeg_decompressinit (mng_datap  pData);
mng_retcode mngjpeg_decompressdata (mng_datap  pData,
                                    mng_uint32 iRawsize,
                                    mng_uint8p pRawdata);
mng_retcode mngjpeg_decompressfree (mng_datap  pData);

/* ************************************************************************** */

#endif /* _mng_jpeg_h_ */

/* ************************************************************************** */
/* * end of file                                                            * */
/* ************************************************************************** */
