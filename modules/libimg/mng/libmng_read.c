/* ************************************************************************** */
/* *             For conditions of distribution and use,                    * */
/* *                see copyright notice in libmng.h                        * */
/* ************************************************************************** */
/* *                                                                        * */
/* * project   : libmng                                                     * */
/* * file      : libmng_read.c             copyright (c) 2000-2002 G.Juyn   * */
/* * version   : 1.0.5                                                      * */
/* *                                                                        * */
/* * purpose   : Read logic (implementation)                                * */
/* *                                                                        * */
/* * author    : G.Juyn                                                     * */
/* * web       : http://www.3-t.com                                         * */
/* * email     : mailto:info@3-t.com                                        * */
/* *                                                                        * */
/* * comment   : implementation of the high-level read logic                * */
/* *                                                                        * */
/* * changes   : 0.5.1 - 05/08/2000 - G.Juyn                                * */
/* *             - changed strict-ANSI stuff                                * */
/* *             0.5.1 - 05/11/2000 - G.Juyn                                * */
/* *             - added callback error-reporting support                   * */
/* *             0.5.1 - 05/12/2000 - G.Juyn                                * */
/* *             - changed trace to macro for callback error-reporting      * */
/* *                                                                        * */
/* *             0.5.2 - 05/19/2000 - G.Juyn                                * */
/* *             - cleaned up some code regarding mixed support             * */
/* *             0.5.2 - 05/20/2000 - G.Juyn                                * */
/* *             - added support for JNG                                    * */
/* *             0.5.2 - 05/31/2000 - G.Juyn                                * */
/* *             - fixed up punctuation (contribution by Tim Rowley)        * */
/* *                                                                        * */
/* *             0.5.3 - 06/16/2000 - G.Juyn                                * */
/* *             - changed progressive-display processing                   * */
/* *                                                                        * */
/* *             0.9.1 - 07/08/2000 - G.Juyn                                * */
/* *             - changed read-processing for improved I/O-suspension      * */
/* *             0.9.1 - 07/14/2000 - G.Juyn                                * */
/* *             - changed EOF processing behavior                          * */
/* *             0.9.1 - 07/14/2000 - G.Juyn                                * */
/* *             - changed default readbuffer size from 1024 to 4200        * */
/* *                                                                        * */
/* *             0.9.2 - 07/27/2000 - G.Juyn                                * */
/* *             - B110320 - fixed GCC warning about mix-sized pointer math * */
/* *             0.9.2 - 07/31/2000 - G.Juyn                                * */
/* *             - B110546 - fixed for improperly returning UNEXPECTEDEOF   * */
/* *             0.9.2 - 08/04/2000 - G.Juyn                                * */
/* *             - B111096 - fixed large-buffer read-suspension             * */
/* *             0.9.2 - 08/05/2000 - G.Juyn                                * */
/* *             - changed file-prefixes                                    * */
/* *                                                                        * */
/* *             0.9.3 - 08/26/2000 - G.Juyn                                * */
/* *             - added MAGN chunk                                         * */
/* *             0.9.3 - 10/11/2000 - G.Juyn                                * */
/* *             - removed test-MaGN                                        * */
/* *             0.9.3 - 10/16/2000 - G.Juyn                                * */
/* *             - added support for JDAA                                   * */
/* *                                                                        * */
/* *             0.9.5 - 01/23/2001 - G.Juyn                                * */
/* *             - fixed timing-problem with switching framing_modes        * */
/* *                                                                        * */
/* *             1.0.4 - 06/22/2002 - G.Juyn                                * */
/* *             - B495443 - incorrect suspend check in read_databuffer     * */
/* *                                                                        * */
/* *             1.0.5 - 07/04/2002 - G.Juyn                                * */
/* *             - added errorcode for extreme chunk-sizes                  * */
/* *             1.0.5 - 07/08/2002 - G.Juyn                                * */
/* *             - B578572 - removed eMNGma hack (thanks Dimitri!)          * */
/* *             1.0.5 - 07/16/2002 - G.Juyn                                * */
/* *             - B581625 - large chunks fail with suspension reads        * */
/* *             1.0.5 - 08/19/2002 - G.Juyn                                * */
/* *             - B597134 - libmng pollutes the linker namespace           * */
/* *             - added HLAPI function to copy chunks                      * */
/* *             1.0.5 - 09/16/2002 - G.Juyn                                * */
/* *             - added event handling for dynamic MNG                     * */
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
#include "libmng_objects.h"
#include "libmng_object_prc.h"
#include "libmng_chunks.h"
#include "libmng_chunk_prc.h"
#include "libmng_chunk_io.h"
#include "libmng_display.h"
#include "libmng_read.h"

