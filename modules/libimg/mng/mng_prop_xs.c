/* ************************************************************************** */
/* *             For conditions of distribution and use,                    * */
/* *                see copyright notice in libmng.h                        * */
/* ************************************************************************** */
/* *                                                                        * */
/* * project   : libmng                                                     * */
/* * file      : mng_prop_xs.c             copyright (c) 2000 G.Juyn        * */
/* * version   : 0.9.0                                                      * */
/* *                                                                        * */
/* * purpose   : property get/set interface (implementation)                * */
/* *                                                                        * */
/* * author    : G.Juyn                                                     * */
/* * web       : http://www.3-t.com                                         * */
/* * email     : mailto:info@3-t.com                                        * */
/* *                                                                        * */
/* * comment   : implementation of the property get/set functions           * */
/* *                                                                        * */
/* * changes   : 0.5.1 - 05/08/2000 - G.Juyn                                * */
/* *             - fixed calling convention                                 * */
/* *             - changed strict-ANSI stuff                                * */
/* *             0.5.1 - 05/11/2000 - G.Juyn                                * */
/* *             - added set_outputprofile2 & set_srgbprofile2              * */
/* *             0.5.1 - 05/12/2000 - G.Juyn                                * */
/* *             - changed trace to macro for callback error-reporting      * */
/* *                                                                        * */
/* *             0.5.2 - 05/23/2000 - G.Juyn                                * */
/* *             - changed inclusion of cms-routines                        * */
/* *             0.5.2 - 05/24/2000 - G.Juyn                                * */
/* *             - added support for get/set default zlib/IJG parms         * */
/* *             0.5.2 - 05/31/2000 - G.Juyn                                * */
/* *             - fixed up punctuation (contribution by Tim Rowley)        * */
/* *             0.5.2 - 06/05/2000 - G.Juyn                                * */
/* *             - added support for RGB8_A8 canvasstyle                    * */
/* *                                                                        * */
/* *             0.5.3 - 06/21/2000 - G.Juyn                                * */
/* *             - added get/set for speedtype to facilitate testing        * */
/* *             - added get for imagelevel during processtext callback     * */
/* *             0.5.3 - 06/26/2000 - G.Juyn                                * */
/* *             - changed userdata variable to mng_ptr                     * */
/* *             0.5.3 - 06/29/2000 - G.Juyn                                * */
/* *             - fixed incompatible return-types                          * */
/* *                                                                        * */
/* ************************************************************************** */

#include "libmng.h"
#include "mng_data.h"
#include "mng_error.h"
#include "mng_trace.h"
#ifdef __BORLANDC__
#pragma hdrstop
#endif
#include "mng_cms.h"

#if defined(__BORLANDC__) && defined(MNG_STRICT_ANSI)
#pragma option -A                      /* force ANSI-C */
#endif

/* ************************************************************************** */
/* *                                                                        * */
/* *  Property set functions                                                * */
/* *                                                                        * */
/* ************************************************************************** */

