/* ************************************************************************** */
/* *             For conditions of distribution and use,                    * */
/* *                see copyright notice in libmng.h                        * */
/* ************************************************************************** */
/* *                                                                        * */
/* * project   : libmng                                                     * */
/* * file      : libmng_chunk_io.c         copyright (c) 2000 G.Juyn        * */
/* * version   : 1.0.2                                                      * */
/* *                                                                        * */
/* * purpose   : Chunk I/O routines (implementation)                        * */
/* *                                                                        * */
/* * author    : G.Juyn                                                     * */
/* * web       : http://www.3-t.com                                         * */
/* * email     : mailto:info@3-t.com                                        * */
/* *                                                                        * */
/* * comment   : implementation of chunk input/output routines              * */
/* *                                                                        * */
/* * changes   : 0.5.1 - 05/01/2000 - G.Juyn                                * */
/* *             - cleaned up left-over teststuff in the BACK chunk routine * */
/* *             0.5.1 - 05/04/2000 - G.Juyn                                * */
/* *             - changed CRC initialization to use dynamic structure      * */
/* *               (wasn't thread-safe the old way !)                       * */
/* *             0.5.1 - 05/06/2000 - G.Juyn                                * */
/* *             - filled in many missing sequence&length checks            * */
/* *             - filled in many missing chunk-store snippets              * */
/* *             0.5.1 - 05/08/2000 - G.Juyn                                * */
/* *             - added checks for running animations                      * */
/* *             - filled some write routines                               * */
/* *             - changed strict-ANSI stuff                                * */
/* *             0.5.1 - 05/10/2000 - G.Juyn                                * */
/* *             - filled some more write routines                          * */
/* *             0.5.1 - 05/11/2000 - G.Juyn                                * */
/* *             - filled remaining write routines                          * */
/* *             - fixed read_pplt with regard to deltatype                 * */
/* *             - added callback error-reporting support                   * */
/* *             - added pre-draft48 support (short MHDR, frame_mode, LOOP) * */
/* *             0.5.1 - 05/12/2000 - G.Juyn                                * */
/* *             - changed trace to macro for callback error-reporting      * */
/* *             - fixed chunk-storage bit in several routines              * */
/* *             0.5.1 - 05/13/2000 - G.Juyn                                * */
/* *             - added eMNGma hack (will be removed in 1.0.0 !!!)         * */
/* *             - added TERM animation object pointer (easier reference)   * */
/* *             - supplemented the SAVE & SEEK display processing          * */
/* *                                                                        * */
/* *             0.5.2 - 05/18/2000 - G.Juyn                                * */
/* *             - B004 - fixed problem with MNG_SUPPORT_WRITE not defined  * */
/* *               also for MNG_SUPPORT_WRITE without MNG_INCLUDE_JNG       * */
/* *             0.5.2 - 05/19/2000 - G.Juyn                                * */
/* *             - cleaned up some code regarding mixed support             * */
/* *             0.5.2 - 05/20/2000 - G.Juyn                                * */
/* *             - implemented JNG support                                  * */
/* *             0.5.2 - 05/24/2000 - G.Juyn                                * */
/* *             - added support for global color-chunks in animation       * */
/* *             - added support for global PLTE,tRNS,bKGD in animation     * */
/* *             - added support for SAVE & SEEK in animation               * */
/* *             0.5.2 - 05/29/2000 - G.Juyn                                * */
/* *             - changed ani_create calls not returning object pointer    * */
/* *             - create ani objects always (not just inside TERM/LOOP)    * */
/* *             0.5.2 - 05/30/2000 - G.Juyn                                * */
/* *             - added support for delta-image processing                 * */
/* *             0.5.2 - 05/31/2000 - G.Juyn                                * */
/* *             - fixed up punctuation (contributed by Tim Rowley)         * */
/* *             0.5.2 - 06/02/2000 - G.Juyn                                * */
/* *             - changed SWAP_ENDIAN to BIGENDIAN_SUPPORTED               * */
/* *             0.5.2 - 06/03/2000 - G.Juyn                                * */
/* *             - fixed makeup for Linux gcc compile                       * */
/* *                                                                        * */
/* *             0.5.3 - 06/12/2000 - G.Juyn                                * */
/* *             - added processing of color-info on delta-image            * */
/* *             0.5.3 - 06/13/2000 - G.Juyn                                * */
/* *             - fixed handling of empty SAVE chunk                       * */
/* *             0.5.3 - 06/17/2000 - G.Juyn                                * */
/* *             - changed to support delta-images                          * */
/* *             - added extra checks for delta-images                      * */
/* *             0.5.3 - 06/20/2000 - G.Juyn                                * */
/* *             - fixed possible trouble if IEND display-process got       * */
/* *               broken up                                                * */
/* *             0.5.3 - 06/21/2000 - G.Juyn                                * */
/* *             - added processing of PLTE & tRNS for delta-images         * */
/* *             - added administration of imagelevel parameter             * */
/* *             0.5.3 - 06/22/2000 - G.Juyn                                * */
/* *             - implemented support for PPLT chunk                       * */
/* *             0.5.3 - 06/26/2000 - G.Juyn                                * */
/* *             - added precaution against faulty iCCP chunks from PS      * */
/* *             0.5.3 - 06/29/2000 - G.Juyn                                * */
/* *             - fixed some 64-bit warnings                               * */
/* *                                                                        * */
/* *             0.9.1 - 07/14/2000 - G.Juyn                                * */
/* *             - changed pre-draft48 frame_mode=3 to frame_mode=1         * */
/* *             0.9.1 - 07/16/2000 - G.Juyn                                * */
/* *             - fixed storage of images during mng_read()                * */
/* *             - fixed support for mng_display() after mng_read()         * */
/* *             0.9.1 - 07/19/2000 - G.Juyn                                * */
/* *             - fixed several chunk-writing routines                     * */
/* *             0.9.1 - 07/24/2000 - G.Juyn                                * */
/* *             - fixed reading of still-images                            * */
/* *                                                                        * */
/* *             0.9.2 - 08/05/2000 - G.Juyn                                * */
/* *             - changed file-prefixes                                    * */
/* *                                                                        * */
/* *             0.9.3 - 08/07/2000 - G.Juyn                                * */
/* *             - B111300 - fixup for improved portability                 * */
/* *             0.9.3 - 08/08/2000 - G.Juyn                                * */
/* *             - fixed compiler-warnings from Mozilla                     * */
/* *             0.9.3 - 08/09/2000 - G.Juyn                                * */
/* *             - added check for simplicity-bits in MHDR                  * */
/* *             0.9.3 - 08/12/2000 - G.Juyn                                * */
/* *             - fixed check for simplicity-bits in MHDR (JNG)            * */
/* *             0.9.3 - 08/12/2000 - G.Juyn                                * */
/* *             - added workaround for faulty PhotoShop iCCP chunk         * */
/* *             0.9.3 - 08/22/2000 - G.Juyn                                * */
/* *             - fixed write-code for zTXt & iTXt                         * */
/* *             - fixed read-code for iTXt                                 * */
/* *             0.9.3 - 08/26/2000 - G.Juyn                                * */
/* *             - added MAGN chunk                                         * */
/* *             0.9.3 - 09/07/2000 - G.Juyn                                * */
/* *             - added support for new filter_types                       * */
/* *             0.9.3 - 09/10/2000 - G.Juyn                                * */
/* *             - fixed DEFI behavior                                      * */
/* *             0.9.3 - 10/02/2000 - G.Juyn                                * */
/* *             - fixed simplicity-check in compliance with draft 81/0.98a * */
/* *             0.9.3 - 10/10/2000 - G.Juyn                                * */
/* *             - added support for alpha-depth prediction                 * */
/* *             0.9.3 - 10/11/2000 - G.Juyn                                * */
/* *             - added support for nEED                                   * */
/* *             0.9.3 - 10/16/2000 - G.Juyn                                * */
/* *             - added support for JDAA                                   * */
/* *             0.9.3 - 10/17/2000 - G.Juyn                                * */
/* *             - fixed support for MAGN                                   * */
/* *             - implemented nEED "xxxx" (where "xxxx" is a chunkid)      * */
/* *             - added callback to process non-critical unknown chunks    * */
/* *             - fixed support for bKGD                                   * */
/* *             0.9.3 - 10/23/2000 - G.Juyn                                * */
/* *             - fixed bug in empty PLTE handling                         * */
/* *                                                                        * */
/* *             0.9.4 - 11/20/2000 - G.Juyn                                * */
/* *             - changed IHDR filter_method check for PNGs                * */
/* *             0.9.4 -  1/18/2001 - G.Juyn                                * */
/* *             - added errorchecking for MAGN methods                     * */
/* *             - removed test filter-methods 1 & 65                       * */
/* *                                                                        * */
/* *             0.9.5 -  1/25/2001 - G.Juyn                                * */
/* *             - fixed some small compiler warnings (thanks Nikki)        * */
/* *                                                                        * */
/* *             1.0.2 - 05/05/2000 - G.Juyn                                * */
/* *             - B421427 - writes wrong format in bKGD and tRNS           * */
/* *             1.0.2 - 06/20/2000 - G.Juyn                                * */
/* *             - B434583 - compiler-warning if MNG_STORE_CHUNKS undefined * */
/* *                                                                        * */
/* ************************************************************************** */

#include "libmng.h"
#include "libmng_data.h"
#include "libmng_error.h"
#include "libmng_trace.h"
#ifdef __BORLANDC__
#pragma hdrstop
#endif
#include "libmng_objects.h"
#include "libmng_object_prc.h"
#include "libmng_chunks.h"
#ifdef MNG_CHECK_BAD_ICCP
#include "libmng_chunk_prc.h"
#endif
#include "libmng_memory.h"
#include "libmng_display.h"
#include "libmng_zlib.h"
#include "libmng_pixels.h"
#include "libmng_chunk_io.h"

#if defined(__BORLANDC__) && defined(MNG_STRICT_ANSI)
#pragma option -A                      /* force ANSI-C */
#endif

/* ************************************************************************** */
/* *                                                                        * */
/* * CRC - Cyclic Redundancy Check                                          * */
/* *                                                                        * */
/* * The code below is taken directly from the sample provided with the     * */
/* * PNG specification.                                                     * */
/* * (it is only adapted to the library's internal data-definitions)        * */
/* *                                                                        * */
/* ************************************************************************** */

/* Make the table for a fast CRC. */
void make_crc_table (mng_datap pData)
{
  mng_uint32 iC;
  mng_int32  iN, iK;

  for (iN = 0; iN < 256; iN++)
  {
    iC = (mng_uint32) iN;

    for (iK = 0; iK < 8; iK++)
    {
      if (iC & 1)
        iC = 0xedb88320U ^ (iC >> 1);
      else
        iC = iC >> 1;
    }

    pData->aCRCtable [iN] = iC;
  }

  pData->bCRCcomputed = MNG_TRUE;
}

/* Update a running CRC with the bytes buf[0..len-1]--the CRC
   should be initialized to all 1's, and the transmitted value
   is the 1's complement of the final running CRC (see the
   crc() routine below). */

mng_uint32 update_crc (mng_datap  pData,
                       mng_uint32 iCrc,
                       mng_uint8p pBuf,
                       mng_int32  iLen)
{
  mng_uint32 iC = iCrc;
  mng_int32 iN;

  if (!pData->bCRCcomputed)
    make_crc_table (pData);

  for (iN = 0; iN < iLen; iN++)
    iC = pData->aCRCtable [(iC ^ pBuf [iN]) & 0xff] ^ (iC >> 8);

  return iC;
}

/* Return the CRC of the bytes buf[0..len-1]. */
mng_uint32 crc (mng_datap  pData,
                mng_uint8p pBuf,
                mng_int32  iLen)
{
  return update_crc (pData, 0xffffffffU, pBuf, iLen) ^ 0xffffffffU;
}

/* ************************************************************************** */
/* *                                                                        * */
/* * Routines for swapping byte-order from and to graphic files             * */
/* * (This code is adapted from the libpng package)                         * */
/* *                                                                        * */
/* ************************************************************************** */

#ifndef MNG_BIGENDIAN_SUPPORTED

/* ************************************************************************** */

mng_uint32 mng_get_uint32 (mng_uint8p pBuf)
{
   mng_uint32 i = ((mng_uint32)(*pBuf)       << 24) +
                  ((mng_uint32)(*(pBuf + 1)) << 16) +
                  ((mng_uint32)(*(pBuf + 2)) <<  8) +
                   (mng_uint32)(*(pBuf + 3));
   return (i);
}

/* ************************************************************************** */

mng_int32 mng_get_int32 (mng_uint8p pBuf)
{
   mng_int32 i = ((mng_int32)(*pBuf)       << 24) +
                 ((mng_int32)(*(pBuf + 1)) << 16) +
                 ((mng_int32)(*(pBuf + 2)) <<  8) +
                  (mng_int32)(*(pBuf + 3));
   return (i);
}

/* ************************************************************************** */

mng_uint16 mng_get_uint16 (mng_uint8p pBuf)
{
   mng_uint16 i = (mng_uint16)(((mng_uint16)(*pBuf) << 8) +
                                (mng_uint16)(*(pBuf + 1)));
   return (i);
}

/* ************************************************************************** */

void mng_put_uint32 (mng_uint8p pBuf,
                     mng_uint32 i)
{
   *pBuf     = (mng_uint8)((i >> 24) & 0xff);
   *(pBuf+1) = (mng_uint8)((i >> 16) & 0xff);
   *(pBuf+2) = (mng_uint8)((i >> 8) & 0xff);
   *(pBuf+3) = (mng_uint8)(i & 0xff);
}

/* ************************************************************************** */

void mng_put_int32 (mng_uint8p pBuf,
                    mng_int32  i)
{
   *pBuf     = (mng_uint8)((i >> 24) & 0xff);
   *(pBuf+1) = (mng_uint8)((i >> 16) & 0xff);
   *(pBuf+2) = (mng_uint8)((i >> 8) & 0xff);
   *(pBuf+3) = (mng_uint8)(i & 0xff);
}

/* ************************************************************************** */

void mng_put_uint16 (mng_uint8p pBuf,
                     mng_uint16 i)
{
   *pBuf     = (mng_uint8)((i >> 8) & 0xff);
   *(pBuf+1) = (mng_uint8)(i & 0xff);
}

/* ************************************************************************** */

#endif /* !MNG_BIGENDIAN_SUPPORTED */

/* ************************************************************************** */
/* *                                                                        * */
/* * Helper routines to simplify chunk-data extraction                      * */
/* *                                                                        * */
/* ************************************************************************** */

#ifdef MNG_INCLUDE_READ_PROCS

/* ************************************************************************** */

mng_uint8p find_null (mng_uint8p pIn)
{
  mng_uint8p pOut = pIn;

  while (*pOut)                        /* the read_graphic routine has made sure there's */
    pOut++;                            /* always at least 1 zero-byte in the buffer */

  return pOut;
}

/* ************************************************************************** */

mng_retcode inflate_buffer (mng_datap  pData,
                            mng_uint8p pInbuf,
                            mng_uint32 iInsize,
                            mng_uint8p *pOutbuf,
                            mng_uint32 *iOutsize,
                            mng_uint32 *iRealsize)
{
  mng_retcode iRetcode = MNG_NOERROR;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INFLATE_BUFFER, MNG_LC_START)
#endif

  if (iInsize)                         /* anything to do ? */
  {
    *iOutsize = iInsize * 3;           /* estimate uncompressed size */
                                       /* and allocate a temporary buffer */
    MNG_ALLOC (pData, *pOutbuf, *iOutsize)

    do
    {
      mngzlib_inflateinit (pData);     /* initialize zlib */
                                       /* let zlib know where to store the output */
      pData->sZlib.next_out  = *pOutbuf;
                                       /* "size - 1" so we've got space for the
                                          zero-termination of a possible string */
      pData->sZlib.avail_out = *iOutsize - 1;
                                       /* ok; let's inflate... */
      iRetcode = mngzlib_inflatedata (pData, iInsize, pInbuf);
                                       /* determine actual output size */
      *iRealsize = (mng_uint32)pData->sZlib.total_out;

      mngzlib_inflatefree (pData);     /* zlib's done */

      if (iRetcode == MNG_BUFOVERFLOW) /* not enough space ? */
      {                                /* then get some more */
        MNG_FREEX (pData, *pOutbuf, *iOutsize)
        *iOutsize = *iOutsize + iInsize;
        MNG_ALLOC (pData, *pOutbuf, *iOutsize)
      }
    }                                  /* repeat if we didn't have enough space */
    while ((iRetcode == MNG_BUFOVERFLOW) &&
           (*iOutsize < 20 * iInsize));

    if (!iRetcode)                     /* if oke ? */
      *((*pOutbuf) + *iRealsize) = 0;  /* then put terminator zero */

  }
  else
  {
    *pOutbuf   = 0;                    /* nothing to do; then there's no output */
    *iOutsize  = 0;
    *iRealsize = 0;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INFLATE_BUFFER, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */

#endif /* MNG_INCLUDE_READ_PROCS */

/* ************************************************************************** */
/* *                                                                        * */
/* * Helper routines to simplify chunk writing                              * */
/* *                                                                        * */
/* ************************************************************************** */
/* B004 */
#ifdef MNG_INCLUDE_WRITE_PROCS
/* B004 */
/* ************************************************************************** */

mng_retcode deflate_buffer (mng_datap  pData,
                            mng_uint8p pInbuf,
                            mng_uint32 iInsize,
                            mng_uint8p *pOutbuf,
                            mng_uint32 *iOutsize,
                            mng_uint32 *iRealsize)
{
  mng_retcode iRetcode = MNG_NOERROR;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DEFLATE_BUFFER, MNG_LC_START)
#endif

  if (iInsize)                         /* anything to do ? */
  {
    *iOutsize = (iInsize * 5) >> 2;    /* estimate compressed size */
                                       /* and allocate a temporary buffer */
    MNG_ALLOC (pData, *pOutbuf, *iOutsize)

    do
    {
      mngzlib_deflateinit (pData);     /* initialize zlib */
                                       /* let zlib know where to store the output */
      pData->sZlib.next_out  = *pOutbuf;
      pData->sZlib.avail_out = *iOutsize;
                                       /* ok; let's deflate... */
      iRetcode = mngzlib_deflatedata (pData, iInsize, pInbuf);
                                       /* determine actual output size */
      *iRealsize = pData->sZlib.total_out;

      mngzlib_deflatefree (pData);     /* zlib's done */

      if (iRetcode == MNG_BUFOVERFLOW) /* not enough space ? */
      {                                /* then get some more */
        MNG_FREEX (pData, *pOutbuf, *iOutsize)
        *iOutsize = *iOutsize + (iInsize >> 1);
        MNG_ALLOC (pData, *pOutbuf, *iOutsize)
      }
    }                                  /* repeat if we didn't have enough space */
    while (iRetcode == MNG_BUFOVERFLOW);
  }
  else
  {
    *pOutbuf   = 0;                    /* nothing to do; then there's no output */
    *iOutsize  = 0;
    *iRealsize = 0;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DEFLATE_BUFFER, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */

mng_retcode write_raw_chunk (mng_datap   pData,
                             mng_chunkid iChunkname,
                             mng_uint32  iRawlen,
                             mng_uint8p  pRawdata)
{
  mng_uint32 iCrc;
  mng_uint32 iWritten;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_RAW_CHUNK, MNG_LC_START)
#endif
                                       /* temporary buffer ? */
  if ((pRawdata != 0) && (pRawdata != pData->pWritebuf+8))
  {                                    /* store length & chunktype in default buffer */
    mng_put_uint32 (pData->pWritebuf,   iRawlen);
    mng_put_uint32 (pData->pWritebuf+4, (mng_uint32)iChunkname);
                                       /* calculate the crc */
    iCrc = update_crc (pData, 0xffffffffL, pData->pWritebuf+4, 4);
    iCrc = update_crc (pData, iCrc, pRawdata, iRawlen) ^ 0xffffffffL;
                                       /* store crc in default buffer */
    mng_put_uint32 (pData->pWritebuf+8, iCrc);
                                       /* write the length & chunktype */
    if (!pData->fWritedata ((mng_handle)pData, pData->pWritebuf, 8, &iWritten))
      MNG_ERROR (pData, MNG_APPIOERROR)

    if (iWritten != 8)                 /* disk full ? */
      MNG_ERROR (pData, MNG_OUTPUTERROR)
                                       /* write the temporary buffer */
    if (!pData->fWritedata ((mng_handle)pData, pRawdata, iRawlen, &iWritten))
      MNG_ERROR (pData, MNG_APPIOERROR)

    if (iWritten != iRawlen)           /* disk full ? */
      MNG_ERROR (pData, MNG_OUTPUTERROR)

                                       /* write the crc */
    if (!pData->fWritedata ((mng_handle)pData, pData->pWritebuf+8, 4, &iWritten))
      MNG_ERROR (pData, MNG_APPIOERROR)

    if (iWritten != 4)                 /* disk full ? */
      MNG_ERROR (pData, MNG_OUTPUTERROR)

  }
  else
  {                                    /* prefix with length & chunktype */
    mng_put_uint32 (pData->pWritebuf,   iRawlen);
    mng_put_uint32 (pData->pWritebuf+4, (mng_uint32)iChunkname);
                                       /* calculate the crc */
    iCrc = crc (pData, pData->pWritebuf+4, iRawlen + 4);
                                       /* add it to the buffer */
    mng_put_uint32 (pData->pWritebuf + iRawlen + 8, iCrc);
                                       /* write it in a single pass */
    if (!pData->fWritedata ((mng_handle)pData, pData->pWritebuf, iRawlen + 12, &iWritten))
      MNG_ERROR (pData, MNG_APPIOERROR)

    if (iWritten != iRawlen + 12)      /* disk full ? */
      MNG_ERROR (pData, MNG_OUTPUTERROR)

  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_RAW_CHUNK, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */
/* B004 */
#endif /* MNG_INCLUDE_WRITE_PROCS */
/* B004 */
/* ************************************************************************** */
/* *                                                                        * */
/* * chunk read functions                                                   * */
/* *                                                                        * */
/* ************************************************************************** */

#ifdef MNG_INCLUDE_READ_PROCS

/* ************************************************************************** */

READ_CHUNK (read_ihdr)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_IHDR, MNG_LC_START)
#endif

  if (iRawlen != 13)                   /* length oke ? */
    MNG_ERROR (pData, MNG_INVALIDLENGTH)
                                       /* only allowed inside PNG or MNG */
  if ((pData->eSigtype != mng_it_png) && (pData->eSigtype != mng_it_mng))
    MNG_ERROR (pData, MNG_CHUNKNOTALLOWED)
                                       /* sequence checks */
  if ((pData->eSigtype == mng_it_png) && (pData->iChunkseq > 1))
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

#ifdef MNG_INCLUDE_JNG
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasIDAT) || (pData->bHasJHDR))
#else
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasIDAT))
#endif
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

  pData->bHasIHDR      = MNG_TRUE;     /* indicate IHDR is present */
                                       /* and store interesting fields */
  if ((!pData->bHasDHDR) || (pData->iDeltatype == MNG_DELTATYPE_NOCHANGE))
  {
    pData->iDatawidth  = mng_get_uint32 (pRawdata);
    pData->iDataheight = mng_get_uint32 (pRawdata+4);
  }
  
  pData->iBitdepth     = *(pRawdata+8);
  pData->iColortype    = *(pRawdata+9);
  pData->iCompression  = *(pRawdata+10);
  pData->iFilter       = *(pRawdata+11);
  pData->iInterlace    = *(pRawdata+12);

  if ((pData->iBitdepth !=  1) &&      /* parameter validity checks */
      (pData->iBitdepth !=  2) &&
      (pData->iBitdepth !=  4) &&
      (pData->iBitdepth !=  8) &&
      (pData->iBitdepth != 16)    )
    MNG_ERROR (pData, MNG_INVALIDBITDEPTH)

  if ((pData->iColortype != MNG_COLORTYPE_GRAY   ) &&
      (pData->iColortype != MNG_COLORTYPE_RGB    ) &&
      (pData->iColortype != MNG_COLORTYPE_INDEXED) &&
      (pData->iColortype != MNG_COLORTYPE_GRAYA  ) &&
      (pData->iColortype != MNG_COLORTYPE_RGBA   )    )
    MNG_ERROR (pData, MNG_INVALIDCOLORTYPE)

  if ((pData->iColortype == MNG_COLORTYPE_INDEXED) && (pData->iBitdepth > 8))
    MNG_ERROR (pData, MNG_INVALIDBITDEPTH)

  if (((pData->iColortype == MNG_COLORTYPE_RGB    ) ||
       (pData->iColortype == MNG_COLORTYPE_GRAYA  ) ||
       (pData->iColortype == MNG_COLORTYPE_RGBA   )    ) &&
      (pData->iBitdepth < 8                            )    )
    MNG_ERROR (pData, MNG_INVALIDBITDEPTH)

  if (pData->iCompression != MNG_COMPRESSION_DEFLATE)
    MNG_ERROR (pData, MNG_INVALIDCOMPRESS)

  if ((pData->eSigtype == mng_it_png) && (pData->iFilter))
    MNG_ERROR (pData, MNG_INVALIDFILTER)
  else
  if (pData->iFilter & (~MNG_FILTER_DIFFERING))
    MNG_ERROR (pData, MNG_INVALIDFILTER)

  if ((pData->iInterlace != MNG_INTERLACE_NONE ) &&
      (pData->iInterlace != MNG_INTERLACE_ADAM7)    )
    MNG_ERROR (pData, MNG_INVALIDINTERLACE)

  if (pData->bHasDHDR)                 /* check the colortype for delta-images ! */
  {
    mng_imagedatap pBuf = ((mng_imagep)pData->pObjzero)->pImgbuf;

    if (pData->iColortype != pBuf->iColortype)
    {
      if ( ( (pData->iColortype != MNG_COLORTYPE_INDEXED) ||
             (pBuf->iColortype  == MNG_COLORTYPE_GRAY   )    ) &&
           ( (pData->iColortype != MNG_COLORTYPE_GRAY   ) ||
             (pBuf->iColortype  == MNG_COLORTYPE_INDEXED)    )    )
        MNG_ERROR (pData, MNG_INVALIDCOLORTYPE)
    }
  }

  if (!pData->bHasheader)              /* first chunk ? */
  {
    pData->bHasheader = MNG_TRUE;      /* we've got a header */
    pData->eImagetype = mng_it_png;    /* then this must be a PNG */
    pData->iWidth     = pData->iDatawidth;
    pData->iHeight    = pData->iDataheight;
                                       /* predict alpha-depth ! */
    if ((pData->iColortype == MNG_COLORTYPE_GRAYA  ) ||
        (pData->iColortype == MNG_COLORTYPE_RGBA   )    )
      pData->iAlphadepth = pData->iBitdepth;
    else
    if (pData->iColortype == MNG_COLORTYPE_INDEXED)
      pData->iAlphadepth = 8;          /* worst case scenario */
    else
      pData->iAlphadepth = 0;
                                       /* fits on maximum canvas ? */
    if ((pData->iWidth > pData->iMaxwidth) || (pData->iHeight > pData->iMaxheight))
      MNG_WARNING (pData, MNG_IMAGETOOLARGE)

    if (pData->fProcessheader)         /* inform the app ? */
      if (!pData->fProcessheader (((mng_handle)pData), pData->iWidth, pData->iHeight))
        MNG_ERROR (pData, MNG_APPMISCERROR)
  }

  if (!pData->bHasDHDR)
    pData->iImagelevel++;              /* one level deeper */

#ifdef MNG_SUPPORT_DISPLAY
  {
    mng_retcode iRetcode = process_display_ihdr (pData);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
  }
#endif /* MNG_SUPPORT_DISPLAY */

#ifdef MNG_STORE_CHUNKS
  if (pData->bStorechunks)
  {                                    /* initialize storage */
    mng_retcode iRetcode = ((mng_chunk_headerp)pHeader)->fCreate (pData, pHeader, ppChunk);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
                                       /* fill the fields */
    ((mng_ihdrp)*ppChunk)->iWidth       = mng_get_uint32 (pRawdata);
    ((mng_ihdrp)*ppChunk)->iHeight      = mng_get_uint32 (pRawdata+4);
    ((mng_ihdrp)*ppChunk)->iBitdepth    = pData->iBitdepth;
    ((mng_ihdrp)*ppChunk)->iColortype   = pData->iColortype;
    ((mng_ihdrp)*ppChunk)->iCompression = pData->iCompression;
    ((mng_ihdrp)*ppChunk)->iFilter      = pData->iFilter;
    ((mng_ihdrp)*ppChunk)->iInterlace   = pData->iInterlace;
  }
#endif /* MNG_STORE_CHUNKS */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_IHDR, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* done */
}

/* ************************************************************************** */

READ_CHUNK (read_plte)
{
#if defined(MNG_SUPPORT_DISPLAY) || defined(MNG_STORE_CHUNKS)
  mng_uint32  iX;
  mng_uint8p  pRawdata2;
#endif
#ifdef MNG_SUPPORT_DISPLAY
  mng_uint32  iRawlen2;
#endif

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_PLTE, MNG_LC_START)
#endif
                                       /* sequence checks */
  if ((!pData->bHasMHDR) && (!pData->bHasIHDR) &&
      (!pData->bHasBASI) && (!pData->bHasDHDR)    )
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

#ifdef MNG_INCLUDE_JNG
  if ((pData->bHasIDAT) || (pData->bHasJHDR))
#else
  if (pData->bHasIDAT)
#endif  
    MNG_ERROR (pData, MNG_SEQUENCEERROR)
                                       /* multiple PLTE only inside BASI */
  if ((pData->bHasPLTE) && (!pData->bHasBASI))
    MNG_ERROR (pData, MNG_MULTIPLEERROR)
                                       /* length must be multiple of 3 */
  if (((iRawlen % 3) != 0) || (iRawlen > 768))
    MNG_ERROR (pData, MNG_INVALIDLENGTH)

  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR))
  {                                    /* only allowed for indexed-color or
                                          rgb(a)-color! */
    if ((pData->iColortype != 2) && (pData->iColortype != 3) && (pData->iColortype != 6))
      MNG_ERROR (pData, MNG_CHUNKNOTALLOWED)
                                       /* empty only allowed if global present */
    if ((iRawlen == 0) && (!pData->bHasglobalPLTE))
        MNG_ERROR (pData, MNG_CANNOTBEEMPTY)
  }
  else
  {
    if (iRawlen == 0)                  /* cannot be empty as global! */
      MNG_ERROR (pData, MNG_CANNOTBEEMPTY)
  }

  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR))
    pData->bHasPLTE = MNG_TRUE;        /* got it! */
  else
    pData->bHasglobalPLTE = MNG_TRUE;

  pData->iPLTEcount = iRawlen / 3;  

#ifdef MNG_SUPPORT_DISPLAY
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR))
  {
    mng_imagep     pImage;
    mng_imagedatap pBuf;

    if (pData->bHasDHDR)               /* processing delta-image ? */
    {                                  /* store in object 0 !!! */
      pImage           = (mng_imagep)pData->pObjzero;
      pBuf             = pImage->pImgbuf;
      pBuf->bHasPLTE   = MNG_TRUE;     /* it's definitely got a PLTE now */
      pBuf->iPLTEcount = iRawlen / 3;  /* this is the exact length */
      pRawdata2        = pRawdata;     /* copy the entries */

      for (iX = 0; iX < iRawlen / 3; iX++)
      {
        pBuf->aPLTEentries[iX].iRed   = *pRawdata2;
        pBuf->aPLTEentries[iX].iGreen = *(pRawdata2+1);
        pBuf->aPLTEentries[iX].iBlue  = *(pRawdata2+2);

        pRawdata2 += 3;
      }
    }
    else
    {                                  /* get the current object */
      pImage = (mng_imagep)pData->pCurrentobj;

      if (!pImage)                     /* no object then dump it in obj 0 */
        pImage = (mng_imagep)pData->pObjzero;

      pBuf = pImage->pImgbuf;          /* address the object buffer */
      pBuf->bHasPLTE = MNG_TRUE;       /* and tell it it's got a PLTE now */

      if (!iRawlen)                    /* if empty, inherit from global */
      {
        pBuf->iPLTEcount = pData->iGlobalPLTEcount;
        MNG_COPY (pBuf->aPLTEentries, pData->aGlobalPLTEentries,
                  sizeof (pBuf->aPLTEentries))

        if (pData->bHasglobalTRNS)     /* also copy global tRNS ? */
        {                              /* indicate tRNS available */
          pBuf->bHasTRNS = MNG_TRUE;

          iRawlen2  = pData->iGlobalTRNSrawlen;
          pRawdata2 = (mng_uint8p)(pData->aGlobalTRNSrawdata);
                                       /* global length oke ? */
          if ((iRawlen2 == 0) || (iRawlen2 > pBuf->iPLTEcount))
            MNG_ERROR (pData, MNG_GLOBALLENGTHERR)
                                       /* copy it */
          pBuf->iTRNScount = iRawlen2;
          MNG_COPY (pBuf->aTRNSentries, pRawdata2, iRawlen2)
        }
      }
      else
      {                                /* store fields for future reference */
        pBuf->iPLTEcount = iRawlen / 3;
        pRawdata2        = pRawdata;

        for (iX = 0; iX < pBuf->iPLTEcount; iX++)
        {
          pBuf->aPLTEentries[iX].iRed   = *pRawdata2;
          pBuf->aPLTEentries[iX].iGreen = *(pRawdata2+1);
          pBuf->aPLTEentries[iX].iBlue  = *(pRawdata2+2);

          pRawdata2 += 3;
        }
      }
    }
  }
  else                                 /* store as global */
  {
    pData->iGlobalPLTEcount = iRawlen / 3;
    pRawdata2               = pRawdata;

    for (iX = 0; iX < pData->iGlobalPLTEcount; iX++)
    {
      pData->aGlobalPLTEentries[iX].iRed   = *pRawdata2;
      pData->aGlobalPLTEentries[iX].iGreen = *(pRawdata2+1);
      pData->aGlobalPLTEentries[iX].iBlue  = *(pRawdata2+2);

      pRawdata2 += 3;
    }

    {                                  /* create an animation object */
      mng_retcode iRetcode = create_ani_plte (pData, pData->iGlobalPLTEcount,
                                              pData->aGlobalPLTEentries);

      if (iRetcode)                    /* on error bail out */
        return iRetcode;
    }
  }
#endif /* MNG_SUPPORT_DISPLAY */

#ifdef MNG_STORE_CHUNKS
  if (pData->bStorechunks)
  {                                    /* initialize storage */
    mng_retcode iRetcode = ((mng_chunk_headerp)pHeader)->fCreate (pData, pHeader, ppChunk);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
                                       /* store the fields */
    ((mng_pltep)*ppChunk)->bEmpty      = (mng_bool)(iRawlen == 0);
    ((mng_pltep)*ppChunk)->iEntrycount = iRawlen / 3;
    pRawdata2                          = pRawdata;

    for (iX = 0; iX < ((mng_pltep)*ppChunk)->iEntrycount; iX++)
    {
      ((mng_pltep)*ppChunk)->aEntries[iX].iRed   = *pRawdata2;
      ((mng_pltep)*ppChunk)->aEntries[iX].iGreen = *(pRawdata2+1);
      ((mng_pltep)*ppChunk)->aEntries[iX].iBlue  = *(pRawdata2+2);

      pRawdata2 += 3;
    }
  }
#endif /* MNG_STORE_CHUNKS */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_PLTE, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* done */
}

/* ************************************************************************** */

READ_CHUNK (read_idat)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_IDAT, MNG_LC_START)
#endif

#ifdef MNG_INCLUDE_JNG                 /* sequence checks */
  if ((!pData->bHasIHDR) && (!pData->bHasBASI) && (!pData->bHasDHDR) && (!pData->bHasJHDR))
#else
  if ((!pData->bHasIHDR) && (!pData->bHasBASI) && (!pData->bHasDHDR))
#endif
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

