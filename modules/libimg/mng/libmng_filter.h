/* ************************************************************************** */
/* *             For conditions of distribution and use,                    * */
/* *                see copyright notice in libmng.h                        * */
/* ************************************************************************** */
/* *                                                                        * */
/* * project   : libmng                                                     * */
/* * file      : libmng_filter.h           copyright (c) 2000 G.Juyn        * */
/* * version   : 0.9.2                                                      * */
/* *                                                                        * */
/* * purpose   : Filtering routines (definition)                            * */
/* *                                                                        * */
/* * author    : G.Juyn                                                     * */
/* * web       : http://www.3-t.com                                         * */
/* * email     : mailto:info@3-t.com                                        * */
/* *                                                                        * */
/* * comment   : Definition of the filtering routines                       * */
/* *                                                                        * */
/* * changes   : 0.5.1 - 05/08/2000 - G.Juyn                                * */
/* *             - changed strict-ANSI stuff                                * */
/* *                                                                        * */
/* *             0.9.2 - 08/05/2000 - G.Juyn                                * */
/* *             - changed file-prefixes                                    * */
/* *                                                                        * */
/* ************************************************************************** */

#if defined(__BORLANDC__) && defined(MNG_STRICT_ANSI)
#pragma option -A                      /* force ANSI-C */
#endif

#ifndef _libmng_filter_h_
#define _libmng_filter_h_

#include "libmng.h"
#include "libmng_data.h"

/* ************************************************************************** */

mng_retcode filter_a_row   (mng_datap pData);

mng_retcode filter_sub     (mng_datap pData);
mng_retcode filter_up      (mng_datap pData);
mng_retcode filter_average (mng_datap pData);
mng_retcode filter_paeth   (mng_datap pData);

/* ************************************************************************** */

#endif /* _libmng_filter_h_ */

/* ************************************************************************** */
/* * end of file                                                            * */
/* ************************************************************************** */
