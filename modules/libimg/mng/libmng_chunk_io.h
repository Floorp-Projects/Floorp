/* ************************************************************************** */
/* *             For conditions of distribution and use,                    * */
/* *                see copyright notice in libmng.h                        * */
/* ************************************************************************** */
/* *                                                                        * */
/* * project   : libmng                                                     * */
/* * file      : libmng_chunk_io.h         copyright (c) 2000-2002 G.Juyn   * */
/* * version   : 1.0.5                                                      * */
/* *                                                                        * */
/* * purpose   : Chunk I/O routines (definition)                            * */
/* *                                                                        * */
/* * author    : G.Juyn                                                     * */
/* * web       : http://www.3-t.com                                         * */
/* * email     : mailto:info@3-t.com                                        * */
/* *                                                                        * */
/* * comment   : Definition of the chunk input/output routines              * */
/* *                                                                        * */
/* * changes   : 0.5.1 - 05/04/2000 - G.Juyn                                * */
/* *             - changed CRC initializtion to use dynamic structure       * */
/* *               (wasn't thread-safe the old way !)                       * */
/* *             0.5.1 - 05/08/2000 - G.Juyn                                * */
/* *             - changed write routines definition                        * */
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

#ifndef _libmng_chunk_io_h_
#define _libmng_chunk_io_h_

/* ************************************************************************** */

mng_uint32 mng_crc (mng_datap  pData,
                    mng_uint8p buf,
                    mng_int32  len);

/* ************************************************************************** */

#ifdef MNG_INCLUDE_READ_PROCS

#define READ_CHUNK(n) mng_retcode n (mng_datap   pData,    \
                                     mng_chunkp  pHeader,  \
                                     mng_uint32  iRawlen,  \
                                     mng_uint8p  pRawdata, \
                                     mng_chunkp* ppChunk)

READ_CHUNK (mng_read_ihdr) ;
READ_CHUNK (mng_read_plte) ;
READ_CHUNK (mng_read_idat) ;
READ_CHUNK (mng_read_iend) ;
READ_CHUNK (mng_read_trns) ;
READ_CHUNK (mng_read_gama) ;
READ_CHUNK (mng_read_chrm) ;
READ_CHUNK (mng_read_srgb) ;
READ_CHUNK (mng_read_iccp) ;
READ_CHUNK (mng_read_text) ;
READ_CHUNK (mng_read_ztxt) ;
READ_CHUNK (mng_read_itxt) ;
READ_CHUNK (mng_read_bkgd) ;
READ_CHUNK (mng_read_phys) ;
READ_CHUNK (mng_read_sbit) ;
READ_CHUNK (mng_read_splt) ;
READ_CHUNK (mng_read_hist) ;
READ_CHUNK (mng_read_time) ;
READ_CHUNK (mng_read_mhdr) ;
READ_CHUNK (mng_read_mend) ;
READ_CHUNK (mng_read_loop) ;
READ_CHUNK (mng_read_endl) ;
READ_CHUNK (mng_read_defi) ;
READ_CHUNK (mng_read_basi) ;
READ_CHUNK (mng_read_clon) ;
READ_CHUNK (mng_read_past) ;
READ_CHUNK (mng_read_disc) ;
READ_CHUNK (mng_read_back) ;
READ_CHUNK (mng_read_fram) ;
READ_CHUNK (mng_read_move) ;
READ_CHUNK (mng_read_clip) ;
READ_CHUNK (mng_read_show) ;
READ_CHUNK (mng_read_term) ;
READ_CHUNK (mng_read_save) ;
READ_CHUNK (mng_read_seek) ;
READ_CHUNK (mng_read_expi) ;
READ_CHUNK (mng_read_fpri) ;
READ_CHUNK (mng_read_phyg) ;
READ_CHUNK (mng_read_jhdr) ;
READ_CHUNK (mng_read_jdaa) ;
READ_CHUNK (mng_read_jdat) ;
READ_CHUNK (mng_read_jsep) ;
READ_CHUNK (mng_read_dhdr) ;
READ_CHUNK (mng_read_prom) ;
READ_CHUNK (mng_read_ipng) ;
READ_CHUNK (mng_read_pplt) ;
READ_CHUNK (mng_read_ijng) ;
READ_CHUNK (mng_read_drop) ;
READ_CHUNK (mng_read_dbyk) ;
READ_CHUNK (mng_read_ordr) ;
READ_CHUNK (mng_read_magn) ;
READ_CHUNK (mng_read_need) ;
READ_CHUNK (mng_read_evnt) ;
READ_CHUNK (mng_read_unknown) ;

