/* ************************************************************************** */
/* *             For conditions of distribution and use,                    * */
/* *                see copyright notice in libmng.h                        * */
/* ************************************************************************** */
/* *                                                                        * */
/* * project   : libmng                                                     * */
/* * file      : libmng_chunk_io.h         copyright (c) 2000 G.Juyn        * */
/* * version   : 1.0.0                                                      * */
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
/* ************************************************************************** */

#if defined(__BORLANDC__) && defined(MNG_STRICT_ANSI)
#pragma option -A                      /* force ANSI-C */
#endif

#ifndef _libmng_chunk_io_h_
#define _libmng_chunk_io_h_

/* ************************************************************************** */

mng_uint32 crc (mng_datap  pData,
                mng_uint8p buf,
                mng_int32  len);

/* ************************************************************************** */

#ifdef MNG_INCLUDE_READ_PROCS

#define READ_CHUNK(n) mng_retcode n (mng_datap   pData,    \
                                     mng_chunkp  pHeader,  \
                                     mng_uint32  iRawlen,  \
                                     mng_uint8p  pRawdata, \
                                     mng_chunkp* ppChunk)

READ_CHUNK (read_ihdr) ;
READ_CHUNK (read_plte) ;
READ_CHUNK (read_idat) ;
READ_CHUNK (read_iend) ;
READ_CHUNK (read_trns) ;
READ_CHUNK (read_gama) ;
READ_CHUNK (read_chrm) ;
READ_CHUNK (read_srgb) ;
READ_CHUNK (read_iccp) ;
READ_CHUNK (read_text) ;
READ_CHUNK (read_ztxt) ;
READ_CHUNK (read_itxt) ;
READ_CHUNK (read_bkgd) ;
READ_CHUNK (read_phys) ;
READ_CHUNK (read_sbit) ;
READ_CHUNK (read_splt) ;
READ_CHUNK (read_hist) ;
READ_CHUNK (read_time) ;
READ_CHUNK (read_mhdr) ;
READ_CHUNK (read_mend) ;
READ_CHUNK (read_loop) ;
READ_CHUNK (read_endl) ;
READ_CHUNK (read_defi) ;
READ_CHUNK (read_basi) ;
READ_CHUNK (read_clon) ;
READ_CHUNK (read_past) ;
READ_CHUNK (read_disc) ;
READ_CHUNK (read_back) ;
READ_CHUNK (read_fram) ;
READ_CHUNK (read_move) ;
READ_CHUNK (read_clip) ;
READ_CHUNK (read_show) ;
READ_CHUNK (read_term) ;
READ_CHUNK (read_save) ;
READ_CHUNK (read_seek) ;
READ_CHUNK (read_expi) ;
READ_CHUNK (read_fpri) ;
READ_CHUNK (read_phyg) ;
READ_CHUNK (read_jhdr) ;
READ_CHUNK (read_jdaa) ;
READ_CHUNK (read_jdat) ;
READ_CHUNK (read_jsep) ;
READ_CHUNK (read_dhdr) ;
READ_CHUNK (read_prom) ;
READ_CHUNK (read_ipng) ;
READ_CHUNK (read_pplt) ;
READ_CHUNK (read_ijng) ;
READ_CHUNK (read_drop) ;
READ_CHUNK (read_dbyk) ;
READ_CHUNK (read_ordr) ;
READ_CHUNK (read_magn) ;
READ_CHUNK (read_need) ;
READ_CHUNK (read_unknown) ;

/* ************************************************************************** */

#else /* MNG_INCLUDE_READ_PROCS */
#define read_ihdr 0
#define read_plte 0
#define read_idat 0
#define read_iend 0
#define read_trns 0
#define read_gama 0
#define read_chrm 0
#define read_srgb 0
#define read_iccp 0
#define read_text 0
#define read_ztxt 0
#define read_itxt 0
#define read_bkgd 0
#define read_phys 0
#define read_sbit 0
#define read_splt 0
#define read_hist 0
#define read_time 0
#define read_mhdr 0
#define read_mend 0
#define read_loop 0
#define read_endl 0
#define read_defi 0
#define read_basi 0
#define read_clon 0
#define read_past 0
#define read_disc 0
#define read_back 0
#define read_fram 0
#define read_move 0
#define read_clip 0
#define read_show 0
#define read_term 0
#define read_save 0
#define read_seek 0
#define read_expi 0
#define read_fpri 0
#define read_phyg 0
#define read_jhdr 0
#define read_jdaa 0
#define read_jdat 0
#define read_jsep 0
#define read_dhdr 0
#define read_prom 0
#define read_ipng 0
#define read_pplt 0
#define read_ijng 0
#define read_drop 0
#define read_dbyk 0
#define read_ordr 0
#define read_magn 0
#define read_need 0
#define read_unknown 0
#endif /* MNG_INCLUDE_READ_PROCS */

