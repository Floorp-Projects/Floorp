/* ************************************************************************** */
/* *             For conditions of distribution and use,                    * */
/* *                see copyright notice in libmng.h                        * */
/* ************************************************************************** */
/* *                                                                        * */
/* * project   : libmng                                                     * */
/* * file      : mng_zlib.h                copyright (c) 2000 G.Juyn        * */
/* * version   : 0.9.0                                                      * */
/* *                                                                        * */
/* * purpose   : ZLIB package interface (definition)                        * */
/* *                                                                        * */
/* * author    : G.Juyn                                                     * */
/* * web       : http://www.3-t.com                                         * */
/* * email     : mailto:info@3-t.com                                        * */
/* *                                                                        * */
/* * comment   : Definition of the ZLIB package interface                   * */
/* *                                                                        * */
/* * changes   : 0.5.1 - 05/08/2000 - G.Juyn                                * */
/* *             - changed strict-ANSI stuff                                * */
/* *                                                                        * */
/* ************************************************************************** */

#if defined(__BORLANDC__) && defined(MNG_STRICT_ANSI)
#pragma option -A                      /* force ANSI-C */
#endif

#ifndef _mng_zlib_h_
#define _mng_zlib_h_

#include "libmng.h"
#include "mng_data.h"

/* ************************************************************************** */

mng_retcode mngzlib_initialize  (mng_datap pData);
mng_retcode mngzlib_cleanup     (mng_datap pData);

mng_retcode mngzlib_inflateinit (mng_datap pData);
mng_retcode mngzlib_inflaterows (mng_datap  pData,
                                 mng_uint32 iInlen,
                                 mng_uint8p pIndata);
mng_retcode mngzlib_inflatedata (mng_datap  pData,
                                 mng_uint32 iInlen,
                                 mng_uint8p pIndata);
mng_retcode mngzlib_inflatefree (mng_datap pData);

mng_retcode mngzlib_deflateinit (mng_datap pData);
mng_retcode mngzlib_deflaterows (mng_datap  pData,
                                 mng_uint32 iInlen,
                                 mng_uint8p pIndata);
mng_retcode mngzlib_deflatedata (mng_datap  pData,
                                 mng_uint32 iInlen,
                                 mng_uint8p pIndata);
mng_retcode mngzlib_deflatefree (mng_datap pData);

/* ************************************************************************** */

#endif /* _mng_zlib_h_ */

/* ************************************************************************** */
/* * end of file                                                            * */
/* ************************************************************************** */
