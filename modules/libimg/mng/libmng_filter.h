/* ************************************************************************** */
/* *             For conditions of distribution and use,                    * */
/* *                see copyright notice in libmng.h                        * */
/* ************************************************************************** */
/* *                                                                        * */
/* * project   : libmng                                                     * */
/* * file      : libmng_filter.h           copyright (c) 2000 G.Juyn        * */
/* * version   : 1.0.0                                                      * */
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
/* *             0.9.3 - 09/07/2000 - G.Juyn                                * */
/* *             - added support for new filter_types                       * */
/* *                                                                        * */
/* ************************************************************************** */

#if defined(__BORLANDC__) && defined(MNG_STRICT_ANSI)
#pragma option -A                      /* force ANSI-C */
#endif

#ifndef _libmng_filter_h_
#define _libmng_filter_h_

/* ************************************************************************** */

mng_retcode filter_a_row      (mng_datap pData);

mng_retcode filter_sub        (mng_datap pData);
mng_retcode filter_up         (mng_datap pData);
mng_retcode filter_average    (mng_datap pData);
mng_retcode filter_paeth      (mng_datap pData);

/* ************************************************************************** */

mng_retcode init_rowdiffering (mng_datap pData);

mng_retcode differ_g1         (mng_datap pData);
mng_retcode differ_g2         (mng_datap pData);
mng_retcode differ_g4         (mng_datap pData);
mng_retcode differ_g8         (mng_datap pData);
mng_retcode differ_g16        (mng_datap pData);
mng_retcode differ_rgb8       (mng_datap pData);
mng_retcode differ_rgb16      (mng_datap pData);
mng_retcode differ_idx1       (mng_datap pData);
mng_retcode differ_idx2       (mng_datap pData);
mng_retcode differ_idx4       (mng_datap pData);
mng_retcode differ_idx8       (mng_datap pData);
mng_retcode differ_ga8        (mng_datap pData);
mng_retcode differ_ga16       (mng_datap pData);
mng_retcode differ_rgba8      (mng_datap pData);
mng_retcode differ_rgba16     (mng_datap pData);

/* ************************************************************************** */

#endif /* _libmng_filter_h_ */

/* ************************************************************************** */
/* * end of file                                                            * */
/* ************************************************************************** */