mng_retcode MNG_DECL mng_set_userdata (mng_handle hHandle,
                                       mng_ptr    pUserdata)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_USERDATA, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)
  ((mng_datap)hHandle)->pUserdata = pUserdata;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_USERDATA, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_set_canvasstyle (mng_handle hHandle,
                                          mng_uint32 iStyle)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_CANVASSTYLE, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)

  switch (iStyle)
  {
    case MNG_CANVAS_RGB8    : break;
    case MNG_CANVAS_RGBA8   : break;
    case MNG_CANVAS_ARGB8   : break;
    case MNG_CANVAS_RGB8_A8 : break;
    case MNG_CANVAS_BGR8    : break;
    case MNG_CANVAS_BGRA8   : break;
    case MNG_CANVAS_ABGR8   : break;
/*    case MNG_CANVAS_RGB16   : break; */
/*    case MNG_CANVAS_RGBA16  : break; */
/*    case MNG_CANVAS_ARGB16  : break; */
/*    case MNG_CANVAS_BGR16   : break; */
/*    case MNG_CANVAS_BGRA16  : break; */
/*    case MNG_CANVAS_ABGR16  : break; */
/*    case MNG_CANVAS_INDEX8  : break; */
/*    case MNG_CANVAS_INDEXA8 : break; */
/*    case MNG_CANVAS_AINDEX8 : break; */
/*    case MNG_CANVAS_GRAY8   : break; */
/*    case MNG_CANVAS_GRAY16  : break; */
/*    case MNG_CANVAS_GRAYA8  : break; */
/*    case MNG_CANVAS_GRAYA16 : break; */
/*    case MNG_CANVAS_AGRAY8  : break; */
/*    case MNG_CANVAS_AGRAY16 : break; */
/*    case MNG_CANVAS_DX15    : break; */
/*    case MNG_CANVAS_DX16    : break; */
    default                 : { MNG_ERROR (((mng_datap)hHandle), MNG_INVALIDCNVSTYLE) }
  }

  ((mng_datap)hHandle)->iCanvasstyle = iStyle;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_CANVASSTYLE, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_set_bkgdstyle (mng_handle hHandle,
                                        mng_uint32 iStyle)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_BKGDSTYLE, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)

  switch (iStyle)                      /* alpha-modes not supported */
  {
    case MNG_CANVAS_RGB8    : break;
    case MNG_CANVAS_BGR8    : break;
/*    case MNG_CANVAS_RGB16   : break; */
/*    case MNG_CANVAS_BGR16   : break; */
/*    case MNG_CANVAS_INDEX8  : break; */
/*    case MNG_CANVAS_GRAY8   : break; */
/*    case MNG_CANVAS_GRAY16  : break; */
/*    case MNG_CANVAS_DX15    : break; */
/*    case MNG_CANVAS_DX16    : break; */
    default                 : MNG_ERROR (((mng_datap)hHandle), MNG_INVALIDCNVSTYLE)
  }

  ((mng_datap)hHandle)->iBkgdstyle = iStyle;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_BKGDSTYLE, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_set_bgcolor (mng_handle hHandle,
                                      mng_uint16 iRed,
                                      mng_uint16 iGreen,
                                      mng_uint16 iBlue)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_BGCOLOR, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)
  ((mng_datap)hHandle)->iBGred   = iRed;
  ((mng_datap)hHandle)->iBGgreen = iGreen;
  ((mng_datap)hHandle)->iBGblue  = iBlue;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_BGCOLOR, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_set_storechunks (mng_handle hHandle,
                                          mng_bool   bStorechunks)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_STORECHUNKS, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)
  ((mng_datap)hHandle)->bStorechunks = bStorechunks;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_STORECHUNKS, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

#ifdef MNG_SUPPORT_DISPLAY
mng_retcode MNG_DECL mng_set_srgb (mng_handle hHandle,
                                   mng_bool   bIssRGB)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_SRGB, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)
  ((mng_datap)hHandle)->bIssRGB = bIssRGB;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_SRGB, MNG_LC_END)
#endif

  return MNG_NOERROR;
}
#endif /* MNG_SUPPORT_DISPLAY */

/* ************************************************************************** */

#ifdef MNG_SUPPORT_DISPLAY
mng_retcode MNG_DECL mng_set_outputprofile (mng_handle hHandle,
                                            mng_pchar  zFilename)
{
#ifdef MNG_INCLUDE_LCMS
  mng_datap pData;
#endif

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_OUTPUTPROFILE, MNG_LC_START)
#endif

#ifdef MNG_INCLUDE_LCMS
  MNG_VALIDHANDLE (hHandle)

  pData = (mng_datap)hHandle;          /* address the structure */

  if (pData->hProf2)                   /* previously defined ? */
    mnglcms_freeprofile (pData->hProf2);
                                       /* allocate new CMS profile handle */
  pData->hProf2 = mnglcms_createfileprofile (zFilename);

  if (!pData->hProf2)                  /* handle error ? */
    MNG_ERRORL (pData, MNG_LCMS_NOHANDLE)
#endif /* MNG_INCLUDE_LCMS */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_OUTPUTPROFILE, MNG_LC_END)
#endif

  return MNG_NOERROR;
}
#endif /* MNG_SUPPORT_DISPLAY */

/* ************************************************************************** */

#ifdef MNG_SUPPORT_DISPLAY
mng_retcode MNG_DECL mng_set_outputprofile2 (mng_handle hHandle,
                                             mng_uint32 iProfilesize,
                                             mng_ptr    pProfile)
{
#ifdef MNG_INCLUDE_LCMS
  mng_datap pData;
#endif

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_OUTPUTPROFILE, MNG_LC_START)
#endif

#ifdef MNG_INCLUDE_LCMS
  MNG_VALIDHANDLE (hHandle)

  pData = (mng_datap)hHandle;          /* address the structure */

  if (pData->hProf2)                   /* previously defined ? */
    mnglcms_freeprofile (pData->hProf2);
                                       /* allocate new CMS profile handle */
  pData->hProf2 = mnglcms_creatememprofile (iProfilesize, pProfile);

  if (!pData->hProf2)                  /* handle error ? */
    MNG_ERRORL (pData, MNG_LCMS_NOHANDLE)
#endif /* MNG_INCLUDE_LCMS */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_OUTPUTPROFILE, MNG_LC_END)
#endif

  return MNG_NOERROR;
}
#endif /* MNG_SUPPORT_DISPLAY */

