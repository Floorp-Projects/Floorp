/* ************************************************************************** */
/* *             For conditions of distribution and use,                    * */
/* *                see copyright notice in libmng.h                        * */
/* ************************************************************************** */
/* *                                                                        * */
/* * project   : libmng                                                     * */
/* * file      : libmng_zlib.c             copyright (c) 2000 G.Juyn        * */
/* * version   : 1.0.0                                                      * */
/* *                                                                        * */
/* * purpose   : ZLIB library interface (implementation)                    * */
/* *                                                                        * */
/* * author    : G.Juyn                                                     * */
/* * web       : http://www.3-t.com                                         * */
/* * email     : mailto:info@3-t.com                                        * */
/* *                                                                        * */
/* * comment   : implementation of the ZLIB library interface               * */
/* *                                                                        * */
/* * changes   : 0.5.1 - 05/08/2000 - G.Juyn                                * */
/* *             - changed strict-ANSI stuff                                * */
/* *             0.5.1 - 05/11/2000 - G.Juyn                                * */
/* *             - filled the deflatedata routine                           * */
/* *             0.5.1 - 05/12/2000 - G.Juyn                                * */
/* *             - changed trace to macro for callback error-reporting      * */
/* *                                                                        * */
/* *             0.5.2 - 05/20/2000 - G.Juyn                                * */
/* *             - fixed for JNG alpha handling                             * */
/* *             0.5.2 - 05/24/2000 - G.Juyn                                * */
/* *             - moved init of default zlib parms from here to            * */
/* *               "mng_hlapi.c"                                            * */
/* *                                                                        * */
/* *             0.5.3 - 06/16/2000 - G.Juyn                                * */
/* *             - changed progressive-display processing                   * */
/* *                                                                        * */
/* *             0.9.2 - 08/05/2000 - G.Juyn                                * */
/* *             - changed file-prefixes                                    * */
/* *                                                                        * */
/* *             0.9.3 - 08/08/2000 - G.Juyn                                * */
/* *             - fixed compiler-warnings from Mozilla                     * */
/* *             0.9.3 - 09/07/2000 - G.Juyn                                * */
/* *             - added support for new filter_types                       * */
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
#include "libmng_pixels.h"
#include "libmng_filter.h"
#include "libmng_zlib.h"

#if defined(__BORLANDC__) && defined(MNG_STRICT_ANSI)
#pragma option -A                      /* force ANSI-C */
#endif

/* ************************************************************************** */

#ifdef MNG_INCLUDE_ZLIB

/* ************************************************************************** */

voidpf mngzlib_alloc (voidpf pData,
                      uInt   iCount,
                      uInt   iSize)
{
  voidpf pPtr;                         /* temporary space */

#ifdef MNG_INTERNAL_MEMMNGMT
  pPtr = calloc (iCount, iSize);       /* local allocation */
#else
  if (((mng_datap)pData)->fMemalloc)   /* callback function set ? */
    pPtr = ((mng_datap)pData)->fMemalloc (iCount * iSize);
  else
    pPtr = Z_NULL;                     /* can't allocate! */
#endif

  return pPtr;                         /* return the result */
}

/* ************************************************************************** */

void mngzlib_free (voidpf pData,
                   voidpf pAddress)
{
#ifdef MNG_INTERNAL_MEMMNGMT
  free (pAddress);                     /* free locally */
#else
  if (((mng_datap)pData)->fMemfree)    /* callback set? */
    ((mng_datap)pData)->fMemfree (pAddress, 1);
#endif
}

/* ************************************************************************** */

mng_retcode mngzlib_initialize (mng_datap pData)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_ZLIB_INITIALIZE, MNG_LC_START)
#endif

#ifdef MNG_INTERNAL_MEMMNGMT
  pData->sZlib.zalloc = Z_NULL;        /* let zlib figure out memory management */
  pData->sZlib.zfree  = Z_NULL;
  pData->sZlib.opaque = Z_NULL;
#else                                  /* use user-provided callbacks */
  pData->sZlib.zalloc = mngzlib_alloc;
  pData->sZlib.zfree  = mngzlib_free;
  pData->sZlib.opaque = (voidpf)pData;
#endif

  pData->bInflating   = MNG_FALSE;     /* not performing any action yet */
  pData->bDeflating   = MNG_FALSE;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_ZLIB_INITIALIZE, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* done */
}

/* ************************************************************************** */

mng_retcode mngzlib_cleanup (mng_datap pData)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_ZLIB_CLEANUP, MNG_LC_START)
#endif

  if (pData->bInflating)               /* force zlib cleanup */
    mngzlib_inflatefree (pData);
  if (pData->bDeflating)
    mngzlib_deflatefree (pData);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_ZLIB_CLEANUP, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* done */
}

/* ************************************************************************** */

