/* ************************************************************************** */
/* *             For conditions of distribution and use,                    * */
/* *                see copyright notice in libmng.h                        * */
/* ************************************************************************** */
/* *                                                                        * */
/* * project   : libmng                                                     * */
/* * file      : libmng_cms.c              copyright (c) 2000 G.Juyn        * */
/* * version   : 1.0.1                                                      * */
/* *                                                                        * */
/* * purpose   : color management routines (implementation)                 * */
/* *                                                                        * */
/* * author    : G.Juyn                                                     * */
/* * web       : http://www.3-t.com                                         * */
/* * email     : mailto:info@3-t.com                                        * */
/* *                                                                        * */
/* * comment   : implementation of the color management routines            * */
/* *                                                                        * */
/* * changes   : 0.5.1 - 05/01/2000 - G.Juyn                                * */
/* *             - B001(105795) - fixed a typo and misconception about      * */
/* *               freeing allocated gamma-table. (reported by Marti Maria) * */
/* *             0.5.1 - 05/08/2000 - G.Juyn                                * */
/* *             - changed strict-ANSI stuff                                * */
/* *             0.5.1 - 05/09/2000 - G.Juyn                                * */
/* *             - filled application-based color-management routines       * */
/* *             0.5.1 - 05/11/2000 - G.Juyn                                * */
/* *             - added creatememprofile                                   * */
/* *             - added callback error-reporting support                   * */
/* *             0.5.1 - 05/12/2000 - G.Juyn                                * */
/* *             - changed trace to macro for callback error-reporting      * */
/* *                                                                        * */
/* *             0.5.2 - 06/10/2000 - G.Juyn                                * */
/* *             - fixed some compilation-warnings (contrib Jason Morris)   * */
/* *                                                                        * */
/* *             0.5.3 - 06/21/2000 - G.Juyn                                * */
/* *             - fixed problem with color-correction for stored images    * */
/* *             0.5.3 - 06/23/2000 - G.Juyn                                * */
/* *             - fixed problem with incorrect gamma-correction            * */
/* *                                                                        * */
/* *             0.9.2 - 08/05/2000 - G.Juyn                                * */
/* *             - changed file-prefixes                                    * */
/* *                                                                        * */
/* *             0.9.3 - 08/31/2000 - G.Juyn                                * */
/* *             - fixed sRGB precedence for gamma_only corection           * */
/* *                                                                        * */
/* *             0.9.4 - 12/16/2000 - G.Juyn                                * */
/* *             - fixed mixup of data- & function-pointers (thanks Dimitri)* */
/* *                                                                        * */
/* *             1.0.1 - 03/31/2001 - G.Juyn                                * */
/* *             - ignore gamma=0 (see png-list for more info)              * */
/* *             1.0.1 - 04/25/2001 - G.Juyn (reported by Gregg Kelly)      * */
/* *             - fixed problem with cms profile being created multiple    * */
/* *               times when both iCCP & cHRM/gAMA are present             * */
/* *             1.0.1 - 04/25/2001 - G.Juyn                                * */
/* *             - moved mng_clear_cms to libmng_cms                        * */
/* *             1.0.1 - 05/02/2001 - G.Juyn                                * */
/* *             - added "default" sRGB generation (Thanks Marti!)          * */
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
#include "libmng_cms.h"

#if defined(__BORLANDC__) && defined(MNG_STRICT_ANSI)
#pragma option -A                      /* force ANSI-C */
#endif

/* ************************************************************************** */

#ifdef MNG_INCLUDE_DISPLAY_PROCS

/* ************************************************************************** */
/* *                                                                        * */
/* * Little CMS helper routines                                             * */
/* *                                                                        * */
/* ************************************************************************** */

#ifdef MNG_INCLUDE_LCMS

#define MNG_CMS_FLAGS 0

/* ************************************************************************** */

void mnglcms_initlibrary ()
{
  cmsErrorAction (LCMS_ERROR_IGNORE);  /* LCMS should ignore errors! */
}

/* ************************************************************************** */

mng_cmsprof mnglcms_createfileprofile (mng_pchar zFilename)
{
  return cmsOpenProfileFromFile (zFilename, "r");
}

/* ************************************************************************** */

mng_cmsprof mnglcms_creatememprofile (mng_uint32 iProfilesize,
                                      mng_ptr    pProfile)
{
  return cmsOpenProfileFromMem (pProfile, iProfilesize);
}

/* ************************************************************************** */