#ifdef MNG_INCLUDE_JNG
  if ((pData->bHasJHDR) &&
      (pData->iJHDRalphacompression != MNG_COMPRESSION_DEFLATE))
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

  if (pData->bHasJSEP)
    MNG_ERROR (pData, MNG_SEQUENCEERROR)
#endif
                                       /* not allowed for for deltatype NO_CHANGE */
  if ((pData->bHasDHDR) && ((pData->iDeltatype == MNG_DELTATYPE_NOCHANGE)))
    MNG_ERROR (pData, MNG_CHUNKNOTALLOWED)
                                       /* can only be empty in BASI-block! */
  if ((iRawlen == 0) && (!pData->bHasBASI))
    MNG_ERROR (pData, MNG_INVALIDLENGTH)
                                       /* indexed-color requires PLTE */
  if ((pData->bHasIHDR) && (pData->iColortype == 3) && (!pData->bHasPLTE))
    MNG_ERROR (pData, MNG_PLTEMISSING)

  pData->bHasIDAT = MNG_TRUE;          /* got some IDAT now, don't we */

#ifdef MNG_SUPPORT_DISPLAY
  if (iRawlen)
  {                                    /* display processing */
    mng_retcode iRetcode = process_display_idat (pData, iRawlen, pRawdata);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
  }
#endif /* MNG_SUPPORT_DISPLAY */

#ifdef MNG_STORE_CHUNKS
  if (pData->bStorechunks)
  {                                    /* initialize storage */
    mng_retcode iRetcode = ((mng_chunk_headerp)pHeader)->fCreate (pData, pHeader, ppChunk);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
                                       /* store the fields */
    ((mng_idatp)*ppChunk)->bEmpty    = (mng_bool)(iRawlen == 0);
    ((mng_idatp)*ppChunk)->iDatasize = iRawlen;

    if (iRawlen != 0)                  /* is there any data ? */
    {
      MNG_ALLOC (pData, ((mng_idatp)*ppChunk)->pData, iRawlen)
      MNG_COPY  (((mng_idatp)*ppChunk)->pData, pRawdata, iRawlen)
    }
  }
#endif /* MNG_STORE_CHUNKS */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_IDAT, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* done */
}

/* ************************************************************************** */

READ_CHUNK (read_iend)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_IEND, MNG_LC_START)
#endif

  if (iRawlen > 0)                     /* must not contain data! */
    MNG_ERROR (pData, MNG_INVALIDLENGTH);

#ifdef MNG_INCLUDE_JNG                 /* sequence checks */
  if ((!pData->bHasIHDR) && (!pData->bHasBASI) && (!pData->bHasDHDR) && (!pData->bHasJHDR))
#else
  if ((!pData->bHasIHDR) && (!pData->bHasBASI) && (!pData->bHasDHDR))
#endif
    MNG_ERROR (pData, MNG_SEQUENCEERROR)
                                       /* IHDR-block requires IDAT */
  if ((pData->bHasIHDR) && (!pData->bHasIDAT))
    MNG_ERROR (pData, MNG_IDATMISSING)

  pData->iImagelevel--;                /* one level up */

#ifdef MNG_SUPPORT_DISPLAY
  {                                    /* create an animation object */
    mng_retcode iRetcode = create_ani_image (pData);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
                                       /* display processing */
    iRetcode = process_display_iend (pData);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
  }
#endif /* MNG_SUPPORT_DISPLAY */

#ifdef MNG_SUPPORT_DISPLAY
  if (!pData->bTimerset)               /* reset only if not broken !!! */
  {
#endif
                                       /* IEND signals the end for most ... */
    pData->bHasIHDR         = MNG_FALSE;
    pData->bHasBASI         = MNG_FALSE;
    pData->bHasDHDR         = MNG_FALSE;
#ifdef MNG_INCLUDE_JNG
    pData->bHasJHDR         = MNG_FALSE;
    pData->bHasJSEP         = MNG_FALSE;
    pData->bHasJDAA         = MNG_FALSE;
    pData->bHasJDAT         = MNG_FALSE;
#endif
    pData->bHasPLTE         = MNG_FALSE;
    pData->bHasTRNS         = MNG_FALSE;
    pData->bHasGAMA         = MNG_FALSE;
    pData->bHasCHRM         = MNG_FALSE;
    pData->bHasSRGB         = MNG_FALSE;
    pData->bHasICCP         = MNG_FALSE;
    pData->bHasBKGD         = MNG_FALSE;
    pData->bHasIDAT         = MNG_FALSE;
#ifdef MNG_SUPPORT_DISPLAY
  }
#endif  

#ifdef MNG_STORE_CHUNKS
  if (pData->bStorechunks)
  {                                    /* initialize storage */
    mng_retcode iRetcode = ((mng_chunk_headerp)pHeader)->fCreate (pData, pHeader, ppChunk);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
  }
#endif /* MNG_STORE_CHUNKS */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_IEND, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* done */
}

/* ************************************************************************** */

READ_CHUNK (read_trns)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_TRNS, MNG_LC_START)
#endif
                                       /* sequence checks */
  if ((!pData->bHasMHDR) && (!pData->bHasIHDR) &&
      (!pData->bHasBASI) && (!pData->bHasDHDR)    )
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

#ifdef MNG_INCLUDE_JNG
  if ((pData->bHasIDAT) || (pData->bHasJHDR))
#else
  if (pData->bHasIDAT)
#endif  
    MNG_ERROR (pData, MNG_SEQUENCEERROR)
                                       /* multiple tRNS only inside BASI */
  if ((pData->bHasTRNS) && (!pData->bHasBASI))
    MNG_ERROR (pData, MNG_MULTIPLEERROR)

  if (iRawlen > 256)                   /* it just can't be bigger than that! */
    MNG_ERROR (pData, MNG_INVALIDLENGTH)

  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR))
  {                                    /* not allowed with full alpha-channel */
    if ((pData->iColortype == 4) || (pData->iColortype == 6))
      MNG_ERROR (pData, MNG_CHUNKNOTALLOWED)

    if (iRawlen != 0)                  /* filled ? */
    {                                  /* length checks */
      if ((pData->iColortype == 0) && (iRawlen != 2))
        MNG_ERROR (pData, MNG_INVALIDLENGTH)

      if ((pData->iColortype == 2) && (iRawlen != 6))
        MNG_ERROR (pData, MNG_INVALIDLENGTH)

#ifdef MNG_SUPPORT_DISPLAY
      if (pData->iColortype == 3)
      {
        mng_imagep     pImage = (mng_imagep)pData->pCurrentobj;
        mng_imagedatap pBuf;

        if (!pImage)                   /* no object then check obj 0 */
          pImage = (mng_imagep)pData->pObjzero;

        pBuf = pImage->pImgbuf;        /* address object buffer */

        if ((iRawlen == 0) || (iRawlen > pBuf->iPLTEcount))
          MNG_ERROR (pData, MNG_INVALIDLENGTH)
      }
#endif
    }
    else                               /* if empty there must be global stuff! */
    {
      if (!pData->bHasglobalTRNS)
        MNG_ERROR (pData, MNG_CANNOTBEEMPTY)
    }
  }

  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR))
    pData->bHasTRNS = MNG_TRUE;        /* indicate tRNS available */
  else
    pData->bHasglobalTRNS = MNG_TRUE;

#ifdef MNG_SUPPORT_DISPLAY
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR))
  {
    mng_imagep     pImage;
    mng_imagedatap pBuf;
    mng_uint8p     pRawdata2;
    mng_uint32     iRawlen2;

    if (pData->bHasDHDR)               /* processing delta-image ? */
    {                                  /* store in object 0 !!! */
      pImage = (mng_imagep)pData->pObjzero;
      pBuf   = pImage->pImgbuf;        /* address object buffer */

      switch (pData->iColortype)       /* store fields for future reference */
      {
        case 0: {                      /* gray */
                  pBuf->iTRNSgray  = mng_get_uint16 (pRawdata);
                  pBuf->iTRNSred   = 0;
                  pBuf->iTRNSgreen = 0;
                  pBuf->iTRNSblue  = 0;
                  pBuf->iTRNScount = 0;
                  break;
                }
        case 2: {                      /* rgb */
                  pBuf->iTRNSgray  = 0;
                  pBuf->iTRNSred   = mng_get_uint16 (pRawdata);
                  pBuf->iTRNSgreen = mng_get_uint16 (pRawdata+2);
                  pBuf->iTRNSblue  = mng_get_uint16 (pRawdata+4);
                  pBuf->iTRNScount = 0;
                  break;
                }
        case 3: {                      /* indexed */
                  pBuf->iTRNSgray  = 0;
                  pBuf->iTRNSred   = 0;
                  pBuf->iTRNSgreen = 0;
                  pBuf->iTRNSblue  = 0;
                  pBuf->iTRNScount = iRawlen;
                  MNG_COPY (pBuf->aTRNSentries, pRawdata, iRawlen)
                  break;
                }
      }

      pBuf->bHasTRNS = MNG_TRUE;       /* tell it it's got a tRNS now */
    }
    else
    {                                  /* address current object */
      pImage = (mng_imagep)pData->pCurrentobj;

      if (!pImage)                     /* no object then dump it in obj 0 */
        pImage = (mng_imagep)pData->pObjzero;

      pBuf = pImage->pImgbuf;          /* address object buffer */
      pBuf->bHasTRNS = MNG_TRUE;       /* and tell it it's got a tRNS now */

      if (iRawlen == 0)                /* if empty, inherit from global */
      {
        iRawlen2  = pData->iGlobalTRNSrawlen;
        pRawdata2 = (mng_ptr)(pData->aGlobalTRNSrawdata);
                                         /* global length oke ? */
        if ((pData->iColortype == 0) && (iRawlen2 != 2))
          MNG_ERROR (pData, MNG_GLOBALLENGTHERR)

        if ((pData->iColortype == 2) && (iRawlen2 != 6))
          MNG_ERROR (pData, MNG_GLOBALLENGTHERR)

        if ((pData->iColortype == 3) && ((iRawlen2 == 0) || (iRawlen2 > pBuf->iPLTEcount)))
          MNG_ERROR (pData, MNG_GLOBALLENGTHERR)
      }
      else
      {
        iRawlen2  = iRawlen;
        pRawdata2 = pRawdata;
      }

      switch (pData->iColortype)        /* store fields for future reference */
      {
        case 0: {                      /* gray */
                  pBuf->iTRNSgray  = mng_get_uint16 (pRawdata2);
                  pBuf->iTRNSred   = 0;
                  pBuf->iTRNSgreen = 0;
                  pBuf->iTRNSblue  = 0;
                  pBuf->iTRNScount = 0;
                  break;
                }
        case 2: {                      /* rgb */
                  pBuf->iTRNSgray  = 0;
                  pBuf->iTRNSred   = mng_get_uint16 (pRawdata2);
                  pBuf->iTRNSgreen = mng_get_uint16 (pRawdata2+2);
                  pBuf->iTRNSblue  = mng_get_uint16 (pRawdata2+4);
                  pBuf->iTRNScount = 0;
                  break;
                }
        case 3: {                      /* indexed */
                  pBuf->iTRNSgray  = 0;
                  pBuf->iTRNSred   = 0;
                  pBuf->iTRNSgreen = 0;
                  pBuf->iTRNSblue  = 0;
                  pBuf->iTRNScount = iRawlen2;
                  MNG_COPY (pBuf->aTRNSentries, pRawdata2, iRawlen2)
                  break;
                }
      }
    }  
  }
  else                                 /* store as global */
  {
    pData->iGlobalTRNSrawlen = iRawlen;
    MNG_COPY (pData->aGlobalTRNSrawdata, pRawdata, iRawlen)

    {                                  /* create an animation object */
      mng_retcode iRetcode = create_ani_trns (pData, pData->iGlobalTRNSrawlen,
                                              pData->aGlobalTRNSrawdata);

      if (iRetcode)                    /* on error bail out */
        return iRetcode;
    }
  }
#endif /* MNG_SUPPORT_DISPLAY */

#ifdef MNG_STORE_CHUNKS
  if (pData->bStorechunks)
  {                                    /* initialize storage */
    mng_retcode iRetcode = ((mng_chunk_headerp)pHeader)->fCreate (pData, pHeader, ppChunk);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;

    if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR))
    {                                  /* not global! */
      ((mng_trnsp)*ppChunk)->bGlobal  = MNG_FALSE;
      ((mng_trnsp)*ppChunk)->iType    = pData->iColortype;

      if (iRawlen == 0)                /* if empty, indicate so */
        ((mng_trnsp)*ppChunk)->bEmpty = MNG_TRUE;
      else
      {
        ((mng_trnsp)*ppChunk)->bEmpty = MNG_FALSE;

        switch (pData->iColortype)     /* store fields */
        {
          case 0: {                    /* gray */
                    ((mng_trnsp)*ppChunk)->iGray  = mng_get_uint16 (pRawdata);
                    break;
                  }
          case 2: {                    /* rgb */
                    ((mng_trnsp)*ppChunk)->iRed   = mng_get_uint16 (pRawdata);
                    ((mng_trnsp)*ppChunk)->iGreen = mng_get_uint16 (pRawdata+2);
                    ((mng_trnsp)*ppChunk)->iBlue  = mng_get_uint16 (pRawdata+4);
                    break;
                  }
          case 3: {                    /* indexed */
                    ((mng_trnsp)*ppChunk)->iCount = iRawlen;
                    MNG_COPY (((mng_trnsp)*ppChunk)->aEntries, pRawdata, iRawlen)
                    break;
                  }
        }
      }
    }
    else                               /* it's global! */
    {
      ((mng_trnsp)*ppChunk)->bEmpty  = (mng_bool)(iRawlen == 0);
      ((mng_trnsp)*ppChunk)->bGlobal = MNG_TRUE;
      ((mng_trnsp)*ppChunk)->iType   = 0;
      ((mng_trnsp)*ppChunk)->iRawlen = iRawlen;

      MNG_COPY (((mng_trnsp)*ppChunk)->aRawdata, pRawdata, iRawlen)
    }
  }
#endif /* MNG_STORE_CHUNKS */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_TRNS, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* done */
}

/* ************************************************************************** */

READ_CHUNK (read_gama)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_GAMA, MNG_LC_START)
#endif
                                       /* sequence checks */
#ifdef MNG_INCLUDE_JNG
  if ((!pData->bHasMHDR) && (!pData->bHasIHDR) &&
      (!pData->bHasBASI) && (!pData->bHasDHDR) && (!pData->bHasJHDR))
#else
  if ((!pData->bHasMHDR) && (!pData->bHasIHDR) &&
      (!pData->bHasBASI) && (!pData->bHasDHDR)    )
#endif
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

#ifdef MNG_INCLUDE_JNG
  if ((pData->bHasIDAT) || (pData->bHasPLTE) || (pData->bHasJDAT) || (pData->bHasJDAA))
#else
  if ((pData->bHasIDAT) || (pData->bHasPLTE))
#endif
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

#ifdef MNG_INCLUDE_JNG
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR) || (pData->bHasJHDR))
#else
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR))
#endif
  {                                    /* length must be exactly 4 */
    if (iRawlen != 4)
      MNG_ERROR (pData, MNG_INVALIDLENGTH)
  }
  else
  {                                    /* length must be empty or exactly 4 */
    if ((iRawlen != 0) && (iRawlen != 4))
      MNG_ERROR (pData, MNG_INVALIDLENGTH)
  }

#ifdef MNG_INCLUDE_JNG
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR) || (pData->bHasJHDR))
#else
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR))
#endif
    pData->bHasGAMA = MNG_TRUE;        /* indicate we've got it */
  else
    pData->bHasglobalGAMA = (mng_bool)(iRawlen != 0);

#ifdef MNG_SUPPORT_DISPLAY
#ifdef MNG_INCLUDE_JNG
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR) || (pData->bHasJHDR))
#else
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR))
#endif
  {
    mng_imagep pImage;

    if (pData->bHasDHDR)               /* update delta image ? */
    {                                  /* store in object 0 ! */
      pImage = (mng_imagep)pData->pObjzero;
                                       /* store for color-processing routines */
      pImage->pImgbuf->iGamma   = mng_get_uint32 (pRawdata);
      pImage->pImgbuf->bHasGAMA = MNG_TRUE;
    }
    else
    {
      pImage = (mng_imagep)pData->pCurrentobj;

      if (!pImage)                     /* no object then dump it in obj 0 */
        pImage = (mng_imagep)pData->pObjzero;
                                       /* store for color-processing routines */
      pImage->pImgbuf->iGamma   = mng_get_uint32 (pRawdata);
      pImage->pImgbuf->bHasGAMA = MNG_TRUE;
    }
  }
  else
  {                                    /* store as global */
    if (iRawlen != 0)
      pData->iGlobalGamma = mng_get_uint32 (pRawdata);

    {                                  /* create an animation object */
      mng_retcode iRetcode = create_ani_gama (pData, (mng_bool)(iRawlen == 0),
                                              pData->iGlobalGamma);

      if (iRetcode)                    /* on error bail out */
        return iRetcode;
    }
  }
#endif /* MNG_SUPPORT_DISPLAY */

#ifdef MNG_STORE_CHUNKS
  if (pData->bStorechunks)
  {                                    /* initialize storage */
    mng_retcode iRetcode = ((mng_chunk_headerp)pHeader)->fCreate (pData, pHeader, ppChunk);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
                                       /* store the fields */
    ((mng_gamap)*ppChunk)->bEmpty = (mng_bool)(iRawlen == 0);

    if (iRawlen)
      ((mng_gamap)*ppChunk)->iGamma = mng_get_uint32 (pRawdata);

  }
#endif /* MNG_STORE_CHUNKS */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_GAMA, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* done */
}

/* ************************************************************************** */

READ_CHUNK (read_chrm)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_CHRM, MNG_LC_START)
#endif
                                       /* sequence checks */
#ifdef MNG_INCLUDE_JNG
  if ((!pData->bHasMHDR) && (!pData->bHasIHDR) &&
      (!pData->bHasBASI) && (!pData->bHasDHDR) && (!pData->bHasJHDR))
#else
  if ((!pData->bHasMHDR) && (!pData->bHasIHDR) &&
      (!pData->bHasBASI) && (!pData->bHasDHDR)    )
#endif
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

#ifdef MNG_INCLUDE_JNG
  if ((pData->bHasIDAT) || (pData->bHasPLTE) || (pData->bHasJDAT) || (pData->bHasJDAA))
#else
  if ((pData->bHasIDAT) || (pData->bHasPLTE))
#endif
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

#ifdef MNG_INCLUDE_JNG
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR) || (pData->bHasJHDR))
#else
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR))
#endif
  {                                    /* length must be exactly 32 */
    if (iRawlen != 32)
      MNG_ERROR (pData, MNG_INVALIDLENGTH)
  }
  else
  {                                    /* length must be empty or exactly 32 */
    if ((iRawlen != 0) && (iRawlen != 32))
      MNG_ERROR (pData, MNG_INVALIDLENGTH)
  }

#ifdef MNG_INCLUDE_JNG
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR) || (pData->bHasJHDR))
#else
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR))
#endif
    pData->bHasCHRM = MNG_TRUE;        /* indicate we've got it */
  else
    pData->bHasglobalCHRM = (mng_bool)(iRawlen != 0);

#ifdef MNG_SUPPORT_DISPLAY
  {
    mng_uint32 iWhitepointx,   iWhitepointy;
    mng_uint32 iPrimaryredx,   iPrimaryredy;
    mng_uint32 iPrimarygreenx, iPrimarygreeny;
    mng_uint32 iPrimarybluex,  iPrimarybluey;

    iWhitepointx   = mng_get_uint32 (pRawdata);
    iWhitepointy   = mng_get_uint32 (pRawdata+4);
    iPrimaryredx   = mng_get_uint32 (pRawdata+8);
    iPrimaryredy   = mng_get_uint32 (pRawdata+12);
    iPrimarygreenx = mng_get_uint32 (pRawdata+16);
    iPrimarygreeny = mng_get_uint32 (pRawdata+20);
    iPrimarybluex  = mng_get_uint32 (pRawdata+24);
    iPrimarybluey  = mng_get_uint32 (pRawdata+28);

#ifdef MNG_INCLUDE_JNG
    if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR) || (pData->bHasJHDR))
#else
    if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR))
#endif
    {
      mng_imagep     pImage;
      mng_imagedatap pBuf;

      if (pData->bHasDHDR)             /* update delta image ? */
      {                                /* store it in object 0 ! */
        pImage = (mng_imagep)pData->pObjzero;

        pBuf = pImage->pImgbuf;        /* address object buffer */
        pBuf->bHasCHRM = MNG_TRUE;     /* and tell it it's got a CHRM now */
                                       /* store for color-processing routines */
        pBuf->iWhitepointx   = iWhitepointx;
        pBuf->iWhitepointy   = iWhitepointy;
        pBuf->iPrimaryredx   = iPrimaryredx;
        pBuf->iPrimaryredy   = iPrimaryredy;
        pBuf->iPrimarygreenx = iPrimarygreenx;
        pBuf->iPrimarygreeny = iPrimarygreeny;
        pBuf->iPrimarybluex  = iPrimarybluex;
        pBuf->iPrimarybluey  = iPrimarybluey;
      }
      else
      {
        pImage = (mng_imagep)pData->pCurrentobj;

        if (!pImage)                   /* no object then dump it in obj 0 */
          pImage = (mng_imagep)pData->pObjzero;

        pBuf = pImage->pImgbuf;        /* address object buffer */
        pBuf->bHasCHRM = MNG_TRUE;     /* and tell it it's got a CHRM now */
                                       /* store for color-processing routines */
        pBuf->iWhitepointx   = iWhitepointx;
        pBuf->iWhitepointy   = iWhitepointy;
        pBuf->iPrimaryredx   = iPrimaryredx;
        pBuf->iPrimaryredy   = iPrimaryredy;
        pBuf->iPrimarygreenx = iPrimarygreenx;
        pBuf->iPrimarygreeny = iPrimarygreeny;
        pBuf->iPrimarybluex  = iPrimarybluex;
        pBuf->iPrimarybluey  = iPrimarybluey;
      }
    }
    else
    {                                  /* store as global */
      if (iRawlen != 0)
      {
        pData->iGlobalWhitepointx   = iWhitepointx;
        pData->iGlobalWhitepointy   = iWhitepointy;
        pData->iGlobalPrimaryredx   = iPrimaryredx;
        pData->iGlobalPrimaryredy   = iPrimaryredy;
        pData->iGlobalPrimarygreenx = iPrimarygreenx;
        pData->iGlobalPrimarygreeny = iPrimarygreeny;
        pData->iGlobalPrimarybluex  = iPrimarybluex;
        pData->iGlobalPrimarybluey  = iPrimarybluey;
      }

      {                                /* create an animation object */
        mng_retcode iRetcode = create_ani_chrm (pData, (mng_bool)(iRawlen == 0),
                                                iWhitepointx,   iWhitepointy,
                                                iPrimaryredx,   iPrimaryredy,
                                                iPrimarygreenx, iPrimarygreeny,
                                                iPrimarybluex,  iPrimarybluey);

        if (iRetcode)                  /* on error bail out */
          return iRetcode;
      }
    }
  }
#endif /* MNG_SUPPORT_DISPLAY */

#ifdef MNG_STORE_CHUNKS
  if (pData->bStorechunks)
  {                                    /* initialize storage */
    mng_retcode iRetcode = ((mng_chunk_headerp)pHeader)->fCreate (pData, pHeader, ppChunk);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
                                       /* store the fields */
    ((mng_chrmp)*ppChunk)->bEmpty = (mng_bool)(iRawlen == 0);

    if (iRawlen)
    {
      ((mng_chrmp)*ppChunk)->iWhitepointx = mng_get_uint32 (pRawdata);
      ((mng_chrmp)*ppChunk)->iWhitepointy = mng_get_uint32 (pRawdata+4);
      ((mng_chrmp)*ppChunk)->iRedx        = mng_get_uint32 (pRawdata+8);
      ((mng_chrmp)*ppChunk)->iRedy        = mng_get_uint32 (pRawdata+12);
      ((mng_chrmp)*ppChunk)->iGreenx      = mng_get_uint32 (pRawdata+16);
      ((mng_chrmp)*ppChunk)->iGreeny      = mng_get_uint32 (pRawdata+20);
      ((mng_chrmp)*ppChunk)->iBluex       = mng_get_uint32 (pRawdata+24);
      ((mng_chrmp)*ppChunk)->iBluey       = mng_get_uint32 (pRawdata+28);
    }  
  }
#endif /* MNG_STORE_CHUNKS */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_CHRM, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* done */
}

/* ************************************************************************** */

READ_CHUNK (read_srgb)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_SRGB, MNG_LC_START)
#endif
                                       /* sequence checks */
#ifdef MNG_INCLUDE_JNG
  if ((!pData->bHasMHDR) && (!pData->bHasIHDR) &&
      (!pData->bHasBASI) && (!pData->bHasDHDR) && (!pData->bHasJHDR))
#else
  if ((!pData->bHasMHDR) && (!pData->bHasIHDR) &&
      (!pData->bHasBASI) && (!pData->bHasDHDR)    )
#endif
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

#ifdef MNG_INCLUDE_JNG
  if ((pData->bHasIDAT) || (pData->bHasPLTE) || (pData->bHasJDAT) || (pData->bHasJDAA))
#else
  if ((pData->bHasIDAT) || (pData->bHasPLTE))
#endif
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

#ifdef MNG_INCLUDE_JNG
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR) || (pData->bHasJHDR))
#else
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR))
#endif
  {                                    /* length must be exactly 1 */
    if (iRawlen != 1)
      MNG_ERROR (pData, MNG_INVALIDLENGTH)
  }
  else
  {                                    /* length must be empty or exactly 1 */
    if ((iRawlen != 0) && (iRawlen != 1))
      MNG_ERROR (pData, MNG_INVALIDLENGTH)
  }

#ifdef MNG_INCLUDE_JNG
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR) || (pData->bHasJHDR))
#else
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR))
#endif
    pData->bHasSRGB = MNG_TRUE;        /* indicate we've got it */
  else
    pData->bHasglobalSRGB = (mng_bool)(iRawlen != 0);

#ifdef MNG_SUPPORT_DISPLAY
#ifdef MNG_INCLUDE_JNG
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR) || (pData->bHasJHDR))
#else
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR))
#endif
  {
    mng_imagep pImage;

    if (pData->bHasDHDR)               /* update delta image ? */
    {                                  /* store in object 0 ! */
      pImage = (mng_imagep)pData->pObjzero;
                                       /* store for color-processing routines */
      pImage->pImgbuf->iRenderingintent = *pRawdata;
      pImage->pImgbuf->bHasSRGB         = MNG_TRUE;
    }
    else
    {
      pImage = (mng_imagep)pData->pCurrentobj;

      if (!pImage)                     /* no object then dump it in obj 0 */
        pImage = (mng_imagep)pData->pObjzero;
                                       /* store for color-processing routines */
      pImage->pImgbuf->iRenderingintent = *pRawdata;
      pImage->pImgbuf->bHasSRGB         = MNG_TRUE;
    }
  }
  else
  {                                    /* store as global */
    if (iRawlen != 0)
      pData->iGlobalRendintent = *pRawdata;

    {                                  /* create an animation object */
      mng_retcode iRetcode = create_ani_srgb (pData, (mng_bool)(iRawlen == 0),
                                              pData->iGlobalRendintent);

      if (iRetcode)                    /* on error bail out */
        return iRetcode;
    }
  }
#endif /* MNG_SUPPORT_DISPLAY */

#ifdef MNG_STORE_CHUNKS
  if (pData->bStorechunks)
  {                                    /* initialize storage */
    mng_retcode iRetcode = ((mng_chunk_headerp)pHeader)->fCreate (pData, pHeader, ppChunk);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
                                       /* store the fields */
    ((mng_srgbp)*ppChunk)->bEmpty = (mng_bool)(iRawlen == 0);

    if (iRawlen)
      ((mng_srgbp)*ppChunk)->iRenderingintent = *pRawdata;

  }
#endif /* MNG_STORE_CHUNKS */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_SRGB, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* done */
}

/* ************************************************************************** */

READ_CHUNK (read_iccp)
{
  mng_retcode iRetcode;
  mng_uint8p  pTemp;
  mng_uint32  iCompressedsize;
  mng_uint32  iProfilesize;
  mng_uint32  iBufsize = 0;
  mng_uint8p  pBuf = 0;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_ICCP, MNG_LC_START)
#endif
                                       /* sequence checks */
#ifdef MNG_INCLUDE_JNG
  if ((!pData->bHasMHDR) && (!pData->bHasIHDR) &&
      (!pData->bHasBASI) && (!pData->bHasDHDR) && (!pData->bHasJHDR))
#else
  if ((!pData->bHasMHDR) && (!pData->bHasIHDR) &&
      (!pData->bHasBASI) && (!pData->bHasDHDR)    )
#endif
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

#ifdef MNG_INCLUDE_JNG
  if ((pData->bHasIDAT) || (pData->bHasPLTE) || (pData->bHasJDAT) || (pData->bHasJDAA))
#else
  if ((pData->bHasIDAT) || (pData->bHasPLTE))
#endif
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

#ifdef MNG_INCLUDE_JNG
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR) || (pData->bHasJHDR))
#else
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR))
#endif
  {                                    /* length must be at least 2 */
    if (iRawlen < 2)
      MNG_ERROR (pData, MNG_INVALIDLENGTH)
  }
  else
  {                                    /* length must be empty or at least 2 */
    if ((iRawlen != 0) && (iRawlen < 2))
      MNG_ERROR (pData, MNG_INVALIDLENGTH)
  }

  pTemp = find_null (pRawdata);        /* find null-separator */
                                       /* not found inside input-data ? */
  if ((pTemp - pRawdata) > (mng_int32)iRawlen)
    MNG_ERROR (pData, MNG_NULLNOTFOUND)
                                       /* determine size of compressed profile */
  iCompressedsize = (mng_uint32)(iRawlen - (pTemp - pRawdata) - 2);
                                       /* decompress the profile */
  iRetcode = inflate_buffer (pData, pTemp+2, iCompressedsize,
                             &pBuf, &iBufsize, &iProfilesize);

#ifdef MNG_CHECK_BAD_ICCP              /* Check for bad iCCP chunk */
  if ((iRetcode) && (!strncmp ((char *)pRawdata, "Photoshop ICC profile", 21)))
  {
    if (iRawlen == 2615)               /* is it the sRGB profile ? */
    {
      mng_chunk_header chunk_srgb = {MNG_UINT_sRGB, init_srgb, free_srgb,
                                     read_srgb, write_srgb, 0, 0};
                                       /* pretend it's an sRGB chunk then ! */
      iRetcode = read_srgb (pData, &chunk_srgb, 1, (mng_ptr)"0", ppChunk);

      if (iRetcode)                    /* on error bail out */
      {                                /* don't forget to drop the temp buffer */
        MNG_FREEX (pData, pBuf, iBufsize)
        return iRetcode;
      }
    }
  }
  else
  {
#endif /* MNG_CHECK_BAD_ICCP */

    if (iRetcode)                      /* on error bail out */
    {                                  /* don't forget to drop the temp buffer */
      MNG_FREEX (pData, pBuf, iBufsize)
      return iRetcode;
    }

#ifdef MNG_INCLUDE_JNG
    if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR) || (pData->bHasJHDR))
#else
    if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR))
#endif
      pData->bHasICCP = MNG_TRUE;      /* indicate we've got it */
    else
      pData->bHasglobalICCP = (mng_bool)(iRawlen != 0);

#ifdef MNG_SUPPORT_DISPLAY
#ifdef MNG_INCLUDE_JNG
    if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR) || (pData->bHasJHDR))
#else
    if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR))
#endif
    {
      mng_imagep pImage;

      if (pData->bHasDHDR)             /* update delta image ? */
      {                                /* store in object 0 ! */
        pImage = (mng_imagep)pData->pObjzero;

        if (pImage->pImgbuf->pProfile) /* profile existed ? */
          MNG_FREEX (pData, pImage->pImgbuf->pProfile, pImage->pImgbuf->iProfilesize)
                                       /* allocate a buffer & copy it */
        MNG_ALLOC (pData, pImage->pImgbuf->pProfile, iProfilesize)
        MNG_COPY  (pImage->pImgbuf->pProfile, pBuf, iProfilesize)
                                       /* store it's length as well */
        pImage->pImgbuf->iProfilesize = iProfilesize;
        pImage->pImgbuf->bHasICCP     = MNG_TRUE;
      }
      else
      {
        pImage = (mng_imagep)pData->pCurrentobj;

        if (!pImage)                   /* no object then dump it in obj 0 */
          pImage = (mng_imagep)pData->pObjzero;

        if (pImage->pImgbuf->pProfile) /* profile existed ? */
          MNG_FREEX (pData, pImage->pImgbuf->pProfile, pImage->pImgbuf->iProfilesize)
                                       /* allocate a buffer & copy it */
        MNG_ALLOC (pData, pImage->pImgbuf->pProfile, iProfilesize)
        MNG_COPY  (pImage->pImgbuf->pProfile, pBuf, iProfilesize)
                                       /* store it's length as well */
        pImage->pImgbuf->iProfilesize = iProfilesize;
        pImage->pImgbuf->bHasICCP     = MNG_TRUE;
      }
    }
    else
    {                                  /* store as global */
      if (iRawlen == 0)                /* empty chunk ? */
      {
        if (pData->pGlobalProfile)     /* did we have a global profile ? */
          MNG_FREEX (pData, pData->pGlobalProfile, pData->iGlobalProfilesize)

        pData->iGlobalProfilesize = 0; /* reset to null */
        pData->pGlobalProfile     = MNG_NULL; 
      }
      else
      {                                /* allocate a global buffer & copy it */
        MNG_ALLOC (pData, pData->pGlobalProfile, iProfilesize)
        MNG_COPY  (pData->pGlobalProfile, pBuf, iProfilesize)
                                       /* store it's length as well */
        pData->iGlobalProfilesize = iProfilesize;
      }

                                       /* create an animation object */
      iRetcode = create_ani_iccp (pData, (mng_bool)(iRawlen == 0),
                                  pData->iGlobalProfilesize,
                                  pData->pGlobalProfile);

      if (iRetcode)                    /* on error bail out */
        return iRetcode;
    }
#endif /* MNG_SUPPORT_DISPLAY */

#ifdef MNG_STORE_CHUNKS
    if (pData->bStorechunks)
    {                                  /* initialize storage */
      iRetcode = ((mng_chunk_headerp)pHeader)->fCreate (pData, pHeader, ppChunk);

      if (iRetcode)                    /* on error bail out */
      {                                /* don't forget to drop the temp buffer */
        MNG_FREEX (pData, pBuf, iBufsize)
        return iRetcode;
      }
                                       /* store the fields */
      ((mng_iccpp)*ppChunk)->bEmpty = (mng_bool)(iRawlen == 0);

      if (iRawlen)                     /* not empty ? */
      {
        if (!pBuf)                     /* hasn't been unpuzzled it yet ? */
        {                              /* find null-separator */
          pTemp = find_null (pRawdata);
                                       /* not found inside input-data ? */
          if ((pTemp - pRawdata) > (mng_int32)iRawlen)
            MNG_ERROR (pData, MNG_NULLNOTFOUND)
                                       /* determine size of compressed profile */
          iCompressedsize = iRawlen - (pTemp - pRawdata) - 2;
                                       /* decompress the profile */
          iRetcode = inflate_buffer (pData, pTemp+2, iCompressedsize,
                                     &pBuf, &iBufsize, &iProfilesize);

          if (iRetcode)                /* on error bail out */
          {                            /* don't forget to drop the temp buffer */
            MNG_FREEX (pData, pBuf, iBufsize)
            return iRetcode;
          }
        }

        ((mng_iccpp)*ppChunk)->iNamesize = (mng_uint32)(pTemp - pRawdata);

        if (((mng_iccpp)*ppChunk)->iNamesize)
        {
          MNG_ALLOC (pData, ((mng_iccpp)*ppChunk)->zName,
                            ((mng_iccpp)*ppChunk)->iNamesize + 1)
          MNG_COPY  (((mng_iccpp)*ppChunk)->zName, pRawdata,
                     ((mng_iccpp)*ppChunk)->iNamesize)
        }

        ((mng_iccpp)*ppChunk)->iCompression = *(pTemp+1);
        ((mng_iccpp)*ppChunk)->iProfilesize = iProfilesize;

        MNG_ALLOC (pData, ((mng_iccpp)*ppChunk)->pProfile, iProfilesize)
        MNG_COPY  (((mng_iccpp)*ppChunk)->pProfile, pBuf, iProfilesize)
      }
    }
