/* ************************************************************************** */
/* *             For conditions of distribution and use,                    * */
/* *                see copyright notice in libmng.h                        * */
/* ************************************************************************** */
/* *                                                                        * */
/* * project   : libmng                                                     * */
/* * file      : libmng_chunk_prc.h        copyright (c) 2000-2002 G.Juyn   * */
/* * version   : 1.0.5                                                      * */
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
/* *             1.0.5 - 08/19/2002 - G.Juyn                                * */
/* *             - B597134 - libmng pollutes the linker namespace           * */
/* *             1.0.5 - 09/14/2002 - G.Juyn                                * */
/* *             - added event handling for dynamic MNG                     * */
/* *                                                                        * */
/* ************************************************************************** */

#if defined(__BORLANDC__) && defined(MNG_STRICT_ANSI)
#pragma option -A                      /* force ANSI-C */
#endif

#ifndef _libmng_chunk_prc_h_
#define _libmng_chunk_prc_h_

/* ************************************************************************** */

void mng_add_chunk (mng_datap  pData,
                    mng_chunkp pChunk);

/* ************************************************************************** */

#define INIT_CHUNK_HDR(n) mng_retcode n (mng_datap   pData,    \
                                         mng_chunkp  pHeader,  \
                                         mng_chunkp* ppChunk)

INIT_CHUNK_HDR (mng_init_ihdr) ;
INIT_CHUNK_HDR (mng_init_plte) ;
INIT_CHUNK_HDR (mng_init_idat) ;
INIT_CHUNK_HDR (mng_init_iend) ;
INIT_CHUNK_HDR (mng_init_trns) ;
INIT_CHUNK_HDR (mng_init_gama) ;
INIT_CHUNK_HDR (mng_init_chrm) ;
INIT_CHUNK_HDR (mng_init_srgb) ;
INIT_CHUNK_HDR (mng_init_iccp) ;
INIT_CHUNK_HDR (mng_init_text) ;
INIT_CHUNK_HDR (mng_init_ztxt) ;
INIT_CHUNK_HDR (mng_init_itxt) ;
INIT_CHUNK_HDR (mng_init_bkgd) ;
INIT_CHUNK_HDR (mng_init_phys) ;
INIT_CHUNK_HDR (mng_init_sbit) ;
INIT_CHUNK_HDR (mng_init_splt) ;
INIT_CHUNK_HDR (mng_init_hist) ;
INIT_CHUNK_HDR (mng_init_time) ;
INIT_CHUNK_HDR (mng_init_mhdr) ;
INIT_CHUNK_HDR (mng_init_mend) ;
INIT_CHUNK_HDR (mng_init_loop) ;
INIT_CHUNK_HDR (mng_init_endl) ;
INIT_CHUNK_HDR (mng_init_defi) ;
INIT_CHUNK_HDR (mng_init_basi) ;
INIT_CHUNK_HDR (mng_init_clon) ;
INIT_CHUNK_HDR (mng_init_past) ;
INIT_CHUNK_HDR (mng_init_disc) ;
INIT_CHUNK_HDR (mng_init_back) ;
INIT_CHUNK_HDR (mng_init_fram) ;
INIT_CHUNK_HDR (mng_init_move) ;
INIT_CHUNK_HDR (mng_init_clip) ;
INIT_CHUNK_HDR (mng_init_show) ;
INIT_CHUNK_HDR (mng_init_term) ;
INIT_CHUNK_HDR (mng_init_save) ;
INIT_CHUNK_HDR (mng_init_seek) ;
INIT_CHUNK_HDR (mng_init_expi) ;
INIT_CHUNK_HDR (mng_init_fpri) ;
INIT_CHUNK_HDR (mng_init_need) ;
INIT_CHUNK_HDR (mng_init_phyg) ;
INIT_CHUNK_HDR (mng_init_jhdr) ;
INIT_CHUNK_HDR (mng_init_jdaa) ;
INIT_CHUNK_HDR (mng_init_jdat) ;
INIT_CHUNK_HDR (mng_init_jsep) ;
INIT_CHUNK_HDR (mng_init_dhdr) ;
INIT_CHUNK_HDR (mng_init_prom) ;
INIT_CHUNK_HDR (mng_init_ipng) ;
INIT_CHUNK_HDR (mng_init_pplt) ;
INIT_CHUNK_HDR (mng_init_ijng) ;
INIT_CHUNK_HDR (mng_init_drop) ;
INIT_CHUNK_HDR (mng_init_dbyk) ;
INIT_CHUNK_HDR (mng_init_ordr) ;
INIT_CHUNK_HDR (mng_init_magn) ;
INIT_CHUNK_HDR (mng_init_evnt) ;
INIT_CHUNK_HDR (mng_init_unknown) ;

/* ************************************************************************** */

#define FREE_CHUNK_HDR(n) mng_retcode n (mng_datap   pData,    \
                                         mng_chunkp  pHeader)

