/* ************************************************************************** */
/* *             For conditions of distribution and use,                    * */
/* *                see copyright notice in libmng.h                        * */
/* ************************************************************************** */
/* *                                                                        * */
/* * project   : libmng                                                     * */
/* * file      : libmng_chunk_prc.c        copyright (c) 2000 G.Juyn        * */
/* * version   : 1.0.0                                                      * */
/* *                                                                        * */
/* * purpose   : Chunk initialization & cleanup (implementation)            * */
/* *                                                                        * */
/* * author    : G.Juyn                                                     * */
/* * web       : http://www.3-t.com                                         * */
/* * email     : mailto:info@3-t.com                                        * */
/* *                                                                        * */
/* * comment   : implementation of the chunk initialization & cleanup       * */
/* *             routines                                                   * */
/* *                                                                        * */
/* * changes   : 0.5.1 - 05/08/2000 - G.Juyn                                * */
/* *             - changed strict-ANSI stuff                                * */
/* *             0.5.1 - 05/12/2000 - G.Juyn                                * */
/* *             - changed trace to macro for callback error-reporting      * */
/* *                                                                        * */
/* *             0.9.1 - 07/19/2000 - G.Juyn                                * */
/* *             - fixed creation-code                                      * */
/* *                                                                        * */
/* *             0.9.2 - 07/31/2000 - G.Juyn                                * */
/* *             - put add_chunk() inside MNG_INCLUDE_WRITE_PROCS wrapper   * */
/* *             0.9.2 - 08/01/2000 - G.Juyn                                * */
/* *             - wrapper for add_chunk() changed                          * */
/* *             0.9.2 - 08/05/2000 - G.Juyn                                * */
/* *             - changed file-prefixes                                    * */
/* *                                                                        * */
/* *             0.9.3 - 08/26/2000 - G.Juyn                                * */
/* *             - added MAGN chunk                                         * */
/* *             0.9.3 - 10/16/2000 - G.Juyn                                * */
/* *             - added support for JDAA                                   * */
/* *                                                                        * */
/* ************************************************************************** */

#include "libmng.h"
#include "libmng_data.h"
#include "libmng_error.h"
#include "libmng_trace.h"
#ifdef __BORLANDC__
#pragma hdrstop
#endif
#include "libmng_memory.h"
#include "libmng_chunks.h"
#include "libmng_chunk_prc.h"

#if defined(__BORLANDC__) && defined(MNG_STRICT_ANSI)
#pragma option -A                      /* force ANSI-C */
#endif

/* ************************************************************************** */
/* *                                                                        * */
/* * General chunk routines                                                 * */
/* *                                                                        * */
/* ************************************************************************** */

void add_chunk (mng_datap  pData,
                mng_chunkp pChunk)
{
  if (!pData->pFirstchunk)             /* list is still empty ? */
  {
    pData->pFirstchunk      = pChunk;  /* then this becomes the first */
    
#ifdef MNG_SUPPORT_WRITE
    pData->iFirstchunkadded = ((mng_chunk_headerp)pChunk)->iChunkname;
#endif

    if (((mng_chunk_headerp)pChunk)->iChunkname == MNG_UINT_IHDR)
      pData->eImagetype     = mng_it_png;
    else
#ifdef MNG_INCLUDE_JNG
    if (((mng_chunk_headerp)pChunk)->iChunkname == MNG_UINT_JHDR)
      pData->eImagetype     = mng_it_jng;
    else
#endif
      pData->eImagetype     = mng_it_mng;

    pData->eSigtype         = pData->eImagetype;
  }
  else
  {                                    /* else we make appropriate links */
    ((mng_chunk_headerp)pChunk)->pPrev = pData->pLastchunk;
    ((mng_chunk_headerp)pData->pLastchunk)->pNext = pChunk;
  }

  pData->pLastchunk = pChunk;          /* and it's always the last */

  return;
}

/* ************************************************************************** */
/* *                                                                        * */
/* * Chunk specific initialization routines                                 * */
/* *                                                                        * */
/* ************************************************************************** */

INIT_CHUNK_HDR (init_ihdr)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_IHDR, MNG_LC_START)
#endif

  MNG_ALLOC (pData, *ppChunk, sizeof (mng_ihdr))
  ((mng_ihdrp)*ppChunk)->sHeader = *((mng_chunk_headerp)pHeader);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_IHDR, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

INIT_CHUNK_HDR (init_plte)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_PLTE, MNG_LC_START)
#endif

  MNG_ALLOC (pData, *ppChunk, sizeof (mng_plte))
  ((mng_pltep)*ppChunk)->sHeader = *((mng_chunk_headerp)pHeader);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_PLTE, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

INIT_CHUNK_HDR (init_idat)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_IDAT, MNG_LC_START)
#endif

  MNG_ALLOC (pData, *ppChunk, sizeof (mng_idat))
  ((mng_idatp)*ppChunk)->sHeader = *((mng_chunk_headerp)pHeader);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_IDAT, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