#endif /* MNG_STORE_CHUNKS */

    if (pBuf)                          /* free the temporary buffer */
      MNG_FREEX (pData, pBuf, iBufsize)

#ifdef MNG_CHECK_BAD_ICCP
  }
#endif

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_ICCP, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* done */
}

/* ************************************************************************** */

READ_CHUNK (read_text)
{
  mng_uint32 iKeywordlen, iTextlen;
  mng_pchar  zKeyword, zText;
  mng_uint8p pTemp;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_TEXT, MNG_LC_START)
#endif
                                       /* sequence checks */
#ifdef MNG_INCLUDE_JNG
  if ((!pData->bHasMHDR) && (!pData->bHasIHDR) &&
      (!pData->bHasBASI) && (!pData->bHasDHDR) && (!pData->bHasJHDR))
#else
  if ((!pData->bHasMHDR) && (!pData->bHasIHDR) &&
      (!pData->bHasBASI) && (!pData->bHasDHDR)    )
#endif
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

  if (iRawlen < 2)                     /* length must be at least 2 */
    MNG_ERROR (pData, MNG_INVALIDLENGTH)

  pTemp = find_null (pRawdata);        /* find the null separator */
                                       /* not found inside input-data ? */
  if ((pTemp - pRawdata) > (mng_int32)iRawlen)
    MNG_ERROR (pData, MNG_NULLNOTFOUND)

  if (pTemp == pRawdata)               /* there must be at least 1 char for keyword */
    MNG_ERROR (pData, MNG_KEYWORDNULL)

  iKeywordlen = (mng_uint32)(pTemp - pRawdata);
  iTextlen    = iRawlen - iKeywordlen - 1;

  if (pData->fProcesstext)             /* inform the application ? */
  {
    mng_bool bOke;

    MNG_ALLOC (pData, zKeyword, iKeywordlen + 1)
    MNG_COPY  (zKeyword, pRawdata, iKeywordlen)

    MNG_ALLOCX (pData, zText, iTextlen + 1)

    if (!zText)                        /* on error bail out */
    {
      MNG_FREEX (pData, zKeyword, iKeywordlen + 1)
      MNG_ERROR (pData, MNG_OUTOFMEMORY)
    }

    if (iTextlen)
      MNG_COPY (zText, pTemp+1, iTextlen)

    bOke = pData->fProcesstext ((mng_handle)pData, MNG_TYPE_TEXT, zKeyword, zText, 0, 0);

    MNG_FREEX (pData, zText, iTextlen + 1)
    MNG_FREEX (pData, zKeyword, iKeywordlen + 1)

    if (!bOke)
      MNG_ERROR (pData, MNG_APPMISCERROR)

  }

#ifdef MNG_STORE_CHUNKS
  if (pData->bStorechunks)
  {                                    /* initialize storage */
    mng_retcode iRetcode = ((mng_chunk_headerp)pHeader)->fCreate (pData, pHeader, ppChunk);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
                                       /* store the fields */
    ((mng_textp)*ppChunk)->iKeywordsize = iKeywordlen;
    ((mng_textp)*ppChunk)->iTextsize    = iTextlen;

    if (iKeywordlen)
    {
      MNG_ALLOC (pData, ((mng_textp)*ppChunk)->zKeyword, iKeywordlen+1)
      MNG_COPY  (((mng_textp)*ppChunk)->zKeyword, pRawdata, iKeywordlen)
    }

    if (iTextlen)
    {
      MNG_ALLOC (pData, ((mng_textp)*ppChunk)->zText, iTextlen+1)
      MNG_COPY  (((mng_textp)*ppChunk)->zText, pTemp+1, iTextlen)
    }
  }
#endif /* MNG_STORE_CHUNKS */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_TEXT, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* done */
}

/* ************************************************************************** */

READ_CHUNK (read_ztxt)
{
  mng_retcode iRetcode;
  mng_uint32  iKeywordlen, iTextlen;
  mng_pchar   zKeyword;
  mng_uint8p  pTemp;
  mng_uint32  iCompressedsize;
  mng_uint32  iBufsize;
  mng_uint8p  pBuf;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_ZTXT, MNG_LC_START)
#endif
                                       /* sequence checks */
#ifdef MNG_INCLUDE_JNG
  if ((!pData->bHasMHDR) && (!pData->bHasIHDR) &&
      (!pData->bHasBASI) && (!pData->bHasDHDR) && (!pData->bHasJHDR))
#else
  if ((!pData->bHasMHDR) && (!pData->bHasIHDR) &&
      (!pData->bHasBASI) && (!pData->bHasDHDR)    )
#endif
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

  if (iRawlen < 3)                     /* length must be at least 3 */
    MNG_ERROR (pData, MNG_INVALIDLENGTH)

  pTemp = find_null (pRawdata);        /* find the null separator */
                                       /* not found inside input-data ? */
  if ((pTemp - pRawdata) > (mng_int32)iRawlen)
    MNG_ERROR (pData, MNG_NULLNOTFOUND)

  if (pTemp == pRawdata)               /* there must be at least 1 char for keyword */
    MNG_ERROR (pData, MNG_KEYWORDNULL)

  if (*(pTemp+1) != 0)                 /* only deflate compression-method allowed */
    MNG_ERROR (pData, MNG_INVALIDCOMPRESS)

  iKeywordlen     = (mng_uint32)(pTemp - pRawdata);
  iCompressedsize = (mng_uint32)(iRawlen - iKeywordlen - 2);

  zKeyword        = 0;                 /* there's no keyword buffer yet */
  pBuf            = 0;                 /* or a temporary buffer ! */

  if (pData->fProcesstext)             /* inform the application ? */
  {                                    /* decompress the text */
    iRetcode = inflate_buffer (pData, pTemp+2, iCompressedsize,
                               &pBuf, &iBufsize, &iTextlen);

    if (iRetcode)                      /* on error bail out */
    {                                  /* don't forget to drop the temp buffers */
      MNG_FREEX (pData, pBuf, iBufsize)
      return iRetcode;
    }

    MNG_ALLOCX (pData, zKeyword, iKeywordlen+1)

    if (!zKeyword)                     /* on error bail out */
    {                                  /* don't forget to drop the temp buffers */
      MNG_FREEX (pData, pBuf, iBufsize)
      MNG_ERROR (pData, MNG_OUTOFMEMORY);
    }

    MNG_COPY (zKeyword, pRawdata, iKeywordlen)

    if (!pData->fProcesstext ((mng_handle)pData, MNG_TYPE_ZTXT, zKeyword, (mng_pchar)pBuf, 0, 0))
    {                                  /* don't forget to drop the temp buffers */
      MNG_FREEX (pData, pBuf, iBufsize)
      MNG_FREEX (pData, zKeyword, iKeywordlen+1)
      MNG_ERROR (pData, MNG_APPMISCERROR)
    }
  }

#ifdef MNG_STORE_CHUNKS
  if (pData->bStorechunks)
  {                                    /* initialize storage */
    iRetcode = ((mng_chunk_headerp)pHeader)->fCreate (pData, pHeader, ppChunk);

    if (iRetcode)                      /* on error bail out */
    {                                  /* don't forget to drop the temp buffers */
      MNG_FREEX (pData, pBuf, iBufsize)
      MNG_FREEX (pData, zKeyword, iKeywordlen+1)
      return iRetcode;
    }
                                       /* store the fields */
    ((mng_ztxtp)*ppChunk)->iKeywordsize = iKeywordlen;
    ((mng_ztxtp)*ppChunk)->iCompression = *(pTemp+1);

    if ((!pBuf) && (iCompressedsize))  /* did we not get a text-buffer yet ? */
    {                                  /* decompress the text */
      iRetcode = inflate_buffer (pData, pTemp+2, iCompressedsize,
                                 &pBuf, &iBufsize, &iTextlen);

      if (iRetcode)                    /* on error bail out */
      {                                /* don't forget to drop the temp buffers */
        MNG_FREEX (pData, pBuf, iBufsize)
        MNG_FREEX (pData, zKeyword, iKeywordlen+1)
        return iRetcode;
      }
    }

    MNG_ALLOCX (pData, ((mng_ztxtp)*ppChunk)->zKeyword, iKeywordlen + 1)
                                       /* on error bail out */
    if (!((mng_ztxtp)*ppChunk)->zKeyword)
    {                                  /* don't forget to drop the temp buffers */
      MNG_FREEX (pData, pBuf, iBufsize)
      MNG_FREEX (pData, zKeyword, iKeywordlen+1)
      MNG_ERROR (pData, MNG_OUTOFMEMORY);
    }

    MNG_COPY (((mng_ztxtp)*ppChunk)->zKeyword, pRawdata, iKeywordlen)

    ((mng_ztxtp)*ppChunk)->iTextsize = iTextlen;

    if (iCompressedsize)
    {
      MNG_ALLOCX (pData, ((mng_ztxtp)*ppChunk)->zText, iTextlen + 1)
                                       /* on error bail out */
      if (!((mng_ztxtp)*ppChunk)->zText)
      {                                /* don't forget to drop the temp buffers */
        MNG_FREEX (pData, pBuf, iBufsize)
        MNG_FREEX (pData, zKeyword, iKeywordlen+1)
        MNG_ERROR (pData, MNG_OUTOFMEMORY);
      }

      MNG_COPY (((mng_ztxtp)*ppChunk)->zText, pBuf, iTextlen)
    }
  }
#endif /* MNG_STORE_CHUNKS */

  MNG_FREEX (pData, pBuf, iBufsize)    /* free the temporary buffers */
  MNG_FREEX (pData, zKeyword, iKeywordlen+1)

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_ZTXT, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* done */
}

/* ************************************************************************** */

READ_CHUNK (read_itxt)
{
  mng_retcode iRetcode;
  mng_uint32  iKeywordlen, iTextlen, iLanguagelen, iTranslationlen;
  mng_pchar   zKeyword, zLanguage, zTranslation;
  mng_uint8p  pNull1, pNull2, pNull3;
  mng_uint32  iCompressedsize;
  mng_uint8   iCompressionflag;
  mng_uint32  iBufsize;
  mng_uint8p  pBuf;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_ITXT, MNG_LC_START)
#endif
                                       /* sequence checks */
#ifdef MNG_INCLUDE_JNG
  if ((!pData->bHasMHDR) && (!pData->bHasIHDR) &&
      (!pData->bHasBASI) && (!pData->bHasDHDR) && (!pData->bHasJHDR))
#else
  if ((!pData->bHasMHDR) && (!pData->bHasIHDR) &&
      (!pData->bHasBASI) && (!pData->bHasDHDR)    )
#endif
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

  if (iRawlen < 6)                     /* length must be at least 6 */
    MNG_ERROR (pData, MNG_INVALIDLENGTH)

  pNull1 = find_null (pRawdata);       /* find the null separators */
  pNull2 = find_null (pNull1+3);
  pNull3 = find_null (pNull2+1);
                                       /* not found inside input-data ? */
  if (((pNull1 - pRawdata) > (mng_int32)iRawlen) ||
      ((pNull2 - pRawdata) > (mng_int32)iRawlen) ||
      ((pNull3 - pRawdata) > (mng_int32)iRawlen)    )
    MNG_ERROR (pData, MNG_NULLNOTFOUND)

  if (pNull1 == pRawdata)              /* there must be at least 1 char for keyword */
    MNG_ERROR (pData, MNG_KEYWORDNULL)
                                       /* compression or not ? */
  if ((*(pNull1+1) != 0) && (*(pNull1+1) != 1))
    MNG_ERROR (pData, MNG_INVALIDCOMPRESS)

  if (*(pNull1+2) != 0)                /* only deflate compression-method allowed */
    MNG_ERROR (pData, MNG_INVALIDCOMPRESS)

  iKeywordlen      = (mng_uint32)(pNull1 - pRawdata);
  iLanguagelen     = (mng_uint32)(pNull2 - pNull1 - 3);
  iTranslationlen  = (mng_uint32)(pNull3 - pNull2 - 1);
  iCompressedsize  = (mng_uint32)(iRawlen - iKeywordlen - iLanguagelen - iTranslationlen - 5);
  iCompressionflag = *(pNull1+1);

  zKeyword     = 0;                    /* no buffers acquired yet */
  zLanguage    = 0;
  zTranslation = 0;
  pBuf         = 0;
  iTextlen     = 0;

  if (pData->fProcesstext)             /* inform the application ? */
  {
    if (iCompressionflag)              /* decompress the text ? */
    {
      iRetcode = inflate_buffer (pData, pNull3+1, iCompressedsize,
                                 &pBuf, &iBufsize, &iTextlen);

      if (iRetcode)                    /* on error bail out */
      {                                /* don't forget to drop the temp buffer */
        MNG_FREEX (pData, pBuf, iBufsize)
        return iRetcode;
      }
    }
    else
    {
      iTextlen = iCompressedsize;
      iBufsize = iTextlen+1;           /* plus 1 for terminator byte!!! */

      MNG_ALLOCX (pData, pBuf, iBufsize);
      MNG_COPY   (pBuf, pNull3+1, iTextlen);
    }

    MNG_ALLOCX (pData, zKeyword,     iKeywordlen     + 1)
    MNG_ALLOCX (pData, zLanguage,    iLanguagelen    + 1)
    MNG_ALLOCX (pData, zTranslation, iTranslationlen + 1)
                                       /* on error bail out */
    if ((!zKeyword) || (!zLanguage) || (!zTranslation))
    {                                  /* don't forget to drop the temp buffers */
      MNG_FREEX (pData, zTranslation, iTranslationlen + 1)
      MNG_FREEX (pData, zLanguage,    iLanguagelen    + 1)
      MNG_FREEX (pData, zKeyword,     iKeywordlen     + 1)
      MNG_FREEX (pData, pBuf, iBufsize)
      MNG_ERROR (pData, MNG_OUTOFMEMORY)
    }

    MNG_COPY (zKeyword,     pRawdata, iKeywordlen)
    MNG_COPY (zLanguage,    pNull1+3, iLanguagelen)
    MNG_COPY (zTranslation, pNull2+1, iTranslationlen)

    if (!pData->fProcesstext ((mng_handle)pData, MNG_TYPE_ITXT, zKeyword, (mng_pchar)pBuf,
                                                                zLanguage, zTranslation))
    {                                  /* don't forget to drop the temp buffers */
      MNG_FREEX (pData, zTranslation, iTranslationlen + 1)
      MNG_FREEX (pData, zLanguage,    iLanguagelen    + 1)
      MNG_FREEX (pData, zKeyword,     iKeywordlen     + 1)
      MNG_FREEX (pData, pBuf,         iBufsize)
      
      MNG_ERROR (pData, MNG_APPMISCERROR)
    }
  }

#ifdef MNG_STORE_CHUNKS
  if (pData->bStorechunks)
  {                                    /* initialize storage */
    iRetcode = ((mng_chunk_headerp)pHeader)->fCreate (pData, pHeader, ppChunk);

    if (iRetcode)                      /* on error bail out */
    {                                  /* don't forget to drop the temp buffers */
      MNG_FREEX (pData, zTranslation, iTranslationlen + 1)
      MNG_FREEX (pData, zLanguage,    iLanguagelen    + 1)
      MNG_FREEX (pData, zKeyword,     iKeywordlen     + 1)
      MNG_FREEX (pData, pBuf,         iBufsize)
      return iRetcode;
    }
                                       /* store the fields */
    ((mng_itxtp)*ppChunk)->iKeywordsize       = iKeywordlen;
    ((mng_itxtp)*ppChunk)->iLanguagesize      = iLanguagelen;
    ((mng_itxtp)*ppChunk)->iTranslationsize   = iTranslationlen;
    ((mng_itxtp)*ppChunk)->iCompressionflag   = *(pNull1+1);
    ((mng_itxtp)*ppChunk)->iCompressionmethod = *(pNull1+2);

    if ((!pBuf) && (iCompressedsize))  /* did we not get a text-buffer yet ? */
    {
      if (iCompressionflag)            /* decompress the text ? */
      {
        iRetcode = inflate_buffer (pData, pNull3+1, iCompressedsize,
                                   &pBuf, &iBufsize, &iTextlen);

        if (iRetcode)                  /* on error bail out */
        {                              /* don't forget to drop the temp buffers */
          MNG_FREEX (pData, zTranslation, iTranslationlen + 1)
          MNG_FREEX (pData, zLanguage,    iLanguagelen    + 1)
          MNG_FREEX (pData, zKeyword,     iKeywordlen     + 1)
          MNG_FREEX (pData, pBuf,         iBufsize)
          return iRetcode;
        }
      }
      else
      {
        iTextlen = iCompressedsize;
        iBufsize = iTextlen+1;         /* plus 1 for terminator byte!!! */

        MNG_ALLOCX (pData, pBuf, iBufsize);
        MNG_COPY   (pBuf, pNull3+1, iTextlen);
      }
    }

    MNG_ALLOCX (pData, ((mng_itxtp)*ppChunk)->zKeyword,     iKeywordlen     + 1)
    MNG_ALLOCX (pData, ((mng_itxtp)*ppChunk)->zLanguage,    iLanguagelen    + 1)
    MNG_ALLOCX (pData, ((mng_itxtp)*ppChunk)->zTranslation, iTranslationlen + 1)
                                       /* on error bail out */
    if ((!((mng_itxtp)*ppChunk)->zKeyword    ) ||
        (!((mng_itxtp)*ppChunk)->zLanguage   ) ||
        (!((mng_itxtp)*ppChunk)->zTranslation)    )
    {                                  /* don't forget to drop the temp buffers */
      MNG_FREEX (pData, zTranslation, iTranslationlen + 1)
      MNG_FREEX (pData, zLanguage,    iLanguagelen    + 1)
      MNG_FREEX (pData, zKeyword,     iKeywordlen     + 1)
      MNG_FREEX (pData, pBuf,         iBufsize)
      MNG_ERROR (pData, MNG_OUTOFMEMORY)
    }

    MNG_COPY (((mng_itxtp)*ppChunk)->zKeyword,     pRawdata, iKeywordlen)
    MNG_COPY (((mng_itxtp)*ppChunk)->zLanguage,    pNull1+3, iLanguagelen)
    MNG_COPY (((mng_itxtp)*ppChunk)->zTranslation, pNull2+1, iTranslationlen)

    ((mng_itxtp)*ppChunk)->iTextsize = iTextlen;

    if (iTextlen)
    {
      MNG_ALLOCX (pData, ((mng_itxtp)*ppChunk)->zText, iTextlen + 1)

      if (!((mng_itxtp)*ppChunk)->zText)
      {                                /* don't forget to drop the temp buffers */
        MNG_FREEX (pData, zTranslation, iTranslationlen + 1)
        MNG_FREEX (pData, zLanguage,    iLanguagelen    + 1)
        MNG_FREEX (pData, zKeyword,     iKeywordlen     + 1)
        MNG_FREEX (pData, pBuf,         iBufsize)
        MNG_ERROR (pData, MNG_OUTOFMEMORY)
      }

      MNG_COPY  (((mng_itxtp)*ppChunk)->zText, pBuf, iTextlen)
    }
  }
#endif /* MNG_STORE_CHUNKS */
                                       /* free the temporary buffers */
  MNG_FREEX (pData, zTranslation, iTranslationlen + 1)
  MNG_FREEX (pData, zLanguage,    iLanguagelen    + 1)
  MNG_FREEX (pData, zKeyword,     iKeywordlen     + 1)
  MNG_FREEX (pData, pBuf,         iBufsize)

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_ITXT, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* done */
}

/* ************************************************************************** */

READ_CHUNK (read_bkgd)
{
#ifdef MNG_SUPPORT_DISPLAY
  mng_imagep     pImage = (mng_imagep)pData->pCurrentobj;
  mng_imagedatap pBuf;
#endif

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_BKGD, MNG_LC_START)
#endif
                                       /* sequence checks */
#ifdef MNG_INCLUDE_JNG
  if ((!pData->bHasMHDR) && (!pData->bHasIHDR) &&
      (!pData->bHasBASI) && (!pData->bHasDHDR) && (!pData->bHasJHDR))
#else
  if ((!pData->bHasMHDR) && (!pData->bHasIHDR) &&
      (!pData->bHasBASI) && (!pData->bHasDHDR)    )
#endif
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

#ifdef MNG_INCLUDE_JNG
  if ((pData->bHasIDAT) || (pData->bHasJDAT) || (pData->bHasJDAA))
#else
  if (pData->bHasIDAT)
#endif
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

  if (iRawlen > 6)                     /* it just can't be bigger than that! */
    MNG_ERROR (pData, MNG_INVALIDLENGTH)

#ifdef MNG_INCLUDE_JNG                 /* length checks */
  if (pData->bHasJHDR)
  {
    if (((pData->iJHDRcolortype == 8) || (pData->iJHDRcolortype == 12)) && (iRawlen != 2))
      MNG_ERROR (pData, MNG_INVALIDLENGTH)

    if (((pData->iJHDRcolortype == 10) || (pData->iJHDRcolortype == 14)) && (iRawlen != 6))
      MNG_ERROR (pData, MNG_INVALIDLENGTH)
  }
  else
#endif /* MNG_INCLUDE_JNG */
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR))
  {
    if (((pData->iColortype == 0) || (pData->iColortype == 4)) && (iRawlen != 2))
      MNG_ERROR (pData, MNG_INVALIDLENGTH)

    if (((pData->iColortype == 2) || (pData->iColortype == 6)) && (iRawlen != 6))
      MNG_ERROR (pData, MNG_INVALIDLENGTH)

    if ((pData->iColortype == 3) && (iRawlen != 1))
      MNG_ERROR (pData, MNG_INVALIDLENGTH)
  }
  else
  {
    if (iRawlen != 6)                  /* global is always 16-bit RGB ! */
      MNG_ERROR (pData, MNG_INVALIDLENGTH)
  }

#ifdef MNG_INCLUDE_JNG
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR) || (pData->bHasJHDR))
#else
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR))
#endif
    pData->bHasBKGD = MNG_TRUE;        /* indicate bKGD available */
  else
    pData->bHasglobalBKGD = (mng_bool)(iRawlen != 0);

#ifdef MNG_SUPPORT_DISPLAY
  if (!pImage)                         /* if no object dump it in obj 0 */
    pImage = (mng_imagep)pData->pObjzero;

  pBuf = pImage->pImgbuf;              /* address object buffer */

#ifdef MNG_INCLUDE_JNG
  if (pData->bHasJHDR)
  {
    pBuf->bHasBKGD = MNG_TRUE;         /* tell the object it's got bKGD now */

    switch (pData->iJHDRcolortype)     /* store fields for future reference */
    {
      case  8 : ;                      /* gray */
      case 12 : {                      /* graya */
                  pBuf->iBKGDgray  = mng_get_uint16 (pRawdata);
                  break;
                }
      case 10 : ;                      /* rgb */
      case 14 : {                      /* rgba */
                  pBuf->iBKGDred   = mng_get_uint16 (pRawdata);
                  pBuf->iBKGDgreen = mng_get_uint16 (pRawdata+2);
                  pBuf->iBKGDblue  = mng_get_uint16 (pRawdata+4);
                  break;
                }
    }
  }
  else
#endif /* MNG_INCLUDE_JNG */
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR))
  {
    pBuf->bHasBKGD = MNG_TRUE;         /* tell the object it's got bKGD now */

    switch (pData->iColortype)         /* store fields for future reference */
    {
      case 0 : ;                        /* gray */
      case 4 : {                        /* graya */
                 pBuf->iBKGDgray  = mng_get_uint16 (pRawdata);
                 break;
               }
      case 2 : ;                        /* rgb */
      case 6 : {                        /* rgba */
                 pBuf->iBKGDred   = mng_get_uint16 (pRawdata);
                 pBuf->iBKGDgreen = mng_get_uint16 (pRawdata+2);
                 pBuf->iBKGDblue  = mng_get_uint16 (pRawdata+4);
                 break;
               }
      case 3 : {                        /* indexed */
                 pBuf->iBKGDindex = *pRawdata;
                 break;
               }
    }
  }
  else                                 /* store as global */
  {
    if (iRawlen)
    {
      pData->iGlobalBKGDred   = mng_get_uint16 (pRawdata);
      pData->iGlobalBKGDgreen = mng_get_uint16 (pRawdata+2);
      pData->iGlobalBKGDblue  = mng_get_uint16 (pRawdata+4);
    }

    {                                  /* create an animation object */
      mng_retcode iRetcode = create_ani_bkgd (pData, pData->iGlobalBKGDred,
                                              pData->iGlobalBKGDgreen,
                                              pData->iGlobalBKGDblue);

      if (iRetcode)                    /* on error bail out */
        return iRetcode;
    }
  }
#endif /* MNG_SUPPORT_DISPLAY */

#ifdef MNG_STORE_CHUNKS
  if (pData->bStorechunks)
  {                                    /* initialize storage */
    mng_retcode iRetcode = ((mng_chunk_headerp)pHeader)->fCreate (pData, pHeader, ppChunk);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
                                       /* store the fields */
    ((mng_bkgdp)*ppChunk)->bEmpty = (mng_bool)(iRawlen == 0);
    ((mng_bkgdp)*ppChunk)->iType  = pData->iColortype;

    if (iRawlen)
    {
      switch (iRawlen)                 /* guess from length */
      {
        case 1 : {                     /* indexed */
                   ((mng_bkgdp)*ppChunk)->iType  = 3;
                   ((mng_bkgdp)*ppChunk)->iIndex = *pRawdata;
                   break;
                 }
        case 2 : {                     /* gray */
                   ((mng_bkgdp)*ppChunk)->iType  = 0;
                   ((mng_bkgdp)*ppChunk)->iGray  = mng_get_uint16 (pRawdata);
                   break;
                 }
        case 6 : {                     /* rgb */
                   ((mng_bkgdp)*ppChunk)->iType  = 2;
                   ((mng_bkgdp)*ppChunk)->iRed   = mng_get_uint16 (pRawdata);
                   ((mng_bkgdp)*ppChunk)->iGreen = mng_get_uint16 (pRawdata+2);
                   ((mng_bkgdp)*ppChunk)->iBlue  = mng_get_uint16 (pRawdata+4);
                   break;
                 }
      }
    }
  }
#endif /* MNG_STORE_CHUNKS */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_BKGD, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* done */
}

/* ************************************************************************** */

READ_CHUNK (read_phys)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_PHYS, MNG_LC_START)
#endif
                                       /* sequence checks */
#ifdef MNG_INCLUDE_JNG
  if ((!pData->bHasMHDR) && (!pData->bHasIHDR) &&
      (!pData->bHasBASI) && (!pData->bHasDHDR) && (!pData->bHasJHDR))
#else
  if ((!pData->bHasMHDR) && (!pData->bHasIHDR) &&
      (!pData->bHasBASI) && (!pData->bHasDHDR)    )
#endif
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

#ifdef MNG_INCLUDE_JNG
  if ((pData->bHasIDAT) || (pData->bHasJDAT) || (pData->bHasJDAA))
#else
  if (pData->bHasIDAT)
#endif
    MNG_ERROR (pData, MNG_SEQUENCEERROR)
                                       /* it's 9 bytes or empty; no more, no less! */
  if ((iRawlen != 9) && (iRawlen != 0))
    MNG_ERROR (pData, MNG_INVALIDLENGTH)

#ifdef MNG_SUPPORT_DISPLAY
  {


    /* TODO: something !!! */


  }
#endif /* MNG_SUPPORT_DISPLAY */

#ifdef MNG_STORE_CHUNKS
  if (pData->bStorechunks)
  {                                    /* initialize storage */
    mng_retcode iRetcode = ((mng_chunk_headerp)pHeader)->fCreate (pData, pHeader, ppChunk);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
                                       /* store the fields */
    ((mng_physp)*ppChunk)->bEmpty = (mng_bool)(iRawlen == 0);

    if (iRawlen)
    {
      ((mng_physp)*ppChunk)->iSizex = mng_get_uint32 (pRawdata);
      ((mng_physp)*ppChunk)->iSizey = mng_get_uint32 (pRawdata+4);
      ((mng_physp)*ppChunk)->iUnit  = *(pRawdata+8);
    }
  }
#endif /* MNG_STORE_CHUNKS */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_PHYS, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* done */
}

/* ************************************************************************** */

READ_CHUNK (read_sbit)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_SBIT, MNG_LC_START)
#endif
                                       /* sequence checks */
#ifdef MNG_INCLUDE_JNG
  if ((!pData->bHasMHDR) && (!pData->bHasIHDR) &&
      (!pData->bHasBASI) && (!pData->bHasDHDR) && (!pData->bHasJHDR))
#else
  if ((!pData->bHasMHDR) && (!pData->bHasIHDR) &&
      (!pData->bHasBASI) && (!pData->bHasDHDR)    )
#endif
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

#ifdef MNG_INCLUDE_JNG
  if ((pData->bHasPLTE) || (pData->bHasIDAT) || (pData->bHasJDAT) || (pData->bHasJDAA))
#else
  if ((pData->bHasPLTE) || (pData->bHasIDAT))
#endif
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

  if (iRawlen > 4)                     /* it just can't be bigger than that! */
    MNG_ERROR (pData, MNG_INVALIDLENGTH)

#ifdef MNG_INCLUDE_JNG                 /* length checks */
  if (pData->bHasJHDR)
  {
    if ((pData->iJHDRcolortype ==  8) && (iRawlen != 1))
      MNG_ERROR (pData, MNG_INVALIDLENGTH)

    if ((pData->iJHDRcolortype == 10) && (iRawlen != 3))
      MNG_ERROR (pData, MNG_INVALIDLENGTH)

    if ((pData->iJHDRcolortype == 12) && (iRawlen != 2))
      MNG_ERROR (pData, MNG_INVALIDLENGTH)

    if ((pData->iJHDRcolortype == 14) && (iRawlen != 4))
      MNG_ERROR (pData, MNG_INVALIDLENGTH)
  }
  else
#endif /* MNG_INCLUDE_JNG */
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR))
  {
    if ((pData->iColortype == 0) && (iRawlen != 1))
      MNG_ERROR (pData, MNG_INVALIDLENGTH)

    if ((pData->iColortype == 2) && (iRawlen != 3))
      MNG_ERROR (pData, MNG_INVALIDLENGTH)

    if ((pData->iColortype == 3) && (iRawlen != 3))
      MNG_ERROR (pData, MNG_INVALIDLENGTH)

    if ((pData->iColortype == 4) && (iRawlen != 2))
      MNG_ERROR (pData, MNG_INVALIDLENGTH)

    if ((pData->iColortype == 6) && (iRawlen != 4))
      MNG_ERROR (pData, MNG_INVALIDLENGTH)
  }
  else
  {                                    /* global = empty or RGBA */
    if ((iRawlen != 0) && (iRawlen != 4))
      MNG_ERROR (pData, MNG_INVALIDLENGTH)
  }

#ifdef MNG_SUPPORT_DISPLAY
  {


    /* TODO: something !!! */


  }
#endif /* MNG_SUPPORT_DISPLAY */

#ifdef MNG_STORE_CHUNKS
  if (pData->bStorechunks)
  {                                    /* initialize storage */
    mng_retcode iRetcode = ((mng_chunk_headerp)pHeader)->fCreate (pData, pHeader, ppChunk);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
                                       /* store the fields */
    ((mng_sbitp)*ppChunk)->bEmpty = (mng_bool)(iRawlen == 0);

    if (iRawlen)
    {
#ifdef MNG_INCLUDE_JNG
      if (pData->bHasJHDR)
        ((mng_sbitp)*ppChunk)->iType = pData->iJHDRcolortype;
      else
#endif
      if (pData->bHasIHDR)
        ((mng_sbitp)*ppChunk)->iType = pData->iColortype;
      else                             /* global ! */
        ((mng_sbitp)*ppChunk)->iType = 6;

      if (iRawlen > 0)
        ((mng_sbitp)*ppChunk)->aBits [0] = *pRawdata;
      if (iRawlen > 1)
        ((mng_sbitp)*ppChunk)->aBits [1] = *(pRawdata+1);
      if (iRawlen > 2)
        ((mng_sbitp)*ppChunk)->aBits [2] = *(pRawdata+2);
      if (iRawlen > 3)
        ((mng_sbitp)*ppChunk)->aBits [3] = *(pRawdata+3);

    }
  }
#endif /* MNG_STORE_CHUNKS */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_SBIT, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* done */
}

/* ************************************************************************** */

READ_CHUNK (read_splt)
{
  mng_uint8p pTemp;
  mng_uint32 iNamelen;
  mng_uint8  iSampledepth;
  mng_uint32 iRemain;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_SPLT, MNG_LC_START)
#endif
                                       /* sequence checks */
  if ((!pData->bHasMHDR) && (!pData->bHasIHDR) &&
      (!pData->bHasBASI) && (!pData->bHasDHDR)    )
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

  if (pData->bHasIDAT)
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

  if (iRawlen)
  {
    pTemp = find_null (pRawdata);      /* find null-separator */
                                       /* not found inside input-data ? */
    if ((pTemp - pRawdata) > (mng_int32)iRawlen)
      MNG_ERROR (pData, MNG_NULLNOTFOUND)

    iNamelen     = (mng_uint32)(pTemp - pRawdata);
    iSampledepth = *(pTemp+1);
    iRemain      = (iRawlen - 2 - iNamelen);

    if ((iSampledepth != 1) && (iSampledepth != 2))
      MNG_ERROR (pData, MNG_INVSAMPLEDEPTH)
                                       /* check remaining length */
    if ( ((iSampledepth == 1) && (iRemain %  6 != 0)) ||
         ((iSampledepth == 2) && (iRemain % 10 != 0))    )
      MNG_ERROR (pData, MNG_INVALIDLENGTH)

  }
  else
  {
    pTemp        = MNG_NULL;
    iNamelen     = 0;
    iSampledepth = 0;
    iRemain      = 0;
  }

#ifdef MNG_SUPPORT_DISPLAY
  {


    /* TODO: something !!! */


  }
#endif /* MNG_SUPPORT_DISPLAY */

#ifdef MNG_STORE_CHUNKS
  if (pData->bStorechunks)
  {                                    /* initialize storage */
    mng_retcode iRetcode = ((mng_chunk_headerp)pHeader)->fCreate (pData, pHeader, ppChunk);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
                                       /* store the fields */
    ((mng_spltp)*ppChunk)->bEmpty = (mng_bool)(iRawlen == 0);

    if (iRawlen)
    {
      ((mng_spltp)*ppChunk)->iNamesize    = iNamelen;
      ((mng_spltp)*ppChunk)->iSampledepth = iSampledepth;

      if (iSampledepth == 1)
        ((mng_spltp)*ppChunk)->iEntrycount = iRemain / 6;
      else
        ((mng_spltp)*ppChunk)->iEntrycount = iRemain / 10;

      if (iNamelen)
      {
        MNG_ALLOC (pData, ((mng_spltp)*ppChunk)->zName, iNamelen+1)
        MNG_COPY (((mng_spltp)*ppChunk)->zName, pRawdata, iNamelen)
      }

      if (iRemain)
      {
        MNG_ALLOC (pData, ((mng_spltp)*ppChunk)->pEntries, iRemain)
        MNG_COPY (((mng_spltp)*ppChunk)->pEntries, pTemp+2, iRemain)
      }
    }
  }
#endif /* MNG_STORE_CHUNKS */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_SPLT, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* done */
}

/* ************************************************************************** */

