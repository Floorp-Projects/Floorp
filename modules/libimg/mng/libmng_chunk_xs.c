/* ************************************************************************** */
/* *             For conditions of distribution and use,                    * */
/* *                see copyright notice in libmng.h                        * */
/* ************************************************************************** */
/* *                                                                        * */
/* * project   : libmng                                                     * */
/* * file      : libmng_chunk_xs.c         copyright (c) 2000 G. Juyn       * */
/* * version   : 1.0.0                                                      * */
/* *                                                                        * */
/* * purpose   : chunk access functions (implementation)                    * */
/* *                                                                        * */
/* * author    : G.Juyn                                                     * */
/* * web       : http://www.3-t.com                                         * */
/* * email     : mailto:info@3-t.com                                        * */
/* *                                                                        * */
/* * comment   : implementation of the chunk access functions               * */
/* *                                                                        * */
/* * changes   : 0.5.1 - 05/06/2000 - G.Juyn                                * */
/* *             - changed and filled iterate-chunk function                * */
/* *             0.5.1 - 05/08/2000 - G.Juyn                                * */
/* *             - fixed calling convention                                 * */
/* *             - added getchunk functions                                 * */
/* *             - added putchunk functions                                 * */
/* *             - changed strict-ANSI stuff                                * */
/* *             0.5.1 - 05/11/2000 - G.Juyn                                * */
/* *             - added empty-chunk put-routines                           * */
/* *             0.5.1 - 05/12/2000 - G.Juyn                                * */
/* *             - changed trace to macro for callback error-reporting      * */
/* *             0.5.1 - 05/15/2000 - G.Juyn                                * */
/* *             - added getimgdata & putimgdata functions                  * */
/* *                                                                        * */
/* *             0.5.2 - 05/19/2000 - G.Juyn                                * */
/* *             - B004 - fixed problem with MNG_SUPPORT_WRITE not defined  * */
/* *               also for MNG_SUPPORT_WRITE without MNG_INCLUDE_JNG       * */
/* *             - Cleaned up some code regarding mixed support             * */
/* *                                                                        * */
/* *             0.9.1 - 07/19/2000 - G.Juyn                                * */
/* *             - fixed creation-code                                      * */
/* *                                                                        * */
/* *             0.9.2 - 08/05/2000 - G.Juyn                                * */
/* *             - changed file-prefixes                                    * */
/* *             - added function to set simplicity field                   * */
/* *             - fixed putchunk_unknown() function                        * */
/* *                                                                        * */
/* *             0.9.3 - 08/07/2000 - G.Juyn                                * */
/* *             - B111300 - fixup for improved portability                 * */
/* *             0.9.3 - 08/26/2000 - G.Juyn                                * */
/* *             - added MAGN chunk                                         * */
/* *             0.9.3 - 10/20/2000 - G.Juyn                                * */
/* *             - fixed putchunk_plte() to set bEmpty parameter            * */
/* *                                                                        * */
/* *             0.9.5 -  1/25/2001 - G.Juyn                                * */
/* *             - fixed some small compiler warnings (thanks Nikki)        * */
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
#include "libmng_chunk_io.h"

#if defined(__BORLANDC__) && defined(MNG_STRICT_ANSI)
#pragma option -A                      /* force ANSI-C */
#endif

/* ************************************************************************** */

#ifdef MNG_ACCESS_CHUNKS

/* ************************************************************************** */