INIT_CHUNK_HDR (init_iend)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_IEND, MNG_LC_START)
#endif

  MNG_ALLOC (pData, *ppChunk, sizeof (mng_iend))
  ((mng_iendp)*ppChunk)->sHeader = *((mng_chunk_headerp)pHeader);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_IEND, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

INIT_CHUNK_HDR (init_trns)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_TRNS, MNG_LC_START)
#endif

  MNG_ALLOC (pData, *ppChunk, sizeof (mng_trns))
  ((mng_trnsp)*ppChunk)->sHeader = *((mng_chunk_headerp)pHeader);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_TRNS, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

INIT_CHUNK_HDR (init_gama)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_GAMA, MNG_LC_START)
#endif

  MNG_ALLOC (pData, *ppChunk, sizeof (mng_gama))
  ((mng_gamap)*ppChunk)->sHeader = *((mng_chunk_headerp)pHeader);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_GAMA, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

INIT_CHUNK_HDR (init_chrm)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_CHRM, MNG_LC_START)
#endif

  MNG_ALLOC (pData, *ppChunk, sizeof (mng_chrm))
  ((mng_chrmp)*ppChunk)->sHeader = *((mng_chunk_headerp)pHeader);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_CHRM, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

INIT_CHUNK_HDR (init_srgb)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_SRGB, MNG_LC_START)
#endif

  MNG_ALLOC (pData, *ppChunk, sizeof (mng_srgb))
  ((mng_srgbp)*ppChunk)->sHeader = *((mng_chunk_headerp)pHeader);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_SRGB, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

INIT_CHUNK_HDR (init_iccp)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_ICCP, MNG_LC_START)
#endif

  MNG_ALLOC (pData, *ppChunk, sizeof (mng_iccp))
  ((mng_iccpp)*ppChunk)->sHeader = *((mng_chunk_headerp)pHeader);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_ICCP, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

INIT_CHUNK_HDR (init_text)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_TEXT, MNG_LC_START)
#endif

  MNG_ALLOC (pData, *ppChunk, sizeof (mng_text))
  ((mng_textp)*ppChunk)->sHeader = *((mng_chunk_headerp)pHeader);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_TEXT, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

INIT_CHUNK_HDR (init_ztxt)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_ZTXT, MNG_LC_START)
#endif

  MNG_ALLOC (pData, *ppChunk, sizeof (mng_ztxt))
  ((mng_ztxtp)*ppChunk)->sHeader = *((mng_chunk_headerp)pHeader);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_ZTXT, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

INIT_CHUNK_HDR (init_itxt)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_ITXT, MNG_LC_START)
#endif

  MNG_ALLOC (pData, *ppChunk, sizeof (mng_itxt))
  ((mng_itxtp)*ppChunk)->sHeader = *((mng_chunk_headerp)pHeader);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_ITXT, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

INIT_CHUNK_HDR (init_bkgd)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_BKGD, MNG_LC_START)
#endif

  MNG_ALLOC (pData, *ppChunk, sizeof (mng_bkgd))
  ((mng_bkgdp)*ppChunk)->sHeader = *((mng_chunk_headerp)pHeader);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_BKGD, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

INIT_CHUNK_HDR (init_phys)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_PHYS, MNG_LC_START)
#endif

  MNG_ALLOC (pData, *ppChunk, sizeof (mng_phys))
  ((mng_physp)*ppChunk)->sHeader = *((mng_chunk_headerp)pHeader);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_PHYS, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

INIT_CHUNK_HDR (init_sbit)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_SBIT, MNG_LC_START)
#endif

  MNG_ALLOC (pData, *ppChunk, sizeof (mng_sbit))
  ((mng_sbitp)*ppChunk)->sHeader = *((mng_chunk_headerp)pHeader);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_SBIT, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

INIT_CHUNK_HDR (init_splt)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_SPLT, MNG_LC_START)
#endif

  MNG_ALLOC (pData, *ppChunk, sizeof (mng_splt))
  ((mng_spltp)*ppChunk)->sHeader = *((mng_chunk_headerp)pHeader);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_SPLT, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

INIT_CHUNK_HDR (init_hist)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_HIST, MNG_LC_START)
#endif

  MNG_ALLOC (pData, *ppChunk, sizeof (mng_hist))
  ((mng_histp)*ppChunk)->sHeader = *((mng_chunk_headerp)pHeader);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_HIST, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

INIT_CHUNK_HDR (init_time)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_TIME, MNG_LC_START)
#endif

  MNG_ALLOC (pData, *ppChunk, sizeof (mng_time))
  ((mng_timep)*ppChunk)->sHeader = *((mng_chunk_headerp)pHeader);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_TIME, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

INIT_CHUNK_HDR (init_mhdr)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_MHDR, MNG_LC_START)
#endif

  MNG_ALLOC (pData, *ppChunk, sizeof (mng_mhdr))
  ((mng_mhdrp)*ppChunk)->sHeader = *((mng_chunk_headerp)pHeader);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_MHDR, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