/* ************************************************************************** */

#else /* MNG_INCLUDE_READ_PROCS */
#define mng_read_ihdr 0
#define mng_read_plte 0
#define mng_read_idat 0
#define mng_read_iend 0
#define mng_read_trns 0
#define mng_read_gama 0
#define mng_read_chrm 0
#define mng_read_srgb 0
#define mng_read_iccp 0
#define mng_read_text 0
#define mng_read_ztxt 0
#define mng_read_itxt 0
#define mng_read_bkgd 0
#define mng_read_phys 0
#define mng_read_sbit 0
#define mng_read_splt 0
#define mng_read_hist 0
#define mng_read_time 0
#define mng_read_mhdr 0
#define mng_read_mend 0
#define mng_read_loop 0
#define mng_read_endl 0
#define mng_read_defi 0
#define mng_read_basi 0
#define mng_read_clon 0
#define mng_read_past 0
#define mng_read_disc 0
#define mng_read_back 0
#define mng_read_fram 0
#define mng_read_move 0
#define mng_read_clip 0
#define mng_read_show 0
#define mng_read_term 0
#define mng_read_save 0
#define mng_read_seek 0
#define mng_read_expi 0
#define mng_read_fpri 0
#define mng_read_phyg 0
#define mng_read_jhdr 0
#define mng_read_jdaa 0
#define mng_read_jdat 0
#define mng_read_jsep 0
#define mng_read_dhdr 0
#define mng_read_prom 0
#define mng_read_ipng 0
#define mng_read_pplt 0
#define mng_read_ijng 0
#define mng_read_drop 0
#define mng_read_dbyk 0
#define mng_read_ordr 0
#define mng_read_magn 0
#define mng_read_need 0
#define mng_read_evnt 0
#define mng_read_unknown 0
#endif /* MNG_INCLUDE_READ_PROCS */

/* ************************************************************************** */

#ifdef MNG_INCLUDE_WRITE_PROCS

#define WRITE_CHUNK(n) mng_retcode n (mng_datap  pData,   \
                                      mng_chunkp pChunk)