#if defined(__BORLANDC__) && defined(MNG_STRICT_ANSI)
#pragma option -A                      /* force ANSI-C */
#endif

/* ************************************************************************** */

#ifdef MNG_INCLUDE_READ_PROCS

/* ************************************************************************** */

mng_retcode mng_process_eof (mng_datap pData)
{
  if (!pData->bEOF)                    /* haven't closed the stream yet ? */
  {
    pData->bEOF = MNG_TRUE;            /* now we do! */

    if (!pData->fClosestream ((mng_handle)pData))
      MNG_ERROR (pData, MNG_APPIOERROR)
  }

  return MNG_NOERROR;
}

/* ************************************************************************** */

MNG_LOCAL mng_retcode read_databuffer (mng_datap    pData,
                                       mng_uint8p   pBuf,
                                       mng_uint8p * pBufnext,
                                       mng_uint32   iSize,
                                       mng_uint32 * iRead)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_DATABUFFER, MNG_LC_START)
#endif

  if (pData->bSuspensionmode)
  {
    mng_uint8p pTemp;
    mng_uint32 iTemp;

    *iRead = 0;                        /* let's be negative about the outcome */

    if (!pData->pSuspendbuf)           /* need to create a suspension buffer ? */
    {
      pData->iSuspendbufsize = MNG_SUSPENDBUFFERSIZE;
                                       /* so, create it */
      MNG_ALLOC (pData, pData->pSuspendbuf, pData->iSuspendbufsize)

      pData->iSuspendbufleft = 0;      /* make sure to fill it first time */
      pData->pSuspendbufnext = pData->pSuspendbuf;
    }
                                       /* more than our buffer can hold ? */
    if (iSize > pData->iSuspendbufsize)
    {
      mng_uint32 iRemain;

      if (!*pBufnext)                  /* first time ? */
      {
        if (pData->iSuspendbufleft)    /* do we have some data left ? */
        {                              /* then copy it */
          MNG_COPY (pBuf, pData->pSuspendbufnext, pData->iSuspendbufleft)
                                       /* fixup variables */
          *pBufnext              = pBuf + pData->iSuspendbufleft;
          pData->pSuspendbufnext = pData->pSuspendbuf;
          pData->iSuspendbufleft = 0;
        }
        else
        {
          *pBufnext              = pBuf;
        }
      }
                                       /* calculate how much to get */
      iRemain = iSize - (mng_uint32)(*pBufnext - pBuf);
                                       /* let's go get it */
      if (!pData->fReaddata (((mng_handle)pData), *pBufnext, iRemain, &iTemp))
        MNG_ERROR (pData, MNG_APPIOERROR);
                                       /* first read after suspension return 0 means EOF */
      if ((pData->iSuspendpoint) && (iTemp == 0))
      {                                /* that makes it final */
        mng_retcode iRetcode = mng_process_eof (pData);

        if (iRetcode)                  /* on error bail out */
          return iRetcode;
                                       /* indicate the source is depleted */
        *iRead = iSize - iRemain + iTemp;
      }
      else
      {
        if (iTemp < iRemain)           /* suspension required ? */
        {
          *pBufnext         = *pBufnext + iTemp;
          pData->bSuspended = MNG_TRUE;
        }
        else
        {
          *iRead = iSize;              /* got it all now ! */
        }
      }
    }
    else
    {                                  /* need to read some more ? */
      while ((!pData->bSuspended) && (!pData->bEOF) && (iSize > pData->iSuspendbufleft))
      {                                /* not enough space left in buffer ? */
        if (pData->iSuspendbufsize - pData->iSuspendbufleft -
            (mng_uint32)(pData->pSuspendbufnext - pData->pSuspendbuf) <
                                                          MNG_SUSPENDREQUESTSIZE)
        {
          if (pData->iSuspendbufleft)  /* then lets shift (if there's anything left) */
            MNG_COPY (pData->pSuspendbuf, pData->pSuspendbufnext, pData->iSuspendbufleft)
                                       /* adjust running pointer */
          pData->pSuspendbufnext = pData->pSuspendbuf;
        }
                                       /* still not enough room ? */
        if (pData->iSuspendbufsize - pData->iSuspendbufleft < MNG_SUSPENDREQUESTSIZE)
          MNG_ERROR (pData, MNG_INTERNALERROR)
                                       /* now read some more data */
        pTemp = pData->pSuspendbufnext + pData->iSuspendbufleft;

        if (!pData->fReaddata (((mng_handle)pData), pTemp, MNG_SUSPENDREQUESTSIZE, &iTemp))
          MNG_ERROR (pData, MNG_APPIOERROR);
                                       /* adjust fill-counter */
        pData->iSuspendbufleft += iTemp;
                                       /* first read after suspension returning 0 means EOF */
        if ((pData->iSuspendpoint) && (iTemp == 0))
        {                              /* that makes it final */
          mng_retcode iRetcode = mng_process_eof (pData);

          if (iRetcode)                /* on error bail out */
            return iRetcode;

          if (pData->iSuspendbufleft)  /* return the leftover scraps */
            MNG_COPY (pBuf, pData->pSuspendbufnext, pData->iSuspendbufleft)
                                       /* and indicate so */
          *iRead = pData->iSuspendbufleft;
          pData->pSuspendbufnext = pData->pSuspendbuf;
          pData->iSuspendbufleft = 0;
        }
        else
        {                              /* suspension required ? */
          if ((iSize > pData->iSuspendbufleft) && (iTemp < MNG_SUSPENDREQUESTSIZE))
            pData->bSuspended = MNG_TRUE;

        }

        pData->iSuspendpoint = 0;      /* reset it here in case we loop back */ 
      }

      if ((!pData->bSuspended) && (!pData->bEOF))
      {                                /* return the data ! */
        MNG_COPY (pBuf, pData->pSuspendbufnext, iSize)

        *iRead = iSize;                /* returned it all */
                                       /* adjust suspension-buffer variables */
        pData->pSuspendbufnext += iSize;
        pData->iSuspendbufleft -= iSize;
      }
    }
  }
  else
  {
    if (!pData->fReaddata (((mng_handle)pData), (mng_ptr)pBuf, iSize, iRead))
    {
      if (*iRead == 0)                 /* suspension required ? */
        pData->bSuspended = MNG_TRUE;
      else
        MNG_ERROR (pData, MNG_APPIOERROR);
    }
  }

  pData->iSuspendpoint = 0;            /* safely reset it here ! */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_DATABUFFER, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