READ_CHUNK (read_hist)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_HIST, MNG_LC_START)
#endif
                                       /* sequence checks */
  if ((!pData->bHasIHDR) && (!pData->bHasBASI) && (!pData->bHasDHDR)    )
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

  if ((!pData->bHasPLTE) || (pData->bHasIDAT))
    MNG_ERROR (pData, MNG_SEQUENCEERROR)
                                       /* length oke ? */
  if ( ((iRawlen & 0x01) != 0) || ((iRawlen >> 1) != pData->iPLTEcount) )
    MNG_ERROR (pData, MNG_INVALIDLENGTH)

#ifdef MNG_SUPPORT_DISPLAY
  {


    /* TODO: something !!! */


  }
#endif /* MNG_SUPPORT_DISPLAY */

#ifdef MNG_STORE_CHUNKS
  if (pData->bStorechunks)
  {
    mng_uint32 iX;
                                       /* initialize storage */
    mng_retcode iRetcode = ((mng_chunk_headerp)pHeader)->fCreate (pData, pHeader, ppChunk);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
                                       /* store the fields */
    ((mng_histp)*ppChunk)->iEntrycount = iRawlen >> 1;

    for (iX = 0; iX < (iRawlen >> 1); iX++)
    {
      ((mng_histp)*ppChunk)->aEntries [iX] = mng_get_uint16 (pRawdata);
      pRawdata += 2;
    }
  }
#endif /* MNG_STORE_CHUNKS */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_HIST, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* done */
}

/* ************************************************************************** */

READ_CHUNK (read_time)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_TIME, MNG_LC_START)
#endif
                                       /* sequence checks */
#ifdef MNG_INCLUDE_JNG
  if ((!pData->bHasMHDR) && (!pData->bHasIHDR) &&
      (!pData->bHasBASI) && (!pData->bHasDHDR) && (!pData->bHasJHDR))
#else
  if ((!pData->bHasMHDR) && (!pData->bHasIHDR) &&
      (!pData->bHasBASI) && (!pData->bHasDHDR)    )
#endif
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

  if (iRawlen != 7)                    /* length must be exactly 7 */
    MNG_ERROR (pData, MNG_INVALIDLENGTH)

/*  if (pData->fProcesstime) */            /* inform the application ? */
/*  {

    pData->fProcesstime ((mng_handle)pData, );
  } */

#ifdef MNG_STORE_CHUNKS
  if (pData->bStorechunks)
  {                                    /* initialize storage */
    mng_retcode iRetcode = ((mng_chunk_headerp)pHeader)->fCreate (pData, pHeader, ppChunk);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
                                       /* store the fields */
    ((mng_timep)*ppChunk)->iYear   = mng_get_uint16 (pRawdata);
    ((mng_timep)*ppChunk)->iMonth  = *(pRawdata+2);
    ((mng_timep)*ppChunk)->iDay    = *(pRawdata+3);
    ((mng_timep)*ppChunk)->iHour   = *(pRawdata+4);
    ((mng_timep)*ppChunk)->iMinute = *(pRawdata+5);
    ((mng_timep)*ppChunk)->iSecond = *(pRawdata+6);
  }
#endif /* MNG_STORE_CHUNKS */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_TIME, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* done */
}

/* ************************************************************************** */

READ_CHUNK (read_mhdr)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_MHDR, MNG_LC_START)
#endif

  if (pData->eSigtype != mng_it_mng)   /* sequence checks */
    MNG_ERROR (pData, MNG_CHUNKNOTALLOWED)

  if (pData->bHasheader)               /* can only be the first chunk! */
    MNG_ERROR (pData, MNG_SEQUENCEERROR)
                                       /* correct length ? */
  if ((iRawlen != 28) && (iRawlen != 12))
    MNG_ERROR (pData, MNG_INVALIDLENGTH);

  pData->bHasMHDR       = MNG_TRUE;    /* oh boy, a real MNG */
  pData->bHasheader     = MNG_TRUE;    /* we've got a header */
  pData->eImagetype     = mng_it_mng;  /* fill header fields */
  pData->iWidth         = mng_get_uint32 (pRawdata);
  pData->iHeight        = mng_get_uint32 (pRawdata+4);
  pData->iTicks         = mng_get_uint32 (pRawdata+8);

  if (iRawlen == 28)                   /* proper MHDR ? */
  {
    pData->iLayercount  = mng_get_uint32 (pRawdata+12);
    pData->iFramecount  = mng_get_uint32 (pRawdata+16);
    pData->iPlaytime    = mng_get_uint32 (pRawdata+20);
    pData->iSimplicity  = mng_get_uint32 (pRawdata+24);

    pData->bPreDraft48  = MNG_FALSE;
  }
  else                                 /* probably pre-draft48 then */
  {
    pData->iLayercount  = 0;
    pData->iFramecount  = 0;
    pData->iPlaytime    = 0;
    pData->iSimplicity  = 0;

    pData->bPreDraft48  = MNG_TRUE;
  }
                                       /* predict alpha-depth */
  if ((pData->iSimplicity & 0x00000001) == 0)
    pData->iAlphadepth = 16;           /* no indicators = assume the worst */
  else
  if ((pData->iSimplicity & 0x00000008) == 0)
    pData->iAlphadepth = 0;            /* no transparency at all */
  else
  if ((pData->iSimplicity & 0x00000140) == 0x00000040)
    pData->iAlphadepth = 1;            /* no semi-transparency guaranteed */
  else
    pData->iAlphadepth = 16;           /* anything else = assume the worst */

#ifdef MNG_INCLUDE_JNG                 /* can we handle the complexity ? */
  if (pData->iSimplicity & 0x0000FC00)
#else
  if (pData->iSimplicity & 0x0000FC10)
#endif
    MNG_ERROR (pData, MNG_MNGTOOCOMPLEX)
                                       /* fits on maximum canvas ? */
  if ((pData->iWidth > pData->iMaxwidth) || (pData->iHeight > pData->iMaxheight))
    MNG_WARNING (pData, MNG_IMAGETOOLARGE)

  if (pData->fProcessheader)           /* inform the app ? */
    if (!pData->fProcessheader (((mng_handle)pData), pData->iWidth, pData->iHeight))
      MNG_ERROR (pData, MNG_APPMISCERROR)

  pData->iImagelevel++;                /* one level deeper */

#ifdef MNG_STORE_CHUNKS
  if (pData->bStorechunks)
  {                                    /* initialize storage */
    mng_retcode iRetcode = ((mng_chunk_headerp)pHeader)->fCreate (pData, pHeader, ppChunk);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
                                       /* store the fields */
    ((mng_mhdrp)*ppChunk)->iWidth      = pData->iWidth;
    ((mng_mhdrp)*ppChunk)->iHeight     = pData->iHeight;
    ((mng_mhdrp)*ppChunk)->iTicks      = pData->iTicks;
    ((mng_mhdrp)*ppChunk)->iLayercount = pData->iLayercount;
    ((mng_mhdrp)*ppChunk)->iFramecount = pData->iFramecount;
    ((mng_mhdrp)*ppChunk)->iPlaytime   = pData->iPlaytime;
    ((mng_mhdrp)*ppChunk)->iSimplicity = pData->iSimplicity;
  }
#endif /* MNG_STORE_CHUNKS */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_MHDR, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* done */
}

/* ************************************************************************** */

READ_CHUNK (read_mend)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_MEND, MNG_LC_START)
#endif

  if (!pData->bHasMHDR)                /* sequence checks */
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

  if (iRawlen > 0)                     /* must not contain data! */
    MNG_ERROR (pData, MNG_INVALIDLENGTH)

#ifdef MNG_SUPPORT_DISPLAY
  {                                    /* do something */
    mng_retcode iRetcode = process_display_mend (pData);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
  }
#endif /* MNG_SUPPORT_DISPLAY */

  pData->bHasMHDR = MNG_FALSE;         /* end of the line, bro! */

#ifdef MNG_STORE_CHUNKS
  if (pData->bStorechunks)
  {                                    /* initialize storage */
    mng_retcode iRetcode = ((mng_chunk_headerp)pHeader)->fCreate (pData, pHeader, ppChunk);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
  }
#endif /* MNG_STORE_CHUNKS */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_MEND, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* done */
}

/* ************************************************************************** */

READ_CHUNK (read_loop)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_LOOP, MNG_LC_START)
#endif

  if (!pData->bHasMHDR)                /* sequence checks */
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

  if (!pData->bCacheplayback)          /* must store playback info to work!! */
    MNG_ERROR (pData, MNG_LOOPWITHCACHEOFF)

#ifdef MNG_INCLUDE_JNG
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR) || (pData->bHasJHDR))
#else
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR))
#endif
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

  if (iRawlen >= 5)                    /* length checks */
  {
    if (iRawlen >= 6)
    {
      if ((iRawlen - 6) % 4 != 0)
        MNG_ERROR (pData, MNG_INVALIDLENGTH)
    }
  }
  else
    MNG_ERROR (pData, MNG_INVALIDLENGTH)

#ifdef MNG_SUPPORT_DISPLAY
  {
    mng_uint8   iLevel;
    mng_uint32  iRepeat;
    mng_uint8   iTermination = 0;
    mng_uint32  iItermin     = 1;
    mng_uint32  iItermax     = 0x7fffffffL;
    mng_retcode iRetcode;

    pData->bHasLOOP = MNG_TRUE;        /* indicate we're inside a loop */

    iLevel = *pRawdata;                /* determine the fields for processing */

    if (pData->bPreDraft48)
    {
      iTermination = *(pRawdata+1);

      iRepeat = mng_get_uint32 (pRawdata+2);
    }
    else
      iRepeat = mng_get_uint32 (pRawdata+1);

    if (iRawlen >= 6)
    {
      if (!pData->bPreDraft48)
        iTermination = *(pRawdata+5);

      if (iRawlen >= 10)
      {
        iItermin = mng_get_uint32 (pRawdata+6);

        if (iRawlen >= 14)
        {
          iItermax = mng_get_uint32 (pRawdata+10);

          /* TODO: process signals */

        }
      }
    }

                                       /* create the LOOP ani-object */
    iRetcode = create_ani_loop (pData, iLevel, iRepeat, iTermination,
                                       iItermin, iItermax, 0, 0);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
  }
#endif /* MNG_SUPPORT_DISPLAY */

#ifdef MNG_STORE_CHUNKS
  if (pData->bStorechunks)
  {                                    /* initialize storage */
    mng_retcode iRetcode = ((mng_chunk_headerp)pHeader)->fCreate (pData, pHeader, ppChunk);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;

    if (iRawlen >= 5)                  /* store the fields */
    {
      ((mng_loopp)*ppChunk)->iLevel  = *pRawdata;

      if (pData->bPreDraft48)
      {
        ((mng_loopp)*ppChunk)->iTermination = *(pRawdata+1);
        ((mng_loopp)*ppChunk)->iRepeat = mng_get_uint32 (pRawdata+2);
      }
      else
      {
        ((mng_loopp)*ppChunk)->iRepeat = mng_get_uint32 (pRawdata+1);
      }

      if (iRawlen >= 6)
      {
        if (!pData->bPreDraft48)
          ((mng_loopp)*ppChunk)->iTermination = *(pRawdata+5);

        if (iRawlen >= 10)
        {
          ((mng_loopp)*ppChunk)->iItermin = mng_get_uint32 (pRawdata+6);

          if (iRawlen >= 14)
          {
            ((mng_loopp)*ppChunk)->iItermax = mng_get_uint32 (pRawdata+10);
            ((mng_loopp)*ppChunk)->iCount   = (iRawlen - 14) / 4;

            if (((mng_loopp)*ppChunk)->iCount)
            {
              MNG_ALLOC (pData, ((mng_loopp)*ppChunk)->pSignals,
                                ((mng_loopp)*ppChunk)->iCount << 2)

#ifndef MNG_BIGENDIAN_SUPPORTED
              {
                mng_uint32  iX;
                mng_uint8p  pIn  = pRawdata + 14;
                mng_uint32p pOut = (mng_uint32p)((mng_loopp)*ppChunk)->pSignals;

                for (iX = 0; iX < ((mng_loopp)*ppChunk)->iCount; iX++)
                {
                  *pOut++ = mng_get_uint32 (pIn);
                  pIn += 4;
                }
              }
#else
              MNG_COPY (((mng_loopp)*ppChunk)->pSignals, pRawdata + 14,
                        ((mng_loopp)*ppChunk)->iCount << 2)
#endif /* !MNG_BIGENDIAN_SUPPORTED */
            }
          }
        }
      }
    }
  }
#endif /* MNG_STORE_CHUNKS */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_LOOP, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* done */
}

/* ************************************************************************** */

READ_CHUNK (read_endl)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_ENDL, MNG_LC_START)
#endif

  if (!pData->bHasMHDR)                /* sequence checks */
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

#ifdef MNG_INCLUDE_JNG
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR) || (pData->bHasJHDR))
#else
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR))
#endif
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

  if (iRawlen != 1)                    /* length must be exactly 1 */
    MNG_ERROR (pData, MNG_INVALIDLENGTH)

#ifdef MNG_SUPPORT_DISPLAY
  {
    if (pData->bHasLOOP)               /* are we really processing a loop ? */
    {
      mng_uint8 iLevel = *pRawdata;    /* get the nest level */
                                       /* create an ENDL animation object */
      mng_retcode iRetcode = create_ani_endl (pData, iLevel);

      if (iRetcode)                    /* on error bail out */
        return iRetcode;

      {                                /* process it */
        mng_ani_endlp pENDL = (mng_ani_endlp)pData->pLastaniobj;

        iRetcode = pENDL->sHeader.fProcess (pData, pENDL);

        if (iRetcode)                  /* on error bail out */
          return iRetcode;
      }
    }
    else
    {

      /* TODO: error abort ??? */

    }
  }
#endif /* MNG_SUPPORT_DISPLAY */

#ifdef MNG_STORE_CHUNKS
  if (pData->bStorechunks)
  {                                    /* initialize storage */
    mng_retcode iRetcode = ((mng_chunk_headerp)pHeader)->fCreate (pData, pHeader, ppChunk);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
                                       /* store the fields */
    ((mng_endlp)*ppChunk)->iLevel = *pRawdata;
  }
#endif /* MNG_STORE_CHUNKS */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_ENDL, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* done */
}

/* ************************************************************************** */

READ_CHUNK (read_defi)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_DEFI, MNG_LC_START)
#endif

  if (!pData->bHasMHDR)                /* sequence checks */
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

#ifdef MNG_INCLUDE_JNG
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR) || (pData->bHasJHDR))
#else
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR))
#endif
    MNG_ERROR (pData, MNG_SEQUENCEERROR)
                                       /* check the length */
  if ((iRawlen != 2) && (iRawlen != 3) && (iRawlen != 4) &&
      (iRawlen != 12) && (iRawlen != 28))
    MNG_ERROR (pData, MNG_INVALIDLENGTH)

#ifdef MNG_SUPPORT_DISPLAY
  {
    mng_retcode iRetcode;

    pData->iDEFIobjectid       = mng_get_uint16 (pRawdata);

    if (iRawlen > 2)
    {
      pData->bDEFIhasdonotshow = MNG_TRUE;
      pData->iDEFIdonotshow    = *(pRawdata+2);
    }
    else
    {
      pData->bDEFIhasdonotshow = MNG_FALSE;
      pData->iDEFIdonotshow    = 0;
    }

    if (iRawlen > 3)
    {
      pData->bDEFIhasconcrete  = MNG_TRUE;
      pData->iDEFIconcrete     = *(pRawdata+3);
    }
    else
    {
      pData->bDEFIhasconcrete  = MNG_FALSE;
      pData->iDEFIconcrete     = 0;
    }

    if (iRawlen > 4)
    {
      pData->bDEFIhasloca      = MNG_TRUE;
      pData->iDEFIlocax        = mng_get_int32 (pRawdata+4);
      pData->iDEFIlocay        = mng_get_int32 (pRawdata+8);
    }
    else
    {
      pData->bDEFIhasloca      = MNG_FALSE;
      pData->iDEFIlocax        = 0;
      pData->iDEFIlocay        = 0;
    }

    if (iRawlen > 12)
    {
      pData->bDEFIhasclip      = MNG_TRUE;
      pData->iDEFIclipl        = mng_get_int32 (pRawdata+12);
      pData->iDEFIclipr        = mng_get_int32 (pRawdata+16);
      pData->iDEFIclipt        = mng_get_int32 (pRawdata+20);
      pData->iDEFIclipb        = mng_get_int32 (pRawdata+24);
    }
    else
    {
      pData->bDEFIhasclip      = MNG_FALSE;
      pData->iDEFIclipl        = 0;
      pData->iDEFIclipr        = 0;
      pData->iDEFIclipt        = 0;
      pData->iDEFIclipb        = 0;
    }
                                       /* create an animation object */
    iRetcode = create_ani_defi (pData);

    if (!iRetcode)                     /* do display processing */
      iRetcode = process_display_defi (pData);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;

  }
#endif /* MNG_SUPPORT_DISPLAY */

#ifdef MNG_STORE_CHUNKS
  if (pData->bStorechunks)
  {                                    /* initialize storage */
    mng_retcode iRetcode = ((mng_chunk_headerp)pHeader)->fCreate (pData, pHeader, ppChunk);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
                                       /* store the fields */
    ((mng_defip)*ppChunk)->iObjectid       = mng_get_uint16 (pRawdata);

    if (iRawlen > 2)
    {
      ((mng_defip)*ppChunk)->bHasdonotshow = MNG_TRUE;
      ((mng_defip)*ppChunk)->iDonotshow    = *(pRawdata+2);
    }
    else
      ((mng_defip)*ppChunk)->bHasdonotshow = MNG_FALSE;

    if (iRawlen > 3)
    {
      ((mng_defip)*ppChunk)->bHasconcrete  = MNG_TRUE;
      ((mng_defip)*ppChunk)->iConcrete     = *(pRawdata+3);
    }
    else
      ((mng_defip)*ppChunk)->bHasconcrete  = MNG_FALSE;

    if (iRawlen > 4)
    {
      ((mng_defip)*ppChunk)->bHasloca      = MNG_TRUE;
      ((mng_defip)*ppChunk)->iXlocation    = mng_get_int32 (pRawdata+4);
      ((mng_defip)*ppChunk)->iYlocation    = mng_get_int32 (pRawdata+8);
    }
    else
      ((mng_defip)*ppChunk)->bHasloca      = MNG_FALSE;

    if (iRawlen > 12)
    {
      ((mng_defip)*ppChunk)->bHasclip      = MNG_TRUE;
      ((mng_defip)*ppChunk)->iLeftcb       = mng_get_int32 (pRawdata+12);
      ((mng_defip)*ppChunk)->iRightcb      = mng_get_int32 (pRawdata+16);
      ((mng_defip)*ppChunk)->iTopcb        = mng_get_int32 (pRawdata+20);
      ((mng_defip)*ppChunk)->iBottomcb     = mng_get_int32 (pRawdata+24);
    }
    else
      ((mng_defip)*ppChunk)->bHasclip      = MNG_FALSE;

  }
#endif /* MNG_STORE_CHUNKS */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_DEFI, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* done */
}

/* ************************************************************************** */

READ_CHUNK (read_basi)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_BASI, MNG_LC_START)
#endif

  if (!pData->bHasMHDR)                /* sequence checks */
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

#ifdef MNG_INCLUDE_JNG
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR) || (pData->bHasJHDR))
#else
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR))
#endif
    MNG_ERROR (pData, MNG_SEQUENCEERROR)
                                       /* check the length */
  if ((iRawlen != 13) && (iRawlen != 19) && (iRawlen != 21) && (iRawlen != 22))
    MNG_ERROR (pData, MNG_INVALIDLENGTH)

  pData->bHasBASI     = MNG_TRUE;      /* inside a BASI-IEND block now */
                                       /* store interesting fields */
  pData->iDatawidth   = mng_get_uint32 (pRawdata);
  pData->iDataheight  = mng_get_uint32 (pRawdata+4);
  pData->iBitdepth    = *(pRawdata+8);
  pData->iColortype   = *(pRawdata+9);
  pData->iCompression = *(pRawdata+10);
  pData->iFilter      = *(pRawdata+11);
  pData->iInterlace   = *(pRawdata+12);

  if ((pData->iBitdepth !=  1) &&      /* parameter validity checks */
      (pData->iBitdepth !=  2) &&
      (pData->iBitdepth !=  4) &&
      (pData->iBitdepth !=  8) &&
      (pData->iBitdepth != 16)    )
    MNG_ERROR (pData, MNG_INVALIDBITDEPTH)

  if ((pData->iColortype != MNG_COLORTYPE_GRAY   ) &&
      (pData->iColortype != MNG_COLORTYPE_RGB    ) &&
      (pData->iColortype != MNG_COLORTYPE_INDEXED) &&
      (pData->iColortype != MNG_COLORTYPE_GRAYA  ) &&
      (pData->iColortype != MNG_COLORTYPE_RGBA   )    )
    MNG_ERROR (pData, MNG_INVALIDCOLORTYPE)

  if ((pData->iColortype == MNG_COLORTYPE_INDEXED) && (pData->iBitdepth > 8))
    MNG_ERROR (pData, MNG_INVALIDBITDEPTH)

  if (((pData->iColortype == MNG_COLORTYPE_RGB    ) ||
       (pData->iColortype == MNG_COLORTYPE_GRAYA  ) ||
       (pData->iColortype == MNG_COLORTYPE_RGBA   )    ) &&
      (pData->iBitdepth < 8                            )    )
    MNG_ERROR (pData, MNG_INVALIDBITDEPTH)

  if (pData->iCompression != MNG_COMPRESSION_DEFLATE)
    MNG_ERROR (pData, MNG_INVALIDCOMPRESS)

  if (pData->iFilter & (~MNG_FILTER_DIFFERING))
    MNG_ERROR (pData, MNG_INVALIDFILTER)

  if ((pData->iInterlace != MNG_INTERLACE_NONE ) &&
      (pData->iInterlace != MNG_INTERLACE_ADAM7)    )
    MNG_ERROR (pData, MNG_INVALIDINTERLACE)

  pData->iImagelevel++;                /* one level deeper */

#ifdef MNG_SUPPORT_DISPLAY
  {
    mng_uint16  iRed      = 0;
    mng_uint16  iGreen    = 0;
    mng_uint16  iBlue     = 0;
    mng_bool    bHasalpha = MNG_FALSE;
    mng_uint16  iAlpha    = 0xFFFF;
    mng_uint8   iViewable = 0;
    mng_retcode iRetcode;

    if (iRawlen > 13)                  /* get remaining fields, if any */
    {
      iRed      = mng_get_uint16 (pRawdata+13);
      iGreen    = mng_get_uint16 (pRawdata+15);
      iBlue     = mng_get_uint16 (pRawdata+17);
    }

    if (iRawlen > 19)
    {
      bHasalpha = MNG_TRUE;
      iAlpha    = mng_get_uint16 (pRawdata+19);
    }

    if (iRawlen > 21)
      iViewable = *(pRawdata+21);
                                       /* create an animation object */
    iRetcode = create_ani_basi (pData, iRed, iGreen, iBlue,
                                bHasalpha, iAlpha, iViewable);

    if (!iRetcode)                     /* display-processing... */
      iRetcode = process_display_basi (pData, iRed, iGreen, iBlue,
                                       bHasalpha, iAlpha, iViewable);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;

  }
#endif /* MNG_SUPPORT_DISPLAY */

#ifdef MNG_STORE_CHUNKS
  if (pData->bStorechunks)
  {                                    /* initialize storage */
    mng_retcode iRetcode = ((mng_chunk_headerp)pHeader)->fCreate (pData, pHeader, ppChunk);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
                                       /* store the fields */
    ((mng_basip)*ppChunk)->iWidth       = mng_get_uint32 (pRawdata);
    ((mng_basip)*ppChunk)->iHeight      = mng_get_uint32 (pRawdata+4);
    ((mng_basip)*ppChunk)->iBitdepth    = *(pRawdata+8);
    ((mng_basip)*ppChunk)->iColortype   = *(pRawdata+9);
    ((mng_basip)*ppChunk)->iCompression = *(pRawdata+10);
    ((mng_basip)*ppChunk)->iFilter      = *(pRawdata+11);
    ((mng_basip)*ppChunk)->iInterlace   = *(pRawdata+12);

    if (iRawlen > 13)
    {
      ((mng_basip)*ppChunk)->iRed       = mng_get_uint16 (pRawdata+13);
      ((mng_basip)*ppChunk)->iGreen     = mng_get_uint16 (pRawdata+15);
      ((mng_basip)*ppChunk)->iBlue      = mng_get_uint16 (pRawdata+17);
    }

    if (iRawlen > 19)
      ((mng_basip)*ppChunk)->iAlpha     = mng_get_uint16 (pRawdata+19);

    if (iRawlen > 21)
      ((mng_basip)*ppChunk)->iViewable  = *(pRawdata+21);

  }
#endif /* MNG_STORE_CHUNKS */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_BASI, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* done */
}

/* ************************************************************************** */

READ_CHUNK (read_clon)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_CLON, MNG_LC_START)
#endif

  if (!pData->bHasMHDR)                /* sequence checks */
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

#ifdef MNG_INCLUDE_JNG
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR) || (pData->bHasJHDR))
#else
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR))
#endif
    MNG_ERROR (pData, MNG_SEQUENCEERROR)
                                       /* check the length */
  if ((iRawlen != 4) && (iRawlen != 5) && (iRawlen != 6) &&
      (iRawlen != 7) && (iRawlen != 16))
    MNG_ERROR (pData, MNG_INVALIDLENGTH)

#ifdef MNG_SUPPORT_DISPLAY
  {
    mng_uint16  iSourceid, iCloneid;
    mng_uint8   iClonetype    = 0;
    mng_bool    bHasdonotshow = MNG_FALSE;
    mng_uint8   iDonotshow    = 0;
    mng_uint8   iConcrete     = 0;
    mng_bool    bHasloca      = MNG_FALSE;
    mng_uint8   iLocationtype = 0;
    mng_int32   iLocationx    = 0;
    mng_int32   iLocationy    = 0;
    mng_retcode iRetcode;

    iSourceid       = mng_get_uint16 (pRawdata);
    iCloneid        = mng_get_uint16 (pRawdata+2);

    if (iRawlen > 4)
      iClonetype    = *(pRawdata+4);

    if (iRawlen > 5)
    {
      bHasdonotshow = MNG_TRUE;
      iDonotshow    = *(pRawdata+5);
    }

    if (iRawlen > 6)
      iConcrete     = *(pRawdata+6);

    if (iRawlen > 7)
    {
      bHasloca      = MNG_TRUE;
      iLocationtype = *(pRawdata+7);
      iLocationx    = mng_get_int32 (pRawdata+8);
      iLocationy    = mng_get_int32 (pRawdata+12);
    }

    iRetcode = create_ani_clon (pData, iSourceid, iCloneid, iClonetype,
                                bHasdonotshow, iDonotshow, iConcrete,
                                bHasloca, iLocationtype, iLocationx, iLocationy);

    if (!iRetcode)                     /* do display processing */
      iRetcode = process_display_clon (pData, iSourceid, iCloneid, iClonetype,
                                       bHasdonotshow, iDonotshow, iConcrete,
                                       bHasloca, iLocationtype, iLocationx, iLocationy);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;

  }
#endif /* MNG_SUPPORT_DISPLAY */

#ifdef MNG_STORE_CHUNKS
  if (pData->bStorechunks)
  {                                    /* initialize storage */
    mng_retcode iRetcode = ((mng_chunk_headerp)pHeader)->fCreate (pData, pHeader, ppChunk);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
                                       /* store the fields */
    ((mng_clonp)*ppChunk)->iSourceid       = mng_get_uint16 (pRawdata);
    ((mng_clonp)*ppChunk)->iCloneid        = mng_get_uint16 (pRawdata+2);

    if (iRawlen > 4)
      ((mng_clonp)*ppChunk)->iClonetype    = *(pRawdata+4);

    if (iRawlen > 5)
      ((mng_clonp)*ppChunk)->iDonotshow    = *(pRawdata+5);

    if (iRawlen > 6)
      ((mng_clonp)*ppChunk)->iConcrete     = *(pRawdata+6);

    if (iRawlen > 7)
    {
      ((mng_clonp)*ppChunk)->bHasloca      = MNG_TRUE;
      ((mng_clonp)*ppChunk)->iLocationtype = *(pRawdata+7);
      ((mng_clonp)*ppChunk)->iLocationx    = mng_get_int32 (pRawdata+8);
      ((mng_clonp)*ppChunk)->iLocationy    = mng_get_int32 (pRawdata+12);
    }
    else
    {
      ((mng_clonp)*ppChunk)->bHasloca      = MNG_FALSE;
    }
  }
#endif /* MNG_STORE_CHUNKS */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_CLON, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* done */
}

/* ************************************************************************** */

READ_CHUNK (read_past)
{
#ifdef MNG_STORE_CHUNKS
  mng_uint32 iCount;
#endif

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_PAST, MNG_LC_START)
#endif

  if (!pData->bHasMHDR)                /* sequence checks */
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

#ifdef MNG_INCLUDE_JNG
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR) || (pData->bHasJHDR))
#else
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR))
#endif
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

                                       /* check the length */
  if ((iRawlen < 41) || (((iRawlen - 11) % 30) != 0))
    MNG_ERROR (pData, MNG_INVALIDLENGTH)

#ifdef MNG_STORE_CHUNKS
  iCount = ((iRawlen - 11) / 30);      /* how many entries again */
#endif

#ifdef MNG_SUPPORT_DISPLAY
  {


    /* TODO: something !!! */


  }
#endif /* MNG_SUPPORT_DISPLAY */

#ifdef MNG_STORE_CHUNKS
  if (pData->bStorechunks)
  {
    mng_uint32       iX;
    mng_past_sourcep pSource;
                                       /* initialize storage */
    mng_retcode iRetcode = ((mng_chunk_headerp)pHeader)->fCreate (pData, pHeader, ppChunk);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
                                       /* store the fields */
    ((mng_pastp)*ppChunk)->iDestid     = mng_get_uint16 (pRawdata);
    ((mng_pastp)*ppChunk)->iTargettype = *(pRawdata+2);
    ((mng_pastp)*ppChunk)->iTargetx    = mng_get_int32  (pRawdata+3);
    ((mng_pastp)*ppChunk)->iTargety    = mng_get_int32  (pRawdata+7);
    ((mng_pastp)*ppChunk)->iCount      = iCount;

    pRawdata += 11;
                                       /* get a buffer for all the source blocks */
    MNG_ALLOC (pData, ((mng_pastp)*ppChunk)->pSources, (iCount * sizeof (mng_past_source)))

    pSource = ((mng_pastp)*ppChunk)->pSources;

    for (iX = 0; iX < iCount; iX++)    /* now copy the source blocks */
    {
      pSource->iSourceid     = mng_get_uint16 (pRawdata);
      pSource->iComposition  = *(pRawdata+2);
      pSource->iOrientation  = *(pRawdata+3);
      pSource->iOffsettype   = *(pRawdata+4);
      pSource->iOffsetx      = mng_get_int32 (pRawdata+5);
      pSource->iOffsety      = mng_get_int32 (pRawdata+9);
      pSource->iBoundarytype = *(pRawdata+13);
      pSource->iBoundaryl    = mng_get_int32 (pRawdata+14);
      pSource->iBoundaryr    = mng_get_int32 (pRawdata+18);
      pSource->iBoundaryt    = mng_get_int32 (pRawdata+22);
      pSource->iBoundaryb    = mng_get_int32 (pRawdata+26);

      pSource  += sizeof (mng_past_source);
      pRawdata += 30;
    }
  }
#endif /* MNG_STORE_CHUNKS */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_PAST, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* done */
}

/* ************************************************************************** */

READ_CHUNK (read_disc)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_DISC, MNG_LC_START)
#endif

  if (!pData->bHasMHDR)                /* sequence checks */
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

#ifdef MNG_INCLUDE_JNG
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR) || (pData->bHasJHDR))
#else
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR))
#endif
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

  if ((iRawlen % 2) != 0)              /* check the length */
    MNG_ERROR (pData, MNG_INVALIDLENGTH)

#ifdef MNG_SUPPORT_DISPLAY
  {                                    /* process it */
    mng_retcode iRetcode = process_display_disc (pData, (iRawlen / 2),
                                                 (mng_uint16p)pRawdata);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;

  }
#endif /* MNG_SUPPORT_DISPLAY */

#ifdef MNG_STORE_CHUNKS
  if (pData->bStorechunks)
  {                                    /* initialize storage */
    mng_retcode iRetcode = ((mng_chunk_headerp)pHeader)->fCreate (pData, pHeader, ppChunk);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
                                       /* store the fields */
    ((mng_discp)*ppChunk)->iCount = iRawlen / 2;

    MNG_ALLOC (pData, ((mng_discp)*ppChunk)->pObjectids, iRawlen)

#ifndef MNG_BIGENDIAN_SUPPORTED
    {
      mng_uint32  iX;
      mng_uint8p  pIn  = pRawdata;
      mng_uint16p pOut = ((mng_discp)*ppChunk)->pObjectids;

      for (iX = 0; iX < ((mng_discp)*ppChunk)->iCount; iX++)
      {
        *pOut++ = mng_get_uint16 (pIn);
        pIn += 2;
      }
    }
#else
    MNG_COPY (((mng_discp)*ppChunk)->pObjectids, pRawdata, iRawlen)
#endif /* !MNG_BIGENDIAN_SUPPORTED */
  }
#endif /* MNG_STORE_CHUNKS */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_DISC, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* done */
}

/* ************************************************************************** */

READ_CHUNK (read_back)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_BACK, MNG_LC_START)
#endif

  if (!pData->bHasMHDR)                /* sequence checks */
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

#ifdef MNG_INCLUDE_JNG
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR) || (pData->bHasJHDR))
#else
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR))
#endif
    MNG_ERROR (pData, MNG_SEQUENCEERROR)
                                       /* check the length */
  if ((iRawlen != 6) && (iRawlen != 7) && (iRawlen != 9) && (iRawlen != 10))
    MNG_ERROR (pData, MNG_INVALIDLENGTH)

#ifdef MNG_SUPPORT_DISPLAY
  {
    mng_retcode iRetcode;
                                       /* retrieve the fields */
    pData->bHasBACK         = MNG_TRUE;
    pData->iBACKred         = mng_get_uint16 (pRawdata);
    pData->iBACKgreen       = mng_get_uint16 (pRawdata+2);
    pData->iBACKblue        = mng_get_uint16 (pRawdata+4);

    if (iRawlen > 6)
      pData->iBACKmandatory = *(pRawdata+6);
    else
      pData->iBACKmandatory = 0;

    if (iRawlen > 7)
      pData->iBACKimageid   = mng_get_uint16 (pRawdata+7);
    else
      pData->iBACKimageid   = 0;

    if (iRawlen > 9)
      pData->iBACKtile      = *(pRawdata+9);
    else
      pData->iBACKtile      = 0;

    iRetcode = create_ani_back (pData, pData->iBACKred, pData->iBACKgreen,
                                pData->iBACKblue, pData->iBACKmandatory,
                                pData->iBACKimageid, pData->iBACKtile);

    if (iRetcode)                    /* on error bail out */
      return iRetcode;
  }
#endif /* MNG_SUPPORT_DISPLAY */

#ifdef MNG_STORE_CHUNKS
  if (pData->bStorechunks)
  {                                    /* initialize storage */
    mng_retcode iRetcode = ((mng_chunk_headerp)pHeader)->fCreate (pData, pHeader, ppChunk);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
                                       /* store the fields */
    ((mng_backp)*ppChunk)->iRed         = mng_get_uint16 (pRawdata);
    ((mng_backp)*ppChunk)->iGreen       = mng_get_uint16 (pRawdata+2);
    ((mng_backp)*ppChunk)->iBlue        = mng_get_uint16 (pRawdata+4);

    if (iRawlen > 6)
      ((mng_backp)*ppChunk)->iMandatory = *(pRawdata+6);

    if (iRawlen > 7)
      ((mng_backp)*ppChunk)->iImageid   = mng_get_uint16 (pRawdata+7);

    if (iRawlen > 9)
      ((mng_backp)*ppChunk)->iTile      = *(pRawdata+9);

  }
