/* ************************************************************************** */
/* *             For conditions of distribution and use,                    * */
/* *                see copyright notice in libmng.h                        * */
/* ************************************************************************** */
/* *                                                                        * */
/* * project   : libmng                                                     * */
/* * file      : libmng_chunk_prc.h        copyright (c) 2000 G.Juyn        * */
/* * version   : 1.0.0                                                      * */
/* *                                                                        * */
/* * purpose   : Chunk initialization & cleanup (definition)                * */
/* *                                                                        * */
/* * author    : G.Juyn                                                     * */
/* * web       : http://www.3-t.com                                         * */
/* * email     : mailto:info@3-t.com                                        * */
/* *                                                                        * */
/* * comment   : definition of the chunk initialization & cleanup routines  * */
/* *                                                                        * */
/* * changes   : 0.5.1 - 05/08/2000 - G.Juyn                                * */
/* *             - changed strict-ANSI stuff                                * */
/* *                                                                        * */
/* *             0.9.2 - 08/05/2000 - G.Juyn                                * */
/* *             - changed file-prefixes                                    * */
/* *                                                                        * */
/* *             0.9.3 - 08/26/2000 - G.Juyn                                * */
/* *             - added MAGN chunk                                         * */
/* *             0.9.3 - 10/16/2000 - G.Juyn                                * */
/* *             - added support for JDAA                                   * */
/* *                                                                        * */
/* ************************************************************************** */

#if defined(__BORLANDC__) && defined(MNG_STRICT_ANSI)
#pragma option -A                      /* force ANSI-C */
#endif

#ifndef _libmng_chunk_prc_h_
#define _libmng_chunk_prc_h_

/* ************************************************************************** */

void add_chunk (mng_datap  pData,
                mng_chunkp pChunk);

/* ************************************************************************** */

#define INIT_CHUNK_HDR(n) mng_retcode n (mng_datap   pData,    \
                                         mng_chunkp  pHeader,  \
                                         mng_chunkp* ppChunk)

INIT_CHUNK_HDR (init_ihdr) ;
INIT_CHUNK_HDR (init_plte) ;
INIT_CHUNK_HDR (init_idat) ;
INIT_CHUNK_HDR (init_iend) ;
INIT_CHUNK_HDR (init_trns) ;
INIT_CHUNK_HDR (init_gama) ;
INIT_CHUNK_HDR (init_chrm) ;
INIT_CHUNK_HDR (init_srgb) ;
INIT_CHUNK_HDR (init_iccp) ;
INIT_CHUNK_HDR (init_text) ;
INIT_CHUNK_HDR (init_ztxt) ;
INIT_CHUNK_HDR (init_itxt) ;
INIT_CHUNK_HDR (init_bkgd) ;
INIT_CHUNK_HDR (init_phys) ;
INIT_CHUNK_HDR (init_sbit) ;
INIT_CHUNK_HDR (init_splt) ;
INIT_CHUNK_HDR (init_hist) ;
INIT_CHUNK_HDR (init_time) ;
INIT_CHUNK_HDR (init_mhdr) ;
INIT_CHUNK_HDR (init_mend) ;
INIT_CHUNK_HDR (init_loop) ;
INIT_CHUNK_HDR (init_endl) ;
INIT_CHUNK_HDR (init_defi) ;
INIT_CHUNK_HDR (init_basi) ;
INIT_CHUNK_HDR (init_clon) ;
INIT_CHUNK_HDR (init_past) ;
INIT_CHUNK_HDR (init_disc) ;
INIT_CHUNK_HDR (init_back) ;
INIT_CHUNK_HDR (init_fram) ;
INIT_CHUNK_HDR (init_move) ;
INIT_CHUNK_HDR (init_clip) ;
INIT_CHUNK_HDR (init_show) ;
INIT_CHUNK_HDR (init_term) ;
INIT_CHUNK_HDR (init_save) ;
INIT_CHUNK_HDR (init_seek) ;
INIT_CHUNK_HDR (init_expi) ;
INIT_CHUNK_HDR (init_fpri) ;
INIT_CHUNK_HDR (init_need) ;
INIT_CHUNK_HDR (init_phyg) ;
INIT_CHUNK_HDR (init_jhdr) ;
INIT_CHUNK_HDR (init_jdaa) ;
INIT_CHUNK_HDR (init_jdat) ;
INIT_CHUNK_HDR (init_jsep) ;
INIT_CHUNK_HDR (init_dhdr) ;
INIT_CHUNK_HDR (init_prom) ;
INIT_CHUNK_HDR (init_ipng) ;
INIT_CHUNK_HDR (init_pplt) ;
INIT_CHUNK_HDR (init_ijng) ;
INIT_CHUNK_HDR (init_drop) ;
INIT_CHUNK_HDR (init_dbyk) ;
INIT_CHUNK_HDR (init_ordr) ;
INIT_CHUNK_HDR (init_magn) ;
INIT_CHUNK_HDR (init_unknown) ;