/* ************************************************************************** */

#ifdef MNG_SUPPORT_DISPLAY
mng_retcode MNG_DECL mng_set_srgbprofile (mng_handle hHandle,
                                          mng_pchar  zFilename)
{
#ifdef MNG_INCLUDE_LCMS
  mng_datap pData;
#endif

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_SRGBPROFILE, MNG_LC_START)
#endif

#ifdef MNG_INCLUDE_LCMS
  MNG_VALIDHANDLE (hHandle)

  pData = (mng_datap)hHandle;          /* address the structure */

  if (pData->hProf3)                   /* previously defined ? */
    mnglcms_freeprofile (pData->hProf3);
                                       /* allocate new CMS profile handle */
  pData->hProf3 = mnglcms_createfileprofile (zFilename);

  if (!pData->hProf3)                  /* handle error ? */
    MNG_ERRORL (pData, MNG_LCMS_NOHANDLE)
#endif /* MNG_INCLUDE_LCMS */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_SRGBPROFILE, MNG_LC_END)
#endif

  return MNG_NOERROR;
}
#endif /* MNG_SUPPORT_DISPLAY */

/* ************************************************************************** */

#ifdef MNG_SUPPORT_DISPLAY
mng_retcode MNG_DECL mng_set_srgbprofile2 (mng_handle hHandle,
                                           mng_uint32 iProfilesize,
                                           mng_ptr    pProfile)
{
#ifdef MNG_INCLUDE_LCMS
  mng_datap pData;
#endif

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_SRGBPROFILE, MNG_LC_START)
#endif

#ifdef MNG_INCLUDE_LCMS
  MNG_VALIDHANDLE (hHandle)

  pData = (mng_datap)hHandle;          /* address the structure */

  if (pData->hProf3)                   /* previously defined ? */
    mnglcms_freeprofile (pData->hProf3);
                                       /* allocate new CMS profile handle */
  pData->hProf3 = mnglcms_creatememprofile (iProfilesize, pProfile);

  if (!pData->hProf3)                  /* handle error ? */
    MNG_ERRORL (pData, MNG_LCMS_NOHANDLE)
#endif /* MNG_INCLUDE_LCMS */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_SRGBPROFILE, MNG_LC_END)
#endif

  return MNG_NOERROR;
}
#endif /* MNG_SUPPORT_DISPLAY */

/* ************************************************************************** */

mng_retcode MNG_DECL mng_set_viewgamma (mng_handle hHandle,
                                        mng_float  dGamma)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_VIEWGAMMA, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)
  ((mng_datap)hHandle)->dViewgamma = dGamma;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_VIEWGAMMA, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_set_displaygamma (mng_handle hHandle,
                                           mng_float  dGamma)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_DISPLAYGAMMA, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)
  ((mng_datap)hHandle)->dDisplaygamma = dGamma;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_DISPLAYGAMMA, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_set_dfltimggamma (mng_handle hHandle,
                                           mng_float  dGamma)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_DFLTIMGGAMMA, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)
  ((mng_datap)hHandle)->dDfltimggamma = dGamma;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_DFLTIMGGAMMA, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_set_viewgammaint (mng_handle hHandle,
                                           mng_uint32 iGamma)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_VIEWGAMMA, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)
  ((mng_datap)hHandle)->dViewgamma = (mng_float)iGamma / 100000;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_VIEWGAMMA, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_set_displaygammaint (mng_handle hHandle,
                                              mng_uint32 iGamma)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_DISPLAYGAMMA, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)
  ((mng_datap)hHandle)->dDisplaygamma = (mng_float)iGamma / 100000;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_DISPLAYGAMMA, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_set_dfltimggammaint (mng_handle hHandle,
                                              mng_uint32 iGamma)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_DFLTIMGGAMMA, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)
  ((mng_datap)hHandle)->dDfltimggamma = (mng_float)iGamma / 100000;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_DFLTIMGGAMMA, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_set_maxcanvaswidth (mng_handle hHandle,
                                             mng_uint32 iMaxwidth)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_MAXCANVASWIDTH, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)
  ((mng_datap)hHandle)->iMaxwidth = iMaxwidth;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_MAXCANVASWIDTH, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_set_maxcanvasheight (mng_handle hHandle,
                                              mng_uint32 iMaxheight)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_MAXCANVASHEIGHT, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)
  ((mng_datap)hHandle)->iMaxheight = iMaxheight;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_MAXCANVASHEIGHT, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_set_maxcanvassize (mng_handle hHandle,
                                            mng_uint32 iMaxwidth,
                                            mng_uint32 iMaxheight)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_MAXCANVASSIZE, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)
  ((mng_datap)hHandle)->iMaxwidth  = iMaxwidth;
  ((mng_datap)hHandle)->iMaxheight = iMaxheight;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_MAXCANVASSIZE, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