INIT_CHUNK_HDR (init_mend)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_MEND, MNG_LC_START)
#endif

  MNG_ALLOC (pData, *ppChunk, sizeof (mng_mend))
  ((mng_mendp)*ppChunk)->sHeader = *((mng_chunk_headerp)pHeader);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_MEND, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

INIT_CHUNK_HDR (init_loop)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_LOOP, MNG_LC_START)
#endif

  MNG_ALLOC (pData, *ppChunk, sizeof (mng_loop))
  ((mng_loopp)*ppChunk)->sHeader = *((mng_chunk_headerp)pHeader);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_LOOP, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

INIT_CHUNK_HDR (init_endl)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_ENDL, MNG_LC_START)
#endif

  MNG_ALLOC (pData, *ppChunk, sizeof (mng_endl))
  ((mng_endlp)*ppChunk)->sHeader = *((mng_chunk_headerp)pHeader);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_ENDL, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

INIT_CHUNK_HDR (init_defi)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_DEFI, MNG_LC_START)
#endif

  MNG_ALLOC (pData, *ppChunk, sizeof (mng_defi))
  ((mng_defip)*ppChunk)->sHeader = *((mng_chunk_headerp)pHeader);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_DEFI, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

INIT_CHUNK_HDR (init_basi)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_BASI, MNG_LC_START)
#endif

  MNG_ALLOC (pData, *ppChunk, sizeof (mng_basi))
  ((mng_basip)*ppChunk)->sHeader = *((mng_chunk_headerp)pHeader);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_BASI, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

INIT_CHUNK_HDR (init_clon)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_CLON, MNG_LC_START)
#endif

  MNG_ALLOC (pData, *ppChunk, sizeof (mng_clon))
  ((mng_clonp)*ppChunk)->sHeader = *((mng_chunk_headerp)pHeader);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_CLON, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

INIT_CHUNK_HDR (init_past)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_PAST, MNG_LC_START)
#endif

  MNG_ALLOC (pData, *ppChunk, sizeof (mng_past))
  ((mng_pastp)*ppChunk)->sHeader = *((mng_chunk_headerp)pHeader);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_PAST, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

INIT_CHUNK_HDR (init_disc)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_DISC, MNG_LC_START)
#endif

  MNG_ALLOC (pData, *ppChunk, sizeof (mng_disc))
  ((mng_discp)*ppChunk)->sHeader = *((mng_chunk_headerp)pHeader);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_DISC, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

INIT_CHUNK_HDR (init_back)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_BACK, MNG_LC_START)
#endif

  MNG_ALLOC (pData, *ppChunk, sizeof (mng_back))
  ((mng_backp)*ppChunk)->sHeader = *((mng_chunk_headerp)pHeader);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_BACK, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

INIT_CHUNK_HDR (init_fram)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_FRAM, MNG_LC_START)
#endif

  MNG_ALLOC (pData, *ppChunk, sizeof (mng_fram))
  ((mng_framp)*ppChunk)->sHeader = *((mng_chunk_headerp)pHeader);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_FRAM, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

INIT_CHUNK_HDR (init_move)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_MOVE, MNG_LC_START)
#endif

  MNG_ALLOC (pData, *ppChunk, sizeof (mng_move))
  ((mng_movep)*ppChunk)->sHeader = *((mng_chunk_headerp)pHeader);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_MOVE, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

INIT_CHUNK_HDR (init_clip)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_CLIP, MNG_LC_START)
#endif

  MNG_ALLOC (pData, *ppChunk, sizeof (mng_clip))
  ((mng_clipp)*ppChunk)->sHeader = *((mng_chunk_headerp)pHeader);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_CLIP, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

INIT_CHUNK_HDR (init_show)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_SHOW, MNG_LC_START)
#endif

  MNG_ALLOC (pData, *ppChunk, sizeof (mng_show))
  ((mng_showp)*ppChunk)->sHeader = *((mng_chunk_headerp)pHeader);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_SHOW, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

INIT_CHUNK_HDR (init_term)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_TERM, MNG_LC_START)
#endif

  MNG_ALLOC (pData, *ppChunk, sizeof (mng_term))
  ((mng_termp)*ppChunk)->sHeader = *((mng_chunk_headerp)pHeader);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_TERM, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

INIT_CHUNK_HDR (init_save)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_SAVE, MNG_LC_START)
#endif

  MNG_ALLOC (pData, *ppChunk, sizeof (mng_save))
  ((mng_savep)*ppChunk)->sHeader = *((mng_chunk_headerp)pHeader);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_SAVE, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

INIT_CHUNK_HDR (init_seek)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_SEEK, MNG_LC_START)
#endif

  MNG_ALLOC (pData, *ppChunk, sizeof (mng_seek))
  ((mng_seekp)*ppChunk)->sHeader = *((mng_chunk_headerp)pHeader);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_SEEK, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