/* ************************************************************************** */

#define FREE_CHUNK_HDR(n) mng_retcode n (mng_datap   pData,    \
                                         mng_chunkp  pHeader)

FREE_CHUNK_HDR (free_ihdr) ;
FREE_CHUNK_HDR (free_plte) ;
FREE_CHUNK_HDR (free_idat) ;
FREE_CHUNK_HDR (free_iend) ;
FREE_CHUNK_HDR (free_trns) ;
FREE_CHUNK_HDR (free_gama) ;
FREE_CHUNK_HDR (free_chrm) ;
FREE_CHUNK_HDR (free_srgb) ;
FREE_CHUNK_HDR (free_iccp) ;
FREE_CHUNK_HDR (free_text) ;
FREE_CHUNK_HDR (free_ztxt) ;
FREE_CHUNK_HDR (free_itxt) ;
FREE_CHUNK_HDR (free_bkgd) ;
FREE_CHUNK_HDR (free_phys) ;
FREE_CHUNK_HDR (free_sbit) ;
FREE_CHUNK_HDR (free_splt) ;
FREE_CHUNK_HDR (free_hist) ;
FREE_CHUNK_HDR (free_time) ;
FREE_CHUNK_HDR (free_mhdr) ;
FREE_CHUNK_HDR (free_mend) ;
FREE_CHUNK_HDR (free_loop) ;
FREE_CHUNK_HDR (free_endl) ;
FREE_CHUNK_HDR (free_defi) ;
FREE_CHUNK_HDR (free_basi) ;
FREE_CHUNK_HDR (free_clon) ;
FREE_CHUNK_HDR (free_past) ;
FREE_CHUNK_HDR (free_disc) ;
FREE_CHUNK_HDR (free_back) ;
FREE_CHUNK_HDR (free_fram) ;
FREE_CHUNK_HDR (free_move) ;
FREE_CHUNK_HDR (free_clip) ;
FREE_CHUNK_HDR (free_show) ;
FREE_CHUNK_HDR (free_term) ;
FREE_CHUNK_HDR (free_save) ;
FREE_CHUNK_HDR (free_seek) ;
FREE_CHUNK_HDR (free_expi) ;
FREE_CHUNK_HDR (free_fpri) ;
FREE_CHUNK_HDR (free_need) ;
FREE_CHUNK_HDR (free_phyg) ;
FREE_CHUNK_HDR (free_jhdr) ;
FREE_CHUNK_HDR (free_jdaa) ;
FREE_CHUNK_HDR (free_jdat) ;
FREE_CHUNK_HDR (free_jsep) ;
FREE_CHUNK_HDR (free_dhdr) ;
FREE_CHUNK_HDR (free_prom) ;
FREE_CHUNK_HDR (free_ipng) ;
FREE_CHUNK_HDR (free_pplt) ;
FREE_CHUNK_HDR (free_ijng) ;
FREE_CHUNK_HDR (free_drop) ;
FREE_CHUNK_HDR (free_dbyk) ;
FREE_CHUNK_HDR (free_ordr) ;
FREE_CHUNK_HDR (free_magn) ;
FREE_CHUNK_HDR (free_unknown) ;

/* ************************************************************************** */

#endif /* _libmng_chunk_prc_h_ */

/* ************************************************************************** */
/* * end of file                                                            * */
/* ************************************************************************** */