#endif /* MNG_STORE_CHUNKS */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_BACK, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* done */
}

/* ************************************************************************** */

READ_CHUNK (read_fram)
{
  mng_uint8p pTemp;
#ifdef MNG_STORE_CHUNKS
  mng_uint32 iNamelen;
#endif
  mng_uint32 iRemain;
  mng_uint32 iRequired = 0;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_FRAM, MNG_LC_START)
#endif

  if (!pData->bHasMHDR)                /* sequence checks */
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

#ifdef MNG_INCLUDE_JNG
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR) || (pData->bHasJHDR))
#else
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR))
#endif
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

  if (iRawlen <= 1)                    /* only framing-mode ? */
  {
#ifdef MNG_STORE_CHUNKS
    iNamelen = 0;                      /* indicate so */
#endif
    iRemain  = 0;
    pTemp    = MNG_NULL;
  }
  else
  {
    pTemp = find_null (pRawdata+1);    /* find null-separator */
                                       /* not found inside input-data ? */
    if ((pTemp - pRawdata) > (mng_int32)iRawlen)
      MNG_ERROR (pData, MNG_NULLNOTFOUND)

#ifdef MNG_STORE_CHUNKS
    iNamelen = (mng_uint32)((pTemp - pRawdata) - 1);
#endif
    iRemain  = (mng_uint32)(iRawlen - (pTemp - pRawdata) - 1);
                                       /* remains must be empty or at least 4 bytes */
    if ((iRemain != 0) && (iRemain < 4))
      MNG_ERROR (pData, MNG_INVALIDLENGTH)

    if (iRemain)
    {
      iRequired = 4;                   /* calculate and check required remaining length */

      if (*(pTemp+1)) { iRequired += 4; }
      if (*(pTemp+2)) { iRequired += 4; }
      if (*(pTemp+3)) { iRequired += 17; }

      if (*(pTemp+4))
      {
        if ((iRemain - iRequired) % 4 != 0)
          MNG_ERROR (pData, MNG_INVALIDLENGTH)
      }
      else
      {
        if (iRemain != iRequired)
          MNG_ERROR (pData, MNG_INVALIDLENGTH)
      }
    }
  }

#ifdef MNG_SUPPORT_DISPLAY
  {
    mng_uint8p  pWork           = pTemp;
    mng_uint8   iFramemode      = 0;
    mng_uint8   iChangedelay    = 0;
    mng_uint32  iDelay          = 0;
    mng_uint8   iChangetimeout  = 0;
    mng_uint32  iTimeout        = 0;
    mng_uint8   iChangeclipping = 0;
    mng_uint8   iCliptype       = 0;
    mng_int32   iClipl          = 0;
    mng_int32   iClipr          = 0;
    mng_int32   iClipt          = 0;
    mng_int32   iClipb          = 0;
    mng_retcode iRetcode;

    if (iRawlen)                       /* any data specified ? */
    {
      if (*(pRawdata))                 /* save the new framing mode ? */
      {
        iFramemode = *(pRawdata);

        if (pData->bPreDraft48)        /* old style input-stream ? */
        {
          switch (iFramemode)
          {
            case  0: { break; }
            case  1: { iFramemode = 3; break; }
            case  2: { iFramemode = 4; break; }
            case  3: { iFramemode = 1; break; }
            case  4: { iFramemode = 1; break; }
            case  5: { iFramemode = 2; break; }
            default: { iFramemode = 1; break; }
          }
        }
      }

      if (iRemain)
      {
        iChangedelay    = *(pWork+1);
        iChangetimeout  = *(pWork+2);
        iChangeclipping = *(pWork+3);
        pWork += 5;

        if (iChangedelay)              /* delay changed ? */
        {
          iDelay = mng_get_uint32 (pWork);
          pWork += 4;
        }

        if (iChangetimeout)            /* timeout changed ? */
        {
          iTimeout = mng_get_uint32 (pWork);
          pWork += 4;
        }

        if (iChangeclipping)           /* clipping changed ? */
        {
          iCliptype = *pWork;
          iClipl    = mng_get_int32 (pWork+1);
          iClipr    = mng_get_int32 (pWork+5);
          iClipt    = mng_get_int32 (pWork+9);
          iClipb    = mng_get_int32 (pWork+13);
        }
      }
    }

    iRetcode = create_ani_fram (pData, iFramemode, iChangedelay, iDelay,
                                iChangetimeout, iTimeout,
                                iChangeclipping, iCliptype,
                                iClipl, iClipr, iClipt, iClipb);

    if (!iRetcode)                     /* now go and do something */
      iRetcode = process_display_fram (pData, iFramemode, iChangedelay, iDelay,
                                       iChangetimeout, iTimeout,
                                       iChangeclipping, iCliptype,
                                       iClipl, iClipr, iClipt, iClipb);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;

  }
#endif /* MNG_SUPPORT_DISPLAY */

#ifdef MNG_STORE_CHUNKS
  if (pData->bStorechunks)
  {                                    /* initialize storage */
    mng_retcode iRetcode = ((mng_chunk_headerp)pHeader)->fCreate (pData, pHeader, ppChunk);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
                                       /* store the fields */
    ((mng_framp)*ppChunk)->bEmpty              = (mng_bool)(iRawlen == 0);

    if (iRawlen)
    {
      mng_uint8 iFramemode = *(pRawdata);

      if (pData->bPreDraft48)          /* old style input-stream ? */
      {
        switch (iFramemode)
        {
          case  1: { iFramemode = 3; break; }
          case  2: { iFramemode = 4; break; }
          case  3: { iFramemode = 5; break; }    /* TODO: provision for mode=5 ??? */
          case  4: { iFramemode = 1; break; }
          case  5: { iFramemode = 2; break; }
          default: { iFramemode = 1; break; }
        }
      }

      ((mng_framp)*ppChunk)->iMode             = iFramemode;
      ((mng_framp)*ppChunk)->iNamesize         = iNamelen;

      if (iNamelen)
      {
        MNG_ALLOC (pData, ((mng_framp)*ppChunk)->zName, iNamelen+1)
        MNG_COPY (((mng_framp)*ppChunk)->zName, pRawdata+1, iNamelen)
      }

      if (iRemain)
      {
        ((mng_framp)*ppChunk)->iChangedelay    = *(pTemp+1);
        ((mng_framp)*ppChunk)->iChangetimeout  = *(pTemp+2);
        ((mng_framp)*ppChunk)->iChangeclipping = *(pTemp+3);
        ((mng_framp)*ppChunk)->iChangesyncid   = *(pTemp+4);

        pTemp += 5;

        if (((mng_framp)*ppChunk)->iChangedelay)
        {
          ((mng_framp)*ppChunk)->iDelay        = mng_get_uint32 (pTemp);
          pTemp += 4;
        }

        if (((mng_framp)*ppChunk)->iChangetimeout)
        {
          ((mng_framp)*ppChunk)->iTimeout      = mng_get_uint32 (pTemp);
          pTemp += 4;
        }

        if (((mng_framp)*ppChunk)->iChangeclipping)
        {
          ((mng_framp)*ppChunk)->iBoundarytype = *pTemp;
          ((mng_framp)*ppChunk)->iBoundaryl    = mng_get_int32 (pTemp+1);
          ((mng_framp)*ppChunk)->iBoundaryr    = mng_get_int32 (pTemp+5);
          ((mng_framp)*ppChunk)->iBoundaryt    = mng_get_int32 (pTemp+9);
          ((mng_framp)*ppChunk)->iBoundaryb    = mng_get_int32 (pTemp+13);
          pTemp += 17;
        }

        if (((mng_framp)*ppChunk)->iChangesyncid)
        {
          ((mng_framp)*ppChunk)->iCount        = (iRemain - iRequired) / 4;

          if (((mng_framp)*ppChunk)->iCount)
          {
            MNG_ALLOC (pData, ((mng_framp)*ppChunk)->pSyncids,
                              ((mng_framp)*ppChunk)->iCount * 4);

#ifndef MNG_BIGENDIAN_SUPPORTED
            {
              mng_uint32 iX;
              mng_uint32p pOut = ((mng_framp)*ppChunk)->pSyncids;

              for (iX = 0; iX < ((mng_framp)*ppChunk)->iCount; iX++)
              {
                *pOut++ = mng_get_uint32 (pTemp);
                pTemp += 4;
              }
            }
#else
            MNG_COPY (((mng_framp)*ppChunk)->pSyncids, pTemp,
                      ((mng_framp)*ppChunk)->iCount * 4)
#endif /* !MNG_BIGENDIAN_SUPPORTED */
          }
        }
      }
    }
  }
#endif /* MNG_STORE_CHUNKS */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_FRAM, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* done */
}

/* ************************************************************************** */

READ_CHUNK (read_move)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_MOVE, MNG_LC_START)
#endif

  if (!pData->bHasMHDR)                /* sequence checks */
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

#ifdef MNG_INCLUDE_JNG
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR) || (pData->bHasJHDR))
#else
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR))
#endif
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

  if (iRawlen != 13)                   /* check the length */
    MNG_ERROR (pData, MNG_INVALIDLENGTH)

#ifdef MNG_SUPPORT_DISPLAY
  {
    mng_retcode iRetcode;
                                       /* create a MOVE animation object */
    iRetcode = create_ani_move (pData, mng_get_uint16 (pRawdata),
                                       mng_get_uint16 (pRawdata+2),
                                       *(pRawdata+4),
                                       mng_get_int32 (pRawdata+5),
                                       mng_get_int32 (pRawdata+9));

    if (!iRetcode)                     /* process the move */
      iRetcode = process_display_move (pData,
                                       mng_get_uint16 (pRawdata),
                                       mng_get_uint16 (pRawdata+2),
                                       *(pRawdata+4),
                                       mng_get_int32 (pRawdata+5),
                                       mng_get_int32 (pRawdata+9));

    if (iRetcode)                      /* on error bail out */
      return iRetcode;

  }
#endif /* MNG_SUPPORT_DISPLAY */

#ifdef MNG_STORE_CHUNKS
  if (pData->bStorechunks)
  {                                    /* initialize storage */
    mng_retcode iRetcode = ((mng_chunk_headerp)pHeader)->fCreate (pData, pHeader, ppChunk);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
                                       /* store the fields */
    ((mng_movep)*ppChunk)->iFirstid  = mng_get_uint16 (pRawdata);
    ((mng_movep)*ppChunk)->iLastid   = mng_get_uint16 (pRawdata+2);
    ((mng_movep)*ppChunk)->iMovetype = *(pRawdata+4);
    ((mng_movep)*ppChunk)->iMovex    = mng_get_int32 (pRawdata+5);
    ((mng_movep)*ppChunk)->iMovey    = mng_get_int32 (pRawdata+9);
  }
#endif /* MNG_STORE_CHUNKS */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_MOVE, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* done */
}

/* ************************************************************************** */

READ_CHUNK (read_clip)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_CLIP, MNG_LC_START)
#endif

  if (!pData->bHasMHDR)                /* sequence checks */
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

#ifdef MNG_INCLUDE_JNG
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR) || (pData->bHasJHDR))
#else
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR))
#endif
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

  if (iRawlen != 21)                   /* check the length */
    MNG_ERROR (pData, MNG_INVALIDLENGTH)

#ifdef MNG_SUPPORT_DISPLAY
  {
    mng_retcode iRetcode;
                                       /* create a CLIP animation object */
    iRetcode = create_ani_clip (pData, mng_get_uint16 (pRawdata),
                                       mng_get_uint16 (pRawdata+2),
                                       *(pRawdata+4),
                                       mng_get_int32 (pRawdata+5),
                                       mng_get_int32 (pRawdata+9),
                                       mng_get_int32 (pRawdata+13),
                                       mng_get_int32 (pRawdata+17));

    if (!iRetcode)                     /* process the clipping */
      iRetcode = process_display_clip (pData,
                                       mng_get_uint16 (pRawdata),
                                       mng_get_uint16 (pRawdata+2),
                                       *(pRawdata+4),
                                       mng_get_int32 (pRawdata+5),
                                       mng_get_int32 (pRawdata+9),
                                       mng_get_int32 (pRawdata+13),
                                       mng_get_int32 (pRawdata+17));

    if (iRetcode)                      /* on error bail out */
      return iRetcode;

  }
#endif /* MNG_SUPPORT_DISPLAY */

#ifdef MNG_STORE_CHUNKS
  if (pData->bStorechunks)
  {                                    /* initialize storage */
    mng_retcode iRetcode = ((mng_chunk_headerp)pHeader)->fCreate (pData, pHeader, ppChunk);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
                                       /* store the fields */
    ((mng_clipp)*ppChunk)->iFirstid  = mng_get_uint16 (pRawdata);
    ((mng_clipp)*ppChunk)->iLastid   = mng_get_uint16 (pRawdata+2);
    ((mng_clipp)*ppChunk)->iCliptype = *(pRawdata+4);
    ((mng_clipp)*ppChunk)->iClipl    = mng_get_int32 (pRawdata+5);
    ((mng_clipp)*ppChunk)->iClipr    = mng_get_int32 (pRawdata+9);
    ((mng_clipp)*ppChunk)->iClipt    = mng_get_int32 (pRawdata+13);
    ((mng_clipp)*ppChunk)->iClipb    = mng_get_int32 (pRawdata+17);
  }
#endif /* MNG_STORE_CHUNKS */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_CLIP, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* done */
}

/* ************************************************************************** */

READ_CHUNK (read_show)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_SHOW, MNG_LC_START)
#endif

  if (!pData->bHasMHDR)                /* sequence checks */
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

#ifdef MNG_INCLUDE_JNG
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR) || (pData->bHasJHDR))
#else
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR))
#endif
    MNG_ERROR (pData, MNG_SEQUENCEERROR)
                                       /* check the length */
  if ((iRawlen != 0) && (iRawlen != 2) && (iRawlen != 4) && (iRawlen != 5))
    MNG_ERROR (pData, MNG_INVALIDLENGTH)

#ifdef MNG_SUPPORT_DISPLAY
  {
    mng_retcode iRetcode;

    if (iRawlen)                       /* determine parameters if any */
    {
      pData->iSHOWfromid = mng_get_uint16 (pRawdata);

      if (iRawlen > 2)
        pData->iSHOWtoid = mng_get_uint16 (pRawdata+2);
      else
        pData->iSHOWtoid = pData->iSHOWfromid;

      if (iRawlen > 4)
        pData->iSHOWmode = *(pRawdata+4);
      else
        pData->iSHOWmode = 0;
    }
    else                               /* use defaults then */
    {
      pData->iSHOWmode   = 2;
      pData->iSHOWfromid = 1;
      pData->iSHOWtoid   = 65535;
    }
                                       /* create a SHOW animation object */
    iRetcode = create_ani_show (pData, pData->iSHOWfromid, pData->iSHOWtoid,
                                       pData->iSHOWmode);

    if (!iRetcode)                     /* go and do it! */
      iRetcode = process_display_show (pData);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;

  }
#endif /* MNG_SUPPORT_DISPLAY */

#ifdef MNG_STORE_CHUNKS
  if (pData->bStorechunks)
  {                                    /* initialize storage */
    mng_retcode iRetcode = ((mng_chunk_headerp)pHeader)->fCreate (pData, pHeader, ppChunk);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
                                       /* store the fields */
    ((mng_showp)*ppChunk)->bEmpty      = (mng_bool)(iRawlen == 0);

    if (iRawlen)
    {
      ((mng_showp)*ppChunk)->iFirstid  = mng_get_uint16 (pRawdata);

      if (iRawlen > 2)
        ((mng_showp)*ppChunk)->iLastid = mng_get_uint16 (pRawdata+2);

      if (iRawlen > 4)
        ((mng_showp)*ppChunk)->iMode   = *(pRawdata+4);
    }
  }
#endif /* MNG_STORE_CHUNKS */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_SHOW, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* done */
}

/* ************************************************************************** */

READ_CHUNK (read_term)
{
  mng_uint8   iTermaction;
  mng_uint8   iIteraction = 0;
  mng_uint32  iDelay      = 0;
  mng_uint32  iItermax    = 0;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_TERM, MNG_LC_START)
#endif

  if (!pData->bHasMHDR)                /* sequence checks */
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

#ifdef MNG_INCLUDE_JNG
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR) || (pData->bHasJHDR))
#else
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR))
#endif
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

  if (pData->bHasLOOP)                 /* no way, jose! */
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

  if (pData->bHasTERM)                 /* only 1 allowed! */
    MNG_ERROR (pData, MNG_MULTIPLEERROR)
                                       /* check the length */
  if ((iRawlen != 1) && (iRawlen != 10))
    MNG_ERROR (pData, MNG_INVALIDLENGTH)

  pData->bHasTERM = MNG_TRUE;
                                       /* TODO: remove in 1.0.0 !!! */
  if ((!pData->bHasSAVE) && (pData->iChunkseq > 2))
    pData->bEMNGMAhack = MNG_TRUE;

  iTermaction = *pRawdata;             /* get the fields */

  if (iRawlen > 1)
  {
    iIteraction = *(pRawdata+1);
    iDelay      = mng_get_uint32 (pRawdata+2);
    iItermax    = mng_get_uint32 (pRawdata+6);
  }

  if (pData->fProcessterm)             /* inform the app ? */
    if (!pData->fProcessterm (((mng_handle)pData), iTermaction, iIteraction,
                                                   iDelay, iItermax))
      MNG_ERROR (pData, MNG_APPMISCERROR)

#ifdef MNG_SUPPORT_DISPLAY
  {                                    /* create the TERM ani-object */
    mng_retcode iRetcode = create_ani_term (pData, iTermaction, iIteraction,
                                                   iDelay, iItermax);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
                                       /* save for future reference */
    pData->pTermaniobj = pData->pLastaniobj;
  }
#endif /* MNG_SUPPORT_DISPLAY */

#ifdef MNG_STORE_CHUNKS
  if (pData->bStorechunks)
  {                                    /* initialize storage */
    mng_retcode iRetcode = ((mng_chunk_headerp)pHeader)->fCreate (pData, pHeader, ppChunk);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
                                       /* store the fields */
    ((mng_termp)*ppChunk)->iTermaction = iTermaction;
    ((mng_termp)*ppChunk)->iIteraction = iIteraction;
    ((mng_termp)*ppChunk)->iDelay      = iDelay;
    ((mng_termp)*ppChunk)->iItermax    = iItermax;
  }
#endif /* MNG_STORE_CHUNKS */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_TERM, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* done */
}

/* ************************************************************************** */

READ_CHUNK (read_save)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_SAVE, MNG_LC_START)
#endif
                                       /* sequence checks */
  if ((!pData->bHasMHDR) || (pData->bHasSAVE))
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

#ifdef MNG_INCLUDE_JNG
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR) || (pData->bHasJHDR))
#else
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR))
#endif
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

  pData->bHasSAVE = MNG_TRUE;

  if (pData->fProcesssave)             /* inform the application ? */
  {
    mng_bool bOke = pData->fProcesssave ((mng_handle)pData);

    if (!bOke)
      MNG_ERROR (pData, MNG_APPMISCERROR)
  }

#ifdef MNG_SUPPORT_DISPLAY
  {
    mng_retcode iRetcode;


    /* TODO: something with the parameters */


                                       /* create a SAVE animation object */
    iRetcode = create_ani_save (pData);

    if (!iRetcode)                     /* process it */
      iRetcode = process_display_save (pData);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
      
  }
#endif /* MNG_SUPPORT_DISPLAY */

#ifdef MNG_STORE_CHUNKS
  if (pData->bStorechunks)
  {                                    /* initialize storage */
    mng_retcode iRetcode = ((mng_chunk_headerp)pHeader)->fCreate (pData, pHeader, ppChunk);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
                                       /* store the fields */
    ((mng_savep)*ppChunk)->bEmpty = (mng_bool)(iRawlen == 0);

    if (iRawlen)                       /* not empty ? */
    {
      mng_uint8       iOtype = *pRawdata;
      mng_uint8       iEtype;
      mng_uint32      iCount = 0;
      mng_uint8p      pTemp;
      mng_uint8p      pNull;
      mng_uint32      iLen;
      mng_uint32      iOffset[2];
      mng_uint32      iStarttime[2];
      mng_uint32      iFramenr;
      mng_uint32      iLayernr;
      mng_uint32      iX;
      mng_save_entryp pEntry = MNG_NULL;
      mng_uint32      iNamesize;

      if ((iOtype != 4) && (iOtype != 8))
        MNG_ERROR (pData, MNG_INVOFFSETSIZE);

      ((mng_savep)*ppChunk)->iOffsettype = iOtype;

      for (iX = 0; iX < 2; iX++)       /* do this twice to get the count first ! */
      {
        pTemp = pRawdata + 1;
        iLen  = iRawlen  - 1;

        if (iX)                        /* second run ? */
        {
          MNG_ALLOC (pData, pEntry, (iCount * sizeof (mng_save_entry)))

          ((mng_savep)*ppChunk)->iCount   = iCount;
          ((mng_savep)*ppChunk)->pEntries = pEntry;
        }

        while (iLen)                   /* anything left ? */
        {
          iEtype = *pTemp;             /* entrytype */

          if ((iEtype != 0) && (iEtype != 1) && (iEtype != 2) && (iEtype != 3))
            MNG_ERROR (pData, MNG_INVENTRYTYPE);

          pTemp++;

          if (iEtype > 1)
          {
            iOffset    [0] = 0;
            iOffset    [1] = 0;
            iStarttime [0] = 0;
            iStarttime [1] = 0;
            iLayernr       = 0;
            iFramenr       = 0;
          }
          else
          {
            if (iOtype == 4)
            {
              iOffset [0] = 0;
              iOffset [1] = mng_get_uint32 (pTemp);

              pTemp += 4;
            }
            else
            {
              iOffset [0] = mng_get_uint32 (pTemp);
              iOffset [1] = mng_get_uint32 (pTemp+4);

              pTemp += 8;
            }

            if (iEtype > 0)
            {
              iStarttime [0] = 0;
              iStarttime [1] = 0;
              iLayernr       = 0;
              iFramenr       = 0;
            }
            else
            {
              if (iOtype == 4)
              {
                iStarttime [0] = 0;
                iStarttime [1] = mng_get_uint32 (pTemp+0);
                iLayernr       = mng_get_uint32 (pTemp+4);
                iFramenr       = mng_get_uint32 (pTemp+8);

                pTemp += 12;
              }
              else
              {
                iStarttime [0] = mng_get_uint32 (pTemp+0);
                iStarttime [1] = mng_get_uint32 (pTemp+4);
                iLayernr       = mng_get_uint32 (pTemp+8);
                iFramenr       = mng_get_uint32 (pTemp+12);

                pTemp += 16;
              }
            }
          }

          pNull = find_null (pTemp);   /* get the name length */

          if ((pNull - pRawdata) > (mng_int32)iRawlen)
          {
            iNamesize = iLen;          /* no null found; so end of SAVE */
            iLen      = 0;
          }
          else
          {
            iNamesize = pNull - pTemp; /* should be another entry */
            iLen     -= iNamesize;

            if (!iLen)                 /* must not end with a null ! */
              MNG_ERROR (pData, MNG_ENDWITHNULL)
          }

          if (!pEntry)
          {
            iCount++;
          }
          else
          {
            pEntry->iEntrytype     = iEtype;
            pEntry->iOffset    [0] = iOffset    [0];
            pEntry->iOffset    [1] = iOffset    [1];
            pEntry->iStarttime [0] = iStarttime [0];
            pEntry->iStarttime [1] = iStarttime [1];
            pEntry->iLayernr       = iLayernr;
            pEntry->iFramenr       = iFramenr;
            pEntry->iNamesize      = iNamesize;

            if (iNamesize)
            {
              MNG_ALLOC (pData, pEntry->zName, iNamesize+1)
              MNG_COPY (pEntry->zName, pTemp, iNamesize)
            }

            pEntry++;
          }

          pTemp += iNamesize;
        }
      }
    }
  }
#endif /* MNG_STORE_CHUNKS */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_SAVE, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* done */
}

/* ************************************************************************** */

READ_CHUNK (read_seek)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_SEEK, MNG_LC_START)
#endif
                                       /* sequence checks */
  if ((!pData->bHasMHDR) || (!pData->bHasSAVE))
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

#ifdef MNG_INCLUDE_JNG
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR) || (pData->bHasJHDR))
#else
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR))
#endif
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

  if (pData->fProcessseek)             /* inform the app ? */
  {
    mng_bool  bOke;
    mng_pchar zName;

    MNG_ALLOC (pData, zName, iRawlen + 1)

    if (iRawlen)
      MNG_COPY (zName, pRawdata, iRawlen)

    bOke = pData->fProcessseek ((mng_handle)pData, zName);

    MNG_FREEX (pData, zName, iRawlen + 1)

    if (!bOke)
      MNG_ERROR (pData, MNG_APPMISCERROR)
  }

#ifdef MNG_SUPPORT_DISPLAY
  {
    mng_retcode iRetcode;


    /* TODO: something with the name ??? */



                                       /* create a SEEK animation object */
    iRetcode = create_ani_seek (pData);

    if (!iRetcode)                     /* process it */
      iRetcode = process_display_seek (pData);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
  }
#endif /* MNG_SUPPORT_DISPLAY */

#ifdef MNG_STORE_CHUNKS
  if (pData->bStorechunks)
  {                                    /* initialize storage */
    mng_retcode iRetcode = ((mng_chunk_headerp)pHeader)->fCreate (pData, pHeader, ppChunk);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
                                       /* store the fields */
    ((mng_seekp)*ppChunk)->iNamesize = iRawlen;

    if (iRawlen)
    {
      MNG_ALLOC (pData, ((mng_seekp)*ppChunk)->zName, iRawlen+1)
      MNG_COPY (((mng_seekp)*ppChunk)->zName, pRawdata, iRawlen)
    }
  }
#endif /* MNG_STORE_CHUNKS */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_SEEK, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* done */
}

/* ************************************************************************** */

READ_CHUNK (read_expi)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_EXPI, MNG_LC_START)
#endif

  if (!pData->bHasMHDR)                /* sequence checks */
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

#ifdef MNG_INCLUDE_JNG
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR) || (pData->bHasJHDR))
#else
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR))
#endif
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

  if (iRawlen < 3)                     /* check the length */
    MNG_ERROR (pData, MNG_INVALIDLENGTH)

#ifdef MNG_SUPPORT_DISPLAY
  {


    /* TODO: something !!! */


  }
#endif /* MNG_SUPPORT_DISPLAY */

#ifdef MNG_STORE_CHUNKS
  if (pData->bStorechunks)
  {                                    /* initialize storage */
    mng_retcode iRetcode = ((mng_chunk_headerp)pHeader)->fCreate (pData, pHeader, ppChunk);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
                                       /* store the fields */
    ((mng_expip)*ppChunk)->iSnapshotid = mng_get_uint16 (pRawdata);
    ((mng_expip)*ppChunk)->iNamesize   = iRawlen - 2;

    if (((mng_expip)*ppChunk)->iNamesize)
    {
      MNG_ALLOC (pData, ((mng_expip)*ppChunk)->zName,
                        ((mng_expip)*ppChunk)->iNamesize + 1)
      MNG_COPY (((mng_expip)*ppChunk)->zName, pRawdata+2,
                ((mng_expip)*ppChunk)->iNamesize)
    }
  }
#endif /* MNG_STORE_CHUNKS */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_EXPI, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* done */
}

/* ************************************************************************** */

READ_CHUNK (read_fpri)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_FPRI, MNG_LC_START)
#endif

  if (!pData->bHasMHDR)                /* sequence checks */
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

#ifdef MNG_INCLUDE_JNG
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR) || (pData->bHasJHDR))
#else
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR))
#endif
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

  if (iRawlen != 2)                    /* must be two bytes long */
    MNG_ERROR (pData, MNG_INVALIDLENGTH)

#ifdef MNG_SUPPORT_DISPLAY
  {


    /* TODO: something !!! */


  }
#endif /* MNG_SUPPORT_DISPLAY */

#ifdef MNG_STORE_CHUNKS
  if (pData->bStorechunks)
  {                                    /* initialize storage */
    mng_retcode iRetcode = ((mng_chunk_headerp)pHeader)->fCreate (pData, pHeader, ppChunk);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
                                       /* store the fields */
    ((mng_fprip)*ppChunk)->iDeltatype = *pRawdata;
    ((mng_fprip)*ppChunk)->iPriority  = *(pRawdata+1);
  }
#endif /* MNG_STORE_CHUNKS */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_FPRI, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* done */
}

/* ************************************************************************** */

mng_bool CheckKeyword (mng_datap  pData,
                       mng_uint8p pKeyword)
{
  mng_chunkid handled_chunks [] =
  {
    MNG_UINT_BACK,
    MNG_UINT_BASI,
    MNG_UINT_CLIP,
    MNG_UINT_CLON,
/* TODO:    MNG_UINT_DBYK,  */
    MNG_UINT_DEFI,
    MNG_UINT_DHDR,
    MNG_UINT_DISC,
/* TODO:    MNG_UINT_DROP,  */
    MNG_UINT_ENDL,
    MNG_UINT_FRAM,
    MNG_UINT_IDAT,
    MNG_UINT_IEND,
    MNG_UINT_IHDR,
    MNG_UINT_IJNG,
    MNG_UINT_IPNG,
#ifdef MNG_INCLUDE_JNG
    MNG_UINT_JDAA,
    MNG_UINT_JDAT,
    MNG_UINT_JHDR,
/* TODO:    MNG_UINT_JSEP,  */
    MNG_UINT_JdAA,
#endif
    MNG_UINT_LOOP,
    MNG_UINT_MAGN,
    MNG_UINT_MEND,
    MNG_UINT_MHDR,
    MNG_UINT_MOVE,
/* TODO:    MNG_UINT_ORDR,  */
/* TODO:    MNG_UINT_PAST,  */
    MNG_UINT_PLTE,
    MNG_UINT_PPLT,
    MNG_UINT_PROM,
    MNG_UINT_SAVE,
    MNG_UINT_SEEK,
    MNG_UINT_SHOW,
    MNG_UINT_TERM,
    MNG_UINT_bKGD,
    MNG_UINT_cHRM,
/* TODO:    MNG_UINT_eXPI,  */
/* TODO:    MNG_UINT_fPRI,  */
    MNG_UINT_gAMA,
/* TODO:    MNG_UINT_hIST,  */
    MNG_UINT_iCCP,
    MNG_UINT_iTXt,
    MNG_UINT_nEED,
/* TODO:    MNG_UINT_oFFs,  */
/* TODO:    MNG_UINT_pCAL,  */
/* TODO:    MNG_UINT_pHYg,  */
/* TODO:    MNG_UINT_pHYs,  */
/* TODO:    MNG_UINT_sBIT,  */
/* TODO:    MNG_UINT_sCAL,  */
/* TODO:    MNG_UINT_sPLT,  */
    MNG_UINT_sRGB,
    MNG_UINT_tEXt,
    MNG_UINT_tIME,
    MNG_UINT_tRNS,
    MNG_UINT_zTXt,
  };

  mng_bool bOke = MNG_FALSE;

  if (pData->fProcessneed)             /* does the app handle it ? */
    bOke = pData->fProcessneed ((mng_handle)pData, (mng_pchar)pKeyword);

  if (!bOke)
  {                                    /* find the keyword length */
    mng_uint8p pNull = find_null (pKeyword);

    if (pNull - pKeyword == 4)         /* test a chunk ? */
    {                                  /* get the chunk-id */
      mng_chunkid iChunkid = (*pKeyword     << 24) + (*(pKeyword+1) << 16) +
                             (*(pKeyword+2) <<  8) + (*(pKeyword+3)      );
                                       /* binary search variables */
      mng_int32   iTop, iLower, iUpper, iMiddle;
                                       /* determine max index of table */
      iTop = (sizeof (handled_chunks) / sizeof (handled_chunks [0])) - 1;

      /* binary search; with 52 chunks, worst-case is 7 comparisons */
      iLower  = 0;
      iMiddle = iTop >> 1;
      iUpper  = iTop;

      do                                   /* the binary search itself */
        {
          if (handled_chunks [iMiddle] < iChunkid)
            iLower = iMiddle + 1;
          else if (handled_chunks [iMiddle] > iChunkid)
            iUpper = iMiddle - 1;
          else
          {
            bOke = MNG_TRUE;
            break;
          }

          iMiddle = (iLower + iUpper) >> 1;
        }
      while (iLower <= iUpper);
    }
                                       /* test draft ? */
    if ((!bOke) && (pNull - pKeyword == 8) &&
        (*pKeyword     == 'd') && (*(pKeyword+1) == 'r') &&
        (*(pKeyword+2) == 'a') && (*(pKeyword+3) == 'f') &&
        (*(pKeyword+4) == 't') && (*(pKeyword+5) == ' '))
    {
      mng_uint32 iDraft;

      iDraft = (*(pKeyword+6) - '0') * 10 + (*(pKeyword+7) - '0');
      bOke   = (mng_bool)(iDraft <= MNG_MNG_DRAFT);
    }
                                       /* test MNG 1.0 ? */
    if ((!bOke) && (pNull - pKeyword == 7) &&
        (*pKeyword     == 'M') && (*(pKeyword+1) == 'N') &&
        (*(pKeyword+2) == 'G') && (*(pKeyword+3) == '-') &&
        (*(pKeyword+4) == '1') && (*(pKeyword+5) == '.') &&
        (*(pKeyword+6) == '0'))
      bOke   = MNG_TRUE;

  }

  return bOke;
}

/* ************************************************************************** */

READ_CHUNK (read_need)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_NEED, MNG_LC_START)
#endif

  if (!pData->bHasMHDR)                /* sequence checks */
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

#ifdef MNG_INCLUDE_JNG
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR) || (pData->bHasJHDR))
#else
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR))
#endif
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

  if (iRawlen < 1)                     /* check the length */
    MNG_ERROR (pData, MNG_INVALIDLENGTH)

  {                                    /* let's check it */
    mng_bool   bOke = MNG_TRUE;
    mng_pchar  zKeywords;
    mng_uint8p pNull, pTemp;

    MNG_ALLOC (pData, zKeywords, iRawlen + 1)

    if (iRawlen)
      MNG_COPY (zKeywords, pRawdata, iRawlen)

    pTemp = (mng_uint8p)zKeywords;
    pNull = find_null (pTemp);

    while ((bOke) && (pNull < (mng_uint8p)zKeywords + iRawlen))
    {
      bOke  = CheckKeyword (pData, pTemp);
      pTemp = pNull + 1;
      pNull = find_null (pTemp);
    }

    if (bOke)
      bOke = CheckKeyword (pData, pTemp);

    MNG_FREEX (pData, zKeywords, iRawlen + 1)

    if (!bOke)
      MNG_ERROR (pData, MNG_UNSUPPORTEDNEED)
  }

#ifdef MNG_STORE_CHUNKS
  if (pData->bStorechunks)
  {                                    /* initialize storage */
    mng_retcode iRetcode = ((mng_chunk_headerp)pHeader)->fCreate (pData, pHeader, ppChunk);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
                                       /* store the fields */
    ((mng_needp)*ppChunk)->iKeywordssize = iRawlen;

    if (iRawlen)
    {
      MNG_ALLOC (pData, ((mng_needp)*ppChunk)->zKeywords, iRawlen+1)
      MNG_COPY (((mng_needp)*ppChunk)->zKeywords, pRawdata, iRawlen)
    }
  }
#endif /* MNG_STORE_CHUNKS */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_NEED, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* done */
}

/* ************************************************************************** */

READ_CHUNK (read_phyg)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_PHYG, MNG_LC_START)
#endif

  if (!pData->bHasMHDR)                /* sequence checks */
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

#ifdef MNG_INCLUDE_JNG
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR) || (pData->bHasJHDR))
#else
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR))
#endif
    MNG_ERROR (pData, MNG_SEQUENCEERROR)
                                       /* it's 9 bytes or empty; no more, no less! */
  if ((iRawlen != 9) && (iRawlen != 0))
    MNG_ERROR (pData, MNG_INVALIDLENGTH)