MNG_LOCAL mng_retcode process_raw_chunk (mng_datap  pData,
                                         mng_uint8p pBuf,
                                         mng_uint32 iBuflen)
{
  /* the table-idea & binary search code was adapted from
     libpng 1.1.0 (pngread.c) */
  /* NOTE1: the table must remain sorted by chunkname, otherwise the binary
     search will break !!! (ps. watch upper-/lower-case chunknames !!) */
  /* NOTE2: the layout must remain equal to the header part of all the
     chunk-structures (yes, that means even the pNext and pPrev fields;
     it's wasting a bit of space, but hey, the code is a lot easier) */

  mng_chunk_header chunk_unknown = {MNG_UINT_HUH, mng_init_unknown, mng_free_unknown,
                                    mng_read_unknown, mng_write_unknown, mng_assign_unknown, 0, 0};

  mng_chunk_header chunk_table [] =
  {
    {MNG_UINT_BACK, mng_init_back, mng_free_back, mng_read_back, mng_write_back, mng_assign_back, 0, 0},
    {MNG_UINT_BASI, mng_init_basi, mng_free_basi, mng_read_basi, mng_write_basi, mng_assign_basi, 0, 0},
    {MNG_UINT_CLIP, mng_init_clip, mng_free_clip, mng_read_clip, mng_write_clip, mng_assign_clip, 0, 0},
    {MNG_UINT_CLON, mng_init_clon, mng_free_clon, mng_read_clon, mng_write_clon, mng_assign_clon, 0, 0},
    {MNG_UINT_DBYK, mng_init_dbyk, mng_free_dbyk, mng_read_dbyk, mng_write_dbyk, mng_assign_dbyk, 0, 0},
    {MNG_UINT_DEFI, mng_init_defi, mng_free_defi, mng_read_defi, mng_write_defi, mng_assign_defi, 0, 0},
    {MNG_UINT_DHDR, mng_init_dhdr, mng_free_dhdr, mng_read_dhdr, mng_write_dhdr, mng_assign_dhdr, 0, 0},
    {MNG_UINT_DISC, mng_init_disc, mng_free_disc, mng_read_disc, mng_write_disc, mng_assign_disc, 0, 0},
    {MNG_UINT_DROP, mng_init_drop, mng_free_drop, mng_read_drop, mng_write_drop, mng_assign_drop, 0, 0},
    {MNG_UINT_ENDL, mng_init_endl, mng_free_endl, mng_read_endl, mng_write_endl, mng_assign_endl, 0, 0},
    {MNG_UINT_FRAM, mng_init_fram, mng_free_fram, mng_read_fram, mng_write_fram, mng_assign_fram, 0, 0},
    {MNG_UINT_IDAT, mng_init_idat, mng_free_idat, mng_read_idat, mng_write_idat, mng_assign_idat, 0, 0},  /* 12-th element! */
    {MNG_UINT_IEND, mng_init_iend, mng_free_iend, mng_read_iend, mng_write_iend, mng_assign_iend, 0, 0},
    {MNG_UINT_IHDR, mng_init_ihdr, mng_free_ihdr, mng_read_ihdr, mng_write_ihdr, mng_assign_ihdr, 0, 0},
    {MNG_UINT_IJNG, mng_init_ijng, mng_free_ijng, mng_read_ijng, mng_write_ijng, mng_assign_ijng, 0, 0},
    {MNG_UINT_IPNG, mng_init_ipng, mng_free_ipng, mng_read_ipng, mng_write_ipng, mng_assign_ipng, 0, 0},
#ifdef MNG_INCLUDE_JNG
    {MNG_UINT_JDAA, mng_init_jdaa, mng_free_jdaa, mng_read_jdaa, mng_write_jdaa, mng_assign_jdaa, 0, 0},
    {MNG_UINT_JDAT, mng_init_jdat, mng_free_jdat, mng_read_jdat, mng_write_jdat, mng_assign_jdat, 0, 0},
    {MNG_UINT_JHDR, mng_init_jhdr, mng_free_jhdr, mng_read_jhdr, mng_write_jhdr, mng_assign_jhdr, 0, 0},
    {MNG_UINT_JSEP, mng_init_jsep, mng_free_jsep, mng_read_jsep, mng_write_jsep, mng_assign_jsep, 0, 0},
    {MNG_UINT_JdAA, mng_init_jdaa, mng_free_jdaa, mng_read_jdaa, mng_write_jdaa, mng_assign_jdaa, 0, 0},
#endif
    {MNG_UINT_LOOP, mng_init_loop, mng_free_loop, mng_read_loop, mng_write_loop, mng_assign_loop, 0, 0},
    {MNG_UINT_MAGN, mng_init_magn, mng_free_magn, mng_read_magn, mng_write_magn, mng_assign_magn, 0, 0},
    {MNG_UINT_MEND, mng_init_mend, mng_free_mend, mng_read_mend, mng_write_mend, mng_assign_mend, 0, 0},
    {MNG_UINT_MHDR, mng_init_mhdr, mng_free_mhdr, mng_read_mhdr, mng_write_mhdr, mng_assign_mhdr, 0, 0},
    {MNG_UINT_MOVE, mng_init_move, mng_free_move, mng_read_move, mng_write_move, mng_assign_move, 0, 0},
    {MNG_UINT_ORDR, mng_init_ordr, mng_free_ordr, mng_read_ordr, mng_write_ordr, mng_assign_ordr, 0, 0},
    {MNG_UINT_PAST, mng_init_past, mng_free_past, mng_read_past, mng_write_past, mng_assign_past, 0, 0},
    {MNG_UINT_PLTE, mng_init_plte, mng_free_plte, mng_read_plte, mng_write_plte, mng_assign_plte, 0, 0},
    {MNG_UINT_PPLT, mng_init_pplt, mng_free_pplt, mng_read_pplt, mng_write_pplt, mng_assign_pplt, 0, 0},
    {MNG_UINT_PROM, mng_init_prom, mng_free_prom, mng_read_prom, mng_write_prom, mng_assign_prom, 0, 0},
    {MNG_UINT_SAVE, mng_init_save, mng_free_save, mng_read_save, mng_write_save, mng_assign_save, 0, 0},
    {MNG_UINT_SEEK, mng_init_seek, mng_free_seek, mng_read_seek, mng_write_seek, mng_assign_seek, 0, 0},
    {MNG_UINT_SHOW, mng_init_show, mng_free_show, mng_read_show, mng_write_show, mng_assign_show, 0, 0},
    {MNG_UINT_TERM, mng_init_term, mng_free_term, mng_read_term, mng_write_term, mng_assign_term, 0, 0},
    {MNG_UINT_bKGD, mng_init_bkgd, mng_free_bkgd, mng_read_bkgd, mng_write_bkgd, mng_assign_bkgd, 0, 0},
    {MNG_UINT_cHRM, mng_init_chrm, mng_free_chrm, mng_read_chrm, mng_write_chrm, mng_assign_chrm, 0, 0},
    {MNG_UINT_eXPI, mng_init_expi, mng_free_expi, mng_read_expi, mng_write_expi, mng_assign_expi, 0, 0},
    {MNG_UINT_evNT, mng_init_evnt, mng_free_evnt, mng_read_evnt, mng_write_evnt, mng_assign_evnt, 0, 0},
    {MNG_UINT_fPRI, mng_init_fpri, mng_free_fpri, mng_read_fpri, mng_write_fpri, mng_assign_fpri, 0, 0},
    {MNG_UINT_gAMA, mng_init_gama, mng_free_gama, mng_read_gama, mng_write_gama, mng_assign_gama, 0, 0},
    {MNG_UINT_hIST, mng_init_hist, mng_free_hist, mng_read_hist, mng_write_hist, mng_assign_hist, 0, 0},
    {MNG_UINT_iCCP, mng_init_iccp, mng_free_iccp, mng_read_iccp, mng_write_iccp, mng_assign_iccp, 0, 0},
    {MNG_UINT_iTXt, mng_init_itxt, mng_free_itxt, mng_read_itxt, mng_write_itxt, mng_assign_itxt, 0, 0},
    {MNG_UINT_nEED, mng_init_need, mng_free_need, mng_read_need, mng_write_need, mng_assign_need, 0, 0},
/* TODO:     {MNG_UINT_oFFs, 0, 0, 0, 0, 0, 0},  */
/* TODO:     {MNG_UINT_pCAL, 0, 0, 0, 0, 0, 0},  */
    {MNG_UINT_pHYg, mng_init_phyg, mng_free_phyg, mng_read_phyg, mng_write_phyg, mng_assign_phyg, 0, 0},
    {MNG_UINT_pHYs, mng_init_phys, mng_free_phys, mng_read_phys, mng_write_phys, mng_assign_phys, 0, 0},
    {MNG_UINT_sBIT, mng_init_sbit, mng_free_sbit, mng_read_sbit, mng_write_sbit, mng_assign_sbit, 0, 0},
/* TODO:     {MNG_UINT_sCAL, 0, 0, 0, 0, 0, 0},  */
    {MNG_UINT_sPLT, mng_init_splt, mng_free_splt, mng_read_splt, mng_write_splt, mng_assign_splt, 0, 0},
    {MNG_UINT_sRGB, mng_init_srgb, mng_free_srgb, mng_read_srgb, mng_write_srgb, mng_assign_srgb, 0, 0},
    {MNG_UINT_tEXt, mng_init_text, mng_free_text, mng_read_text, mng_write_text, mng_assign_text, 0, 0},
    {MNG_UINT_tIME, mng_init_time, mng_free_time, mng_read_time, mng_write_time, mng_assign_time, 0, 0},
    {MNG_UINT_tRNS, mng_init_trns, mng_free_trns, mng_read_trns, mng_write_trns, mng_assign_trns, 0, 0},
    {MNG_UINT_zTXt, mng_init_ztxt, mng_free_ztxt, mng_read_ztxt, mng_write_ztxt, mng_assign_ztxt, 0, 0},
  };
                                       /* binary search variables */
  mng_int32         iTop, iLower, iUpper, iMiddle;
  mng_chunk_headerp pEntry;            /* pointer to found entry */
  mng_chunkid       iChunkname;        /* the chunk's tag */
  mng_chunkp        pChunk;            /* chunk structure (if #define MNG_STORE_CHUNKS) */
  mng_retcode       iRetcode;          /* temporary error-code */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_RAW_CHUNK, MNG_LC_START)