FREE_CHUNK_HDR (mng_free_ihdr) ;
FREE_CHUNK_HDR (mng_free_plte) ;
FREE_CHUNK_HDR (mng_free_idat) ;
FREE_CHUNK_HDR (mng_free_iend) ;
FREE_CHUNK_HDR (mng_free_trns) ;
FREE_CHUNK_HDR (mng_free_gama) ;
FREE_CHUNK_HDR (mng_free_chrm) ;
FREE_CHUNK_HDR (mng_free_srgb) ;
FREE_CHUNK_HDR (mng_free_iccp) ;
FREE_CHUNK_HDR (mng_free_text) ;
FREE_CHUNK_HDR (mng_free_ztxt) ;
FREE_CHUNK_HDR (mng_free_itxt) ;
FREE_CHUNK_HDR (mng_free_bkgd) ;
FREE_CHUNK_HDR (mng_free_phys) ;
FREE_CHUNK_HDR (mng_free_sbit) ;
FREE_CHUNK_HDR (mng_free_splt) ;
FREE_CHUNK_HDR (mng_free_hist) ;
FREE_CHUNK_HDR (mng_free_time) ;
FREE_CHUNK_HDR (mng_free_mhdr) ;
FREE_CHUNK_HDR (mng_free_mend) ;
FREE_CHUNK_HDR (mng_free_loop) ;
FREE_CHUNK_HDR (mng_free_endl) ;
FREE_CHUNK_HDR (mng_free_defi) ;
FREE_CHUNK_HDR (mng_free_basi) ;
FREE_CHUNK_HDR (mng_free_clon) ;
FREE_CHUNK_HDR (mng_free_past) ;
FREE_CHUNK_HDR (mng_free_disc) ;
FREE_CHUNK_HDR (mng_free_back) ;
FREE_CHUNK_HDR (mng_free_fram) ;
FREE_CHUNK_HDR (mng_free_move) ;
FREE_CHUNK_HDR (mng_free_clip) ;
FREE_CHUNK_HDR (mng_free_show) ;
FREE_CHUNK_HDR (mng_free_term) ;
FREE_CHUNK_HDR (mng_free_save) ;
FREE_CHUNK_HDR (mng_free_seek) ;
FREE_CHUNK_HDR (mng_free_expi) ;
FREE_CHUNK_HDR (mng_free_fpri) ;
FREE_CHUNK_HDR (mng_free_need) ;
FREE_CHUNK_HDR (mng_free_phyg) ;
FREE_CHUNK_HDR (mng_free_jhdr) ;
FREE_CHUNK_HDR (mng_free_jdaa) ;
FREE_CHUNK_HDR (mng_free_jdat) ;
FREE_CHUNK_HDR (mng_free_jsep) ;
FREE_CHUNK_HDR (mng_free_dhdr) ;
FREE_CHUNK_HDR (mng_free_prom) ;
FREE_CHUNK_HDR (mng_free_ipng) ;
FREE_CHUNK_HDR (mng_free_pplt) ;
FREE_CHUNK_HDR (mng_free_ijng) ;
FREE_CHUNK_HDR (mng_free_drop) ;
FREE_CHUNK_HDR (mng_free_dbyk) ;
FREE_CHUNK_HDR (mng_free_ordr) ;
FREE_CHUNK_HDR (mng_free_magn) ;
FREE_CHUNK_HDR (mng_free_evnt) ;
FREE_CHUNK_HDR (mng_free_unknown) ;

/* ************************************************************************** */

#ifdef MNG_INCLUDE_WRITE_PROCS

#define ASSIGN_CHUNK_HDR(n) mng_retcode n (mng_datap   pData,    \
                                           mng_chunkp  pChunkto, \
                                           mng_chunkp  pChunkfrom)