/* ************************************************************************** */

#ifdef MNG_INCLUDE_WRITE_PROCS

#define WRITE_CHUNK(n) mng_retcode n (mng_datap  pData,   \
                                      mng_chunkp pChunk)

WRITE_CHUNK (write_ihdr) ;
WRITE_CHUNK (write_plte) ;
WRITE_CHUNK (write_idat) ;
WRITE_CHUNK (write_iend) ;
WRITE_CHUNK (write_trns) ;
WRITE_CHUNK (write_gama) ;
WRITE_CHUNK (write_chrm) ;
WRITE_CHUNK (write_srgb) ;
WRITE_CHUNK (write_iccp) ;
WRITE_CHUNK (write_text) ;
WRITE_CHUNK (write_ztxt) ;
WRITE_CHUNK (write_itxt) ;
WRITE_CHUNK (write_bkgd) ;
WRITE_CHUNK (write_phys) ;
WRITE_CHUNK (write_sbit) ;
WRITE_CHUNK (write_splt) ;
WRITE_CHUNK (write_hist) ;
WRITE_CHUNK (write_time) ;
WRITE_CHUNK (write_mhdr) ;
WRITE_CHUNK (write_mend) ;
WRITE_CHUNK (write_loop) ;
WRITE_CHUNK (write_endl) ;
WRITE_CHUNK (write_defi) ;
WRITE_CHUNK (write_basi) ;
WRITE_CHUNK (write_clon) ;
WRITE_CHUNK (write_past) ;
WRITE_CHUNK (write_disc) ;
WRITE_CHUNK (write_back) ;
WRITE_CHUNK (write_fram) ;
WRITE_CHUNK (write_move) ;
WRITE_CHUNK (write_clip) ;
WRITE_CHUNK (write_show) ;
WRITE_CHUNK (write_term) ;
WRITE_CHUNK (write_save) ;
WRITE_CHUNK (write_seek) ;
WRITE_CHUNK (write_expi) ;
WRITE_CHUNK (write_fpri) ;
WRITE_CHUNK (write_phyg) ;
WRITE_CHUNK (write_jhdr) ;
WRITE_CHUNK (write_jdaa) ;
WRITE_CHUNK (write_jdat) ;
WRITE_CHUNK (write_jsep) ;
WRITE_CHUNK (write_dhdr) ;
WRITE_CHUNK (write_prom) ;
WRITE_CHUNK (write_ipng) ;
WRITE_CHUNK (write_pplt) ;
WRITE_CHUNK (write_ijng) ;
WRITE_CHUNK (write_drop) ;
WRITE_CHUNK (write_dbyk) ;
WRITE_CHUNK (write_ordr) ;
WRITE_CHUNK (write_magn) ;
WRITE_CHUNK (write_need) ;
WRITE_CHUNK (write_unknown) ;

/* ************************************************************************** */

#else /* MNG_INCLUDE_WRITE_PROCS */
#define write_ihdr 0
#define write_plte 0
#define write_idat 0
#define write_iend 0
#define write_trns 0
#define write_gama 0
#define write_chrm 0
#define write_srgb 0
#define write_iccp 0
#define write_text 0
#define write_ztxt 0
#define write_itxt 0
#define write_bkgd 0
#define write_phys 0
#define write_sbit 0
#define write_splt 0
#define write_hist 0
#define write_time 0
#define write_mhdr 0
#define write_mend 0
#define write_loop 0
#define write_endl 0
#define write_defi 0
#define write_basi 0
#define write_clon 0
#define write_past 0
#define write_disc 0
#define write_back 0
#define write_fram 0
#define write_move 0
#define write_clip 0
#define write_show 0
#define write_term 0
#define write_save 0
#define write_seek 0
#define write_expi 0
#define write_fpri 0
#define write_phyg 0
#define write_jhdr 0
#define write_jdaa 0
#define write_jdat 0
#define write_jsep 0
#define write_dhdr 0
#define write_prom 0
#define write_ipng 0
#define write_pplt 0
#define write_ijng 0
#define write_drop 0
#define write_dbyk 0
#define write_ordr 0
#define write_magn 0
#define write_need 0
#define write_unknown 0
#endif /* MNG_INCLUDE_WRITE_PROCS */

/* ************************************************************************** */

#endif /* _libmng_chunk_io_h_ */

/* ************************************************************************** */
/* * end of file                                                            * */
/* ************************************************************************** */