INIT_CHUNK_HDR (init_expi)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_EXPI, MNG_LC_START)
#endif

  MNG_ALLOC (pData, *ppChunk, sizeof (mng_expi))
  ((mng_expip)*ppChunk)->sHeader = *((mng_chunk_headerp)pHeader);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_EXPI, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

INIT_CHUNK_HDR (init_fpri)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_FPRI, MNG_LC_START)
#endif

  MNG_ALLOC (pData, *ppChunk, sizeof (mng_fpri))
  ((mng_fprip)*ppChunk)->sHeader = *((mng_chunk_headerp)pHeader);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_FPRI, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

INIT_CHUNK_HDR (init_need)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_NEED, MNG_LC_START)
#endif

  MNG_ALLOC (pData, *ppChunk, sizeof (mng_need))
  ((mng_needp)*ppChunk)->sHeader = *((mng_chunk_headerp)pHeader);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_NEED, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

INIT_CHUNK_HDR (init_phyg)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_PHYG, MNG_LC_START)
#endif

  MNG_ALLOC (pData, *ppChunk, sizeof (mng_phyg))
  ((mng_phygp)*ppChunk)->sHeader = *((mng_chunk_headerp)pHeader);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_PHYG, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

#ifdef MNG_INCLUDE_JNG
INIT_CHUNK_HDR (init_jhdr)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_JHDR, MNG_LC_START)
#endif

  MNG_ALLOC (pData, *ppChunk, sizeof (mng_jhdr))
  ((mng_jhdrp)*ppChunk)->sHeader = *((mng_chunk_headerp)pHeader);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_JHDR, MNG_LC_END)
#endif

  return MNG_NOERROR;
}
#endif /* MNG_INCLUDE_JNG */

/* ************************************************************************** */

#ifdef MNG_INCLUDE_JNG
INIT_CHUNK_HDR (init_jdaa)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_JDAA, MNG_LC_START)
#endif

  MNG_ALLOC (pData, *ppChunk, sizeof (mng_jdaa))
  ((mng_jdaap)*ppChunk)->sHeader = *((mng_chunk_headerp)pHeader);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_JDAA, MNG_LC_END)
#endif

  return MNG_NOERROR;
}
#endif /* MNG_INCLUDE_JNG */

/* ************************************************************************** */

#ifdef MNG_INCLUDE_JNG
INIT_CHUNK_HDR (init_jdat)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_JDAT, MNG_LC_START)
#endif

  MNG_ALLOC (pData, *ppChunk, sizeof (mng_jdat))
  ((mng_jdatp)*ppChunk)->sHeader = *((mng_chunk_headerp)pHeader);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_JDAT, MNG_LC_END)
#endif

  return MNG_NOERROR;
}
#endif /* MNG_INCLUDE_JNG */

/* ************************************************************************** */

#ifdef MNG_INCLUDE_JNG
INIT_CHUNK_HDR (init_jsep)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_JSEP, MNG_LC_START)
#endif

  MNG_ALLOC (pData, *ppChunk, sizeof (mng_jsep))
  ((mng_jsepp)*ppChunk)->sHeader = *((mng_chunk_headerp)pHeader);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_JSEP, MNG_LC_END)
#endif

  return MNG_NOERROR;
}
#endif /* MNG_INCLUDE_JNG */

/* ************************************************************************** */

INIT_CHUNK_HDR (init_dhdr)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_DHDR, MNG_LC_START)
#endif

  MNG_ALLOC (pData, *ppChunk, sizeof (mng_dhdr))
  ((mng_dhdrp)*ppChunk)->sHeader = *((mng_chunk_headerp)pHeader);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_DHDR, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

INIT_CHUNK_HDR (init_prom)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_PROM, MNG_LC_START)
#endif

  MNG_ALLOC (pData, *ppChunk, sizeof (mng_prom))
  ((mng_promp)*ppChunk)->sHeader = *((mng_chunk_headerp)pHeader);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_PROM, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

INIT_CHUNK_HDR (init_ipng)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_IPNG, MNG_LC_START)
#endif

  MNG_ALLOC (pData, *ppChunk, sizeof (mng_ipng))
  ((mng_ipngp)*ppChunk)->sHeader = *((mng_chunk_headerp)pHeader);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_IPNG, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

INIT_CHUNK_HDR (init_pplt)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_PPLT, MNG_LC_START)
#endif

  MNG_ALLOC (pData, *ppChunk, sizeof (mng_pplt))
  ((mng_ppltp)*ppChunk)->sHeader = *((mng_chunk_headerp)pHeader);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_PPLT, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

INIT_CHUNK_HDR (init_ijng)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_IJNG, MNG_LC_START)
#endif

  MNG_ALLOC (pData, *ppChunk, sizeof (mng_ijng))
  ((mng_ijngp)*ppChunk)->sHeader = *((mng_chunk_headerp)pHeader);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_IJNG, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