#ifdef MNG_INCLUDE_ZLIB
mng_retcode MNG_DECL mng_set_zlib_level (mng_handle hHandle,
                                         mng_int32  iZlevel)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_ZLIB_LEVEL, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)
  ((mng_datap)hHandle)->iZlevel = iZlevel;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_ZLIB_LEVEL, MNG_LC_END)
#endif

  return MNG_NOERROR;
}
#endif /* MNG_INCLUDE_ZLIB */

/* ************************************************************************** */

#ifdef MNG_INCLUDE_ZLIB
mng_retcode MNG_DECL mng_set_zlib_method (mng_handle hHandle,
                                          mng_int32  iZmethod)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_ZLIB_METHOD, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)
  ((mng_datap)hHandle)->iZmethod = iZmethod;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_ZLIB_METHOD, MNG_LC_END)
#endif

  return MNG_NOERROR;
}
#endif /* MNG_INCLUDE_ZLIB */

/* ************************************************************************** */

#ifdef MNG_INCLUDE_ZLIB
mng_retcode MNG_DECL mng_set_zlib_windowbits (mng_handle hHandle,
                                              mng_int32  iZwindowbits)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_ZLIB_WINDOWBITS, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)
  ((mng_datap)hHandle)->iZwindowbits = iZwindowbits;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_ZLIB_WINDOWBITS, MNG_LC_END)
#endif

  return MNG_NOERROR;
}
#endif /* MNG_INCLUDE_ZLIB */

/* ************************************************************************** */

#ifdef MNG_INCLUDE_ZLIB
mng_retcode MNG_DECL mng_set_zlib_memlevel (mng_handle hHandle,
                                            mng_int32  iZmemlevel)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_ZLIB_MEMLEVEL, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)
  ((mng_datap)hHandle)->iZmemlevel = iZmemlevel;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_ZLIB_MEMLEVEL, MNG_LC_END)
#endif

  return MNG_NOERROR;
}
#endif /* MNG_INCLUDE_ZLIB */

/* ************************************************************************** */

#ifdef MNG_INCLUDE_ZLIB
mng_retcode MNG_DECL mng_set_zlib_strategy (mng_handle hHandle,
                                            mng_int32  iZstrategy)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_ZLIB_STRATEGY, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)
  ((mng_datap)hHandle)->iZstrategy = iZstrategy;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_ZLIB_STRATEGY, MNG_LC_END)
#endif

  return MNG_NOERROR;
}
#endif /* MNG_INCLUDE_ZLIB */

/* ************************************************************************** */

#ifdef MNG_INCLUDE_ZLIB
mng_retcode MNG_DECL mng_set_zlib_maxidat (mng_handle hHandle,
                                           mng_uint32 iMaxIDAT)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_ZLIB_MAXIDAT, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)
  ((mng_datap)hHandle)->iMaxIDAT = iMaxIDAT;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_ZLIB_MAXIDAT, MNG_LC_END)
#endif

  return MNG_NOERROR;
}
#endif /* MNG_INCLUDE_ZLIB */

/* ************************************************************************** */

#ifdef MNG_INCLUDE_JNG
mng_retcode MNG_DECL mng_set_jpeg_dctmethod (mng_handle        hHandle,
                                             mngjpeg_dctmethod eJPEGdctmethod)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_JPEG_DCTMETHOD, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)
  ((mng_datap)hHandle)->eJPEGdctmethod = eJPEGdctmethod;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_JPEG_DCTMETHOD, MNG_LC_END)
#endif

  return MNG_NOERROR;
}
#endif /* MNG_INCLUDE_JNG */

/* ************************************************************************** */

#ifdef MNG_INCLUDE_JNG
mng_retcode MNG_DECL mng_set_jpeg_quality (mng_handle hHandle,
                                           mng_int32  iJPEGquality)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_JPEG_QUALITY, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)
  ((mng_datap)hHandle)->iJPEGquality = iJPEGquality;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_JPEG_QUALITY, MNG_LC_END)
#endif

  return MNG_NOERROR;
}
#endif /* MNG_INCLUDE_JNG */

/* ************************************************************************** */

#ifdef MNG_INCLUDE_JNG
mng_retcode MNG_DECL mng_set_jpeg_smoothing (mng_handle hHandle,
                                             mng_int32  iJPEGsmoothing)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_JPEG_SMOOTHING, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)
  ((mng_datap)hHandle)->iJPEGsmoothing = iJPEGsmoothing;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_JPEG_SMOOTHING, MNG_LC_END)