mng_cmsprof mnglcms_createsrgbprofile (void)
{
  cmsCIExyY       D65;
  cmsCIExyYTRIPLE Rec709Primaries = {
                                      {0.6400, 0.3300, 1.0},
                                      {0.3000, 0.6000, 1.0},
                                      {0.1500, 0.0600, 1.0}
                                    };
  LPGAMMATABLE    Gamma24[3];
  mng_cmsprof     hsRGB;

  cmsWhitePointFromTemp(6504, &D65);
  Gamma24[0] = Gamma24[1] = Gamma24[2] = cmsBuildGamma(256, 2.4);
  hsRGB = cmsCreateRGBProfile(&D65, &Rec709Primaries, Gamma24);
  cmsFreeGamma(Gamma24[0]);

  return hsRGB;
}

/* ************************************************************************** */

void mnglcms_freeprofile (mng_cmsprof hProf)
{
  cmsCloseProfile (hProf);
  return;
}

/* ************************************************************************** */

void mnglcms_freetransform (mng_cmstrans hTrans)
{
/* B001 start */
  cmsDeleteTransform (hTrans);
/* B001 end */
  return;
}

/* ************************************************************************** */

mng_retcode mng_clear_cms (mng_datap pData)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CLEAR_CMS, MNG_LC_START)
#endif

  if (pData->hTrans)                   /* transformation still active ? */
    mnglcms_freetransform (pData->hTrans);

  pData->hTrans = 0;

  if (pData->hProf1)                   /* file profile still active ? */
    mnglcms_freeprofile (pData->hProf1);

  pData->hProf1 = 0;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CLEAR_CMS, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

#endif /* MNG_INCLUDE_LCMS */

/* ************************************************************************** */
/* *                                                                        * */
/* * Color-management initialization & correction routines                  * */
/* *                                                                        * */
/* ************************************************************************** */

#ifdef MNG_INCLUDE_LCMS

mng_retcode init_full_cms (mng_datap pData)
{
  mng_cmsprof    hProf;
  mng_cmstrans   hTrans;
  mng_imagep     pImage = (mng_imagep)pData->pCurrentobj;
  mng_imagedatap pBuf;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_FULL_CMS, MNG_LC_START)