WRITE_CHUNK (mng_write_ihdr) ;
WRITE_CHUNK (mng_write_plte) ;
WRITE_CHUNK (mng_write_idat) ;
WRITE_CHUNK (mng_write_iend) ;
WRITE_CHUNK (mng_write_trns) ;
WRITE_CHUNK (mng_write_gama) ;
WRITE_CHUNK (mng_write_chrm) ;
WRITE_CHUNK (mng_write_srgb) ;
WRITE_CHUNK (mng_write_iccp) ;
WRITE_CHUNK (mng_write_text) ;
WRITE_CHUNK (mng_write_ztxt) ;
WRITE_CHUNK (mng_write_itxt) ;
WRITE_CHUNK (mng_write_bkgd) ;
WRITE_CHUNK (mng_write_phys) ;
WRITE_CHUNK (mng_write_sbit) ;
WRITE_CHUNK (mng_write_splt) ;
WRITE_CHUNK (mng_write_hist) ;
WRITE_CHUNK (mng_write_time) ;
WRITE_CHUNK (mng_write_mhdr) ;
WRITE_CHUNK (mng_write_mend) ;
WRITE_CHUNK (mng_write_loop) ;
WRITE_CHUNK (mng_write_endl) ;
WRITE_CHUNK (mng_write_defi) ;
WRITE_CHUNK (mng_write_basi) ;
WRITE_CHUNK (mng_write_clon) ;
WRITE_CHUNK (mng_write_past) ;
WRITE_CHUNK (mng_write_disc) ;
WRITE_CHUNK (mng_write_back) ;
WRITE_CHUNK (mng_write_fram) ;
WRITE_CHUNK (mng_write_move) ;
WRITE_CHUNK (mng_write_clip) ;
WRITE_CHUNK (mng_write_show) ;
WRITE_CHUNK (mng_write_term) ;
WRITE_CHUNK (mng_write_save) ;
WRITE_CHUNK (mng_write_seek) ;
WRITE_CHUNK (mng_write_expi) ;
WRITE_CHUNK (mng_write_fpri) ;
WRITE_CHUNK (mng_write_phyg) ;
WRITE_CHUNK (mng_write_jhdr) ;
WRITE_CHUNK (mng_write_jdaa) ;
WRITE_CHUNK (mng_write_jdat) ;
WRITE_CHUNK (mng_write_jsep) ;
WRITE_CHUNK (mng_write_dhdr) ;
WRITE_CHUNK (mng_write_prom) ;
WRITE_CHUNK (mng_write_ipng) ;
WRITE_CHUNK (mng_write_pplt) ;
WRITE_CHUNK (mng_write_ijng) ;
WRITE_CHUNK (mng_write_drop) ;
WRITE_CHUNK (mng_write_dbyk) ;
WRITE_CHUNK (mng_write_ordr) ;
WRITE_CHUNK (mng_write_magn) ;
WRITE_CHUNK (mng_write_need) ;
WRITE_CHUNK (mng_write_evnt) ;
WRITE_CHUNK (mng_write_unknown) ;

/* ************************************************************************** */

#else /* MNG_INCLUDE_WRITE_PROCS */
#define mng_write_ihdr 0
#define mng_write_plte 0
#define mng_write_idat 0
#define mng_write_iend 0
#define mng_write_trns 0
#define mng_write_gama 0
#define mng_write_chrm 0
#define mng_write_srgb 0
#define mng_write_iccp 0
#define mng_write_text 0
#define mng_write_ztxt 0
#define mng_write_itxt 0
#define mng_write_bkgd 0
#define mng_write_phys 0
#define mng_write_sbit 0
#define mng_write_splt 0
#define mng_write_hist 0
#define mng_write_time 0
#define mng_write_mhdr 0
#define mng_write_mend 0
#define mng_write_loop 0
#define mng_write_endl 0
#define mng_write_defi 0
#define mng_write_basi 0
#define mng_write_clon 0
#define mng_write_past 0
#define mng_write_disc 0
#define mng_write_back 0
#define mng_write_fram 0
#define mng_write_move 0
#define mng_write_clip 0
#define mng_write_show 0
#define mng_write_term 0
#define mng_write_save 0
#define mng_write_seek 0
#define mng_write_expi 0
#define mng_write_fpri 0
#define mng_write_phyg 0
#define mng_write_jhdr 0
#define mng_write_jdaa 0
#define mng_write_jdat 0
#define mng_write_jsep 0
#define mng_write_dhdr 0
#define mng_write_prom 0
#define mng_write_ipng 0
#define mng_write_pplt 0
#define mng_write_ijng 0
#define mng_write_drop 0
#define mng_write_dbyk 0
#define mng_write_ordr 0
#define mng_write_magn 0
#define mng_write_need 0
#define mng_write_evnt 0
#define mng_write_unknown 0
#endif /* MNG_INCLUDE_WRITE_PROCS */

/* ************************************************************************** */

#endif /* _libmng_chunk_io_h_ */

/* ************************************************************************** */
/* * end of file                                                            * */
/* ************************************************************************** */