INIT_CHUNK_HDR (init_drop)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_DROP, MNG_LC_START)
#endif

  MNG_ALLOC (pData, *ppChunk, sizeof (mng_drop))
  ((mng_dropp)*ppChunk)->sHeader = *((mng_chunk_headerp)pHeader);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_DROP, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

INIT_CHUNK_HDR (init_dbyk)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_DBYK, MNG_LC_START)
#endif

  MNG_ALLOC (pData, *ppChunk, sizeof (mng_dbyk))
  ((mng_dbykp)*ppChunk)->sHeader = *((mng_chunk_headerp)pHeader);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_DBYK, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

INIT_CHUNK_HDR (init_ordr)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_ORDR, MNG_LC_START)
#endif

  MNG_ALLOC (pData, *ppChunk, sizeof (mng_ordr))
  ((mng_ordrp)*ppChunk)->sHeader = *((mng_chunk_headerp)pHeader);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_ORDR, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

INIT_CHUNK_HDR (init_magn)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_MAGN, MNG_LC_START)
#endif

  MNG_ALLOC (pData, *ppChunk, sizeof (mng_magn))
  ((mng_magnp)*ppChunk)->sHeader = *((mng_chunk_headerp)pHeader);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_MAGN, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

INIT_CHUNK_HDR (init_unknown)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_UNKNOWN, MNG_LC_START)
#endif

  MNG_ALLOC (pData, *ppChunk, sizeof (mng_unknown_chunk))
  ((mng_unknown_chunkp)*ppChunk)->sHeader = *((mng_chunk_headerp)pHeader);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_UNKNOWN, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */
/* *                                                                        * */
/* * Chunk specific cleanup routines                                        * */
/* *                                                                        * */
/* ************************************************************************** */

FREE_CHUNK_HDR (free_ihdr)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_IHDR, MNG_LC_START)
#endif

  MNG_FREEX (pData, pHeader, sizeof (mng_ihdr))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_IHDR, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

FREE_CHUNK_HDR (free_plte)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_PLTE, MNG_LC_START)
#endif

  MNG_FREEX (pData, pHeader, sizeof (mng_plte))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_PLTE, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

FREE_CHUNK_HDR (free_idat)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_IDAT, MNG_LC_START)
#endif

  if (((mng_idatp)pHeader)->iDatasize)
    MNG_FREEX (pData, ((mng_idatp)pHeader)->pData,
                      ((mng_idatp)pHeader)->iDatasize)

  MNG_FREEX (pData, pHeader, sizeof (mng_idat))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_IDAT, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

FREE_CHUNK_HDR (free_iend)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_IEND, MNG_LC_START)
#endif

  MNG_FREEX (pData, pHeader, sizeof (mng_iend))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_IEND, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

FREE_CHUNK_HDR (free_trns)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_TRNS, MNG_LC_START)
#endif

  MNG_FREEX (pData, pHeader, sizeof (mng_trns))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_TRNS, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

FREE_CHUNK_HDR (free_gama)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_GAMA, MNG_LC_START)
#endif

  MNG_FREEX (pData, pHeader, sizeof (mng_gama))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_GAMA, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

FREE_CHUNK_HDR (free_chrm)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_CHRM, MNG_LC_START)
#endif

  MNG_FREEX (pData, pHeader, sizeof (mng_chrm))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_CHRM, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

FREE_CHUNK_HDR (free_srgb)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_SRGB, MNG_LC_START)
#endif

  MNG_FREEX (pData, pHeader, sizeof (mng_srgb))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_SRGB, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

FREE_CHUNK_HDR (free_iccp)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ICCP, MNG_LC_START)
#endif

  if (((mng_iccpp)pHeader)->iNamesize)
    MNG_FREEX (pData, ((mng_iccpp)pHeader)->zName,
                      ((mng_iccpp)pHeader)->iNamesize + 1)

  if (((mng_iccpp)pHeader)->iProfilesize)
    MNG_FREEX (pData, ((mng_iccpp)pHeader)->pProfile,
                      ((mng_iccpp)pHeader)->iProfilesize)

  MNG_FREEX (pData, pHeader, sizeof (mng_iccp))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ICCP, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

FREE_CHUNK_HDR (free_text)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_TEXT, MNG_LC_START)
#endif

  if (((mng_textp)pHeader)->iKeywordsize)
    MNG_FREEX (pData, ((mng_textp)pHeader)->zKeyword,
                      ((mng_textp)pHeader)->iKeywordsize + 1)

  if (((mng_textp)pHeader)->iTextsize)
    MNG_FREEX (pData, ((mng_textp)pHeader)->zText,
                      ((mng_textp)pHeader)->iTextsize + 1)

  MNG_FREEX (pData, pHeader, sizeof (mng_text))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_TEXT, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

