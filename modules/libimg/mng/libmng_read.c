/* ************************************************************************** */
/* *             For conditions of distribution and use,                    * */
/* *                see copyright notice in libmng.h                        * */
/* ************************************************************************** */
/* *                                                                        * */
/* * project   : libmng                                                     * */
/* * file      : libmng_read.c             copyright (c) 2000 G.Juyn        * */
/* * version   : 1.0.0                                                      * */
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
/* *             0.9.5 -  1/23/2001 - G.Juyn                                * */
/* *             - fixed timing-problem with switching framing_modes        * */
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

mng_retcode process_eof (mng_datap pData)
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

mng_retcode read_databuffer (mng_datap    pData,
                             mng_uint8p   pBuf,
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

      if (!pData->pReadbufnext)        /* first time ? */
      {
        if (pData->iSuspendbufleft)    /* do we have some data left ? */
        {                              /* then copy it */
          MNG_COPY (pBuf, pData->pSuspendbufnext, pData->iSuspendbufleft)
                                       /* fixup variables */
          pData->pReadbufnext    = pBuf + pData->iSuspendbufleft;
          pData->pSuspendbufnext = pData->pSuspendbuf;
          pData->iSuspendbufleft = 0;
        }
        else
        {
          pData->pReadbufnext    = pBuf;
        }
      }
                                       /* calculate how much to get */
      iRemain = iSize - (mng_uint32)(pData->pReadbufnext - pBuf);
                                       /* let's go get it */
      if (!pData->fReaddata (((mng_handle)pData), pData->pReadbufnext, iRemain, &iTemp))
        MNG_ERROR (pData, MNG_APPIOERROR);
                                       /* first read after suspension return 0 means EOF */
      if ((pData->iSuspendpoint) && (iTemp == 0))
      {                                /* that makes it final */
        mng_retcode iRetcode = process_eof (pData);

        if (iRetcode)                  /* on error bail out */
          return iRetcode;
                                       /* indicate the source is depleted */
        *iRead = iSize - iRemain + iTemp;
      }
      else
      {
        if (iTemp < iRemain)           /* suspension required ? */
        {
          pData->pReadbufnext = pData->pReadbufnext + iTemp;
          pData->bSuspended   = MNG_TRUE;
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
          mng_retcode iRetcode = process_eof (pData);

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
      if (iRead == 0)                  /* suspension required ? */
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

mng_retcode process_raw_chunk (mng_datap  pData,
                               mng_uint8p pBuf,
                               mng_uint32 iBuflen)
{
  /* the table-idea & binary search code was adapted from
     libpng 1.1.0 (pngread.c) */
  /* NOTE1: the table must remain sorted by chunkname, otherwise the binary
     search will break !!! */
  /* NOTE2: the layout must remain equal to the header part of all the
     chunk-structures (yes, that means even the pNext and pPrev fields;
     it's wasting a bit of space, but hey, the code is a lot easier) */

  mng_chunk_header chunk_unknown = {MNG_UINT_HUH, init_unknown, free_unknown,
                                    read_unknown, write_unknown, 0, 0};

  mng_chunk_header chunk_table [] =
  {
    {MNG_UINT_BACK, init_back, free_back, read_back, write_back, 0, 0},
    {MNG_UINT_BASI, init_basi, free_basi, read_basi, write_basi, 0, 0},
    {MNG_UINT_CLIP, init_clip, free_clip, read_clip, write_clip, 0, 0},
    {MNG_UINT_CLON, init_clon, free_clon, read_clon, write_clon, 0, 0},
    {MNG_UINT_DBYK, init_dbyk, free_dbyk, read_dbyk, write_dbyk, 0, 0},
    {MNG_UINT_DEFI, init_defi, free_defi, read_defi, write_defi, 0, 0},
    {MNG_UINT_DHDR, init_dhdr, free_dhdr, read_dhdr, write_dhdr, 0, 0},
    {MNG_UINT_DISC, init_disc, free_disc, read_disc, write_disc, 0, 0},
    {MNG_UINT_DROP, init_drop, free_drop, read_drop, write_drop, 0, 0},
    {MNG_UINT_ENDL, init_endl, free_endl, read_endl, write_endl, 0, 0},
    {MNG_UINT_FRAM, init_fram, free_fram, read_fram, write_fram, 0, 0},
    {MNG_UINT_IDAT, init_idat, free_idat, read_idat, write_idat, 0, 0},  /* 12-th element! */
    {MNG_UINT_IEND, init_iend, free_iend, read_iend, write_iend, 0, 0},
    {MNG_UINT_IHDR, init_ihdr, free_ihdr, read_ihdr, write_ihdr, 0, 0},
    {MNG_UINT_IJNG, init_ijng, free_ijng, read_ijng, write_ijng, 0, 0},
    {MNG_UINT_IPNG, init_ipng, free_ipng, read_ipng, write_ipng, 0, 0},
#ifdef MNG_INCLUDE_JNG
    {MNG_UINT_JDAA, init_jdaa, free_jdaa, read_jdaa, write_jdaa, 0, 0},
    {MNG_UINT_JDAT, init_jdat, free_jdat, read_jdat, write_jdat, 0, 0},
    {MNG_UINT_JHDR, init_jhdr, free_jhdr, read_jhdr, write_jhdr, 0, 0},
    {MNG_UINT_JSEP, init_jsep, free_jsep, read_jsep, write_jsep, 0, 0},
    {MNG_UINT_JdAA, init_jdaa, free_jdaa, read_jdaa, write_jdaa, 0, 0},
#endif
    {MNG_UINT_LOOP, init_loop, free_loop, read_loop, write_loop, 0, 0},
    {MNG_UINT_MAGN, init_magn, free_magn, read_magn, write_magn, 0, 0},
    {MNG_UINT_MEND, init_mend, free_mend, read_mend, write_mend, 0, 0},
    {MNG_UINT_MHDR, init_mhdr, free_mhdr, read_mhdr, write_mhdr, 0, 0},
    {MNG_UINT_MOVE, init_move, free_move, read_move, write_move, 0, 0},
    {MNG_UINT_ORDR, init_ordr, free_ordr, read_ordr, write_ordr, 0, 0},
    {MNG_UINT_PAST, init_past, free_past, read_past, write_past, 0, 0},
    {MNG_UINT_PLTE, init_plte, free_plte, read_plte, write_plte, 0, 0},
    {MNG_UINT_PPLT, init_pplt, free_pplt, read_pplt, write_pplt, 0, 0},
    {MNG_UINT_PROM, init_prom, free_prom, read_prom, write_prom, 0, 0},
    {MNG_UINT_SAVE, init_save, free_save, read_save, write_save, 0, 0},
    {MNG_UINT_SEEK, init_seek, free_seek, read_seek, write_seek, 0, 0},
    {MNG_UINT_SHOW, init_show, free_show, read_show, write_show, 0, 0},
    {MNG_UINT_TERM, init_term, free_term, read_term, write_term, 0, 0},
    {MNG_UINT_bKGD, init_bkgd, free_bkgd, read_bkgd, write_bkgd, 0, 0},
    {MNG_UINT_cHRM, init_chrm, free_chrm, read_chrm, write_chrm, 0, 0},
    {MNG_UINT_eXPI, init_expi, free_expi, read_expi, write_expi, 0, 0},
    {MNG_UINT_fPRI, init_fpri, free_fpri, read_fpri, write_fpri, 0, 0},
    {MNG_UINT_gAMA, init_gama, free_gama, read_gama, write_gama, 0, 0},
    {MNG_UINT_hIST, init_hist, free_hist, read_hist, write_hist, 0, 0},
    {MNG_UINT_iCCP, init_iccp, free_iccp, read_iccp, write_iccp, 0, 0},
    {MNG_UINT_iTXt, init_itxt, free_itxt, read_itxt, write_itxt, 0, 0},
    {MNG_UINT_nEED, init_need, free_need, read_need, write_need, 0, 0},
/* TODO:     {MNG_UINT_oFFs, 0, 0, 0, 0, 0, 0},  */
/* TODO:     {MNG_UINT_pCAL, 0, 0, 0, 0, 0, 0},  */
    {MNG_UINT_pHYg, init_phyg, free_phyg, read_phyg, write_phyg, 0, 0},
    {MNG_UINT_pHYs, init_phys, free_phys, read_phys, write_phys, 0, 0},
    {MNG_UINT_sBIT, init_sbit, free_sbit, read_sbit, write_sbit, 0, 0},
/* TODO:     {MNG_UINT_sCAL, 0, 0, 0, 0, 0, 0},  */
    {MNG_UINT_sPLT, init_splt, free_splt, read_splt, write_splt, 0, 0},
    {MNG_UINT_sRGB, init_srgb, free_srgb, read_srgb, write_srgb, 0, 0},
    {MNG_UINT_tEXt, init_text, free_text, read_text, write_text, 0, 0},
    {MNG_UINT_tIME, init_time, free_time, read_time, write_time, 0, 0},
    {MNG_UINT_tRNS, init_trns, free_trns, read_trns, write_trns, 0, 0},
    {MNG_UINT_zTXt, init_ztxt, free_ztxt, read_ztxt, write_ztxt, 0, 0},
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
                                       /* get the chunkname */
  iChunkname = (mng_chunkid)(mng_get_uint32 (pBuf));

  pBuf += sizeof (mng_chunkid);        /* adjust the buffer */
  iBuflen -= sizeof (mng_chunkid);
                                       /* determine max index of table */
  iTop = (sizeof (chunk_table) / sizeof (chunk_table [0])) - 1;

  /* binary search; with 52 chunks, worst-case is 7 comparisons */
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
    add_chunk (pData, pChunk);         /* do it */

#ifdef MNG_INCLUDE_JNG                 /* implicit EOF ? */
  if ((!pData->bHasMHDR) && (!pData->bHasIHDR) && (!pData->bHasJHDR))
#else
  if ((!pData->bHasMHDR) && (!pData->bHasIHDR))
#endif
    iRetcode = process_eof (pData);    /* then do some EOF processing */

  if (iRetcode)                        /* on error bail out */
    return iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_RAW_CHUNK, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode read_chunk (mng_datap  pData)
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
      {
        pData->pCurraniobj = ((mng_object_headerp)pData->pCurraniobj)->pNext;
                                       /* TERM processing to be done ? */
        if ((!pData->pCurraniobj) && (pData->bHasTERM) && (!pData->bHasMHDR))
          iRetcode = process_display_mend (pData);
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
        case 1 : { iRetcode = process_display_fram2 (pData); break; }
        case 2 : { iRetcode = process_display_ihdr  (pData); break; }
        case 3 : ;                     /* same as 4 !!! */
        case 4 : { iRetcode = process_display_show  (pData); break; }
        case 5 : { iRetcode = process_display_clon2 (pData); break; }
#ifdef MNG_INCLUDE_JNG
        case 7 : { iRetcode = process_display_jhdr  (pData); break; }
#endif
        case 6 : ;                     /* same as 8 !!! */
        case 8 : { iRetcode = process_display_iend  (pData); break; }
        case 9 : { iRetcode = process_display_magn2 (pData); break; }
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
  {                                    /* freezing in progress ? */
    if ((pData->bFreezing) && (pData->iSuspendpoint == 0))
      pData->bRunning = MNG_FALSE;     /* then this is the right moment to do it */

    if (pData->iSuspendpoint <= 2)
    {
      iBuflen  = sizeof (mng_uint32);  /* read length */
      iRetcode = read_databuffer (pData, pBuf, iBuflen, &iRead);

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
          iRetcode = read_databuffer (pData, pBuf, iBuflen, &iRead);

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
              iCrc = crc (pData, pBuf, iL);
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
          if (!pData->iSuspendpoint)   /* create additional large buffer ? */
          {                            /* again reserve space for the last zero-byte */
            pData->iLargebufsize = iBuflen + 1;
            MNG_ALLOC (pData, pData->pLargebuf, pData->iLargebufsize)
          }

          iRetcode = read_databuffer (pData, pData->pLargebuf, iBuflen, &iRead);

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
              iCrc = crc (pData, pData->pLargebuf, iL);
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
        iRetcode = process_eof (pData);

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
    iRetcode = display_progressive_refresh (pData, 1);

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

mng_retcode read_graphic (mng_datap pData)
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

    iRetcode = read_databuffer (pData, pData->pReadbuf, iBuflen, &iRead);

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
    {
      iRetcode = read_chunk (pData);   /* process a chunk */

      if (iRetcode)                    /* on error bail out */
        return iRetcode;
    }
#ifdef MNG_SUPPORT_DISPLAY             /* until EOF or a break-request */
    while (((!pData->bEOF) || (pData->pCurraniobj)) && (!pData->bSuspended) &&
           (!pData->bTimerset) && (!pData->bSectionwait));
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