#endif
                                       /* reset timer indicator on read-cycle */
  if ((pData->bReading) && (!pData->bDisplaying))
    pData->bTimerset = MNG_FALSE;
                                       /* get the chunkname */
  iChunkname = (mng_chunkid)(mng_get_uint32 (pBuf));

  pBuf += sizeof (mng_chunkid);        /* adjust the buffer */
  iBuflen -= sizeof (mng_chunkid);
                                       /* determine max index of table */
  iTop = (sizeof (chunk_table) / sizeof (chunk_table [0])) - 1;

  /* binary search; with 54 chunks, worst-case is 7 comparisons */
  iLower  = 0;
  iMiddle = 11;                        /* start with the IDAT entry */
  iUpper  = iTop;
  pEntry  = 0;                         /* no goods yet! */
  pChunk  = 0;

  do                                   /* the binary search itself */
    {
      if (chunk_table [iMiddle].iChunkname < iChunkname)
        iLower = iMiddle + 1;
      else if (chunk_table [iMiddle].iChunkname > iChunkname)
        iUpper = iMiddle - 1;
      else
      {
        pEntry = &chunk_table [iMiddle];
        break;
      }

      iMiddle = (iLower + iUpper) >> 1;
    }
  while (iLower <= iUpper);

  if (!pEntry)                         /* unknown chunk ? */
    pEntry = &chunk_unknown;           /* make it so! */

  pData->iChunkname = iChunkname;      /* keep track of where we are */
  pData->iChunkseq++;

  if (pEntry->fRead)                   /* read-callback available ? */
  {
    iRetcode = pEntry->fRead (pData, pEntry, iBuflen, (mng_ptr)pBuf, &pChunk);

    if (!iRetcode)                     /* everything oke ? */
    {                                  /* remember unknown chunk's id */
      if ((pChunk) && (pEntry == &chunk_unknown))
        ((mng_chunk_headerp)pChunk)->iChunkname = iChunkname;
    }
  }
  else
    iRetcode = MNG_NOERROR;

  if (pChunk)                          /* store this chunk ? */
    mng_add_chunk (pData, pChunk);     /* do it */