#endif

  return MNG_NOERROR;
}
#endif /* MNG_INCLUDE_JNG */

/* ************************************************************************** */

#ifdef MNG_INCLUDE_JNG
mng_retcode MNG_DECL mng_set_jpeg_progressive (mng_handle hHandle,
                                               mng_bool   bJPEGprogressive)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_JPEG_PROGRESSIVE, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)
  ((mng_datap)hHandle)->bJPEGcompressprogr = bJPEGprogressive;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_JPEG_PROGRESSIVE, MNG_LC_END)
#endif

  return MNG_NOERROR;
}
#endif /* MNG_INCLUDE_JNG */

/* ************************************************************************** */

#ifdef MNG_INCLUDE_JNG
mng_retcode MNG_DECL mng_set_jpeg_optimized (mng_handle hHandle,
                                             mng_bool   bJPEGoptimized)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_JPEG_OPTIMIZED, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)
  ((mng_datap)hHandle)->bJPEGcompressopt = bJPEGoptimized;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_JPEG_OPTIMIZED, MNG_LC_END)
#endif

  return MNG_NOERROR;
}
#endif /* MNG_INCLUDE_JNG */

/* ************************************************************************** */

#ifdef MNG_INCLUDE_JNG
mng_retcode MNG_DECL mng_set_jpeg_maxjdat (mng_handle hHandle,
                                           mng_uint32 iMaxJDAT)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_JPEG_MAXJDAT, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)
  ((mng_datap)hHandle)->iMaxJDAT = iMaxJDAT;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_JPEG_MAXJDAT, MNG_LC_END)
#endif

  return MNG_NOERROR;
}
#endif /* MNG_INCLUDE_JNG */

/* ************************************************************************** */

#ifdef MNG_SUPPORT_DISPLAY
mng_retcode MNG_DECL mng_set_speed (mng_handle    hHandle,
                                    mng_speedtype iSpeed)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_SPEED, MNG_LC_START)
#endif

  ((mng_datap)hHandle)->iSpeed = iSpeed;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_SET_SPEED, MNG_LC_END)
#endif

  return MNG_NOERROR;
}
#endif /* MNG_SUPPORT_DISPLAY */

/* ************************************************************************** */
/* *                                                                        * */
/* *  Property get functions                                                * */
/* *                                                                        * */
/* ************************************************************************** */

mng_ptr MNG_DECL mng_get_userdata (mng_handle hHandle)
{                            /* no tracing in here to prevent recursive calls */
  MNG_VALIDHANDLEX (hHandle)
  return ((mng_datap)hHandle)->pUserdata;
}

/* ************************************************************************** */

mng_imgtype MNG_DECL mng_get_sigtype (mng_handle hHandle)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_SIGTYPE, MNG_LC_START)
#endif

  if ((hHandle == 0) || (((mng_datap)hHandle)->iMagic != MNG_MAGIC))
    return mng_it_unknown;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_SIGTYPE, MNG_LC_END)
#endif

  return ((mng_datap)hHandle)->eSigtype;
}

/* ************************************************************************** */

mng_imgtype MNG_DECL mng_get_imagetype (mng_handle hHandle)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_IMAGETYPE, MNG_LC_START)
#endif

  if ((hHandle == 0) || (((mng_datap)hHandle)->iMagic != MNG_MAGIC))
    return mng_it_unknown;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_IMAGETYPE, MNG_LC_END)
#endif

  return ((mng_datap)hHandle)->eImagetype;
}

/* ************************************************************************** */

mng_uint32 MNG_DECL mng_get_imagewidth (mng_handle hHandle)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_IMAGEWIDTH, MNG_LC_START)
#endif

  MNG_VALIDHANDLEX (hHandle)

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_IMAGEWIDTH, MNG_LC_END)
#endif

  return ((mng_datap)hHandle)->iWidth;
}

/* ************************************************************************** */

mng_uint32 MNG_DECL mng_get_imageheight (mng_handle hHandle)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_IMAGEWIDTH, MNG_LC_START)
#endif

  MNG_VALIDHANDLEX (hHandle)

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_IMAGEHEIGHT, MNG_LC_END)
#endif

  return ((mng_datap)hHandle)->iHeight;
}

/* ************************************************************************** */

mng_uint32 MNG_DECL mng_get_ticks (mng_handle hHandle)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_TICKS, MNG_LC_START)
#endif

  MNG_VALIDHANDLEX (hHandle)

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_TICKS, MNG_LC_END)
#endif

  return ((mng_datap)hHandle)->iTicks;
}

/* ************************************************************************** */

