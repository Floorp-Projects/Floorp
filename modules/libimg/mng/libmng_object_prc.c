/* ************************************************************************** */
/* *             For conditions of distribution and use,                    * */
/* *                see copyright notice in libmng.h                        * */
/* ************************************************************************** */
/* *                                                                        * */
/* * project   : libmng                                                     * */
/* * file      : libmng_object_prc.c       copyright (c) 2000 G.Juyn        * */
/* * version   : 1.0.2                                                      * */
/* *                                                                        * */
/* * purpose   : Object processing routines (implementation)                * */
/* *                                                                        * */
/* * author    : G.Juyn                                                     * */
/* * web       : http://www.3-t.com                                         * */
/* * email     : mailto:info@3-t.com                                        * */
/* *                                                                        * */
/* * comment   : implementation of the internal object processing routines  * */
/* *                                                                        * */
/* * changes   : 0.5.1 - 05/08/2000 - G.Juyn                                * */
/* *             - changed strict-ANSI stuff                                * */
/* *             0.5.1 - 05/12/2000 - G.Juyn                                * */
/* *             - changed trace to macro for callback error-reporting      * */
/* *                                                                        * */
/* *             0.5.2 - 05/20/2000 - G.Juyn                                * */
/* *             - fixed to support JNG objects                             * */
/* *             0.5.2 - 05/24/2000 - G.Juyn                                * */
/* *             - added support for global color-chunks in animation       * */
/* *             - added support for global PLTE,tRNS,bKGD in animation     * */
/* *             - added SAVE & SEEK animation objects                      * */
/* *             0.5.2 - 05/29/2000 - G.Juyn                                * */
/* *             - added initialization of framenr/layernr/playtime         * */
/* *             - changed ani_object create routines not to return the     * */
/* *               created object (wasn't necessary)                        * */
/* *             0.5.2 - 05/30/2000 - G.Juyn                                * */
/* *             - added object promotion routine (PROM handling)           * */
/* *             - added ani-object routines for delta-image processing     * */
/* *             - added compression/filter/interlace fields to             * */
/* *               object-buffer for delta-image processing                 * */
/* *                                                                        * */
/* *             0.5.3 - 06/17/2000 - G.Juyn                                * */
/* *             - changed support for delta-image processing               * */
/* *             0.5.3 - 06/20/2000 - G.Juyn                                * */
/* *             - fixed some small things (as precaution)                  * */
/* *             0.5.3 - 06/21/2000 - G.Juyn                                * */
/* *             - added processing of PLTE/tRNS & color-info for           * */
/* *               delta-images in the ani_objects chain                    * */
/* *             0.5.3 - 06/22/2000 - G.Juyn                                * */
/* *             - added support for PPLT chunk                             * */
/* *                                                                        * */
/* *             0.9.1 - 07/07/2000 - G.Juyn                                * */
/* *             - added support for freeze/restart/resume & go_xxxx        * */
/* *             0.9.1 - 07/16/2000 - G.Juyn                                * */
/* *             - fixed support for mng_display() after mng_read()         * */
/* *                                                                        * */
/* *             0.9.2 - 07/29/2000 - G.Juyn                                * */
/* *             - fixed small bugs in display processing                   * */
/* *             0.9.2 - 08/05/2000 - G.Juyn                                * */
/* *             - changed file-prefixes                                    * */
/* *                                                                        * */
/* *             0.9.3 - 08/07/2000 - G.Juyn                                * */
/* *             - B111300 - fixup for improved portability                 * */
/* *             0.9.3 - 08/26/2000 - G.Juyn                                * */
/* *             - added MAGN chunk                                         * */
/* *             0.9.3 - 09/10/2000 - G.Juyn                                * */
/* *             - fixed DEFI behavior                                      * */
/* *             0.9.3 - 10/17/2000 - G.Juyn                                * */
/* *             - added valid-flag to stored objects for read() / display()* */
/* *             - added routine to discard "invalid" objects               * */
/* *             0.9.3 - 10/18/2000 - G.Juyn                                * */
/* *             - fixed delta-processing behavior                          * */
/* *             0.9.3 - 10/19/2000 - G.Juyn                                * */
/* *             - added storage for pixel-/alpha-sampledepth for delta's   * */
/* *                                                                        * */
/* *             0.9.4 -  1/18/2001 - G.Juyn                                * */
/* *             - removed "old" MAGN methods 3 & 4                         * */
/* *             - added "new" MAGN methods 3, 4 & 5                        * */
/* *                                                                        * */
/* *             0.9.5 -  1/22/2001 - G.Juyn                                * */
/* *             - B129681 - fixed compiler warnings SGI/Irix               * */
/* *                                                                        * */
/* *             1.0.2 - 06/23/2001 - G.Juyn                                * */
/* *             - added optimization option for MNG-video playback         * */
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
#include "libmng_display.h"
#include "libmng_pixels.h"
#include "libmng_object_prc.h"

#if defined(__BORLANDC__) && defined(MNG_STRICT_ANSI)
#pragma option -A                      /* force ANSI-C */
#endif

/* ************************************************************************** */

#ifdef MNG_INCLUDE_DISPLAY_PROCS

/* ************************************************************************** */
/* *                                                                        * */
/* * Generic object routines                                                * */
/* *                                                                        * */
/* ************************************************************************** */

mng_retcode drop_invalid_objects (mng_datap pData)
{
  mng_objectp       pObject;
  mng_objectp       pNext;
  mng_cleanupobject fCleanup;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DROP_INVALID_OBJECTS, MNG_LC_START)
#endif

  pObject = pData->pFirstimgobj;       /* get first stored image-object (if any) */

  while (pObject)                      /* more objects to check ? */
  {
    pNext = ((mng_object_headerp)pObject)->pNext;
                                       /* invalid ? */
    if (!((mng_imagep)pObject)->bValid)
    {                                  /* call appropriate cleanup */
      fCleanup = ((mng_object_headerp)pObject)->fCleanup;
      fCleanup (pData, pObject);
    }

    pObject = pNext;                   /* neeeext */
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DROP_INVALID_OBJECTS, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */
/* *                                                                        * */
/* * Image-data-object routines                                             * */
/* *                                                                        * */
/* * these handle the "object buffer" as defined by the MNG specification   * */
/* *                                                                        * */
/* ************************************************************************** */

mng_retcode create_imagedataobject (mng_datap      pData,
                                    mng_bool       bConcrete,
                                    mng_bool       bViewable,
                                    mng_uint32     iWidth,
                                    mng_uint32     iHeight,
                                    mng_uint8      iBitdepth,
                                    mng_uint8      iColortype,
                                    mng_uint8      iCompression,
                                    mng_uint8      iFilter,
                                    mng_uint8      iInterlace,
                                    mng_imagedatap *ppObject)
{
  mng_imagedatap pImagedata;
  mng_uint32 iSamplesize = 0;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CREATE_IMGDATAOBJECT, MNG_LC_START)
#endif
                                       /* get a buffer */
  MNG_ALLOC (pData, pImagedata, sizeof (mng_imagedata))
                                       /* fill the appropriate fields */
  pImagedata->sHeader.fCleanup   = (mng_cleanupobject)free_imagedataobject;
  pImagedata->sHeader.fProcess   = 0;
  pImagedata->iRefcount          = 1;
  pImagedata->bFrozen            = MNG_FALSE;
  pImagedata->bConcrete          = bConcrete;
  pImagedata->bViewable          = bViewable;
  pImagedata->iWidth             = iWidth;
  pImagedata->iHeight            = iHeight;
  pImagedata->iBitdepth          = iBitdepth;
  pImagedata->iColortype         = iColortype;
  pImagedata->iCompression       = iCompression;
  pImagedata->iFilter            = iFilter;
  pImagedata->iInterlace         = iInterlace;
  pImagedata->iAlphabitdepth     = 0;
  pImagedata->iJHDRcompression   = 0;
  pImagedata->iJHDRinterlace     = 0;
  pImagedata->iPixelsampledepth  = iBitdepth;
  pImagedata->iAlphasampledepth  = iBitdepth;
                                       /* determine samplesize from color_type/bit_depth */
  switch (iColortype)                  /* for < 8-bit samples we just reserve 8 bits */
  {
    case  0  : ;                       /* gray */
    case  8  : {                       /* JPEG gray */
                 if (iBitdepth > 8)
                   iSamplesize = 2;
                 else
                   iSamplesize = 1;

                 break;
               }
    case  2  : ;                       /* rgb */
    case 10  : {                       /* JPEG rgb */
                 if (iBitdepth > 8)
                   iSamplesize = 6;
                 else
                   iSamplesize = 3;

                 break;
               }
    case  3  : {                       /* indexed */
                 iSamplesize = 1;
                 break;
               }
    case  4  : ;                       /* gray+alpha */
    case 12  : {                       /* JPEG gray+alpha */
                 if (iBitdepth > 8)
                   iSamplesize = 4;
                 else
                   iSamplesize = 2;

                 break;
               }
    case  6  : ;                       /* rgb+alpha */
    case 14  : {                       /* JPEG rgb+alpha */
                 if (iBitdepth > 8)
                   iSamplesize = 8;
                 else
                   iSamplesize = 4;

                 break;
               }
  }
                                       /* make sure we remember all this */
  pImagedata->iSamplesize  = iSamplesize;
  pImagedata->iRowsize     = iSamplesize * iWidth;
  pImagedata->iImgdatasize = pImagedata->iRowsize * iHeight;

  if (pImagedata->iImgdatasize)        /* need a buffer ? */
  {                                    /* so allocate it */
    MNG_ALLOCX (pData, pImagedata->pImgdata, pImagedata->iImgdatasize)

    if (!pImagedata->pImgdata)         /* enough memory ? */
    {
      MNG_FREEX (pData, pImagedata, sizeof (mng_imagedata))
      MNG_ERROR (pData, MNG_OUTOFMEMORY)
    }
  }
                                       /* check global stuff */
  pImagedata->bHasGAMA           = pData->bHasglobalGAMA;
  pImagedata->bHasCHRM           = pData->bHasglobalCHRM;
  pImagedata->bHasSRGB           = pData->bHasglobalSRGB;
  pImagedata->bHasICCP           = pData->bHasglobalICCP;
  pImagedata->bHasBKGD           = pData->bHasglobalBKGD;

  if (pData->bHasglobalGAMA)           /* global gAMA present ? */
    pImagedata->iGamma           = pData->iGlobalGamma;

  if (pData->bHasglobalCHRM)           /* global cHRM present ? */
  {
    pImagedata->iWhitepointx     = pData->iGlobalWhitepointx;
    pImagedata->iWhitepointy     = pData->iGlobalWhitepointy;
    pImagedata->iPrimaryredx     = pData->iGlobalPrimaryredx;
    pImagedata->iPrimaryredy     = pData->iGlobalPrimaryredy;
    pImagedata->iPrimarygreenx   = pData->iGlobalPrimarygreenx;
    pImagedata->iPrimarygreeny   = pData->iGlobalPrimarygreeny;
    pImagedata->iPrimarybluex    = pData->iGlobalPrimarybluex;
    pImagedata->iPrimarybluey    = pData->iGlobalPrimarybluey;
  }

  if (pData->bHasglobalSRGB)           /* glbal sRGB present ? */
    pImagedata->iRenderingintent = pData->iGlobalRendintent;

  if (pData->bHasglobalICCP)           /* glbal iCCP present ? */
  {
    pImagedata->iProfilesize     = pData->iGlobalProfilesize;

    if (pImagedata->iProfilesize)
    {
      MNG_ALLOCX (pData, pImagedata->pProfile, pImagedata->iProfilesize)

      if (!pImagedata->pProfile)       /* enough memory ? */
      {
        MNG_FREEX (pData, pImagedata->pImgdata, pImagedata->iImgdatasize)
        MNG_FREEX (pData, pImagedata, sizeof (mng_imagedata))
        MNG_ERROR (pData, MNG_OUTOFMEMORY)
      }

      MNG_COPY  (pImagedata->pProfile, pData->pGlobalProfile, pImagedata->iProfilesize)
    }
  }

  if (pData->bHasglobalBKGD)           /* global bKGD present ? */
  {
    pImagedata->iBKGDred         = pData->iGlobalBKGDred;
    pImagedata->iBKGDgreen       = pData->iGlobalBKGDgreen;
    pImagedata->iBKGDblue        = pData->iGlobalBKGDblue;
  }

  *ppObject = pImagedata;              /* return it */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CREATE_IMGDATAOBJECT, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode free_imagedataobject   (mng_datap      pData,
                                    mng_imagedatap pImagedata)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_IMGDATAOBJECT, MNG_LC_START)