FREE_CHUNK_HDR (free_ztxt)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ZTXT, MNG_LC_START)
#endif

  if (((mng_ztxtp)pHeader)->iKeywordsize)
    MNG_FREEX (pData, ((mng_ztxtp)pHeader)->zKeyword,
                      ((mng_ztxtp)pHeader)->iKeywordsize + 1)

  if (((mng_ztxtp)pHeader)->iTextsize)
    MNG_FREEX (pData, ((mng_ztxtp)pHeader)->zText,
                      ((mng_ztxtp)pHeader)->iTextsize)

  MNG_FREEX (pData, pHeader, sizeof (mng_ztxt))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ZTXT, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

FREE_CHUNK_HDR (free_itxt)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ITXT, MNG_LC_START)
#endif

  if (((mng_itxtp)pHeader)->iKeywordsize)
    MNG_FREEX (pData, ((mng_itxtp)pHeader)->zKeyword,
                      ((mng_itxtp)pHeader)->iKeywordsize + 1)

  if (((mng_itxtp)pHeader)->iLanguagesize)
    MNG_FREEX (pData, ((mng_itxtp)pHeader)->zLanguage,
                      ((mng_itxtp)pHeader)->iLanguagesize + 1)

  if (((mng_itxtp)pHeader)->iTranslationsize)
    MNG_FREEX (pData, ((mng_itxtp)pHeader)->zTranslation,
                      ((mng_itxtp)pHeader)->iTranslationsize + 1)

  if (((mng_itxtp)pHeader)->iTextsize)
    MNG_FREEX (pData, ((mng_itxtp)pHeader)->zText,
                      ((mng_itxtp)pHeader)->iTextsize)

  MNG_FREEX (pData, pHeader, sizeof (mng_itxt))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ITXT, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

FREE_CHUNK_HDR (free_bkgd)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_BKGD, MNG_LC_START)
#endif

  MNG_FREEX (pData, pHeader, sizeof (mng_bkgd))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_BKGD, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

FREE_CHUNK_HDR (free_phys)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_PHYS, MNG_LC_START)
#endif

  MNG_FREEX (pData, pHeader, sizeof (mng_phys))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_PHYS, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

FREE_CHUNK_HDR (free_sbit)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_SBIT, MNG_LC_START)
#endif

  MNG_FREEX (pData, pHeader, sizeof (mng_sbit))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_SBIT, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

FREE_CHUNK_HDR (free_splt)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_SPLT, MNG_LC_START)
#endif

  if (((mng_spltp)pHeader)->iNamesize)
    MNG_FREEX (pData, ((mng_spltp)pHeader)->zName,
                      ((mng_spltp)pHeader)->iNamesize + 1)

  if (((mng_spltp)pHeader)->iEntrycount)
    MNG_FREEX (pData, ((mng_spltp)pHeader)->pEntries,
                      ((mng_spltp)pHeader)->iEntrycount *
                      (((mng_spltp)pHeader)->iSampledepth * 3 + sizeof (mng_uint16)) )

  MNG_FREEX (pData, pHeader, sizeof (mng_splt))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_SPLT, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

FREE_CHUNK_HDR (free_hist)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_HIST, MNG_LC_START)
#endif

  MNG_FREEX (pData, pHeader, sizeof (mng_hist))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_HIST, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

FREE_CHUNK_HDR (free_time)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_TIME, MNG_LC_START)
#endif

  MNG_FREEX (pData, pHeader, sizeof (mng_time))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_TIME, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

FREE_CHUNK_HDR (free_mhdr)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_MHDR, MNG_LC_START)
#endif

  MNG_FREEX (pData, pHeader, sizeof (mng_mhdr))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_MHDR, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

FREE_CHUNK_HDR (free_mend)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_MEND, MNG_LC_START)
#endif

  MNG_FREEX (pData, pHeader, sizeof (mng_mend))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_MEND, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

FREE_CHUNK_HDR (free_loop)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_LOOP, MNG_LC_START)
#endif

  if (((mng_loopp)pHeader)->iCount)
    MNG_FREEX (pData, ((mng_loopp)pHeader)->pSignals,
                      ((mng_loopp)pHeader)->iCount * sizeof (mng_uint32) )

  MNG_FREEX (pData, pHeader, sizeof (mng_loop))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_LOOP, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

FREE_CHUNK_HDR (free_endl)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ENDL, MNG_LC_START)
#endif

  MNG_FREEX (pData, pHeader, sizeof (mng_endl))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ENDL, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

FREE_CHUNK_HDR (free_defi)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_DEFI, MNG_LC_START)
#endif

  MNG_FREEX (pData, pHeader, sizeof (mng_defi))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_DEFI, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

FREE_CHUNK_HDR (free_basi)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_BASI, MNG_LC_START)
#endif

  MNG_FREEX (pData, pHeader, sizeof (mng_basi))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_BASI, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

FREE_CHUNK_HDR (free_clon)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_CLON, MNG_LC_START)
#endif

  MNG_FREEX (pData, pHeader, sizeof (mng_clon))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_CLON, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