mng_uint32 MNG_DECL mng_get_framecount (mng_handle hHandle)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_FRAMECOUNT, MNG_LC_START)
#endif

  MNG_VALIDHANDLEX (hHandle)

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_FRAMECOUNT, MNG_LC_END)
#endif

  return ((mng_datap)hHandle)->iFramecount;
}

/* ************************************************************************** */

mng_uint32 MNG_DECL mng_get_layercount (mng_handle hHandle)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_LAYERCOUNT, MNG_LC_START)
#endif

  MNG_VALIDHANDLEX (hHandle)

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_LAYERCOUNT, MNG_LC_END)
#endif

  return ((mng_datap)hHandle)->iLayercount;
}

/* ************************************************************************** */

mng_uint32 MNG_DECL mng_get_playtime (mng_handle hHandle)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_PLAYTIME, MNG_LC_START)
#endif

  MNG_VALIDHANDLEX (hHandle)

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_PLAYTIME, MNG_LC_END)
#endif

  return ((mng_datap)hHandle)->iPlaytime;
}

/* ************************************************************************** */

mng_uint32 MNG_DECL mng_get_simplicity (mng_handle hHandle)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_SIMPLICITY, MNG_LC_START)
#endif

  MNG_VALIDHANDLEX (hHandle)

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_SIMPLICITY, MNG_LC_END)
#endif

  return ((mng_datap)hHandle)->iSimplicity;
}

/* ************************************************************************** */

mng_uint32 MNG_DECL mng_get_canvasstyle (mng_handle hHandle)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_CANVASSTYLE, MNG_LC_START)
#endif

  MNG_VALIDHANDLEX (hHandle)

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_CANVASSTYLE, MNG_LC_END)
#endif

  return ((mng_datap)hHandle)->iCanvasstyle;
}

/* ************************************************************************** */

mng_uint32 MNG_DECL mng_get_bkgdstyle (mng_handle hHandle)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_BKGDSTYLE, MNG_LC_START)
#endif

  MNG_VALIDHANDLEX (hHandle)

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_BKGDSTYLE, MNG_LC_END)
#endif

  return ((mng_datap)hHandle)->iBkgdstyle;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_get_bgcolor (mng_handle  hHandle,
                                      mng_uint16* iRed,
                                      mng_uint16* iGreen,
                                      mng_uint16* iBlue)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GET_BGCOLOR, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)
  *iRed   = ((mng_datap)hHandle)->iBGred;
  *iGreen = ((mng_datap)hHandle)->iBGgreen;
  *iBlue  = ((mng_datap)hHandle)->iBGblue;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GET_BGCOLOR, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_bool MNG_DECL mng_get_storechunks (mng_handle hHandle)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEB (((mng_datap)hHandle), MNG_FN_GET_STORECHUNKS, MNG_LC_START)
#endif

  MNG_VALIDHANDLEX (hHandle)

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEB (((mng_datap)hHandle), MNG_FN_GET_STORECHUNKS, MNG_LC_END)
#endif

  return ((mng_datap)hHandle)->bStorechunks;
}

/* ************************************************************************** */

#ifdef MNG_SUPPORT_DISPLAY
mng_bool MNG_DECL mng_get_srgb (mng_handle hHandle)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEB (((mng_datap)hHandle), MNG_FN_GET_SRGB, MNG_LC_START)
#endif

  MNG_VALIDHANDLEX (hHandle)

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEB (((mng_datap)hHandle), MNG_FN_GET_SRGB, MNG_LC_END)
#endif

  return ((mng_datap)hHandle)->bIssRGB;
}
#endif /* MNG_SUPPORT_DISPLAY */

/* ************************************************************************** */

mng_float MNG_DECL mng_get_viewgamma (mng_handle hHandle)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_VIEWGAMMA, MNG_LC_START)
#endif

  MNG_VALIDHANDLEX (hHandle)

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_VIEWGAMMA, MNG_LC_END)
#endif

  return ((mng_datap)hHandle)->dViewgamma;
}

/* ************************************************************************** */

mng_float MNG_DECL mng_get_displaygamma (mng_handle hHandle)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_DISPLAYGAMMA, MNG_LC_START)
#endif

  MNG_VALIDHANDLEX (hHandle)

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_DISPLAYGAMMA, MNG_LC_END)
#endif

  return ((mng_datap)hHandle)->dDisplaygamma;
}

/* ************************************************************************** */

mng_float MNG_DECL mng_get_dfltimggamma (mng_handle hHandle)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_DFLTIMGGAMMA, MNG_LC_START)
#endif

  MNG_VALIDHANDLEX (hHandle)

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_DFLTIMGGAMMA, MNG_LC_END)
#endif

  return ((mng_datap)hHandle)->dDfltimggamma;
}