#ifdef MNG_SUPPORT_DISPLAY
  {


    /* TODO: something !!! */


  }
#endif /* MNG_SUPPORT_DISPLAY */

#ifdef MNG_STORE_CHUNKS
  if (pData->bStorechunks)
  {                                    /* initialize storage */
    mng_retcode iRetcode = ((mng_chunk_headerp)pHeader)->fCreate (pData, pHeader, ppChunk);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
                                       /* store the fields */
    ((mng_phygp)*ppChunk)->bEmpty = (mng_bool)(iRawlen == 0);

    if (iRawlen)
    {
      ((mng_phygp)*ppChunk)->iSizex = mng_get_uint32 (pRawdata);
      ((mng_phygp)*ppChunk)->iSizey = mng_get_uint32 (pRawdata+4);
      ((mng_phygp)*ppChunk)->iUnit  = *(pRawdata+8);
    }
  }
#endif /* MNG_STORE_CHUNKS */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_PHYG, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* done */
}

/* ************************************************************************** */

#ifdef MNG_INCLUDE_JNG
READ_CHUNK (read_jhdr)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_JHDR, MNG_LC_START)
#endif
                                       /* sequence checks */
  if ((pData->eSigtype != mng_it_jng) && (pData->eSigtype != mng_it_mng))
    MNG_ERROR (pData, MNG_CHUNKNOTALLOWED)

  if ((pData->eSigtype == mng_it_jng) && (pData->iChunkseq > 1))
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR) || (pData->bHasJHDR))
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

  if (iRawlen != 16)                   /* length oke ? */
    MNG_ERROR (pData, MNG_INVALIDLENGTH)
                                       /* inside a JHDR-IEND block now */
  pData->bHasJHDR              = MNG_TRUE;
                                       /* and store interesting fields */
  pData->iDatawidth            = mng_get_uint32 (pRawdata);
  pData->iDataheight           = mng_get_uint32 (pRawdata+4);
  pData->iJHDRcolortype        = *(pRawdata+8);
  pData->iJHDRimgbitdepth      = *(pRawdata+9);
  pData->iJHDRimgcompression   = *(pRawdata+10);
  pData->iJHDRimginterlace     = *(pRawdata+11);
  pData->iJHDRalphabitdepth    = *(pRawdata+12);
  pData->iJHDRalphacompression = *(pRawdata+13);
  pData->iJHDRalphafilter      = *(pRawdata+14);
  pData->iJHDRalphainterlace   = *(pRawdata+15);
                                       /* parameter validity checks */
  if ((pData->iJHDRcolortype != MNG_COLORTYPE_JPEGGRAY  ) &&
      (pData->iJHDRcolortype != MNG_COLORTYPE_JPEGCOLOR ) &&
      (pData->iJHDRcolortype != MNG_COLORTYPE_JPEGGRAYA ) &&
      (pData->iJHDRcolortype != MNG_COLORTYPE_JPEGCOLORA)    )
    MNG_ERROR (pData, MNG_INVALIDCOLORTYPE)

  if ((pData->iJHDRimgbitdepth !=  8) &&
      (pData->iJHDRimgbitdepth != 12) &&
      (pData->iJHDRimgbitdepth != 20)    )
    MNG_ERROR (pData, MNG_INVALIDBITDEPTH)

  if (pData->iJHDRimgcompression != MNG_COMPRESSION_BASELINEJPEG)
    MNG_ERROR (pData, MNG_INVALIDCOMPRESS)

  if ((pData->iJHDRimginterlace != MNG_INTERLACE_SEQUENTIAL ) &&
      (pData->iJHDRimginterlace != MNG_INTERLACE_PROGRESSIVE)    )
    MNG_ERROR (pData, MNG_INVALIDINTERLACE)

  if ((pData->iJHDRcolortype == MNG_COLORTYPE_JPEGGRAYA ) ||
      (pData->iJHDRcolortype == MNG_COLORTYPE_JPEGCOLORA)    )
  {
    if ((pData->iJHDRalphabitdepth !=  1) &&
        (pData->iJHDRalphabitdepth !=  2) &&
        (pData->iJHDRalphabitdepth !=  4) &&
        (pData->iJHDRalphabitdepth !=  8) &&
        (pData->iJHDRalphabitdepth != 16)    )
      MNG_ERROR (pData, MNG_INVALIDBITDEPTH)

    if ((pData->iJHDRalphacompression != MNG_COMPRESSION_DEFLATE     ) &&
        (pData->iJHDRalphacompression != MNG_COMPRESSION_BASELINEJPEG)    )
      MNG_ERROR (pData, MNG_INVALIDCOMPRESS)

    if ((pData->iJHDRalphacompression == MNG_COMPRESSION_BASELINEJPEG) &&
        (pData->iJHDRalphabitdepth    !=  8                          )    )
      MNG_ERROR (pData, MNG_INVALIDBITDEPTH)

    if (pData->iJHDRalphafilter & (~MNG_FILTER_DIFFERING))
      MNG_ERROR (pData, MNG_INVALIDFILTER)

    if ((pData->iJHDRalphainterlace != MNG_INTERLACE_NONE ) &&
        (pData->iJHDRalphainterlace != MNG_INTERLACE_ADAM7)    )
      MNG_ERROR (pData, MNG_INVALIDINTERLACE)

  }
  else
  {
    if (pData->iJHDRalphabitdepth    != 0)
      MNG_ERROR (pData, MNG_INVALIDBITDEPTH)

    if (pData->iJHDRalphacompression != 0)
      MNG_ERROR (pData, MNG_INVALIDCOMPRESS)

    if (pData->iJHDRalphafilter      != 0)
      MNG_ERROR (pData, MNG_INVALIDFILTER)

    if (pData->iJHDRalphainterlace   != 0)
      MNG_ERROR (pData, MNG_INVALIDINTERLACE)

  }

  if (!pData->bHasheader)              /* first chunk ? */
  {
    pData->bHasheader = MNG_TRUE;      /* we've got a header */
    pData->eImagetype = mng_it_jng;    /* then this must be a JNG */
    pData->iWidth     = mng_get_uint32 (pRawdata);
    pData->iHeight    = mng_get_uint32 (pRawdata+4);
                                       /* predict alpha-depth ! */
  if ((pData->iJHDRcolortype == MNG_COLORTYPE_JPEGGRAYA ) ||
      (pData->iJHDRcolortype == MNG_COLORTYPE_JPEGCOLORA)    )
      pData->iAlphadepth = pData->iJHDRalphabitdepth;
    else
      pData->iAlphadepth = 0;
                                       /* fits on maximum canvas ? */
    if ((pData->iWidth > pData->iMaxwidth) || (pData->iHeight > pData->iMaxheight))
      MNG_WARNING (pData, MNG_IMAGETOOLARGE)

    if (pData->fProcessheader)         /* inform the app ? */
      if (!pData->fProcessheader (((mng_handle)pData), pData->iWidth, pData->iHeight))
      MNG_ERROR (pData, MNG_APPMISCERROR)

  }

  pData->iColortype = 0;               /* fake grayscale for other routines */
  pData->iImagelevel++;                /* one level deeper */

#ifdef MNG_SUPPORT_DISPLAY
  {
    mng_retcode iRetcode = process_display_jhdr (pData);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
  }
#endif /* MNG_SUPPORT_DISPLAY */

#ifdef MNG_STORE_CHUNKS
  if (pData->bStorechunks)
  {                                    /* initialize storage */
    mng_retcode iRetcode = ((mng_chunk_headerp)pHeader)->fCreate (pData, pHeader, ppChunk);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
                                       /* store the fields */
    ((mng_jhdrp)*ppChunk)->iWidth            = mng_get_uint32 (pRawdata);
    ((mng_jhdrp)*ppChunk)->iHeight           = mng_get_uint32 (pRawdata+4);
    ((mng_jhdrp)*ppChunk)->iColortype        = *(pRawdata+8);
    ((mng_jhdrp)*ppChunk)->iImagesampledepth = *(pRawdata+9);
    ((mng_jhdrp)*ppChunk)->iImagecompression = *(pRawdata+10);
    ((mng_jhdrp)*ppChunk)->iImageinterlace   = *(pRawdata+11);
    ((mng_jhdrp)*ppChunk)->iAlphasampledepth = *(pRawdata+12);
    ((mng_jhdrp)*ppChunk)->iAlphacompression = *(pRawdata+13);
    ((mng_jhdrp)*ppChunk)->iAlphafilter      = *(pRawdata+14);
    ((mng_jhdrp)*ppChunk)->iAlphainterlace   = *(pRawdata+15);
  }
#endif /* MNG_STORE_CHUNKS */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_JHDR, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* done */
}
#else
#define read_jhdr 0
#endif /* MNG_INCLUDE_JNG */

/* ************************************************************************** */

#ifdef MNG_INCLUDE_JNG
READ_CHUNK (read_jdaa)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_JDAA, MNG_LC_START)
#endif
                                       /* sequence checks */
  if ((!pData->bHasJHDR) && (!pData->bHasDHDR))
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

  if (pData->bHasJSEP)
    MNG_ERROR (pData, MNG_SEQUENCEERROR)
    
  if (pData->iJHDRalphacompression != MNG_COMPRESSION_BASELINEJPEG)
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

  if (iRawlen == 0)                    /* can never be empty */
    MNG_ERROR (pData, MNG_INVALIDLENGTH)

  pData->bHasJDAA = MNG_TRUE;          /* got some JDAA now, don't we */

#ifdef MNG_SUPPORT_DISPLAY
  if (iRawlen)
  {                                    /* display processing for non-empty chunks */
    mng_retcode iRetcode = process_display_jdaa (pData, iRawlen, pRawdata);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
  }
#endif /* MNG_SUPPORT_DISPLAY */

#ifdef MNG_STORE_CHUNKS
  if (pData->bStorechunks)
  {                                    /* initialize storage */
    mng_retcode iRetcode = ((mng_chunk_headerp)pHeader)->fCreate (pData, pHeader, ppChunk);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
                                       /* store the fields */
    ((mng_jdaap)*ppChunk)->bEmpty    = (mng_bool)(iRawlen == 0);
    ((mng_jdaap)*ppChunk)->iDatasize = iRawlen;

    if (iRawlen != 0)                  /* is there any data ? */
    {
      MNG_ALLOC (pData, ((mng_jdaap)*ppChunk)->pData, iRawlen)
      MNG_COPY  (((mng_jdaap)*ppChunk)->pData, pRawdata, iRawlen)
    }
  }
#endif /* MNG_STORE_CHUNKS */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_JDAA, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* done */
}
#else
#define read_jdaa 0
#endif /* MNG_INCLUDE_JNG */

/* ************************************************************************** */

#ifdef MNG_INCLUDE_JNG
READ_CHUNK (read_jdat)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_JDAT, MNG_LC_START)
#endif
                                       /* sequence checks */
  if ((!pData->bHasJHDR) && (!pData->bHasDHDR))
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

  if (iRawlen == 0)                    /* can never be empty */
    MNG_ERROR (pData, MNG_INVALIDLENGTH)

  pData->bHasJDAT = MNG_TRUE;          /* got some JDAT now, don't we */

#ifdef MNG_SUPPORT_DISPLAY
  if (iRawlen)
  {                                    /* display processing for non-empty chunks */
    mng_retcode iRetcode = process_display_jdat (pData, iRawlen, pRawdata);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
  }
#endif /* MNG_SUPPORT_DISPLAY */

#ifdef MNG_STORE_CHUNKS
  if (pData->bStorechunks)
  {                                    /* initialize storage */
    mng_retcode iRetcode = ((mng_chunk_headerp)pHeader)->fCreate (pData, pHeader, ppChunk);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
                                       /* store the fields */
    ((mng_jdatp)*ppChunk)->bEmpty    = (mng_bool)(iRawlen == 0);
    ((mng_jdatp)*ppChunk)->iDatasize = iRawlen;

    if (iRawlen != 0)                  /* is there any data ? */
    {
      MNG_ALLOC (pData, ((mng_jdatp)*ppChunk)->pData, iRawlen)
      MNG_COPY  (((mng_jdatp)*ppChunk)->pData, pRawdata, iRawlen)
    }
  }
#endif /* MNG_STORE_CHUNKS */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_JDAT, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* done */
}
#else
#define read_jdat 0
#endif /* MNG_INCLUDE_JNG */

/* ************************************************************************** */

#ifdef MNG_INCLUDE_JNG
READ_CHUNK (read_jsep)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_JSEP, MNG_LC_START)
#endif

  if (!pData->bHasJHDR)                /* sequence checks */
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

  if (iRawlen != 0)                    /* must be empty ! */
    MNG_ERROR (pData, MNG_INVALIDLENGTH)

  pData->bHasJSEP = MNG_TRUE;          /* indicate we've had the 8-/12-bit separator */

#ifdef MNG_STORE_CHUNKS
  if (pData->bStorechunks)
  {                                    /* initialize storage */
    mng_retcode iRetcode = ((mng_chunk_headerp)pHeader)->fCreate (pData, pHeader, ppChunk);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;

  }
#endif /* MNG_STORE_CHUNKS */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_JSEP, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* done */
}
#else
#define read_jsep 0
#endif /* MNG_INCLUDE_JNG */

/* ************************************************************************** */

READ_CHUNK (read_dhdr)
{
  mng_uint8 iImagetype, iDeltatype;
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_DHDR, MNG_LC_START)
#endif

  if (!pData->bHasMHDR)                /* sequence checks */
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

#ifdef MNG_INCLUDE_JNG
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR) || (pData->bHasJHDR))
#else
  if ((pData->bHasIHDR) || (pData->bHasBASI) || (pData->bHasDHDR))
#endif
    MNG_ERROR (pData, MNG_SEQUENCEERROR)
                                       /* check for valid length */
  if ((iRawlen != 4) && (iRawlen != 12) && (iRawlen != 20))
    MNG_ERROR (pData, MNG_INVALIDLENGTH)

  iImagetype = *(pRawdata+2);          /* check fields for validity */
  iDeltatype = *(pRawdata+3);

  if (iImagetype > MNG_IMAGETYPE_JNG)
    MNG_ERROR (pData, MNG_INVIMAGETYPE)

  if (iDeltatype > MNG_DELTATYPE_NOCHANGE)
    MNG_ERROR (pData, MNG_INVDELTATYPE)

  if ((iDeltatype == MNG_DELTATYPE_REPLACE) && (iRawlen > 12))
    MNG_ERROR (pData, MNG_INVALIDLENGTH)

  if ((iDeltatype == MNG_DELTATYPE_NOCHANGE) && (iRawlen > 4))
    MNG_ERROR (pData, MNG_INVALIDLENGTH)

  pData->bHasDHDR = MNG_TRUE;          /* inside a DHDR-IEND block now */

  pData->iImagelevel++;                /* one level deeper */

#ifdef MNG_SUPPORT_DISPLAY
  {
    mng_uint16  iObjectid    = mng_get_uint16 (pRawdata);
    mng_uint32  iBlockwidth  = 0;
    mng_uint32  iBlockheight = 0;
    mng_uint32  iBlockx      = 0;
    mng_uint32  iBlocky      = 0;
    mng_retcode iRetcode;

    if (iRawlen > 4)
    {
      iBlockwidth  = mng_get_uint32 (pRawdata+4);
      iBlockheight = mng_get_uint32 (pRawdata+8);
    }

    if (iRawlen > 12)
    {
      iBlockx      = mng_get_uint32 (pRawdata+12);
      iBlocky      = mng_get_uint32 (pRawdata+16);
    }

    iRetcode = create_ani_dhdr (pData, iObjectid, iImagetype, iDeltatype,
                                iBlockwidth, iBlockheight, iBlockx, iBlocky);

    if (!iRetcode)                     /* display processing ? */
      iRetcode = process_display_dhdr (pData, iObjectid, iImagetype, iDeltatype,
                                       iBlockwidth, iBlockheight, iBlockx, iBlocky);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;

  }
#endif /* MNG_SUPPORT_DISPLAY */

#ifdef MNG_STORE_CHUNKS
  if (pData->bStorechunks)
  {                                    /* initialize storage */
    mng_retcode iRetcode = ((mng_chunk_headerp)pHeader)->fCreate (pData, pHeader, ppChunk);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
                                       /* store the fields */
    ((mng_dhdrp)*ppChunk)->iObjectid      = mng_get_uint16 (pRawdata);
    ((mng_dhdrp)*ppChunk)->iImagetype     = iImagetype;
    ((mng_dhdrp)*ppChunk)->iDeltatype     = iDeltatype;

    if (iRawlen > 4)
    {
      ((mng_dhdrp)*ppChunk)->iBlockwidth  = mng_get_uint32 (pRawdata+4);
      ((mng_dhdrp)*ppChunk)->iBlockheight = mng_get_uint32 (pRawdata+8);
    }

    if (iRawlen > 12)
    {
      ((mng_dhdrp)*ppChunk)->iBlockx      = mng_get_uint32 (pRawdata+12);
      ((mng_dhdrp)*ppChunk)->iBlocky      = mng_get_uint32 (pRawdata+16);
    }
  }
#endif /* MNG_STORE_CHUNKS */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_DHDR, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* done */
}

/* ************************************************************************** */

READ_CHUNK (read_prom)
{
  mng_uint8 iColortype;
  mng_uint8 iSampledepth;
  mng_uint8 iFilltype;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_PROM, MNG_LC_START)
#endif
                                       /* sequence checks */
  if ((!pData->bHasMHDR) || (!pData->bHasDHDR))
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

  if (iRawlen != 3)                    /* gotta be exactly 3 bytes */
    MNG_ERROR (pData, MNG_INVALIDLENGTH)

  iColortype   = *pRawdata;            /* check fields for validity */
  iSampledepth = *(pRawdata+1);
  iFilltype    = *(pRawdata+2);

  if ((iColortype != MNG_COLORTYPE_GRAY   ) &&
      (iColortype != MNG_COLORTYPE_RGB    ) &&
      (iColortype != MNG_COLORTYPE_INDEXED) &&
      (iColortype != MNG_COLORTYPE_GRAYA  ) &&
      (iColortype != MNG_COLORTYPE_RGBA   )    )
    MNG_ERROR (pData, MNG_INVALIDCOLORTYPE)

  if ((iSampledepth != MNG_BITDEPTH_1 ) &&
      (iSampledepth != MNG_BITDEPTH_2 ) &&
      (iSampledepth != MNG_BITDEPTH_4 ) &&
      (iSampledepth != MNG_BITDEPTH_8 ) &&
      (iSampledepth != MNG_BITDEPTH_16)    )
    MNG_ERROR (pData, MNG_INVSAMPLEDEPTH)

  if ((iFilltype != MNG_FILLMETHOD_LEFTBITREPLICATE) &&
      (iFilltype != MNG_FILLMETHOD_ZEROFILL        )    )
    MNG_ERROR (pData, MNG_INVFILLMETHOD)

#ifdef MNG_SUPPORT_DISPLAY
  {
    mng_retcode iRetcode = create_ani_prom (pData, iSampledepth, iColortype, iFilltype);

    if (!iRetcode)                     /* display processing ? */
      iRetcode = process_display_prom (pData, iSampledepth,
                                       iColortype, iFilltype);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;

  }
#endif /* MNG_SUPPORT_DISPLAY */

#ifdef MNG_STORE_CHUNKS
  if (pData->bStorechunks)
  {                                    /* initialize storage */
    mng_retcode iRetcode = ((mng_chunk_headerp)pHeader)->fCreate (pData, pHeader, ppChunk);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
                                       /* store the fields */
    ((mng_promp)*ppChunk)->iColortype   = iColortype;
    ((mng_promp)*ppChunk)->iSampledepth = iSampledepth;
    ((mng_promp)*ppChunk)->iFilltype    = iFilltype;
  }
#endif /* MNG_STORE_CHUNKS */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_PROM, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* done */
}

/* ************************************************************************** */

READ_CHUNK (read_ipng)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_IPNG, MNG_LC_START)
#endif
                                       /* sequence checks */
  if ((!pData->bHasMHDR) || (!pData->bHasDHDR))
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

  if (iRawlen != 0)                    /* gotta be empty */
    MNG_ERROR (pData, MNG_INVALIDLENGTH)

#ifdef MNG_SUPPORT_DISPLAY
  {
    mng_retcode iRetcode = create_ani_ipng (pData);

    if (!iRetcode)                     /* process it */
      iRetcode = process_display_ipng (pData);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;

  }
#endif /* MNG_SUPPORT_DISPLAY */

#ifdef MNG_STORE_CHUNKS
  if (pData->bStorechunks)
  {                                    /* initialize storage */
    mng_retcode iRetcode = ((mng_chunk_headerp)pHeader)->fCreate (pData, pHeader, ppChunk);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
  }
#endif /* MNG_STORE_CHUNKS */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_IPNG, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* done */
}

/* ************************************************************************** */

READ_CHUNK (read_pplt)
{
  mng_uint8     iDeltatype;
  mng_uint8p    pTemp;
  mng_uint32    iLen;
  mng_uint8     iX, iM;
  mng_uint32    iY;
  mng_uint32    iMax;
  mng_rgbpaltab aIndexentries;
  mng_uint8arr  aAlphaentries;
  mng_uint8arr  aUsedentries;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_PPLT, MNG_LC_START)
#endif
                                       /* sequence checks */
  if ((!pData->bHasMHDR) && (!pData->bHasDHDR))
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

  if (iRawlen < 1)                     /* must have at least 1 byte */
    MNG_ERROR (pData, MNG_INVALIDLENGTH)

  iDeltatype = *pRawdata;
                                       /* valid ? */
  if (iDeltatype > MNG_DELTATYPE_DELTARGBA)
    MNG_ERROR (pData, MNG_INVDELTATYPE)
                                       /* must be indexed color ! */
  if (pData->iColortype != MNG_COLORTYPE_INDEXED)
    MNG_ERROR (pData, MNG_INVALIDCOLORTYPE)

  pTemp = pRawdata + 1;
  iLen  = iRawlen - 1;
  iMax  = 0;

  for (iY = 0; iY < 256; iY++)         /* reset arrays */
  {
    aIndexentries [iY].iRed   = 0;
    aIndexentries [iY].iGreen = 0;
    aIndexentries [iY].iBlue  = 0;
    aAlphaentries [iY]        = 255;
    aUsedentries  [iY]        = 0;
  }

  while (iLen)                         /* as long as there are entries left ... */
  {
    mng_uint32 iDiff;

    if (iLen < 2)
      MNG_ERROR (pData, MNG_INVALIDLENGTH)

    iX = *pTemp;                       /* get start and end index */
    iM = *(pTemp+1);

    if (iM < iX)
      MNG_ERROR (pData, MNG_INVALIDINDEX)

    if ((mng_uint32)iM >= iMax)        /* determine highest used index */
      iMax = (mng_uint32)iM + 1;

    pTemp += 2;
    iLen  -= 2;

    if ((iDeltatype == MNG_DELTATYPE_REPLACERGB  ) ||
        (iDeltatype == MNG_DELTATYPE_DELTARGB    )    )
      iDiff = (iM - iX + 1) * 3;
    else
    if ((iDeltatype == MNG_DELTATYPE_REPLACEALPHA) ||
        (iDeltatype == MNG_DELTATYPE_DELTAALPHA  )    )
      iDiff = (iM - iX + 1);
    else
      iDiff = (iM - iX + 1) * 4;

    if (iLen < iDiff)
      MNG_ERROR (pData, MNG_INVALIDLENGTH)

    if ((iDeltatype == MNG_DELTATYPE_REPLACERGB  ) ||
        (iDeltatype == MNG_DELTATYPE_DELTARGB    )    )
    {
      for (iY = (mng_uint32)iX; iY <= (mng_uint32)iM; iY++)
      {
        aIndexentries [iY].iRed   = *pTemp;
        aIndexentries [iY].iGreen = *(pTemp+1);
        aIndexentries [iY].iBlue  = *(pTemp+2);
        aUsedentries  [iY]        = 1;

        pTemp += 3;
        iLen  -= 3;
      }
    }
    else
    if ((iDeltatype == MNG_DELTATYPE_REPLACEALPHA) ||
        (iDeltatype == MNG_DELTATYPE_DELTAALPHA  )    )
    {
      for (iY = (mng_uint32)iX; iY <= (mng_uint32)iM; iY++)
      {
        aAlphaentries [iY]        = *pTemp;
        aUsedentries  [iY]        = 1;

        pTemp++;
        iLen--;
      }
    }
    else
    {
      for (iY = (mng_uint32)iX; iY <= (mng_uint32)iM; iY++)
      {
        aIndexentries [iY].iRed   = *pTemp;
        aIndexentries [iY].iGreen = *(pTemp+1);
        aIndexentries [iY].iBlue  = *(pTemp+2);
        aAlphaentries [iY]        = *(pTemp+3);
        aUsedentries  [iY]        = 1;

        pTemp += 4;
        iLen  -= 4;
      }
    }
  }

  switch (pData->iBitdepth)            /* check maximum allowed entries for bitdepth */
  {
    case MNG_BITDEPTH_1 : {
                            if (iMax > 2)
                              MNG_ERROR (pData, MNG_INVALIDINDEX)
                            break;
                          }
    case MNG_BITDEPTH_2 : {
                            if (iMax > 4)
                              MNG_ERROR (pData, MNG_INVALIDINDEX)
                            break;
                          }
    case MNG_BITDEPTH_4 : {
                            if (iMax > 16)
                              MNG_ERROR (pData, MNG_INVALIDINDEX)
                            break;
                          }
  }

#ifdef MNG_SUPPORT_DISPLAY
  {                                    /* create animation object */
    mng_retcode iRetcode = create_ani_pplt (pData, iDeltatype, iMax,
                                            aIndexentries, aAlphaentries,
                                            aUsedentries);

    if (!iRetcode)                     /* execute it now ? */
      iRetcode = process_display_pplt (pData, iDeltatype, iMax, aIndexentries,
                                       aAlphaentries, aUsedentries);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
      
  }
#endif /* MNG_SUPPORT_DISPLAY */

#ifdef MNG_STORE_CHUNKS
  if (pData->bStorechunks)
  {                                    /* initialize storage */
    mng_retcode iRetcode = ((mng_chunk_headerp)pHeader)->fCreate (pData, pHeader, ppChunk);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
                                       /* store the fields */
    ((mng_ppltp)*ppChunk)->iDeltatype = iDeltatype;
    ((mng_ppltp)*ppChunk)->iCount     = iMax;

    for (iY = 0; iY < 256; iY++)
    {
      ((mng_ppltp)*ppChunk)->aEntries [iY].iRed   = aIndexentries [iY].iRed;
      ((mng_ppltp)*ppChunk)->aEntries [iY].iGreen = aIndexentries [iY].iGreen;
      ((mng_ppltp)*ppChunk)->aEntries [iY].iBlue  = aIndexentries [iY].iBlue;
      ((mng_ppltp)*ppChunk)->aEntries [iY].iAlpha = aAlphaentries [iY];
      ((mng_ppltp)*ppChunk)->aEntries [iY].bUsed  = (mng_bool)(aUsedentries [iY]);
    }
  }
#endif /* MNG_STORE_CHUNKS */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_PPLT, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* done */
}

/* ************************************************************************** */

READ_CHUNK (read_ijng)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_IJNG, MNG_LC_START)
#endif
                                       /* sequence checks */
  if ((!pData->bHasMHDR) || (!pData->bHasDHDR))
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

  if (iRawlen != 0)                    /* gotta be empty */
    MNG_ERROR (pData, MNG_INVALIDLENGTH)

#ifdef MNG_SUPPORT_DISPLAY
  {
    mng_retcode iRetcode = create_ani_ijng (pData);

    if (!iRetcode)                     /* process it */
      iRetcode = process_display_ijng (pData);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;

  }
#endif /* MNG_SUPPORT_DISPLAY */

#ifdef MNG_STORE_CHUNKS
  if (pData->bStorechunks)
  {                                    /* initialize storage */
    mng_retcode iRetcode = ((mng_chunk_headerp)pHeader)->fCreate (pData, pHeader, ppChunk);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
  }
#endif /* MNG_STORE_CHUNKS */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_IJNG, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* done */
}

/* ************************************************************************** */

READ_CHUNK (read_drop)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_DROP, MNG_LC_START)
#endif
                                       /* sequence checks */
  if ((!pData->bHasMHDR) || (!pData->bHasDHDR))
    MNG_ERROR (pData, MNG_SEQUENCEERROR)
                                       /* check length */
  if ((iRawlen < 4) || ((iRawlen % 4) != 0))
    MNG_ERROR (pData, MNG_INVALIDLENGTH)

#ifdef MNG_SUPPORT_DISPLAY
  {


    /* TODO: something !!! */


  }
#endif /* MNG_SUPPORT_DISPLAY */

#ifdef MNG_STORE_CHUNKS
  if (pData->bStorechunks)
  {                                    /* initialize storage */
    mng_retcode iRetcode = ((mng_chunk_headerp)pHeader)->fCreate (pData, pHeader, ppChunk);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
                                       /* store the fields */
    ((mng_dropp)*ppChunk)->iCount = iRawlen / 4;

    if (iRawlen)
    {
      mng_uint32      iX;
      mng_uint8p      pTemp = pRawdata;
      mng_uint32p     pEntry;

      MNG_ALLOC (pData, pEntry, iRawlen)

      ((mng_dropp)*ppChunk)->pChunknames = (mng_ptr)pEntry;

      for (iX = 0; iX < iRawlen / 4; iX++)
      {
        *pEntry = mng_get_uint32 (pTemp);

        pTemp  += 4;
        pEntry++;
      }
    }
  }
#endif /* MNG_STORE_CHUNKS */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_DROP, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* done */
}

/* ************************************************************************** */

READ_CHUNK (read_dbyk)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_DBYK, MNG_LC_START)
#endif
                                       /* sequence checks */
  if ((!pData->bHasMHDR) || (!pData->bHasDHDR))
    MNG_ERROR (pData, MNG_SEQUENCEERROR)

  if (iRawlen < 6)                     /* must be at least 6 long */
    MNG_ERROR (pData, MNG_INVALIDLENGTH)

#ifdef MNG_SUPPORT_DISPLAY
  {


    /* TODO: something !!! */


  }
#endif /* MNG_SUPPORT_DISPLAY */

#ifdef MNG_STORE_CHUNKS
  if (pData->bStorechunks)
  {                                    /* initialize storage */
    mng_retcode iRetcode = ((mng_chunk_headerp)pHeader)->fCreate (pData, pHeader, ppChunk);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
                                       /* store the fields */
    ((mng_dbykp)*ppChunk)->iChunkname    = mng_get_uint32 (pRawdata);
    ((mng_dbykp)*ppChunk)->iPolarity     = *(pRawdata+4);
    ((mng_dbykp)*ppChunk)->iKeywordssize = iRawlen - 5;

    if (iRawlen > 5)
    {
      MNG_ALLOC (pData, ((mng_dbykp)*ppChunk)->zKeywords, iRawlen-4)
      MNG_COPY (((mng_dbykp)*ppChunk)->zKeywords, pRawdata+5, iRawlen-5)
    }  
  }
#endif /* MNG_STORE_CHUNKS */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_DBYK, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* done */
}

/* ************************************************************************** */

READ_CHUNK (read_ordr)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_ORDR, MNG_LC_START)
#endif
                                       /* sequence checks */
  if ((!pData->bHasMHDR) || (!pData->bHasDHDR))
    MNG_ERROR (pData, MNG_SEQUENCEERROR)
                                       /* check length */
  if ((iRawlen < 5) || ((iRawlen % 5) != 0))
    MNG_ERROR (pData, MNG_INVALIDLENGTH)

#ifdef MNG_SUPPORT_DISPLAY
  {


    /* TODO: something !!! */


  }
#endif /* MNG_SUPPORT_DISPLAY */

#ifdef MNG_STORE_CHUNKS
  if (pData->bStorechunks)
  {                                    /* initialize storage */
    mng_retcode iRetcode = ((mng_chunk_headerp)pHeader)->fCreate (pData, pHeader, ppChunk);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
                                       /* store the fields */
    ((mng_ordrp)*ppChunk)->iCount = iRawlen / 5;

    if (iRawlen)
    {
      mng_uint32      iX;
      mng_ordr_entryp pEntry;
      mng_uint8p      pTemp = pRawdata;
      
      MNG_ALLOC (pData, pEntry, iRawlen)

      ((mng_ordrp)*ppChunk)->pEntries = pEntry;

      for (iX = 0; iX < iRawlen / 5; iX++)
      {
        pEntry->iChunkname = mng_get_uint32 (pTemp);
        pEntry->iOrdertype = *(pTemp+4);

        pTemp += 5;
        pEntry++;
      }
    }
  }
#endif /* MNG_STORE_CHUNKS */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_ORDR, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* done */
}

/* ************************************************************************** */

READ_CHUNK (read_magn)
{
  mng_uint16 iFirstid, iLastid;
  mng_uint16 iMethodX, iMethodY;
  mng_uint16 iMX, iMY, iML, iMR, iMT, iMB;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_MAGN, MNG_LC_START)
#endif
                                       /* sequence checks */
#ifdef MNG_SUPPORT_JNG
  if ((!pData->bHasMHDR) || (pData->bHasIHDR) || (pData->bHasDHDR) || (pData->bHasJHDR))
#else
  if ((!pData->bHasMHDR) || (pData->bHasIHDR) || (pData->bHasDHDR))
#endif
    MNG_ERROR (pData, MNG_SEQUENCEERROR)
                                       /* check length */
  if ((iRawlen > 20) || ((iRawlen & 0x01) != 0))
    MNG_ERROR (pData, MNG_INVALIDLENGTH)

  if (iRawlen > 0)                     /* get the fields */
    iFirstid = mng_get_uint16 (pRawdata);
  else
    iFirstid = 0;

  if (iRawlen > 2)
    iLastid  = mng_get_uint16 (pRawdata+2);
  else
    iLastid  = iFirstid;

  if (iRawlen > 4)
    iMethodX = mng_get_uint16 (pRawdata+4);
  else
    iMethodX = 0;

  if (iRawlen > 6)
    iMX      = mng_get_uint16 (pRawdata+6);
  else
    iMX      = 1;

  if (iRawlen > 8)
    iMY      = mng_get_uint16 (pRawdata+8);
  else
    iMY      = iMX;

  if (iRawlen > 10)
    iML      = mng_get_uint16 (pRawdata+10);
  else
    iML      = iMX;

  if (iRawlen > 12)
    iMR      = mng_get_uint16 (pRawdata+12);
  else
    iMR      = iMX;

  if (iRawlen > 14)
    iMT      = mng_get_uint16 (pRawdata+14);
  else
    iMT      = iMY;

  if (iRawlen > 16)
    iMB      = mng_get_uint16 (pRawdata+16);
  else
    iMB      = iMY;

  if (iRawlen > 18)
    iMethodY = mng_get_uint16 (pRawdata+18);
  else
    iMethodY = iMethodX;
                                       /* check field validity */
  if ((iMethodX > 5) || (iMethodY > 5))
    MNG_ERROR (pData, MNG_INVALIDMETHOD)

#ifdef MNG_SUPPORT_DISPLAY
  {
    mng_retcode iRetcode;

    iRetcode = create_ani_magn (pData, iFirstid, iLastid, iMethodX,
                                iMX, iMY, iML, iMR, iMT, iMB, iMethodY);

    if (!iRetcode)                     /* display processing ? */
      iRetcode = process_display_magn (pData, iFirstid, iLastid, iMethodX,
                                       iMX, iMY, iML, iMR, iMT, iMB, iMethodY);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;

  }
#endif /* MNG_SUPPORT_DISPLAY */