#endif

  if (!pImage)                         /* no current object? then use object 0 */
    pImage = (mng_imagep)pData->pObjzero;

  pBuf = pImage->pImgbuf;              /* address the buffer */ 

  if ((pBuf->bHasICCP) || (pData->bHasglobalICCP))
  {
    if (!pData->hProf2)                /* output profile not defined ? */
    {                                  /* then assume sRGB !! */
      pData->hProf2 = mnglcms_createsrgbprofile ();

      if (!pData->hProf2)              /* handle error ? */
        MNG_ERRORL (pData, MNG_LCMS_NOHANDLE)
    }

    if (pBuf->bHasICCP)                /* generate a profile handle */
      hProf = cmsOpenProfileFromMem (pBuf->pProfile, pBuf->iProfilesize);
    else
      hProf = cmsOpenProfileFromMem (pData->pGlobalProfile, pData->iGlobalProfilesize);

    pData->hProf1 = hProf;             /* save for future use */

    if (!hProf)                        /* handle error ? */
      MNG_ERRORL (pData, MNG_LCMS_NOHANDLE)

    if (pData->bIsRGBA16)              /* 16-bit intermediates ? */
      hTrans = cmsCreateTransform (hProf,         TYPE_RGBA_16_SE,
                                   pData->hProf2, TYPE_RGBA_16_SE,
                                   INTENT_PERCEPTUAL, MNG_CMS_FLAGS);
    else
      hTrans = cmsCreateTransform (hProf,         TYPE_RGBA_8,
                                   pData->hProf2, TYPE_RGBA_8,
                                   INTENT_PERCEPTUAL, MNG_CMS_FLAGS);

    pData->hTrans = hTrans;            /* save for future use */

    if (!hTrans)                       /* handle error ? */
      MNG_ERRORL (pData, MNG_LCMS_NOTRANS)
                                       /* load color-correction routine */
    pData->fCorrectrow = (mng_fptr)correct_full_cms;

    return MNG_NOERROR;                /* and done */
  }
  else
  if ((pBuf->bHasSRGB) || (pData->bHasglobalSRGB))
  {
    mng_uint8 iIntent;

    if (pData->bIssRGB)                /* sRGB system ? */
      return MNG_NOERROR;              /* no conversion required */

    if (!pData->hProf3)                /* sRGB profile not defined ? */
    {                                  /* then create it implicitly !! */
      pData->hProf3 = mnglcms_createsrgbprofile ();

      if (!pData->hProf3)              /* handle error ? */
        MNG_ERRORL (pData, MNG_LCMS_NOHANDLE)
    }

    hProf = pData->hProf3;             /* convert from sRGB profile */

    if (pBuf->bHasSRGB)                /* determine rendering intent */
      iIntent = pBuf->iRenderingintent;
    else
      iIntent = pData->iGlobalRendintent;

    if (pData->bIsRGBA16)              /* 16-bit intermediates ? */
      hTrans = cmsCreateTransform (hProf,         TYPE_RGBA_16_SE,
                                   pData->hProf2, TYPE_RGBA_16_SE,
                                   iIntent, MNG_CMS_FLAGS);
    else
      hTrans = cmsCreateTransform (hProf,         TYPE_RGBA_8,
                                   pData->hProf2, TYPE_RGBA_8,
                                   iIntent, MNG_CMS_FLAGS);

    pData->hTrans = hTrans;            /* save for future use */

    if (!hTrans)                       /* handle error ? */
      MNG_ERRORL (pData, MNG_LCMS_NOTRANS)
                                       /* load color-correction routine */
    pData->fCorrectrow = (mng_fptr)correct_full_cms;

    return MNG_NOERROR;                /* and done */
  }
  else
  if ( ((pBuf->bHasCHRM) || (pData->bHasglobalCHRM)) &&
       ( ((pBuf->bHasGAMA)        && (pBuf->iGamma        > 0)) ||
         ((pData->bHasglobalGAMA) && (pData->iGlobalGamma > 0))    ))
  {
    mng_CIExyY       sWhitepoint;
    mng_CIExyYTRIPLE sPrimaries;
    mng_gammatabp    pGammatable[3];
    mng_float        dGamma;

    if (!pData->hProf2)                /* output profile not defined ? */
    {                                  /* then assume sRGB !! */
      pData->hProf2 = mnglcms_createsrgbprofile ();

      if (!pData->hProf2)              /* handle error ? */
        MNG_ERRORL (pData, MNG_LCMS_NOHANDLE)
    }

    if (pBuf->bHasCHRM)                /* local cHRM ? */
    {
      sWhitepoint.x      = (mng_float)pBuf->iWhitepointx   / 100000;
      sWhitepoint.y      = (mng_float)pBuf->iWhitepointy   / 100000;
      sPrimaries.Red.x   = (mng_float)pBuf->iPrimaryredx   / 100000;
      sPrimaries.Red.y   = (mng_float)pBuf->iPrimaryredy   / 100000;
      sPrimaries.Green.x = (mng_float)pBuf->iPrimarygreenx / 100000;
      sPrimaries.Green.y = (mng_float)pBuf->iPrimarygreeny / 100000;
      sPrimaries.Blue.x  = (mng_float)pBuf->iPrimarybluex  / 100000;
      sPrimaries.Blue.y  = (mng_float)pBuf->iPrimarybluey  / 100000;
    }
    else
    {
      sWhitepoint.x      = (mng_float)pData->iGlobalWhitepointx   / 100000;
      sWhitepoint.y      = (mng_float)pData->iGlobalWhitepointy   / 100000;
      sPrimaries.Red.x   = (mng_float)pData->iGlobalPrimaryredx   / 100000;
      sPrimaries.Red.y   = (mng_float)pData->iGlobalPrimaryredy   / 100000;
      sPrimaries.Green.x = (mng_float)pData->iGlobalPrimarygreenx / 100000;
      sPrimaries.Green.y = (mng_float)pData->iGlobalPrimarygreeny / 100000;
      sPrimaries.Blue.x  = (mng_float)pData->iGlobalPrimarybluex  / 100000;
      sPrimaries.Blue.y  = (mng_float)pData->iGlobalPrimarybluey  / 100000;
    }

    sWhitepoint.Y      =               /* Y component is always 1.0 */
    sPrimaries.Red.Y   =
    sPrimaries.Green.Y =
    sPrimaries.Blue.Y  = 1.0;

    if (pBuf->bHasGAMA)                /* get the gamma value */
      dGamma = (mng_float)pBuf->iGamma / 100000;
    else
      dGamma = (mng_float)pData->iGlobalGamma / 100000;

/*    dGamma = pData->dViewgamma / (dGamma * pData->dDisplaygamma); ??? */
    dGamma = pData->dViewgamma / dGamma;

    pGammatable [0] =                  /* and build the lookup tables */
    pGammatable [1] =
    pGammatable [2] = cmsBuildGamma (256, dGamma);

/* B001 start */
    if (!pGammatable [0])              /* enough memory ? */
/* B001 end */
      MNG_ERRORL (pData, MNG_LCMS_NOMEM)
                                       /* create the profile */
    hProf = cmsCreateRGBProfile (&sWhitepoint, &sPrimaries, pGammatable);

/* B001 start */
    cmsFreeGamma (pGammatable [0]);    /* free the temporary gamma tables ? */
                                       /* yes! but just the one! */
/* B001 end */

    pData->hProf1 = hProf;             /* save for future use */

    if (!hProf)                        /* handle error ? */
      MNG_ERRORL (pData, MNG_LCMS_NOHANDLE)

    if (pData->bIsRGBA16)              /* 16-bit intermediates ? */
      hTrans = cmsCreateTransform (hProf,         TYPE_RGBA_16_SE,
                                   pData->hProf2, TYPE_RGBA_16_SE,
                                   INTENT_PERCEPTUAL, MNG_CMS_FLAGS);
    else
      hTrans = cmsCreateTransform (hProf,         TYPE_RGBA_8,
                                   pData->hProf2, TYPE_RGBA_8,
                                   INTENT_PERCEPTUAL, MNG_CMS_FLAGS);

    pData->hTrans = hTrans;            /* save for future use */

    if (!hTrans)                       /* handle error ? */
      MNG_ERRORL (pData, MNG_LCMS_NOTRANS)
                                       /* load color-correction routine */
    pData->fCorrectrow = (mng_fptr)correct_full_cms;

    return MNG_NOERROR;                /* and done */
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_FULL_CMS, MNG_LC_END)
#endif

  return init_gamma_only (pData);      /* if we get here, we'll only do gamma */
}
#endif /* MNG_INCLUDE_LCMS */