ASSIGN_CHUNK_HDR (mng_assign_ihdr) ;
ASSIGN_CHUNK_HDR (mng_assign_plte) ;
ASSIGN_CHUNK_HDR (mng_assign_idat) ;
ASSIGN_CHUNK_HDR (mng_assign_iend) ;
ASSIGN_CHUNK_HDR (mng_assign_trns) ;
ASSIGN_CHUNK_HDR (mng_assign_gama) ;
ASSIGN_CHUNK_HDR (mng_assign_chrm) ;
ASSIGN_CHUNK_HDR (mng_assign_srgb) ;
ASSIGN_CHUNK_HDR (mng_assign_iccp) ;
ASSIGN_CHUNK_HDR (mng_assign_text) ;
ASSIGN_CHUNK_HDR (mng_assign_ztxt) ;
ASSIGN_CHUNK_HDR (mng_assign_itxt) ;
ASSIGN_CHUNK_HDR (mng_assign_bkgd) ;
ASSIGN_CHUNK_HDR (mng_assign_phys) ;
ASSIGN_CHUNK_HDR (mng_assign_sbit) ;
ASSIGN_CHUNK_HDR (mng_assign_splt) ;
ASSIGN_CHUNK_HDR (mng_assign_hist) ;
ASSIGN_CHUNK_HDR (mng_assign_time) ;
ASSIGN_CHUNK_HDR (mng_assign_mhdr) ;
ASSIGN_CHUNK_HDR (mng_assign_mend) ;
ASSIGN_CHUNK_HDR (mng_assign_loop) ;
ASSIGN_CHUNK_HDR (mng_assign_endl) ;
ASSIGN_CHUNK_HDR (mng_assign_defi) ;
ASSIGN_CHUNK_HDR (mng_assign_basi) ;
ASSIGN_CHUNK_HDR (mng_assign_clon) ;
ASSIGN_CHUNK_HDR (mng_assign_past) ;
ASSIGN_CHUNK_HDR (mng_assign_disc) ;
ASSIGN_CHUNK_HDR (mng_assign_back) ;
ASSIGN_CHUNK_HDR (mng_assign_fram) ;
ASSIGN_CHUNK_HDR (mng_assign_move) ;
ASSIGN_CHUNK_HDR (mng_assign_clip) ;
ASSIGN_CHUNK_HDR (mng_assign_show) ;
ASSIGN_CHUNK_HDR (mng_assign_term) ;
ASSIGN_CHUNK_HDR (mng_assign_save) ;
ASSIGN_CHUNK_HDR (mng_assign_seek) ;
ASSIGN_CHUNK_HDR (mng_assign_expi) ;
ASSIGN_CHUNK_HDR (mng_assign_fpri) ;
ASSIGN_CHUNK_HDR (mng_assign_need) ;
ASSIGN_CHUNK_HDR (mng_assign_phyg) ;
ASSIGN_CHUNK_HDR (mng_assign_jhdr) ;
ASSIGN_CHUNK_HDR (mng_assign_jdaa) ;
ASSIGN_CHUNK_HDR (mng_assign_jdat) ;
ASSIGN_CHUNK_HDR (mng_assign_jsep) ;
ASSIGN_CHUNK_HDR (mng_assign_dhdr) ;
ASSIGN_CHUNK_HDR (mng_assign_prom) ;
ASSIGN_CHUNK_HDR (mng_assign_ipng) ;
ASSIGN_CHUNK_HDR (mng_assign_pplt) ;
ASSIGN_CHUNK_HDR (mng_assign_ijng) ;
ASSIGN_CHUNK_HDR (mng_assign_drop) ;
ASSIGN_CHUNK_HDR (mng_assign_dbyk) ;
ASSIGN_CHUNK_HDR (mng_assign_ordr) ;
ASSIGN_CHUNK_HDR (mng_assign_magn) ;
ASSIGN_CHUNK_HDR (mng_assign_evnt) ;
ASSIGN_CHUNK_HDR (mng_assign_unknown) ;

/* ************************************************************************** */

#else /* MNG_INCLUDE_WRITE_PROCS */
#define mng_assign_ihdr 0
#define mng_assign_plte 0
#define mng_assign_idat 0
#define mng_assign_iend 0
#define mng_assign_trns 0
#define mng_assign_gama 0
#define mng_assign_chrm 0
#define mng_assign_srgb 0
#define mng_assign_iccp 0
#define mng_assign_text 0
#define mng_assign_ztxt 0
#define mng_assign_itxt 0
#define mng_assign_bkgd 0
#define mng_assign_phys 0
#define mng_assign_sbit 0
#define mng_assign_splt 0
#define mng_assign_hist 0
#define mng_assign_time 0
#define mng_assign_mhdr 0
#define mng_assign_mend 0
#define mng_assign_loop 0
#define mng_assign_endl 0
#define mng_assign_defi 0
#define mng_assign_basi 0
#define mng_assign_clon 0
#define mng_assign_past 0
#define mng_assign_disc 0
#define mng_assign_back 0
#define mng_assign_fram 0
#define mng_assign_move 0
#define mng_assign_clip 0
#define mng_assign_show 0
#define mng_assign_term 0
#define mng_assign_save 0
#define mng_assign_seek 0
#define mng_assign_expi 0
#define mng_assign_fpri 0
#define mng_assign_phyg 0
#define mng_assign_jhdr 0
#define mng_assign_jdaa 0
#define mng_assign_jdat 0
#define mng_assign_jsep 0
#define mng_assign_dhdr 0
#define mng_assign_prom 0
#define mng_assign_ipng 0
#define mng_assign_pplt 0
#define mng_assign_ijng 0
#define mng_assign_drop 0
#define mng_assign_dbyk 0
#define mng_assign_ordr 0
#define mng_assign_magn 0
#define mng_assign_need 0
#define mng_assign_evnt 0
#define mng_assign_unknown 0
#endif /* MNG_INCLUDE_WRITE_PROCS */

/* ************************************************************************** */

#endif /* _libmng_chunk_prc_h_ */

/* ************************************************************************** */
/* * end of file                                                            * */
/* ************************************************************************** */