#endif

  if (pImagedata->iRefcount)           /* decrease reference count */
    pImagedata->iRefcount--;

  if (!pImagedata->iRefcount)          /* reached zero ? */
  {
    if (pImagedata->iProfilesize)      /* stored an iCCP profile ? */
      MNG_FREEX (pData, pImagedata->pProfile, pImagedata->iProfilesize)

    if (pImagedata->iImgdatasize)      /* sample-buffer present ? */
      MNG_FREEX (pData, pImagedata->pImgdata, pImagedata->iImgdatasize)
                                       /* drop the buffer */
    MNG_FREEX (pData, pImagedata, sizeof (mng_imagedata))
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_IMGDATAOBJECT, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode clone_imagedataobject  (mng_datap      pData,
                                    mng_bool       bConcrete,
                                    mng_imagedatap pSource,
                                    mng_imagedatap *ppClone)
{
  mng_imagedatap pNewdata;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CLONE_IMGDATAOBJECT, MNG_LC_START)
#endif
                                       /* get a buffer */
  MNG_ALLOC (pData, pNewdata, sizeof (mng_imagedata))
                                       /* blatently copy the original buffer */
  MNG_COPY (pNewdata, pSource, sizeof (mng_imagedata))

  pNewdata->iRefcount = 1;             /* only the reference count */
  pNewdata->bConcrete = bConcrete;     /* and concrete-flag are different */

  if (pNewdata->iImgdatasize)          /* sample buffer present ? */
  {
    MNG_ALLOCX (pData, pNewdata->pImgdata, pNewdata->iImgdatasize)

    if (!pNewdata->pImgdata)           /* not enough memory ? */
    {
      MNG_FREEX (pData, pNewdata, sizeof (mng_imagedata))
      MNG_ERROR (pData, MNG_OUTOFMEMORY)
    }
                                       /* make a copy */
    MNG_COPY (pNewdata->pImgdata, pSource->pImgdata, pNewdata->iImgdatasize)
  }

  if (pNewdata->iProfilesize)          /* iCCP profile present ? */
  {
    MNG_ALLOCX (pData, pNewdata->pProfile, pNewdata->iProfilesize)

    if (!pNewdata->pProfile)           /* enough memory ? */
    {
      MNG_FREEX (pData, pNewdata, sizeof (mng_imagedata))
      MNG_ERROR (pData, MNG_OUTOFMEMORY)
    }
                                       /* make a copy */
    MNG_COPY (pNewdata->pProfile, pSource->pProfile, pNewdata->iProfilesize)
  }

  *ppClone = pNewdata;                 /* return the clone */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CLONE_IMGDATAOBJECT, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */
/* *                                                                        * */
/* * Image-object routines                                                  * */
/* *                                                                        * */
/* * these handle the "object" as defined by the MNG specification          * */
/* *                                                                        * */
/* ************************************************************************** */

mng_retcode create_imageobject (mng_datap  pData,
                                mng_uint16 iId,
                                mng_bool   bConcrete,
                                mng_bool   bVisible,
                                mng_bool   bViewable,
                                mng_uint32 iWidth,
                                mng_uint32 iHeight,
                                mng_uint8  iBitdepth,
                                mng_uint8  iColortype,
                                mng_uint8  iCompression,
                                mng_uint8  iFilter,
                                mng_uint8  iInterlace,
                                mng_int32  iPosx,
                                mng_int32  iPosy,
                                mng_bool   bClipped,
                                mng_int32  iClipl,
                                mng_int32  iClipr,
                                mng_int32  iClipt,
                                mng_int32  iClipb,
                                mng_imagep *ppObject)
{
  mng_imagep     pImage;
  mng_imagep     pPrev, pNext;
  mng_retcode    iRetcode;
  mng_imagedatap pImgbuf;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CREATE_IMGOBJECT, MNG_LC_START)
#endif
                                       /* get a buffer */
  MNG_ALLOC (pData, pImage, sizeof (mng_image))
                                       /* now get a new "object buffer" */
  iRetcode = create_imagedataobject (pData, bConcrete, bViewable,
                                     iWidth, iHeight, iBitdepth, iColortype,
                                     iCompression, iFilter, iInterlace,
                                     &pImgbuf);

  if (iRetcode)                        /* on error bail out */
  {
    MNG_FREEX (pData, pImage, sizeof (mng_image))
    return iRetcode;
  }
                                       /* fill the appropriate fields */
  pImage->sHeader.fCleanup = (mng_cleanupobject)free_imageobject;
  pImage->sHeader.fProcess = 0;
  pImage->iId              = iId;
  pImage->bFrozen          = MNG_FALSE;
  pImage->bVisible         = bVisible;
  pImage->bViewable        = bViewable;
  pImage->bValid           = (mng_bool)((pData->bDisplaying) &&
                                        (pData->bRunning   ) &&
                                        (!pData->bFreezing )    ); 
  pImage->iPosx            = iPosx;
  pImage->iPosy            = iPosy;
  pImage->bClipped         = bClipped;
  pImage->iClipl           = iClipl;
  pImage->iClipr           = iClipr;
  pImage->iClipt           = iClipt;
  pImage->iClipb           = iClipb;
  pImage->iMAGN_MethodX    = 0;
  pImage->iMAGN_MethodY    = 0;
  pImage->iMAGN_MX         = 0;
  pImage->iMAGN_MY         = 0;
  pImage->iMAGN_ML         = 0;
  pImage->iMAGN_MR         = 0;
  pImage->iMAGN_MT         = 0;
  pImage->iMAGN_MB         = 0;
  pImage->pImgbuf          = pImgbuf;

  if (iId)                             /* only if not object 0 ! */
  {                                    /* find previous lower object-id */
    pPrev = (mng_imagep)pData->pLastimgobj;

    while ((pPrev) && (pPrev->iId > iId))
      pPrev = (mng_imagep)pPrev->sHeader.pPrev;

    if (pPrev)                         /* found it ? */
    {
      pImage->sHeader.pPrev = pPrev;   /* than link it in place */
      pImage->sHeader.pNext = pPrev->sHeader.pNext;
      pPrev->sHeader.pNext  = pImage;
    }
    else                               /* if not found, it becomes the first ! */
    {
      pImage->sHeader.pNext = pData->pFirstimgobj;
      pData->pFirstimgobj   = pImage;
    }

    pNext                   = (mng_imagep)pImage->sHeader.pNext;

    if (pNext)
      pNext->sHeader.pPrev  = pImage;
    else
      pData->pLastimgobj    = pImage;
    
  }  

  *ppObject = pImage;                  /* and return the new buffer */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CREATE_IMGOBJECT, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* okido */
}

/* ************************************************************************** */

mng_retcode free_imageobject (mng_datap  pData,
                              mng_imagep pImage)
{
  mng_retcode    iRetcode;
  mng_imagep     pPrev   = pImage->sHeader.pPrev;
  mng_imagep     pNext   = pImage->sHeader.pNext;
  mng_imagedatap pImgbuf = pImage->pImgbuf;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_IMGOBJECT, MNG_LC_START)
#endif

  if (pImage->iId)                     /* not for object 0 */
  {
    if (pPrev)                         /* unlink from the list first ! */
      pPrev->sHeader.pNext = pImage->sHeader.pNext;
    else
      pData->pFirstimgobj  = pImage->sHeader.pNext;

    if (pNext)
      pNext->sHeader.pPrev = pImage->sHeader.pPrev;
    else
      pData->pLastimgobj   = pImage->sHeader.pPrev;

  }
                                       /* unlink the image-data buffer */
  iRetcode = free_imagedataobject (pData, pImgbuf);
                                       /* drop it's own buffer */
  MNG_FREEX (pData, pImage, sizeof (mng_image))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_IMGOBJECT, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */

mng_imagep find_imageobject (mng_datap  pData,
                             mng_uint16 iId)
{
  mng_imagep pImage = (mng_imagep)pData->pFirstimgobj;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (pData, MNG_FN_FIND_IMGOBJECT, MNG_LC_START)
#endif
                                       /* look up the right id */
  while ((pImage) && (pImage->iId != iId))
    pImage = (mng_imagep)pImage->sHeader.pNext;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (pData, MNG_FN_FIND_IMGOBJECT, MNG_LC_END)
#endif

  return pImage;
}

/* ************************************************************************** */

mng_retcode clone_imageobject (mng_datap  pData,
                               mng_uint16 iId,
                               mng_bool   bPartial,
                               mng_bool   bVisible,
                               mng_bool   bAbstract,
                               mng_bool   bHasloca,
                               mng_uint8  iLocationtype,
                               mng_int32  iLocationx,
                               mng_int32  iLocationy,
                               mng_imagep pSource,
                               mng_imagep *ppClone)
{
  mng_imagep     pNew;
  mng_imagep     pPrev, pNext;
  mng_retcode    iRetcode;
  mng_imagedatap pImgbuf;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CLONE_IMGOBJECT, MNG_LC_START)
#endif

  if ((pSource->iId) &&                /* needs magnification ? */
      ((pSource->iMAGN_MethodX) || (pSource->iMAGN_MethodY)))
  {
    iRetcode = magnify_imageobject (pData, pSource);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
  }
                                       /* get a buffer */
  MNG_ALLOC (pData, pNew, sizeof (mng_image))
                                       /* fill or copy the appropriate fields */
  pNew->sHeader.fCleanup = (mng_cleanupobject)free_imageobject;
  pNew->sHeader.fProcess = 0;
  pNew->iId              = iId;
  pNew->bFrozen          = MNG_FALSE;
  pNew->bVisible         = bVisible;
  pNew->bViewable        = pSource->bViewable;

  if (bHasloca)                        /* location info available ? */
  {
    if (iLocationtype == 0)            /* absolute position ? */
    {
      pNew->iPosx        = iLocationx;
      pNew->iPosy        = iLocationy;
    }
    else                               /* relative */
    {
      pNew->iPosx        = pSource->iPosx + iLocationx;
      pNew->iPosy        = pSource->iPosy + iLocationy;
    }
  }
  else                                 /* copy from source */
  {
    pNew->iPosx          = pSource->iPosx;
    pNew->iPosy          = pSource->iPosy;
  }
                                       /* copy clipping info */
  pNew->bClipped         = pSource->bClipped;
  pNew->iClipl           = pSource->iClipl;
  pNew->iClipr           = pSource->iClipr;
  pNew->iClipt           = pSource->iClipt;
  pNew->iClipb           = pSource->iClipb;
                                       /* copy magnification info */
  pNew->iMAGN_MethodX    = pSource->iMAGN_MethodX;
  pNew->iMAGN_MethodY    = pSource->iMAGN_MethodY;
  pNew->iMAGN_MX         = pSource->iMAGN_MX;
  pNew->iMAGN_MY         = pSource->iMAGN_MY;
  pNew->iMAGN_ML         = pSource->iMAGN_ML;
  pNew->iMAGN_MR         = pSource->iMAGN_MR;
  pNew->iMAGN_MT         = pSource->iMAGN_MT;
  pNew->iMAGN_MB         = pSource->iMAGN_MB;

  if (iId)                             /* not for object 0 */
  {                                    /* find previous lower object-id */
    pPrev = (mng_imagep)pData->pLastimgobj;
    while ((pPrev) && (pPrev->iId > iId))
      pPrev = (mng_imagep)pPrev->sHeader.pPrev;

    if (pPrev)                         /* found it ? */
    {
      pNew->sHeader.pPrev  = pPrev;    /* than link it in place */
      pNew->sHeader.pNext  = pPrev->sHeader.pNext;
      pPrev->sHeader.pNext = pNew;
    }
    else                               /* if not found, it becomes the first ! */
    {
      pNew->sHeader.pNext  = pData->pFirstimgobj;
      pData->pFirstimgobj  = pNew;
    }

    pNext                  = (mng_imagep)pNew->sHeader.pNext;

    if (pNext)
      pNext->sHeader.pPrev = pNew;
    else
      pData->pLastimgobj   = pNew;

  }

  if (bPartial)                        /* partial clone ? */
  {
    pNew->pImgbuf = pSource->pImgbuf;  /* use the same object buffer */
    pNew->pImgbuf->iRefcount++;        /* and increase the reference count */
  }
  else                                 /* create a full clone ! */
  {
    mng_bool bConcrete = MNG_FALSE;    /* it's abstract by default (?) */

    if (!bAbstract)                    /* determine concreteness from source ? */
      bConcrete = pSource->pImgbuf->bConcrete;
                                       /* create a full clone ! */
    iRetcode = clone_imagedataobject (pData, bConcrete, pSource->pImgbuf, &pImgbuf);

    if (iRetcode)                      /* on error bail out */
    {
      MNG_FREEX (pData, pNew, sizeof (mng_image))
      return iRetcode;
    }

    pNew->pImgbuf = pImgbuf;           /* and remember it */
  }

  *ppClone = pNew;                     /* return it */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CLONE_IMGOBJECT, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode renum_imageobject (mng_datap  pData,
                               mng_imagep pSource,
                               mng_uint16 iId,
                               mng_bool   bVisible,
                               mng_bool   bAbstract,
                               mng_bool   bHasloca,
                               mng_uint8  iLocationtype,
                               mng_int32  iLocationx,
                               mng_int32  iLocationy)
{
  mng_imagep pPrev, pNext;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_RENUM_IMGOBJECT, MNG_LC_START)