/* ************************************************************************** */

#ifdef MNG_INCLUDE_LCMS

mng_retcode init_full_cms_object (mng_datap pData)
{
  mng_cmsprof    hProf;
  mng_cmstrans   hTrans;
  mng_imagedatap pBuf;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_FULL_CMS_OBJ, MNG_LC_START)
#endif
                                       /* address the object-buffer */
  pBuf = ((mng_imagep)pData->pRetrieveobj)->pImgbuf;

  if (pBuf->bHasICCP)
  {
    if (!pData->hProf2)                /* output profile not defined ? */
    {                                  /* then assume sRGB !! */
      pData->hProf2 = mnglcms_createsrgbprofile ();

      if (!pData->hProf2)              /* handle error ? */
        MNG_ERRORL (pData, MNG_LCMS_NOHANDLE)
    }
                                       /* generate a profile handle */
    hProf = cmsOpenProfileFromMem (pBuf->pProfile, pBuf->iProfilesize);

    pData->hProf1 = hProf;             /* save for future use */

    if (!hProf)                        /* handle error ? */
      MNG_ERRORL (pData, MNG_LCMS_NOHANDLE)

    if (pData->bIsRGBA16)              /* 16-bit intermediates ? */
      hTrans = cmsCreateTransform (hProf,         TYPE_RGBA_16_SE,
                                   pData->hProf2, TYPE_RGBA_16_SE,
                                   INTENT_PERCEPTUAL, MNG_CMS_FLAGS);
    else
      hTrans = cmsCreateTransform (hProf,         TYPE_RGBA_8,
                                   pData->hProf2, TYPE_RGBA_8,
                                   INTENT_PERCEPTUAL, MNG_CMS_FLAGS);

    pData->hTrans = hTrans;            /* save for future use */

    if (!hTrans)                       /* handle error ? */
      MNG_ERRORL (pData, MNG_LCMS_NOTRANS)
                                       /* load color-correction routine */
    pData->fCorrectrow = (mng_fptr)correct_full_cms;

    return MNG_NOERROR;                /* and done */
  }
  else
  if (pBuf->bHasSRGB)
  {
    if (pData->bIssRGB)                /* sRGB system ? */
      return MNG_NOERROR;              /* no conversion required */

    if (!pData->hProf3)                /* sRGB profile not defined ? */
    {                                  /* then create it implicitly !! */
      pData->hProf3 = mnglcms_createsrgbprofile ();

      if (!pData->hProf3)              /* handle error ? */
        MNG_ERRORL (pData, MNG_LCMS_NOHANDLE)
    }

    hProf = pData->hProf3;             /* convert from sRGB profile */

    if (pData->bIsRGBA16)              /* 16-bit intermediates ? */
      hTrans = cmsCreateTransform (hProf,         TYPE_RGBA_16_SE,
                                   pData->hProf2, TYPE_RGBA_16_SE,
                                   pBuf->iRenderingintent, MNG_CMS_FLAGS);
    else
      hTrans = cmsCreateTransform (hProf,         TYPE_RGBA_8,
                                   pData->hProf2, TYPE_RGBA_8,
                                   pBuf->iRenderingintent, MNG_CMS_FLAGS);

    pData->hTrans = hTrans;            /* save for future use */

    if (!hTrans)                       /* handle error ? */
      MNG_ERRORL (pData, MNG_LCMS_NOTRANS)
                                       /* load color-correction routine */
    pData->fCorrectrow = (mng_fptr)correct_full_cms;

    return MNG_NOERROR;                /* and done */
  }
  else
  if ((pBuf->bHasCHRM) && (pBuf->bHasGAMA) && (pBuf->iGamma > 0))
  {
    mng_CIExyY       sWhitepoint;
    mng_CIExyYTRIPLE sPrimaries;
    mng_gammatabp    pGammatable[3];
    mng_float        dGamma;

    if (!pData->hProf2)                /* output profile not defined ? */
    {                                  /* then assume sRGB !! */
      pData->hProf2 = mnglcms_createsrgbprofile ();

      if (!pData->hProf2)              /* handle error ? */
        MNG_ERRORL (pData, MNG_LCMS_NOHANDLE)
    }

    sWhitepoint.x      = (mng_float)pBuf->iWhitepointx   / 100000;
    sWhitepoint.y      = (mng_float)pBuf->iWhitepointy   / 100000;
    sPrimaries.Red.x   = (mng_float)pBuf->iPrimaryredx   / 100000;
    sPrimaries.Red.y   = (mng_float)pBuf->iPrimaryredy   / 100000;
    sPrimaries.Green.x = (mng_float)pBuf->iPrimarygreenx / 100000;
    sPrimaries.Green.y = (mng_float)pBuf->iPrimarygreeny / 100000;
    sPrimaries.Blue.x  = (mng_float)pBuf->iPrimarybluex  / 100000;
    sPrimaries.Blue.y  = (mng_float)pBuf->iPrimarybluey  / 100000;

    sWhitepoint.Y      =               /* Y component is always 1.0 */
    sPrimaries.Red.Y   =
    sPrimaries.Green.Y =
    sPrimaries.Blue.Y  = 1.0;

    dGamma = (mng_float)pBuf->iGamma / 100000;

/*    dGamma = pData->dViewgamma / (dGamma * pData->dDisplaygamma); ??? */
    dGamma = pData->dViewgamma / dGamma;

    pGammatable [0] =                  /* and build the lookup tables */
    pGammatable [1] =
    pGammatable [2] = cmsBuildGamma (256, dGamma);

/* B001 start */
    if (!pGammatable [0])              /* enough memory ? */
/* B001 end */
      MNG_ERRORL (pData, MNG_LCMS_NOMEM)

                                       /* create the profile */
    hProf = cmsCreateRGBProfile (&sWhitepoint, &sPrimaries, pGammatable);

/* B001 start */
    cmsFreeGamma (pGammatable [0]);    /* free the temporary gamma tables ? */
                                       /* yes! but just the one! */
/* B001 end */

    pData->hProf1 = hProf;             /* save for future use */

    if (!hProf)                        /* handle error ? */
      MNG_ERRORL (pData, MNG_LCMS_NOHANDLE)

    if (pData->bIsRGBA16)              /* 16-bit intermediates ? */
      hTrans = cmsCreateTransform (hProf,         TYPE_RGBA_16_SE,
                                   pData->hProf2, TYPE_RGBA_16_SE,
                                   INTENT_PERCEPTUAL, MNG_CMS_FLAGS);
    else
      hTrans = cmsCreateTransform (hProf,         TYPE_RGBA_8,
                                   pData->hProf2, TYPE_RGBA_8,
                                   INTENT_PERCEPTUAL, MNG_CMS_FLAGS);

    pData->hTrans = hTrans;            /* save for future use */

    if (!hTrans)                       /* handle error ? */
      MNG_ERRORL (pData, MNG_LCMS_NOTRANS)
                                       /* load color-correction routine */
    pData->fCorrectrow = (mng_fptr)correct_full_cms;

    return MNG_NOERROR;                /* and done */
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_FULL_CMS_OBJ, MNG_LC_END)
#endif
                                       /* if we get here, we'll only do gamma */
  return init_gamma_only_object (pData);
}
#endif /* MNG_INCLUDE_LCMS */