FREE_CHUNK_HDR (free_past)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_PAST, MNG_LC_START)
#endif

  if (((mng_pastp)pHeader)->iCount)
    MNG_FREEX (pData, ((mng_pastp)pHeader)->pSources,
                      ((mng_pastp)pHeader)->iCount * sizeof (mng_past_source) )

  MNG_FREEX (pData, pHeader, sizeof (mng_past))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_PAST, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

FREE_CHUNK_HDR (free_disc)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_DISC, MNG_LC_START)
#endif

  if (((mng_discp)pHeader)->iCount)
    MNG_FREEX (pData, ((mng_discp)pHeader)->pObjectids,
                      ((mng_discp)pHeader)->iCount * sizeof (mng_uint16) )

  MNG_FREEX (pData, pHeader, sizeof (mng_disc))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_DISC, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

FREE_CHUNK_HDR (free_back)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_BACK, MNG_LC_START)
#endif

  MNG_FREEX (pData, pHeader, sizeof (mng_back))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_BACK, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

FREE_CHUNK_HDR (free_fram)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_FRAM, MNG_LC_START)
#endif

  if (((mng_framp)pHeader)->iNamesize)
    MNG_FREEX (pData, ((mng_framp)pHeader)->zName,
                      ((mng_framp)pHeader)->iNamesize + 1)

  if (((mng_framp)pHeader)->iCount)
    MNG_FREEX (pData, ((mng_framp)pHeader)->pSyncids,
                      ((mng_framp)pHeader)->iCount * sizeof (mng_uint32) )

  MNG_FREEX (pData, pHeader, sizeof (mng_fram))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_FRAM, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

FREE_CHUNK_HDR (free_move)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_MOVE, MNG_LC_START)
#endif

  MNG_FREEX (pData, pHeader, sizeof (mng_move))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_MOVE, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

FREE_CHUNK_HDR (free_clip)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_CLIP, MNG_LC_START)
#endif

  MNG_FREEX (pData, pHeader, sizeof (mng_clip))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_CLIP, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

FREE_CHUNK_HDR (free_show)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_SHOW, MNG_LC_START)
#endif

  MNG_FREEX (pData, pHeader, sizeof (mng_show))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_SHOW, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

FREE_CHUNK_HDR (free_term)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_TERM, MNG_LC_START)
#endif

  MNG_FREEX (pData, pHeader, sizeof (mng_term))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_TERM, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

FREE_CHUNK_HDR (free_save)
{
  mng_save_entryp pEntry = ((mng_savep)pHeader)->pEntries;
  mng_uint32      iX;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_SAVE, MNG_LC_START)
#endif

  for (iX = 0; iX < ((mng_savep)pHeader)->iCount; iX++)
  {
    if (pEntry->iNamesize)
      MNG_FREEX (pData, pEntry->zName, pEntry->iNamesize)

    pEntry = pEntry + sizeof (mng_save_entry);
  }

  if (((mng_savep)pHeader)->iCount)
    MNG_FREEX (pData, ((mng_savep)pHeader)->pEntries,
                      ((mng_savep)pHeader)->iCount * sizeof (mng_save_entry) )

  MNG_FREEX (pData, pHeader, sizeof (mng_save))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_SAVE, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

FREE_CHUNK_HDR (free_seek)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_SEEK, MNG_LC_START)
#endif

  if (((mng_seekp)pHeader)->iNamesize)
    MNG_FREEX (pData, ((mng_seekp)pHeader)->zName,
                      ((mng_seekp)pHeader)->iNamesize + 1)

  MNG_FREEX (pData, pHeader, sizeof (mng_seek))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_SEEK, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

FREE_CHUNK_HDR (free_expi)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_EXPI, MNG_LC_START)
#endif

  if (((mng_expip)pHeader)->iNamesize)
    MNG_FREEX (pData, ((mng_expip)pHeader)->zName,
                      ((mng_expip)pHeader)->iNamesize + 1)

  MNG_FREEX (pData, pHeader, sizeof (mng_expi))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_EXPI, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

FREE_CHUNK_HDR (free_fpri)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_FPRI, MNG_LC_START)
#endif

  MNG_FREEX (pData, pHeader, sizeof (mng_fpri))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_FPRI, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

FREE_CHUNK_HDR (free_need)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_NEED, MNG_LC_START)
#endif

  if (((mng_needp)pHeader)->iKeywordssize)
    MNG_FREEX (pData, ((mng_needp)pHeader)->zKeywords,
                      ((mng_needp)pHeader)->iKeywordssize + 1)

  MNG_FREEX (pData, pHeader, sizeof (mng_need))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_NEED, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

FREE_CHUNK_HDR (free_phyg)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_PHYG, MNG_LC_START)
#endif

  MNG_FREEX (pData, pHeader, sizeof (mng_phyg))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_PHYG, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