#ifdef MNG_INCLUDE_JNG                 /* implicit EOF ? */
  if ((!pData->bHasMHDR) && (!pData->bHasIHDR) && (!pData->bHasJHDR))
#else
  if ((!pData->bHasMHDR) && (!pData->bHasIHDR))
#endif
    iRetcode = mng_process_eof (pData);/* then do some EOF processing */

  if (iRetcode)                        /* on error bail out */
    return iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_RAW_CHUNK, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

MNG_LOCAL mng_retcode read_chunk (mng_datap  pData)
{
  mng_uint32  iBufmax   = pData->iReadbufsize;
  mng_uint8p  pBuf      = pData->pReadbuf;
  mng_uint32  iBuflen   = 0;           /* number of bytes requested */
  mng_uint32  iRead     = 0;           /* number of bytes read */
  mng_uint32  iCrc;                    /* calculated CRC */
  mng_retcode iRetcode  = MNG_NOERROR;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_CHUNK, MNG_LC_START)
#endif

#ifdef MNG_SUPPORT_DISPLAY
  if (pData->pCurraniobj)              /* processing an animation object ? */
  {
    do                                 /* process it then */
    {
      iRetcode = ((mng_object_headerp)pData->pCurraniobj)->fProcess (pData, pData->pCurraniobj);
                                       /* refresh needed ? */
/*      if ((!iRetcode) && (!pData->bTimerset) && (pData->bNeedrefresh))
        iRetcode = display_progressive_refresh (pData, 1); */
                                       /* can we advance to next object ? */
      if ((!iRetcode) && (pData->pCurraniobj) &&
          (!pData->bTimerset) && (!pData->bSectionwait))
      {                                /* reset timer indicator on read-cycle */
        if ((pData->bReading) && (!pData->bDisplaying))
          pData->bTimerset = MNG_FALSE;

        pData->pCurraniobj = ((mng_object_headerp)pData->pCurraniobj)->pNext;
                                       /* TERM processing to be done ? */
        if ((!pData->pCurraniobj) && (pData->bHasTERM) && (!pData->bHasMHDR))
          iRetcode = mng_process_display_mend (pData);
      }
    }                                  /* until error or a break or no more objects */
    while ((!iRetcode) && (pData->pCurraniobj) &&
           (!pData->bTimerset) && (!pData->bSectionwait) && (!pData->bFreezing));
  }
  else
  {
    if (pData->iBreakpoint)            /* do we need to finish something first ? */
    {
      switch (pData->iBreakpoint)      /* return to broken display routine */
      {
        case  1 : { iRetcode = mng_process_display_fram2 (pData); break; }
        case  2 : { iRetcode = mng_process_display_ihdr  (pData); break; }
        case  3 : ;                     /* same as 4 !!! */
        case  4 : { iRetcode = mng_process_display_show  (pData); break; }
        case  5 : { iRetcode = mng_process_display_clon2 (pData); break; }
#ifdef MNG_INCLUDE_JNG
        case  7 : { iRetcode = mng_process_display_jhdr  (pData); break; }
#endif
        case  6 : ;                     /* same as 8 !!! */
        case  8 : { iRetcode = mng_process_display_iend  (pData); break; }
        case  9 : { iRetcode = mng_process_display_magn2 (pData); break; }
        case 10 : { iRetcode = mng_process_display_mend2 (pData); break; }
        case 11 : { iRetcode = mng_process_display_past2 (pData); break; }
      }
    }
  }

  if (iRetcode)                        /* on error bail out */
    return iRetcode;