/* ************************************************************************** */

#ifdef MNG_INCLUDE_LCMS
mng_retcode correct_full_cms (mng_datap pData)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CORRECT_FULL_CMS, MNG_LC_START)
#endif

  cmsDoTransform (pData->hTrans, pData->pRGBArow, pData->pRGBArow, pData->iRowsamples);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CORRECT_FULL_CMS, MNG_LC_END)
#endif

  return MNG_NOERROR;
}
#endif /* MNG_INCLUDE_LCMS */

/* ************************************************************************** */

#if defined(MNG_GAMMA_ONLY) || defined(MNG_FULL_CMS)
mng_retcode init_gamma_only (mng_datap pData)
{
  mng_float      dGamma;
  mng_imagep     pImage = (mng_imagep)pData->pCurrentobj;
  mng_imagedatap pBuf;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_GAMMA_ONLY, MNG_LC_START)
#endif

  if (!pImage)                         /* no current object? then use object 0 */
    pImage = (mng_imagep)pData->pObjzero;

  pBuf = pImage->pImgbuf;              /* address the buffer */

  if (pBuf->bHasSRGB)                  /* get the gamma value */
    dGamma = 0.45455;
  else
  if (pBuf->bHasGAMA)
    dGamma = (mng_float)pBuf->iGamma / 100000;
  else
  if (pData->bHasglobalSRGB)
    dGamma = 0.45455;
  else
  if (pData->bHasglobalGAMA)
    dGamma = (mng_float)pData->iGlobalGamma / 100000;
  else
    dGamma = pData->dDfltimggamma;

  if (dGamma > 0)                      /* ignore gamma=0 */
  {
    dGamma = pData->dViewgamma / (dGamma * pData->dDisplaygamma);

    if (dGamma != pData->dLastgamma)   /* lookup table needs to be computed ? */
    {
      mng_int32 iX;

      pData->aGammatab [0] = 0;

      for (iX = 1; iX <= 255; iX++)
        pData->aGammatab [iX] = (mng_uint8)(pow (iX / 255.0, dGamma) * 255 + 0.5);

      pData->dLastgamma = dGamma;      /* keep for next time */
    }
                                       /* load color-correction routine */
    pData->fCorrectrow = (mng_fptr)correct_gamma_only;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_GAMMA_ONLY, MNG_LC_END)