/* ************************************************************************** */

mng_uint32 MNG_DECL mng_get_viewgammaint (mng_handle hHandle)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_VIEWGAMMA, MNG_LC_START)
#endif

  MNG_VALIDHANDLEX (hHandle)

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_VIEWGAMMA, MNG_LC_END)
#endif

  return (mng_uint32)(((mng_datap)hHandle)->dViewgamma * 100000);
}

/* ************************************************************************** */

mng_uint32 MNG_DECL mng_get_displaygammaint (mng_handle hHandle)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_DISPLAYGAMMA, MNG_LC_START)
#endif

  MNG_VALIDHANDLEX (hHandle)

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_DISPLAYGAMMA, MNG_LC_END)
#endif

  return (mng_uint32)(((mng_datap)hHandle)->dDisplaygamma * 100000);
}

/* ************************************************************************** */

mng_uint32 MNG_DECL mng_get_dfltimggammaint (mng_handle hHandle)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_DFLTIMGGAMMA, MNG_LC_START)
#endif

  MNG_VALIDHANDLEX (hHandle)

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_DFLTIMGGAMMA, MNG_LC_END)
#endif

  return (mng_uint32)(((mng_datap)hHandle)->dDfltimggamma * 100000);
}

/* ************************************************************************** */

mng_uint32 MNG_DECL mng_get_maxcanvaswidth (mng_handle hHandle)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_MAXCANVASWIDTH, MNG_LC_START)
#endif

  MNG_VALIDHANDLEX (hHandle)

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_MAXCANVASWIDTH, MNG_LC_END)
#endif

  return ((mng_datap)hHandle)->iMaxwidth;
}

/* ************************************************************************** */

mng_uint32 MNG_DECL mng_get_maxcanvasheight (mng_handle hHandle)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_MAXCANVASHEIGHT, MNG_LC_START)
#endif

  MNG_VALIDHANDLEX (hHandle)

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_MAXCANVASHEIGHT, MNG_LC_END)
#endif

  return ((mng_datap)hHandle)->iMaxheight;
}

/* ************************************************************************** */

#ifdef MNG_INCLUDE_ZLIB
mng_int32 MNG_DECL mng_get_zlib_level (mng_handle hHandle)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_ZLIB_LEVEL, MNG_LC_START)
#endif

  MNG_VALIDHANDLEX (hHandle)

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_ZLIB_LEVEL, MNG_LC_END)
#endif

  return ((mng_datap)hHandle)->iZlevel;
}
#endif /* MNG_INCLUDE_ZLIB */

/* ************************************************************************** */

#ifdef MNG_INCLUDE_ZLIB
mng_int32 MNG_DECL mng_get_zlib_method (mng_handle hHandle)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_ZLIB_METHOD, MNG_LC_START)
#endif

  MNG_VALIDHANDLEX (hHandle)

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_ZLIB_METHOD, MNG_LC_END)
#endif

  return ((mng_datap)hHandle)->iZmethod;
}
#endif /* MNG_INCLUDE_ZLIB */

/* ************************************************************************** */

#ifdef MNG_INCLUDE_ZLIB
mng_int32 MNG_DECL mng_get_zlib_windowbits (mng_handle hHandle)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_ZLIB_WINDOWBITS, MNG_LC_START)
#endif

  MNG_VALIDHANDLEX (hHandle)

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_ZLIB_WINDOWBITS, MNG_LC_END)
#endif

  return ((mng_datap)hHandle)->iZwindowbits;
}
#endif /* MNG_INCLUDE_ZLIB */

/* ************************************************************************** */

#ifdef MNG_INCLUDE_ZLIB
mng_int32 MNG_DECL mng_get_zlib_memlevel (mng_handle hHandle)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_ZLIB_MEMLEVEL, MNG_LC_START)
#endif

  MNG_VALIDHANDLEX (hHandle)

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_ZLIB_MEMLEVEL, MNG_LC_END)
#endif

  return ((mng_datap)hHandle)->iZmemlevel;
}
#endif /* MNG_INCLUDE_ZLIB */

/* ************************************************************************** */

#ifdef MNG_INCLUDE_ZLIB
mng_int32 MNG_DECL mng_get_zlib_strategy (mng_handle hHandle)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_ZLIB_STRATEGY, MNG_LC_START)
#endif

  MNG_VALIDHANDLEX (hHandle)

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_ZLIB_STRATEGY, MNG_LC_END)
#endif

  return ((mng_datap)hHandle)->iZstrategy;
}
#endif /* MNG_INCLUDE_ZLIB */

/* ************************************************************************** */