#ifdef MNG_STORE_CHUNKS
  if (pData->bStorechunks)
  {                                    /* initialize storage */
    mng_retcode iRetcode = ((mng_chunk_headerp)pHeader)->fCreate (pData, pHeader, ppChunk);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
                                       /* store the fields */
    ((mng_magnp)*ppChunk)->iFirstid = iFirstid;
    ((mng_magnp)*ppChunk)->iLastid  = iLastid;
    ((mng_magnp)*ppChunk)->iMethodX = iMethodX;
    ((mng_magnp)*ppChunk)->iMX      = iMX;
    ((mng_magnp)*ppChunk)->iMY      = iMY;
    ((mng_magnp)*ppChunk)->iML      = iML;
    ((mng_magnp)*ppChunk)->iMR      = iMR;
    ((mng_magnp)*ppChunk)->iMT      = iMT;
    ((mng_magnp)*ppChunk)->iMB      = iMB;
    ((mng_magnp)*ppChunk)->iMethodY = iMethodY;
  }
#endif /* MNG_STORE_CHUNKS */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_MAGN, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* done */
}

/* ************************************************************************** */

READ_CHUNK (read_unknown)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_UNKNOWN, MNG_LC_START)
#endif
                                       /* sequence checks */
#ifdef MNG_INCLUDE_JNG
  if ((!pData->bHasMHDR) && (!pData->bHasIHDR) &&
      (!pData->bHasBASI) && (!pData->bHasDHDR) && (!pData->bHasJHDR))
#else
  if ((!pData->bHasMHDR) && (!pData->bHasIHDR) &&
      (!pData->bHasBASI) && (!pData->bHasDHDR)    )
#endif
    MNG_ERROR (pData, MNG_SEQUENCEERROR)
                                       /* critical chunk ? */
  if (((mng_uint32)pData->iChunkname & 0x20000000) == 0)
    MNG_ERROR (pData, MNG_UNKNOWNCRITICAL)

  if (pData->fProcessunknown)          /* let the app handle it ? */
  {
    mng_bool bOke = pData->fProcessunknown ((mng_handle)pData, pData->iChunkname,
                                            iRawlen, (mng_ptr)pRawdata);

    if (!bOke)
      MNG_ERROR (pData, MNG_APPMISCERROR)
  }

#ifdef MNG_STORE_CHUNKS
  if (pData->bStorechunks)
  {                                    /* initialize storage */
    mng_retcode iRetcode = ((mng_chunk_headerp)pHeader)->fCreate (pData, pHeader, ppChunk);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
                                       /* store the length */
    ((mng_chunk_headerp)*ppChunk)->iChunkname = pData->iChunkname;
    ((mng_unknown_chunkp)*ppChunk)->iDatasize = iRawlen;

    if (iRawlen == 0)                  /* any data at all ? */
      ((mng_unknown_chunkp)*ppChunk)->pData = 0;
    else
    {                                  /* then store it */
      MNG_ALLOC (pData, ((mng_unknown_chunkp)*ppChunk)->pData, iRawlen)
      MNG_COPY (((mng_unknown_chunkp)*ppChunk)->pData, pRawdata, iRawlen)
    }
  }
#endif /* MNG_STORE_CHUNKS */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_READ_UNKNOWN, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* done */
}

/* ************************************************************************** */

#endif /* MNG_INCLUDE_READ_PROCS */

/* ************************************************************************** */
/* *                                                                        * */
/* * chunk write functions                                                  * */
/* *                                                                        * */
/* ************************************************************************** */

#ifdef MNG_INCLUDE_WRITE_PROCS

/* ************************************************************************** */

WRITE_CHUNK (write_ihdr)
{
  mng_ihdrp   pIHDR;
  mng_uint8p  pRawdata;
  mng_uint32  iRawlen;
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_IHDR, MNG_LC_START)
#endif

  pIHDR    = (mng_ihdrp)pChunk;        /* address the proper chunk */
  pRawdata = pData->pWritebuf+8;       /* init output buffer & size */
  iRawlen  = 13;
                                       /* fill the output buffer */
  mng_put_uint32 (pRawdata,   pIHDR->iWidth);
  mng_put_uint32 (pRawdata+4, pIHDR->iHeight);

  *(pRawdata+8)  = pIHDR->iBitdepth;
  *(pRawdata+9)  = pIHDR->iColortype;
  *(pRawdata+10) = pIHDR->iCompression;
  *(pRawdata+11) = pIHDR->iFilter;
  *(pRawdata+12) = pIHDR->iInterlace;
                                       /* and write it */
  iRetcode = write_raw_chunk (pData, pIHDR->sHeader.iChunkname, iRawlen, pRawdata);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_IHDR, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */

WRITE_CHUNK (write_plte)
{
  mng_pltep   pPLTE;
  mng_uint8p  pRawdata;
  mng_uint32  iRawlen;
  mng_retcode iRetcode;
  mng_uint8p  pTemp;
  mng_uint32  iX;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_PLTE, MNG_LC_START)