mng_retcode MNG_DECL mng_iterate_chunks (mng_handle       hHandle,
                                         mng_uint32       iChunkseq,
                                         mng_iteratechunk fProc)
{
  mng_uint32  iSeq;
  mng_chunkid iChunkname;
  mng_datap   pData;
  mng_chunkp  pChunk;
  mng_bool    bCont;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_ITERATE_CHUNKS, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = ((mng_datap)hHandle);        /* and make it addressable */

  iSeq   = 0;
  bCont  = MNG_TRUE;
  pChunk = pData->pFirstchunk;         /* get the first chunk */
                                       /* as long as there are some more */
  while ((pChunk) && (bCont))          /* and the app didn't signal a stop */
  {
    if (iSeq >= iChunkseq)             /* reached the first target ? */
    {                                  /* then call this and next ones back in... */
      iChunkname = ((mng_chunk_headerp)pChunk)->iChunkname;
      bCont      = fProc (hHandle, (mng_handle)pChunk, iChunkname, iSeq);
    }

    iSeq++;                            /* next one */
    pChunk = ((mng_chunk_headerp)pChunk)->pNext;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_ITERATE_CHUNKS, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_getchunk_ihdr (mng_handle hHandle,
                                        mng_handle hChunk,
                                        mng_uint32 *iWidth,
                                        mng_uint32 *iHeight,
                                        mng_uint8  *iBitdepth,
                                        mng_uint8  *iColortype,
                                        mng_uint8  *iCompression,
                                        mng_uint8  *iFilter,
                                        mng_uint8  *iInterlace)
{
  mng_datap pData;
  mng_ihdrp pChunk;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_IHDR, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData  = (mng_datap)hHandle;         /* and make it addressable */
  pChunk = (mng_ihdrp)hChunk;          /* address the chunk */

  if (pChunk->sHeader.iChunkname != MNG_UINT_IHDR)
    MNG_ERROR (pData, MNG_WRONGCHUNK)  /* ouch */

  *iWidth       = pChunk->iWidth;      /* fill the fields */
  *iHeight      = pChunk->iHeight;
  *iBitdepth    = pChunk->iBitdepth;
  *iColortype   = pChunk->iColortype;
  *iCompression = pChunk->iCompression;
  *iFilter      = pChunk->iFilter;
  *iInterlace   = pChunk->iInterlace;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_IHDR, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_getchunk_plte (mng_handle   hHandle,
                                        mng_handle   hChunk,
                                        mng_uint32   *iCount,
                                        mng_palette8 *aPalette)
{
  mng_datap pData;
  mng_pltep pChunk;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_PLTE, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData  = (mng_datap)hHandle;         /* and make it addressable */
  pChunk = (mng_pltep)hChunk;          /* address the chunk */

  if (pChunk->sHeader.iChunkname != MNG_UINT_PLTE)
    MNG_ERROR (pData, MNG_WRONGCHUNK)  /* ouch */

  *iCount = pChunk->iEntrycount;       /* fill the fields */

  MNG_COPY (*aPalette, pChunk->aEntries, sizeof (mng_palette8))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_PLTE, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_getchunk_idat (mng_handle hHandle,
                                        mng_handle hChunk,
                                        mng_uint32 *iRawlen,
                                        mng_ptr    *pRawdata)
{
  mng_datap pData;
  mng_idatp pChunk;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_IDAT, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData  = (mng_datap)hHandle;         /* and make it addressable */
  pChunk = (mng_idatp)hChunk;          /* address the chunk */

  if (pChunk->sHeader.iChunkname != MNG_UINT_IDAT)
    MNG_ERROR (pData, MNG_WRONGCHUNK)  /* ouch */

  *iRawlen  = pChunk->iDatasize;       /* fill the fields */
  *pRawdata = pChunk->pData;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_IDAT, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_getchunk_trns (mng_handle   hHandle,
                                        mng_handle   hChunk,
                                        mng_bool     *bEmpty,
                                        mng_bool     *bGlobal,
                                        mng_uint8    *iType,
                                        mng_uint32   *iCount,
                                        mng_uint8arr *aAlphas,
                                        mng_uint16   *iGray,
                                        mng_uint16   *iRed,
                                        mng_uint16   *iGreen,
                                        mng_uint16   *iBlue,
                                        mng_uint32   *iRawlen,
                                        mng_uint8arr *aRawdata)
{
  mng_datap pData;
  mng_trnsp pChunk;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_TRNS, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData  = (mng_datap)hHandle;         /* and make it addressable */
  pChunk = (mng_trnsp)hChunk;          /* address the chunk */

  if (pChunk->sHeader.iChunkname != MNG_UINT_tRNS)
    MNG_ERROR (pData, MNG_WRONGCHUNK)  /* ouch */

  *bEmpty   = pChunk->bEmpty;          /* fill the fields */
  *bGlobal  = pChunk->bGlobal;
  *iType    = pChunk->iType;
  *iCount   = pChunk->iCount;
  *iGray    = pChunk->iGray;
  *iRed     = pChunk->iRed;
  *iGreen   = pChunk->iGreen;
  *iBlue    = pChunk->iBlue;
  *iRawlen  = pChunk->iRawlen;

  MNG_COPY (*aAlphas,  pChunk->aEntries, sizeof (mng_uint8arr))
  MNG_COPY (*aRawdata, pChunk->aRawdata, sizeof (mng_uint8arr))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_TRNS, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_getchunk_gama (mng_handle hHandle,
                                        mng_handle hChunk,
                                        mng_bool   *bEmpty,
                                        mng_uint32 *iGamma)
{
  mng_datap pData;
  mng_gamap pChunk;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_GAMA, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData  = (mng_datap)hHandle;         /* and make it addressable */
  pChunk = (mng_gamap)hChunk;          /* address the chunk */

  if (pChunk->sHeader.iChunkname != MNG_UINT_gAMA)
    MNG_ERROR (pData, MNG_WRONGCHUNK)  /* ouch */

  *bEmpty = pChunk->bEmpty;            /* fill the fields */
  *iGamma = pChunk->iGamma;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_GAMA, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_getchunk_chrm (mng_handle hHandle,
                                        mng_handle hChunk,
                                        mng_bool   *bEmpty,
                                        mng_uint32 *iWhitepointx,
                                        mng_uint32 *iWhitepointy,
                                        mng_uint32 *iRedx,
                                        mng_uint32 *iRedy,
                                        mng_uint32 *iGreenx,
                                        mng_uint32 *iGreeny,
                                        mng_uint32 *iBluex,
                                        mng_uint32 *iBluey)
{
  mng_datap pData;
  mng_chrmp pChunk;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_CHRM, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData  = (mng_datap)hHandle;         /* and make it addressable */
  pChunk = (mng_chrmp)hChunk;          /* address the chunk */

  if (pChunk->sHeader.iChunkname != MNG_UINT_cHRM)
    MNG_ERROR (pData, MNG_WRONGCHUNK)  /* ouch */

  *bEmpty       = pChunk->bEmpty;      /* fill the fields */     
  *iWhitepointx = pChunk->iWhitepointx;
  *iWhitepointy = pChunk->iWhitepointy;
  *iRedx        = pChunk->iRedx;
  *iRedy        = pChunk->iRedy;
  *iGreenx      = pChunk->iGreenx;
  *iGreeny      = pChunk->iGreeny;
  *iBluex       = pChunk->iBluex;
  *iBluey       = pChunk->iBluey;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_CHRM, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_getchunk_srgb (mng_handle hHandle,
                                        mng_handle hChunk,
                                        mng_bool   *bEmpty,
                                        mng_uint8  *iRenderingintent)
{
  mng_datap pData;
  mng_srgbp pChunk;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_SRGB, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData  = (mng_datap)hHandle;         /* and make it addressable */
  pChunk = (mng_srgbp)hChunk;          /* address the chunk */

  if (pChunk->sHeader.iChunkname != MNG_UINT_sRGB)
    MNG_ERROR (pData, MNG_WRONGCHUNK)  /* ouch */

  *bEmpty           = pChunk->bEmpty;  /* fill the fields */        
  *iRenderingintent = pChunk->iRenderingintent;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_SRGB, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_getchunk_iccp (mng_handle hHandle,
                                        mng_handle hChunk,
                                        mng_bool   *bEmpty,
                                        mng_uint32 *iNamesize,
                                        mng_pchar  *zName,
                                        mng_uint8  *iCompression,
                                        mng_uint32 *iProfilesize,
                                        mng_ptr    *pProfile)
{
  mng_datap pData;
  mng_iccpp pChunk;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_ICCP, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData  = (mng_datap)hHandle;         /* and make it addressable */
  pChunk = (mng_iccpp)hChunk;          /* address the chunk */

  if (pChunk->sHeader.iChunkname != MNG_UINT_iCCP)
    MNG_ERROR (pData, MNG_WRONGCHUNK)  /* ouch */

  *bEmpty       = pChunk->bEmpty;      /* fill the fields */     
  *iNamesize    = pChunk->iNamesize;
  *zName        = pChunk->zName;
  *iCompression = pChunk->iCompression;
  *iProfilesize = pChunk->iProfilesize;
  *pProfile     = pChunk->pProfile;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_ICCP, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_getchunk_text (mng_handle hHandle,
                                        mng_handle hChunk,
                                        mng_uint32 *iKeywordsize,
                                        mng_pchar  *zKeyword,
                                        mng_uint32 *iTextsize,
                                        mng_pchar  *zText)
{
  mng_datap pData;
  mng_textp pChunk;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_TEXT, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData  = (mng_datap)hHandle;         /* and make it addressable */
  pChunk = (mng_textp)hChunk;          /* address the chunk */

  if (pChunk->sHeader.iChunkname != MNG_UINT_tEXt)
    MNG_ERROR (pData, MNG_WRONGCHUNK)  /* ouch */
                                       /* fill the fields */
  *iKeywordsize = pChunk->iKeywordsize;
  *zKeyword     = pChunk->zKeyword;
  *iTextsize    = pChunk->iTextsize;
  *zText        = pChunk->zText;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_TEXT, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_getchunk_ztxt (mng_handle hHandle,
                                        mng_handle hChunk,
                                        mng_uint32 *iKeywordsize,
                                        mng_pchar  *zKeyword,
                                        mng_uint8  *iCompression,
                                        mng_uint32 *iTextsize,
                                        mng_pchar  *zText)
{
  mng_datap pData;
  mng_ztxtp pChunk;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_ZTXT, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData  = (mng_datap)hHandle;         /* and make it addressable */
  pChunk = (mng_ztxtp)hChunk;          /* address the chunk */

  if (pChunk->sHeader.iChunkname != MNG_UINT_zTXt)
    MNG_ERROR (pData, MNG_WRONGCHUNK)  /* ouch */
                                       /* fill the fields */
  *iKeywordsize = pChunk->iKeywordsize;
  *zKeyword     = pChunk->zKeyword;
  *iCompression = pChunk->iCompression;
  *iTextsize    = pChunk->iTextsize;
  *zText        = pChunk->zText;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_ZTXT, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_getchunk_itxt (mng_handle hHandle,
                                        mng_handle hChunk,
                                        mng_uint32 *iKeywordsize,
                                        mng_pchar  *zKeyword,
                                        mng_uint8  *iCompressionflag,
                                        mng_uint8  *iCompressionmethod,
                                        mng_uint32 *iLanguagesize,
                                        mng_pchar  *zLanguage,
                                        mng_uint32 *iTranslationsize,
                                        mng_pchar  *zTranslation,
                                        mng_uint32 *iTextsize,
                                        mng_pchar  *zText)
{
  mng_datap pData;
  mng_itxtp pChunk;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_ITXT, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData  = (mng_datap)hHandle;         /* and make it addressable */
  pChunk = (mng_itxtp)hChunk;          /* address the chunk */

  if (pChunk->sHeader.iChunkname != MNG_UINT_iTXt)
    MNG_ERROR (pData, MNG_WRONGCHUNK)  /* ouch */
                                       /* fill the fields */
  *iKeywordsize       = pChunk->iKeywordsize;
  *zKeyword           = pChunk->zKeyword;
  *iCompressionflag   = pChunk->iCompressionflag;
  *iCompressionmethod = pChunk->iCompressionmethod;
  *iLanguagesize      = pChunk->iLanguagesize;
  *zLanguage          = pChunk->zLanguage;
  *iTranslationsize   = pChunk->iTranslationsize;
  *zTranslation       = pChunk->zTranslation;
  *iTextsize          = pChunk->iTextsize;
  *zText              = pChunk->zText;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_ITXT, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_getchunk_bkgd (mng_handle hHandle,
                                        mng_handle hChunk,
                                        mng_bool   *bEmpty,
                                        mng_uint8  *iType,
                                        mng_uint8  *iIndex,
                                        mng_uint16 *iGray,
                                        mng_uint16 *iRed,
                                        mng_uint16 *iGreen,
                                        mng_uint16 *iBlue)
{
  mng_datap pData;
  mng_bkgdp pChunk;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_BKGD, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData  = (mng_datap)hHandle;         /* and make it addressable */
  pChunk = (mng_bkgdp)hChunk;          /* address the chunk */

  if (pChunk->sHeader.iChunkname != MNG_UINT_bKGD)
    MNG_ERROR (pData, MNG_WRONGCHUNK)  /* ouch */

  *bEmpty = pChunk->bEmpty;            /* fill the fields */
  *iType  = pChunk->iType;
  *iIndex = pChunk->iIndex;
  *iGray  = pChunk->iGray;
  *iRed   = pChunk->iRed;
  *iGreen = pChunk->iGreen;
  *iBlue  = pChunk->iBlue;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_BKGD, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_getchunk_phys (mng_handle hHandle,
                                        mng_handle hChunk,
                                        mng_bool   *bEmpty,
                                        mng_uint32 *iSizex,
                                        mng_uint32 *iSizey,
                                        mng_uint8  *iUnit)
{
  mng_datap pData;
  mng_physp pChunk;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_PHYS, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData  = (mng_datap)hHandle;         /* and make it addressable */
  pChunk = (mng_physp)hChunk;          /* address the chunk */

  if (pChunk->sHeader.iChunkname != MNG_UINT_pHYs)
    MNG_ERROR (pData, MNG_WRONGCHUNK)  /* ouch */

  *bEmpty = pChunk->bEmpty;            /* fill the fields */
  *iSizex = pChunk->iSizex;
  *iSizey = pChunk->iSizey;
  *iUnit  = pChunk->iUnit;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_PHYS, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_getchunk_sbit (mng_handle    hHandle,
                                        mng_handle    hChunk,
                                        mng_bool      *bEmpty,
                                        mng_uint8     *iType,
                                        mng_uint8arr4 *aBits)
{
  mng_datap pData;
  mng_sbitp pChunk;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_SBIT, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData  = (mng_datap)hHandle;         /* and make it addressable */
  pChunk = (mng_sbitp)hChunk;          /* address the chunk */

  if (pChunk->sHeader.iChunkname != MNG_UINT_sBIT)
    MNG_ERROR (pData, MNG_WRONGCHUNK)  /* ouch */

  *bEmpty     = pChunk->bEmpty;
  *iType      = pChunk->iType;
  (*aBits)[0] = pChunk->aBits[0];
  (*aBits)[1] = pChunk->aBits[1];
  (*aBits)[2] = pChunk->aBits[2];
  (*aBits)[3] = pChunk->aBits[3];

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_SBIT, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_getchunk_splt (mng_handle hHandle,
                                        mng_handle hChunk,
                                        mng_bool   *bEmpty,
                                        mng_uint32 *iNamesize,
                                        mng_pchar  *zName,
                                        mng_uint8  *iSampledepth,
                                        mng_uint32 *iEntrycount,
                                        mng_ptr    *pEntries)
{
  mng_datap pData;
  mng_spltp pChunk;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_SPLT, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData  = (mng_datap)hHandle;         /* and make it addressable */
  pChunk = (mng_spltp)hChunk;          /* address the chunk */

  if (pChunk->sHeader.iChunkname != MNG_UINT_sPLT)
    MNG_ERROR (pData, MNG_WRONGCHUNK)  /* ouch */

  *bEmpty       = pChunk->bEmpty;      /* fill the fields */      
  *iNamesize    = pChunk->iNamesize;
  *zName        = pChunk->zName;
  *iSampledepth = pChunk->iSampledepth;
  *iEntrycount  = pChunk->iEntrycount;
  *pEntries     = pChunk->pEntries;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_SPLT, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_getchunk_hist (mng_handle    hHandle,
                                        mng_handle    hChunk,
                                        mng_uint32    *iEntrycount,
                                        mng_uint16arr *aEntries)
{
  mng_datap pData;
  mng_histp pChunk;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_HIST, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData  = (mng_datap)hHandle;         /* and make it addressable */
  pChunk = (mng_histp)hChunk;          /* address the chunk */

  if (pChunk->sHeader.iChunkname != MNG_UINT_hIST)
    MNG_ERROR (pData, MNG_WRONGCHUNK)  /* ouch */

  *iEntrycount = pChunk->iEntrycount;  /* fill the fields */

  MNG_COPY (*aEntries, pChunk->aEntries, sizeof (mng_uint16arr))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_HIST, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_getchunk_time (mng_handle hHandle,
                                        mng_handle hChunk,
                                        mng_uint16 *iYear,
                                        mng_uint8  *iMonth,
                                        mng_uint8  *iDay,
                                        mng_uint8  *iHour,
                                        mng_uint8  *iMinute,
                                        mng_uint8  *iSecond)
{
  mng_datap pData;
  mng_timep pChunk;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_TIME, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData  = (mng_datap)hHandle;         /* and make it addressable */
  pChunk = (mng_timep)hChunk;          /* address the chunk */

  if (pChunk->sHeader.iChunkname != MNG_UINT_tIME)
    MNG_ERROR (pData, MNG_WRONGCHUNK)  /* ouch */

  *iYear   = pChunk->iYear;            /* fill the fields */ 
  *iMonth  = pChunk->iMonth;
  *iDay    = pChunk->iDay;
  *iHour   = pChunk->iHour;
  *iMinute = pChunk->iMinute;
  *iSecond = pChunk->iSecond;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_TIME, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_getchunk_mhdr (mng_handle hHandle,
                                        mng_handle hChunk,
                                        mng_uint32 *iWidth,
                                        mng_uint32 *iHeight,
                                        mng_uint32 *iTicks,
                                        mng_uint32 *iLayercount,
                                        mng_uint32 *iFramecount,
                                        mng_uint32 *iPlaytime,
                                        mng_uint32 *iSimplicity)
{
  mng_datap pData;
  mng_mhdrp pChunk;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_MHDR, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData  = (mng_datap)hHandle;         /* and make it addressable */
  pChunk = (mng_mhdrp)hChunk;          /* address the chunk */

  if (pChunk->sHeader.iChunkname != MNG_UINT_MHDR)
    MNG_ERROR (pData, MNG_WRONGCHUNK)  /* ouch */

  *iWidth      = pChunk->iWidth;       /* fill the fields */   
  *iHeight     = pChunk->iHeight;
  *iTicks      = pChunk->iTicks;
  *iLayercount = pChunk->iLayercount;
  *iFramecount = pChunk->iFramecount;
  *iPlaytime   = pChunk->iPlaytime;
  *iSimplicity = pChunk->iSimplicity;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_MHDR, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_getchunk_loop (mng_handle  hHandle,
                                        mng_handle  hChunk,
                                        mng_uint8   *iLevel,
                                        mng_uint32  *iRepeat,
                                        mng_uint8   *iTermination,
                                        mng_uint32  *iItermin,
                                        mng_uint32  *iItermax,
                                        mng_uint32  *iCount,
                                        mng_uint32p *pSignals)
{
  mng_datap pData;
  mng_loopp pChunk;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_LOOP, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData  = (mng_datap)hHandle;         /* and make it addressable */
  pChunk = (mng_loopp)hChunk;          /* address the chunk */

  if (pChunk->sHeader.iChunkname != MNG_UINT_LOOP)
    MNG_ERROR (pData, MNG_WRONGCHUNK)  /* ouch */

  *iLevel       = pChunk->iLevel;      /* fill teh fields */
  *iRepeat      = pChunk->iRepeat;
  *iTermination = pChunk->iTermination;
  *iItermin     = pChunk->iItermin;
  *iItermax     = pChunk->iItermax;
  *iCount       = pChunk->iCount;
  *pSignals     = pChunk->pSignals;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_LOOP, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_getchunk_endl (mng_handle hHandle,
                                        mng_handle hChunk,
                                        mng_uint8  *iLevel)
{
  mng_datap pData;
  mng_endlp pChunk;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_ENDL, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData  = (mng_datap)hHandle;         /* and make it addressable */
  pChunk = (mng_endlp)hChunk;          /* address the chunk */

  if (pChunk->sHeader.iChunkname != MNG_UINT_ENDL)
    MNG_ERROR (pData, MNG_WRONGCHUNK)  /* ouch */

  *iLevel = pChunk->iLevel;            /* fill the field */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_ENDL, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_getchunk_defi (mng_handle hHandle,
                                        mng_handle hChunk,
                                        mng_uint16 *iObjectid,
                                        mng_uint8  *iDonotshow,
                                        mng_uint8  *iConcrete,
                                        mng_bool   *bHasloca,
                                        mng_int32  *iXlocation,
                                        mng_int32  *iYlocation,
                                        mng_bool   *bHasclip,
                                        mng_int32  *iLeftcb,
                                        mng_int32  *iRightcb,
                                        mng_int32  *iTopcb,
                                        mng_int32  *iBottomcb)
{
  mng_datap pData;
  mng_defip pChunk;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_DEFI, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData  = (mng_datap)hHandle;         /* and make it addressable */
  pChunk = (mng_defip)hChunk;          /* address the chunk */

  if (pChunk->sHeader.iChunkname != MNG_UINT_DEFI)
    MNG_ERROR (pData, MNG_WRONGCHUNK)  /* ouch */

  *iObjectid  = pChunk->iObjectid;     /* fill the fields */
  *iDonotshow = pChunk->iDonotshow;
  *iConcrete  = pChunk->iConcrete;
  *bHasloca   = pChunk->bHasloca;
  *iXlocation = pChunk->iXlocation;
  *iYlocation = pChunk->iYlocation;
  *bHasclip   = pChunk->bHasclip;
  *iLeftcb    = pChunk->iLeftcb;
  *iRightcb   = pChunk->iRightcb;
  *iTopcb     = pChunk->iTopcb;
  *iBottomcb  = pChunk->iBottomcb;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_DEFI, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_getchunk_basi (mng_handle hHandle,
                                        mng_handle hChunk,
                                        mng_uint32 *iWidth,
                                        mng_uint32 *iHeight,
                                        mng_uint8  *iBitdepth,
                                        mng_uint8  *iColortype,
                                        mng_uint8  *iCompression,
                                        mng_uint8  *iFilter,
                                        mng_uint8  *iInterlace,
                                        mng_uint16 *iRed,
                                        mng_uint16 *iGreen,
                                        mng_uint16 *iBlue,
                                        mng_uint16 *iAlpha,
                                        mng_uint8  *iViewable)
{
  mng_datap pData;
  mng_basip pChunk;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_BASI, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData  = (mng_datap)hHandle;         /* and make it addressable */
  pChunk = (mng_basip)hChunk;          /* address the chunk */

  if (pChunk->sHeader.iChunkname != MNG_UINT_BASI)
    MNG_ERROR (pData, MNG_WRONGCHUNK)  /* ouch */

  *iWidth       = pChunk->iWidth;      /* fill the fields */
  *iHeight      = pChunk->iHeight;
  *iBitdepth    = pChunk->iBitdepth;
  *iColortype   = pChunk->iColortype;
  *iCompression = pChunk->iCompression;
  *iFilter      = pChunk->iFilter;
  *iInterlace   = pChunk->iInterlace;
  *iRed         = pChunk->iRed;
  *iGreen       = pChunk->iGreen;
  *iBlue        = pChunk->iBlue;
  *iAlpha       = pChunk->iAlpha;
  *iViewable    = pChunk->iViewable;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_BASI, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_getchunk_clon (mng_handle hHandle,
                                        mng_handle hChunk,
                                        mng_uint16 *iSourceid,
                                        mng_uint16 *iCloneid,
                                        mng_uint8  *iClonetype,
                                        mng_uint8  *iDonotshow,
                                        mng_uint8  *iConcrete,
                                        mng_bool   *bHasloca,
                                        mng_uint8  *iLocationtype,
                                        mng_int32  *iLocationx,
                                        mng_int32  *iLocationy)
{
  mng_datap pData;
  mng_clonp pChunk;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_CLON, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData  = (mng_datap)hHandle;         /* and make it addressable */
  pChunk = (mng_clonp)hChunk;          /* address the chunk */

  if (pChunk->sHeader.iChunkname != MNG_UINT_CLON)
    MNG_ERROR (pData, MNG_WRONGCHUNK)  /* ouch */

  *iSourceid     = pChunk->iSourceid;  /* fill the fields */  
  *iCloneid      = pChunk->iCloneid;
  *iClonetype    = pChunk->iClonetype;
  *iDonotshow    = pChunk->iDonotshow;
  *iConcrete     = pChunk->iConcrete;
  *bHasloca      = pChunk->bHasloca;
  *iLocationtype = pChunk->iLocationtype;
  *iLocationx    = pChunk->iLocationx;
  *iLocationy    = pChunk->iLocationy;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_CLON, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_getchunk_past (mng_handle hHandle,
                                        mng_handle hChunk,
                                        mng_uint16 *iDestid,
                                        mng_uint8  *iTargettype,
                                        mng_int32  *iTargetx,
                                        mng_int32  *iTargety,
                                        mng_uint32 *iCount)
{
  mng_datap pData;
  mng_pastp pChunk;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_PAST, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData  = (mng_datap)hHandle;         /* and make it addressable */
  pChunk = (mng_pastp)hChunk;          /* address the chunk */

  if (pChunk->sHeader.iChunkname != MNG_UINT_PAST)
    MNG_ERROR (pData, MNG_WRONGCHUNK)  /* ouch */

  *iDestid     = pChunk->iDestid;       /* fill the fields */
  *iTargettype = pChunk->iTargettype;
  *iTargetx    = pChunk->iTargetx;
  *iTargety    = pChunk->iTargety;
  *iCount      = pChunk->iCount;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_PAST, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_getchunk_past_src (mng_handle hHandle,
                                            mng_handle hChunk,
                                            mng_uint32 iEntry,
                                            mng_uint16 *iSourceid,
                                            mng_uint8  *iComposition,
                                            mng_uint8  *iOrientation,
                                            mng_uint8  *iOffsettype,
                                            mng_int32  *iOffsetx,
                                            mng_int32  *iOffsety,
                                            mng_uint8  *iBoundarytype,
                                            mng_int32  *iBoundaryl,
                                            mng_int32  *iBoundaryr,
                                            mng_int32  *iBoundaryt,
                                            mng_int32  *iBoundaryb)
{
  mng_datap        pData;
  mng_pastp        pChunk;
  mng_past_sourcep pEntry;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_PAST_SRC, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData  = (mng_datap)hHandle;         /* and make it addressable */
  pChunk = (mng_pastp)hChunk;          /* address the chunk */

  if (pChunk->sHeader.iChunkname != MNG_UINT_PAST)
    MNG_ERROR (pData, MNG_WRONGCHUNK)  /* ouch */

  if (iEntry >= pChunk->iCount)        /* valid index ? */
    MNG_ERROR (pData, MNG_INVALIDENTRYIX)
                                       /* address the entry */
  pEntry         = pChunk->pSources + iEntry;

  *iSourceid     = pEntry->iSourceid;  /* fill the fields */
  *iComposition  = pEntry->iComposition;
  *iOrientation  = pEntry->iOrientation;
  *iOffsettype   = pEntry->iOffsettype;
  *iOffsetx      = pEntry->iOffsetx;
  *iOffsety      = pEntry->iOffsety;
  *iBoundarytype = pEntry->iBoundarytype;
  *iBoundaryl    = pEntry->iBoundaryl;
  *iBoundaryr    = pEntry->iBoundaryr;
  *iBoundaryt    = pEntry->iBoundaryt;
  *iBoundaryb    = pEntry->iBoundaryb;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_PAST_SRC, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_getchunk_disc (mng_handle  hHandle,
                                        mng_handle  hChunk,
                                        mng_uint32  *iCount,
                                        mng_uint16p *pObjectids)
{
  mng_datap pData;
  mng_discp pChunk;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_DISC, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData  = (mng_datap)hHandle;         /* and make it addressable */
  pChunk = (mng_discp)hChunk;          /* address the chunk */

  if (pChunk->sHeader.iChunkname != MNG_UINT_DISC)
    MNG_ERROR (pData, MNG_WRONGCHUNK)  /* ouch */

  *iCount     = pChunk->iCount;        /* fill the fields */
  *pObjectids = pChunk->pObjectids;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_DISC, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_getchunk_back (mng_handle hHandle,
                                        mng_handle hChunk,
                                        mng_uint16 *iRed,
                                        mng_uint16 *iGreen,
                                        mng_uint16 *iBlue,
                                        mng_uint8  *iMandatory,
                                        mng_uint16 *iImageid,
                                        mng_uint8  *iTile)
{
  mng_datap pData;
  mng_backp pChunk;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_BACK, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData  = (mng_datap)hHandle;         /* and make it addressable */
  pChunk = (mng_backp)hChunk;          /* address the chunk */

  if (pChunk->sHeader.iChunkname != MNG_UINT_BACK)
    MNG_ERROR (pData, MNG_WRONGCHUNK)  /* ouch */

  *iRed       = pChunk->iRed;          /* fill the fields */
  *iGreen     = pChunk->iGreen;
  *iBlue      = pChunk->iBlue;
  *iMandatory = pChunk->iMandatory;
  *iImageid   = pChunk->iImageid;
  *iTile      = pChunk->iTile;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_BACK, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_getchunk_fram (mng_handle  hHandle,
                                        mng_handle  hChunk,
                                        mng_bool    *bEmpty,
                                        mng_uint8   *iMode,
                                        mng_uint32  *iNamesize,
                                        mng_pchar   *zName,
                                        mng_uint8   *iChangedelay,
                                        mng_uint8   *iChangetimeout,
                                        mng_uint8   *iChangeclipping,
                                        mng_uint8   *iChangesyncid,
                                        mng_uint32  *iDelay,
                                        mng_uint32  *iTimeout,
                                        mng_uint8   *iBoundarytype,
                                        mng_int32   *iBoundaryl,
                                        mng_int32   *iBoundaryr,
                                        mng_int32   *iBoundaryt,
                                        mng_int32   *iBoundaryb,
                                        mng_uint32  *iCount,
                                        mng_uint32p *pSyncids)
{
  mng_datap pData;
  mng_framp pChunk;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_FRAM, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData  = (mng_datap)hHandle;         /* and make it addressable */
  pChunk = (mng_framp)hChunk;          /* address the chunk */

  if (pChunk->sHeader.iChunkname != MNG_UINT_FRAM)
    MNG_ERROR (pData, MNG_WRONGCHUNK)  /* ouch */

  *bEmpty          = pChunk->bEmpty;   /* fill the fields */      
  *iMode           = pChunk->iMode;
  *iNamesize       = pChunk->iNamesize;
  *zName           = pChunk->zName;
  *iChangedelay    = pChunk->iChangedelay;
  *iChangetimeout  = pChunk->iChangetimeout;
  *iChangeclipping = pChunk->iChangeclipping;
  *iChangesyncid   = pChunk->iChangesyncid;
  *iDelay          = pChunk->iDelay;
  *iTimeout        = pChunk->iTimeout;
  *iBoundarytype   = pChunk->iBoundarytype;
  *iBoundaryl      = pChunk->iBoundaryl;
  *iBoundaryr      = pChunk->iBoundaryr;
  *iBoundaryt      = pChunk->iBoundaryt;
  *iBoundaryb      = pChunk->iBoundaryb;
  *iCount          = pChunk->iCount;
  *pSyncids        = pChunk->pSyncids;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_FRAM, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_getchunk_move (mng_handle hHandle,
                                        mng_handle hChunk,
                                        mng_uint16 *iFirstid,
                                        mng_uint16 *iLastid,
                                        mng_uint8  *iMovetype,
                                        mng_int32  *iMovex,
                                        mng_int32  *iMovey)
{
  mng_datap pData;
  mng_movep pChunk;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_MOVE, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData  = (mng_datap)hHandle;         /* and make it addressable */
  pChunk = (mng_movep)hChunk;          /* address the chunk */

  if (pChunk->sHeader.iChunkname != MNG_UINT_MOVE)
    MNG_ERROR (pData, MNG_WRONGCHUNK)  /* ouch */

  *iFirstid  = pChunk->iFirstid;       /* fill the fields */
  *iLastid   = pChunk->iLastid;
  *iMovetype = pChunk->iMovetype;
  *iMovex    = pChunk->iMovex;
  *iMovey    = pChunk->iMovey;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_MOVE, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_getchunk_clip (mng_handle hHandle,
                                        mng_handle hChunk,
                                        mng_uint16 *iFirstid,
                                        mng_uint16 *iLastid,
                                        mng_uint8  *iCliptype,
                                        mng_int32  *iClipl,
                                        mng_int32  *iClipr,
                                        mng_int32  *iClipt,
                                        mng_int32  *iClipb)
{
  mng_datap pData;
  mng_clipp pChunk;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_CLIP, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData  = (mng_datap)hHandle;         /* and make it addressable */
  pChunk = (mng_clipp)hChunk;          /* address the chunk */

  if (pChunk->sHeader.iChunkname != MNG_UINT_CLIP)
    MNG_ERROR (pData, MNG_WRONGCHUNK)  /* ouch */

  *iFirstid  = pChunk->iFirstid;       /* fill the fields */
  *iLastid   = pChunk->iLastid;
  *iCliptype = pChunk->iCliptype;
  *iClipl    = pChunk->iClipl;
  *iClipr    = pChunk->iClipr;
  *iClipt    = pChunk->iClipt;
  *iClipb    = pChunk->iClipb;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_CLIP, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_getchunk_show (mng_handle hHandle,
                                        mng_handle hChunk,
                                        mng_bool   *bEmpty,
                                        mng_uint16 *iFirstid,
                                        mng_uint16 *iLastid,
                                        mng_uint8  *iMode)
{
  mng_datap pData;
  mng_showp pChunk;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_SHOW, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData  = (mng_datap)hHandle;         /* and make it addressable */
  pChunk = (mng_showp)hChunk;          /* address the chunk */

  if (pChunk->sHeader.iChunkname != MNG_UINT_SHOW)
    MNG_ERROR (pData, MNG_WRONGCHUNK)  /* ouch */

  *bEmpty   = pChunk->bEmpty;          /* fill the fields */
  *iFirstid = pChunk->iFirstid;
  *iLastid  = pChunk->iLastid;
  *iMode    = pChunk->iMode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_SHOW, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_getchunk_term (mng_handle hHandle,
                                        mng_handle hChunk,
                                        mng_uint8  *iTermaction,
                                        mng_uint8  *iIteraction,
                                        mng_uint32 *iDelay,
                                        mng_uint32 *iItermax)
{
  mng_datap pData;
  mng_termp pChunk;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_TERM, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData  = (mng_datap)hHandle;         /* and make it addressable */
  pChunk = (mng_termp)hChunk;          /* address the chunk */

  if (pChunk->sHeader.iChunkname != MNG_UINT_TERM)
    MNG_ERROR (pData, MNG_WRONGCHUNK)  /* ouch */

  *iTermaction = pChunk->iTermaction;  /* fill the fields */
  *iIteraction = pChunk->iIteraction;
  *iDelay      = pChunk->iDelay;
  *iItermax    = pChunk->iItermax;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_TERM, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_getchunk_save (mng_handle hHandle,
                                        mng_handle hChunk,
                                        mng_bool   *bEmpty,
                                        mng_uint8  *iOffsettype,
                                        mng_uint32 *iCount)
{
  mng_datap pData;
  mng_savep pChunk;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_SAVE, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData  = (mng_datap)hHandle;         /* and make it addressable */
  pChunk = (mng_savep)hChunk;          /* address the chunk */

  if (pChunk->sHeader.iChunkname != MNG_UINT_SAVE)
    MNG_ERROR (pData, MNG_WRONGCHUNK)  /* ouch */

  *bEmpty      = pChunk->bEmpty;       /* fill the fields */
  *iOffsettype = pChunk->iOffsettype;
  *iCount      = pChunk->iCount;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_SAVE, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_getchunk_save_entry (mng_handle     hHandle,
                                              mng_handle     hChunk,
                                              mng_uint32     iEntry,
                                              mng_uint8      *iEntrytype,
                                              mng_uint32arr2 *iOffset,
                                              mng_uint32arr2 *iStarttime,
                                              mng_uint32     *iLayernr,
                                              mng_uint32     *iFramenr,
                                              mng_uint32     *iNamesize,
                                              mng_pchar      *zName)
{
  mng_datap       pData;
  mng_savep       pChunk;
  mng_save_entryp pEntry;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_SAVE_ENTRY, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData  = (mng_datap)hHandle;         /* and make it addressable */
  pChunk = (mng_savep)hChunk;          /* address the chunk */

  if (pChunk->sHeader.iChunkname != MNG_UINT_SAVE)
    MNG_ERROR (pData, MNG_WRONGCHUNK)  /* ouch */

  if (iEntry >= pChunk->iCount)        /* valid index ? */
    MNG_ERROR (pData, MNG_INVALIDENTRYIX)

  pEntry  = pChunk->pEntries + iEntry; /* address the entry */
                                       /* fill the fields */
  *iEntrytype      = pEntry->iEntrytype;
  (*iOffset)[0]    = pEntry->iOffset[0];
  (*iOffset)[1]    = pEntry->iOffset[1];
  (*iStarttime)[0] = pEntry->iStarttime[0];
  (*iStarttime)[1] = pEntry->iStarttime[1];
  *iLayernr        = pEntry->iLayernr;
  *iFramenr        = pEntry->iFramenr;
  *iNamesize       = pEntry->iNamesize;
  *zName           = pEntry->zName;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_SAVE_ENTRY, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_getchunk_seek (mng_handle hHandle,
                                        mng_handle hChunk,
                                        mng_uint32 *iNamesize,
                                        mng_pchar  *zName)
{
  mng_datap pData;
  mng_seekp pChunk;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_SEEK, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData  = (mng_datap)hHandle;         /* and make it addressable */
  pChunk = (mng_seekp)hChunk;          /* address the chunk */

  if (pChunk->sHeader.iChunkname != MNG_UINT_SEEK)
    MNG_ERROR (pData, MNG_WRONGCHUNK)  /* ouch */

  *iNamesize = pChunk->iNamesize;      /* fill the fields */
  *zName     = pChunk->zName;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_SEEK, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_getchunk_expi (mng_handle hHandle,
                                        mng_handle hChunk,
                                        mng_uint16 *iSnapshotid,
                                        mng_uint32 *iNamesize,
                                        mng_pchar  *zName)
{
  mng_datap pData;
  mng_expip pChunk;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_EXPI, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData  = (mng_datap)hHandle;         /* and make it addressable */
  pChunk = (mng_expip)hChunk;          /* address the chunk */

  if (pChunk->sHeader.iChunkname != MNG_UINT_eXPI)
    MNG_ERROR (pData, MNG_WRONGCHUNK)  /* ouch */

  *iSnapshotid = pChunk->iSnapshotid;  /* fill the fields */
  *iNamesize   = pChunk->iNamesize;
  *zName       = pChunk->zName;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_EXPI, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_getchunk_fpri (mng_handle hHandle,
                                        mng_handle hChunk,
                                        mng_uint8  *iDeltatype,
                                        mng_uint8  *iPriority)
{
  mng_datap pData;
  mng_fprip pChunk;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_FPRI, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData  = (mng_datap)hHandle;         /* and make it addressable */
  pChunk = (mng_fprip)hChunk;          /* address the chunk */

  if (pChunk->sHeader.iChunkname != MNG_UINT_fPRI)
    MNG_ERROR (pData, MNG_WRONGCHUNK)  /* ouch */

  *iDeltatype = pChunk->iDeltatype;    /* fill the fields */
  *iPriority  = pChunk->iPriority;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_FPRI, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_getchunk_need (mng_handle hHandle,
                                        mng_handle hChunk,
                                        mng_uint32 *iKeywordssize,
                                        mng_pchar  *zKeywords)
{
  mng_datap pData;
  mng_needp pChunk;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_NEED, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData  = (mng_datap)hHandle;         /* and make it addressable */
  pChunk = (mng_needp)hChunk;          /* address the chunk */

  if (pChunk->sHeader.iChunkname != MNG_UINT_nEED)
    MNG_ERROR (pData, MNG_WRONGCHUNK)  /* ouch */
                                       /* fill the fields */
  *iKeywordssize = pChunk->iKeywordssize;
  *zKeywords     = pChunk->zKeywords;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_NEED, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_getchunk_phyg (mng_handle hHandle,
                                        mng_handle hChunk,
                                        mng_bool   *bEmpty,
                                        mng_uint32 *iSizex,
                                        mng_uint32 *iSizey,
                                        mng_uint8  *iUnit)
{
  mng_datap pData;
  mng_phygp pChunk;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_PHYG, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData  = (mng_datap)hHandle;         /* and make it addressable */
  pChunk = (mng_phygp)hChunk;          /* address the chunk */

  if (pChunk->sHeader.iChunkname != MNG_UINT_pHYg)
    MNG_ERROR (pData, MNG_WRONGCHUNK)  /* ouch */

  *bEmpty = pChunk->bEmpty;            /* fill the fields */
  *iSizex = pChunk->iSizex;
  *iSizey = pChunk->iSizey;
  *iUnit  = pChunk->iUnit;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_PHYG, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */
/* B004 */
#ifdef MNG_INCLUDE_JNG
/* B004 */
mng_retcode MNG_DECL mng_getchunk_jhdr (mng_handle hHandle,
                                        mng_handle hChunk,
                                        mng_uint32 *iWidth,
                                        mng_uint32 *iHeight,
                                        mng_uint8  *iColortype,
                                        mng_uint8  *iImagesampledepth,
                                        mng_uint8  *iImagecompression,
                                        mng_uint8  *iImageinterlace,
                                        mng_uint8  *iAlphasampledepth,
                                        mng_uint8  *iAlphacompression,
                                        mng_uint8  *iAlphafilter,
                                        mng_uint8  *iAlphainterlace)
{
  mng_datap pData;
  mng_jhdrp pChunk;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_JHDR, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData  = (mng_datap)hHandle;         /* and make it addressable */
  pChunk = (mng_jhdrp)hChunk;          /* address the chunk */

  if (pChunk->sHeader.iChunkname != MNG_UINT_JHDR)
    MNG_ERROR (pData, MNG_WRONGCHUNK)  /* ouch */

  *iWidth            = pChunk->iWidth; /* fill the fields */          
  *iHeight           = pChunk->iHeight;
  *iColortype        = pChunk->iColortype;
  *iImagesampledepth = pChunk->iImagesampledepth;
  *iImagecompression = pChunk->iImagecompression;
  *iImageinterlace   = pChunk->iImageinterlace;
  *iAlphasampledepth = pChunk->iAlphasampledepth;
  *iAlphacompression = pChunk->iAlphacompression;
  *iAlphafilter      = pChunk->iAlphafilter;
  *iAlphainterlace   = pChunk->iAlphainterlace;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_JHDR, MNG_LC_END)
#endif

  return MNG_NOERROR;
}
/* B004 */
#endif /* MNG_INCLUDE_JNG */
/* B004 */
/* ************************************************************************** */
/* B004 */
#ifdef MNG_INCLUDE_JNG
/* B004 */
mng_retcode MNG_DECL mng_getchunk_jdat (mng_handle hHandle,
                                        mng_handle hChunk,
                                        mng_uint32 *iRawlen,
                                        mng_ptr    *pRawdata)
{
  mng_datap pData;
  mng_jdatp pChunk;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_JDAT, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData  = (mng_datap)hHandle;         /* and make it addressable */
  pChunk = (mng_jdatp)hChunk;          /* address the chunk */

  if (pChunk->sHeader.iChunkname != MNG_UINT_JDAT)
    MNG_ERROR (pData, MNG_WRONGCHUNK)  /* ouch */

  *iRawlen  = pChunk->iDatasize;       /* fill the fields */
  *pRawdata = pChunk->pData;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_JDAT, MNG_LC_END)
#endif

  return MNG_NOERROR;
}
/* B004 */
#endif /* MNG_INCLUDE_JNG */
/* B004 */
/* ************************************************************************** */

mng_retcode MNG_DECL mng_getchunk_dhdr (mng_handle hHandle,
                                        mng_handle hChunk,
                                        mng_uint16 *iObjectid,
                                        mng_uint8  *iImagetype,
                                        mng_uint8  *iDeltatype,
                                        mng_uint32 *iBlockwidth,
                                        mng_uint32 *iBlockheight,
                                        mng_uint32 *iBlockx,
                                        mng_uint32 *iBlocky)
{
  mng_datap pData;
  mng_dhdrp pChunk;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_DHDR, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData  = (mng_datap)hHandle;         /* and make it addressable */
  pChunk = (mng_dhdrp)hChunk;          /* address the chunk */

  if (pChunk->sHeader.iChunkname != MNG_UINT_DHDR)
    MNG_ERROR (pData, MNG_WRONGCHUNK)  /* ouch */

  *iObjectid    = pChunk->iObjectid;   /* fill the fields */
  *iImagetype   = pChunk->iImagetype;
  *iDeltatype   = pChunk->iDeltatype;
  *iBlockwidth  = pChunk->iBlockwidth;
  *iBlockheight = pChunk->iBlockheight;
  *iBlockx      = pChunk->iBlockx;
  *iBlocky      = pChunk->iBlocky;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_DHDR, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_getchunk_prom (mng_handle hHandle,
                                        mng_handle hChunk,
                                        mng_uint8  *iColortype,
                                        mng_uint8  *iSampledepth,
                                        mng_uint8  *iFilltype)
{
  mng_datap pData;
  mng_promp pChunk;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_PROM, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData  = (mng_datap)hHandle;         /* and make it addressable */
  pChunk = (mng_promp)hChunk;          /* address the chunk */

  if (pChunk->sHeader.iChunkname != MNG_UINT_PROM)
    MNG_ERROR (pData, MNG_WRONGCHUNK)  /* ouch */

  *iColortype   = pChunk->iColortype;  /* fill the fields */
  *iSampledepth = pChunk->iSampledepth;
  *iFilltype    = pChunk->iFilltype;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_PROM, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_getchunk_pplt (mng_handle hHandle,
                                        mng_handle hChunk,
                                        mng_uint32 *iCount)
{
  mng_datap pData;
  mng_ppltp pChunk;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_PPLT, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData  = (mng_datap)hHandle;         /* and make it addressable */
  pChunk = (mng_ppltp)hChunk;          /* address the chunk */

  if (pChunk->sHeader.iChunkname != MNG_UINT_PPLT)
    MNG_ERROR (pData, MNG_WRONGCHUNK)  /* ouch */

  *iCount = pChunk->iCount;            /* fill the field */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_PPLT, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_getchunk_pplt_entry (mng_handle hHandle,
                                              mng_handle hChunk,
                                              mng_uint32 iEntry,
                                              mng_uint16 *iRed,
                                              mng_uint16 *iGreen,
                                              mng_uint16 *iBlue,
                                              mng_uint16 *iAlpha,
                                              mng_bool   *bUsed)
{
  mng_datap       pData;
  mng_ppltp       pChunk;
  mng_pplt_entryp pEntry;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_PPLT_ENTRY, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData  = (mng_datap)hHandle;         /* and make it addressable */
  pChunk = (mng_ppltp)hChunk;          /* address the chunk */

  if (pChunk->sHeader.iChunkname != MNG_UINT_PPLT)
    MNG_ERROR (pData, MNG_WRONGCHUNK)  /* ouch */

  if (iEntry >= pChunk->iCount)        /* valid index ? */
    MNG_ERROR (pData, MNG_INVALIDENTRYIX)

  pEntry  = &pChunk->aEntries[iEntry]; /* address the entry */

  *iRed   = pEntry->iRed;              /* fill the fields */
  *iGreen = pEntry->iGreen;
  *iBlue  = pEntry->iBlue;
  *iAlpha = pEntry->iAlpha;
  *bUsed  = pEntry->bUsed;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_PPLT_ENTRY, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_getchunk_drop (mng_handle   hHandle,
                                        mng_handle   hChunk,
                                        mng_uint32   *iCount,
                                        mng_chunkidp *pChunknames)
{
  mng_datap pData;
  mng_dropp pChunk;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_DROP, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData  = (mng_datap)hHandle;         /* and make it addressable */
  pChunk = (mng_dropp)hChunk;          /* address the chunk */

  if (pChunk->sHeader.iChunkname != MNG_UINT_DROP)
    MNG_ERROR (pData, MNG_WRONGCHUNK)  /* ouch */

  *iCount      = pChunk->iCount;       /* fill the fields */
  *pChunknames = pChunk->pChunknames;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_DROP, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_getchunk_dbyk (mng_handle  hHandle,
                                        mng_handle  hChunk,
                                        mng_chunkid *iChunkname,
                                        mng_uint8   *iPolarity,
                                        mng_uint32  *iKeywordssize,
                                        mng_pchar   *zKeywords)
{
  mng_datap pData;
  mng_dbykp pChunk;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_DBYK, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData  = (mng_datap)hHandle;         /* and make it addressable */
  pChunk = (mng_dbykp)hChunk;          /* address the chunk */

  if (pChunk->sHeader.iChunkname != MNG_UINT_DBYK)
    MNG_ERROR (pData, MNG_WRONGCHUNK)  /* ouch */

  *iChunkname    = pChunk->iChunkname; /* fill the fields */  
  *iPolarity     = pChunk->iPolarity;
  *iKeywordssize = pChunk->iKeywordssize;
  *zKeywords     = pChunk->zKeywords;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_DBYK, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_getchunk_ordr (mng_handle hHandle,
                                        mng_handle hChunk,
                                        mng_uint32 *iCount)
{
  mng_datap pData;
  mng_ordrp pChunk;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_ORDR, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData  = (mng_datap)hHandle;         /* and make it addressable */
  pChunk = (mng_ordrp)hChunk;          /* address the chunk */

  if (pChunk->sHeader.iChunkname != MNG_UINT_ORDR)
    MNG_ERROR (pData, MNG_WRONGCHUNK)  /* ouch */

  *iCount = pChunk->iCount;            /* fill the field */ 

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_ORDR, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_getchunk_ordr_entry (mng_handle  hHandle,
                                              mng_handle  hChunk,
                                              mng_uint32  iEntry,
                                              mng_chunkid *iChunkname,
                                              mng_uint8   *iOrdertype)
{
  mng_datap       pData;
  mng_ordrp       pChunk;
  mng_ordr_entryp pEntry;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_ORDR_ENTRY, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData  = (mng_datap)hHandle;         /* and make it addressable */
  pChunk = (mng_ordrp)hChunk;          /* address the chunk */

  if (pChunk->sHeader.iChunkname != MNG_UINT_ORDR)
    MNG_ERROR (pData, MNG_WRONGCHUNK)  /* ouch */

  if (iEntry >= pChunk->iCount)        /* valid index ? */
    MNG_ERROR (pData, MNG_INVALIDENTRYIX)

  pEntry = pChunk->pEntries + iEntry;  /* address the proper entry */

  *iChunkname = pEntry->iChunkname;    /* fill the fields */
  *iOrdertype = pEntry->iOrdertype;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_ORDR_ENTRY, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_getchunk_magn (mng_handle hHandle,
                                        mng_handle hChunk,
                                        mng_uint16 *iFirstid,
                                        mng_uint16 *iLastid,
                                        mng_uint16 *iMethodX,
                                        mng_uint16 *iMX,
                                        mng_uint16 *iMY,
                                        mng_uint16 *iML,
                                        mng_uint16 *iMR,
                                        mng_uint16 *iMT,
                                        mng_uint16 *iMB,
                                        mng_uint16 *iMethodY)
{
  mng_datap pData;
  mng_magnp pChunk;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_MAGN, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData  = (mng_datap)hHandle;         /* and make it addressable */
  pChunk = (mng_magnp)hChunk;          /* address the chunk */

  if (pChunk->sHeader.iChunkname != MNG_UINT_MAGN)
    MNG_ERROR (pData, MNG_WRONGCHUNK)  /* ouch */

  *iFirstid = pChunk->iFirstid;        /* fill the fields */
  *iLastid  = pChunk->iLastid;
  *iMethodX = pChunk->iMethodX;
  *iMX      = pChunk->iMX;
  *iMY      = pChunk->iMY;
  *iML      = pChunk->iML;
  *iMR      = pChunk->iMR;
  *iMT      = pChunk->iMT;
  *iMB      = pChunk->iMB;
  *iMethodY = pChunk->iMethodY;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_MAGN, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_getchunk_unknown (mng_handle  hHandle,
                                           mng_handle  hChunk,
                                           mng_chunkid *iChunkname,
                                           mng_uint32  *iRawlen,
                                           mng_ptr     *pRawdata)
{
  mng_datap          pData;
  mng_unknown_chunkp pChunk;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_UNKNOWN, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData  = (mng_datap)hHandle;         /* and make it addressable */
  pChunk = (mng_unknown_chunkp)hChunk; /* address the chunk */

  if (pChunk->sHeader.fCreate != init_unknown)
    MNG_ERROR (pData, MNG_WRONGCHUNK)  /* ouch */
                                       /* fill the fields */
  *iChunkname = pChunk->sHeader.iChunkname;
  *iRawlen    = pChunk->iDatasize;
  *pRawdata   = pChunk->pData;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETCHUNK_UNKNOWN, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */
/* ************************************************************************** */
/* B004 */
#ifdef MNG_INCLUDE_WRITE_PROCS
/* B004 */
/* ************************************************************************** */

mng_retcode MNG_DECL mng_putchunk_ihdr (mng_handle hHandle,
                                        mng_uint32 iWidth,
                                        mng_uint32 iHeight,
                                        mng_uint8  iBitdepth,
                                        mng_uint8  iColortype,
                                        mng_uint8  iCompression,
                                        mng_uint8  iFilter,
                                        mng_uint8  iInterlace)
{
  mng_datap        pData;
  mng_chunkp       pChunk;
  mng_retcode      iRetcode;
  mng_chunk_header sChunkheader =
          {MNG_UINT_IHDR, init_ihdr, free_ihdr, read_ihdr, write_ihdr, 0, 0};

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_IHDR, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = (mng_datap)hHandle;          /* and make it addressable */

  if (!pData->bCreating)               /* aren't we creating a new file ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
                                       /* create the chunk */
  iRetcode = init_ihdr (pData, &sChunkheader, &pChunk);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;
                                       /* fill the chunk */
  ((mng_ihdrp)pChunk)->iWidth       = iWidth;
  ((mng_ihdrp)pChunk)->iHeight      = iHeight;
  ((mng_ihdrp)pChunk)->iBitdepth    = iBitdepth;
  ((mng_ihdrp)pChunk)->iColortype   = iColortype;
  ((mng_ihdrp)pChunk)->iCompression = iCompression;
  ((mng_ihdrp)pChunk)->iFilter      = iFilter;
  ((mng_ihdrp)pChunk)->iInterlace   = iInterlace;

  add_chunk (pData, pChunk);           /* add it to the list */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_IHDR, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_putchunk_plte (mng_handle   hHandle,
                                        mng_uint32   iCount,
                                        mng_palette8 aPalette)
{
  mng_datap        pData;
  mng_chunkp       pChunk;
  mng_retcode      iRetcode;
  mng_chunk_header sChunkheader =
          {MNG_UINT_PLTE, init_plte, free_plte, read_plte, write_plte, 0, 0};

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_PLTE, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = (mng_datap)hHandle;          /* and make it addressable */

  if (!pData->bCreating)               /* aren't we creating a new file ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
                                       /* must have had a header first! */
  if (pData->iFirstchunkadded == 0)
    MNG_ERROR (pData, MNG_NOHEADER)
                                       /* create the chunk */
  iRetcode = init_plte (pData, &sChunkheader, &pChunk);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;
                                       /* fill the chunk */
  ((mng_pltep)pChunk)->iEntrycount = iCount;
  ((mng_pltep)pChunk)->bEmpty      = (mng_bool)(iCount == 0);

  MNG_COPY (((mng_pltep)pChunk)->aEntries, aPalette, sizeof (mng_palette8))

  add_chunk (pData, pChunk);           /* add it to the list */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_PLTE, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_putchunk_idat (mng_handle hHandle,
                                        mng_uint32 iRawlen,
                                        mng_ptr    pRawdata)
{
  mng_datap        pData;
  mng_chunkp       pChunk;
  mng_retcode      iRetcode;
  mng_chunk_header sChunkheader =
          {MNG_UINT_IDAT, init_idat, free_idat, read_idat, write_idat, 0, 0};

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_IDAT, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = (mng_datap)hHandle;          /* and make it addressable */

  if (!pData->bCreating)               /* aren't we creating a new file ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
                                       /* must have had a header first! */
  if (pData->iFirstchunkadded == 0)
    MNG_ERROR (pData, MNG_NOHEADER)
                                       /* create the chunk */
  iRetcode = init_idat (pData, &sChunkheader, &pChunk);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;
                                       /* fill the chunk */
  ((mng_idatp)pChunk)->bEmpty    = (mng_bool)(iRawlen == 0);
  ((mng_idatp)pChunk)->iDatasize = iRawlen;

  if (iRawlen)
  {
    MNG_ALLOC (pData, ((mng_idatp)pChunk)->pData, iRawlen)
    MNG_COPY (((mng_idatp)pChunk)->pData, pRawdata, iRawlen)
  }

  add_chunk (pData, pChunk);           /* add it to the list */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_IDAT, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_putchunk_iend (mng_handle hHandle)
{
  mng_datap        pData;
  mng_chunkp       pChunk;
  mng_retcode      iRetcode;
  mng_chunk_header sChunkheader =
          {MNG_UINT_IEND, init_iend, free_iend, read_iend, write_iend, 0, 0};

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_IEND, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = (mng_datap)hHandle;          /* and make it addressable */

  if (!pData->bCreating)               /* aren't we creating a new file ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
                                       /* must have had a header first! */
  if (pData->iFirstchunkadded == 0)
    MNG_ERROR (pData, MNG_NOHEADER)
                                       /* create the chunk */
  iRetcode = init_iend (pData, &sChunkheader, &pChunk);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;

  add_chunk (pData, pChunk);           /* add it to the list */

#ifdef MNG_INCLUDE_JNG
  if ((pData->iFirstchunkadded == MNG_UINT_IHDR) ||
      (pData->iFirstchunkadded == MNG_UINT_JHDR)    )
#else
  if (pData->iFirstchunkadded == MNG_UINT_IHDR)
#endif
    pData->bCreating = MNG_FALSE;      /* should be last chunk !!! */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_IEND, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_putchunk_trns (mng_handle   hHandle,
                                        mng_bool     bEmpty,
                                        mng_bool     bGlobal,
                                        mng_uint8    iType,
                                        mng_uint32   iCount,
                                        mng_uint8arr aAlphas,
                                        mng_uint16   iGray,
                                        mng_uint16   iRed,
                                        mng_uint16   iGreen,
                                        mng_uint16   iBlue,
                                        mng_uint32   iRawlen,
                                        mng_uint8arr aRawdata)
{
  mng_datap        pData;
  mng_chunkp       pChunk;
  mng_retcode      iRetcode;
  mng_chunk_header sChunkheader =
          {MNG_UINT_tRNS, init_trns, free_trns, read_trns, write_trns, 0, 0};

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_TRNS, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = (mng_datap)hHandle;          /* and make it addressable */

  if (!pData->bCreating)               /* aren't we creating a new file ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
                                       /* must have had a header first! */
  if (pData->iFirstchunkadded == 0)
    MNG_ERROR (pData, MNG_NOHEADER)
                                       /* create the chunk */
  iRetcode = init_trns (pData, &sChunkheader, &pChunk);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;
                                       /* fill the chunk */
  ((mng_trnsp)pChunk)->bEmpty   = bEmpty;
  ((mng_trnsp)pChunk)->bGlobal  = bGlobal;
  ((mng_trnsp)pChunk)->iType    = iType;
  ((mng_trnsp)pChunk)->iCount   = iCount;
  ((mng_trnsp)pChunk)->iGray    = iGray;
  ((mng_trnsp)pChunk)->iRed     = iRed;
  ((mng_trnsp)pChunk)->iGreen   = iGreen;
  ((mng_trnsp)pChunk)->iBlue    = iBlue;
  ((mng_trnsp)pChunk)->iRawlen  = iRawlen;

  MNG_COPY (((mng_trnsp)pChunk)->aEntries, aAlphas,  sizeof (mng_uint8arr))
  MNG_COPY (((mng_trnsp)pChunk)->aRawdata, aRawdata, sizeof (mng_uint8arr))

  add_chunk (pData, pChunk);           /* add it to the list */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_TRNS, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_putchunk_gama (mng_handle hHandle,
                                        mng_bool   bEmpty,
                                        mng_uint32 iGamma)
{
  mng_datap        pData;
  mng_chunkp       pChunk;
  mng_retcode      iRetcode;
  mng_chunk_header sChunkheader =
          {MNG_UINT_gAMA, init_gama, free_gama, read_gama, write_gama, 0, 0};

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_GAMA, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = (mng_datap)hHandle;          /* and make it addressable */

  if (!pData->bCreating)               /* aren't we creating a new file ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
                                       /* must have had a header first! */
  if (pData->iFirstchunkadded == 0)
    MNG_ERROR (pData, MNG_NOHEADER)
                                       /* create the chunk */
  iRetcode = init_gama (pData, &sChunkheader, &pChunk);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;
                                       /* fill the chunk */
  ((mng_gamap)pChunk)->bEmpty = bEmpty;
  ((mng_gamap)pChunk)->iGamma = iGamma;

  add_chunk (pData, pChunk);           /* add it to the list */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_GAMA, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_putchunk_chrm (mng_handle hHandle,
                                        mng_bool   bEmpty,
                                        mng_uint32 iWhitepointx,
                                        mng_uint32 iWhitepointy,
                                        mng_uint32 iRedx,
                                        mng_uint32 iRedy,
                                        mng_uint32 iGreenx,
                                        mng_uint32 iGreeny,
                                        mng_uint32 iBluex,
                                        mng_uint32 iBluey)
{
  mng_datap        pData;
  mng_chunkp       pChunk;
  mng_retcode      iRetcode;
  mng_chunk_header sChunkheader =
          {MNG_UINT_cHRM, init_chrm, free_chrm, read_chrm, write_chrm, 0, 0};

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_CHRM, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = (mng_datap)hHandle;          /* and make it addressable */

  if (!pData->bCreating)               /* aren't we creating a new file ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
                                       /* must have had a header first! */
  if (pData->iFirstchunkadded == 0)
    MNG_ERROR (pData, MNG_NOHEADER)
                                       /* create the chunk */
  iRetcode = init_chrm (pData, &sChunkheader, &pChunk);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;
                                       /* fill the chunk */
  ((mng_chrmp)pChunk)->bEmpty       = bEmpty;
  ((mng_chrmp)pChunk)->iWhitepointx = iWhitepointx;
  ((mng_chrmp)pChunk)->iWhitepointy = iWhitepointy;
  ((mng_chrmp)pChunk)->iRedx        = iRedx;
  ((mng_chrmp)pChunk)->iRedy        = iRedy;
  ((mng_chrmp)pChunk)->iGreenx      = iGreenx;
  ((mng_chrmp)pChunk)->iGreeny      = iGreeny;
  ((mng_chrmp)pChunk)->iBluex       = iBluex;
  ((mng_chrmp)pChunk)->iBluey       = iBluey;

  add_chunk (pData, pChunk);           /* add it to the list */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_CHRM, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_putchunk_srgb (mng_handle hHandle,
                                        mng_bool   bEmpty,
                                        mng_uint8  iRenderingintent)
{
  mng_datap        pData;
  mng_chunkp       pChunk;
  mng_retcode      iRetcode;
  mng_chunk_header sChunkheader =
          {MNG_UINT_sRGB, init_srgb, free_srgb, read_srgb, write_srgb, 0, 0};

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_SRGB, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = (mng_datap)hHandle;          /* and make it addressable */

  if (!pData->bCreating)               /* aren't we creating a new file ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
                                       /* must have had a header first! */
  if (pData->iFirstchunkadded == 0)
    MNG_ERROR (pData, MNG_NOHEADER)
                                       /* create the chunk */
  iRetcode = init_srgb (pData, &sChunkheader, &pChunk);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;
                                       /* fill the chunk */
  ((mng_srgbp)pChunk)->bEmpty           = bEmpty;
  ((mng_srgbp)pChunk)->iRenderingintent = iRenderingintent;

  add_chunk (pData, pChunk);           /* add it to the list */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_SRGB, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_putchunk_iccp (mng_handle hHandle,
                                        mng_bool   bEmpty,
                                        mng_uint32 iNamesize,
                                        mng_pchar  zName,
                                        mng_uint8  iCompression,
                                        mng_uint32 iProfilesize,
                                        mng_ptr    pProfile)
{
  mng_datap        pData;
  mng_chunkp       pChunk;
  mng_retcode      iRetcode;
  mng_chunk_header sChunkheader =
          {MNG_UINT_iCCP, init_iccp, free_iccp, read_iccp, write_iccp, 0, 0};

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_ICCP, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = (mng_datap)hHandle;          /* and make it addressable */

  if (!pData->bCreating)               /* aren't we creating a new file ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
                                       /* must have had a header first! */
  if (pData->iFirstchunkadded == 0)
    MNG_ERROR (pData, MNG_NOHEADER)
                                       /* create the chunk */
  iRetcode = init_iccp (pData, &sChunkheader, &pChunk);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;
                                       /* fill the chunk */
  ((mng_iccpp)pChunk)->bEmpty       = bEmpty;
  ((mng_iccpp)pChunk)->iNamesize    = iNamesize;
  ((mng_iccpp)pChunk)->iCompression = iCompression;
  ((mng_iccpp)pChunk)->iProfilesize = iProfilesize;

  if (iNamesize)
  {
    MNG_ALLOC (pData, ((mng_iccpp)pChunk)->zName, iNamesize + 1)
    MNG_COPY (((mng_iccpp)pChunk)->zName, zName, iNamesize)
  }

  if (iProfilesize)
  {
    MNG_ALLOC (pData, ((mng_iccpp)pChunk)->pProfile, iProfilesize)
    MNG_COPY (((mng_iccpp)pChunk)->pProfile, pProfile, iProfilesize)
  }

  add_chunk (pData, pChunk);           /* add it to the list */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_ICCP, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_putchunk_text (mng_handle hHandle,
                                        mng_uint32 iKeywordsize,
                                        mng_pchar  zKeyword,
                                        mng_uint32 iTextsize,
                                        mng_pchar  zText)
{
  mng_datap        pData;
  mng_chunkp       pChunk;
  mng_retcode      iRetcode;
  mng_chunk_header sChunkheader =
          {MNG_UINT_tEXt, init_text, free_text, read_text, write_text, 0, 0};

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_TEXT, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = (mng_datap)hHandle;          /* and make it addressable */

  if (!pData->bCreating)               /* aren't we creating a new file ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
                                       /* must have had a header first! */
  if (pData->iFirstchunkadded == 0)
    MNG_ERROR (pData, MNG_NOHEADER)
                                       /* create the chunk */
  iRetcode = init_text (pData, &sChunkheader, &pChunk);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;
                                       /* fill the chunk */
  ((mng_textp)pChunk)->iKeywordsize = iKeywordsize;
  ((mng_textp)pChunk)->iTextsize    = iTextsize;

  if (iKeywordsize)
  {
    MNG_ALLOC (pData, ((mng_textp)pChunk)->zKeyword, iKeywordsize + 1)
    MNG_COPY (((mng_textp)pChunk)->zKeyword, zKeyword, iKeywordsize)
  }

  if (iTextsize)
  {
    MNG_ALLOC (pData, ((mng_textp)pChunk)->zText, iTextsize + 1)
    MNG_COPY (((mng_textp)pChunk)->zText, zText, iTextsize)
  }

  add_chunk (pData, pChunk);           /* add it to the list */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_TEXT, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_putchunk_ztxt (mng_handle hHandle,
                                        mng_uint32 iKeywordsize,
                                        mng_pchar  zKeyword,
                                        mng_uint8  iCompression,
                                        mng_uint32 iTextsize,
                                        mng_pchar  zText)
{
  mng_datap        pData;
  mng_chunkp       pChunk;
  mng_retcode      iRetcode;
  mng_chunk_header sChunkheader =
          {MNG_UINT_zTXt, init_ztxt, free_ztxt, read_ztxt, write_ztxt, 0, 0};

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_ZTXT, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = (mng_datap)hHandle;          /* and make it addressable */

  if (!pData->bCreating)               /* aren't we creating a new file ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
                                       /* must have had a header first! */
  if (pData->iFirstchunkadded == 0)
    MNG_ERROR (pData, MNG_NOHEADER)
                                       /* create the chunk */
  iRetcode = init_ztxt (pData, &sChunkheader, &pChunk);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;
                                       /* fill the chunk */
  ((mng_ztxtp)pChunk)->iKeywordsize = iKeywordsize;
  ((mng_ztxtp)pChunk)->iCompression = iCompression;
  ((mng_ztxtp)pChunk)->iTextsize    = iTextsize;

  if (iKeywordsize)
  {
    MNG_ALLOC (pData, ((mng_ztxtp)pChunk)->zKeyword, iKeywordsize + 1)
    MNG_COPY (((mng_ztxtp)pChunk)->zKeyword, zKeyword, iKeywordsize)
  }

  if (iTextsize)
  {
    MNG_ALLOC (pData, ((mng_ztxtp)pChunk)->zText, iTextsize + 1)
    MNG_COPY  (((mng_ztxtp)pChunk)->zText, zText, iTextsize)
  }

  add_chunk (pData, pChunk);           /* add it to the list */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_ZTXT, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_putchunk_itxt (mng_handle hHandle,
                                        mng_uint32 iKeywordsize,
                                        mng_pchar  zKeyword,
                                        mng_uint8  iCompressionflag,
                                        mng_uint8  iCompressionmethod,
                                        mng_uint32 iLanguagesize,
                                        mng_pchar  zLanguage,
                                        mng_uint32 iTranslationsize,
                                        mng_pchar  zTranslation,
                                        mng_uint32 iTextsize,
                                        mng_pchar  zText)
{
  mng_datap        pData;
  mng_chunkp       pChunk;
  mng_retcode      iRetcode;
  mng_chunk_header sChunkheader =
          {MNG_UINT_iTXt, init_itxt, free_itxt, read_itxt, write_itxt, 0, 0};

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_ITXT, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = (mng_datap)hHandle;          /* and make it addressable */

  if (!pData->bCreating)               /* aren't we creating a new file ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
                                       /* must have had a header first! */
  if (pData->iFirstchunkadded == 0)
    MNG_ERROR (pData, MNG_NOHEADER)
                                       /* create the chunk */
  iRetcode = init_itxt (pData, &sChunkheader, &pChunk);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;
                                       /* fill the chunk */
  ((mng_itxtp)pChunk)->iKeywordsize       = iKeywordsize;
  ((mng_itxtp)pChunk)->iCompressionflag   = iCompressionflag;
  ((mng_itxtp)pChunk)->iCompressionmethod = iCompressionmethod;
  ((mng_itxtp)pChunk)->iLanguagesize      = iLanguagesize;
  ((mng_itxtp)pChunk)->iTranslationsize   = iTranslationsize;
  ((mng_itxtp)pChunk)->iTextsize          = iTextsize;

  if (iKeywordsize)
  {
    MNG_ALLOC (pData, ((mng_itxtp)pChunk)->zKeyword, iKeywordsize + 1)
    MNG_COPY (((mng_itxtp)pChunk)->zKeyword, zKeyword, iKeywordsize)
  }

  if (iLanguagesize)
  {
    MNG_ALLOC (pData, ((mng_itxtp)pChunk)->zLanguage, iLanguagesize + 1)
    MNG_COPY (((mng_itxtp)pChunk)->zLanguage, zLanguage, iLanguagesize)
  }

  if (iTranslationsize)
  {
    MNG_ALLOC (pData, ((mng_itxtp)pChunk)->zTranslation, iTranslationsize + 1)
    MNG_COPY (((mng_itxtp)pChunk)->zTranslation, zTranslation, iTranslationsize)
  }

  if (iTextsize)
  {
    MNG_ALLOC (pData, ((mng_itxtp)pChunk)->zText, iTextsize + 1)
    MNG_COPY (((mng_itxtp)pChunk)->zText, zText, iTextsize)
  }

  add_chunk (pData, pChunk);           /* add it to the list */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_ITXT, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_putchunk_bkgd (mng_handle hHandle,
                                        mng_bool   bEmpty,
                                        mng_uint8  iType,
                                        mng_uint8  iIndex,
                                        mng_uint16 iGray,
                                        mng_uint16 iRed,
                                        mng_uint16 iGreen,
                                        mng_uint16 iBlue)
{
  mng_datap        pData;
  mng_chunkp       pChunk;
  mng_retcode      iRetcode;
  mng_chunk_header sChunkheader =
          {MNG_UINT_bKGD, init_bkgd, free_bkgd, read_bkgd, write_bkgd, 0, 0};

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_BKGD, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = (mng_datap)hHandle;          /* and make it addressable */

  if (!pData->bCreating)               /* aren't we creating a new file ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
                                       /* must have had a header first! */
  if (pData->iFirstchunkadded == 0)
    MNG_ERROR (pData, MNG_NOHEADER)
                                       /* create the chunk */
  iRetcode = init_bkgd (pData, &sChunkheader, &pChunk);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;
                                       /* fill the chunk */
  ((mng_bkgdp)pChunk)->bEmpty = bEmpty;
  ((mng_bkgdp)pChunk)->iType  = iType;
  ((mng_bkgdp)pChunk)->iIndex = iIndex;
  ((mng_bkgdp)pChunk)->iGray  = iGray;
  ((mng_bkgdp)pChunk)->iRed   = iRed;
  ((mng_bkgdp)pChunk)->iGreen = iGreen;
  ((mng_bkgdp)pChunk)->iBlue  = iBlue;

  add_chunk (pData, pChunk);           /* add it to the list */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_BKGD, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_putchunk_phys (mng_handle hHandle,
                                        mng_bool   bEmpty,
                                        mng_uint32 iSizex,
                                        mng_uint32 iSizey,
                                        mng_uint8  iUnit)
{
  mng_datap        pData;
  mng_chunkp       pChunk;
  mng_retcode      iRetcode;
  mng_chunk_header sChunkheader =
          {MNG_UINT_pHYs, init_phys, free_phys, read_phys, write_phys, 0, 0};

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_PHYS, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = (mng_datap)hHandle;          /* and make it addressable */

  if (!pData->bCreating)               /* aren't we creating a new file ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
                                       /* must have had a header first! */
  if (pData->iFirstchunkadded == 0)
    MNG_ERROR (pData, MNG_NOHEADER)
                                       /* create the chunk */
  iRetcode = init_phys (pData, &sChunkheader, &pChunk);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;
                                       /* fill the chunk */
  ((mng_physp)pChunk)->bEmpty = bEmpty;
  ((mng_physp)pChunk)->iSizex = iSizex;
  ((mng_physp)pChunk)->iSizey = iSizey;
  ((mng_physp)pChunk)->iUnit  = iUnit;

  add_chunk (pData, pChunk);           /* add it to the list */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_PHYS, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_putchunk_sbit (mng_handle    hHandle,
                                        mng_bool      bEmpty,
                                        mng_uint8     iType,
                                        mng_uint8arr4 aBits)
{
  mng_datap        pData;
  mng_chunkp       pChunk;
  mng_retcode      iRetcode;
  mng_chunk_header sChunkheader =
          {MNG_UINT_sBIT, init_sbit, free_sbit, read_sbit, write_sbit, 0, 0};

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_SBIT, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = (mng_datap)hHandle;          /* and make it addressable */

  if (!pData->bCreating)               /* aren't we creating a new file ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
                                       /* must have had a header first! */
  if (pData->iFirstchunkadded == 0)
    MNG_ERROR (pData, MNG_NOHEADER)
                                       /* create the chunk */
  iRetcode = init_sbit (pData, &sChunkheader, &pChunk);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;
                                       /* fill the chunk */
  ((mng_sbitp)pChunk)->bEmpty   = bEmpty;
  ((mng_sbitp)pChunk)->iType    = iType;
  ((mng_sbitp)pChunk)->aBits[0] = aBits[0];
  ((mng_sbitp)pChunk)->aBits[1] = aBits[1];
  ((mng_sbitp)pChunk)->aBits[2] = aBits[2];
  ((mng_sbitp)pChunk)->aBits[3] = aBits[3];

  add_chunk (pData, pChunk);           /* add it to the list */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_SBIT, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_putchunk_splt (mng_handle hHandle,
                                        mng_bool   bEmpty,
                                        mng_uint32 iNamesize,
                                        mng_pchar  zName,
                                        mng_uint8  iSampledepth,
                                        mng_uint32 iEntrycount,
                                        mng_ptr    pEntries)
{
  mng_datap        pData;
  mng_chunkp       pChunk;
  mng_retcode      iRetcode;
  mng_chunk_header sChunkheader =
          {MNG_UINT_sPLT, init_splt, free_splt, read_splt, write_splt, 0, 0};

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_SPLT, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = (mng_datap)hHandle;          /* and make it addressable */

  if (!pData->bCreating)               /* aren't we creating a new file ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
                                       /* must have had a header first! */
  if (pData->iFirstchunkadded == 0)
    MNG_ERROR (pData, MNG_NOHEADER)
                                       /* create the chunk */
  iRetcode = init_splt (pData, &sChunkheader, &pChunk);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;
                                       /* fill the chunk */
  ((mng_spltp)pChunk)->bEmpty       = bEmpty;
  ((mng_spltp)pChunk)->iNamesize    = iNamesize;
  ((mng_spltp)pChunk)->iSampledepth = iSampledepth;
  ((mng_spltp)pChunk)->iEntrycount  = iEntrycount;

  if (iNamesize)
  {
    MNG_ALLOC (pData, ((mng_spltp)pChunk)->zName, iNamesize + 1)
    MNG_COPY (((mng_spltp)pChunk)->zName, zName, iNamesize)
  }

  if (iEntrycount)
  {
    mng_uint32 iSize = iEntrycount * ((iSampledepth >> 1) + 2);

    MNG_ALLOC (pData, ((mng_spltp)pChunk)->pEntries, iSize)
    MNG_COPY  (((mng_spltp)pChunk)->pEntries, pEntries, iSize)
  }

  add_chunk (pData, pChunk);           /* add it to the list */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_SPLT, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_putchunk_hist (mng_handle    hHandle,
                                        mng_uint32    iEntrycount,
                                        mng_uint16arr aEntries)
{
  mng_datap        pData;
  mng_chunkp       pChunk;
  mng_retcode      iRetcode;
  mng_chunk_header sChunkheader =
          {MNG_UINT_hIST, init_hist, free_hist, read_hist, write_hist, 0, 0};

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_HIST, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = (mng_datap)hHandle;          /* and make it addressable */

  if (!pData->bCreating)               /* aren't we creating a new file ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
                                       /* must have had a header first! */
  if (pData->iFirstchunkadded == 0)
    MNG_ERROR (pData, MNG_NOHEADER)
                                       /* create the chunk */
  iRetcode = init_hist (pData, &sChunkheader, &pChunk);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;
                                       /* fill the chunk */
  ((mng_histp)pChunk)->iEntrycount = iEntrycount;

  MNG_COPY (((mng_histp)pChunk)->aEntries, aEntries, sizeof (mng_uint16arr))

  add_chunk (pData, pChunk);           /* add it to the list */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_HIST, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_putchunk_time (mng_handle hHandle,
                                        mng_uint16 iYear,
                                        mng_uint8  iMonth,
                                        mng_uint8  iDay,
                                        mng_uint8  iHour,
                                        mng_uint8  iMinute,
                                        mng_uint8  iSecond)
{
  mng_datap        pData;
  mng_chunkp       pChunk;
  mng_retcode      iRetcode;
  mng_chunk_header sChunkheader =
          {MNG_UINT_tIME, init_time, free_time, read_time, write_time, 0, 0};

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_TIME, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = (mng_datap)hHandle;          /* and make it addressable */

  if (!pData->bCreating)               /* aren't we creating a new file ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
                                       /* must have had a header first! */
  if (pData->iFirstchunkadded == 0)
    MNG_ERROR (pData, MNG_NOHEADER)
                                       /* create the chunk */
  iRetcode = init_time (pData, &sChunkheader, &pChunk);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;
                                       /* fill the chunk */
  ((mng_timep)pChunk)->iYear   = iYear;
  ((mng_timep)pChunk)->iMonth  = iMonth;
  ((mng_timep)pChunk)->iDay    = iDay;
  ((mng_timep)pChunk)->iHour   = iHour;
  ((mng_timep)pChunk)->iMinute = iMinute;
  ((mng_timep)pChunk)->iSecond = iSecond;

  add_chunk (pData, pChunk);           /* add it to the list */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_TIME, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_putchunk_mhdr (mng_handle hHandle,
                                        mng_uint32 iWidth,
                                        mng_uint32 iHeight,
                                        mng_uint32 iTicks,
                                        mng_uint32 iLayercount,
                                        mng_uint32 iFramecount,
                                        mng_uint32 iPlaytime,
                                        mng_uint32 iSimplicity)
{
  mng_datap        pData;
  mng_chunkp       pChunk;
  mng_retcode      iRetcode;
  mng_chunk_header sChunkheader =
          {MNG_UINT_MHDR, init_mhdr, free_mhdr, read_mhdr, write_mhdr, 0, 0};

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_MHDR, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = (mng_datap)hHandle;          /* and make it addressable */

  if (!pData->bCreating)               /* aren't we creating a new file ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
                                       /* create the chunk */
  iRetcode = init_mhdr (pData, &sChunkheader, &pChunk);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;
                                       /* fill the chunk */
  ((mng_mhdrp)pChunk)->iWidth      = iWidth;
  ((mng_mhdrp)pChunk)->iHeight     = iHeight;
  ((mng_mhdrp)pChunk)->iTicks      = iTicks;
  ((mng_mhdrp)pChunk)->iLayercount = iLayercount;
  ((mng_mhdrp)pChunk)->iFramecount = iFramecount;
  ((mng_mhdrp)pChunk)->iPlaytime   = iPlaytime;
  ((mng_mhdrp)pChunk)->iSimplicity = iSimplicity;

  add_chunk (pData, pChunk);           /* add it to the list */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_MHDR, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_putchunk_mend (mng_handle hHandle)
{
  mng_datap        pData;
  mng_chunkp       pChunk;
  mng_retcode      iRetcode;
  mng_chunk_header sChunkheader =
          {MNG_UINT_MEND, init_mend, free_mend, read_mend, write_mend, 0, 0};

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_MEND, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = (mng_datap)hHandle;          /* and make it addressable */

  if (!pData->bCreating)               /* aren't we creating a new file ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
                                       /* must have had a header first! */
  if (pData->iFirstchunkadded == 0)
    MNG_ERROR (pData, MNG_NOHEADER)
                                       /* create the chunk */
  iRetcode = init_mend (pData, &sChunkheader, &pChunk);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;

  add_chunk (pData, pChunk);           /* add it to the list */

  pData->bCreating = MNG_FALSE;        /* should be last chunk !!! */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_MEND, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_putchunk_loop (mng_handle  hHandle,
                                        mng_uint8   iLevel,
                                        mng_uint32  iRepeat,
                                        mng_uint8   iTermination,
                                        mng_uint32  iItermin,
                                        mng_uint32  iItermax,
                                        mng_uint32  iCount,
                                        mng_uint32p pSignals)
{
  mng_datap        pData;
  mng_chunkp       pChunk;
  mng_retcode      iRetcode;
  mng_chunk_header sChunkheader =
          {MNG_UINT_LOOP, init_loop, free_loop, read_loop, write_loop, 0, 0};

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_LOOP, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = (mng_datap)hHandle;          /* and make it addressable */

  if (!pData->bCreating)               /* aren't we creating a new file ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
                                       /* must have had a MHDR first! */
  if (pData->iFirstchunkadded != MNG_UINT_MHDR)
    MNG_ERROR (pData, MNG_NOHEADER)
                                       /* create the chunk */
  iRetcode = init_loop (pData, &sChunkheader, &pChunk);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;
                                       /* fill the chunk */
  ((mng_loopp)pChunk)->iLevel       = iLevel;
  ((mng_loopp)pChunk)->iRepeat      = iRepeat;
  ((mng_loopp)pChunk)->iTermination = iTermination;
  ((mng_loopp)pChunk)->iItermin     = iItermin;
  ((mng_loopp)pChunk)->iItermax     = iItermax;
  ((mng_loopp)pChunk)->iCount       = iCount;
  ((mng_loopp)pChunk)->pSignals     = pSignals;

  add_chunk (pData, pChunk);           /* add it to the list */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_LOOP, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_putchunk_endl (mng_handle hHandle,
                                        mng_uint8  iLevel)
{
  mng_datap        pData;
  mng_chunkp       pChunk;
  mng_retcode      iRetcode;
  mng_chunk_header sChunkheader =
          {MNG_UINT_ENDL, init_endl, free_endl, read_endl, write_endl, 0, 0};

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_ENDL, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = (mng_datap)hHandle;          /* and make it addressable */

  if (!pData->bCreating)               /* aren't we creating a new file ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
                                       /* must have had a MHDR first! */
  if (pData->iFirstchunkadded != MNG_UINT_MHDR)
    MNG_ERROR (pData, MNG_NOHEADER)
                                       /* create the chunk */
  iRetcode = init_endl (pData, &sChunkheader, &pChunk);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;
                                       /* fill the chunk */
  ((mng_endlp)pChunk)->iLevel = iLevel;

  add_chunk (pData, pChunk);           /* add it to the list */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_ENDL, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_putchunk_defi (mng_handle hHandle,
                                        mng_uint16 iObjectid,
                                        mng_uint8  iDonotshow,
                                        mng_uint8  iConcrete,
                                        mng_bool   bHasloca,
                                        mng_int32  iXlocation,
                                        mng_int32  iYlocation,
                                        mng_bool   bHasclip,
                                        mng_int32  iLeftcb,
                                        mng_int32  iRightcb,
                                        mng_int32  iTopcb,
                                        mng_int32  iBottomcb)
{
  mng_datap        pData;
  mng_chunkp       pChunk;
  mng_retcode      iRetcode;
  mng_chunk_header sChunkheader =
          {MNG_UINT_DEFI, init_defi, free_defi, read_defi, write_defi, 0, 0};

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_DEFI, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = (mng_datap)hHandle;          /* and make it addressable */

  if (!pData->bCreating)               /* aren't we creating a new file ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
                                       /* must have had a MHDR first! */
  if (pData->iFirstchunkadded != MNG_UINT_MHDR)
    MNG_ERROR (pData, MNG_NOHEADER)
                                       /* create the chunk */
  iRetcode = init_defi (pData, &sChunkheader, &pChunk);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;
                                       /* fill the chunk */
  ((mng_defip)pChunk)->iObjectid  = iObjectid;
  ((mng_defip)pChunk)->iDonotshow = iDonotshow;
  ((mng_defip)pChunk)->iConcrete  = iConcrete;
  ((mng_defip)pChunk)->bHasloca   = bHasloca;
  ((mng_defip)pChunk)->iXlocation = iXlocation;
  ((mng_defip)pChunk)->iYlocation = iYlocation;
  ((mng_defip)pChunk)->bHasclip   = bHasclip;
  ((mng_defip)pChunk)->iLeftcb    = iLeftcb;
  ((mng_defip)pChunk)->iRightcb   = iRightcb;
  ((mng_defip)pChunk)->iTopcb     = iTopcb;
  ((mng_defip)pChunk)->iBottomcb  = iBottomcb;

  add_chunk (pData, pChunk);           /* add it to the list */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_DEFI, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_putchunk_basi (mng_handle hHandle,
                                        mng_uint32 iWidth,
                                        mng_uint32 iHeight,
                                        mng_uint8  iBitdepth,
                                        mng_uint8  iColortype,
                                        mng_uint8  iCompression,
                                        mng_uint8  iFilter,
                                        mng_uint8  iInterlace,
                                        mng_uint16 iRed,
                                        mng_uint16 iGreen,
                                        mng_uint16 iBlue,
                                        mng_uint16 iAlpha,
                                        mng_uint8  iViewable)
{
  mng_datap        pData;
  mng_chunkp       pChunk;
  mng_retcode      iRetcode;
  mng_chunk_header sChunkheader =
          {MNG_UINT_BASI, init_basi, free_basi, read_basi, write_basi, 0, 0};

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_BASI, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = (mng_datap)hHandle;          /* and make it addressable */

  if (!pData->bCreating)               /* aren't we creating a new file ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
                                       /* must have had a MHDR first! */
  if (pData->iFirstchunkadded != MNG_UINT_MHDR)
    MNG_ERROR (pData, MNG_NOHEADER)
                                       /* create the chunk */
  iRetcode = init_basi (pData, &sChunkheader, &pChunk);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;
                                       /* fill the chunk */
  ((mng_basip)pChunk)->iWidth       = iWidth;
  ((mng_basip)pChunk)->iHeight      = iHeight;
  ((mng_basip)pChunk)->iBitdepth    = iBitdepth;
  ((mng_basip)pChunk)->iColortype   = iColortype;
  ((mng_basip)pChunk)->iCompression = iCompression;
  ((mng_basip)pChunk)->iFilter      = iFilter;
  ((mng_basip)pChunk)->iInterlace   = iInterlace;
  ((mng_basip)pChunk)->iRed         = iRed;
  ((mng_basip)pChunk)->iGreen       = iGreen;
  ((mng_basip)pChunk)->iBlue        = iBlue;
  ((mng_basip)pChunk)->iAlpha       = iAlpha;
  ((mng_basip)pChunk)->iViewable    = iViewable;

  add_chunk (pData, pChunk);           /* add it to the list */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_BASI, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_putchunk_clon (mng_handle hHandle,
                                        mng_uint16 iSourceid,
                                        mng_uint16 iCloneid,
                                        mng_uint8  iClonetype,
                                        mng_uint8  iDonotshow,
                                        mng_uint8  iConcrete,
                                        mng_bool   bHasloca,
                                        mng_uint8  iLocationtype,
                                        mng_int32  iLocationx,
                                        mng_int32  iLocationy)
{
  mng_datap        pData;
  mng_chunkp       pChunk;
  mng_retcode      iRetcode;
  mng_chunk_header sChunkheader =
          {MNG_UINT_CLON, init_clon, free_clon, read_clon, write_clon, 0, 0};

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_CLON, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = (mng_datap)hHandle;          /* and make it addressable */

  if (!pData->bCreating)               /* aren't we creating a new file ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
                                       /* must have had a MHDR first! */
  if (pData->iFirstchunkadded != MNG_UINT_MHDR)
    MNG_ERROR (pData, MNG_NOHEADER)
                                       /* create the chunk */
  iRetcode = init_clon (pData, &sChunkheader, &pChunk);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;
                                       /* fill the chunk */
  ((mng_clonp)pChunk)->iSourceid     = iSourceid;
  ((mng_clonp)pChunk)->iCloneid      = iCloneid;
  ((mng_clonp)pChunk)->iClonetype    = iClonetype;
  ((mng_clonp)pChunk)->iDonotshow    = iDonotshow;
  ((mng_clonp)pChunk)->iConcrete     = iConcrete;
  ((mng_clonp)pChunk)->bHasloca      = bHasloca;
  ((mng_clonp)pChunk)->iLocationtype = iLocationtype;
  ((mng_clonp)pChunk)->iLocationx    = iLocationx;
  ((mng_clonp)pChunk)->iLocationy    = iLocationy;

  add_chunk (pData, pChunk);           /* add it to the list */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_CLON, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_putchunk_past (mng_handle hHandle,
                                        mng_uint16 iDestid,
                                        mng_uint8  iTargettype,
                                        mng_int32  iTargetx,
                                        mng_int32  iTargety,
                                        mng_uint32 iCount)
{
  mng_datap        pData;
  mng_chunkp       pChunk;
  mng_retcode      iRetcode;
  mng_chunk_header sChunkheader =
          {MNG_UINT_PAST, init_past, free_past, read_past, write_past, 0, 0};

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_PAST, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = (mng_datap)hHandle;          /* and make it addressable */

  if (!pData->bCreating)               /* aren't we creating a new file ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
                                       /* must have had a MHDR first! */
  if (pData->iFirstchunkadded != MNG_UINT_MHDR)
    MNG_ERROR (pData, MNG_NOHEADER)
                                       /* create the chunk */
  iRetcode = init_past (pData, &sChunkheader, &pChunk);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;
                                       /* fill the chunk */
  ((mng_pastp)pChunk)->iDestid     = iDestid;
  ((mng_pastp)pChunk)->iTargettype = iTargettype;
  ((mng_pastp)pChunk)->iTargetx    = iTargetx;
  ((mng_pastp)pChunk)->iTargety    = iTargety;
  ((mng_pastp)pChunk)->iCount      = iCount;

  if (iCount)
    MNG_ALLOC (pData, ((mng_pastp)pChunk)->pSources, iCount * sizeof (mng_past_source))

  add_chunk (pData, pChunk);           /* add it to the list */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_PAST, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_putchunk_past_src (mng_handle hHandle,
                                            mng_uint32 iEntry,
                                            mng_uint16 iSourceid,
                                            mng_uint8  iComposition,
                                            mng_uint8  iOrientation,
                                            mng_uint8  iOffsettype,
                                            mng_int32  iOffsetx,
                                            mng_int32  iOffsety,
                                            mng_uint8  iBoundarytype,
                                            mng_int32  iBoundaryl,
                                            mng_int32  iBoundaryr,
                                            mng_int32  iBoundaryt,
                                            mng_int32  iBoundaryb)
{
  mng_datap        pData;
  mng_chunkp       pChunk;
  mng_past_sourcep pEntry;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_PAST_SRC, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = (mng_datap)hHandle;          /* and make it addressable */

  if (!pData->bCreating)               /* aren't we creating a new file ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
                                       /* must have had a MHDR first! */
  if (pData->iFirstchunkadded != MNG_UINT_MHDR)
    MNG_ERROR (pData, MNG_NOHEADER)

  pChunk = pData->pLastchunk;          /* last one must have been PAST ! */

  if (((mng_chunk_headerp)pChunk)->iChunkname != MNG_UINT_PAST)
    MNG_ERROR (pData, MNG_NOCORRCHUNK)
                                       /* index out of bounds ? */
  if (iEntry >= ((mng_pastp)pChunk)->iCount)
    MNG_ERROR (pData, MNG_INVALIDENTRYIX)
                                       /* address proper entry */
  pEntry = ((mng_pastp)pChunk)->pSources + iEntry;

  pEntry->iSourceid     = iSourceid;   /* fill entry */
  pEntry->iComposition  = iComposition;
  pEntry->iOrientation  = iOrientation;
  pEntry->iOffsettype   = iOffsettype;
  pEntry->iOffsetx      = iOffsetx;
  pEntry->iOffsety      = iOffsety;
  pEntry->iBoundarytype = iBoundarytype;
  pEntry->iBoundaryl    = iBoundaryl;
  pEntry->iBoundaryr    = iBoundaryr;
  pEntry->iBoundaryt    = iBoundaryt;
  pEntry->iBoundaryb    = iBoundaryb;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_PAST_SRC, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_putchunk_disc (mng_handle  hHandle,
                                        mng_uint32  iCount,
                                        mng_uint16p pObjectids)
{
  mng_datap        pData;
  mng_chunkp       pChunk;
  mng_retcode      iRetcode;
  mng_chunk_header sChunkheader =
          {MNG_UINT_DISC, init_disc, free_disc, read_disc, write_disc, 0, 0};

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_DISC, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = (mng_datap)hHandle;          /* and make it addressable */

  if (!pData->bCreating)               /* aren't we creating a new file ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
                                       /* must have had a MHDR first! */
  if (pData->iFirstchunkadded != MNG_UINT_MHDR)
    MNG_ERROR (pData, MNG_NOHEADER)
                                       /* create the chunk */
  iRetcode = init_disc (pData, &sChunkheader, &pChunk);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;
                                       /* fill the chunk */
  ((mng_discp)pChunk)->iCount = iCount;

  if (iCount)
  {
    mng_uint32 iSize = iCount * sizeof (mng_uint32);

    MNG_ALLOC (pData, ((mng_discp)pChunk)->pObjectids, iSize);
    MNG_COPY (((mng_discp)pChunk)->pObjectids, pObjectids, iSize);
  }

  add_chunk (pData, pChunk);           /* add it to the list */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_DISC, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_putchunk_back (mng_handle hHandle,
                                        mng_uint16 iRed,
                                        mng_uint16 iGreen,
                                        mng_uint16 iBlue,
                                        mng_uint8  iMandatory,
                                        mng_uint16 iImageid,
                                        mng_uint8  iTile)
{
  mng_datap        pData;
  mng_chunkp       pChunk;
  mng_retcode      iRetcode;
  mng_chunk_header sChunkheader =
          {MNG_UINT_BACK, init_back, free_back, read_back, write_back, 0, 0};

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_BACK, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = (mng_datap)hHandle;          /* and make it addressable */

  if (!pData->bCreating)               /* aren't we creating a new file ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
                                       /* must have had a MHDR first! */
  if (pData->iFirstchunkadded != MNG_UINT_MHDR)
    MNG_ERROR (pData, MNG_NOHEADER)
                                       /* create the chunk */
  iRetcode = init_back (pData, &sChunkheader, &pChunk);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;
                                       /* fill the chunk */
  ((mng_backp)pChunk)->iRed       = iRed;
  ((mng_backp)pChunk)->iGreen     = iGreen;
  ((mng_backp)pChunk)->iBlue      = iBlue;
  ((mng_backp)pChunk)->iMandatory = iMandatory;
  ((mng_backp)pChunk)->iImageid   = iImageid;
  ((mng_backp)pChunk)->iTile      = iTile;

  add_chunk (pData, pChunk);           /* add it to the list */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_BACK, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_putchunk_fram (mng_handle  hHandle,
                                        mng_bool    bEmpty,
                                        mng_uint8   iMode,
                                        mng_uint32  iNamesize,
                                        mng_pchar   zName,
                                        mng_uint8   iChangedelay,
                                        mng_uint8   iChangetimeout,
                                        mng_uint8   iChangeclipping,
                                        mng_uint8   iChangesyncid,
                                        mng_uint32  iDelay,
                                        mng_uint32  iTimeout,
                                        mng_uint8   iBoundarytype,
                                        mng_int32   iBoundaryl,
                                        mng_int32   iBoundaryr,
                                        mng_int32   iBoundaryt,
                                        mng_int32   iBoundaryb,
                                        mng_uint32  iCount,
                                        mng_uint32p pSyncids)
{
  mng_datap        pData;
  mng_chunkp       pChunk;
  mng_retcode      iRetcode;
  mng_chunk_header sChunkheader =
          {MNG_UINT_FRAM, init_fram, free_fram, read_fram, write_fram, 0, 0};

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_FRAM, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = (mng_datap)hHandle;          /* and make it addressable */

  if (!pData->bCreating)               /* aren't we creating a new file ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
                                       /* must have had a MHDR first! */
  if (pData->iFirstchunkadded != MNG_UINT_MHDR)
    MNG_ERROR (pData, MNG_NOHEADER)
                                       /* create the chunk */
  iRetcode = init_fram (pData, &sChunkheader, &pChunk);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;
                                       /* fill the chunk */
  ((mng_framp)pChunk)->bEmpty          = bEmpty;
  ((mng_framp)pChunk)->iMode           = iMode;
  ((mng_framp)pChunk)->iNamesize       = iNamesize;
  ((mng_framp)pChunk)->iChangedelay    = iChangedelay;
  ((mng_framp)pChunk)->iChangetimeout  = iChangetimeout;
  ((mng_framp)pChunk)->iChangeclipping = iChangeclipping;
  ((mng_framp)pChunk)->iChangesyncid   = iChangesyncid;
  ((mng_framp)pChunk)->iDelay          = iDelay;
  ((mng_framp)pChunk)->iTimeout        = iTimeout;
  ((mng_framp)pChunk)->iBoundarytype   = iBoundarytype;
  ((mng_framp)pChunk)->iBoundaryl      = iBoundaryl;
  ((mng_framp)pChunk)->iBoundaryr      = iBoundaryr;
  ((mng_framp)pChunk)->iBoundaryt      = iBoundaryt;
  ((mng_framp)pChunk)->iBoundaryb      = iBoundaryb;
  ((mng_framp)pChunk)->iCount          = iCount;

  if (iNamesize)
  {
    MNG_ALLOC (pData, ((mng_framp)pChunk)->zName, iNamesize + 1)
    MNG_COPY (((mng_framp)pChunk)->zName, zName, iNamesize)
  }

  if (iCount)
  {
    mng_uint32 iSize = iCount * sizeof (mng_uint32);

    MNG_ALLOC (pData, ((mng_framp)pChunk)->pSyncids, iSize)
    MNG_COPY (((mng_framp)pChunk)->pSyncids, pSyncids, iSize)
  }

  add_chunk (pData, pChunk);           /* add it to the list */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_FRAM, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_putchunk_move (mng_handle hHandle,
                                        mng_uint16 iFirstid,
                                        mng_uint16 iLastid,
                                        mng_uint8  iMovetype,
                                        mng_int32  iMovex,
                                        mng_int32  iMovey)
{
  mng_datap        pData;
  mng_chunkp       pChunk;
  mng_retcode      iRetcode;
  mng_chunk_header sChunkheader =
          {MNG_UINT_MOVE, init_move, free_move, read_move, write_move, 0, 0};

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_MOVE, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = (mng_datap)hHandle;          /* and make it addressable */

  if (!pData->bCreating)               /* aren't we creating a new file ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
                                       /* must have had a MHDR first! */
  if (pData->iFirstchunkadded != MNG_UINT_MHDR)
    MNG_ERROR (pData, MNG_NOHEADER)
                                       /* create the chunk */
  iRetcode = init_move (pData, &sChunkheader, &pChunk);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;
                                       /* fill the chunk */
  ((mng_movep)pChunk)->iFirstid  = iFirstid;
  ((mng_movep)pChunk)->iLastid   = iLastid;
  ((mng_movep)pChunk)->iMovetype = iMovetype;
  ((mng_movep)pChunk)->iMovex    = iMovex;
  ((mng_movep)pChunk)->iMovey    = iMovey;

  add_chunk (pData, pChunk);           /* add it to the list */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_MOVE, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_putchunk_clip (mng_handle hHandle,
                                        mng_uint16 iFirstid,
                                        mng_uint16 iLastid,
                                        mng_uint8  iCliptype,
                                        mng_int32  iClipl,
                                        mng_int32  iClipr,
                                        mng_int32  iClipt,
                                        mng_int32  iClipb)
{
  mng_datap        pData;
  mng_chunkp       pChunk;
  mng_retcode      iRetcode;
  mng_chunk_header sChunkheader =
          {MNG_UINT_CLIP, init_clip, free_clip, read_clip, write_clip, 0, 0};

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_CLIP, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = (mng_datap)hHandle;          /* and make it addressable */

  if (!pData->bCreating)               /* aren't we creating a new file ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
                                       /* must have had a MHDR first! */
  if (pData->iFirstchunkadded != MNG_UINT_MHDR)
    MNG_ERROR (pData, MNG_NOHEADER)
                                       /* create the chunk */
  iRetcode = init_clip (pData, &sChunkheader, &pChunk);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;
                                       /* fill the chunk */
  ((mng_clipp)pChunk)->iFirstid  = iFirstid;
  ((mng_clipp)pChunk)->iLastid   = iLastid;
  ((mng_clipp)pChunk)->iCliptype = iCliptype;
  ((mng_clipp)pChunk)->iClipl    = iClipl;
  ((mng_clipp)pChunk)->iClipr    = iClipr;
  ((mng_clipp)pChunk)->iClipt    = iClipt;
  ((mng_clipp)pChunk)->iClipb    = iClipb;

  add_chunk (pData, pChunk);           /* add it to the list */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_CLIP, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_putchunk_show (mng_handle hHandle,
                                        mng_bool   bEmpty,
                                        mng_uint16 iFirstid,
                                        mng_uint16 iLastid,
                                        mng_uint8  iMode)
{
  mng_datap        pData;
  mng_chunkp       pChunk;
  mng_retcode      iRetcode;
  mng_chunk_header sChunkheader =
          {MNG_UINT_SHOW, init_show, free_show, read_show, write_show, 0, 0};

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_SHOW, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = (mng_datap)hHandle;          /* and make it addressable */

  if (!pData->bCreating)               /* aren't we creating a new file ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
                                       /* must have had a MHDR first! */
  if (pData->iFirstchunkadded != MNG_UINT_MHDR)
    MNG_ERROR (pData, MNG_NOHEADER)
                                       /* create the chunk */
  iRetcode = init_show (pData, &sChunkheader, &pChunk);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;
                                       /* fill the chunk */
  ((mng_showp)pChunk)->bEmpty   = bEmpty;
  ((mng_showp)pChunk)->iFirstid = iFirstid;
  ((mng_showp)pChunk)->iLastid  = iLastid;
  ((mng_showp)pChunk)->iMode    = iMode;

  add_chunk (pData, pChunk);           /* add it to the list */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_SHOW, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_putchunk_term (mng_handle hHandle,
                                        mng_uint8  iTermaction,
                                        mng_uint8  iIteraction,
                                        mng_uint32 iDelay,
                                        mng_uint32 iItermax)
{
  mng_datap        pData;
  mng_chunkp       pChunk;
  mng_retcode      iRetcode;
  mng_chunk_header sChunkheader =
          {MNG_UINT_TERM, init_term, free_term, read_term, write_term, 0, 0};

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_TERM, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = (mng_datap)hHandle;          /* and make it addressable */

  if (!pData->bCreating)               /* aren't we creating a new file ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
                                       /* must have had a MHDR first! */
  if (pData->iFirstchunkadded != MNG_UINT_MHDR)
    MNG_ERROR (pData, MNG_NOHEADER)
                                       /* create the chunk */
  iRetcode = init_term (pData, &sChunkheader, &pChunk);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;
                                       /* fill the chunk */
  ((mng_termp)pChunk)->iTermaction = iTermaction;
  ((mng_termp)pChunk)->iIteraction = iIteraction;
  ((mng_termp)pChunk)->iDelay      = iDelay;
  ((mng_termp)pChunk)->iItermax    = iItermax;

  add_chunk (pData, pChunk);           /* add it to the list */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_TERM, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_putchunk_save (mng_handle hHandle,
                                        mng_bool   bEmpty,
                                        mng_uint8  iOffsettype,
                                        mng_uint32 iCount)
{
  mng_datap        pData;
  mng_chunkp       pChunk;
  mng_retcode      iRetcode;
  mng_chunk_header sChunkheader =
          {MNG_UINT_SAVE, init_save, free_save, read_save, write_save, 0, 0};

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_SAVE, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = (mng_datap)hHandle;          /* and make it addressable */

  if (!pData->bCreating)               /* aren't we creating a new file ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
                                       /* must have had a MHDR first! */
  if (pData->iFirstchunkadded != MNG_UINT_MHDR)
    MNG_ERROR (pData, MNG_NOHEADER)
                                       /* create the chunk */
  iRetcode = init_save (pData, &sChunkheader, &pChunk);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;
                                       /* fill the chunk */
  ((mng_savep)pChunk)->bEmpty      = bEmpty;
  ((mng_savep)pChunk)->iOffsettype = iOffsettype;
  ((mng_savep)pChunk)->iCount      = iCount;

  if (iCount)
    MNG_ALLOC (pData, ((mng_savep)pChunk)->pEntries, iCount * sizeof (mng_save_entry))

  add_chunk (pData, pChunk);           /* add it to the list */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_SAVE, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_putchunk_save_entry (mng_handle     hHandle,
                                              mng_uint32     iEntry,
                                              mng_uint8      iEntrytype,
                                              mng_uint32arr2 iOffset,
                                              mng_uint32arr2 iStarttime,
                                              mng_uint32     iLayernr,
                                              mng_uint32     iFramenr,
                                              mng_uint32     iNamesize,
                                              mng_pchar      zName)
{
  mng_datap       pData;
  mng_chunkp      pChunk;
  mng_save_entryp pEntry;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_SAVE_ENTRY, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = (mng_datap)hHandle;          /* and make it addressable */

  if (!pData->bCreating)               /* aren't we creating a new file ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
                                       /* must have had a MHDR first! */
  if (pData->iFirstchunkadded != MNG_UINT_MHDR)
    MNG_ERROR (pData, MNG_NOHEADER)

  pChunk = pData->pLastchunk;          /* last one must have been SAVE ! */

  if (((mng_chunk_headerp)pChunk)->iChunkname != MNG_UINT_SAVE)
    MNG_ERROR (pData, MNG_NOCORRCHUNK)
                                       /* index out of bounds ? */
  if (iEntry >= ((mng_savep)pChunk)->iCount)
    MNG_ERROR (pData, MNG_INVALIDENTRYIX)
                                       /* address proper entry */
  pEntry = ((mng_savep)pChunk)->pEntries + iEntry;

  pEntry->iEntrytype    = iEntrytype;  /* fill entry */
  pEntry->iOffset[0]    = iOffset[0];
  pEntry->iOffset[1]    = iOffset[1];
  pEntry->iStarttime[0] = iStarttime[0];
  pEntry->iStarttime[1] = iStarttime[1];
  pEntry->iLayernr      = iLayernr;
  pEntry->iFramenr      = iFramenr;
  pEntry->iNamesize     = iNamesize;

  if (iNamesize)
  {
    MNG_ALLOC (pData, pEntry->zName, iNamesize + 1)
    MNG_COPY (pEntry->zName, zName, iNamesize)
  }  

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_SAVE_ENTRY, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_putchunk_seek (mng_handle hHandle,
                                        mng_uint32 iNamesize,
                                        mng_pchar  zName)
{
  mng_datap        pData;
  mng_chunkp       pChunk;
  mng_retcode      iRetcode;
  mng_chunk_header sChunkheader =
          {MNG_UINT_SEEK, init_seek, free_seek, read_seek, write_seek, 0, 0};

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_SEEK, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = (mng_datap)hHandle;          /* and make it addressable */

  if (!pData->bCreating)               /* aren't we creating a new file ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
                                       /* must have had a MHDR first! */
  if (pData->iFirstchunkadded != MNG_UINT_MHDR)
    MNG_ERROR (pData, MNG_NOHEADER)
                                       /* create the chunk */
  iRetcode = init_seek (pData, &sChunkheader, &pChunk);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;
                                       /* fill the chunk */
  ((mng_seekp)pChunk)->iNamesize = iNamesize;

  if (iNamesize)
  {
    MNG_ALLOC (pData, ((mng_seekp)pChunk)->zName, iNamesize + 1)
    MNG_COPY (((mng_seekp)pChunk)->zName, zName, iNamesize)
  }

  add_chunk (pData, pChunk);           /* add it to the list */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_SEEK, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_putchunk_expi (mng_handle hHandle,
                                        mng_uint16 iSnapshotid,
                                        mng_uint32 iNamesize,
                                        mng_pchar  zName)
{
  mng_datap        pData;
  mng_chunkp       pChunk;
  mng_retcode      iRetcode;
  mng_chunk_header sChunkheader =
          {MNG_UINT_eXPI, init_expi, free_expi, read_expi, write_expi, 0, 0};

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_EXPI, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = (mng_datap)hHandle;          /* and make it addressable */

  if (!pData->bCreating)               /* aren't we creating a new file ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
                                       /* must have had a MHDR first! */
  if (pData->iFirstchunkadded != MNG_UINT_MHDR)
    MNG_ERROR (pData, MNG_NOHEADER)
                                       /* create the chunk */
  iRetcode = init_expi (pData, &sChunkheader, &pChunk);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;
                                       /* fill the chunk */
  ((mng_expip)pChunk)->iSnapshotid = iSnapshotid;
  ((mng_expip)pChunk)->iNamesize   = iNamesize;

  if (iNamesize)
  {
    MNG_ALLOC (pData, ((mng_expip)pChunk)->zName, iNamesize + 1)
    MNG_COPY (((mng_expip)pChunk)->zName, zName, iNamesize)
  }  

  add_chunk (pData, pChunk);           /* add it to the list */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_EXPI, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_putchunk_fpri (mng_handle hHandle,
                                        mng_uint8  iDeltatype,
                                        mng_uint8  iPriority)
{
  mng_datap        pData;
  mng_chunkp       pChunk;
  mng_retcode      iRetcode;
  mng_chunk_header sChunkheader =
          {MNG_UINT_fPRI, init_fpri, free_fpri, read_fpri, write_fpri, 0, 0};

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_FPRI, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = (mng_datap)hHandle;          /* and make it addressable */

  if (!pData->bCreating)               /* aren't we creating a new file ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
                                       /* must have had a MHDR first! */
  if (pData->iFirstchunkadded != MNG_UINT_MHDR)
    MNG_ERROR (pData, MNG_NOHEADER)
                                       /* create the chunk */
  iRetcode = init_fpri (pData, &sChunkheader, &pChunk);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;
                                       /* fill the chunk */
  ((mng_fprip)pChunk)->iDeltatype = iDeltatype;
  ((mng_fprip)pChunk)->iPriority  = iPriority;

  add_chunk (pData, pChunk);           /* add it to the list */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_FPRI, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_putchunk_need (mng_handle hHandle,
                                        mng_uint32 iKeywordssize,
                                        mng_pchar  zKeywords)
{
  mng_datap        pData;
  mng_chunkp       pChunk;
  mng_retcode      iRetcode;
  mng_chunk_header sChunkheader =
          {MNG_UINT_nEED, init_need, free_need, read_need, write_need, 0, 0};

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_NEED, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = (mng_datap)hHandle;          /* and make it addressable */

  if (!pData->bCreating)               /* aren't we creating a new file ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
                                       /* must have had a MHDR first! */
  if (pData->iFirstchunkadded != MNG_UINT_MHDR)
    MNG_ERROR (pData, MNG_NOHEADER)
                                       /* create the chunk */
  iRetcode = init_need (pData, &sChunkheader, &pChunk);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;
                                       /* fill the chunk */
  ((mng_needp)pChunk)->iKeywordssize = iKeywordssize;

  if (iKeywordssize)
  {
    MNG_ALLOC (pData, ((mng_needp)pChunk)->zKeywords, iKeywordssize + 1)
    MNG_COPY (((mng_needp)pChunk)->zKeywords, zKeywords, iKeywordssize)
  }

  add_chunk (pData, pChunk);           /* add it to the list */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_NEED, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_putchunk_phyg (mng_handle hHandle,
                                        mng_bool   bEmpty,
                                        mng_uint32 iSizex,
                                        mng_uint32 iSizey,
                                        mng_uint8  iUnit)
{
  mng_datap        pData;
  mng_chunkp       pChunk;
  mng_retcode      iRetcode;
  mng_chunk_header sChunkheader =
          {MNG_UINT_pHYg, init_phyg, free_phyg, read_phyg, write_phyg, 0, 0};

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_PHYG, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = (mng_datap)hHandle;          /* and make it addressable */

  if (!pData->bCreating)               /* aren't we creating a new file ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
                                       /* must have had a MHDR first! */
  if (pData->iFirstchunkadded != MNG_UINT_MHDR)
    MNG_ERROR (pData, MNG_NOHEADER)
                                       /* create the chunk */
  iRetcode = init_phyg (pData, &sChunkheader, &pChunk);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;
                                       /* fill the chunk */
  ((mng_phygp)pChunk)->bEmpty = bEmpty;
  ((mng_phygp)pChunk)->iSizex = iSizex;
  ((mng_phygp)pChunk)->iSizey = iSizey;
  ((mng_phygp)pChunk)->iUnit  = iUnit;

  add_chunk (pData, pChunk);           /* add it to the list */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_PHYG, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */
/* B004 */
#ifdef MNG_INCLUDE_JNG
/* B004 */
mng_retcode MNG_DECL mng_putchunk_jhdr (mng_handle hHandle,
                                        mng_uint32 iWidth,
                                        mng_uint32 iHeight,
                                        mng_uint8  iColortype,
                                        mng_uint8  iImagesampledepth,
                                        mng_uint8  iImagecompression,
                                        mng_uint8  iImageinterlace,
                                        mng_uint8  iAlphasampledepth,
                                        mng_uint8  iAlphacompression,
                                        mng_uint8  iAlphafilter,
                                        mng_uint8  iAlphainterlace)
{
  mng_datap        pData;
  mng_chunkp       pChunk;
  mng_retcode      iRetcode;
  mng_chunk_header sChunkheader =
          {MNG_UINT_JHDR, init_jhdr, free_jhdr, read_jhdr, write_jhdr, 0, 0};

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_JHDR, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = (mng_datap)hHandle;          /* and make it addressable */

  if (!pData->bCreating)               /* aren't we creating a new file ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
                                       /* create the chunk */
  iRetcode = init_jhdr (pData, &sChunkheader, &pChunk);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;
                                       /* fill the chunk */
  ((mng_jhdrp)pChunk)->iWidth            = iWidth;
  ((mng_jhdrp)pChunk)->iHeight           = iHeight;
  ((mng_jhdrp)pChunk)->iColortype        = iColortype;
  ((mng_jhdrp)pChunk)->iImagesampledepth = iImagesampledepth;
  ((mng_jhdrp)pChunk)->iImagecompression = iImagecompression;
  ((mng_jhdrp)pChunk)->iImageinterlace   = iImageinterlace;
  ((mng_jhdrp)pChunk)->iAlphasampledepth = iAlphasampledepth;
  ((mng_jhdrp)pChunk)->iAlphacompression = iAlphacompression;
  ((mng_jhdrp)pChunk)->iAlphafilter      = iAlphafilter;
  ((mng_jhdrp)pChunk)->iAlphainterlace   = iAlphainterlace;

  add_chunk (pData, pChunk);           /* add it to the list */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_JHDR, MNG_LC_END)
#endif

  return MNG_NOERROR;
}
/* B004 */
#endif /* MNG_INCLUDE_JNG */
/* B004 */
/* ************************************************************************** */
/* B004 */
#ifdef MNG_INCLUDE_JNG
/* B004 */
mng_retcode MNG_DECL mng_putchunk_jdat (mng_handle hHandle,
                                        mng_uint32 iRawlen,
                                        mng_ptr    pRawdata)
{
  mng_datap        pData;
  mng_chunkp       pChunk;
  mng_retcode      iRetcode;
  mng_chunk_header sChunkheader =
          {MNG_UINT_JDAT, init_jdat, free_jdat, read_jdat, write_jdat, 0, 0};

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_JDAT, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = (mng_datap)hHandle;          /* and make it addressable */

  if (!pData->bCreating)               /* aren't we creating a new file ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
                                       /* must have had a MHDR or JHDR first! */
  if ((pData->iFirstchunkadded != MNG_UINT_MHDR) &&
      (pData->iFirstchunkadded != MNG_UINT_JHDR)    )
    MNG_ERROR (pData, MNG_NOHEADER)
                                       /* create the chunk */
  iRetcode = init_jdat (pData, &sChunkheader, &pChunk);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;
                                       /* fill the chunk */
  ((mng_jdatp)pChunk)->iDatasize = iRawlen;

  if (iRawlen)
  {
    MNG_ALLOC (pData, ((mng_jdatp)pChunk)->pData, iRawlen)
    MNG_COPY (((mng_jdatp)pChunk)->pData, pRawdata, iRawlen)
  }

  add_chunk (pData, pChunk);           /* add it to the list */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_JDAT, MNG_LC_END)
#endif

  return MNG_NOERROR;
}
/* B004 */
#endif /*  MNG_INCLUDE_JNG */
/* B004 */
/* ************************************************************************** */
/* B004 */
#ifdef MNG_INCLUDE_JNG
/* B004 */
mng_retcode MNG_DECL mng_putchunk_jsep (mng_handle hHandle)
{
  mng_datap        pData;
  mng_chunkp       pChunk;
  mng_retcode      iRetcode;
  mng_chunk_header sChunkheader =
          {MNG_UINT_JSEP, init_jsep, free_jsep, read_jsep, write_jsep, 0, 0};

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_JSEP, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = (mng_datap)hHandle;          /* and make it addressable */

  if (!pData->bCreating)               /* aren't we creating a new file ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
                                       /* must have had a MHDR or JHDR first! */
  if ((pData->iFirstchunkadded != MNG_UINT_MHDR) &&
      (pData->iFirstchunkadded != MNG_UINT_JHDR)    )
    MNG_ERROR (pData, MNG_NOHEADER)
                                       /* create the chunk */
  iRetcode = init_jsep (pData, &sChunkheader, &pChunk);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;

  add_chunk (pData, pChunk);           /* add it to the list */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_JSEP, MNG_LC_END)
#endif

  return MNG_NOERROR;
}
/* B004 */
#endif /* MNG_INCLUDE_JNG */
/* B004 */
/* ************************************************************************** */

mng_retcode MNG_DECL mng_putchunk_dhdr (mng_handle hHandle,
                                        mng_uint16 iObjectid,
                                        mng_uint8  iImagetype,
                                        mng_uint8  iDeltatype,
                                        mng_uint32 iBlockwidth,
                                        mng_uint32 iBlockheight,
                                        mng_uint32 iBlockx,
                                        mng_uint32 iBlocky)
{
  mng_datap        pData;
  mng_chunkp       pChunk;
  mng_retcode      iRetcode;
  mng_chunk_header sChunkheader =
          {MNG_UINT_DHDR, init_dhdr, free_dhdr, read_dhdr, write_dhdr, 0, 0};

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_DHDR, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = (mng_datap)hHandle;          /* and make it addressable */

  if (!pData->bCreating)               /* aren't we creating a new file ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
                                       /* must have had a MHDR first! */
  if (pData->iFirstchunkadded != MNG_UINT_MHDR)
    MNG_ERROR (pData, MNG_NOHEADER)
                                       /* create the chunk */
  iRetcode = init_dhdr (pData, &sChunkheader, &pChunk);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;
                                       /* fill the chunk */
  ((mng_dhdrp)pChunk)->iObjectid    = iObjectid;
  ((mng_dhdrp)pChunk)->iImagetype   = iImagetype;
  ((mng_dhdrp)pChunk)->iDeltatype   = iDeltatype;
  ((mng_dhdrp)pChunk)->iBlockwidth  = iBlockwidth;
  ((mng_dhdrp)pChunk)->iBlockheight = iBlockheight;
  ((mng_dhdrp)pChunk)->iBlockx      = iBlockx;
  ((mng_dhdrp)pChunk)->iBlocky      = iBlocky;

  add_chunk (pData, pChunk);           /* add it to the list */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_DHDR, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_putchunk_prom (mng_handle hHandle,
                                        mng_uint8  iColortype,
                                        mng_uint8  iSampledepth,
                                        mng_uint8  iFilltype)
{
  mng_datap        pData;
  mng_chunkp       pChunk;
  mng_retcode      iRetcode;
  mng_chunk_header sChunkheader =
          {MNG_UINT_PROM, init_prom, free_prom, read_prom, write_prom, 0, 0};

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_PROM, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = (mng_datap)hHandle;          /* and make it addressable */

  if (!pData->bCreating)               /* aren't we creating a new file ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
                                       /* must have had a MHDR first! */
  if (pData->iFirstchunkadded != MNG_UINT_MHDR)
    MNG_ERROR (pData, MNG_NOHEADER)
                                       /* create the chunk */
  iRetcode = init_prom (pData, &sChunkheader, &pChunk);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;
                                       /* fill the chunk */
  ((mng_promp)pChunk)->iColortype   = iColortype;
  ((mng_promp)pChunk)->iSampledepth = iSampledepth;
  ((mng_promp)pChunk)->iFilltype    = iFilltype;

  add_chunk (pData, pChunk);           /* add it to the list */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_PROM, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_putchunk_ipng (mng_handle hHandle)
{
  mng_datap        pData;
  mng_chunkp       pChunk;
  mng_retcode      iRetcode;
  mng_chunk_header sChunkheader =
          {MNG_UINT_IPNG, init_ipng, free_ipng, read_ipng, write_ipng, 0, 0};

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_IPNG, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = (mng_datap)hHandle;          /* and make it addressable */

  if (!pData->bCreating)               /* aren't we creating a new file ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
                                       /* must have had a MHDR first! */
  if (pData->iFirstchunkadded != MNG_UINT_MHDR)
    MNG_ERROR (pData, MNG_NOHEADER)
                                       /* create the chunk */
  iRetcode = init_ipng (pData, &sChunkheader, &pChunk);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;

  add_chunk (pData, pChunk);           /* add it to the list */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_IPNG, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_putchunk_pplt (mng_handle hHandle,
                                        mng_uint32 iCount)
{
  mng_datap        pData;
  mng_chunkp       pChunk;
  mng_retcode      iRetcode;
  mng_chunk_header sChunkheader =
          {MNG_UINT_PPLT, init_pplt, free_pplt, read_pplt, write_pplt, 0, 0};

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_PPLT, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = (mng_datap)hHandle;          /* and make it addressable */

  if (!pData->bCreating)               /* aren't we creating a new file ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
                                       /* must have had a MHDR first! */
  if (pData->iFirstchunkadded != MNG_UINT_MHDR)
    MNG_ERROR (pData, MNG_NOHEADER)
                                       /* create the chunk */
  iRetcode = init_pplt (pData, &sChunkheader, &pChunk);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;
                                       /* fill the chunk */
  ((mng_ppltp)pChunk)->iCount = iCount;

  add_chunk (pData, pChunk);           /* add it to the list */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_PPLT, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_putchunk_pplt_entry (mng_handle hHandle,
                                              mng_uint32 iEntry,
                                              mng_uint16 iRed,
                                              mng_uint16 iGreen,
                                              mng_uint16 iBlue,
                                              mng_uint16 iAlpha,
                                              mng_bool   bUsed)
{
  mng_datap       pData;
  mng_chunkp      pChunk;
  mng_pplt_entryp pEntry;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_PPLT_ENTRY, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = (mng_datap)hHandle;          /* and make it addressable */

  if (!pData->bCreating)               /* aren't we creating a new file ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
                                       /* must have had a MHDR first! */
  if (pData->iFirstchunkadded != MNG_UINT_MHDR)
    MNG_ERROR (pData, MNG_NOHEADER)

  pChunk = pData->pLastchunk;          /* last one must have been PPLT ! */

  if (((mng_chunk_headerp)pChunk)->iChunkname != MNG_UINT_PPLT)
    MNG_ERROR (pData, MNG_NOCORRCHUNK)

                                       /* index out of bounds ? */
  if (iEntry >= ((mng_ppltp)pChunk)->iCount)
    MNG_ERROR (pData, MNG_INVALIDENTRYIX)
                                       /* address proper entry */
  pEntry = (mng_pplt_entryp)(((mng_ppltp)pChunk)->aEntries) + iEntry;

  pEntry->iRed   = (mng_uint8)iRed;    /* fill the entry */
  pEntry->iGreen = (mng_uint8)iGreen;
  pEntry->iBlue  = (mng_uint8)iBlue;
  pEntry->iAlpha = (mng_uint8)iAlpha;
  pEntry->bUsed  = bUsed;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_PPLT_ENTRY, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_putchunk_ijng (mng_handle hHandle)
{
  mng_datap        pData;
  mng_chunkp       pChunk;
  mng_retcode      iRetcode;
  mng_chunk_header sChunkheader =
          {MNG_UINT_IJNG, init_ijng, free_ijng, read_ijng, write_ijng, 0, 0};

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_IJNG, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = (mng_datap)hHandle;          /* and make it addressable */

  if (!pData->bCreating)               /* aren't we creating a new file ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
                                       /* must have had a MHDR first! */
  if (pData->iFirstchunkadded != MNG_UINT_MHDR)
    MNG_ERROR (pData, MNG_NOHEADER)
                                       /* create the chunk */
  iRetcode = init_ijng (pData, &sChunkheader, &pChunk);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;

  add_chunk (pData, pChunk);           /* add it to the list */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_IJNG, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_putchunk_drop (mng_handle   hHandle,
                                        mng_uint32   iCount,
                                        mng_chunkidp pChunknames)
{
  mng_datap        pData;
  mng_chunkp       pChunk;
  mng_retcode      iRetcode;
  mng_chunk_header sChunkheader =
          {MNG_UINT_DROP, init_drop, free_drop, read_drop, write_drop, 0, 0};

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_DROP, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = (mng_datap)hHandle;          /* and make it addressable */

  if (!pData->bCreating)               /* aren't we creating a new file ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
                                       /* must have had a MHDR first! */
  if (pData->iFirstchunkadded != MNG_UINT_MHDR)
    MNG_ERROR (pData, MNG_NOHEADER)
                                       /* create the chunk */
  iRetcode = init_drop (pData, &sChunkheader, &pChunk);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;
                                       /* fill the chunk */
  ((mng_dropp)pChunk)->iCount = iCount;

  if (iCount)
  {
    mng_uint32 iSize = iCount * sizeof (mng_chunkid);

    MNG_ALLOC (pData, ((mng_dropp)pChunk)->pChunknames, iSize)
    MNG_COPY (((mng_dropp)pChunk)->pChunknames, pChunknames, iSize)
  }  

  add_chunk (pData, pChunk);           /* add it to the list */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_DROP, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_putchunk_dbyk (mng_handle  hHandle,
                                        mng_chunkid iChunkname,
                                        mng_uint8   iPolarity,
                                        mng_uint32  iKeywordssize,
                                        mng_pchar   zKeywords)
{
  mng_datap        pData;
  mng_chunkp       pChunk;
  mng_retcode      iRetcode;
  mng_chunk_header sChunkheader =
          {MNG_UINT_DBYK, init_dbyk, free_dbyk, read_dbyk, write_dbyk, 0, 0};

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_DBYK, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = (mng_datap)hHandle;          /* and make it addressable */

  if (!pData->bCreating)               /* aren't we creating a new file ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
                                       /* must have had a MHDR first! */
  if (pData->iFirstchunkadded != MNG_UINT_MHDR)
    MNG_ERROR (pData, MNG_NOHEADER)
                                       /* create the chunk */
  iRetcode = init_dbyk (pData, &sChunkheader, &pChunk);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;
                                       /* fill the chunk */
  ((mng_dbykp)pChunk)->iChunkname    = iChunkname;
  ((mng_dbykp)pChunk)->iPolarity     = iPolarity;
  ((mng_dbykp)pChunk)->iKeywordssize = iKeywordssize;

  if (iKeywordssize)
  {
    MNG_ALLOC (pData, ((mng_dbykp)pChunk)->zKeywords, iKeywordssize + 1)
    MNG_COPY (((mng_dbykp)pChunk)->zKeywords, zKeywords, iKeywordssize)
  }  

  add_chunk (pData, pChunk);           /* add it to the list */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_DBYK, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_putchunk_ordr (mng_handle hHandle,
                                        mng_uint32 iCount)
{
  mng_datap        pData;
  mng_chunkp       pChunk;
  mng_retcode      iRetcode;
  mng_chunk_header sChunkheader =
          {MNG_UINT_ORDR, init_ordr, free_ordr, read_ordr, write_ordr, 0, 0};

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_ORDR, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = (mng_datap)hHandle;          /* and make it addressable */

  if (!pData->bCreating)               /* aren't we creating a new file ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
                                       /* must have had a MHDR first! */
  if (pData->iFirstchunkadded != MNG_UINT_MHDR)
    MNG_ERROR (pData, MNG_NOHEADER)
                                       /* create the chunk */
  iRetcode = init_ordr (pData, &sChunkheader, &pChunk);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;
                                       /* fill the chunk */
  ((mng_ordrp)pChunk)->iCount = iCount;

  if (iCount)
    MNG_ALLOC (pData, ((mng_ordrp)pChunk)->pEntries, iCount * sizeof (mng_ordr_entry))

  add_chunk (pData, pChunk);           /* add it to the list */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_ORDR, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_putchunk_ordr_entry (mng_handle  hHandle,
                                              mng_uint32  iEntry,
                                              mng_chunkid iChunkname,
                                              mng_uint8   iOrdertype)
{
  mng_datap       pData;
  mng_chunkp      pChunk;
  mng_ordr_entryp pEntry;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_ORDR_ENTRY, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = (mng_datap)hHandle;          /* and make it addressable */

  if (!pData->bCreating)               /* aren't we creating a new file ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
                                       /* must have had a MHDR first! */
  if (pData->iFirstchunkadded != MNG_UINT_MHDR)
    MNG_ERROR (pData, MNG_NOHEADER)

  pChunk = pData->pLastchunk;          /* last one must have been ORDR ! */

  if (((mng_chunk_headerp)pChunk)->iChunkname != MNG_UINT_ORDR)
    MNG_ERROR (pData, MNG_NOCORRCHUNK)
                                       /* index out of bounds ? */
  if (iEntry >= ((mng_ordrp)pChunk)->iCount)
    MNG_ERROR (pData, MNG_INVALIDENTRYIX)
                                       /* address proper entry */
  pEntry = ((mng_ordrp)pChunk)->pEntries + iEntry;

  pEntry->iChunkname = iChunkname;     /* fill the entry */
  pEntry->iOrdertype = iOrdertype;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_ORDR_ENTRY, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_putchunk_magn (mng_handle hHandle,
                                        mng_uint16 iFirstid,
                                        mng_uint16 iLastid,
                                        mng_uint16 iMethodX,
                                        mng_uint16 iMX,
                                        mng_uint16 iMY,
                                        mng_uint16 iML,
                                        mng_uint16 iMR,
                                        mng_uint16 iMT,
                                        mng_uint16 iMB,
                                        mng_uint16 iMethodY)
{
  mng_datap        pData;
  mng_chunkp       pChunk;
  mng_retcode      iRetcode;
  mng_chunk_header sChunkheader =
          {MNG_UINT_MAGN, init_magn, free_magn, read_magn, write_magn, 0, 0};

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_MAGN, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = (mng_datap)hHandle;          /* and make it addressable */

  if (!pData->bCreating)               /* aren't we creating a new file ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
                                       /* must have had a MHDR first! */
  if (pData->iFirstchunkadded != MNG_UINT_MHDR)
    MNG_ERROR (pData, MNG_NOHEADER)
                                       /* create the chunk */
  iRetcode = init_magn (pData, &sChunkheader, &pChunk);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;
                                       /* fill the chunk */
  ((mng_magnp)pChunk)->iFirstid = iFirstid;
  ((mng_magnp)pChunk)->iLastid  = iLastid;
  ((mng_magnp)pChunk)->iMethodX = iMethodX;
  ((mng_magnp)pChunk)->iMX      = iMX;
  ((mng_magnp)pChunk)->iMY      = iMY;
  ((mng_magnp)pChunk)->iML      = iML;
  ((mng_magnp)pChunk)->iMR      = iMR;
  ((mng_magnp)pChunk)->iMT      = iMT;
  ((mng_magnp)pChunk)->iMB      = iMB;
  ((mng_magnp)pChunk)->iMethodY = iMethodY;

  add_chunk (pData, pChunk);           /* add it to the list */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_MAGN, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_putchunk_unknown (mng_handle  hHandle,
                                           mng_chunkid iChunkname,
                                           mng_uint32  iRawlen,
                                           mng_ptr     pRawdata)
{
  mng_datap        pData;
  mng_chunkp       pChunk;
  mng_retcode      iRetcode;
  mng_chunk_header sChunkheader =
          {MNG_UINT_HUH, init_unknown, free_unknown, read_unknown, write_unknown, 0, 0};

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_UNKNOWN, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = (mng_datap)hHandle;          /* and make it addressable */

  if (!pData->bCreating)               /* aren't we creating a new file ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
                                       /* must have had a header first! */
  if (pData->iFirstchunkadded == 0)
    MNG_ERROR (pData, MNG_NOHEADER)
                                       /* create the chunk */
  iRetcode = init_unknown (pData, &sChunkheader, &pChunk);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;
                                       /* fill the chunk */
  ((mng_unknown_chunkp)pChunk)->sHeader.iChunkname = iChunkname;
  ((mng_unknown_chunkp)pChunk)->iDatasize          = iRawlen;

  if (iRawlen)
  {
    MNG_ALLOC (pData, ((mng_unknown_chunkp)pChunk)->pData, iRawlen)
    MNG_COPY (((mng_unknown_chunkp)pChunk)->pData, pRawdata, iRawlen)
  }

  add_chunk (pData, pChunk);           /* add it to the list */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTCHUNK_UNKNOWN, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */
/* B004 */
#endif /* MNG_INCLUDE_WRITE_PROCS */
/* B004 */
/* ************************************************************************** */
/* ************************************************************************** */

mng_retcode MNG_DECL mng_getimgdata_seq (mng_handle        hHandle,
                                         mng_uint32        iSeqnr,
                                         mng_uint32        iCanvasstyle,
                                         mng_getcanvasline fGetcanvasline)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETIMGDATA_SEQ, MNG_LC_START)
#endif



#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETIMGDATA_SEQ, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_getimgdata_chunkseq (mng_handle        hHandle,
                                              mng_uint32        iSeqnr,
                                              mng_uint32        iCanvasstyle,
                                              mng_getcanvasline fGetcanvasline)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETIMGDATA_CHUNKSEQ, MNG_LC_START)
#endif



#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETIMGDATA_CHUNKSEQ, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_getimgdata_chunk (mng_handle        hHandle,
                                           mng_handle        hChunk,
                                           mng_uint32        iCanvasstyle,
                                           mng_getcanvasline fGetcanvasline)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETIMGDATA_CHUNK, MNG_LC_START)
#endif



#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETIMGDATA_CHUNK, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */
/* ************************************************************************** */
/* B004 */
#ifdef MNG_INCLUDE_WRITE_PROCS
/* B004 */
/* ************************************************************************** */

mng_retcode MNG_DECL mng_putimgdata_ihdr (mng_handle        hHandle,
                                          mng_uint32        iWidth,
                                          mng_uint32        iHeight,
                                          mng_uint8         iColortype,
                                          mng_uint8         iBitdepth,
                                          mng_uint8         iCompression,
                                          mng_uint8         iFilter,
                                          mng_uint8         iInterlace,
                                          mng_uint32        iCanvasstyle,
                                          mng_getcanvasline fGetcanvasline)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTIMGDATA_IHDR, MNG_LC_START)
#endif



#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTIMGDATA_IHDR, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_putimgdata_jhdr (mng_handle        hHandle,
                                          mng_uint32        iWidth,
                                          mng_uint32        iHeight,
                                          mng_uint8         iColortype,
                                          mng_uint8         iBitdepth,
                                          mng_uint8         iCompression,
                                          mng_uint8         iInterlace,
                                          mng_uint8         iAlphaBitdepth,
                                          mng_uint8         iAlphaCompression,
                                          mng_uint8         iAlphaFilter,
                                          mng_uint8         iAlphaInterlace,
                                          mng_uint32        iCanvasstyle,
                                          mng_getcanvasline fGetcanvasline)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTIMGDATA_JHDR, MNG_LC_START)
#endif



#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_PUTIMGDATA_JHDR, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_updatemngheader (mng_handle hHandle,
                                          mng_uint32 iFramecount,
                                          mng_uint32 iLayercount,
                                          mng_uint32 iPlaytime)
{
  mng_datap  pData;
  mng_chunkp pChunk;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_UPDATEMNGHEADER, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = (mng_datap)hHandle;          /* and make it addressable */

  if (!pData->bCreating)               /* aren't we creating a new file ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
                                       /* must be a MNG animation! */
  if ((pData->eImagetype != mng_it_mng) || (pData->iFirstchunkadded != MNG_UINT_MHDR))
    MNG_ERROR (pData, MNG_NOMHDR)

  pChunk = pData->pFirstchunk;         /* get the first chunk */
                                       /* and update the variables */
  ((mng_mhdrp)pChunk)->iFramecount = iFramecount;
  ((mng_mhdrp)pChunk)->iLayercount = iLayercount;
  ((mng_mhdrp)pChunk)->iPlaytime   = iPlaytime;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_UPDATEMNGHEADER, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_updatemngsimplicity (mng_handle hHandle,
                                              mng_uint32 iSimplicity)
{
  mng_datap  pData;
  mng_chunkp pChunk;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_UPDATEMNGSIMPLICITY, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = (mng_datap)hHandle;          /* and make it addressable */

  if (!pData->bCreating)               /* aren't we creating a new file ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
                                       /* must be a MNG animation! */
  if ((pData->eImagetype != mng_it_mng) || (pData->iFirstchunkadded != MNG_UINT_MHDR))
    MNG_ERROR (pData, MNG_NOMHDR)

  pChunk = pData->pFirstchunk;         /* get the first chunk */
                                       /* and update the variable */
  ((mng_mhdrp)pChunk)->iSimplicity = iSimplicity;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_UPDATEMNGSIMPLICITY, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */
/* B004 */
#endif /* MNG_INCLUDE_WRITE_PROCS */
/* B004 */
/* ************************************************************************** */

#endif /* MNG_ACCESS_CHUNKS */

/* ************************************************************************** */
/* * end of file                                                            * */
/* ************************************************************************** */