mng_retcode mngzlib_inflateinit (mng_datap pData)
{
  int iZrslt;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_ZLIB_INFLATEINIT, MNG_LC_START)
#endif
                                       /* initialize zlib structures and such */
  iZrslt = inflateInit (&pData->sZlib);

  if (iZrslt != Z_OK)                  /* on error bail out */
    MNG_ERRORZ (pData, (mng_uint32)iZrslt)

  pData->bInflating      = MNG_TRUE;   /* really inflating something now */
  pData->sZlib.next_out  = 0;          /* force JIT initialization */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_ZLIB_INFLATEINIT, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* done */
}

/* ************************************************************************** */

#ifdef MNG_SUPPORT_DISPLAY
mng_retcode mngzlib_inflaterows (mng_datap  pData,
                                 mng_uint32 iInlen,
                                 mng_uint8p pIndata)
{
  int         iZrslt;
  mng_retcode iRslt;
  mng_ptr     pSwap;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_ZLIB_INFLATEROWS, MNG_LC_START)
#endif

  pData->sZlib.next_in   = pIndata;    /* let zlib know where to get stuff */
  pData->sZlib.avail_in  = (uInt)iInlen;

  if (pData->sZlib.next_out == 0)      /* initialize output variables ? */
  {                                    /* let zlib know where to store stuff */
    pData->sZlib.next_out  = pData->pWorkrow;
    pData->sZlib.avail_out = (uInt)(pData->iRowsize + pData->iPixelofs);
  }

  do
  {                                    /* now inflate a row */
    iZrslt = inflate (&pData->sZlib, Z_SYNC_FLUSH);
                                       /* produced a full row ? */
    if (((iZrslt == Z_OK) || (iZrslt == Z_STREAM_END)) &&
        (pData->sZlib.avail_out == 0))
    {                                  /* shouldn't we be at the end ? */
      if (pData->iRow >= (mng_int32)pData->iDataheight)
/*        MNG_ERROR (pData, MNG_TOOMUCHIDAT) */ ;  /* TODO: check this!!! */
      else
      {                                /* has leveling info ? */
/*        if (pData->iFilterofs)
          iRslt = init_rowdiffering (pData);
        else
          iRslt = MNG_NOERROR; */
                                       /* filter the row if necessary */
/*        if ((!iRslt) && (pData->iFilterofs < pData->iPixelofs  ) &&
                        (*(pData->pWorkrow + pData->iFilterofs))    ) */
        if (*(pData->pWorkrow + pData->iFilterofs))   
          iRslt = filter_a_row (pData);
        else
          iRslt = MNG_NOERROR;
                                       /* additonal leveling/differing ? */
        if ((!iRslt) && (pData->fDifferrow))
        {
          iRslt = ((mng_differrow)pData->fDifferrow) (pData);

          pSwap           = pData->pWorkrow;
          pData->pWorkrow = pData->pPrevrow;
          pData->pPrevrow = pSwap;     /* make sure we're processing the right data */
        }

        if (!iRslt)
        {
#ifdef MNG_INCLUDE_JNG
          if (pData->bHasJHDR)         /* is JNG alpha-channel ? */
          {                            /* just store in object ? */
            if ((!iRslt) && (pData->fStorerow))
              iRslt = ((mng_storerow)pData->fStorerow)     (pData);
          }
          else
#endif /* MNG_INCLUDE_JNG */
          {                            /* process this row */
            if ((!iRslt) && (pData->fProcessrow))
              iRslt = ((mng_processrow)pData->fProcessrow) (pData);
                                       /* store in object ? */
            if ((!iRslt) && (pData->fStorerow))
              iRslt = ((mng_storerow)pData->fStorerow)     (pData);
                                       /* color correction ? */
            if ((!iRslt) && (pData->fCorrectrow))
              iRslt = ((mng_correctrow)pData->fCorrectrow) (pData);
                                       /* slap onto canvas ? */
            if ((!iRslt) && (pData->fDisplayrow))
            {
              iRslt = ((mng_displayrow)pData->fDisplayrow) (pData);

              if (!iRslt)              /* check progressive display refresh */
                iRslt = display_progressive_check (pData);

            }
          }
        }

        if (iRslt)                     /* on error bail out */
          MNG_ERROR (pData, iRslt);

        if (!pData->fDifferrow)        /* swap row-pointers */
        {
          pSwap           = pData->pWorkrow;
          pData->pWorkrow = pData->pPrevrow;
          pData->pPrevrow = pSwap;     /* so prev points to the processed row! */
        }

        iRslt = next_row (pData);      /* adjust variables for next row */

        if (iRslt)                     /* on error bail out */
          MNG_ERROR (pData, iRslt);
      }
                                       /* let zlib know where to store next output */
      pData->sZlib.next_out  = pData->pWorkrow;
      pData->sZlib.avail_out = (uInt)(pData->iRowsize + pData->iPixelofs);
    }
  }                                    /* until some error or EOI */
  while ((iZrslt == Z_OK) && (pData->sZlib.avail_in > 0));
                                       /* on error bail out */
  if ((iZrslt != Z_OK) && (iZrslt != Z_STREAM_END))
    MNG_ERRORZ (pData, (mng_uint32)iZrslt)

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_ZLIB_INFLATEROWS, MNG_LC_END)
#endif

  return MNG_NOERROR;
}
#endif /* MNG_SUPPORT_DISPLAY */