#endif

  pPLTE    = (mng_pltep)pChunk;        /* address the proper chunk */

  if (pPLTE->bEmpty)                   /* write empty chunk ? */
    iRetcode = write_raw_chunk (pData, pPLTE->sHeader.iChunkname, 0, 0);
  else
  {
    pRawdata = pData->pWritebuf+8;     /* init output buffer & size */
    iRawlen  = pPLTE->iEntrycount * 3;
                                       /* fill the output buffer */
    pTemp = pRawdata;

    for (iX = 0; iX < pPLTE->iEntrycount; iX++)
    {
      *pTemp     = pPLTE->aEntries [iX].iRed;
      *(pTemp+1) = pPLTE->aEntries [iX].iGreen;
      *(pTemp+2) = pPLTE->aEntries [iX].iBlue;

      pTemp += 3;
    }
                                       /* and write it */
    iRetcode = write_raw_chunk (pData, pPLTE->sHeader.iChunkname, iRawlen, pRawdata);
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_PLTE, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */

WRITE_CHUNK (write_idat)
{
  mng_idatp   pIDAT;
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_IDAT, MNG_LC_START)
#endif

  pIDAT = (mng_idatp)pChunk;           /* address the proper chunk */

  if (pIDAT->bEmpty)                   /* and write it */
    iRetcode = write_raw_chunk (pData, pIDAT->sHeader.iChunkname, 0, 0);
  else
    iRetcode = write_raw_chunk (pData, pIDAT->sHeader.iChunkname,
                                pIDAT->iDatasize, pIDAT->pData);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_IDAT, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */

WRITE_CHUNK (write_iend)
{
  mng_iendp   pIEND;
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_IEND, MNG_LC_START)
#endif

  pIEND = (mng_iendp)pChunk;           /* address the proper chunk */
                                       /* and write it */
  iRetcode = write_raw_chunk (pData, pIEND->sHeader.iChunkname, 0, 0);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_IEND, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */

WRITE_CHUNK (write_trns)
{
  mng_trnsp   pTRNS;
  mng_uint8p  pRawdata;
  mng_uint32  iRawlen;
  mng_retcode iRetcode;
  mng_uint8p  pTemp;
  mng_uint32  iX;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_TRNS, MNG_LC_START)
#endif

  pTRNS = (mng_trnsp)pChunk;           /* address the proper chunk */

  if (pTRNS->bEmpty)                   /* write empty chunk ? */
    iRetcode = write_raw_chunk (pData, pTRNS->sHeader.iChunkname, 0, 0);
  else
  if (pTRNS->bGlobal)                  /* write global chunk ? */
    iRetcode = write_raw_chunk (pData, pTRNS->sHeader.iChunkname,
                                pTRNS->iRawlen, (mng_uint8p)pTRNS->aRawdata);
  else
  {
    pRawdata = pData->pWritebuf+8;     /* init output buffer */
    iRawlen  = 0;                      /* and default size */

    switch (pTRNS->iType)
    {
      case 0: {
                iRawlen   = 2;         /* fill the size & output buffer */
                mng_put_uint16 (pRawdata, pTRNS->iGray);

                break;
              }
      case 2: {
                iRawlen       = 6;     /* fill the size & output buffer */
                mng_put_uint16 (pRawdata,   pTRNS->iRed);
                mng_put_uint16 (pRawdata+2, pTRNS->iGreen);
                mng_put_uint16 (pRawdata+4, pTRNS->iBlue);

                break;
              }
      case 3: {                        /* init output buffer size */
                iRawlen = pTRNS->iCount;

                pTemp   = pRawdata;    /* fill the output buffer */

                for (iX = 0; iX < pTRNS->iCount; iX++)
                {
                  *pTemp = pTRNS->aEntries[iX];
                  pTemp++;
                }

                break;
              }
    }
                                       /* write the chunk */
    iRetcode = write_raw_chunk (pData, pTRNS->sHeader.iChunkname,
                                iRawlen, pRawdata);
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_TRNS, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */

WRITE_CHUNK (write_gama)
{
  mng_gamap   pGAMA;
  mng_uint8p  pRawdata;
  mng_uint32  iRawlen;
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_GAMA, MNG_LC_START)
#endif

  pGAMA = (mng_gamap)pChunk;           /* address the proper chunk */

  if (pGAMA->bEmpty)                   /* write empty ? */
    iRetcode = write_raw_chunk (pData, pGAMA->sHeader.iChunkname, 0, 0);
  else
  {
    pRawdata = pData->pWritebuf+8;     /* init output buffer & size */
    iRawlen  = 4;
                                       /* fill the buffer */
    mng_put_uint32 (pRawdata, pGAMA->iGamma);
                                       /* and write it */
    iRetcode = write_raw_chunk (pData, pGAMA->sHeader.iChunkname,
                                iRawlen, pRawdata);
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_GAMA, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */

WRITE_CHUNK (write_chrm)
{
  mng_chrmp   pCHRM;
  mng_uint8p  pRawdata;
  mng_uint32  iRawlen;
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_CHRM, MNG_LC_START)
#endif

  pCHRM = (mng_chrmp)pChunk;           /* address the proper chunk */

  if (pCHRM->bEmpty)                   /* write empty ? */
    iRetcode = write_raw_chunk (pData, pCHRM->sHeader.iChunkname, 0, 0);
  else
  {
    pRawdata = pData->pWritebuf+8;     /* init output buffer & size */
    iRawlen  = 32;
                                       /* fill the buffer */
    mng_put_uint32 (pRawdata,    pCHRM->iWhitepointx);
    mng_put_uint32 (pRawdata+4,  pCHRM->iWhitepointy);
    mng_put_uint32 (pRawdata+8,  pCHRM->iRedx);
    mng_put_uint32 (pRawdata+12, pCHRM->iRedy);
    mng_put_uint32 (pRawdata+16, pCHRM->iGreenx);
    mng_put_uint32 (pRawdata+20, pCHRM->iGreeny);
    mng_put_uint32 (pRawdata+24, pCHRM->iBluex);
    mng_put_uint32 (pRawdata+28, pCHRM->iBluey);
                                       /* and write it */
    iRetcode = write_raw_chunk (pData, pCHRM->sHeader.iChunkname,
                                iRawlen, pRawdata);
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_CHRM, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */

WRITE_CHUNK (write_srgb)
{
  mng_srgbp   pSRGB;
  mng_uint8p  pRawdata;
  mng_uint32  iRawlen;
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_SRGB, MNG_LC_START)
#endif

  pSRGB = (mng_srgbp)pChunk;           /* address the proper chunk */

  if (pSRGB->bEmpty)                   /* write empty ? */
    iRetcode = write_raw_chunk (pData, pSRGB->sHeader.iChunkname, 0, 0);
  else
  {
    pRawdata = pData->pWritebuf+8;     /* init output buffer & size */
    iRawlen  = 1;
                                       /* fill the buffer */
    *pRawdata = pSRGB->iRenderingintent;
                                       /* and write it */
    iRetcode = write_raw_chunk (pData, pSRGB->sHeader.iChunkname,
                                iRawlen, pRawdata);
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_SRGB, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */

WRITE_CHUNK (write_iccp)
{
  mng_iccpp   pICCP;
  mng_uint8p  pRawdata;
  mng_uint32  iRawlen;
  mng_retcode iRetcode;
  mng_uint8p  pTemp;
  mng_uint8p  pBuf = 0;
  mng_uint32  iBuflen;
  mng_uint32  iReallen;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_ICCP, MNG_LC_START)
#endif

  pICCP = (mng_iccpp)pChunk;           /* address the proper chunk */

  if (pICCP->bEmpty)                   /* write empty ? */
    iRetcode = write_raw_chunk (pData, pICCP->sHeader.iChunkname, 0, 0);
  else
  {                                    /* compress the profile */
    iRetcode = deflate_buffer (pData, pICCP->pProfile, pICCP->iProfilesize,
                               &pBuf, &iBuflen, &iReallen);

    if (!iRetcode)                     /* still oke ? */
    {
      pRawdata = pData->pWritebuf+8;   /* init output buffer & size */
      iRawlen  = pICCP->iNamesize + 2 + iReallen;
                                       /* requires large buffer ? */
      if (iRawlen > pData->iWritebufsize)
        MNG_ALLOC (pData, pRawdata, iRawlen)

      pTemp = pRawdata;                /* fill the buffer */

      if (pICCP->iNamesize)
      {
        MNG_COPY (pTemp, pICCP->zName, pICCP->iNamesize)
        pTemp += pICCP->iNamesize;
      }

      *pTemp     = 0;
      *(pTemp+1) = pICCP->iCompression;
      pTemp += 2;

      if (iReallen)
        MNG_COPY (pTemp, pBuf, iReallen)
                                       /* and write it */
      iRetcode = write_raw_chunk (pData, pICCP->sHeader.iChunkname,
                                  iRawlen, pRawdata);
                                       /* drop the temp buffer ? */
      if (iRawlen > pData->iWritebufsize)
        MNG_FREEX (pData, pRawdata, iRawlen)

    }

    MNG_FREEX (pData, pBuf, iBuflen)   /* always drop the extra buffer */
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_ICCP, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */

WRITE_CHUNK (write_text)
{
  mng_textp   pTEXT;
  mng_uint8p  pRawdata;
  mng_uint32  iRawlen;
  mng_retcode iRetcode;
  mng_uint8p  pTemp;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_TEXT, MNG_LC_START)
#endif

  pTEXT = (mng_textp)pChunk;           /* address the proper chunk */

  pRawdata = pData->pWritebuf+8;       /* init output buffer & size */
  iRawlen  = pTEXT->iKeywordsize + 1 + pTEXT->iTextsize;
                                       /* requires large buffer ? */
  if (iRawlen > pData->iWritebufsize)
    MNG_ALLOC (pData, pRawdata, iRawlen)

  pTemp = pRawdata;                    /* fill the buffer */

  if (pTEXT->iKeywordsize)
  {
    MNG_COPY (pTemp, pTEXT->zKeyword, pTEXT->iKeywordsize)
    pTemp += pTEXT->iKeywordsize;
  }

  *pTemp = 0;
  pTemp += 1;

  if (pTEXT->iTextsize)
    MNG_COPY (pTemp, pTEXT->zText, pTEXT->iTextsize)
                                       /* and write it */
  iRetcode = write_raw_chunk (pData, pTEXT->sHeader.iChunkname,
                              iRawlen, pRawdata);

  if (iRawlen > pData->iWritebufsize)  /* drop the temp buffer ? */
    MNG_FREEX (pData, pRawdata, iRawlen)

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_TEXT, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */

WRITE_CHUNK (write_ztxt)
{
  mng_ztxtp   pZTXT;
  mng_uint8p  pRawdata;
  mng_uint32  iRawlen;
  mng_retcode iRetcode;
  mng_uint8p  pTemp;
  mng_uint8p  pBuf = 0;
  mng_uint32  iBuflen;
  mng_uint32  iReallen;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_ZTXT, MNG_LC_START)
#endif

  pZTXT = (mng_ztxtp)pChunk;           /* address the proper chunk */
                                       /* compress the text */
  iRetcode = deflate_buffer (pData, (mng_uint8p)pZTXT->zText, pZTXT->iTextsize,
                             &pBuf, &iBuflen, &iReallen);

  if (!iRetcode)                       /* all ok ? */
  {
    pRawdata = pData->pWritebuf+8;     /* init output buffer & size */
    iRawlen  = pZTXT->iKeywordsize + 2 + iReallen;
                                       /* requires large buffer ? */
    if (iRawlen > pData->iWritebufsize)
      MNG_ALLOC (pData, pRawdata, iRawlen)

    pTemp = pRawdata;                  /* fill the buffer */

    if (pZTXT->iKeywordsize)
    {
      MNG_COPY (pTemp, pZTXT->zKeyword, pZTXT->iKeywordsize)
      pTemp += pZTXT->iKeywordsize;
    }

    *pTemp = 0;                        /* terminator zero */
    pTemp++;
    *pTemp = 0;                        /* compression type */
    pTemp++;

    if (iReallen)
      MNG_COPY (pTemp, pBuf, iReallen)
                                       /* and write it */
    iRetcode = write_raw_chunk (pData, pZTXT->sHeader.iChunkname,
                                iRawlen, pRawdata);
                                       /* drop the temp buffer ? */
    if (iRawlen > pData->iWritebufsize)
      MNG_FREEX (pData, pRawdata, iRawlen)

  }

  MNG_FREEX (pData, pBuf, iBuflen);    /* always drop the compression buffer */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_ZTXT, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */

WRITE_CHUNK (write_itxt)
{
  mng_itxtp   pITXT;
  mng_uint8p  pRawdata;
  mng_uint32  iRawlen;
  mng_retcode iRetcode;
  mng_uint8p  pTemp;
  mng_uint8p  pBuf = 0;
  mng_uint32  iBuflen;
  mng_uint32  iReallen;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_ITXT, MNG_LC_START)
#endif

  pITXT = (mng_itxtp)pChunk;           /* address the proper chunk */

  if (pITXT->iCompressionflag)         /* compress the text */
    iRetcode = deflate_buffer (pData, (mng_uint8p)pITXT->zText, pITXT->iTextsize,
                               &pBuf, &iBuflen, &iReallen);
  else
    iRetcode = MNG_NOERROR;

  if (!iRetcode)                       /* all ok ? */
  {
    pRawdata = pData->pWritebuf+8;     /* init output buffer & size */
    iRawlen  = pITXT->iKeywordsize + pITXT->iLanguagesize +
               pITXT->iTranslationsize + 5;

    if (pITXT->iCompressionflag)
      iRawlen = iRawlen + iReallen;
    else
      iRawlen = iRawlen + pITXT->iTextsize;
                                       /* requires large buffer ? */
    if (iRawlen > pData->iWritebufsize)
      MNG_ALLOC (pData, pRawdata, iRawlen)

    pTemp = pRawdata;                  /* fill the buffer */

    if (pITXT->iKeywordsize)
    {
      MNG_COPY (pTemp, pITXT->zKeyword, pITXT->iKeywordsize)
      pTemp += pITXT->iKeywordsize;
    }

    *pTemp = 0;
    pTemp++;
    *pTemp = pITXT->iCompressionflag;
    pTemp++;
    *pTemp = pITXT->iCompressionmethod;
    pTemp++;

    if (pITXT->iLanguagesize)
    {
      MNG_COPY (pTemp, pITXT->zLanguage, pITXT->iLanguagesize)
      pTemp += pITXT->iLanguagesize;
    }

    *pTemp = 0;
    pTemp++;

    if (pITXT->iTranslationsize)
    {
      MNG_COPY (pTemp, pITXT->zTranslation, pITXT->iTranslationsize)
      pTemp += pITXT->iTranslationsize;
    }

    *pTemp = 0;
    pTemp++;

    if (pITXT->iCompressionflag)
    {
      if (iReallen)
        MNG_COPY (pTemp, pBuf, iReallen)
    }
    else
    {
      if (pITXT->iTextsize)
        MNG_COPY (pTemp, pITXT->zText, pITXT->iTextsize)
    }
                                       /* and write it */
    iRetcode = write_raw_chunk (pData, pITXT->sHeader.iChunkname,
                                iRawlen, pRawdata);
                                       /* drop the temp buffer ? */
    if (iRawlen > pData->iWritebufsize)
      MNG_FREEX (pData, pRawdata, iRawlen)

  }

  MNG_FREEX (pData, pBuf, iBuflen);    /* always drop the compression buffer */

  if (iRetcode)                        /* on error bail out */
    return iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_ITXT, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

WRITE_CHUNK (write_bkgd)
{
  mng_bkgdp   pBKGD;
  mng_uint8p  pRawdata;
  mng_uint32  iRawlen;
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_BKGD, MNG_LC_START)
#endif

  pBKGD = (mng_bkgdp)pChunk;           /* address the proper chunk */

  if (pBKGD->bEmpty)                   /* write empty ? */
    iRetcode = write_raw_chunk (pData, pBKGD->sHeader.iChunkname, 0, 0);
  else
  {
    pRawdata = pData->pWritebuf+8;     /* init output buffer & size */
    iRawlen  = 0;                      /* and default size */

    switch (pBKGD->iType)
    {
      case 0: {                        /* gray */
                iRawlen = 2;           /* fill the size & output buffer */
                mng_put_uint16 (pRawdata, pBKGD->iGray);

                break;
              }
      case 2: {                        /* rgb */
                iRawlen = 6;           /* fill the size & output buffer */
                mng_put_uint16 (pRawdata,   pBKGD->iRed);
                mng_put_uint16 (pRawdata+2, pBKGD->iGreen);
                mng_put_uint16 (pRawdata+4, pBKGD->iBlue);

                break;
              }
      case 3: {                        /* indexed */
                iRawlen   = 1;         /* fill the size & output buffer */
                *pRawdata = pBKGD->iIndex;

                break;
              }
    }
                                       /* and write it */
    iRetcode = write_raw_chunk (pData, pBKGD->sHeader.iChunkname,
                                iRawlen, pRawdata);
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_BKGD, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */

WRITE_CHUNK (write_phys)
{
  mng_physp   pPHYS;
  mng_uint8p  pRawdata;
  mng_uint32  iRawlen;
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_PHYS, MNG_LC_START)
#endif

  pPHYS = (mng_physp)pChunk;           /* address the proper chunk */

  if (pPHYS->bEmpty)                   /* write empty ? */
    iRetcode = write_raw_chunk (pData, pPHYS->sHeader.iChunkname, 0, 0);
  else
  {
    pRawdata = pData->pWritebuf+8;     /* init output buffer & size */
    iRawlen  = 9;
                                       /* fill the output buffer */
    mng_put_uint32 (pRawdata,   pPHYS->iSizex);
    mng_put_uint32 (pRawdata+4, pPHYS->iSizey);

    *(pRawdata+8) = pPHYS->iUnit;
                                       /* and write it */
    iRetcode = write_raw_chunk (pData, pPHYS->sHeader.iChunkname,
                                iRawlen, pRawdata);
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_PHYS, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */

WRITE_CHUNK (write_sbit)
{
  mng_sbitp   pSBIT;
  mng_uint8p  pRawdata;
  mng_uint32  iRawlen;
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_SBIT, MNG_LC_START)
#endif

  pSBIT = (mng_sbitp)pChunk;           /* address the proper chunk */

  if (pSBIT->bEmpty)                   /* write empty ? */
    iRetcode = write_raw_chunk (pData, pSBIT->sHeader.iChunkname, 0, 0);
  else
  {
    pRawdata = pData->pWritebuf+8;     /* init output buffer & size */
    iRawlen  = 0;                      /* and default size */

    switch (pSBIT->iType)
    {
      case  0: {                       /* gray */
                 iRawlen       = 1;    /* fill the size & output buffer */
                 *pRawdata     = pSBIT->aBits[0];

                 break;
               }
      case  2: {                       /* rgb */
                 iRawlen       = 3;    /* fill the size & output buffer */
                 *pRawdata     = pSBIT->aBits[0];
                 *(pRawdata+1) = pSBIT->aBits[1];
                 *(pRawdata+2) = pSBIT->aBits[2];

                 break;
               }
      case  3: {                       /* indexed */
                 iRawlen       = 1;    /* fill the size & output buffer */
                 *pRawdata     = pSBIT->aBits[0];

                 break;
               }
      case  4: {                       /* gray + alpha */
                 iRawlen       = 2;    /* fill the size & output buffer */
                 *pRawdata     = pSBIT->aBits[0];
                 *(pRawdata+1) = pSBIT->aBits[1];

                 break;
               }
      case  6: {                       /* rgb + alpha */
                 iRawlen       = 4;    /* fill the size & output buffer */
                 *pRawdata     = pSBIT->aBits[0];
                 *(pRawdata+1) = pSBIT->aBits[1];
                 *(pRawdata+2) = pSBIT->aBits[2];
                 *(pRawdata+3) = pSBIT->aBits[3];

                 break;
               }
      case 10: {                       /* jpeg gray */
                 iRawlen       = 1;    /* fill the size & output buffer */
                 *pRawdata     = pSBIT->aBits[0];

                 break;
               }
      case 12: {                       /* jpeg rgb */
                 iRawlen       = 3;    /* fill the size & output buffer */
                 *pRawdata     = pSBIT->aBits[0];
                 *(pRawdata+1) = pSBIT->aBits[1];
                 *(pRawdata+2) = pSBIT->aBits[2];

                 break;
               }
      case 14: {                       /* jpeg gray + alpha */
                 iRawlen       = 2;    /* fill the size & output buffer */
                 *pRawdata     = pSBIT->aBits[0];
                 *(pRawdata+1) = pSBIT->aBits[1];

                 break;
               }
      case 16: {                       /* jpeg rgb + alpha */
                 iRawlen       = 4;    /* fill the size & output buffer */
                 *pRawdata     = pSBIT->aBits[0];
                 *(pRawdata+1) = pSBIT->aBits[1];
                 *(pRawdata+2) = pSBIT->aBits[2];
                 *(pRawdata+3) = pSBIT->aBits[3];

                 break;
               }
    }
                                       /* and write it */
    iRetcode = write_raw_chunk (pData, pSBIT->sHeader.iChunkname,
                                iRawlen, pRawdata);
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_SBIT, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */

WRITE_CHUNK (write_splt)
{
  mng_spltp   pSPLT;
  mng_uint8p  pRawdata;
  mng_uint32  iRawlen;
  mng_retcode iRetcode;
  mng_uint32  iEntrieslen;
  mng_uint8p  pTemp;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_SPLT, MNG_LC_START)
#endif

  pSPLT = (mng_spltp)pChunk;           /* address the proper chunk */

  pRawdata    = pData->pWritebuf+8;    /* init output buffer & size */
  iEntrieslen = ((pSPLT->iSampledepth >> 3) * 4 + 2) * pSPLT->iEntrycount;
  iRawlen     = pSPLT->iNamesize + 2 + iEntrieslen;
                                       /* requires large buffer ? */
  if (iRawlen > pData->iWritebufsize)
    MNG_ALLOC (pData, pRawdata, iRawlen)

  pTemp = pRawdata;                    /* fill the buffer */

  if (pSPLT->iNamesize)
  {
    MNG_COPY (pTemp, pSPLT->zName, pSPLT->iNamesize)
    pTemp += pSPLT->iNamesize;
  }

  *pTemp     = 0;
  *(pTemp+1) = pSPLT->iSampledepth;
  pTemp += 2;

  if (pSPLT->iEntrycount)
    MNG_COPY (pTemp, pSPLT->pEntries, iEntrieslen)
                                       /* and write it */
  iRetcode = write_raw_chunk (pData, pSPLT->sHeader.iChunkname,
                              iRawlen, pRawdata);

  if (iRawlen > pData->iWritebufsize)  /* drop the temp buffer ? */
    MNG_FREEX (pData, pRawdata, iRawlen)

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_SPLT, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */

WRITE_CHUNK (write_hist)
{
  mng_histp   pHIST;
  mng_uint8p  pRawdata;
  mng_uint32  iRawlen;
  mng_retcode iRetcode;
  mng_uint8p  pTemp;
  mng_uint32  iX;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_HIST, MNG_LC_START)
#endif

  pHIST = (mng_histp)pChunk;           /* address the proper chunk */

  pRawdata = pData->pWritebuf+8;       /* init output buffer & size */
  iRawlen  = pHIST->iEntrycount << 1;

  pTemp    = pRawdata;                 /* fill the output buffer */

  for (iX = 0; iX < pHIST->iEntrycount; iX++)
  {
    mng_put_uint16 (pTemp, pHIST->aEntries [iX]);
    pTemp += 2;
  }
                                       /* and write it */
  iRetcode = write_raw_chunk (pData, pHIST->sHeader.iChunkname,
                              iRawlen, pRawdata);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_HIST, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */

WRITE_CHUNK (write_time)
{
  mng_timep   pTIME;
  mng_uint8p  pRawdata;
  mng_uint32  iRawlen;
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_TIME, MNG_LC_START)
#endif

  pTIME = (mng_timep)pChunk;           /* address the proper chunk */

  pRawdata = pData->pWritebuf+8;       /* init output buffer & size */
  iRawlen  = 7;
                                       /* fill the output buffer */
  mng_put_uint16 (pRawdata, pTIME->iYear);

  *(pRawdata+2) = pTIME->iMonth;
  *(pRawdata+3) = pTIME->iDay;
  *(pRawdata+4) = pTIME->iHour;
  *(pRawdata+5) = pTIME->iMinute;
  *(pRawdata+6) = pTIME->iSecond;
                                       /* and write it */
  iRetcode = write_raw_chunk (pData, pTIME->sHeader.iChunkname,
                              iRawlen, pRawdata);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_TIME, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */

WRITE_CHUNK (write_mhdr)
{
  mng_mhdrp   pMHDR;
  mng_uint8p  pRawdata;
  mng_uint32  iRawlen;
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_MHDR, MNG_LC_START)
#endif

  pMHDR = (mng_mhdrp)pChunk;           /* address the proper chunk */

  pRawdata = pData->pWritebuf+8;       /* init output buffer & size */
  iRawlen  = 28;
                                       /* fill the output buffer */
  mng_put_uint32 (pRawdata,    pMHDR->iWidth);
  mng_put_uint32 (pRawdata+4,  pMHDR->iHeight);
  mng_put_uint32 (pRawdata+8,  pMHDR->iTicks);
  mng_put_uint32 (pRawdata+12, pMHDR->iLayercount);
  mng_put_uint32 (pRawdata+16, pMHDR->iFramecount);
  mng_put_uint32 (pRawdata+20, pMHDR->iPlaytime);
  mng_put_uint32 (pRawdata+24, pMHDR->iSimplicity);

                                       /* and write it */
  iRetcode = write_raw_chunk (pData, pMHDR->sHeader.iChunkname,
                              iRawlen, pRawdata);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_MHDR, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */

WRITE_CHUNK (write_mend)
{
  mng_mendp   pMEND;
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_MEND, MNG_LC_START)
#endif

  pMEND = (mng_mendp)pChunk;           /* address the proper chunk */
                                       /* and write it */
  iRetcode = write_raw_chunk (pData, pMEND->sHeader.iChunkname, 0, 0);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_MEND, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */

WRITE_CHUNK (write_loop)
{
  mng_loopp   pLOOP;
  mng_uint8p  pRawdata;
  mng_uint32  iRawlen;
  mng_retcode iRetcode;
  mng_uint8p  pTemp1;
  mng_uint32p pTemp2;
  mng_uint32  iX;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_LOOP, MNG_LC_START)
#endif

  pLOOP = (mng_loopp)pChunk;           /* address the proper chunk */

  pRawdata = pData->pWritebuf+8;       /* init output buffer & size */
  iRawlen  = 5;
                                       /* fill the output buffer */
  *pRawdata = pLOOP->iLevel;
  mng_put_uint32 (pRawdata+1,  pLOOP->iRepeat);

  if (pLOOP->iTermination)
  {
    iRawlen++;
    *(pRawdata+5) = pLOOP->iTermination;

    if ((pLOOP->iCount) ||
        (pLOOP->iItermin != 1) || (pLOOP->iItermax != 0x7FFFFFFFL))
    {
      iRawlen += 8;

      mng_put_uint32 (pRawdata+6,  pLOOP->iItermin);
      mng_put_uint32 (pRawdata+10, pLOOP->iItermax);

      if (pLOOP->iCount)
      {
        iRawlen += pLOOP->iCount * 4;

        pTemp1 = pRawdata+14;
        pTemp2 = pLOOP->pSignals;

        for (iX = 0; iX < pLOOP->iCount; iX++)
        {
          mng_put_uint32 (pTemp1, *pTemp2);

          pTemp1 += 4;
          pTemp2++;
        }
      }
    }
  }
                                       /* and write it */
  iRetcode = write_raw_chunk (pData, pLOOP->sHeader.iChunkname,
                              iRawlen, pRawdata);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_LOOP, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */

WRITE_CHUNK (write_endl)
{
  mng_endlp   pENDL;
  mng_uint8p  pRawdata;
  mng_uint32  iRawlen;
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_ENDL, MNG_LC_START)
#endif

  pENDL     = (mng_endlp)pChunk;       /* address the proper chunk */

  pRawdata  = pData->pWritebuf+8;      /* init output buffer & size */
  iRawlen   = 1;

  *pRawdata = pENDL->iLevel;           /* fill the output buffer */
                                       /* and write it */
  iRetcode  = write_raw_chunk (pData, pENDL->sHeader.iChunkname,
                               iRawlen, pRawdata);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_ENDL, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */

WRITE_CHUNK (write_defi)
{
  mng_defip   pDEFI;
  mng_uint8p  pRawdata;
  mng_uint32  iRawlen;
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_DEFI, MNG_LC_START)
#endif

  pDEFI = (mng_defip)pChunk;           /* address the proper chunk */

  pRawdata = pData->pWritebuf+8;       /* init output buffer & size */
  iRawlen  = 2;
                                       /* fill the output buffer */
  mng_put_uint16 (pRawdata, pDEFI->iObjectid);

  if ((pDEFI->iDonotshow) || (pDEFI->iConcrete) || (pDEFI->bHasloca) || (pDEFI->bHasclip))
  {
    iRawlen++;
    *(pRawdata+2) = pDEFI->iDonotshow;

    if ((pDEFI->iConcrete) || (pDEFI->bHasloca) || (pDEFI->bHasclip))
    {
      iRawlen++;
      *(pRawdata+3) = pDEFI->iConcrete;

      if ((pDEFI->bHasloca) || (pDEFI->bHasclip))
      {
        iRawlen += 8;

        mng_put_uint32 (pRawdata+4, pDEFI->iXlocation);
        mng_put_uint32 (pRawdata+8, pDEFI->iYlocation);

        if (pDEFI->bHasclip)
        {
          iRawlen += 16;

          mng_put_uint32 (pRawdata+12, pDEFI->iLeftcb);
          mng_put_uint32 (pRawdata+16, pDEFI->iRightcb);
          mng_put_uint32 (pRawdata+20, pDEFI->iTopcb);
          mng_put_uint32 (pRawdata+24, pDEFI->iBottomcb);
        }
      }
    }
  }
                                       /* and write it */
  iRetcode = write_raw_chunk (pData, pDEFI->sHeader.iChunkname,
                              iRawlen, pRawdata);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_DEFI, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */

WRITE_CHUNK (write_basi)
{
  mng_basip   pBASI;
  mng_uint8p  pRawdata;
  mng_uint32  iRawlen;
  mng_retcode iRetcode;
  mng_bool    bOpaque;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_BASI, MNG_LC_START)
#endif

  pBASI = (mng_basip)pChunk;           /* address the proper chunk */

  if (pBASI->iBitdepth <= 8)           /* determine opacity alpha-field */
    bOpaque = (mng_bool)(pBASI->iAlpha == 0xFF);
  else
    bOpaque = (mng_bool)(pBASI->iAlpha == 0xFFFF);

  pRawdata = pData->pWritebuf+8;       /* init output buffer & size */
  iRawlen  = 13;
                                       /* fill the output buffer */
  mng_put_uint32 (pRawdata,   pBASI->iWidth);
  mng_put_uint32 (pRawdata+4, pBASI->iHeight);

  *(pRawdata+8)  = pBASI->iBitdepth;
  *(pRawdata+9)  = pBASI->iColortype;
  *(pRawdata+10) = pBASI->iCompression;
  *(pRawdata+11) = pBASI->iFilter;
  *(pRawdata+12) = pBASI->iInterlace;

  if ((pBASI->iRed) || (pBASI->iGreen) || (pBASI->iBlue) ||
      (!bOpaque) || (pBASI->iViewable))
  {
    iRawlen += 6;
    mng_put_uint16 (pRawdata+13, pBASI->iRed);
    mng_put_uint16 (pRawdata+15, pBASI->iGreen);
    mng_put_uint16 (pRawdata+17, pBASI->iBlue);

    if ((!bOpaque) || (pBASI->iViewable))
    {
      iRawlen += 2;
      mng_put_uint16 (pRawdata+19, pBASI->iAlpha);

      if (pBASI->iViewable)
      {
        iRawlen++;
        *(pRawdata+21) = pBASI->iViewable;
      }
    }
  }
                                       /* and write it */
  iRetcode = write_raw_chunk (pData, pBASI->sHeader.iChunkname,
                              iRawlen, pRawdata);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_BASI, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */

WRITE_CHUNK (write_clon)
{
  mng_clonp   pCLON;
  mng_uint8p  pRawdata;
  mng_uint32  iRawlen;
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_CLON, MNG_LC_START)
#endif

  pCLON = (mng_clonp)pChunk;           /* address the proper chunk */

  pRawdata = pData->pWritebuf+8;       /* init output buffer & size */
  iRawlen  = 4;
                                       /* fill the output buffer */
  mng_put_uint16 (pRawdata,   pCLON->iSourceid);
  mng_put_uint16 (pRawdata+2, pCLON->iCloneid);

  if ((pCLON->iClonetype) || (pCLON->iDonotshow) || (pCLON->iConcrete) || (pCLON->bHasloca))
  {
    iRawlen++;
    *(pRawdata+4) = pCLON->iClonetype;

    if ((pCLON->iDonotshow) || (pCLON->iConcrete) || (pCLON->bHasloca))
    {
      iRawlen++;
      *(pRawdata+5) = pCLON->iDonotshow;

      if ((pCLON->iConcrete) || (pCLON->bHasloca))
      {
        iRawlen++;
        *(pRawdata+6) = pCLON->iConcrete;

        if (pCLON->bHasloca)
        {
          iRawlen += 9;
          *(pRawdata+7) = pCLON->iLocationtype;
          mng_put_int32 (pRawdata+8,  pCLON->iLocationx);
          mng_put_int32 (pRawdata+12, pCLON->iLocationy);
        }
      }
    }
  }
                                       /* and write it */
  iRetcode = write_raw_chunk (pData, pCLON->sHeader.iChunkname,
                              iRawlen, pRawdata);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_CLON, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */

WRITE_CHUNK (write_past)
{
  mng_pastp        pPAST;
  mng_uint8p       pRawdata;
  mng_uint32       iRawlen;
  mng_retcode      iRetcode;
  mng_past_sourcep pSource;
  mng_uint32       iX;
  mng_uint8p       pTemp;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_PAST, MNG_LC_START)
#endif

  pPAST = (mng_pastp)pChunk;           /* address the proper chunk */

  pRawdata = pData->pWritebuf+8;       /* init output buffer & size */
  iRawlen  = 11 + (30 * pPAST->iCount);
                                       /* requires large buffer ? */
  if (iRawlen > pData->iWritebufsize)
    MNG_ALLOC (pData, pRawdata, iRawlen)
                                       /* fill the output buffer */
  mng_put_uint16 (pRawdata,   pPAST->iDestid);

  *(pRawdata+2) = pPAST->iTargettype;

  mng_put_int32  (pRawdata+3, pPAST->iTargetx);
  mng_put_int32  (pRawdata+7, pPAST->iTargety);

  pTemp   = pRawdata+11;
  pSource = pPAST->pSources;

  for (iX = 0; iX < pPAST->iCount; iX++)
  {
    mng_put_uint16 (pTemp,    pSource->iSourceid);

    *(pTemp+2)  = pSource->iComposition;
    *(pTemp+3)  = pSource->iOrientation;
    *(pTemp+4)  = pSource->iOffsettype;

    mng_put_int32  (pTemp+5,  pSource->iOffsetx);
    mng_put_int32  (pTemp+9,  pSource->iOffsety);

    *(pTemp+13) = pSource->iBoundarytype;

    mng_put_int32  (pTemp+14, pSource->iBoundaryl);
    mng_put_int32  (pTemp+18, pSource->iBoundaryr);
    mng_put_int32  (pTemp+22, pSource->iBoundaryt);
    mng_put_int32  (pTemp+26, pSource->iBoundaryb);

    pSource++;
    pTemp += 30;
  }
                                       /* and write it */
  iRetcode = write_raw_chunk (pData, pPAST->sHeader.iChunkname,
                              iRawlen, pRawdata);
                                       /* free temporary buffer ? */
  if (iRawlen > pData->iWritebufsize)
    MNG_FREEX (pData, pRawdata, iRawlen)

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_PAST, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */

WRITE_CHUNK (write_disc)
{
  mng_discp        pDISC;
  mng_uint8p       pRawdata;
  mng_uint32       iRawlen;
  mng_retcode      iRetcode;
  mng_uint32       iX;
  mng_uint8p       pTemp1;
  mng_uint16p      pTemp2;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_DISC, MNG_LC_START)
#endif

  pDISC    = (mng_discp)pChunk;        /* address the proper chunk */

  pRawdata = pData->pWritebuf+8;       /* init output buffer & size */
  iRawlen  = pDISC->iCount << 1;

  pTemp1   = pRawdata;                 /* fill the output buffer */
  pTemp2   = pDISC->pObjectids;

  for (iX = 0; iX < pDISC->iCount; iX++)
  {
    mng_put_uint16 (pTemp1, *pTemp2);

    pTemp2++;
    pTemp1 += 2;
  }
                                       /* and write it */
  iRetcode = write_raw_chunk (pData, pDISC->sHeader.iChunkname,
                              iRawlen, pRawdata);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_DISC, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */

WRITE_CHUNK (write_back)
{
  mng_backp   pBACK;
  mng_uint8p  pRawdata;
  mng_uint32  iRawlen;
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_BACK, MNG_LC_START)
#endif

  pBACK = (mng_backp)pChunk;           /* address the proper chunk */

  pRawdata = pData->pWritebuf+8;       /* init output buffer & size */
  iRawlen  = 6;
                                       /* fill the output buffer */
  mng_put_uint16 (pRawdata,   pBACK->iRed);
  mng_put_uint16 (pRawdata+2, pBACK->iGreen);
  mng_put_uint16 (pRawdata+4, pBACK->iBlue);

  if ((pBACK->iMandatory) || (pBACK->iImageid) || (pBACK->iTile))
  {
    iRawlen++;
    *(pRawdata+6) = pBACK->iMandatory;

    if ((pBACK->iImageid) || (pBACK->iTile))
    {
      iRawlen += 2;
      mng_put_uint16 (pRawdata+7, pBACK->iImageid);

      if (pBACK->iTile)
      {
        iRawlen++;
        *(pRawdata+9) = pBACK->iTile;
      }
    }
  }
                                       /* and write it */
  iRetcode = write_raw_chunk (pData, pBACK->sHeader.iChunkname,
                              iRawlen, pRawdata);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_BACK, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */

WRITE_CHUNK (write_fram)
{
  mng_framp   pFRAM;
  mng_uint8p  pRawdata;
  mng_uint32  iRawlen;
  mng_retcode iRetcode;
  mng_uint8p  pTemp;
  mng_uint32p pTemp2;
  mng_uint32  iX;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_FRAM, MNG_LC_START)
#endif

  pFRAM = (mng_framp)pChunk;           /* address the proper chunk */

  if (pFRAM->bEmpty)                   /* empty ? */
    iRetcode = write_raw_chunk (pData, pFRAM->sHeader.iChunkname, 0, 0);
  else
  {
    pRawdata = pData->pWritebuf+8;     /* init output buffer & size */
    iRawlen  = 1;
                                       /* fill the output buffer */
    *pRawdata = pFRAM->iMode;

    if ((pFRAM->iNamesize      ) ||
        (pFRAM->iChangedelay   ) || (pFRAM->iChangetimeout) ||
        (pFRAM->iChangeclipping) || (pFRAM->iChangesyncid )    )
    {
      if (pFRAM->iNamesize)
        MNG_COPY (pRawdata+1, pFRAM->zName, pFRAM->iNamesize)

      iRawlen += pFRAM->iNamesize;
      pTemp = pRawdata + pFRAM->iNamesize + 1;

      if ((pFRAM->iChangedelay   ) || (pFRAM->iChangetimeout) ||
          (pFRAM->iChangeclipping) || (pFRAM->iChangesyncid )    )
      {
        *pTemp     = 0;
        *(pTemp+1) = pFRAM->iChangedelay;
        *(pTemp+2) = pFRAM->iChangetimeout;
        *(pTemp+3) = pFRAM->iChangeclipping;
        *(pTemp+4) = pFRAM->iChangesyncid;

        iRawlen += 5;
        pTemp   += 5;

        if (pFRAM->iChangedelay)
        {
          mng_put_uint32 (pTemp, pFRAM->iDelay);
          iRawlen += 4;
          pTemp   += 4;
        }

        if (pFRAM->iChangetimeout)
        {
          mng_put_uint32 (pTemp, pFRAM->iTimeout);
          iRawlen += 4;
          pTemp   += 4;
        }

        if (pFRAM->iChangeclipping)
        {
          *pTemp = pFRAM->iBoundarytype;

          mng_put_uint32 (pTemp+1,  pFRAM->iBoundaryl);
          mng_put_uint32 (pTemp+5,  pFRAM->iBoundaryr);
          mng_put_uint32 (pTemp+9,  pFRAM->iBoundaryt);
          mng_put_uint32 (pTemp+13, pFRAM->iBoundaryb);

          iRawlen += 17;
          pTemp   += 17;
        }

        if (pFRAM->iChangesyncid)
        {
          iRawlen += pFRAM->iCount * 4;
          pTemp2 = pFRAM->pSyncids;

          for (iX = 0; iX < pFRAM->iCount; iX++)
          {
            mng_put_uint32 (pTemp, *pTemp2);

            pTemp2++;
            pTemp += 4;
          }  
        }
      }
    }
                                       /* and write it */
    iRetcode = write_raw_chunk (pData, pFRAM->sHeader.iChunkname,
                                iRawlen, pRawdata);
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_FRAM, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */

WRITE_CHUNK (write_move)
{
  mng_movep   pMOVE;
  mng_uint8p  pRawdata;
  mng_uint32  iRawlen;
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_MOVE, MNG_LC_START)
#endif

  pMOVE = (mng_movep)pChunk;           /* address the proper chunk */

  pRawdata = pData->pWritebuf+8;       /* init output buffer & size */
  iRawlen  = 13;
                                       /* fill the output buffer */
  mng_put_uint16 (pRawdata,   pMOVE->iFirstid);
  mng_put_uint16 (pRawdata+2, pMOVE->iLastid);

  *(pRawdata+4) = pMOVE->iMovetype;

  mng_put_int32  (pRawdata+5, pMOVE->iMovex);
  mng_put_int32  (pRawdata+9, pMOVE->iMovey);
                                       /* and write it */
  iRetcode = write_raw_chunk (pData, pMOVE->sHeader.iChunkname,
                              iRawlen, pRawdata);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_MOVE, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */

WRITE_CHUNK (write_clip)
{
  mng_clipp   pCLIP;
  mng_uint8p  pRawdata;
  mng_uint32  iRawlen;
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_CLIP, MNG_LC_START)
#endif

  pCLIP = (mng_clipp)pChunk;           /* address the proper chunk */

  pRawdata = pData->pWritebuf+8;       /* init output buffer & size */
  iRawlen  = 21;
                                       /* fill the output buffer */
  mng_put_uint16 (pRawdata,    pCLIP->iFirstid);
  mng_put_uint16 (pRawdata+2,  pCLIP->iLastid);

  *(pRawdata+4) = pCLIP->iCliptype;

  mng_put_int32  (pRawdata+5,  pCLIP->iClipl);
  mng_put_int32  (pRawdata+9,  pCLIP->iClipr);
  mng_put_int32  (pRawdata+13, pCLIP->iClipt);
  mng_put_int32  (pRawdata+17, pCLIP->iClipb);
                                       /* and write it */
  iRetcode = write_raw_chunk (pData, pCLIP->sHeader.iChunkname,
                              iRawlen, pRawdata);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_CLIP, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */

WRITE_CHUNK (write_show)
{
  mng_showp   pSHOW;
  mng_uint8p  pRawdata;
  mng_uint32  iRawlen;
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_SHOW, MNG_LC_START)
#endif

  pSHOW = (mng_showp)pChunk;           /* address the proper chunk */

  if (pSHOW->bEmpty)                   /* empty ? */
    iRetcode = write_raw_chunk (pData, pSHOW->sHeader.iChunkname, 0, 0);
  else
  {
    pRawdata = pData->pWritebuf+8;     /* init output buffer & size */
    iRawlen  = 2;
                                       /* fill the output buffer */
    mng_put_uint16 (pRawdata, pSHOW->iFirstid);

    if ((pSHOW->iLastid != pSHOW->iFirstid) || (pSHOW->iMode))
    {
      iRawlen += 2;
      mng_put_uint16 (pRawdata+2, pSHOW->iLastid);

      if (pSHOW->iMode)
      {
        iRawlen++;
        *(pRawdata+4) = pSHOW->iMode;
      }
    }
                                       /* and write it */
    iRetcode = write_raw_chunk (pData, pSHOW->sHeader.iChunkname,
                                iRawlen, pRawdata);
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_SHOW, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */

WRITE_CHUNK (write_term)
{
  mng_termp   pTERM;
  mng_uint8p  pRawdata;
  mng_uint32  iRawlen;
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_TERM, MNG_LC_START)
#endif

  pTERM     = (mng_termp)pChunk;       /* address the proper chunk */

  pRawdata  = pData->pWritebuf+8;      /* init output buffer & size */
  iRawlen   = 1;

  *pRawdata = pTERM->iTermaction;      /* fill the output buffer */

  if (pTERM->iTermaction == 3)
  {
    iRawlen       = 10;
    *(pRawdata+1) = pTERM->iIteraction;

    mng_put_uint32 (pRawdata+2, pTERM->iDelay);
    mng_put_uint32 (pRawdata+6, pTERM->iItermax);
  }
                                       /* and write it */
  iRetcode = write_raw_chunk (pData, pTERM->sHeader.iChunkname,
                              iRawlen, pRawdata);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_TERM, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */

WRITE_CHUNK (write_save)
{
  mng_savep       pSAVE;
  mng_uint8p      pRawdata;
  mng_uint32      iRawlen;
  mng_retcode     iRetcode;
  mng_save_entryp pEntry;
  mng_uint32      iEntrysize;
  mng_uint8p      pTemp;
  mng_uint32      iX;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_SAVE, MNG_LC_START)
#endif

  pSAVE = (mng_savep)pChunk;           /* address the proper chunk */

  if (pSAVE->bEmpty)                   /* empty ? */
    iRetcode = write_raw_chunk (pData, pSAVE->sHeader.iChunkname, 0, 0);
  else
  {
    pRawdata  = pData->pWritebuf+8;    /* init output buffer & size */
    iRawlen   = 1;

    *pRawdata = pSAVE->iOffsettype;    /* fill the output buffer */

    if (pSAVE->iOffsettype == 16)
      iEntrysize = 25;
    else
      iEntrysize = 17;

    pTemp  = pRawdata+1;
    pEntry = pSAVE->pEntries;

    for (iX = 0; iX < pSAVE->iCount; iX++)
    {
      if (iX)                          /* put separator null-byte, except the first */
      {
        *pTemp = 0;
        pTemp++;
        iRawlen++;
      }

      iRawlen += iEntrysize + pEntry->iNamesize;
      *pTemp = pEntry->iEntrytype;

      if (pSAVE->iOffsettype == 16)
      {
        mng_put_uint32 (pTemp+1,  pEntry->iOffset[0]);
        mng_put_uint32 (pTemp+5,  pEntry->iOffset[1]);
        mng_put_uint32 (pTemp+9,  pEntry->iStarttime[0]);
        mng_put_uint32 (pTemp+13, pEntry->iStarttime[1]);
        mng_put_uint32 (pTemp+17, pEntry->iLayernr);
        mng_put_uint32 (pTemp+21, pEntry->iFramenr);

        pTemp += 25;
      }
      else
      {
        mng_put_uint32 (pTemp+1,  pEntry->iOffset[1]);
        mng_put_uint32 (pTemp+5,  pEntry->iStarttime[1]);
        mng_put_uint32 (pTemp+9,  pEntry->iLayernr);
        mng_put_uint32 (pTemp+13, pEntry->iFramenr);

        pTemp += 17;
      }

      if (pEntry->iNamesize)
      {
        MNG_COPY (pTemp, pEntry->zName, pEntry->iNamesize);
        pTemp += pEntry->iNamesize;
      }

      pEntry++;  
    }
                                       /* and write it */
    iRetcode = write_raw_chunk (pData, pSAVE->sHeader.iChunkname,
                                iRawlen, pRawdata);
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_SAVE, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */

WRITE_CHUNK (write_seek)
{
  mng_seekp   pSEEK;
  mng_uint8p  pRawdata;
  mng_uint32  iRawlen;
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_SEEK, MNG_LC_START)
#endif

  pSEEK    = (mng_seekp)pChunk;        /* address the proper chunk */

  pRawdata = pData->pWritebuf+8;       /* init output buffer & size */
  iRawlen  = pSEEK->iNamesize;

  if (iRawlen)                         /* fill the output buffer */
    MNG_COPY (pRawdata, pSEEK->zName, iRawlen)
                                       /* and write it */
  iRetcode = write_raw_chunk (pData, pSEEK->sHeader.iChunkname,
                              iRawlen, pRawdata);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_SEEK, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */

WRITE_CHUNK (write_expi)
{
  mng_expip   pEXPI;
  mng_uint8p  pRawdata;
  mng_uint32  iRawlen;
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_EXPI, MNG_LC_START)
#endif

  pEXPI    = (mng_expip)pChunk;        /* address the proper chunk */

  pRawdata = pData->pWritebuf+8;       /* init output buffer & size */
  iRawlen  = 2 + pEXPI->iNamesize;
                                       /* fill the output buffer */
  mng_put_uint16 (pRawdata, pEXPI->iSnapshotid);

  if (pEXPI->iNamesize)
    MNG_COPY (pRawdata+2, pEXPI->zName, pEXPI->iNamesize)
                                       /* and write it */
  iRetcode = write_raw_chunk (pData, pEXPI->sHeader.iChunkname,
                              iRawlen, pRawdata);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_EXPI, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */

WRITE_CHUNK (write_fpri)
{
  mng_fprip   pFPRI;
  mng_uint8p  pRawdata;
  mng_uint32  iRawlen;
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_FPRI, MNG_LC_START)
#endif

  pFPRI         = (mng_fprip)pChunk;   /* address the proper chunk */

  pRawdata      = pData->pWritebuf+8;  /* init output buffer & size */
  iRawlen       = 2;

  *pRawdata     = pFPRI->iDeltatype;   /* fill the output buffer */
  *(pRawdata+1) = pFPRI->iPriority;
                                       /* and write it */
  iRetcode = write_raw_chunk (pData, pFPRI->sHeader.iChunkname,
                              iRawlen, pRawdata);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_FPRI, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */

WRITE_CHUNK (write_need)
{
  mng_needp   pNEED;
  mng_uint8p  pRawdata;
  mng_uint32  iRawlen;
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_NEED, MNG_LC_START)
#endif

  pNEED    = (mng_needp)pChunk;        /* address the proper chunk */

  pRawdata = pData->pWritebuf+8;       /* init output buffer & size */
  iRawlen  = pNEED->iKeywordssize;
                                       /* fill the output buffer */
  if (pNEED->iKeywordssize)
    MNG_COPY (pRawdata, pNEED->zKeywords, pNEED->iKeywordssize)
                                       /* and write it */
  iRetcode = write_raw_chunk (pData, pNEED->sHeader.iChunkname,
                              iRawlen, pRawdata);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_NEED, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */

WRITE_CHUNK (write_phyg)
{
  mng_phygp   pPHYG;
  mng_uint8p  pRawdata;
  mng_uint32  iRawlen;
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_PHYG, MNG_LC_START)
#endif

  pPHYG = (mng_phygp)pChunk;           /* address the proper chunk */

  if (pPHYG->bEmpty)                   /* write empty ? */
    iRetcode = write_raw_chunk (pData, pPHYG->sHeader.iChunkname, 0, 0);
  else
  {
    pRawdata = pData->pWritebuf+8;     /* init output buffer & size */
    iRawlen  = 9;
                                       /* fill the output buffer */
    mng_put_uint32 (pRawdata,   pPHYG->iSizex);
    mng_put_uint32 (pRawdata+4, pPHYG->iSizey);

    *(pRawdata+8) = pPHYG->iUnit;
                                       /* and write it */
    iRetcode = write_raw_chunk (pData, pPHYG->sHeader.iChunkname,
                                iRawlen, pRawdata);
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_PHYG, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */

/* B004 */
#ifdef MNG_INCLUDE_JNG
/* B004 */
WRITE_CHUNK (write_jhdr)
{
  mng_jhdrp   pJHDR;
  mng_uint8p  pRawdata;
  mng_uint32  iRawlen;
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_JHDR, MNG_LC_START)
#endif

  pJHDR    = (mng_jhdrp)pChunk;        /* address the proper chunk */
  pRawdata = pData->pWritebuf+8;       /* init output buffer & size */
  iRawlen  = 16;
                                       /* fill the output buffer */
  mng_put_uint32 (pRawdata,   pJHDR->iWidth);
  mng_put_uint32 (pRawdata+4, pJHDR->iHeight);

  *(pRawdata+8)  = pJHDR->iColortype;
  *(pRawdata+9)  = pJHDR->iImagesampledepth;
  *(pRawdata+10) = pJHDR->iImagecompression;
  *(pRawdata+11) = pJHDR->iImageinterlace;
  *(pRawdata+12) = pJHDR->iAlphasampledepth;
  *(pRawdata+13) = pJHDR->iAlphacompression;
  *(pRawdata+14) = pJHDR->iAlphafilter;
  *(pRawdata+15) = pJHDR->iAlphainterlace;
                                       /* and write it */
  iRetcode = write_raw_chunk (pData, pJHDR->sHeader.iChunkname, iRawlen, pRawdata);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_JHDR, MNG_LC_END)
#endif

  return iRetcode;
}
#else
#define write_jhdr 0
/* B004 */
#endif /* MNG_INCLUDE_JNG */
/* B004 */

/* ************************************************************************** */

#ifdef MNG_INCLUDE_JNG
WRITE_CHUNK (write_jdaa)
{
  mng_jdatp   pJDAA;
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_JDAA, MNG_LC_START)
#endif

  pJDAA = (mng_jdaap)pChunk;           /* address the proper chunk */

  if (pJDAA->bEmpty)                   /* and write it */
    iRetcode = write_raw_chunk (pData, pJDAA->sHeader.iChunkname, 0, 0);
  else
    iRetcode = write_raw_chunk (pData, pJDAA->sHeader.iChunkname,
                                pJDAA->iDatasize, pJDAA->pData);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_JDAA, MNG_LC_END)
#endif

  return iRetcode;
}
#else
#define write_jdaa 0
#endif /* MNG_INCLUDE_JNG */

/* ************************************************************************** */

/* B004 */
#ifdef MNG_INCLUDE_JNG
/* B004 */
WRITE_CHUNK (write_jdat)
{
  mng_jdatp   pJDAT;
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_JDAT, MNG_LC_START)
#endif

  pJDAT = (mng_jdatp)pChunk;           /* address the proper chunk */

  if (pJDAT->bEmpty)                   /* and write it */
    iRetcode = write_raw_chunk (pData, pJDAT->sHeader.iChunkname, 0, 0);
  else
    iRetcode = write_raw_chunk (pData, pJDAT->sHeader.iChunkname,
                                pJDAT->iDatasize, pJDAT->pData);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_JDAT, MNG_LC_END)
#endif

  return iRetcode;
}
#else
#define write_jdat 0
/* B004 */
#endif /* MNG_INCLUDE_JNG */
/* B004 */

/* ************************************************************************** */

/* B004 */
#ifdef MNG_INCLUDE_JNG
/* B004 */
WRITE_CHUNK (write_jsep)
{
  mng_jsepp   pJSEP;
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_JSEP, MNG_LC_START)
#endif

  pJSEP = (mng_jsepp)pChunk;           /* address the proper chunk */
                                       /* and write it */
  iRetcode = write_raw_chunk (pData, pJSEP->sHeader.iChunkname, 0, 0);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_JSEP, MNG_LC_END)
#endif

  return iRetcode;
}
#else
#define write_jsep 0
/* B004 */
#endif /* MNG_INCLUDE_JNG */
/* B004 */

/* ************************************************************************** */

WRITE_CHUNK (write_dhdr)
{
  mng_dhdrp   pDHDR;
  mng_uint8p  pRawdata;
  mng_uint32  iRawlen;
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_DHDR, MNG_LC_START)
#endif

  pDHDR    = (mng_dhdrp)pChunk;        /* address the proper chunk */

  pRawdata = pData->pWritebuf+8;       /* init output buffer & size */
  iRawlen  = 4;
                                       /* fill the output buffer */
  mng_put_uint16 (pRawdata, pDHDR->iObjectid);

  *(pRawdata+2) = pDHDR->iImagetype;
  *(pRawdata+3) = pDHDR->iDeltatype;

  if (pDHDR->iDeltatype != 7)
  {
    iRawlen += 8;
    mng_put_uint32 (pRawdata+4, pDHDR->iBlockwidth);
    mng_put_uint32 (pRawdata+8, pDHDR->iBlockheight);

    if (pDHDR->iDeltatype != 0)
    {
      iRawlen += 8;
      mng_put_uint32 (pRawdata+12, pDHDR->iBlockx);
      mng_put_uint32 (pRawdata+16, pDHDR->iBlocky);
    }
  }
                                       /* and write it */
  iRetcode = write_raw_chunk (pData, pDHDR->sHeader.iChunkname,
                              iRawlen, pRawdata);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_DHDR, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */

WRITE_CHUNK (write_prom)
{
  mng_promp   pPROM;
  mng_uint8p  pRawdata;
  mng_uint32  iRawlen;
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_PROM, MNG_LC_START)
#endif

  pPROM    = (mng_promp)pChunk;        /* address the proper chunk */

  pRawdata = pData->pWritebuf+8;       /* init output buffer & size */
  iRawlen  = 3;

  *pRawdata     = pPROM->iColortype;   /* fill the output buffer */
  *(pRawdata+1) = pPROM->iSampledepth;
  *(pRawdata+2) = pPROM->iFilltype;
                                       /* and write it */
  iRetcode = write_raw_chunk (pData, pPROM->sHeader.iChunkname,
                              iRawlen, pRawdata);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_PROM, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */

WRITE_CHUNK (write_ipng)
{
  mng_ipngp   pIPNG;
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_IPNG, MNG_LC_START)
#endif

  pIPNG = (mng_ipngp)pChunk;           /* address the proper chunk */
                                       /* and write it */
  iRetcode = write_raw_chunk (pData, pIPNG->sHeader.iChunkname, 0, 0);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_IPNG, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */

WRITE_CHUNK (write_pplt)
{
  mng_ppltp       pPPLT;
  mng_uint8p      pRawdata;
  mng_uint32      iRawlen;
  mng_retcode     iRetcode;
  mng_pplt_entryp pEntry;
  mng_uint8p      pTemp;
  mng_uint32      iX;
  mng_bool        bHasgroup;
  mng_uint8p      pLastid = 0;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_PPLT, MNG_LC_START)
#endif

  pPPLT = (mng_ppltp)pChunk;           /* address the proper chunk */

  pRawdata  = pData->pWritebuf+8;      /* init output buffer & size */
  iRawlen   = 1;

  *pRawdata = pPPLT->iDeltatype;       /* fill the output buffer */

  pTemp     = pRawdata+1;
  bHasgroup = MNG_FALSE;

  for (iX = 0; iX < pPPLT->iCount; iX++)
  {
    pEntry = &pPPLT->aEntries[iX];
    
    if (pEntry->bUsed)                 /* valid entry ? */
    {
      if (!bHasgroup)                  /* start a new group ? */
      {
        bHasgroup  = MNG_TRUE;
        pLastid    = pTemp+1;

        *pTemp     = (mng_uint8)iX;
        *(pTemp+1) = 0;

        pTemp += 2;
      }

      switch (pPPLT->iDeltatype)       /* add group-entry depending on type */
      {
        case 0: ;
        case 1: {
                  *pTemp     = pEntry->iRed;
                  *(pTemp+1) = pEntry->iGreen;
                  *(pTemp+2) = pEntry->iBlue;

                  pTemp += 3;

                  break;
                }

        case 2: ;
        case 3: {
                  *pTemp     = pEntry->iAlpha;

                  pTemp++;

                  break;
                }

        case 4: ;
        case 5: {
                  *pTemp     = pEntry->iRed;
                  *(pTemp+1) = pEntry->iGreen;
                  *(pTemp+2) = pEntry->iBlue;
                  *(pTemp+3) = pEntry->iAlpha;

                  pTemp += 4;

                  break;
                }

      }
    }
    else
    {
      if (bHasgroup)                   /* finish off a group ? */
        *pLastid = (mng_uint8)(iX-1);

      bHasgroup = MNG_FALSE;
    }
  }

  if (bHasgroup)                       /* last group unfinished ? */
    *pLastid = (mng_uint8)(pPPLT->iCount-1);
                                       /* write the output buffer */
  iRetcode = write_raw_chunk (pData, pPPLT->sHeader.iChunkname,
                              iRawlen, pRawdata);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_PPLT, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */

WRITE_CHUNK (write_ijng)
{
  mng_ijngp   pIJNG;
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_IJNG, MNG_LC_START)
#endif

  pIJNG = (mng_ijngp)pChunk;           /* address the proper chunk */
                                       /* and write it */
  iRetcode = write_raw_chunk (pData, pIJNG->sHeader.iChunkname, 0, 0);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_IJNG, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */

WRITE_CHUNK (write_drop)
{
  mng_dropp        pDROP;
  mng_uint8p       pRawdata;
  mng_uint32       iRawlen;
  mng_retcode      iRetcode;
  mng_uint32       iX;
  mng_uint8p       pTemp1;
  mng_chunkidp     pTemp2;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_DROP, MNG_LC_START)
#endif

  pDROP    = (mng_dropp)pChunk;        /* address the proper chunk */

  pRawdata = pData->pWritebuf+8;       /* init output buffer & size */
  iRawlen  = pDROP->iCount << 2;

  pTemp1   = pRawdata;                 /* fill the output buffer */
  pTemp2   = pDROP->pChunknames;

  for (iX = 0; iX < pDROP->iCount; iX++)
  {
    mng_put_uint32 (pTemp1, (mng_uint32)*pTemp2);

    pTemp2++;
    pTemp1 += 4;
  }
                                       /* and write it */
  iRetcode = write_raw_chunk (pData, pDROP->sHeader.iChunkname,
                              iRawlen, pRawdata);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_DROP, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */

WRITE_CHUNK (write_dbyk)
{
  mng_dbykp   pDBYK;
  mng_uint8p  pRawdata;
  mng_uint32  iRawlen;
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_DBYK, MNG_LC_START)
#endif

  pDBYK = (mng_dbykp)pChunk;           /* address the proper chunk */

  pRawdata = pData->pWritebuf+8;       /* init output buffer & size */
  iRawlen  = 5 + pDBYK->iKeywordssize;
                                       /* fill the output buffer */
  mng_put_uint32 (pRawdata, pDBYK->iChunkname);
  *(pRawdata+4) = pDBYK->iPolarity;

  if (pDBYK->iKeywordssize)
    MNG_COPY (pRawdata+5, pDBYK->zKeywords, pDBYK->iKeywordssize)
                                       /* and write it */
  iRetcode = write_raw_chunk (pData, pDBYK->sHeader.iChunkname,
                              iRawlen, pRawdata);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_DBYK, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */

WRITE_CHUNK (write_ordr)
{
  mng_ordrp       pORDR;
  mng_uint8p      pRawdata;
  mng_uint32      iRawlen;
  mng_retcode     iRetcode;
  mng_uint8p      pTemp;
  mng_ordr_entryp pEntry;
  mng_uint32      iX;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_ORDR, MNG_LC_START)
#endif

  pORDR    = (mng_ordrp)pChunk;        /* address the proper chunk */

  pRawdata = pData->pWritebuf+8;       /* init output buffer & size */
  iRawlen  = pORDR->iCount * 5;

  pTemp    = pRawdata;                 /* fill the output buffer */
  pEntry   = pORDR->pEntries;

  for (iX = 0; iX < pORDR->iCount; iX++)
  {
    mng_put_uint32 (pTemp, pEntry->iChunkname);
    *(pTemp+4) = pEntry->iOrdertype;
    pTemp += 5;
    pEntry++;
  }
                                       /* and write it */
  iRetcode = write_raw_chunk (pData, pORDR->sHeader.iChunkname,
                              iRawlen, pRawdata);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_ORDR, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */

WRITE_CHUNK (write_magn)
{
  mng_magnp   pMAGN;
  mng_uint8p  pRawdata;
  mng_uint32  iRawlen;
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_MAGN, MNG_LC_START)
#endif

  pMAGN    = (mng_magnp)pChunk;        /* address the proper chunk */

  pRawdata = pData->pWritebuf+8;       /* init output buffer & size */
  iRawlen  = 20;
                                       /* fill the output buffer */
  mng_put_uint16 (pRawdata,    pMAGN->iFirstid);
  mng_put_uint16 (pRawdata+2,  pMAGN->iLastid);
  mng_put_uint16 (pRawdata+4,  pMAGN->iMethodX);
  mng_put_uint16 (pRawdata+6,  pMAGN->iMX);
  mng_put_uint16 (pRawdata+8,  pMAGN->iMY);
  mng_put_uint16 (pRawdata+10, pMAGN->iML);
  mng_put_uint16 (pRawdata+12, pMAGN->iMR);
  mng_put_uint16 (pRawdata+14, pMAGN->iMT);
  mng_put_uint16 (pRawdata+16, pMAGN->iMB);
  mng_put_uint16 (pRawdata+18, pMAGN->iMethodY);
                                       /* optimize length */
  if (pMAGN->iMethodY == pMAGN->iMethodX)
  {
    iRawlen -= 2;

    if (pMAGN->iMB == pMAGN->iMY)
    {
      iRawlen -= 2;

      if (pMAGN->iMT == pMAGN->iMY)
      {
        iRawlen -= 2;

        if (pMAGN->iMR == pMAGN->iMX)
        {
          iRawlen -= 2;

          if (pMAGN->iML == pMAGN->iMX)
          {
            iRawlen -= 2;

            if (pMAGN->iMY == pMAGN->iMX)
            {
              iRawlen -= 2;

              if (pMAGN->iMX == 1)
              {
                iRawlen -= 2;

                if (pMAGN->iMethodX == 0)
                {
                  iRawlen -= 2;

                  if (pMAGN->iLastid == pMAGN->iFirstid)
                  {
                    iRawlen -= 2;

                    if (pMAGN->iFirstid == 0)
                      iRawlen = 0;

                  }
                }
              }
            }
          }
        }
      }
    }
  }
                                       /* and write it */
  iRetcode = write_raw_chunk (pData, pMAGN->sHeader.iChunkname,
                              iRawlen, pRawdata);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_MAGN, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */

WRITE_CHUNK (write_unknown)
{
  mng_unknown_chunkp pUnknown;
  mng_retcode        iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_UNKNOWN, MNG_LC_START)
#endif
                                       /* address the proper chunk */
  pUnknown = (mng_unknown_chunkp)pChunk;
                                       /* and write it */
  iRetcode = write_raw_chunk (pData, pUnknown->sHeader.iChunkname,
                              pUnknown->iDatasize, pUnknown->pData);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_WRITE_UNKNOWN, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */

#endif /* MNG_INCLUDE_WRITE_PROCS */

/* ************************************************************************** */
/* * end of file                                                            * */
/* ************************************************************************** */