#endif

  return MNG_NOERROR;
}
#endif /* MNG_GAMMA_ONLY || MNG_FULL_CMS */

/* ************************************************************************** */

#if defined(MNG_GAMMA_ONLY) || defined(MNG_FULL_CMS)
mng_retcode init_gamma_only_object (mng_datap pData)
{
  mng_float      dGamma;
  mng_imagedatap pBuf;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_GAMMA_ONLY_OBJ, MNG_LC_START)
#endif
                                       /* address the object-buffer */
  pBuf = ((mng_imagep)pData->pRetrieveobj)->pImgbuf;

  if (pBuf->bHasSRGB)                  /* get the gamma value */
    dGamma = 0.45455;
  else
  if (pBuf->bHasGAMA)
    dGamma = (mng_float)pBuf->iGamma / 100000;
  else
    dGamma = pData->dDfltimggamma;

  if (dGamma)                          /* lets not divide by zero, shall we... */
    dGamma = pData->dViewgamma / (dGamma * pData->dDisplaygamma);

  if (dGamma != pData->dLastgamma)     /* lookup table needs to be computed ? */
  {
    mng_int32 iX;

    pData->aGammatab [0] = 0;

    for (iX = 1; iX <= 255; iX++)
      pData->aGammatab [iX] = (mng_uint8)(pow (iX / 255.0, dGamma) * 255 + 0.5);

    pData->dLastgamma = dGamma;        /* keep for next time */
  }
                                       /* load color-correction routine */
  pData->fCorrectrow = (mng_fptr)correct_gamma_only;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_GAMMA_ONLY_OBJ, MNG_LC_END)
#endif

  return MNG_NOERROR;
}
#endif /* MNG_GAMMA_ONLY || MNG_FULL_CMS */

/* ************************************************************************** */

#if defined(MNG_GAMMA_ONLY) || defined(MNG_FULL_CMS)
mng_retcode correct_gamma_only (mng_datap pData)
{
  mng_uint8p pWork;
  mng_int32  iX;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CORRECT_GAMMA_ONLY, MNG_LC_START)