/* ************************************************************************** */

mng_retcode mngzlib_inflatedata (mng_datap  pData,
                                 mng_uint32 iInlen,
                                 mng_uint8p pIndata)
{
  int iZrslt;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_ZLIB_INFLATEDATA, MNG_LC_START)
#endif
                                       /* let zlib know where to get stuff */
  pData->sZlib.next_in   = pIndata;
  pData->sZlib.avail_in  = (uInt)iInlen;
                                       /* now inflate the data in one go! */
  iZrslt = inflate (&pData->sZlib, Z_FINISH);
                                       /* not enough room in output-buffer ? */
  if ((iZrslt == Z_BUF_ERROR) || (pData->sZlib.avail_in > 0))
    return MNG_BUFOVERFLOW;
                                       /* on error bail out */
  if ((iZrslt != Z_OK) && (iZrslt != Z_STREAM_END))
    MNG_ERRORZ (pData, (mng_uint32)iZrslt)

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_ZLIB_INFLATEDATA, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode mngzlib_inflatefree (mng_datap pData)
{
  int iZrslt;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_ZLIB_INFLATEFREE, MNG_LC_START)
#endif

  pData->bInflating = MNG_FALSE;       /* stopped it */

  iZrslt = inflateEnd (&pData->sZlib); /* let zlib cleanup it's own stuff */

  if (iZrslt != Z_OK)                  /* on error bail out */
    MNG_ERRORZ (pData, (mng_uint32)iZrslt)

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_ZLIB_INFLATEFREE, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* done */
}

/* ************************************************************************** */

mng_retcode mngzlib_deflateinit (mng_datap pData)
{
  int iZrslt;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_ZLIB_DEFLATEINIT, MNG_LC_START)
#endif
                                       /* initialize zlib structures and such */
  iZrslt = deflateInit2 (&pData->sZlib, pData->iZlevel, pData->iZmethod,
                         pData->iZwindowbits, pData->iZmemlevel,
                         pData->iZstrategy);

  if (iZrslt != Z_OK)                  /* on error bail out */
    MNG_ERRORZ (pData, (mng_uint32)iZrslt)

  pData->bDeflating      = MNG_TRUE;   /* really deflating something now */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_ZLIB_DEFLATEINIT, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* done */
}

/* ************************************************************************** */

mng_retcode mngzlib_deflaterows (mng_datap  pData,
                                 mng_uint32 iInlen,
                                 mng_uint8p pIndata)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_ZLIB_DEFLATEROWS, MNG_LC_START)
#endif




#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_ZLIB_DEFLATEROWS, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode mngzlib_deflatedata (mng_datap  pData,
                                 mng_uint32 iInlen,
                                 mng_uint8p pIndata)
{
  int iZrslt;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_ZLIB_DEFLATEDATA, MNG_LC_START)
#endif

  pData->sZlib.next_in  = pIndata;     /* let zlib know where to get stuff */
  pData->sZlib.avail_in = (uInt)iInlen;
                                       /* now deflate the data in one go! */
  iZrslt = deflate (&pData->sZlib, Z_FINISH);
                                       /* not enough room in output-buffer ? */
  if ((iZrslt == Z_BUF_ERROR) || (pData->sZlib.avail_in > 0))
    return MNG_BUFOVERFLOW;
                                       /* on error bail out */
  if ((iZrslt != Z_OK) && (iZrslt != Z_STREAM_END))
    MNG_ERRORZ (pData, (mng_uint32)iZrslt)

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_ZLIB_DEFLATEDATA, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode mngzlib_deflatefree (mng_datap pData)
{
  int iZrslt;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_ZLIB_DEFLATEFREE, MNG_LC_START)
#endif

  iZrslt = deflateEnd (&pData->sZlib); /* let zlib cleanup it's own stuff */

  if (iZrslt != Z_OK)                  /* on error bail out */
    MNG_ERRORZ (pData, (mng_uint32)iZrslt)

  pData->bDeflating = MNG_FALSE;       /* stopped it */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_ZLIB_DEFLATEFREE, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* done */
}

/* ************************************************************************** */

#endif /* MNG_INCLUDE_ZLIB */

/* ************************************************************************** */
/* * end of file                                                            * */
/* ************************************************************************** */