#ifdef MNG_INCLUDE_ZLIB
mng_uint32 MNG_DECL mng_get_zlib_maxidat (mng_handle hHandle)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_ZLIB_MAXIDAT, MNG_LC_START)
#endif

  MNG_VALIDHANDLEX (hHandle)

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_ZLIB_MAXIDAT, MNG_LC_END)
#endif

  return ((mng_datap)hHandle)->iMaxIDAT;
}
#endif /* MNG_INCLUDE_ZLIB */

/* ************************************************************************** */

#ifdef MNG_INCLUDE_JNG
mngjpeg_dctmethod MNG_DECL mng_get_jpeg_dctmethod (mng_handle hHandle)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_JPEG_DCTMETHOD, MNG_LC_START)
#endif

  if ((hHandle == 0) || (((mng_datap)hHandle)->iMagic != MNG_MAGIC))
    return JDCT_ISLOW;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_JPEG_DCTMETHOD, MNG_LC_END)
#endif

  return ((mng_datap)hHandle)->eJPEGdctmethod;
}
#endif /* MNG_INCLUDE_JNG */

/* ************************************************************************** */

#ifdef MNG_INCLUDE_JNG
mng_int32 MNG_DECL mng_get_jpeg_quality (mng_handle hHandle)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_JPEG_QUALITY, MNG_LC_START)
#endif

  MNG_VALIDHANDLEX (hHandle)

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_JPEG_QUALITY, MNG_LC_END)
#endif

  return ((mng_datap)hHandle)->iJPEGquality;
}
#endif /* MNG_INCLUDE_JNG */

/* ************************************************************************** */

#ifdef MNG_INCLUDE_JNG
mng_int32 MNG_DECL mng_get_jpeg_smoothing (mng_handle hHandle)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_JPEG_SMOOTHING, MNG_LC_START)
#endif

  MNG_VALIDHANDLEX (hHandle)

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_JPEG_SMOOTHING, MNG_LC_END)
#endif

  return ((mng_datap)hHandle)->iJPEGsmoothing;
}
#endif /* MNG_INCLUDE_JNG */

/* ************************************************************************** */

#ifdef MNG_INCLUDE_JNG
mng_bool MNG_DECL mng_get_jpeg_progressive (mng_handle hHandle)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_JPEG_PROGRESSIVE, MNG_LC_START)
#endif

  MNG_VALIDHANDLEX (hHandle)

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_JPEG_PROGRESSIVE, MNG_LC_END)
#endif

  return ((mng_datap)hHandle)->bJPEGcompressprogr;
}
#endif /* MNG_INCLUDE_JNG */

/* ************************************************************************** */

#ifdef MNG_INCLUDE_JNG
mng_bool MNG_DECL mng_get_jpeg_optimized (mng_handle hHandle)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_JPEG_OPTIMIZED, MNG_LC_START)
#endif

  MNG_VALIDHANDLEX (hHandle)

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_JPEG_OPTIMIZED, MNG_LC_END)
#endif

  return ((mng_datap)hHandle)->bJPEGcompressopt;
}
#endif /* MNG_INCLUDE_JNG */

/* ************************************************************************** */

#ifdef MNG_INCLUDE_JNG
mng_uint32 MNG_DECL mng_get_jpeg_maxjdat (mng_handle hHandle)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_JPEG_MAXJDAT, MNG_LC_START)
#endif

  MNG_VALIDHANDLEX (hHandle)

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_JPEG_MAXJDAT, MNG_LC_END)
#endif

  return ((mng_datap)hHandle)->iMaxJDAT;
}
#endif /* MNG_INCLUDE_JNG */

/* ************************************************************************** */

#ifdef MNG_SUPPORT_DISPLAY
mng_speedtype MNG_DECL mng_get_speed (mng_handle hHandle)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_SPEED, MNG_LC_START)
#endif

  if ((hHandle == 0) || (((mng_datap)hHandle)->iMagic != MNG_MAGIC))
    return mng_st_normal;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_SPEED, MNG_LC_END)
#endif

  return ((mng_datap)hHandle)->iSpeed;
}
#endif /* MNG_SUPPORT_DISPLAY */

/* ************************************************************************** */

mng_uint32 MNG_DECL mng_get_imagelevel (mng_handle hHandle)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_IMAGELEVEL, MNG_LC_START)
#endif

  MNG_VALIDHANDLEX (hHandle)

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACEX (((mng_datap)hHandle), MNG_FN_GET_IMAGELEVEL, MNG_LC_END)
#endif

  return ((mng_datap)hHandle)->iImagelevel;
}

/* ************************************************************************** */
/* * end of file                                                            * */
/* ************************************************************************** */