#endif

  pSource->bVisible  = bVisible;       /* store the new visibility */

  if (bHasloca)                        /* location info available ? */
  {
    if (iLocationtype == 0)            /* absolute position ? */
    {
      pSource->iPosx = iLocationx;
      pSource->iPosy = iLocationy;
    }
    else                               /* relative */
    {
      pSource->iPosx = pSource->iPosx + iLocationx;
      pSource->iPosy = pSource->iPosy + iLocationy;
    }
  }

  if (iId)                             /* not for object 0 */
  {                                    /* find previous lower object-id */
    pPrev = (mng_imagep)pData->pLastimgobj;
    while ((pPrev) && (pPrev->iId > iId))
      pPrev = (mng_imagep)pPrev->sHeader.pPrev;
                                       /* different from current ? */
    if (pPrev != (mng_imagep)pSource->sHeader.pPrev)
    {
      if (pSource->sHeader.pPrev)      /* unlink from current position !! */
        ((mng_imagep)pSource->sHeader.pPrev)->sHeader.pNext = pSource->sHeader.pNext;
      else
        pData->pFirstimgobj                                 = pSource->sHeader.pNext;

      if (pSource->sHeader.pNext)
        ((mng_imagep)pSource->sHeader.pNext)->sHeader.pPrev = pSource->sHeader.pPrev;
      else
        pData->pLastimgobj                                  = pSource->sHeader.pPrev;

      if (pPrev)                       /* found the previous ? */
      {                                /* than link it in place */
        pSource->sHeader.pPrev = pPrev;
        pSource->sHeader.pNext = pPrev->sHeader.pNext;
        pPrev->sHeader.pNext   = pSource;
      }
      else                             /* if not found, it becomes the first ! */
      {
        pSource->sHeader.pNext = pData->pFirstimgobj;
        pData->pFirstimgobj    = pSource;
      }

      pNext                    = (mng_imagep)pSource->sHeader.pNext;

      if (pNext)
        pNext->sHeader.pPrev   = pSource;
      else
        pData->pLastimgobj     = pSource;

    }
  }

  pSource->iId = iId;                  /* now set the new id! */

  if (bAbstract)                       /* force it to abstract ? */
    pSource->pImgbuf->bConcrete = MNG_FALSE;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_RENUM_IMGOBJECT, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode reset_object_details (mng_datap  pData,
                                  mng_imagep pImage,
                                  mng_uint32 iWidth,
                                  mng_uint32 iHeight,
                                  mng_uint8  iBitdepth,
                                  mng_uint8  iColortype,
                                  mng_uint8  iCompression,
                                  mng_uint8  iFilter,
                                  mng_uint8  iInterlace,
                                  mng_bool   bResetall)
{
  mng_imagedatap pBuf  = pImage->pImgbuf;
  mng_uint32     iSamplesize = 0;
  mng_uint32     iRowsize;
  mng_uint32     iImgdatasize;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_RESET_OBJECTDETAILS, MNG_LC_START)