#endif

  pWork = pData->pRGBArow;             /* address intermediate row */

  if (pData->bIsRGBA16)                /* 16-bit intermediate row ? */
  {

  
     /* TODO: 16-bit precision gamma processing */
     /* we'll just do the high-order byte for now */

     
                                       /* convert all samples in the row */
     for (iX = 0; iX < pData->iRowsamples; iX++)
     {                                 /* using the precalculated gamma lookup table */
       *pWork     = pData->aGammatab [*pWork];
       *(pWork+2) = pData->aGammatab [*(pWork+2)];
       *(pWork+4) = pData->aGammatab [*(pWork+4)];

       pWork += 8;
     }
  }
  else
  {                                    /* convert all samples in the row */
     for (iX = 0; iX < pData->iRowsamples; iX++)
     {                                 /* using the precalculated gamma lookup table */
       *pWork     = pData->aGammatab [*pWork];
       *(pWork+1) = pData->aGammatab [*(pWork+1)];
       *(pWork+2) = pData->aGammatab [*(pWork+2)];

       pWork += 4;
     }
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CORRECT_GAMMA_ONLY, MNG_LC_END)
#endif

  return MNG_NOERROR;
}
#endif /* MNG_GAMMA_ONLY || MNG_FULL_CMS */

/* ************************************************************************** */

#ifdef MNG_APP_CMS
mng_retcode init_app_cms (mng_datap pData)
{
  mng_imagedatap pBuf = ((mng_imagep)pData->pObjzero)->pImgbuf;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_APP_CMS, MNG_LC_START)
#endif

  if ( (pData->fProcessiccp) &&
       ((pBuf->bHasICCP) || (pData->bHasglobalICCP)) )
  {
    mng_uint32 iProfilesize;
    mng_ptr    pProfile;

    if (pBuf->bHasICCP)                /* get the right profile */
    {
      iProfilesize = pBuf->iProfilesize;
      pProfile     = pBuf->pProfile;
    }
    else
    {
      iProfilesize = pData->iGlobalProfilesize;
      pProfile     = pData->pGlobalProfile;
    }
                                       /* inform the app */
    if (!pData->fProcessiccp ((mng_handle)pData, iProfilesize, pProfile))
      MNG_ERROR (pData, MNG_APPCMSERROR)
                                       /* load color-correction routine */
    pData->fCorrectrow = (mng_fptr)correct_app_cms;
  }

  if ( (pData->fProcesssrgb) &&
       ((pBuf->bHasSRGB) || (pData->bHasglobalSRGB)) )
  {
    mng_uint8 iIntent;

    if (pBuf->bHasSRGB)                /* determine rendering intent */
      iIntent = pBuf->iRenderingintent;
    else
      iIntent = pData->iGlobalRendintent;
                                       /* inform the app */
    if (!pData->fProcesssrgb ((mng_handle)pData, iIntent))
      MNG_ERROR (pData, MNG_APPCMSERROR)
                                       /* load color-correction routine */
    pData->fCorrectrow = (mng_fptr)correct_app_cms;
  }

  if ( (pData->fProcesschroma) &&
       ( ((pBuf->bHasCHRM) || (pData->bHasglobalCHRM)) ) )
  {
    mng_uint32 iWhitepointx,   iWhitepointy;
    mng_uint32 iPrimaryredx,   iPrimaryredy;
    mng_uint32 iPrimarygreenx, iPrimarygreeny;
    mng_uint32 iPrimarybluex,  iPrimarybluey;

    if (pBuf->bHasCHRM)                /* local cHRM ? */
    {
      iWhitepointx   = pBuf->iWhitepointx;
      iWhitepointy   = pBuf->iWhitepointy;
      iPrimaryredx   = pBuf->iPrimaryredx;
      iPrimaryredy   = pBuf->iPrimaryredy;
      iPrimarygreenx = pBuf->iPrimarygreenx;
      iPrimarygreeny = pBuf->iPrimarygreeny;
      iPrimarybluex  = pBuf->iPrimarybluex;
      iPrimarybluey  = pBuf->iPrimarybluey;  
    }
    else
    {
      iWhitepointx   = pData->iGlobalWhitepointx;
      iWhitepointy   = pData->iGlobalWhitepointy;
      iPrimaryredx   = pData->iGlobalPrimaryredx;
      iPrimaryredy   = pData->iGlobalPrimaryredy;
      iPrimarygreenx = pData->iGlobalPrimarygreenx;
      iPrimarygreeny = pData->iGlobalPrimarygreeny;
      iPrimarybluex  = pData->iGlobalPrimarybluex;
      iPrimarybluey  = pData->iGlobalPrimarybluey;
    }
                                       /* inform the app */
    if (!pData->fProcesschroma ((mng_handle)pData, iWhitepointx,   iWhitepointy,
                                                   iPrimaryredx,   iPrimaryredy,
                                                   iPrimarygreenx, iPrimarygreeny,
                                                   iPrimarybluex,  iPrimarybluey))
      MNG_ERROR (pData, MNG_APPCMSERROR)
                                       /* load color-correction routine */
    pData->fCorrectrow = (mng_fptr)correct_app_cms;
  }

  if ( (pData->fProcessgamma) &&
       ((pBuf->bHasGAMA) || (pData->bHasglobalGAMA)) )
  {
    mng_uint32 iGamma;

    if (pBuf->bHasGAMA)                /* get the gamma value */
      iGamma = pBuf->iGamma;
    else
      iGamma = pData->iGlobalGamma;
                                       /* inform the app */
    if (!pData->fProcessgamma ((mng_handle)pData, iGamma))
      MNG_ERROR (pData, MNG_APPCMSERROR)
                                       /* load color-correction routine */
    pData->fCorrectrow = (mng_fptr)correct_app_cms;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_APP_CMS, MNG_LC_END)