#ifdef MNG_INCLUDE_JNG
FREE_CHUNK_HDR (free_jhdr)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_JHDR, MNG_LC_START)
#endif

  MNG_FREEX (pData, pHeader, sizeof (mng_jhdr))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_JHDR, MNG_LC_END)
#endif

  return MNG_NOERROR;
}
#endif /* MNG_INCLUDE_JNG */

/* ************************************************************************** */

#ifdef MNG_INCLUDE_JNG
FREE_CHUNK_HDR (free_jdaa)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_JDAA, MNG_LC_START)
#endif

  if (((mng_jdaap)pHeader)->iDatasize)
    MNG_FREEX (pData, ((mng_jdaap)pHeader)->pData,
                      ((mng_jdaap)pHeader)->iDatasize)

  MNG_FREEX (pData, pHeader, sizeof (mng_jdaa))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_JDAA, MNG_LC_END)
#endif

  return MNG_NOERROR;
}
#endif /* MNG_INCLUDE_JNG */

/* ************************************************************************** */

#ifdef MNG_INCLUDE_JNG
FREE_CHUNK_HDR (free_jdat)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_JDAT, MNG_LC_START)
#endif

  if (((mng_jdatp)pHeader)->iDatasize)
    MNG_FREEX (pData, ((mng_jdatp)pHeader)->pData,
                      ((mng_jdatp)pHeader)->iDatasize)

  MNG_FREEX (pData, pHeader, sizeof (mng_jdat))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_JDAT, MNG_LC_END)
#endif

  return MNG_NOERROR;
}
#endif /* MNG_INCLUDE_JNG */

/* ************************************************************************** */

#ifdef MNG_INCLUDE_JNG
FREE_CHUNK_HDR (free_jsep)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_JSEP, MNG_LC_START)
#endif

  MNG_FREEX (pData, pHeader, sizeof (mng_jsep))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_JSEP, MNG_LC_END)
#endif

  return MNG_NOERROR;
}
#endif /* MNG_INCLUDE_JNG */

/* ************************************************************************** */

FREE_CHUNK_HDR (free_dhdr)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_DHDR, MNG_LC_START)
#endif

  MNG_FREEX (pData, pHeader, sizeof (mng_dhdr))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_DHDR, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

FREE_CHUNK_HDR (free_prom)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_PROM, MNG_LC_START)
#endif

  MNG_FREEX (pData, pHeader, sizeof (mng_prom))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_PROM, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

FREE_CHUNK_HDR (free_ipng)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_IPNG, MNG_LC_START)
#endif

  MNG_FREEX (pData, pHeader, sizeof (mng_ipng))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_IPNG, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

FREE_CHUNK_HDR (free_pplt)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_PPLT, MNG_LC_START)
#endif

  MNG_FREEX (pData, pHeader, sizeof (mng_pplt))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_PPLT, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

FREE_CHUNK_HDR (free_ijng)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_IJNG, MNG_LC_START)
#endif

  MNG_FREEX (pData, pHeader, sizeof (mng_ijng))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_IJNG, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

FREE_CHUNK_HDR (free_drop)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_DROP, MNG_LC_START)
#endif

  if (((mng_dropp)pHeader)->iCount)
    MNG_FREEX (pData, ((mng_dropp)pHeader)->pChunknames,
                      ((mng_dropp)pHeader)->iCount * sizeof (mng_chunkid) )

  MNG_FREEX (pData, pHeader, sizeof (mng_drop))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_DROP, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

FREE_CHUNK_HDR (free_dbyk)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_DBYK, MNG_LC_START)
#endif

  if (((mng_dbykp)pHeader)->iKeywordssize)
    MNG_FREEX (pData, ((mng_dbykp)pHeader)->zKeywords,
                      ((mng_dbykp)pHeader)->iKeywordssize)

  MNG_FREEX (pData, pHeader, sizeof (mng_dbyk))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_DBYK, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

FREE_CHUNK_HDR (free_ordr)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ORDR, MNG_LC_START)
#endif

  if (((mng_ordrp)pHeader)->iCount)
    MNG_FREEX (pData, ((mng_ordrp)pHeader)->pEntries,
                      ((mng_ordrp)pHeader)->iCount * sizeof (mng_ordr_entry) )

  MNG_FREEX (pData, pHeader, sizeof (mng_ordr))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ORDR, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

FREE_CHUNK_HDR (free_magn)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_MAGN, MNG_LC_START)
#endif

  MNG_FREEX (pData, pHeader, sizeof (mng_magn))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_MAGN, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

FREE_CHUNK_HDR (free_unknown)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_UNKNOWN, MNG_LC_START)
#endif

  if (((mng_unknown_chunkp)pHeader)->iDatasize)
    MNG_FREEX (pData, ((mng_unknown_chunkp)pHeader)->pData,
                      ((mng_unknown_chunkp)pHeader)->iDatasize)

  MNG_FREEX (pData, pHeader, sizeof (mng_unknown_chunk))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_UNKNOWN, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */
/* * end of file                                                            * */
/* ************************************************************************** */