#endif

  pBuf->iWidth         = iWidth;       /* set buffer characteristics */
  pBuf->iHeight        = iHeight;
  pBuf->iBitdepth      = iBitdepth;
  pBuf->iColortype     = iColortype;
  pBuf->iCompression   = iCompression;
  pBuf->iFilter        = iFilter;
  pBuf->iInterlace     = iInterlace;
  pBuf->iAlphabitdepth = 0;
                                       /* determine samplesize from color_type/bit_depth */
  switch (iColortype)                  /* for < 8-bit samples we just reserve 8 bits */
  {
    case  0  : ;                       /* gray */
    case  8  : {                       /* JPEG gray */
                 if (iBitdepth > 8)
                   iSamplesize = 2;
                 else
                   iSamplesize = 1;

                 break;
               }
    case  2  : ;                       /* rgb */
    case 10  : {                       /* JPEG rgb */
                 if (iBitdepth > 8)
                   iSamplesize = 6;
                 else
                   iSamplesize = 3;

                 break;
               }
    case  3  : {                       /* indexed */
                 iSamplesize = 1;
                 break;
               }
    case  4  : ;                       /* gray+alpha */
    case 12  : {                       /* JPEG gray+alpha */
                 if (iBitdepth > 8)
                   iSamplesize = 4;
                 else
                   iSamplesize = 2;

                 break;
               }
    case  6  : ;                       /* rgb+alpha */
    case 14  : {                       /* JPEG rgb+alpha */
                 if (iBitdepth > 8)
                   iSamplesize = 8;
                 else
                   iSamplesize = 4;

                 break;
               }
  }

  iRowsize     = iSamplesize * iWidth;
  iImgdatasize = iRowsize    * iHeight;
                                       /* buffer size changed ? */
  if (iImgdatasize != pBuf->iImgdatasize)
  {                                    /* drop the old one */
    MNG_FREE (pData, pBuf->pImgdata, pBuf->iImgdatasize)

    if (iImgdatasize)                  /* allocate new sample-buffer ? */
      MNG_ALLOC (pData, pBuf->pImgdata, iImgdatasize)
  }

  pBuf->iSamplesize  = iSamplesize;    /* remember new sizes */
  pBuf->iRowsize     = iRowsize;
  pBuf->iImgdatasize = iImgdatasize;
                                       /* dimension set and clipping not ? */
  if ((iWidth) && (iHeight) && (!pImage->bClipped))
  {
    pImage->iClipl   = 0;              /* set clipping to dimension by default */
    pImage->iClipr   = iWidth;
    pImage->iClipt   = 0;
    pImage->iClipb   = iHeight;
  }

  if (pImage->iId)                     /* reset magnification info ? */
  {
    pImage->iMAGN_MethodX = 0;
    pImage->iMAGN_MethodY = 0;
    pImage->iMAGN_MX      = 0;
    pImage->iMAGN_MY      = 0;
    pImage->iMAGN_ML      = 0;
    pImage->iMAGN_MR      = 0;
    pImage->iMAGN_MT      = 0;
    pImage->iMAGN_MB      = 0;
  }

  if (bResetall)                       /* reset the other characteristics ? */
  {
    pBuf->bHasPLTE = MNG_FALSE;
    pBuf->bHasTRNS = MNG_FALSE;
    pBuf->bHasGAMA = pData->bHasglobalGAMA;
    pBuf->bHasCHRM = pData->bHasglobalCHRM;
    pBuf->bHasSRGB = pData->bHasglobalSRGB;
    pBuf->bHasICCP = pData->bHasglobalICCP;
    pBuf->bHasBKGD = pData->bHasglobalBKGD;

    if (pBuf->iProfilesize)            /* drop possibly old ICC profile */
    {
      MNG_FREE (pData, pBuf->pProfile, pBuf->iProfilesize)
      pBuf->iProfilesize     = 0;
    }  

    if (pData->bHasglobalGAMA)         /* global gAMA present ? */
      pBuf->iGamma           = pData->iGlobalGamma;

    if (pData->bHasglobalCHRM)         /* global cHRM present ? */
    {
      pBuf->iWhitepointx     = pData->iGlobalWhitepointx;
      pBuf->iWhitepointy     = pData->iGlobalWhitepointy;
      pBuf->iPrimaryredx     = pData->iGlobalPrimaryredx;
      pBuf->iPrimaryredy     = pData->iGlobalPrimaryredy;
      pBuf->iPrimarygreenx   = pData->iGlobalPrimarygreenx;
      pBuf->iPrimarygreeny   = pData->iGlobalPrimarygreeny;
      pBuf->iPrimarybluex    = pData->iGlobalPrimarybluex;
      pBuf->iPrimarybluey    = pData->iGlobalPrimarybluey;
    }

    if (pData->bHasglobalSRGB)           /* global sRGB present ? */
      pBuf->iRenderingintent = pData->iGlobalRendintent;

    if (pData->bHasglobalICCP)           /* global iCCP present ? */
    {
      if (pData->iGlobalProfilesize)
      {
        MNG_ALLOC (pData, pBuf->pProfile, pData->iGlobalProfilesize)
        MNG_COPY  (pBuf->pProfile, pData->pGlobalProfile, pData->iGlobalProfilesize)
      }

      pBuf->iProfilesize     = pData->iGlobalProfilesize;
    }

    if (pData->bHasglobalBKGD)           /* global bKGD present ? */
    {
      pBuf->iBKGDred         = pData->iGlobalBKGDred;
      pBuf->iBKGDgreen       = pData->iGlobalBKGDgreen;
      pBuf->iBKGDblue        = pData->iGlobalBKGDblue;
    }
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_RESET_OBJECTDETAILS, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode promote_imageobject (mng_datap  pData,
                                 mng_imagep pImage,
                                 mng_uint8  iBitdepth,
                                 mng_uint8  iColortype,
                                 mng_uint8  iFilltype)
{
  mng_uint8p     pNewbuf;
  mng_uint32     iNewbufsize;
  mng_uint32     iNewrowsize;
  mng_uint32     iNewsamplesize;
  mng_uint32     iX, iY;
  mng_uint8p     pSrcline, pDstline;
  mng_uint8      iB;
  mng_imagedatap pBuf = pImage->pImgbuf;
  mng_uint32     iW   = pBuf->iWidth;
  mng_uint32     iH   = pBuf->iHeight;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROMOTE_IMGOBJECT, MNG_LC_START)
#endif

  if ((pBuf->iColortype == 3) && (iColortype == 2))
  {                                    /* indexed -> rgb */
    iNewsamplesize = 3;
    iNewrowsize    = iW * iNewsamplesize;
    iNewbufsize    = iH * iNewrowsize;

    MNG_ALLOC (pData, pNewbuf, iNewbufsize)

    pSrcline = pBuf->pImgdata;
    pDstline = pNewbuf;

    for (iY = 0; iY < iH; iY++)
    {
      for (iX = 0; iX < iW; iX++)
      {
        iB = *pSrcline;

        if ((mng_uint32)iB < pBuf->iPLTEcount)
        {
          *pDstline     = pBuf->aPLTEentries [iB].iRed;
          *(pDstline+1) = pBuf->aPLTEentries [iB].iGreen;
          *(pDstline+2) = pBuf->aPLTEentries [iB].iBlue;
        }  

        pSrcline++;
        pDstline += 3;
      }
    }

    MNG_FREEX (pData, pBuf->pImgdata, pBuf->iImgdatasize)

    pBuf->iBitdepth    = iBitdepth;
    pBuf->iColortype   = iColortype;
    pBuf->iSamplesize  = iNewsamplesize;
    pBuf->iRowsize     = iNewrowsize;
    pBuf->iImgdatasize = iNewbufsize;
    pBuf->pImgdata     = pNewbuf;
    pBuf->bHasPLTE     = MNG_FALSE;
    pBuf->bHasTRNS     = MNG_FALSE;
  }
  else
  if ((pBuf->iColortype == 3) && (iColortype == 6))
  {                                    /* indexed -> rgba */
    iNewsamplesize = 4;
    iNewrowsize    = iW * iNewsamplesize;
    iNewbufsize    = iH * iNewrowsize;

    MNG_ALLOC (pData, pNewbuf, iNewbufsize)

    pSrcline = pBuf->pImgdata;
    pDstline = pNewbuf;

    for (iY = 0; iY < iH; iY++)
    {
      for (iX = 0; iX < iW; iX++)
      {
        iB = *pSrcline;

        if ((mng_uint32)iB < pBuf->iPLTEcount)
        {
          *pDstline       = pBuf->aPLTEentries [iB].iRed;
          *(pDstline+1)   = pBuf->aPLTEentries [iB].iGreen;
          *(pDstline+2)   = pBuf->aPLTEentries [iB].iBlue;

          if ((mng_uint32)iB < pBuf->iTRNScount)
            *(pDstline+3) = pBuf->aTRNSentries [iB];
          else
            *(pDstline+3) = 255;
        }

        pSrcline++;
        pDstline += 4;
      }
    }

    MNG_FREEX (pData, pBuf->pImgdata, pBuf->iImgdatasize)

    pBuf->iBitdepth    = iBitdepth;
    pBuf->iColortype   = iColortype;
    pBuf->iSamplesize  = iNewsamplesize;
    pBuf->iRowsize     = iNewrowsize;
    pBuf->iImgdatasize = iNewbufsize;
    pBuf->pImgdata     = pNewbuf;
    pBuf->bHasPLTE     = MNG_FALSE;
    pBuf->bHasTRNS     = MNG_FALSE;
  }
  else
  {

    /* TODO: other promotion */

  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROMOTE_IMGOBJECT, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode magnify_imageobject (mng_datap  pData,
                                 mng_imagep pImage)
{
  mng_uint8p     pNewdata;
  mng_uint8p     pSrcline1;
  mng_uint8p     pSrcline2;
  mng_uint8p     pTempline;
  mng_uint8p     pDstline;
  mng_uint32     iNewrowsize;
  mng_uint32     iNewsize;
  mng_uint32     iY;
  mng_int32      iS, iM;
  mng_retcode    iRetcode;

  mng_imagedatap pBuf      = pImage->pImgbuf;
  mng_uint32     iNewW     = pBuf->iWidth;
  mng_uint32     iNewH     = pBuf->iHeight;
  mng_magnify_x  fMagnifyX = MNG_NULL;
  mng_magnify_y  fMagnifyY = MNG_NULL;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_IMGOBJECT, MNG_LC_START)
#endif

  if (pBuf->iColortype == 3)           /* indexed color ? */
  {                                    /* concrete buffer ? */
    if ((pBuf->bConcrete) && (pImage->iId))
      MNG_ERROR (pData, MNG_INVALIDCOLORTYPE)

    if (pBuf->iTRNScount)              /* with transparency ? */
      iRetcode = promote_imageobject (pData, pImage, 8, 6, 0);
    else
      iRetcode = promote_imageobject (pData, pImage, 8, 2, 0);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
  }

  if (pImage->iMAGN_MethodX)           /* determine new width */
  {
    if (pImage->iMAGN_MethodX == 1)
    {
      iNewW   = pImage->iMAGN_ML;
      if (pBuf->iWidth  > 1)
        iNewW = iNewW + pImage->iMAGN_MR;
      if (pBuf->iWidth  > 2)
        iNewW = iNewW + (pBuf->iWidth  - 2) * (pImage->iMAGN_MX);
    }
    else
    {
      iNewW   = pBuf->iWidth  + pImage->iMAGN_ML - 1;
      if (pBuf->iWidth  > 2)
        iNewW = iNewW + pImage->iMAGN_MR - 1;
      if (pBuf->iWidth  > 3)
        iNewW = iNewW + (pBuf->iWidth  - 3) * (pImage->iMAGN_MX - 1);
    }
  }

  if (pImage->iMAGN_MethodY)           /* determine new height */
  {
    if (pImage->iMAGN_MethodY == 1)
    {
      iNewH   = pImage->iMAGN_MT;
      if (pBuf->iHeight > 1)
        iNewH = iNewH + pImage->iMAGN_ML;
      if (pBuf->iHeight > 2)
        iNewH = iNewH + (pBuf->iHeight - 2) * (pImage->iMAGN_MY);
    }
    else
    {
      iNewH   = pBuf->iHeight + pImage->iMAGN_MT - 1;
      if (pBuf->iHeight > 2)
        iNewH = iNewH + pImage->iMAGN_MB - 1;
      if (pBuf->iHeight > 3)
        iNewH = iNewH + (pBuf->iHeight - 3) * (pImage->iMAGN_MY - 1);
    }
  }
                                       /* get new buffer */
  iNewrowsize  = iNewW * pBuf->iSamplesize;
  iNewsize     = iNewH * iNewrowsize;
  
  MNG_ALLOC (pData, pNewdata, iNewsize);

  switch (pBuf->iColortype)            /* determine magnification routines */
  {
    case  0 : ;
    case  8 : {
                if (pBuf->iBitdepth <= 8)
                {
                  switch (pImage->iMAGN_MethodX)
                  {
                    case 1  : { fMagnifyX = magnify_g8_x1; break; }
                    case 2  : { fMagnifyX = magnify_g8_x2; break; }
                    case 3  : { fMagnifyX = magnify_g8_x3; break; }
                    case 4  : { fMagnifyX = magnify_g8_x2; break; }
                    case 5  : { fMagnifyX = magnify_g8_x3; break; }
                  }

                  switch (pImage->iMAGN_MethodY)
                  {
                    case 1  : { fMagnifyY = magnify_g8_y1; break; }
                    case 2  : { fMagnifyY = magnify_g8_y2; break; }
                    case 3  : { fMagnifyY = magnify_g8_y3; break; }
                    case 4  : { fMagnifyY = magnify_g8_y2; break; }
                    case 5  : { fMagnifyY = magnify_g8_y3; break; }
                  }
                }
                else
                {

                  /* TODO: magnify 16-bit */

                }

                break;
              }

    case  2 : ;
    case 10 : {
                if (pBuf->iBitdepth <= 8)
                {
                  switch (pImage->iMAGN_MethodX)
                  {
                    case 1  : { fMagnifyX = magnify_rgb8_x1; break; }
                    case 2  : { fMagnifyX = magnify_rgb8_x2; break; }
                    case 3  : { fMagnifyX = magnify_rgb8_x3; break; }
                    case 4  : { fMagnifyX = magnify_rgb8_x2; break; }
                    case 5  : { fMagnifyX = magnify_rgb8_x3; break; }
                  }

                  switch (pImage->iMAGN_MethodY)
                  {
                    case 1  : { fMagnifyY = magnify_rgb8_y1; break; }
                    case 2  : { fMagnifyY = magnify_rgb8_y2; break; }
                    case 3  : { fMagnifyY = magnify_rgb8_y3; break; }
                    case 4  : { fMagnifyY = magnify_rgb8_y2; break; }
                    case 5  : { fMagnifyY = magnify_rgb8_y3; break; }
                  }
                }
                else
                {

                  /* TODO: magnify 16-bit */

                }

                break;
              }

    case  4 : ;
    case 12 : {
                if (pBuf->iBitdepth <= 8)
                {
                  switch (pImage->iMAGN_MethodX)
                  {
                    case 1  : { fMagnifyX = magnify_ga8_x1; break; }
                    case 2  : { fMagnifyX = magnify_ga8_x2; break; }
                    case 3  : { fMagnifyX = magnify_ga8_x3; break; }
                    case 4  : { fMagnifyX = magnify_ga8_x4; break; }
                    case 5  : { fMagnifyX = magnify_ga8_x5; break; }
                  }

                  switch (pImage->iMAGN_MethodY)
                  {
                    case 1  : { fMagnifyY = magnify_ga8_y1; break; }
                    case 2  : { fMagnifyY = magnify_ga8_y2; break; }
                    case 3  : { fMagnifyY = magnify_ga8_y3; break; }
                    case 4  : { fMagnifyY = magnify_ga8_y4; break; }
                    case 5  : { fMagnifyY = magnify_ga8_y5; break; }
                  }
                }
                else
                {

                  /* TODO: magnify 16-bit */

                }

                break;
              }

    case  6 : ;
    case 14 : {
                if (pBuf->iBitdepth <= 8)
                {
                  switch (pImage->iMAGN_MethodX)
                  {
                    case 1  : { fMagnifyX = magnify_rgba8_x1; break; }
                    case 2  : { fMagnifyX = magnify_rgba8_x2; break; }
                    case 3  : { fMagnifyX = magnify_rgba8_x3; break; }
                    case 4  : { fMagnifyX = magnify_rgba8_x4; break; }
                    case 5  : { fMagnifyX = magnify_rgba8_x5; break; }
                  }

                  switch (pImage->iMAGN_MethodY)
                  {
                    case 1  : { fMagnifyY = magnify_rgba8_y1; break; }
                    case 2  : { fMagnifyY = magnify_rgba8_y2; break; }
                    case 3  : { fMagnifyY = magnify_rgba8_y3; break; }
                    case 4  : { fMagnifyY = magnify_rgba8_y4; break; }
                    case 5  : { fMagnifyY = magnify_rgba8_y5; break; }
                  }
                }
                else
                {

                  /* TODO: magnify 16-bit */

                }

                break;
              }
  }

  pSrcline1 = pBuf->pImgdata;          /* initialize row-loop variables */
  pDstline  = pNewdata;
                                       /* allocate temporary row */
  MNG_ALLOC (pData, pTempline, iNewrowsize)

  for (iY = 0; iY < pBuf->iHeight; iY++)
  {
    pSrcline2 = pSrcline1 + pBuf->iRowsize;

    if (fMagnifyX)                     /* magnifying in X-direction ? */
    {
      iRetcode = fMagnifyX (pData, pImage->iMAGN_MX,
                            pImage->iMAGN_ML, pImage->iMAGN_MR,
                            pBuf->iWidth, pSrcline1, pDstline);

      if (iRetcode)                    /* on error bail out */
      {
        MNG_FREEX (pData, pTempline, iNewrowsize)
        MNG_FREEX (pData, pNewdata,  iNewsize)
        return iRetcode;
      }
    }
    else
    {
      MNG_COPY (pDstline, pSrcline1, iNewrowsize)
    }

    pDstline += iNewrowsize;
                                       /* magnifying in Y-direction ? */
    if ((fMagnifyY) &&
        ((iY < pBuf->iHeight - 1) || (pBuf->iHeight == 1) || (pImage->iMAGN_MethodY == 1)))
    {
      if (iY == 0)                     /* first interval ? */
      {
        if (pBuf->iHeight == 1)        /* single row ? */
          pSrcline2 = MNG_NULL;

        iM = (mng_int32)pImage->iMAGN_MT;
      }
      else                             /* last interval ? */
      if (((pImage->iMAGN_MethodY == 1) && (iY == (pBuf->iHeight - 1))) ||
          ((pImage->iMAGN_MethodY != 1) && (iY == (pBuf->iHeight - 2)))    )
        iM = (mng_int32)pImage->iMAGN_MB;
      else                             /* middle interval */
        iM = (mng_int32)pImage->iMAGN_MY;

      for (iS = 1; iS < iM; iS++)
      {
        iRetcode = fMagnifyY (pData, iS, iM, pBuf->iWidth,
                              pSrcline1, pSrcline2, pTempline);

        if (iRetcode)                  /* on error bail out */
        {
          MNG_FREEX (pData, pTempline, iNewrowsize)
          MNG_FREEX (pData, pNewdata,  iNewsize)
          return iRetcode;
        }

        if (fMagnifyX)                   /* magnifying in X-direction ? */
        {
          iRetcode = fMagnifyX (pData, pImage->iMAGN_MX,
                                pImage->iMAGN_ML, pImage->iMAGN_MR,
                                pBuf->iWidth, pTempline, pDstline);

          if (iRetcode)                  /* on error bail out */
          {
            MNG_FREEX (pData, pTempline, iNewrowsize)
            MNG_FREEX (pData, pNewdata,  iNewsize)
            return iRetcode;
          }
        }
        else
        {
          MNG_COPY (pDstline, pTempline, iNewrowsize)
        }

        pDstline  += iNewrowsize;
      }
    }

    pSrcline1 += pBuf->iRowsize;
  }
                                       /* drop temporary row */
  MNG_FREEX (pData, pTempline, iNewrowsize)
                                       /* drop old pixel-data */
  MNG_FREEX (pData, pBuf->pImgdata, pBuf->iImgdatasize)

  pBuf->pImgdata     = pNewdata;       /* save new buffer dimensions */
  pBuf->iRowsize     = iNewrowsize;
  pBuf->iImgdatasize = iNewsize;
  pBuf->iWidth       = iNewW;
  pBuf->iHeight      = iNewH;

  if (pImage->iId)                     /* real object ? */
  {
    pImage->iMAGN_MethodX = 0;         /* it's done; don't do it again !!! */
    pImage->iMAGN_MethodY = 0;
    pImage->iMAGN_MX      = 0;
    pImage->iMAGN_MY      = 0;
    pImage->iMAGN_ML      = 0;
    pImage->iMAGN_MR      = 0;
    pImage->iMAGN_MT      = 0;
    pImage->iMAGN_MB      = 0;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_IMGOBJECT, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */
/* *                                                                        * */
/* * Animation-object routines                                              * */
/* *                                                                        * */
/* * these handle the animation objects used to re-run parts of a MNG.      * */
/* * eg. during LOOP or TERM processing                                     * */
/* *                                                                        * */
/* ************************************************************************** */

void add_ani_object (mng_datap          pData,
                     mng_object_headerp pObject)
{
  mng_object_headerp pLast = (mng_object_headerp)pData->pLastaniobj;

  if (pLast)                           /* link it as last in the chain */
  {
    pObject->pPrev      = pLast;
    pLast->pNext        = pObject;
  }
  else
  {
    pObject->pPrev      = MNG_NULL;    /* be on the safe side */
    pData->pFirstaniobj = pObject;
  }

  pObject->pNext        = MNG_NULL;    /* be on the safe side */
  pData->pLastaniobj    = pObject;
                                       /* keep track for jumping */
  pObject->iFramenr     = pData->iFrameseq;
  pObject->iLayernr     = pData->iLayerseq;
  pObject->iPlaytime    = pData->iFrametime;
                                       /* save restart object ? */
  if ((pData->bDisplaying) && (!pData->bRunning) && (!pData->pCurraniobj))
    pData->pCurraniobj  = pObject;

  return;
}

/* ************************************************************************** */
/* ************************************************************************** */

mng_retcode create_ani_image (mng_datap pData)
{
  mng_ani_imagep pImage;
  mng_imagep     pCurrent;
  mng_retcode    iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CREATE_ANI_IMAGE, MNG_LC_START)
#endif

  if (pData->bCacheplayback)           /* caching playback info ? */
  {
    if (pData->bHasDHDR)               /* processing delta-image ? */
      pCurrent = (mng_imagep)pData->pObjzero;
    else                               /* get the current object */
      pCurrent = (mng_imagep)pData->pCurrentobj;

    if (!pCurrent)                     /* otherwise object 0 */
      pCurrent = (mng_imagep)pData->pObjzero;
                                       /* now just clone the object !!! */
    iRetcode  = clone_imageobject (pData, 0, MNG_FALSE, pCurrent->bVisible,
                                   MNG_FALSE, MNG_FALSE, 0, 0, 0, pCurrent, &pImage);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;

    pImage->sHeader.fCleanup = free_ani_image;
    pImage->sHeader.fProcess = process_ani_image;

    add_ani_object (pData, (mng_object_headerp)pImage);
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CREATE_ANI_IMAGE, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* okido */
}

/* ************************************************************************** */

mng_retcode free_ani_image (mng_datap   pData,
                            mng_objectp pObject)
{
  mng_ani_imagep pImage = (mng_ani_imagep)pObject;
  mng_imagedatap pImgbuf = pImage->pImgbuf;
  mng_retcode    iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ANI_IMAGE, MNG_LC_START)
#endif
                                       /* unlink the image-data buffer */
  iRetcode = free_imagedataobject (pData, pImgbuf);
                                       /* drop it's own buffer */
  MNG_FREEX (pData, pImage, sizeof (mng_ani_image))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ANI_IMAGE, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */

mng_retcode process_ani_image (mng_datap   pData,
                               mng_objectp pObject)
{
  mng_retcode    iRetcode = MNG_NOERROR;
  mng_ani_imagep pImage   = (mng_imagep)pObject;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_ANI_IMAGE, MNG_LC_START)
#endif

  if (pData->bHasDHDR)                 /* processing delta-image ? */
  {
    mng_imagep pDelta = (mng_imagep)pData->pDeltaImage;

    if (!pData->iBreakpoint)           /* only execute if not broken before */
    {                                  /* make sure to process pixels as well */
      pData->bDeltaimmediate = MNG_FALSE;
                                       /* execute the delta process */
      iRetcode = execute_delta_image (pData, pDelta, (mng_imagep)pObject);

      if (iRetcode)                    /* on error bail out */
        return iRetcode;
    }
                                       /* now go and shoot it off (if required) */
    if ((pDelta->bVisible) && (pDelta->bViewable))
      iRetcode = display_image (pData, pDelta, MNG_FALSE);

    if (!pData->bTimerset)
      pData->bHasDHDR = MNG_FALSE;     /* this image signifies IEND !! */

  }
  else
  if (pData->pCurrentobj)              /* active object ? */
  {
    mng_imagep     pCurrent = (mng_imagep)pData->pCurrentobj;
    mng_imagedatap pBuf     = pCurrent->pImgbuf;

    if (!pData->iBreakpoint)           /* don't copy it again ! */
    {
      if (pBuf->iImgdatasize)          /* buffer present in active object ? */
                                       /* then drop it */
        MNG_FREE (pData, pBuf->pImgdata, pBuf->iImgdatasize)

      if (pBuf->iProfilesize)          /* iCCP profile present ? */
                                       /* then drop it */
        MNG_FREE (pData, pBuf->pProfile, pBuf->iProfilesize)
                                       /* now blatently copy the animation buffer */
      MNG_COPY (pBuf, pImage->pImgbuf, sizeof (mng_imagedata))
                                       /* copy viewability */
      pCurrent->bViewable = pImage->bViewable;

      if (pBuf->iImgdatasize)          /* sample buffer present ? */
      {                                /* then make a copy */
        MNG_ALLOC (pData, pBuf->pImgdata, pBuf->iImgdatasize)
        MNG_COPY (pBuf->pImgdata, pImage->pImgbuf->pImgdata, pBuf->iImgdatasize)
      }

      if (pBuf->iProfilesize)          /* iCCP profile present ? */
      {                                /* then make a copy */
        MNG_ALLOC (pData, pBuf->pProfile, pBuf->iProfilesize)
        MNG_COPY (pBuf->pProfile, pImage->pImgbuf->pProfile, pBuf->iProfilesize)
      }
    }
                                       /* now go and shoot it off (if required) */
    if ((pCurrent->bVisible) && (pCurrent->bViewable))
      iRetcode = display_image (pData, pCurrent, MNG_FALSE);
  }
  else
  {
    mng_imagep pObjzero = (mng_imagep)pData->pObjzero;
                                       /* overlay with object 0 status */
    pImage->bVisible  = pObjzero->bVisible;
    pImage->bViewable = pObjzero->bViewable;
    pImage->iPosx     = pObjzero->iPosx;
    pImage->iPosy     = pObjzero->iPosy;
    pImage->bClipped  = pObjzero->bClipped;
    pImage->iClipl    = pObjzero->iClipl;
    pImage->iClipr    = pObjzero->iClipr;
    pImage->iClipt    = pObjzero->iClipt;
    pImage->iClipb    = pObjzero->iClipb;
                                       /* now this should do the trick */
    if ((pImage->bVisible) && (pImage->bViewable))
      iRetcode = display_image (pData, pImage, MNG_FALSE);
  }

  if (!iRetcode)                       /* all's well ? */
  {
    if (pData->bTimerset)              /* timer break ? */
      pData->iBreakpoint = 99;         /* fictive number; no more processing needed! */
    else
      pData->iBreakpoint = 0;          /* else clear it */
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_ANI_IMAGE, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */
/* ************************************************************************** */

mng_retcode create_ani_plte (mng_datap      pData,
                             mng_uint32     iEntrycount,
                             mng_palette8ep paEntries)
{
  mng_ani_pltep pPLTE;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CREATE_ANI_PLTE, MNG_LC_START)
#endif

  if (pData->bCacheplayback)           /* caching playback info ? */
  {
    MNG_ALLOC (pData, pPLTE, sizeof (mng_ani_plte))

    pPLTE->sHeader.fCleanup = free_ani_plte;
    pPLTE->sHeader.fProcess = process_ani_plte;

    add_ani_object (pData, (mng_object_headerp)pPLTE);

    pPLTE->iEntrycount      = iEntrycount;

    MNG_COPY (pPLTE->aEntries, paEntries, sizeof (pPLTE->aEntries))
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CREATE_ANI_PLTE, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode free_ani_plte (mng_datap   pData,
                           mng_objectp pObject)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ANI_PLTE, MNG_LC_START)
#endif

  MNG_FREEX (pData, pObject, sizeof (mng_ani_plte))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ANI_PLTE, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode process_ani_plte (mng_datap   pData,
                              mng_objectp pObject)
{
  mng_ani_pltep pPLTE = (mng_ani_pltep)pObject;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_ANI_PLTE, MNG_LC_START)
#endif

  pData->bHasglobalPLTE   = MNG_TRUE;
  pData->iGlobalPLTEcount = pPLTE->iEntrycount;

  MNG_COPY (pData->aGlobalPLTEentries, pPLTE->aEntries, sizeof (pPLTE->aEntries))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_ANI_PLTE, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */
/* ************************************************************************** */

mng_retcode create_ani_trns (mng_datap    pData,
                             mng_uint32   iRawlen,
                             mng_uint8p   pRawdata)
{
  mng_ani_trnsp pTRNS;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CREATE_ANI_TRNS, MNG_LC_START)
#endif

  if (pData->bCacheplayback)           /* caching playback info ? */
  {
    MNG_ALLOC (pData, pTRNS, sizeof (mng_ani_trns))

    pTRNS->sHeader.fCleanup = free_ani_trns;
    pTRNS->sHeader.fProcess = process_ani_trns;

    add_ani_object (pData, (mng_object_headerp)pTRNS);

    pTRNS->iRawlen          = iRawlen;

    MNG_COPY (pTRNS->aRawdata, pRawdata, sizeof (pTRNS->aRawdata))
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CREATE_ANI_TRNS, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode free_ani_trns (mng_datap   pData,
                           mng_objectp pObject)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ANI_TRNS, MNG_LC_START)
#endif

  MNG_FREEX (pData, pObject, sizeof (mng_ani_trns))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ANI_TRNS, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode process_ani_trns (mng_datap   pData,
                              mng_objectp pObject)
{
  mng_ani_trnsp pTRNS = (mng_ani_trnsp)pObject;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_ANI_TRNS, MNG_LC_START)
#endif

  pData->bHasglobalTRNS    = MNG_TRUE;
  pData->iGlobalTRNSrawlen = pTRNS->iRawlen;

  MNG_COPY (pData->aGlobalTRNSrawdata, pTRNS->aRawdata, sizeof (pTRNS->aRawdata))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_ANI_TRNS, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */
/* ************************************************************************** */

mng_retcode create_ani_gama (mng_datap  pData,
                             mng_bool   bEmpty,
                             mng_uint32 iGamma)
{
  mng_ani_gamap pGAMA;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CREATE_ANI_GAMA, MNG_LC_START)
#endif

  if (pData->bCacheplayback)           /* caching playback info ? */
  {
    MNG_ALLOC (pData, pGAMA, sizeof (mng_ani_gama))

    pGAMA->sHeader.fCleanup = free_ani_gama;
    pGAMA->sHeader.fProcess = process_ani_gama;

    add_ani_object (pData, (mng_object_headerp)pGAMA);

    pGAMA->bEmpty           = bEmpty;
    pGAMA->iGamma           = iGamma;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CREATE_ANI_GAMA, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode free_ani_gama (mng_datap   pData,
                           mng_objectp pObject)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ANI_GAMA, MNG_LC_START)
#endif

  MNG_FREEX (pData, pObject, sizeof (mng_ani_gama))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ANI_GAMA, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode process_ani_gama (mng_datap   pData,
                              mng_objectp pObject)
{
  mng_ani_gamap pGAMA = (mng_ani_gamap)pObject;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_ANI_GAMA, MNG_LC_START)
#endif

  if (pGAMA->bEmpty)                   /* empty chunk ? */
  {                                    /* clear global gAMA */
    pData->bHasglobalGAMA = MNG_FALSE;
    pData->iGlobalGamma   = 0;
  }
  else
  {                                    /* set global gAMA */
    pData->bHasglobalGAMA = MNG_TRUE;
    pData->iGlobalGamma   = pGAMA->iGamma;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_ANI_GAMA, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */
/* ************************************************************************** */

mng_retcode create_ani_chrm (mng_datap  pData,
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
  mng_ani_chrmp pCHRM;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CREATE_ANI_CHRM, MNG_LC_START)
#endif

  if (pData->bCacheplayback)           /* caching playback info ? */
  {
    MNG_ALLOC (pData, pCHRM, sizeof (mng_ani_chrm))

    pCHRM->sHeader.fCleanup = free_ani_chrm;
    pCHRM->sHeader.fProcess = process_ani_chrm;

    add_ani_object (pData, (mng_object_headerp)pCHRM);

    pCHRM->bEmpty           = bEmpty;
    pCHRM->iWhitepointx     = iWhitepointx;
    pCHRM->iWhitepointy     = iWhitepointy;
    pCHRM->iRedx            = iRedx;
    pCHRM->iRedy            = iRedy;
    pCHRM->iGreenx          = iGreenx;
    pCHRM->iGreeny          = iGreeny;
    pCHRM->iBluex           = iBluex;
    pCHRM->iBluey           = iBluey;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CREATE_ANI_CHRM, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode free_ani_chrm (mng_datap   pData,
                           mng_objectp pObject)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ANI_CHRM, MNG_LC_START)
#endif

  MNG_FREEX (pData, pObject, sizeof (mng_ani_chrm))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ANI_CHRM, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode process_ani_chrm (mng_datap   pData,
                              mng_objectp pObject)
{
  mng_ani_chrmp pCHRM = (mng_ani_chrmp)pObject;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_ANI_CHRM, MNG_LC_START)
#endif

  if (pCHRM->bEmpty)                   /* empty chunk ? */
  {                                    /* clear global cHRM */
    pData->bHasglobalCHRM       = MNG_FALSE;
    pData->iGlobalWhitepointx   = 0;
    pData->iGlobalWhitepointy   = 0;
    pData->iGlobalPrimaryredx   = 0;
    pData->iGlobalPrimaryredy   = 0;
    pData->iGlobalPrimarygreenx = 0;
    pData->iGlobalPrimarygreeny = 0;
    pData->iGlobalPrimarybluex  = 0;
    pData->iGlobalPrimarybluey  = 0;
  }
  else
  {                                    /* set global cHRM */
    pData->bHasglobalCHRM       = MNG_TRUE;
    pData->iGlobalWhitepointx   = pCHRM->iWhitepointx;
    pData->iGlobalWhitepointy   = pCHRM->iWhitepointy;
    pData->iGlobalPrimaryredx   = pCHRM->iRedx;
    pData->iGlobalPrimaryredy   = pCHRM->iRedy;
    pData->iGlobalPrimarygreenx = pCHRM->iGreenx;
    pData->iGlobalPrimarygreeny = pCHRM->iGreeny;
    pData->iGlobalPrimarybluex  = pCHRM->iBluex;
    pData->iGlobalPrimarybluey  = pCHRM->iBluey;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_ANI_CHRM, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */
/* ************************************************************************** */

mng_retcode create_ani_srgb (mng_datap pData,
                             mng_bool  bEmpty,
                             mng_uint8 iRenderingintent)
{
  mng_ani_srgbp pSRGB;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CREATE_ANI_SRGB, MNG_LC_START)
#endif

  if (pData->bCacheplayback)           /* caching playback info ? */
  {
    MNG_ALLOC (pData, pSRGB, sizeof (mng_ani_srgb))

    pSRGB->sHeader.fCleanup = free_ani_srgb;
    pSRGB->sHeader.fProcess = process_ani_srgb;

    add_ani_object (pData, (mng_object_headerp)pSRGB);

    pSRGB->bEmpty           = bEmpty;
    pSRGB->iRenderingintent = iRenderingintent;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CREATE_ANI_SRGB, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode free_ani_srgb (mng_datap   pData,
                           mng_objectp pObject)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ANI_SRGB, MNG_LC_START)
#endif

  MNG_FREEX (pData, pObject, sizeof (mng_ani_srgb))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ANI_SRGB, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode process_ani_srgb (mng_datap   pData,
                              mng_objectp pObject)
{
  mng_ani_srgbp pSRGB = (mng_ani_srgbp)pObject;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_ANI_SRGB, MNG_LC_START)
#endif

  if (pSRGB->bEmpty)                   /* empty chunk ? */
  {                                    /* clear global sRGB */
    pData->bHasglobalSRGB    = MNG_FALSE;
    pData->iGlobalRendintent = 0;
  }
  else
  {                                    /* set global sRGB */
    pData->bHasglobalSRGB    = MNG_TRUE;
    pData->iGlobalRendintent = pSRGB->iRenderingintent;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_ANI_SRGB, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */
/* ************************************************************************** */

mng_retcode create_ani_iccp (mng_datap  pData,
                             mng_bool   bEmpty,
                             mng_uint32 iProfilesize,
                             mng_ptr    pProfile)
{
  mng_ani_iccpp pICCP;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CREATE_ANI_ICCP, MNG_LC_START)
#endif

  if (pData->bCacheplayback)           /* caching playback info ? */
  {
    MNG_ALLOC (pData, pICCP, sizeof (mng_ani_iccp))

    pICCP->sHeader.fCleanup = free_ani_iccp;
    pICCP->sHeader.fProcess = process_ani_iccp;

    add_ani_object (pData, (mng_object_headerp)pICCP);

    pICCP->bEmpty           = bEmpty;
    pICCP->iProfilesize     = iProfilesize;

    if (iProfilesize)
    {
      MNG_ALLOC (pData, pICCP->pProfile, iProfilesize)
      MNG_COPY (pICCP->pProfile, pProfile, iProfilesize)
    }
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CREATE_ANI_ICCP, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode free_ani_iccp (mng_datap   pData,
                           mng_objectp pObject)
{
  mng_ani_iccpp pICCP = (mng_ani_iccpp)pObject;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ANI_ICCP, MNG_LC_START)
#endif

  if (pICCP->iProfilesize)
    MNG_FREEX (pData, pICCP->pProfile, pICCP->iProfilesize)

  MNG_FREEX (pData, pObject, sizeof (mng_ani_iccp))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ANI_ICCP, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode process_ani_iccp (mng_datap   pData,
                              mng_objectp pObject)
{
  mng_ani_iccpp pICCP = (mng_ani_iccpp)pObject;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_ANI_ICCP, MNG_LC_START)
#endif

  if (pICCP->bEmpty)                   /* empty chunk ? */
  {                                    /* clear global iCCP */
    pData->bHasglobalICCP = MNG_FALSE;

    if (pData->iGlobalProfilesize)
      MNG_FREEX (pData, pData->pGlobalProfile, pData->iGlobalProfilesize)

    pData->iGlobalProfilesize = 0;
    pData->pGlobalProfile     = MNG_NULL;
  }
  else
  {                                    /* set global iCCP */
    pData->bHasglobalICCP     = MNG_TRUE;
    pData->iGlobalProfilesize = pICCP->iProfilesize;

    if (pICCP->iProfilesize)
    {
      MNG_ALLOC (pData, pData->pGlobalProfile, pICCP->iProfilesize)
      MNG_COPY (pData->pGlobalProfile, pICCP->pProfile, pICCP->iProfilesize)
    }
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_ANI_ICCP, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */
/* ************************************************************************** */

mng_retcode create_ani_bkgd (mng_datap  pData,
                             mng_uint16 iRed,
                             mng_uint16 iGreen,
                             mng_uint16 iBlue)
{
  mng_ani_bkgdp pBKGD;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CREATE_ANI_BKGD, MNG_LC_START)
#endif

  if (pData->bCacheplayback)           /* caching playback info ? */
  {
    MNG_ALLOC (pData, pBKGD, sizeof (mng_ani_bkgd))

    pBKGD->sHeader.fCleanup = free_ani_bkgd;
    pBKGD->sHeader.fProcess = process_ani_bkgd;

    add_ani_object (pData, (mng_object_headerp)pBKGD);

    pBKGD->iRed             = iRed;
    pBKGD->iGreen           = iGreen;
    pBKGD->iBlue            = iBlue;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CREATE_ANI_BKGD, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode free_ani_bkgd (mng_datap   pData,
                           mng_objectp pObject)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ANI_BKGD, MNG_LC_START)
#endif

  MNG_FREEX (pData, pObject, sizeof (mng_ani_bkgd))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ANI_BKGD, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode process_ani_bkgd (mng_datap   pData,
                              mng_objectp pObject)
{
  mng_ani_bkgdp pBKGD = (mng_ani_bkgdp)pObject;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_ANI_BKGD, MNG_LC_START)
#endif

  pData->bHasglobalBKGD   = MNG_TRUE;
  pData->iGlobalBKGDred   = pBKGD->iRed;
  pData->iGlobalBKGDgreen = pBKGD->iGreen;
  pData->iGlobalBKGDblue  = pBKGD->iBlue;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_ANI_BKGD, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */
/* ************************************************************************** */

mng_retcode create_ani_loop (mng_datap   pData,
                             mng_uint8   iLevel,
                             mng_uint32  iRepeatcount,
                             mng_uint8   iTermcond,
                             mng_uint32  iItermin,
                             mng_uint32  iItermax,
                             mng_uint32  iCount,
                             mng_uint32p pSignals)
{
  mng_ani_loopp pLOOP;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CREATE_ANI_LOOP, MNG_LC_START)
#endif

  if (pData->bCacheplayback)           /* caching playback info ? */
  {
    MNG_ALLOC (pData, pLOOP, sizeof (mng_ani_loop))

    pLOOP->sHeader.fCleanup = free_ani_loop;
    pLOOP->sHeader.fProcess = process_ani_loop;

    add_ani_object (pData, (mng_object_headerp)pLOOP);

    pLOOP->iLevel           = iLevel;
    pLOOP->iRepeatcount     = iRepeatcount;
    pLOOP->iTermcond        = iTermcond;
    pLOOP->iItermin         = iItermin;
    pLOOP->iItermax         = iItermax;
    pLOOP->iCount           = iCount;
                                         /* running counter starts with repeat_count */
    pLOOP->iRunningcount    = iRepeatcount;

    if (iCount)
    {
      MNG_ALLOC (pData, pLOOP->pSignals, (iCount << 1))
      MNG_COPY (pLOOP->pSignals, pSignals, (iCount << 1))
    }
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CREATE_ANI_LOOP, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode free_ani_loop (mng_datap   pData,
                           mng_objectp pObject)
{
  mng_ani_loopp pLOOP = (mng_ani_loopp)pObject;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ANI_LOOP, MNG_LC_START)
#endif

  if (pLOOP->iCount)                   /* drop signal buffer ? */
    MNG_FREEX (pData, pLOOP->pSignals, (pLOOP->iCount << 1))

  MNG_FREEX (pData, pObject, sizeof (mng_ani_loop))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ANI_LOOP, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode process_ani_loop (mng_datap   pData,
                              mng_objectp pObject)
{
  mng_ani_loopp pLOOP = (mng_ani_loopp)pObject;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_ANI_LOOP, MNG_LC_START)
#endif
                                       /* just reset the running counter */
  pLOOP->iRunningcount = pLOOP->iRepeatcount;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_ANI_LOOP, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */
/* ************************************************************************** */

mng_retcode create_ani_endl (mng_datap pData,
                             mng_uint8 iLevel)
{
  mng_ani_endlp pENDL;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CREATE_ANI_ENDL, MNG_LC_START)
#endif

  if (pData->bCacheplayback)           /* caching playback info ? */
  {
    MNG_ALLOC (pData, pENDL, sizeof (mng_ani_endl))

    pENDL->sHeader.fCleanup = free_ani_endl;
    pENDL->sHeader.fProcess = process_ani_endl;

    add_ani_object (pData, (mng_object_headerp)pENDL);

    pENDL->iLevel           = iLevel;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CREATE_ANI_ENDL, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode free_ani_endl (mng_datap   pData,
                           mng_objectp pObject)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ANI_ENDL, MNG_LC_START)
#endif

  MNG_FREEX (pData, pObject, sizeof (mng_ani_endl))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ANI_ENDL, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode process_ani_endl (mng_datap   pData,
                              mng_objectp pObject)
{
  mng_ani_endlp pENDL = (mng_ani_endlp)pObject;
  mng_ani_loopp pLOOP;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_ANI_ENDL, MNG_LC_START)
#endif

  if ((pData->bDisplaying) && (pData->bRunning))
  {
    pLOOP = pENDL->pLOOP;              /* determine matching LOOP */

    if (!pLOOP)                        /* haven't got it yet ? */
    {                                  /* go and look back in the list */
      pLOOP = (mng_ani_loopp)pENDL->sHeader.pPrev;

      while ((pLOOP) &&
             ((pLOOP->sHeader.fCleanup != free_ani_loop) ||
              (pLOOP->iLevel           != pENDL->iLevel)    ))
        pLOOP = pLOOP->sHeader.pPrev;
    }
                                       /* got it now ? */
    if ((pLOOP) && (pLOOP->iLevel == pENDL->iLevel))
    {
      pENDL->pLOOP = pLOOP;            /* save for next time ! */
                                       /* decrease running counter ? */
      if ((pLOOP->iRunningcount) && (pLOOP->iRunningcount < 0x7fffffffL))
        pLOOP->iRunningcount--;

      /* TODO: we're cheating out on the termination_condition,
         iteration_min, iteration_max and possible signals;
         the code is just not ready for that can of worms.... */

      if (!pLOOP->iRunningcount)       /* reached zero ? */
      {                                /* was this the outer LOOP ? */
        if (pData->pFirstaniobj == (mng_objectp)pLOOP)
          pData->bHasLOOP = MNG_FALSE;
      }
      else
      {
        if (pData->pCurraniobj)        /* was we processing objects ? */
          pData->pCurraniobj = pLOOP;  /* then restart with LOOP */
        else                           /* else restart behind LOOP !!! */
          pData->pCurraniobj = pLOOP->sHeader.pNext;
      }
    }
    else
    {
      MNG_ERROR (pData, 1234);
      /* TODO: error abort ??? */

    }
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_ANI_ENDL, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */
/* ************************************************************************** */

mng_retcode create_ani_defi (mng_datap pData)
{
  mng_ani_defip pDEFI;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CREATE_ANI_DEFI, MNG_LC_START)
#endif

  if (pData->bCacheplayback)           /* caching playback info ? */
  {
    MNG_ALLOC (pData, pDEFI, sizeof (mng_ani_defi))

    pDEFI->sHeader.fCleanup = free_ani_defi;
    pDEFI->sHeader.fProcess = process_ani_defi;

    add_ani_object (pData, (mng_object_headerp)pDEFI);

    pDEFI->iId              = pData->iDEFIobjectid;
    pDEFI->bHasdonotshow    = pData->bDEFIhasdonotshow;
    pDEFI->iDonotshow       = pData->iDEFIdonotshow;
    pDEFI->bHasconcrete     = pData->bDEFIhasconcrete;
    pDEFI->iConcrete        = pData->iDEFIconcrete;
    pDEFI->bHasloca         = pData->bDEFIhasloca;
    pDEFI->iLocax           = pData->iDEFIlocax;
    pDEFI->iLocay           = pData->iDEFIlocay;
    pDEFI->bHasclip         = pData->bDEFIhasclip;
    pDEFI->iClipl           = pData->iDEFIclipl;
    pDEFI->iClipr           = pData->iDEFIclipr;
    pDEFI->iClipt           = pData->iDEFIclipt;
    pDEFI->iClipb           = pData->iDEFIclipb;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CREATE_ANI_DEFI, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode free_ani_defi (mng_datap   pData,
                           mng_objectp pObject)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ANI_DEFI, MNG_LC_START)
#endif

  MNG_FREEX (pData, pObject, sizeof (mng_ani_defi))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ANI_DEFI, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode process_ani_defi (mng_datap   pData,
                              mng_objectp pObject)
{
  mng_ani_defip pDEFI = (mng_ani_defip)pObject;
  mng_retcode   iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_ANI_DEFI, MNG_LC_START)
#endif

  pData->iDEFIobjectid     = pDEFI->iId;
  pData->bDEFIhasdonotshow = pDEFI->bHasdonotshow;
  pData->iDEFIdonotshow    = pDEFI->iDonotshow;
  pData->bDEFIhasconcrete  = pDEFI->bHasconcrete;
  pData->iDEFIconcrete     = pDEFI->iConcrete;
  pData->bDEFIhasloca      = pDEFI->bHasloca;
  pData->iDEFIlocax        = pDEFI->iLocax;
  pData->iDEFIlocay        = pDEFI->iLocay;
  pData->bDEFIhasclip      = pDEFI->bHasclip;
  pData->iDEFIclipl        = pDEFI->iClipl;
  pData->iDEFIclipr        = pDEFI->iClipr;
  pData->iDEFIclipt        = pDEFI->iClipt;
  pData->iDEFIclipb        = pDEFI->iClipb;

  iRetcode = process_display_defi (pData);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_ANI_DEFI, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */
/* ************************************************************************** */

mng_retcode create_ani_basi (mng_datap  pData,
                             mng_uint16 iRed,
                             mng_uint16 iGreen,
                             mng_uint16 iBlue,
                             mng_bool   bHasalpha,
                             mng_uint16 iAlpha,
                             mng_uint8  iViewable)
{
  mng_ani_basip pBASI;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CREATE_ANI_BASI, MNG_LC_START)
#endif

  if (pData->bCacheplayback)           /* caching playback info ? */
  {
    MNG_ALLOC (pData, pBASI, sizeof (mng_ani_basi))

    pBASI->sHeader.fCleanup = free_ani_basi;
    pBASI->sHeader.fProcess = process_ani_basi;

    add_ani_object (pData, (mng_object_headerp)pBASI);

    pBASI->iRed             = iRed;
    pBASI->iGreen           = iGreen;
    pBASI->iBlue            = iBlue;
    pBASI->bHasalpha        = bHasalpha;
    pBASI->iAlpha           = iAlpha;
    pBASI->iViewable        = iViewable;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CREATE_ANI_BASI, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode free_ani_basi (mng_datap   pData,
                           mng_objectp pObject)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ANI_BASI, MNG_LC_START)
#endif

  MNG_FREEX (pData, pObject, sizeof (mng_ani_basi))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ANI_BASI, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode process_ani_basi (mng_datap   pData,
                              mng_objectp pObject)
{
  mng_ani_basip pBASI = (mng_ani_basip)pObject;
  mng_retcode   iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_ANI_BASI, MNG_LC_START)
#endif

  iRetcode = process_display_basi (pData, pBASI->iRed, pBASI->iGreen, pBASI->iBlue,
                                   pBASI->bHasalpha, pBASI->iAlpha, pBASI->iViewable);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_ANI_BASI, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */
/* ************************************************************************** */

mng_retcode create_ani_clon (mng_datap  pData,
                             mng_uint16 iCloneid,
                             mng_uint16 iSourceid,
                             mng_uint8  iClonetype,
                             mng_bool   bHasdonotshow,
                             mng_uint8  iDonotshow,
                             mng_uint8  iConcrete,
                             mng_bool   bHasloca,
                             mng_uint8  iLocatype,
                             mng_int32  iLocax,
                             mng_int32  iLocay)
{
  mng_ani_clonp pCLON;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CREATE_ANI_CLON, MNG_LC_START)
#endif

  if (pData->bCacheplayback)           /* caching playback info ? */
  {
    MNG_ALLOC (pData, pCLON, sizeof (mng_ani_clon))

    pCLON->sHeader.fCleanup = free_ani_clon;
    pCLON->sHeader.fProcess = process_ani_clon;

    add_ani_object (pData, (mng_object_headerp)pCLON);

    pCLON->iCloneid         = iCloneid;
    pCLON->iSourceid        = iSourceid;
    pCLON->iClonetype       = iClonetype;
    pCLON->bHasdonotshow    = bHasdonotshow;
    pCLON->iDonotshow       = iDonotshow;
    pCLON->iConcrete        = iConcrete;
    pCLON->bHasloca         = bHasloca;
    pCLON->iLocatype        = iLocatype;
    pCLON->iLocax           = iLocax;
    pCLON->iLocay           = iLocay;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CREATE_ANI_CLON, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode free_ani_clon (mng_datap   pData,
                           mng_objectp pObject)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ANI_CLON, MNG_LC_START)
#endif

  MNG_FREEX (pData, pObject, sizeof (mng_ani_clon))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ANI_CLON, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode process_ani_clon (mng_datap   pData,
                              mng_objectp pObject)
{
  mng_ani_clonp pCLON = (mng_ani_clonp)pObject;
  mng_retcode   iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_ANI_CLON, MNG_LC_START)
#endif

  iRetcode = process_display_clon (pData, pCLON->iCloneid, pCLON->iSourceid,
                                   pCLON->iClonetype, pCLON->bHasdonotshow,
                                   pCLON->iDonotshow, pCLON->iConcrete,
                                   pCLON->bHasloca, pCLON->iLocatype,
                                   pCLON->iLocax, pCLON->iLocay);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_ANI_CLON, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */
/* ************************************************************************** */

mng_retcode create_ani_back (mng_datap  pData,
                             mng_uint16 iRed,
                             mng_uint16 iGreen,
                             mng_uint16 iBlue,
                             mng_uint8  iMandatory,
                             mng_uint16 iImageid,
                             mng_uint8  iTile)
{
  mng_ani_backp pBACK;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CREATE_ANI_BACK, MNG_LC_START)
#endif

  if (pData->bCacheplayback)           /* caching playback info ? */
  {
    MNG_ALLOC (pData, pBACK, sizeof (mng_ani_back))

    pBACK->sHeader.fCleanup = free_ani_back;
    pBACK->sHeader.fProcess = process_ani_back;

    add_ani_object (pData, (mng_object_headerp)pBACK);

    pBACK->iRed             = iRed;
    pBACK->iGreen           = iGreen;
    pBACK->iBlue            = iBlue;
    pBACK->iMandatory       = iMandatory;
    pBACK->iImageid         = iImageid;
    pBACK->iTile            = iTile;
  }
  
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CREATE_ANI_BACK, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode free_ani_back (mng_datap   pData,
                           mng_objectp pObject)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ANI_BACK, MNG_LC_START)
#endif

  MNG_FREEX (pData, pObject, sizeof (mng_ani_back))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ANI_BACK, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode process_ani_back (mng_datap   pData,
                              mng_objectp pObject)
{
  mng_ani_backp pBACK = (mng_ani_backp)pObject;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_ANI_BACK, MNG_LC_START)
#endif

  pData->iBACKred       = pBACK->iRed;
  pData->iBACKgreen     = pBACK->iGreen;
  pData->iBACKblue      = pBACK->iBlue;
  pData->iBACKmandatory = pBACK->iMandatory;
  pData->iBACKimageid   = pBACK->iImageid;
  pData->iBACKtile      = pBACK->iTile;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_ANI_BACK, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */
/* ************************************************************************** */

mng_retcode create_ani_fram (mng_datap  pData,
                             mng_uint8  iFramemode,
                             mng_uint8  iChangedelay,
                             mng_uint32 iDelay,
                             mng_uint8  iChangetimeout,
                             mng_uint32 iTimeout,
                             mng_uint8  iChangeclipping,
                             mng_uint8  iCliptype,
                             mng_int32  iClipl,
                             mng_int32  iClipr,
                             mng_int32  iClipt,
                             mng_int32  iClipb)
{
  mng_ani_framp pFRAM;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CREATE_ANI_FRAM, MNG_LC_START)
#endif

  if (pData->bCacheplayback)           /* caching playback info ? */
  {
    MNG_ALLOC (pData, pFRAM, sizeof (mng_ani_fram))

    pFRAM->sHeader.fCleanup = free_ani_fram;
    pFRAM->sHeader.fProcess = process_ani_fram;

    add_ani_object (pData, (mng_object_headerp)pFRAM);

    pFRAM->iFramemode       = iFramemode;
    pFRAM->iChangedelay     = iChangedelay;
    pFRAM->iDelay           = iDelay;
    pFRAM->iChangetimeout   = iChangetimeout;
    pFRAM->iTimeout         = iTimeout;
    pFRAM->iChangeclipping  = iChangeclipping;
    pFRAM->iCliptype        = iCliptype;
    pFRAM->iClipl           = iClipl;
    pFRAM->iClipr           = iClipr;
    pFRAM->iClipt           = iClipt;
    pFRAM->iClipb           = iClipb;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CREATE_ANI_FRAM, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode free_ani_fram (mng_datap   pData,
                           mng_objectp pObject)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ANI_FRAM, MNG_LC_START)
#endif

  MNG_FREEX (pData, pObject, sizeof (mng_ani_fram))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ANI_FRAM, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode process_ani_fram (mng_datap   pData,
                              mng_objectp pObject)
{
  mng_ani_framp pFRAM = (mng_ani_framp)pObject;
  mng_retcode   iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_ANI_FRAM, MNG_LC_START)
#endif

  if (pData->iBreakpoint)              /* previously broken ? */
  {
    iRetcode           = process_display_fram2 (pData);
    pData->iBreakpoint = 0;            /* not again */
  }
  else
    iRetcode = process_display_fram (pData, pFRAM->iFramemode,
                                     pFRAM->iChangedelay, pFRAM->iDelay,
                                     pFRAM->iChangetimeout, pFRAM->iTimeout,
                                     pFRAM->iChangeclipping, pFRAM->iCliptype,
                                     pFRAM->iClipl, pFRAM->iClipr,
                                     pFRAM->iClipt, pFRAM->iClipb);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_ANI_FRAM, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */
/* ************************************************************************** */

mng_retcode create_ani_move (mng_datap  pData,
                             mng_uint16 iFirstid,
                             mng_uint16 iLastid,
                             mng_uint8  iType,
                             mng_int32  iLocax,
                             mng_int32  iLocay)
{
  mng_ani_movep pMOVE;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CREATE_ANI_MOVE, MNG_LC_START)
#endif

  if (pData->bCacheplayback)           /* caching playback info ? */
  {
    MNG_ALLOC (pData, pMOVE, sizeof (mng_ani_move))

    pMOVE->sHeader.fCleanup = free_ani_move;
    pMOVE->sHeader.fProcess = process_ani_move;

    add_ani_object (pData, (mng_object_headerp)pMOVE);

    pMOVE->iFirstid         = iFirstid;
    pMOVE->iLastid          = iLastid;
    pMOVE->iType            = iType;
    pMOVE->iLocax           = iLocax;
    pMOVE->iLocay           = iLocay;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CREATE_ANI_MOVE, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode free_ani_move (mng_datap   pData,
                           mng_objectp pObject)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ANI_MOVE, MNG_LC_START)
#endif

  MNG_FREEX (pData, pObject, sizeof (mng_ani_move))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ANI_MOVE, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode process_ani_move (mng_datap   pData,
                              mng_objectp pObject)
{
  mng_retcode   iRetcode;
  mng_ani_movep pMOVE = (mng_ani_movep)pObject;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_ANI_MOVE, MNG_LC_START)
#endif
                                       /* re-process the MOVE chunk */
  iRetcode = process_display_move (pData, pMOVE->iFirstid, pMOVE->iLastid,
                                          pMOVE->iType,
                                          pMOVE->iLocax, pMOVE->iLocay);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_ANI_MOVE, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */
/* ************************************************************************** */

mng_retcode create_ani_clip (mng_datap  pData,
                             mng_uint16 iFirstid,
                             mng_uint16 iLastid,
                             mng_uint8  iType,
                             mng_int32  iClipl,
                             mng_int32  iClipr,
                             mng_int32  iClipt,
                             mng_int32  iClipb)
{
  mng_ani_clipp pCLIP;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CREATE_ANI_CLIP, MNG_LC_START)
#endif

  if (pData->bCacheplayback)           /* caching playback info ? */
  {
    MNG_ALLOC (pData, pCLIP, sizeof (mng_ani_clip))

    pCLIP->sHeader.fCleanup = free_ani_clip;
    pCLIP->sHeader.fProcess = process_ani_clip;

    add_ani_object (pData, (mng_object_headerp)pCLIP);

    pCLIP->iFirstid         = iFirstid;
    pCLIP->iLastid          = iLastid;
    pCLIP->iType            = iType;
    pCLIP->iClipl           = iClipl;
    pCLIP->iClipr           = iClipr;
    pCLIP->iClipt           = iClipt;
    pCLIP->iClipb           = iClipb;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CREATE_ANI_CLIP, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode free_ani_clip (mng_datap   pData,
                           mng_objectp pObject)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ANI_CLIP, MNG_LC_START)
#endif

  MNG_FREEX (pData, pObject, sizeof (mng_ani_clip))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ANI_CLIP, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode process_ani_clip (mng_datap   pData,
                              mng_objectp pObject)
{
  mng_retcode   iRetcode;
  mng_ani_clipp pCLIP = (mng_ani_clipp)pObject;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_ANI_CLIP, MNG_LC_START)
#endif
                                       /* re-process the CLIP chunk */
  iRetcode = process_display_clip (pData, pCLIP->iFirstid, pCLIP->iLastid,
                                          pCLIP->iType,
                                          pCLIP->iClipl, pCLIP->iClipr,
                                          pCLIP->iClipt, pCLIP->iClipb);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_ANI_CLIP, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */
/* ************************************************************************** */

mng_retcode create_ani_show (mng_datap  pData,
                             mng_uint16 iFirstid,
                             mng_uint16 iLastid,
                             mng_uint8  iMode)
{
  mng_ani_showp pSHOW;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CREATE_ANI_SHOW, MNG_LC_START)
#endif

  if (pData->bCacheplayback)           /* caching playback info ? */
  {
    MNG_ALLOC (pData, pSHOW, sizeof (mng_ani_show))

    pSHOW->sHeader.fCleanup = free_ani_show;
    pSHOW->sHeader.fProcess = process_ani_show;

    add_ani_object (pData, (mng_object_headerp)pSHOW);

    pSHOW->iFirstid         = iFirstid;
    pSHOW->iLastid          = iLastid;
    pSHOW->iMode            = iMode;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CREATE_ANI_SHOW, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode free_ani_show (mng_datap   pData,
                           mng_objectp pObject)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ANI_SHOW, MNG_LC_START)
#endif

  MNG_FREEX (pData, pObject, sizeof (mng_ani_show))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ANI_SHOW, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode process_ani_show (mng_datap   pData,
                              mng_objectp pObject)
{
  mng_retcode   iRetcode;
  mng_ani_showp pSHOW = (mng_ani_showp)pObject;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_ANI_SHOW, MNG_LC_START)
#endif

  if (pData->iBreakpoint)              /* returning from breakpoint ? */
  {
    iRetcode           = process_display_show (pData);
  }
  else
  {                                    /* "re-run" SHOW chunk */
    pData->iSHOWmode   = pSHOW->iMode;
    pData->iSHOWfromid = pSHOW->iFirstid;
    pData->iSHOWtoid   = pSHOW->iLastid;

    iRetcode           = process_display_show (pData);
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_ANI_SHOW, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */
/* ************************************************************************** */

mng_retcode create_ani_term (mng_datap  pData,
                             mng_uint8  iTermaction,
                             mng_uint8  iIteraction,
                             mng_uint32 iDelay,
                             mng_uint32 iItermax)
{
  mng_ani_termp pTERM;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CREATE_ANI_TERM, MNG_LC_START)
#endif

  if (pData->bCacheplayback)           /* caching playback info ? */
  {
    MNG_ALLOC (pData, pTERM, sizeof (mng_ani_term))

    pTERM->sHeader.fCleanup = free_ani_term;
    pTERM->sHeader.fProcess = process_ani_term;

    add_ani_object (pData, (mng_object_headerp)pTERM);

    pTERM->iTermaction      = iTermaction;
    pTERM->iIteraction      = iIteraction;
    pTERM->iDelay           = iDelay;
    pTERM->iItermax         = iItermax;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CREATE_ANI_TERM, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode free_ani_term (mng_datap   pData,
                           mng_objectp pObject)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ANI_TERM, MNG_LC_START)
#endif

  MNG_FREEX (pData, pObject, sizeof (mng_ani_term))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ANI_TERM, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode process_ani_term (mng_datap   pData,
                              mng_objectp pObject)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_ANI_TERM, MNG_LC_START)
#endif

  /* dummy: no action required! */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_ANI_TERM, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */
/* ************************************************************************** */

mng_retcode create_ani_save (mng_datap pData)
{
  mng_ani_savep pSAVE;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CREATE_ANI_SAVE, MNG_LC_START)
#endif

  if (pData->bCacheplayback)           /* caching playback info ? */
  {
    MNG_ALLOC (pData, pSAVE, sizeof (mng_ani_save))

    pSAVE->sHeader.fCleanup = free_ani_save;
    pSAVE->sHeader.fProcess = process_ani_save;

    add_ani_object (pData, (mng_object_headerp)pSAVE);
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CREATE_ANI_SAVE, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode free_ani_save (mng_datap   pData,
                           mng_objectp pObject)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ANI_SAVE, MNG_LC_START)
#endif

  MNG_FREEX (pData, pObject, sizeof (mng_ani_save))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ANI_SAVE, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode process_ani_save (mng_datap   pData,
                              mng_objectp pObject)
{
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_ANI_SAVE, MNG_LC_START)
#endif

  iRetcode = process_display_save (pData);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_ANI_SAVE, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */
/* ************************************************************************** */

mng_retcode create_ani_seek (mng_datap pData)
{
  mng_ani_seekp pSEEK;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CREATE_ANI_SEEK, MNG_LC_START)
#endif

  if (pData->bCacheplayback)           /* caching playback info ? */
  {
    MNG_ALLOC (pData, pSEEK, sizeof (mng_ani_seek))

    pSEEK->sHeader.fCleanup = free_ani_seek;
    pSEEK->sHeader.fProcess = process_ani_seek;

    add_ani_object (pData, (mng_object_headerp)pSEEK);
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CREATE_ANI_SEEK, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode free_ani_seek (mng_datap   pData,
                           mng_objectp pObject)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ANI_SEEK, MNG_LC_START)
#endif

  MNG_FREEX (pData, pObject, sizeof (mng_ani_seek))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ANI_SEEK, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode process_ani_seek (mng_datap   pData,
                              mng_objectp pObject)
{
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_ANI_SEEK, MNG_LC_START)
#endif

  iRetcode = process_display_seek (pData);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_ANI_SEEK, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */
/* ************************************************************************** */

mng_retcode create_ani_dhdr (mng_datap  pData,
                             mng_uint16 iObjectid,
                             mng_uint8  iImagetype,
                             mng_uint8  iDeltatype,
                             mng_uint32 iBlockwidth,
                             mng_uint32 iBlockheight,
                             mng_uint32 iBlockx,
                             mng_uint32 iBlocky)
{
  mng_ani_dhdrp pDHDR;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CREATE_ANI_DHDR, MNG_LC_START)
#endif

  if (pData->bCacheplayback)           /* caching playback info ? */
  {
    MNG_ALLOC (pData, pDHDR, sizeof (mng_ani_dhdr))

    pDHDR->sHeader.fCleanup = free_ani_dhdr;
    pDHDR->sHeader.fProcess = process_ani_dhdr;

    pDHDR->iObjectid        = iObjectid;
    pDHDR->iImagetype       = iImagetype;
    pDHDR->iDeltatype       = iDeltatype;
    pDHDR->iBlockwidth      = iBlockwidth;
    pDHDR->iBlockheight     = iBlockheight;
    pDHDR->iBlockx          = iBlockx;
    pDHDR->iBlocky          = iBlocky;

    add_ani_object (pData, (mng_object_headerp)pDHDR);
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CREATE_ANI_DHDR, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode free_ani_dhdr (mng_datap   pData,
                           mng_objectp pObject)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ANI_DHDR, MNG_LC_START)
#endif

  MNG_FREEX (pData, pObject, sizeof (mng_ani_dhdr))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ANI_DHDR, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode process_ani_dhdr (mng_datap   pData,
                              mng_objectp pObject)
{
  mng_ani_dhdrp pDHDR = (mng_ani_dhdrp)pObject;
  mng_retcode   iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_ANI_DHDR, MNG_LC_START)
#endif

  pData->bHasDHDR = MNG_TRUE;          /* let everyone know we're inside a DHDR */

  iRetcode = process_display_dhdr (pData, pDHDR->iObjectid,
                                   pDHDR->iImagetype, pDHDR->iDeltatype,
                                   pDHDR->iBlockwidth, pDHDR->iBlockheight,
                                   pDHDR->iBlockx, pDHDR->iBlocky);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_ANI_DHDR, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */
/* ************************************************************************** */

mng_retcode create_ani_prom (mng_datap pData,
                             mng_uint8 iBitdepth,
                             mng_uint8 iColortype,
                             mng_uint8 iFilltype)
{
  mng_ani_promp pPROM;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CREATE_ANI_PROM, MNG_LC_START)
#endif

  if (pData->bCacheplayback)           /* caching playback info ? */
  {
    MNG_ALLOC (pData, pPROM, sizeof (mng_ani_prom))

    pPROM->sHeader.fCleanup = free_ani_prom;
    pPROM->sHeader.fProcess = process_ani_prom;

    pPROM->iBitdepth        = iBitdepth;
    pPROM->iColortype       = iColortype;
    pPROM->iFilltype        = iFilltype;

    add_ani_object (pData, (mng_object_headerp)pPROM);
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CREATE_ANI_PROM, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode free_ani_prom (mng_datap   pData,
                           mng_objectp pObject)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ANI_PROM, MNG_LC_START)
#endif

  MNG_FREEX (pData, pObject, sizeof (mng_ani_prom))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ANI_PROM, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode process_ani_prom (mng_datap   pData,
                              mng_objectp pObject)
{
  mng_ani_promp pPROM = (mng_ani_promp)pObject;
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_ANI_PROM, MNG_LC_START)
#endif

  iRetcode = process_display_prom (pData, pPROM->iBitdepth,
                                   pPROM->iColortype, pPROM->iFilltype);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_ANI_PROM, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */
/* ************************************************************************** */

mng_retcode create_ani_ipng (mng_datap pData)
{
  mng_ani_ipngp pIPNG;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CREATE_ANI_IPNG, MNG_LC_START)
#endif

  if (pData->bCacheplayback)           /* caching playback info ? */
  {
    MNG_ALLOC (pData, pIPNG, sizeof (mng_ani_ipng))

    pIPNG->sHeader.fCleanup = free_ani_ipng;
    pIPNG->sHeader.fProcess = process_ani_ipng;

    add_ani_object (pData, (mng_object_headerp)pIPNG);
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CREATE_ANI_IPNG, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode free_ani_ipng (mng_datap   pData,
                           mng_objectp pObject)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ANI_IPNG, MNG_LC_START)
#endif

  MNG_FREEX (pData, pObject, sizeof (mng_ani_ipng))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ANI_IPNG, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode process_ani_ipng (mng_datap   pData,
                              mng_objectp pObject)
{
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_ANI_IPNG, MNG_LC_START)
#endif

  iRetcode = process_display_ipng (pData);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_ANI_IPNG, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */
/* ************************************************************************** */

mng_retcode create_ani_ijng (mng_datap pData)
{
  mng_ani_ijngp pIJNG;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CREATE_ANI_IJNG, MNG_LC_START)
#endif

  if (pData->bCacheplayback)           /* caching playback info ? */
  {
    MNG_ALLOC (pData, pIJNG, sizeof (mng_ani_ijng))

    pIJNG->sHeader.fCleanup = free_ani_ijng;
    pIJNG->sHeader.fProcess = process_ani_ijng;

    add_ani_object (pData, (mng_object_headerp)pIJNG);
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CREATE_ANI_IJNG, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode free_ani_ijng (mng_datap   pData,
                           mng_objectp pObject)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ANI_IJNG, MNG_LC_START)
#endif

  MNG_FREEX (pData, pObject, sizeof (mng_ani_ijng))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ANI_IJNG, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode process_ani_ijng (mng_datap   pData,
                              mng_objectp pObject)
{
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_ANI_IJNG, MNG_LC_START)
#endif

  iRetcode = process_display_ijng (pData);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_ANI_IJNG, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */
/* ************************************************************************** */

mng_retcode create_ani_pplt (mng_datap      pData,
                             mng_uint8      iType,
                             mng_uint32     iCount,
                             mng_palette8ep paIndexentries,
                             mng_uint8p     paAlphaentries,
                             mng_uint8p     paUsedentries)
{
  mng_ani_ppltp pPPLT;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CREATE_ANI_PPLT, MNG_LC_START)
#endif

  if (pData->bCacheplayback)           /* caching playback info ? */
  {
    MNG_ALLOC (pData, pPPLT, sizeof (mng_ani_pplt))

    pPPLT->sHeader.fCleanup = free_ani_pplt;
    pPPLT->sHeader.fProcess = process_ani_pplt;

    pPPLT->iType            = iType;
    pPPLT->iCount           = iCount;

    MNG_COPY (pPPLT->aIndexentries, paIndexentries, sizeof (pPPLT->aIndexentries))
    MNG_COPY (pPPLT->aAlphaentries, paAlphaentries, sizeof (pPPLT->aAlphaentries))
    MNG_COPY (pPPLT->aUsedentries,  paUsedentries,  sizeof (pPPLT->aUsedentries ))

    add_ani_object (pData, (mng_object_headerp)pPPLT);
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CREATE_ANI_PPLT, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode free_ani_pplt (mng_datap   pData,
                           mng_objectp pObject)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ANI_PPLT, MNG_LC_START)
#endif

  MNG_FREEX (pData, pObject, sizeof (mng_ani_pplt))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ANI_PPLT, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode process_ani_pplt (mng_datap   pData,
                              mng_objectp pObject)
{
  mng_ani_ppltp pPPLT = (mng_ani_ppltp)pObject;
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_ANI_PPLT, MNG_LC_START)
#endif

  iRetcode = process_display_pplt (pData, pPPLT->iType, pPPLT->iCount,
                                   pPPLT->aIndexentries, pPPLT->aAlphaentries,
                                   pPPLT->aUsedentries);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_ANI_PPLT, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */
/* ************************************************************************** */

mng_retcode create_ani_magn (mng_datap  pData,
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
  mng_ani_magnp pMAGN;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CREATE_ANI_MAGN, MNG_LC_START)
#endif

  if (pData->bCacheplayback)           /* caching playback info ? */
  {
    MNG_ALLOC (pData, pMAGN, sizeof (mng_ani_magn))

    pMAGN->sHeader.fCleanup = free_ani_magn;
    pMAGN->sHeader.fProcess = process_ani_magn;

    pMAGN->iFirstid         = iFirstid;
    pMAGN->iLastid          = iLastid;
    pMAGN->iMethodX         = iMethodX;
    pMAGN->iMX              = iMX;
    pMAGN->iMY              = iMY;
    pMAGN->iML              = iML;
    pMAGN->iMR              = iMR;
    pMAGN->iMT              = iMT;
    pMAGN->iMB              = iMB;
    pMAGN->iMethodY         = iMethodY;

    add_ani_object (pData, (mng_object_headerp)pMAGN);
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CREATE_ANI_MAGN, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode free_ani_magn (mng_datap   pData,
                           mng_objectp pObject)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ANI_MAGN, MNG_LC_START)
#endif

  MNG_FREEX (pData, pObject, sizeof (mng_ani_magn))

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_FREE_ANI_MAGN, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode process_ani_magn (mng_datap   pData,
                              mng_objectp pObject)
{
  mng_ani_magnp pMAGN = (mng_ani_magnp)pObject;
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_ANI_MAGN, MNG_LC_START)
#endif

  iRetcode = process_display_magn (pData, pMAGN->iFirstid, pMAGN->iLastid,
                                   pMAGN->iMethodX, pMAGN->iMX, pMAGN->iMY,
                                   pMAGN->iML, pMAGN->iMR, pMAGN->iMT, pMAGN->iMB,
                                   pMAGN->iMethodY);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_ANI_MAGN, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

#endif /* MNG_INCLUDE_DISPLAY_PROCS */

/* ************************************************************************** */
/* * end of file                                                            * */
/* ************************************************************************** */