#endif

  return MNG_NOERROR;
}
#endif /* MNG_APP_CMS */

/* ************************************************************************** */

#ifdef MNG_APP_CMS
mng_retcode init_app_cms_object (mng_datap pData)
{
  mng_imagedatap pBuf = ((mng_imagep)pData->pCurrentobj)->pImgbuf;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_APP_CMS_OBJ, MNG_LC_START)
#endif

  if ((pData->fProcessiccp) && (pBuf->bHasICCP))
  {                                    /* inform the app */
    if (!pData->fProcessiccp ((mng_handle)pData, pBuf->iProfilesize, pBuf->pProfile))
      MNG_ERROR (pData, MNG_APPCMSERROR)
                                       /* load color-correction routine */
    pData->fCorrectrow = (mng_fptr)correct_app_cms;
  }

  if ((pData->fProcesssrgb) && (pBuf->bHasSRGB))
  {                                    /* inform the app */
    if (!pData->fProcesssrgb ((mng_handle)pData, pBuf->iRenderingintent))
      MNG_ERROR (pData, MNG_APPCMSERROR)
                                       /* load color-correction routine */
    pData->fCorrectrow = (mng_fptr)correct_app_cms;
  }

  if ((pData->fProcesschroma) && (pBuf->bHasCHRM))
  {                                    /* inform the app */
    if (!pData->fProcesschroma ((mng_handle)pData, pBuf->iWhitepointx,   pBuf->iWhitepointy,
                                                   pBuf->iPrimaryredx,   pBuf->iPrimaryredy,
                                                   pBuf->iPrimarygreenx, pBuf->iPrimarygreeny,
                                                   pBuf->iPrimarybluex,  pBuf->iPrimarybluey))
      MNG_ERROR (pData, MNG_APPCMSERROR)
                                       /* load color-correction routine */
    pData->fCorrectrow = (mng_fptr)correct_app_cms;
  }

  if ((pData->fProcessgamma) && (pBuf->bHasGAMA))
  {                                    /* inform the app */
    if (!pData->fProcessgamma ((mng_handle)pData, pBuf->iGamma))
      MNG_ERROR (pData, MNG_APPCMSERROR)
                                       /* load color-correction routine */
    pData->fCorrectrow = (mng_fptr)correct_app_cms;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_APP_CMS_OBJ, MNG_LC_END)
#endif

  return MNG_NOERROR;
}
#endif /* MNG_APP_CMS */

/* ************************************************************************** */

#ifdef MNG_APP_CMS
mng_retcode correct_app_cms (mng_datap pData)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CORRECT_APP_CMS, MNG_LC_START)
#endif

  if (pData->fProcessarow)             /* let the app do something with our row */
    if (!pData->fProcessarow ((mng_handle)pData, pData->iRowsamples,
                              pData->bIsRGBA16, pData->pRGBArow))
      MNG_ERROR (pData, MNG_APPCMSERROR)

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CORRECT_APP_CMS, MNG_LC_END)
#endif

  return MNG_NOERROR;
}
#endif /* MNG_APP_CMS */

/* ************************************************************************** */

#endif /* MNG_INCLUDE_DISPLAY_PROCS */

/* ************************************************************************** */
/* * end of file                                                            * */
/* ************************************************************************** */