#endif /* MNG_SUPPORT_DISPLAY */
                                       /* can we continue processing now, or do we */
                                       /* need to wait for the timer to finish (again) ? */
#ifdef MNG_SUPPORT_DISPLAY
  if ((!pData->bTimerset) && (!pData->bSectionwait) && (!pData->bEOF))
#else
  if (!pData->bEOF)
#endif
  {
#ifdef MNG_SUPPORT_DISPLAY
                                       /* freezing in progress ? */
    if ((pData->bFreezing) && (pData->iSuspendpoint == 0))
      pData->bRunning = MNG_FALSE;     /* then this is the right moment to do it */
#endif

    if (pData->iSuspendpoint <= 2)
    {
      iBuflen  = sizeof (mng_uint32);  /* read length */
      iRetcode = read_databuffer (pData, pBuf, &pData->pReadbufnext, iBuflen, &iRead);

      if (iRetcode)                    /* bail on errors */
        return iRetcode;

      if (pData->bSuspended)           /* suspended ? */
        pData->iSuspendpoint = 2;
      else                             /* save the length */
        pData->iChunklen = mng_get_uint32 (pBuf);

    }

    if (!pData->bSuspended)            /* still going ? */
    {                                  /* previously suspended or not eof ? */
      if ((pData->iSuspendpoint > 2) || (iRead == iBuflen))
      {                                /* determine length chunkname + data + crc */
        iBuflen = pData->iChunklen + (mng_uint32)(sizeof (mng_chunkid) + sizeof (iCrc));

        if (iBuflen < iBufmax)         /* does it fit in default buffer ? */
        {                              /* note that we don't use the full size
                                          so there's always a zero-byte at the
                                          very end !!! */
          iRetcode = read_databuffer (pData, pBuf, &pData->pReadbufnext, iBuflen, &iRead);

          if (iRetcode)                /* bail on errors */
            return iRetcode;

          if (pData->bSuspended)       /* suspended ? */
            pData->iSuspendpoint = 3;
          else
          {
            if (iRead != iBuflen)      /* did we get all the data ? */
              iRetcode = MNG_UNEXPECTEDEOF;
            else
            {
              mng_uint32 iL = iBuflen - (mng_uint32)(sizeof (iCrc));
                                       /* calculate the crc */
              iCrc = mng_crc (pData, pBuf, iL);
                                       /* and check it */
              if (!(iCrc == mng_get_uint32 (pBuf + iL)))
                iRetcode = MNG_INVALIDCRC;
              else
                iRetcode = process_raw_chunk (pData, pBuf, iL);
            }
          }
        }
        else
        {
          if (iBuflen > 16777216)      /* is the length incredible? */
            MNG_ERROR (pData, MNG_IMPROBABLELENGTH);

          if (!pData->iSuspendpoint)   /* create additional large buffer ? */
          {                            /* again reserve space for the last zero-byte */
            pData->iLargebufsize = iBuflen + 1;
            pData->pLargebufnext = MNG_NULL;
            MNG_ALLOC (pData, pData->pLargebuf, pData->iLargebufsize)
          }

          iRetcode = read_databuffer (pData, pData->pLargebuf, &pData->pLargebufnext, iBuflen, &iRead);

          if (iRetcode)
            return iRetcode;

          if (pData->bSuspended)       /* suspended ? */
            pData->iSuspendpoint = 4;
          else
          {
            if (iRead != iBuflen)      /* did we get all the data ? */
              iRetcode = MNG_UNEXPECTEDEOF;
            else
            {
              mng_uint32 iL = iBuflen - (mng_uint32)(sizeof (iCrc));
                                       /* calculate the crc */
              iCrc = mng_crc (pData, pData->pLargebuf, iL);
                                       /* and check it */
              if (!(iCrc == mng_get_uint32 (pData->pLargebuf + iL)))
                iRetcode = MNG_INVALIDCRC;
              else
                iRetcode = process_raw_chunk (pData, pData->pLargebuf, iL);
            }
                                       /* cleanup additional large buffer */
            MNG_FREE (pData, pData->pLargebuf, pData->iLargebufsize)
          }  
        }

        if (iRetcode)                  /* on error bail out */
          return iRetcode;

      }
      else
      {                                /* that's final */
        iRetcode = mng_process_eof (pData);

        if (iRetcode)                  /* on error bail out */
          return iRetcode;

        if ((iRead != 0) ||            /* did we get an unexpected eof ? */
#ifdef MNG_INCLUDE_JNG
            (pData->bHasIHDR || pData->bHasMHDR || pData->bHasJHDR))
#else
            (pData->bHasIHDR || pData->bHasMHDR))
#endif
          MNG_ERROR (pData, MNG_UNEXPECTEDEOF);
      } 
    }
  }

#ifdef MNG_SUPPORT_DISPLAY             /* refresh needed ? */
  if ((!pData->bTimerset) && (!pData->bSuspended) && (pData->bNeedrefresh))
  {
    iRetcode = mng_display_progressive_refresh (pData, 1);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
  }
#endif

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_CHUNK, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode mng_read_graphic (mng_datap pData)
{
  mng_uint32  iBuflen;                 /* number of bytes requested */
  mng_uint32  iRead;                   /* number of bytes read */
  mng_retcode iRetcode;                /* temporary error-code */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_GRAPHIC, MNG_LC_START)
#endif

  if (!pData->pReadbuf)                /* buffer allocated ? */
  {
    pData->iReadbufsize = 4200;        /* allocate a default read buffer */
    MNG_ALLOC (pData, pData->pReadbuf, pData->iReadbufsize)
  }
                                       /* haven't processed the signature ? */
  if ((!pData->bHavesig) || (pData->iSuspendpoint == 1))
  {
    iBuflen = 2 * sizeof (mng_uint32); /* read signature */

    iRetcode = read_databuffer (pData, pData->pReadbuf, &pData->pReadbufnext, iBuflen, &iRead);

    if (iRetcode)
      return iRetcode;

    if (pData->bSuspended)             /* input suspension ? */
      pData->iSuspendpoint = 1;
    else
    {
      if (iRead != iBuflen)            /* full signature received ? */
        MNG_ERROR (pData, MNG_UNEXPECTEDEOF);
                                       /* is it a valid signature ? */
      if (mng_get_uint32 (pData->pReadbuf) == PNG_SIG)
        pData->eSigtype = mng_it_png;
      else
      if (mng_get_uint32 (pData->pReadbuf) == JNG_SIG)
        pData->eSigtype = mng_it_jng;
      else
      if (mng_get_uint32 (pData->pReadbuf) == MNG_SIG)
        pData->eSigtype = mng_it_mng;
      else
        MNG_ERROR (pData, MNG_INVALIDSIG);
                                       /* all of it ? */
      if (mng_get_uint32 (pData->pReadbuf+4) != POST_SIG)
        MNG_ERROR (pData, MNG_INVALIDSIG);

      pData->bHavesig = MNG_TRUE;
    }
  }

  if (!pData->bSuspended)              /* still going ? */
  {
    do
    {                                  /* reset timer during mng_read() ? */
      if ((pData->bReading) && (!pData->bDisplaying))
        pData->bTimerset = MNG_FALSE;

      iRetcode = read_chunk (pData);   /* process a chunk */

      if (iRetcode)                    /* on error bail out */
        return iRetcode;
    }
#ifdef MNG_SUPPORT_DISPLAY             /* until EOF or a break-request */
    while (((!pData->bEOF) || (pData->pCurraniobj)) &&
           (!pData->bSuspended) && (!pData->bSectionwait) &&
           ((!pData->bTimerset) || ((pData->bReading) && (!pData->bDisplaying))));
#else
    while ((!pData->bEOF) && (!pData->bSuspended));
#endif
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_GRAPHIC, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

#endif /* MNG_INCLUDE_READ_PROCS */

/* ************************************************************************** */
/* * end of file                                                            * */
/* ************************************************************************** */

