/* ************************************************************************** */
/* *             For conditions of distribution and use,                    * */
/* *                see copyright notice in libmng.h                        * */
/* ************************************************************************** */
/* *                                                                        * */
/* * project   : libmng                                                     * */
/* * file      : libmng_hlapi.c            copyright (c) 2000 G.Juyn        * */
/* * version   : 1.0.2                                                      * */
/* *                                                                        * */
/* * purpose   : high-level application API (implementation)                * */
/* *                                                                        * */
/* * author    : G.Juyn                                                     * */
/* * web       : http://www.3-t.com                                         * */
/* * email     : mailto:info@3-t.com                                        * */
/* *                                                                        * */
/* * comment   : implementation of the high-level function interface        * */
/* *             for applications.                                          * */
/* *                                                                        * */
/* * changes   : 0.5.1 - 05/06/2000 - G.Juyn                                * */
/* *             - added init of iPLTEcount                                 * */
/* *             0.5.1 - 05/08/2000 - G.Juyn                                * */
/* *             - changed calling-convention definition                    * */
/* *             - changed status-handling of display-routines              * */
/* *             - added versioning-control routines                        * */
/* *             - filled the write routine                                 * */
/* *             - changed strict-ANSI stuff                                * */
/* *             0.5.1 - 05/11/2000 - G.Juyn                                * */
/* *             - added callback error-reporting support                   * */
/* *             0.5.1 - 05/12/2000 - G.Juyn                                * */
/* *             - changed trace to macro for callback error-reporting      * */
/* *             0.5.1 - 05/13/2000 - G.Juyn                                * */
/* *             - added eMNGma hack (will be removed in 1.0.0 !!!)         * */
/* *             - added TERM animation object pointer (easier reference)   * */
/* *             0.5.1 - 05/14/2000 - G.Juyn                                * */
/* *             - added cleanup of saved-data (SAVE/SEEK processing)       * */
/* *             0.5.1 - 05/16/2000 - G.Juyn                                * */
/* *             - moved the actual write_graphic functionality from here   * */
/* *               to it's appropriate function in the mng_write module     * */
/* *                                                                        * */
/* *             0.5.2 - 05/19/2000 - G.Juyn                                * */
/* *             - cleaned up some code regarding mixed support             * */
/* *             - added JNG support                                        * */
/* *             0.5.2 - 05/24/2000 - G.Juyn                                * */
/* *             - moved init of default zlib parms here from "mng_zlib.c"  * */
/* *             - added init of default IJG parms                          * */
/* *             0.5.2 - 05/29/2000 - G.Juyn                                * */
/* *             - fixed inconsistancy with freeing global iCCP profile     * */
/* *             0.5.2 - 05/30/2000 - G.Juyn                                * */
/* *             - added delta-image field initialization                   * */
/* *             0.5.2 - 06/06/2000 - G.Juyn                                * */
/* *             - added initialization of the buffer-suspend parameter     * */
/* *                                                                        * */
/* *             0.5.3 - 06/16/2000 - G.Juyn                                * */
/* *             - added initialization of update-region for refresh        * */
/* *             - added initialization of Needrefresh parameter            * */
/* *             0.5.3 - 06/17/2000 - G.Juyn                                * */
/* *             - added initialization of Deltaimmediate                   * */
/* *             0.5.3 - 06/21/2000 - G.Juyn                                * */
/* *             - added initialization of Speed                            * */
/* *             - added initialization of Imagelevel                       * */
/* *             0.5.3 - 06/26/2000 - G.Juyn                                * */
/* *             - changed userdata variable to mng_ptr                     * */
/* *             0.5.3 - 06/29/2000 - G.Juyn                                * */
/* *             - fixed initialization routine for new mng_handle type     * */
/* *                                                                        * */
/* *             0.9.1 - 07/06/2000 - G.Juyn                                * */
/* *             - changed mng_display_resume to allow to be called after   * */
/* *               a suspension return with MNG_NEEDMOREDATA                * */
/* *             - added returncode MNG_NEEDTIMERWAIT for timer breaks      * */
/* *             0.9.1 - 07/07/2000 - G.Juyn                                * */
/* *             - implemented support for freeze/reset/resume & go_xxxx    * */
/* *             0.9.1 - 07/08/2000 - G.Juyn                                * */
/* *             - added support for improved timing                        * */
/* *             - added support for improved I/O-suspension                * */
/* *             0.9.1 - 07/14/2000 - G.Juyn                                * */
/* *             - changed EOF processing behavior                          * */
/* *             0.9.1 - 07/15/2000 - G.Juyn                                * */
/* *             - added callbacks for SAVE/SEEK processing                 * */
/* *             - added variable for NEEDSECTIONWAIT breaks                * */
/* *             - added variable for freeze & reset processing             * */
/* *             0.9.1 - 07/17/2000 - G.Juyn                                * */
/* *             - added error cleanup processing                           * */
/* *             - fixed support for mng_display_reset()                    * */
/* *             - fixed suspension-buffering for 32K+ chunks               * */
/* *                                                                        * */
/* *             0.9.2 - 07/29/2000 - G.Juyn                                * */
/* *             - fixed small bugs in display processing                   * */
/* *             0.9.2 - 07/31/2000 - G.Juyn                                * */
/* *             - fixed wrapping of suspension parameters                  * */
/* *             0.9.2 - 08/04/2000 - G.Juyn                                * */
/* *             - B111096 - fixed large-buffer read-suspension             * */
/* *             0.9.2 - 08/05/2000 - G.Juyn                                * */
/* *             - changed file-prefixes                                    * */
/* *                                                                        * */
/* *             0.9.3 - 09/07/2000 - G.Juyn                                * */
/* *             - added support for new filter_types                       * */
/* *             0.9.3 - 09/10/2000 - G.Juyn                                * */
/* *             - fixed DEFI behavior                                      * */
/* *             0.9.3 - 10/11/2000 - G.Juyn                                * */
/* *             - added support for nEED                                   * */
/* *             0.9.3 - 10/16/2000 - G.Juyn                                * */
/* *             - added optional support for bKGD for PNG images           * */
/* *             - raised initial maximum canvas size                       * */
/* *             - added support for JDAA                                   * */
/* *             0.9.3 - 10/17/2000 - G.Juyn                                * */
/* *             - added callback to process non-critical unknown chunks    * */
/* *             - fixed support for delta-images during read() / display() * */
/* *             0.9.3 - 10/18/2000 - G.Juyn                                * */
/* *             - added closestream() processing for mng_cleanup()         * */
/* *             0.9.3 - 10/27/2000 - G.Juyn                                * */
/* *             - fixed seperate read() & display() processing             * */
/* *                                                                        * */
/* *             0.9.4 - 11/20/2000 - G.Juyn                                * */
/* *             - fixed unwanted repetition in mng_readdisplay()           * */
/* *             0.9.4 - 11/24/2000 - G.Juyn                                * */
/* *             - moved restore of object 0 to libmng_display              * */
/* *                                                                        * */
/* *             1.0.1 - 02/08/2001 - G.Juyn                                * */
/* *             - added MEND processing callback                           * */
/* *             1.0.1 - 02/13/2001 - G.Juyn                                * */
/* *             - fixed first FRAM_MODE=4 timing problem                   * */
/* *             1.0.1 - 04/21/2001 - G.Juyn                                * */
/* *             - fixed bug with display_reset/display_resume (Thanks G!)  * */
/* *             1.0.1 - 04/22/2001 - G.Juyn                                * */
/* *             - fixed memory-leak (Thanks Gregg!)                        * */
/* *             1.0.1 - 04/23/2001 - G.Juyn                                * */
/* *             - fixed reset_rundata to drop all objects                  * */
/* *             1.0.1 - 04/25/2001 - G.Juyn                                * */
/* *             - moved mng_clear_cms to libmng_cms                        * */
/* *                                                                        * */
/* *             1.0.2 - 06/23/2001 - G.Juyn                                * */
/* *             - added optimization option for MNG-video playback         * */
/* *             - added processterm callback                               * */
/* *             1.0.2 - 06/25/2001 - G.Juyn                                * */
/* *             - added option to turn off progressive refresh             * */
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
#include "libmng_memory.h"
#include "libmng_read.h"
#include "libmng_write.h"
#include "libmng_display.h"
#include "libmng_zlib.h"
#include "libmng_jpeg.h"
#include "libmng_cms.h"
#include "libmng_pixels.h"

#if defined(__BORLANDC__) && defined(MNG_STRICT_ANSI)
#pragma option -A                      /* force ANSI-C */
#endif

/* ************************************************************************** */
/* *                                                                        * */
/* * local routines                                                         * */
/* *                                                                        * */
/* ************************************************************************** */

#if defined(MNG_SUPPORT_READ) || defined(MNG_SUPPORT_WRITE)
mng_retcode mng_drop_chunks (mng_datap pData)
{
  mng_chunkp       pChunk;
  mng_chunkp       pNext;
  mng_cleanupchunk fCleanup;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DROP_CHUNKS, MNG_LC_START)
#endif

  pChunk = pData->pFirstchunk;         /* and get first stored chunk (if any) */

  while (pChunk)                       /* more chunks to discard ? */
  {
    pNext = ((mng_chunk_headerp)pChunk)->pNext;
                                       /* call appropriate cleanup */
    fCleanup = ((mng_chunk_headerp)pChunk)->fCleanup;
    fCleanup (pData, pChunk);

    pChunk = pNext;                    /* neeeext */
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DROP_CHUNKS, MNG_LC_END)
#endif

  return MNG_NOERROR;
}
#endif /* MNG_SUPPORT_READ || MNG_SUPPORT_WRITE */

/* ************************************************************************** */

#ifdef MNG_SUPPORT_DISPLAY
mng_retcode mng_drop_objects (mng_datap pData,
                              mng_bool  bDropaniobj)
{
  mng_objectp       pObject;
  mng_objectp       pNext;
  mng_cleanupobject fCleanup;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DROP_OBJECTS, MNG_LC_START)
#endif

  pObject = pData->pFirstimgobj;       /* get first stored image-object (if any) */

  while (pObject)                      /* more objects to discard ? */
  {
    pNext = ((mng_object_headerp)pObject)->pNext;
                                       /* call appropriate cleanup */
    fCleanup = ((mng_object_headerp)pObject)->fCleanup;
    fCleanup (pData, pObject);

    pObject = pNext;                   /* neeeext */
  }

  pData->pFirstimgobj = MNG_NULL;      /* clean this up!!! */

  if (bDropaniobj)                     /* drop animation objects ? */
  {
    pObject = pData->pFirstaniobj;     /* get first stored animation-object (if any) */

    while (pObject)                    /* more objects to discard ? */
    {
      pNext = ((mng_object_headerp)pObject)->pNext;
                                       /* call appropriate cleanup */
      fCleanup = ((mng_object_headerp)pObject)->fCleanup;
      fCleanup (pData, pObject);

      pObject = pNext;                 /* neeeext */
    }

    pData->pFirstaniobj = MNG_NULL;    /* clean this up!!! */
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DROP_OBJECTS, MNG_LC_END)
#endif

  return MNG_NOERROR;
}
#endif /* MNG_SUPPORT_DISPLAY */

/* ************************************************************************** */

#ifdef MNG_SUPPORT_DISPLAY
mng_retcode mng_drop_savedata (mng_datap pData)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DROP_SAVEDATA, MNG_LC_START)
#endif

  if (pData->pSavedata)                /* sanity check */
  {                                    /* address it more directly */
    mng_savedatap pSave = pData->pSavedata;

    if (pSave->iGlobalProfilesize)     /* cleanup the profile ? */
      MNG_FREEX (pData, pSave->pGlobalProfile, pSave->iGlobalProfilesize)
                                       /* cleanup the save structure */
    MNG_FREE (pData, pData->pSavedata, sizeof (mng_savedata))
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DROP_SAVEDATA, MNG_LC_END)
#endif

  return MNG_NOERROR;
}
#endif /* MNG_SUPPORT_DISPLAY */

/* ************************************************************************** */

#ifdef MNG_SUPPORT_DISPLAY
mng_retcode mng_reset_rundata (mng_datap pData)
{
  drop_invalid_objects (pData);        /* drop invalidly stored objects */
  mng_drop_savedata    (pData);        /* drop stored savedata */
  mng_reset_objzero    (pData);        /* reset object 0 */
                                       /* drop stored objects (if any) */
  mng_drop_objects     (pData, MNG_FALSE);

  pData->bFramedone            = MNG_FALSE;
  pData->iFrameseq             = 0;    /* reset counters & stuff */
  pData->iLayerseq             = 0;
  pData->iFrametime            = 0;
  pData->iRequestframe         = 0;
  pData->iRequestlayer         = 0;
  pData->iRequesttime          = 0;
  pData->bSearching            = MNG_FALSE;

  pData->iRuntime              = 0;
  pData->iSynctime             = 0;
  pData->iStarttime            = 0;
  pData->iEndtime              = 0;
  pData->bRunning              = MNG_FALSE;
  pData->bTimerset             = MNG_FALSE;
  pData->iBreakpoint           = 0;
  pData->bSectionwait          = MNG_FALSE;
  pData->bFreezing             = MNG_FALSE;
  pData->bResetting            = MNG_FALSE;
  pData->bNeedrefresh          = MNG_FALSE;

  pData->iIterations           = 0;
                                       /* start of animation objects! */
  pData->pCurraniobj           = MNG_NULL;

  pData->iUpdateleft           = 0;    /* reset region */
  pData->iUpdateright          = 0;
  pData->iUpdatetop            = 0;
  pData->iUpdatebottom         = 0;
  pData->iPLTEcount            = 0;    /* reset PLTE data */

  pData->iDEFIobjectid         = 0;    /* reset DEFI data */
  pData->bDEFIhasdonotshow     = MNG_FALSE;
  pData->iDEFIdonotshow        = 0;
  pData->bDEFIhasconcrete      = MNG_FALSE;
  pData->iDEFIconcrete         = 0;
  pData->bDEFIhasloca          = MNG_FALSE;
  pData->iDEFIlocax            = 0;
  pData->iDEFIlocay            = 0;
  pData->bDEFIhasclip          = MNG_FALSE;
  pData->iDEFIclipl            = 0;
  pData->iDEFIclipr            = 0;
  pData->iDEFIclipt            = 0;
  pData->iDEFIclipb            = 0;

  pData->iBACKred              = 0;    /* reset BACK data */
  pData->iBACKgreen            = 0;
  pData->iBACKblue             = 0;
  pData->iBACKmandatory        = 0;
  pData->iBACKimageid          = 0;
  pData->iBACKtile             = 0;

  pData->iFRAMmode             = 1;     /* default global FRAM variables */
  pData->iFRAMdelay            = 1;
  pData->iFRAMtimeout          = 0x7fffffffl;
  pData->bFRAMclipping         = MNG_FALSE;
  pData->iFRAMclipl            = 0;
  pData->iFRAMclipr            = 0;
  pData->iFRAMclipt            = 0;
  pData->iFRAMclipb            = 0;

  pData->iFramemode            = 1;     /* again for the current frame */
  pData->iFramedelay           = 1;
  pData->iFrametimeout         = 0x7fffffffl;
  pData->bFrameclipping        = MNG_FALSE;
  pData->iFrameclipl           = 0;
  pData->iFrameclipr           = 0;
  pData->iFrameclipt           = 0;
  pData->iFrameclipb           = 0;

  pData->iNextdelay            = 1;

  pData->iSHOWmode             = 0;    /* reset SHOW data */
  pData->iSHOWfromid           = 0;
  pData->iSHOWtoid             = 0;
  pData->iSHOWnextid           = 0;
  pData->iSHOWskip             = 0;

  pData->iGlobalPLTEcount      = 0;    /* reset global PLTE data */

  pData->iGlobalTRNSrawlen     = 0;    /* reset global tRNS data */

  pData->iGlobalGamma          = 0;    /* reset global gAMA data */

  pData->iGlobalWhitepointx    = 0;    /* reset global cHRM data */
  pData->iGlobalWhitepointy    = 0;
  pData->iGlobalPrimaryredx    = 0;
  pData->iGlobalPrimaryredy    = 0;
  pData->iGlobalPrimarygreenx  = 0;
  pData->iGlobalPrimarygreeny  = 0;
  pData->iGlobalPrimarybluex   = 0;
  pData->iGlobalPrimarybluey   = 0;

  pData->iGlobalRendintent     = 0;    /* reset global sRGB data */

  if (pData->iGlobalProfilesize)       /* drop global profile (if any) */
    MNG_FREE (pData, pData->pGlobalProfile, pData->iGlobalProfilesize)

  pData->iGlobalProfilesize    = 0;    

  pData->iGlobalBKGDred        = 0;    /* reset global bKGD data */
  pData->iGlobalBKGDgreen      = 0;
  pData->iGlobalBKGDblue       = 0;
                                       /* reset delta-image */
  pData->pDeltaImage           = MNG_NULL;
  pData->iDeltaImagetype       = 0;
  pData->iDeltatype            = 0;
  pData->iDeltaBlockwidth      = 0;
  pData->iDeltaBlockheight     = 0;
  pData->iDeltaBlockx          = 0;
  pData->iDeltaBlocky          = 0;
  pData->bDeltaimmediate       = MNG_FALSE;

  return MNG_NOERROR;
}
#endif /* MNG_SUPPORT_DISPLAY */

/* ************************************************************************** */

void cleanup_errors (mng_datap pData)
{
  pData->iErrorcode = MNG_NOERROR;
  pData->iSeverity  = 0;
  pData->iErrorx1   = 0;
  pData->iErrorx2   = 0;
  pData->zErrortext = MNG_NULL;

  return;
}

/* ************************************************************************** */
/* *                                                                        * */
/* *  Versioning control                                                    * */
/* *                                                                        * */
/* ************************************************************************** */

mng_pchar MNG_DECL mng_version_text    (void)
{
  return MNG_VERSION_TEXT;
}

/* ************************************************************************** */

mng_uint8 MNG_DECL mng_version_so      (void)
{
  return MNG_VERSION_SO;
}

/* ************************************************************************** */

mng_uint8 MNG_DECL mng_version_dll     (void)
{
  return MNG_VERSION_DLL;
}

/* ************************************************************************** */

mng_uint8 MNG_DECL mng_version_major   (void)
{
  return MNG_VERSION_MAJOR;
}

/* ************************************************************************** */

mng_uint8 MNG_DECL mng_version_minor   (void)
{
  return MNG_VERSION_MINOR;
}

/* ************************************************************************** */

mng_uint8 MNG_DECL mng_version_release (void)
{
  return MNG_VERSION_RELEASE;
}

/* ************************************************************************** */
/* *                                                                        * */
/* * HLAPI routines                                                         * */
/* *                                                                        * */
/* ************************************************************************** */

mng_handle MNG_DECL mng_initialize (mng_ptr       pUserdata,
                                    mng_memalloc  fMemalloc,
                                    mng_memfree   fMemfree,
                                    mng_traceproc fTraceproc)
{
  mng_datap   pData;
#ifdef MNG_SUPPORT_DISPLAY
  mng_retcode iRetcode;
  mng_imagep  pImage;
#endif

#ifdef MNG_INTERNAL_MEMMNGMT           /* allocate the main datastruc */
  pData = (mng_datap)calloc (1, sizeof (mng_data));
#else
  pData = (mng_datap)fMemalloc (sizeof (mng_data));
#endif

  if (!pData)
    return MNG_NULL;                   /* error: out of memory?? */
                                       /* validate the structure */
  pData->iMagic                = MNG_MAGIC;
                                       /* save userdata field */
  pData->pUserdata             = pUserdata;
                                       /* remember trace callback */
  pData->fTraceproc            = fTraceproc;

#ifdef MNG_SUPPORT_TRACE
  if (mng_trace (pData, MNG_FN_INITIALIZE, MNG_LC_INITIALIZE))
  {
    MNG_FREEX (pData, pData, sizeof (mng_data))
    return MNG_NULL;
  }
#endif
                                       /* default canvas styles are 8-bit RGB */
  pData->iCanvasstyle          = MNG_CANVAS_RGB8;
  pData->iBkgdstyle            = MNG_CANVAS_RGB8;

  pData->iBGred                = 0;  /* black */
  pData->iBGgreen              = 0;
  pData->iBGblue               = 0;

  pData->bUseBKGD              = MNG_TRUE;

#ifdef MNG_FULL_CMS
  pData->bIssRGB               = MNG_TRUE;
  pData->hProf1                = 0;    /* no profiles yet */
  pData->hProf2                = 0;
  pData->hProf3                = 0;
  pData->hTrans                = 0;
#endif

  pData->dViewgamma            = 1.0;
  pData->dDisplaygamma         = 2.2;
  pData->dDfltimggamma         = 0.45455;
                                       /* initially remember chunks */
  pData->bStorechunks          = MNG_TRUE;
                                       /* no breaks at section-borders */
  pData->bSectionbreaks        = MNG_FALSE;
                                       /* initially cache playback info */
  pData->bCacheplayback        = MNG_TRUE;
                                       /* progressive refresh for large images */
  pData->bDoProgressive        = MNG_TRUE;
                                       /* normal animation-speed ! */
  pData->iSpeed                = mng_st_normal;
                                       /* initial image limits */
  pData->iMaxwidth             = 10000;
  pData->iMaxheight            = 10000;

#ifdef MNG_INTERNAL_MEMMNGMT           /* internal management */
  pData->fMemalloc             = MNG_NULL;
  pData->fMemfree              = MNG_NULL;
#else                                  /* keep callbacks */
  pData->fMemalloc             = fMemalloc;
  pData->fMemfree              = fMemfree;
#endif
                                       /* no value (yet) */
  pData->fOpenstream           = MNG_NULL;
  pData->fClosestream          = MNG_NULL;
  pData->fReaddata             = MNG_NULL;
  pData->fWritedata            = MNG_NULL;
  pData->fErrorproc            = MNG_NULL;
  pData->fProcessheader        = MNG_NULL;
  pData->fProcesstext          = MNG_NULL;
  pData->fProcesssave          = MNG_NULL;
  pData->fProcessseek          = MNG_NULL;
  pData->fProcessneed          = MNG_NULL;
  pData->fProcessmend          = MNG_NULL;
  pData->fProcessunknown       = MNG_NULL;
  pData->fProcessterm          = MNG_NULL;
  pData->fGetcanvasline        = MNG_NULL;
  pData->fGetbkgdline          = MNG_NULL;
  pData->fGetalphaline         = MNG_NULL;
  pData->fRefresh              = MNG_NULL;
  pData->fGettickcount         = MNG_NULL;
  pData->fSettimer             = MNG_NULL;
  pData->fProcessgamma         = MNG_NULL;
  pData->fProcesschroma        = MNG_NULL;
  pData->fProcesssrgb          = MNG_NULL;
  pData->fProcessiccp          = MNG_NULL;
  pData->fProcessarow          = MNG_NULL;

#if defined(MNG_SUPPORT_DISPLAY) && (defined(MNG_GAMMA_ONLY) || defined(MNG_FULL_CMS))
  pData->dLastgamma            = 0;    /* lookup table needs first-time calc */
#endif

#ifdef MNG_SUPPORT_DISPLAY             /* create object 0 */
  iRetcode = create_imageobject (pData, 0, MNG_TRUE, MNG_TRUE, MNG_TRUE,
                                 0, 0, 0, 0, 0, 0, 0, 0, 0, MNG_FALSE,
                                 0, 0, 0, 0, &pImage);

  if (iRetcode)                        /* on error drop out */
  {
    MNG_FREEX (pData, pData, sizeof (mng_data))
    return MNG_NULL;
  }

  pData->pObjzero = pImage;
#endif

#if defined(MNG_SUPPORT_DISPLAY) && defined(MNG_INCLUDE_LCMS)
  mnglcms_initlibrary ();              /* init lcms particulairs */
#endif

#ifdef MNG_SUPPORT_READ
  pData->bSuspensionmode       = MNG_FALSE;
  pData->iSuspendbufsize       = 0;
  pData->pSuspendbuf           = MNG_NULL;
  pData->pSuspendbufnext       = MNG_NULL;
  pData->iSuspendbufleft       = 0;
  pData->iChunklen             = 0;
  pData->pReadbufnext          = MNG_NULL;
#endif

#ifdef MNG_INCLUDE_ZLIB
  mngzlib_initialize (pData);          /* initialize zlib structures and such */
                                       /* default zlib compression parameters */
  pData->iZlevel               = MNG_ZLIB_LEVEL;
  pData->iZmethod              = MNG_ZLIB_METHOD;
  pData->iZwindowbits          = MNG_ZLIB_WINDOWBITS;
  pData->iZmemlevel            = MNG_ZLIB_MEMLEVEL;
  pData->iZstrategy            = MNG_ZLIB_STRATEGY;
                                       /* default maximum IDAT data size */
  pData->iMaxIDAT              = MNG_MAX_IDAT_SIZE;
#endif

#ifdef MNG_INCLUDE_JNG                 /* default IJG compression parameters */
  pData->eJPEGdctmethod        = MNG_JPEG_DCT;
  pData->iJPEGquality          = MNG_JPEG_QUALITY;
  pData->iJPEGsmoothing        = MNG_JPEG_SMOOTHING;
  pData->bJPEGcompressprogr    = MNG_JPEG_PROGRESSIVE;
  pData->bJPEGcompressopt      = MNG_JPEG_OPTIMIZED;
                                       /* default maximum JDAT data size */
  pData->iMaxJDAT              = MNG_MAX_JDAT_SIZE;
#endif

  mng_reset ((mng_handle)pData);

#ifdef MNG_SUPPORT_TRACE
  if (mng_trace (pData, MNG_FN_INITIALIZE, MNG_LC_END))
  {
    MNG_FREEX (pData, pData, sizeof (mng_data))
    return MNG_NULL;
  }
#endif

  return (mng_handle)pData;            /* if we get here, we're in business */
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_reset (mng_handle hHandle)
{
  mng_datap pData;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_RESET, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = ((mng_datap)(hHandle));      /* address main structure */

#ifdef MNG_SUPPORT_DISPLAY
  mng_drop_savedata (pData);           /* cleanup saved-data from SAVE/SEEK */
#endif

#if defined(MNG_SUPPORT_DISPLAY) && defined(MNG_FULL_CMS)
  mng_clear_cms (pData);               /* cleanup left-over cms stuff if any */
#endif

#ifdef MNG_INCLUDE_JNG
  mngjpeg_cleanup (pData);             /* cleanup jpeg stuff */
#endif

#ifdef MNG_INCLUDE_ZLIB
  if (pData->bInflating)               /* if we've been inflating */
  {
#ifdef MNG_INCLUDE_DISPLAY_PROCS
    cleanup_rowproc (pData);           /* cleanup row-processing, */
#endif
    mngzlib_inflatefree (pData);       /* cleanup inflate! */
  }
#endif /* MNG_INCLUDE_ZLIB */

#ifdef MNG_SUPPORT_READ
  if ((pData->bReading) && (!pData->bEOF))
    process_eof (pData);               /* cleanup app streaming */
                                       /* cleanup default read buffers */
  MNG_FREE (pData, pData->pReadbuf,    pData->iReadbufsize)
  MNG_FREE (pData, pData->pLargebuf,   pData->iLargebufsize)
  MNG_FREE (pData, pData->pSuspendbuf, pData->iSuspendbufsize)
#endif

#ifdef MNG_SUPPORT_WRITE               /* cleanup default write buffer */
  MNG_FREE (pData, pData->pWritebuf, pData->iWritebufsize)
#endif

#if defined(MNG_SUPPORT_READ) || defined(MNG_SUPPORT_WRITE)
  mng_drop_chunks  (pData);            /* drop stored chunks (if any) */
#endif

#ifdef MNG_SUPPORT_DISPLAY
  mng_drop_objects (pData, MNG_TRUE);  /* drop stored objects (if any) */

  if (pData->iGlobalProfilesize)       /* drop global profile (if any) */
    MNG_FREEX (pData, pData->pGlobalProfile, pData->iGlobalProfilesize)
#endif

  pData->eSigtype              = mng_it_unknown;
  pData->eImagetype            = mng_it_unknown;
  pData->iWidth                = 0;    /* these are unknown yet */
  pData->iHeight               = 0;
  pData->iTicks                = 0;
  pData->iLayercount           = 0;
  pData->iFramecount           = 0;
  pData->iPlaytime             = 0;
  pData->iSimplicity           = 0;
  pData->iAlphadepth           = 16;   /* assume the worst! */

  pData->iImagelevel           = 0;    /* no image encountered */

  pData->iMagnify              = 0;    /* 1-to-1 display */
  pData->iOffsetx              = 0;    /* no offsets */
  pData->iOffsety              = 0;
  pData->iCanvaswidth          = 0;    /* let the app decide during processheader */
  pData->iCanvasheight         = 0;
                                       /* so far, so good */
  pData->iErrorcode            = MNG_NOERROR;
  pData->iSeverity             = 0;
  pData->iErrorx1              = 0;
  pData->iErrorx2              = 0;
  pData->zErrortext            = MNG_NULL;

#if defined(MNG_SUPPORT_READ) || defined(MNG_SUPPORT_WRITE)
                                       /* let's assume the best scenario */
  pData->bPreDraft48           = MNG_FALSE;
                                       /* the unknown chunk */
  pData->iChunkname            = MNG_UINT_HUH;
  pData->iChunkseq             = 0;
  pData->pFirstchunk           = MNG_NULL;
  pData->pLastchunk            = MNG_NULL;
                                       /* nothing processed yet */
  pData->bHasheader            = MNG_FALSE;
  pData->bHasMHDR              = MNG_FALSE;
  pData->bHasIHDR              = MNG_FALSE;
  pData->bHasBASI              = MNG_FALSE;
  pData->bHasDHDR              = MNG_FALSE;
#ifdef MNG_INCLUDE_JNG
  pData->bHasJHDR              = MNG_FALSE;
  pData->bHasJSEP              = MNG_FALSE;
  pData->bHasJDAA              = MNG_FALSE;
  pData->bHasJDAT              = MNG_FALSE;
#endif
  pData->bHasPLTE              = MNG_FALSE;
  pData->bHasTRNS              = MNG_FALSE;
  pData->bHasGAMA              = MNG_FALSE;
  pData->bHasCHRM              = MNG_FALSE;
  pData->bHasSRGB              = MNG_FALSE;
  pData->bHasICCP              = MNG_FALSE;
  pData->bHasBKGD              = MNG_FALSE;
  pData->bHasIDAT              = MNG_FALSE;

  pData->bHasSAVE              = MNG_FALSE;
  pData->bHasBACK              = MNG_FALSE;
  pData->bHasFRAM              = MNG_FALSE;
  pData->bHasTERM              = MNG_FALSE;
  pData->bHasLOOP              = MNG_FALSE;
                                       /* there's no global stuff yet either */
  pData->bHasglobalPLTE        = MNG_FALSE;
  pData->bHasglobalTRNS        = MNG_FALSE;
  pData->bHasglobalGAMA        = MNG_FALSE;
  pData->bHasglobalCHRM        = MNG_FALSE;
  pData->bHasglobalSRGB        = MNG_FALSE;
  pData->bHasglobalICCP        = MNG_FALSE;

  pData->iDatawidth            = 0;    /* no IHDR/BASI/DHDR done yet */
  pData->iDataheight           = 0;
  pData->iBitdepth             = 0;
  pData->iColortype            = 0;
  pData->iCompression          = 0;
  pData->iFilter               = 0;
  pData->iInterlace            = 0;

#ifdef MNG_INCLUDE_JNG
  pData->iJHDRcolortype        = 0;    /* no JHDR data */
  pData->iJHDRimgbitdepth      = 0;
  pData->iJHDRimgcompression   = 0;
  pData->iJHDRimginterlace     = 0;
  pData->iJHDRalphabitdepth    = 0;
  pData->iJHDRalphacompression = 0;
  pData->iJHDRalphafilter      = 0;
  pData->iJHDRalphainterlace   = 0;
#endif

#endif /* MNG_SUPPORT_READ || MNG_SUPPORT_WRITE */

#ifdef MNG_SUPPORT_READ                /* no reading done */
  pData->bReading              = MNG_FALSE;
  pData->bHavesig              = MNG_FALSE;
  pData->bEOF                  = MNG_FALSE;
  pData->iReadbufsize          = 0;
  pData->pReadbuf              = MNG_NULL;

  pData->iLargebufsize         = 0;
  pData->pLargebuf             = MNG_NULL;

  pData->iSuspendtime          = 0;
  pData->bSuspended            = MNG_FALSE;
  pData->iSuspendpoint         = 0;

  pData->pSuspendbufnext       = pData->pSuspendbuf;
  pData->iSuspendbufleft       = 0;
#endif /* MNG_SUPPORT_READ */

#ifdef MNG_SUPPORT_WRITE               /* no creating/writing done */
  pData->bCreating             = MNG_FALSE;
  pData->bWriting              = MNG_FALSE;
  pData->iFirstchunkadded      = 0;
  pData->iWritebufsize         = 0;
  pData->pWritebuf             = MNG_NULL;
#endif /* MNG_SUPPORT_WRITE */

#ifdef MNG_SUPPORT_DISPLAY             /* done nuttin' yet */
  pData->bDisplaying           = MNG_FALSE;
  pData->iFrameseq             = 0;
  pData->iLayerseq             = 0;
  pData->iFrametime            = 0;

  pData->iRequestframe         = 0;
  pData->iRequestlayer         = 0;
  pData->iRequesttime          = 0;
  pData->bSearching            = MNG_FALSE;

  pData->bRestorebkgd          = MNG_FALSE;

  pData->iRuntime              = 0;
  pData->iSynctime             = 0;
  pData->iStarttime            = 0;
  pData->iEndtime              = 0;
  pData->bRunning              = MNG_FALSE;
  pData->bTimerset             = MNG_FALSE;
  pData->iBreakpoint           = 0;
  pData->bSectionwait          = MNG_FALSE;
  pData->bFreezing             = MNG_FALSE;
  pData->bResetting            = MNG_FALSE;
  pData->bNeedrefresh          = MNG_FALSE;
                                       /* these don't exist yet */
  pData->pCurrentobj           = MNG_NULL;
  pData->pCurraniobj           = MNG_NULL;
  pData->pTermaniobj           = MNG_NULL;
  pData->pLastclone            = MNG_NULL;
  pData->pStoreobj             = MNG_NULL;
  pData->pStorebuf             = MNG_NULL;
  pData->pRetrieveobj          = MNG_NULL;
                                       /* no saved data ! */
  pData->pSavedata             = MNG_NULL;
                                       /* TODO: remove in 1.0.0 !!! */
  pData->bEMNGMAhack           = MNG_FALSE;

  pData->iUpdateleft           = 0;    /* no region updated yet */
  pData->iUpdateright          = 0;
  pData->iUpdatetop            = 0;
  pData->iUpdatebottom         = 0;

  pData->iPass                 = -1;   /* interlacing stuff and temp buffers */
  pData->iRow                  = 0;
  pData->iRowinc               = 1;
  pData->iCol                  = 0;
  pData->iColinc               = 1;
  pData->iRowsamples           = 0;
  pData->iSamplemul            = 0;
  pData->iSampleofs            = 0;
  pData->iSamplediv            = 0;
  pData->iRowsize              = 0;
  pData->iRowmax               = 0;
  pData->iFilterofs            = 0;
  pData->iPixelofs             = 1;
  pData->iLevel0               = 0;
  pData->iLevel1               = 0;
  pData->iLevel2               = 0;
  pData->iLevel3               = 0;
  pData->pWorkrow              = MNG_NULL;
  pData->pPrevrow              = MNG_NULL;
  pData->pRGBArow              = 0;
  pData->bIsRGBA16             = MNG_TRUE;
  pData->bIsOpaque             = MNG_TRUE;
  pData->iFilterbpp            = 1;

  pData->iSourcel              = 0;    /* always initialized just before */
  pData->iSourcer              = 0;    /* compositing the next layer */
  pData->iSourcet              = 0;
  pData->iSourceb              = 0;
  pData->iDestl                = 0;
  pData->iDestr                = 0;
  pData->iDestt                = 0;
  pData->iDestb                = 0;
                                       /* lists are empty */
  pData->pFirstimgobj          = MNG_NULL;
  pData->pLastimgobj           = MNG_NULL;
  pData->pFirstaniobj          = MNG_NULL;
  pData->pLastaniobj           = MNG_NULL;
                                       /* no processing callbacks */
  pData->fDisplayrow           = MNG_NULL;
  pData->fRestbkgdrow          = MNG_NULL;
  pData->fCorrectrow           = MNG_NULL;
  pData->fRetrieverow          = MNG_NULL;
  pData->fStorerow             = MNG_NULL;
  pData->fProcessrow           = MNG_NULL;
  pData->fDifferrow            = MNG_NULL;
  pData->fInitrowproc          = MNG_NULL;

  pData->iPLTEcount            = 0;    /* no PLTE data */

  pData->iDEFIobjectid         = 0;    /* no DEFI data */
  pData->bDEFIhasdonotshow     = MNG_FALSE;
  pData->iDEFIdonotshow        = 0;
  pData->bDEFIhasconcrete      = MNG_FALSE;
  pData->iDEFIconcrete         = 0;
  pData->bDEFIhasloca          = MNG_FALSE;
  pData->iDEFIlocax            = 0;
  pData->iDEFIlocay            = 0;
  pData->bDEFIhasclip          = MNG_FALSE;
  pData->iDEFIclipl            = 0;
  pData->iDEFIclipr            = 0;
  pData->iDEFIclipt            = 0;
  pData->iDEFIclipb            = 0;

  pData->iBACKred              = 0;    /* no BACK data */
  pData->iBACKgreen            = 0;
  pData->iBACKblue             = 0;
  pData->iBACKmandatory        = 0;
  pData->iBACKimageid          = 0;
  pData->iBACKtile             = 0;

  pData->iFRAMmode             = 1;     /* default global FRAM variables */
  pData->iFRAMdelay            = 1;
  pData->iFRAMtimeout          = 0x7fffffffl;
  pData->bFRAMclipping         = MNG_FALSE;
  pData->iFRAMclipl            = 0;
  pData->iFRAMclipr            = 0;
  pData->iFRAMclipt            = 0;
  pData->iFRAMclipb            = 0;

  pData->iFramemode            = 1;     /* again for the current frame */
  pData->iFramedelay           = 1;
  pData->iFrametimeout         = 0x7fffffffl;
  pData->bFrameclipping        = MNG_FALSE;
  pData->iFrameclipl           = 0;
  pData->iFrameclipr           = 0;
  pData->iFrameclipt           = 0;
  pData->iFrameclipb           = 0;

  pData->iNextdelay            = 1;

  pData->iSHOWmode             = 0;    /* no SHOW data */
  pData->iSHOWfromid           = 0;
  pData->iSHOWtoid             = 0;
  pData->iSHOWnextid           = 0;
  pData->iSHOWskip             = 0;

  pData->iGlobalPLTEcount      = 0;    /* no global PLTE data */

  pData->iGlobalTRNSrawlen     = 0;    /* no global tRNS data */

  pData->iGlobalGamma          = 0;    /* no global gAMA data */

  pData->iGlobalWhitepointx    = 0;    /* no global cHRM data */
  pData->iGlobalWhitepointy    = 0;
  pData->iGlobalPrimaryredx    = 0;
  pData->iGlobalPrimaryredy    = 0;
  pData->iGlobalPrimarygreenx  = 0;
  pData->iGlobalPrimarygreeny  = 0;
  pData->iGlobalPrimarybluex   = 0;
  pData->iGlobalPrimarybluey   = 0;

  pData->iGlobalRendintent     = 0;    /* no global sRGB data */

  pData->iGlobalProfilesize    = 0;    /* no global iCCP data */
  pData->pGlobalProfile        = MNG_NULL;

  pData->iGlobalBKGDred        = 0;    /* no global bKGD data */
  pData->iGlobalBKGDgreen      = 0;
  pData->iGlobalBKGDblue       = 0;
                                       /* no delta-image */
  pData->pDeltaImage           = MNG_NULL;
  pData->iDeltaImagetype       = 0;
  pData->iDeltatype            = 0;
  pData->iDeltaBlockwidth      = 0;
  pData->iDeltaBlockheight     = 0;
  pData->iDeltaBlockx          = 0;
  pData->iDeltaBlocky          = 0;
  pData->bDeltaimmediate       = MNG_FALSE;
#endif

#ifdef MNG_INCLUDE_ZLIB
  pData->bInflating            = 0;    /* no inflating or deflating */
  pData->bDeflating            = 0;    /* going on at the moment */
#endif

#ifdef MNG_SUPPORT_DISPLAY             /* reset object 0 */
  mng_reset_objzero (pData);
#endif

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_RESET, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode MNG_DECL mng_cleanup (mng_handle* hHandle)
{
  mng_datap pData;                     /* local vars */
#ifndef MNG_INTERNAL_MEMMNGMT
  mng_memfree fFree;
#endif

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)*hHandle), MNG_FN_CLEANUP, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (*hHandle)           /* check validity handle */
  pData = ((mng_datap)(*hHandle));     /* and address main structure */

  mng_reset (*hHandle);                /* do an implicit reset to cleanup most stuff */

#ifdef MNG_SUPPORT_DISPLAY             /* drop object 0 */
  free_imageobject (pData, (mng_imagep)pData->pObjzero);
#endif

#if defined(MNG_SUPPORT_DISPLAY) && defined(MNG_INCLUDE_LCMS)
  if (pData->hProf2)                   /* output profile defined ? */
    mnglcms_freeprofile (pData->hProf2);

  if (pData->hProf3)                   /* sRGB profile defined ? */
    mnglcms_freeprofile (pData->hProf3);
#endif /* MNG_SUPPORT_DISPLAY && MNG_INCLUDE_LCMS */

#ifdef MNG_INCLUDE_ZLIB
  mngzlib_cleanup (pData);             /* cleanup zlib stuff */
#endif

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)*hHandle), MNG_FN_CLEANUP, MNG_LC_CLEANUP)
#endif

  pData->iMagic = 0;                   /* invalidate the actual memory */

#ifdef MNG_INTERNAL_MEMMNGMT
  free ((void *)*hHandle);             /* cleanup the data-structure */
#else
  fFree = ((mng_datap)*hHandle)->fMemfree;
  fFree ((mng_ptr)*hHandle, sizeof (mng_data));
#endif

  *hHandle = 0;                        /* wipe pointer to inhibit future use */

  return MNG_NOERROR;                  /* and we're done */
}

/* ************************************************************************** */

#ifdef MNG_SUPPORT_READ
mng_retcode MNG_DECL mng_read (mng_handle hHandle)
{
  mng_datap   pData;                   /* local vars */
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_READ, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle and callbacks */
  pData = ((mng_datap)hHandle);        /* and make it addressable */

#ifndef MNG_INTERNAL_MEMMNGMT
  MNG_VALIDCB (hHandle, fMemalloc)
  MNG_VALIDCB (hHandle, fMemfree)
#endif

  MNG_VALIDCB (hHandle, fOpenstream)
  MNG_VALIDCB (hHandle, fClosestream)
  MNG_VALIDCB (hHandle, fReaddata)

#ifdef MNG_SUPPORT_DISPLAY             /* valid at this point ? */
  if ((pData->bReading) || (pData->bDisplaying))
#else
  if (pData->bReading)
#endif
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)

#ifdef MNG_SUPPORT_WRITE
  if ((pData->bWriting) || (pData->bCreating))
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
#endif

  if (!pData->bCacheplayback)          /* must store playback info to work!! */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)

  cleanup_errors (pData);              /* cleanup previous errors */

  pData->bReading = MNG_TRUE;          /* read only! */

  if (!pData->fOpenstream (hHandle))   /* open it and start reading */
    iRetcode = MNG_APPIOERROR;
  else
    iRetcode = read_graphic (pData);

  if (pData->bEOF)                     /* already at EOF ? */
  {
    pData->bReading = MNG_FALSE;       /* then we're no longer reading */
    
#ifdef MNG_SUPPORT_DISPLAY
    mng_reset_rundata (pData);         /* reset rundata */
#endif
  }

  if (iRetcode)                        /* on error bail out */
    return iRetcode;

  if (pData->bSuspended)               /* read suspension ? */
  {
     iRetcode            = MNG_NEEDMOREDATA;
     pData->iSuspendtime = pData->fGettickcount ((mng_handle)pData);
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_READ, MNG_LC_END)
#endif

  return iRetcode;
}
#endif /* MNG_SUPPORT_READ */

/* ************************************************************************** */

#ifdef MNG_SUPPORT_READ
mng_retcode MNG_DECL mng_read_resume (mng_handle hHandle)
{
  mng_datap   pData;                   /* local vars */
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_READ_RESUME, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = ((mng_datap)hHandle);        /* and make it addressable */
                                       /* can we expect this call ? */
  if ((!pData->bReading) || (!pData->bSuspended))
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)

  cleanup_errors (pData);              /* cleanup previous errors */

  pData->bSuspended = MNG_FALSE;       /* reset the flag */

#ifdef MNG_SUPPORT_DISPLAY             /* re-synchronize ? */
  if ((pData->bDisplaying) && (pData->bRunning))
    pData->iSynctime  = pData->iSynctime - pData->iSuspendtime +
                        pData->fGettickcount (hHandle);
#endif

  iRetcode = read_graphic (pData);     /* continue reading now */

  if (pData->bEOF)                     /* at EOF ? */
  {
    pData->bReading = MNG_FALSE;       /* then we're no longer reading */
    
#ifdef MNG_SUPPORT_DISPLAY
    mng_reset_rundata (pData);         /* reset rundata */
#endif
  }

  if (iRetcode)                        /* on error bail out */
    return iRetcode;

  if (pData->bSuspended)               /* read suspension ? */
  {
     iRetcode            = MNG_NEEDMOREDATA;
     pData->iSuspendtime = pData->fGettickcount ((mng_handle)pData);
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_READ_RESUME, MNG_LC_END)
#endif

  return iRetcode;
}
#endif /* MNG_SUPPORT_READ */

/* ************************************************************************** */

#ifdef MNG_SUPPORT_WRITE
mng_retcode MNG_DECL mng_write (mng_handle hHandle)
{
  mng_datap   pData;
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_WRITE, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle and callbacks */
  pData = ((mng_datap)hHandle);        /* and make it addressable */

#ifndef MNG_INTERNAL_MEMMNGMT
  MNG_VALIDCB (hHandle, fMemalloc)
  MNG_VALIDCB (hHandle, fMemfree)
#endif

  MNG_VALIDCB (hHandle, fOpenstream)
  MNG_VALIDCB (hHandle, fClosestream)
  MNG_VALIDCB (hHandle, fWritedata)

#ifdef MNG_SUPPORT_READ
  if (pData->bReading)                 /* valid at this point ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
#endif

  if (pData->bCreating)                /* can't write while it's still being made! */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)

  cleanup_errors (pData);              /* cleanup previous errors */

  iRetcode = write_graphic (pData);    /* do the write */

  if (iRetcode)                        /* on error bail out */
    return iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_WRITE, MNG_LC_END)
#endif

  return MNG_NOERROR;
}
#endif /* MNG_SUPPORT_WRITE */

/* ************************************************************************** */

#ifdef MNG_SUPPORT_WRITE
mng_retcode MNG_DECL mng_create (mng_handle hHandle)
{
  mng_datap   pData;
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_CREATE, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle and callbacks */
  pData = ((mng_datap)hHandle);        /* and make it addressable */

#ifndef MNG_INTERNAL_MEMMNGMT
  MNG_VALIDCB (hHandle, fMemalloc)
  MNG_VALIDCB (hHandle, fMemfree)
#endif

#ifdef MNG_SUPPORT_READ
  if (pData->bReading)                 /* valid at this point ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
#endif

  if ((pData->bWriting) || (pData->bCreating))
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)

  cleanup_errors (pData);              /* cleanup previous errors */

  iRetcode = mng_reset (hHandle);      /* clear any previous stuff */

  if (iRetcode)                        /* on error bail out */
    return iRetcode;

  pData->bCreating = MNG_TRUE;         /* indicate we're creating a new file */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_CREATE, MNG_LC_END)
#endif

  return MNG_NOERROR;
}
#endif /* MNG_SUPPORT_WRITE */

/* ************************************************************************** */

#if defined(MNG_SUPPORT_DISPLAY) && defined(MNG_SUPPORT_READ)
mng_retcode MNG_DECL mng_readdisplay (mng_handle hHandle)
{
  mng_datap   pData;                   /* local vars */
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_READDISPLAY, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle and callbacks */
  pData = ((mng_datap)hHandle);        /* and make it addressable */

#ifndef MNG_INTERNAL_MEMMNGMT
  MNG_VALIDCB (hHandle, fMemalloc)
  MNG_VALIDCB (hHandle, fMemfree)
#endif

  MNG_VALIDCB (hHandle, fReaddata)
  MNG_VALIDCB (hHandle, fGetcanvasline)
  MNG_VALIDCB (hHandle, fRefresh)
  MNG_VALIDCB (hHandle, fGettickcount)
  MNG_VALIDCB (hHandle, fSettimer)
                                       /* valid at this point ? */
  if ((pData->bReading) || (pData->bDisplaying))
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)

#ifdef MNG_SUPPORT_WRITE
  if ((pData->bWriting) || (pData->bCreating))
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
#endif

  cleanup_errors (pData);              /* cleanup previous errors */

  pData->bReading      = MNG_TRUE;     /* read & display! */
  pData->bDisplaying   = MNG_TRUE;
  pData->bRunning      = MNG_TRUE;
  pData->iFrameseq     = 0;
  pData->iLayerseq     = 0;
  pData->iFrametime    = 0;
  pData->iRequestframe = 0;
  pData->iRequestlayer = 0;
  pData->iRequesttime  = 0;
  pData->bSearching    = MNG_FALSE;
  pData->iRuntime      = 0;
  pData->iSynctime     = pData->fGettickcount (hHandle);
  pData->iSuspendtime  = 0;
  pData->iStarttime    = pData->iSynctime;
  pData->iEndtime      = 0;

  if (!pData->fOpenstream (hHandle))   /* open it and start reading */
    iRetcode = MNG_APPIOERROR;
  else
    iRetcode = read_graphic (pData);

  if (pData->bEOF)                     /* already at EOF ? */
  {
    pData->bReading = MNG_FALSE;       /* then we're no longer reading */
    drop_invalid_objects (pData);      /* drop invalidly stored objects */
  }
  
  if (iRetcode)                        /* on error bail out */
    return iRetcode;

  if (pData->bSuspended)               /* read suspension ? */
  {
     iRetcode            = MNG_NEEDMOREDATA;
     pData->iSuspendtime = pData->fGettickcount ((mng_handle)pData);
  }
  else
  if (pData->bTimerset)                /* indicate timer break ? */
    iRetcode = MNG_NEEDTIMERWAIT;
  else
  if (pData->bSectionwait)             /* indicate section break ? */
    iRetcode = MNG_NEEDSECTIONWAIT;
  else
    pData->bRunning = MNG_FALSE;       /* no breaks = end of run */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_READDISPLAY, MNG_LC_END)
#endif

  return iRetcode;
}
#endif /* MNG_SUPPORT_DISPLAY && MNG_SUPPORT_READ */

/* ************************************************************************** */

#ifdef MNG_SUPPORT_DISPLAY
mng_retcode MNG_DECL mng_display (mng_handle hHandle)
{
  mng_datap   pData;                   /* local vars */
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_DISPLAY, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle and callbacks */
  pData = ((mng_datap)hHandle);        /* and make it addressable */

#ifndef MNG_INTERNAL_MEMMNGMT
  MNG_VALIDCB (hHandle, fMemalloc)
  MNG_VALIDCB (hHandle, fMemfree)
#endif

  MNG_VALIDCB (hHandle, fGetcanvasline)
  MNG_VALIDCB (hHandle, fRefresh)
  MNG_VALIDCB (hHandle, fGettickcount)
  MNG_VALIDCB (hHandle, fSettimer)

  if (pData->bDisplaying)              /* valid at this point ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
    
#ifdef MNG_SUPPORT_READ
  if (pData->bReading)
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
#endif

#ifdef MNG_SUPPORT_WRITE
  if ((pData->bWriting) || (pData->bCreating))
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)
#endif

  cleanup_errors (pData);              /* cleanup previous errors */

  pData->bDisplaying   = MNG_TRUE;     /* display! */
  pData->bRunning      = MNG_TRUE;
  pData->iFrameseq     = 0;
  pData->iLayerseq     = 0;
  pData->iFrametime    = 0;
  pData->iRequestframe = 0;
  pData->iRequestlayer = 0;
  pData->iRequesttime  = 0;
  pData->bSearching    = MNG_FALSE;
  pData->iRuntime      = 0;
  pData->iSynctime     = pData->fGettickcount (hHandle);
#ifdef MNG_SUPPORT_READ
  pData->iSuspendtime  = 0;
#endif  
  pData->iStarttime    = pData->iSynctime;
  pData->iEndtime      = 0;
  pData->pCurraniobj   = pData->pFirstaniobj;

  iRetcode = process_display (pData);  /* go do it */

  if (iRetcode)                        /* on error bail out */
    return iRetcode;

  if (pData->bTimerset)                /* indicate timer break ? */
    iRetcode = MNG_NEEDTIMERWAIT;
  else
    pData->bRunning = MNG_FALSE;       /* no breaks = end of run */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_DISPLAY, MNG_LC_END)
#endif

  return iRetcode;
}
#endif /* MNG_SUPPORT_DISPLAY */

/* ************************************************************************** */

#ifdef MNG_SUPPORT_DISPLAY
mng_retcode MNG_DECL mng_display_resume (mng_handle hHandle)
{
  mng_datap   pData;                   /* local vars */
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_DISPLAY_RESUME, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = ((mng_datap)hHandle);        /* and make it addressable */

  if (!pData->bDisplaying)             /* can we expect this call ? */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)

  cleanup_errors (pData);              /* cleanup previous errors */

  if (pData->bRunning)                 /* was it running ? */
  {                                    /* are we expecting this call ? */
    if ((pData->bTimerset) || (pData->bSuspended) || (pData->bSectionwait)) 
    {
      pData->bTimerset    = MNG_FALSE; /* reset the flags */
      pData->bSectionwait = MNG_FALSE;

#ifdef MNG_SUPPORT_READ
      if (pData->bReading)             /* set during read&display ? */
      {
        if (pData->bSuspended)         /* calculate proper synchronization */
          pData->iSynctime = pData->iSynctime - pData->iSuspendtime +
                             pData->fGettickcount (hHandle);
        else
          pData->iSynctime = pData->fGettickcount (hHandle);

        pData->bSuspended = MNG_FALSE; /* now reset this flag */  
                                       /* and continue reading */
        iRetcode = read_graphic (pData);

        if (pData->bEOF)               /* already at EOF ? */
        {
          pData->bReading = MNG_FALSE; /* then we're no longer reading */
                                       /* drop invalidly stored objects */
          drop_invalid_objects (pData);
        }
      }
      else
#endif /* MNG_SUPPORT_READ */
      {                                /* synchronize timing */
        pData->iSynctime = pData->fGettickcount (hHandle);
                                       /* resume display processing */
        iRetcode = process_display (pData);
      }
    }
    else
    {
      MNG_ERROR (pData, MNG_FUNCTIONINVALID)
    }
  }
  else
  {                                    /* synchronize timing */
    pData->iSynctime = pData->fGettickcount (hHandle);
    pData->bRunning  = MNG_TRUE;       /* it's restarted again ! */
                                       /* resume display processing */
    iRetcode = process_display (pData);
  }

  if (iRetcode)                        /* on error bail out */
    return iRetcode;

  if (pData->bSuspended)               /* read suspension ? */
  {
     iRetcode            = MNG_NEEDMOREDATA;
     pData->iSuspendtime = pData->fGettickcount ((mng_handle)pData);
  }
  else
  if (pData->bTimerset)                /* indicate timer break ? */
    iRetcode = MNG_NEEDTIMERWAIT;
  else
  if (pData->bSectionwait)             /* indicate section break ? */
    iRetcode = MNG_NEEDSECTIONWAIT;
  else
  {                                    /* no breaks = end of run */
    pData->bRunning = MNG_FALSE;

    if (pData->bFreezing)              /* trying to freeze ? */
    {                                  /* then we're there ! */
      pData->bFreezing = MNG_FALSE;
    }

    if (pData->bResetting)             /* trying to reset as well ? */
    {                                  /* full stop!!! */
      pData->bDisplaying = MNG_FALSE;

      iRetcode = mng_reset_rundata (pData);

      if (iRetcode)                    /* on error bail out */
        return iRetcode;
    }
  }
  
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_DISPLAY_RESUME, MNG_LC_END)
#endif

  return iRetcode;
}
#endif /* MNG_SUPPORT_DISPLAY */

/* ************************************************************************** */

#ifdef MNG_SUPPORT_DISPLAY
mng_retcode MNG_DECL mng_display_freeze (mng_handle hHandle)
{
  mng_datap pData;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_DISPLAY_FREEZE, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = ((mng_datap)hHandle);        /* and make it addressable */
                                       /* can we expect this call ? */
  if ((!pData->bDisplaying) || (pData->bReading))
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)

  cleanup_errors (pData);              /* cleanup previous errors */

  if (pData->bRunning)                 /* is it running ? */
  {
    mng_retcode iRetcode;

    pData->bFreezing = MNG_TRUE;       /* indicate we need to freeze */
                                       /* continue "normal" processing */
    iRetcode = mng_display_resume (hHandle);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_DISPLAY_FREEZE, MNG_LC_END)
#endif

  return MNG_NOERROR;
}
#endif /* MNG_SUPPORT_DISPLAY */

/* ************************************************************************** */

#ifdef MNG_SUPPORT_DISPLAY
mng_retcode MNG_DECL mng_display_reset (mng_handle hHandle)
{
  mng_datap   pData;
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_DISPLAY_RESET, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = ((mng_datap)hHandle);        /* and make it addressable */
                                       /* can we expect this call ? */
  if ((!pData->bDisplaying) || (pData->bReading))
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)

  if (!pData->bCacheplayback)          /* must store playback info to work!! */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)

  cleanup_errors (pData);              /* cleanup previous errors */

  if (pData->bRunning)                 /* is it running ? */
  {
    pData->bFreezing  = MNG_TRUE;      /* indicate we need to freeze */
    pData->bResetting = MNG_TRUE;      /* indicate we're about to reset too */
                                       /* continue normal processing ? */
    iRetcode = mng_display_resume (hHandle);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
  }
  else
  {                                    /* full stop!!! */
    pData->bDisplaying = MNG_FALSE;

    iRetcode = mng_reset_rundata (pData);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_DISPLAY_RESET, MNG_LC_END)
#endif

  return MNG_NOERROR;
}
#endif /* MNG_SUPPORT_DISPLAY */

/* ************************************************************************** */

#ifdef MNG_SUPPORT_DISPLAY
mng_retcode MNG_DECL mng_display_goframe (mng_handle hHandle,
                                          mng_uint32 iFramenr)
{
  mng_datap   pData;
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_DISPLAY_GOFRAME, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = ((mng_datap)hHandle);        /* and make it addressable */

  if (pData->eImagetype != mng_it_mng) /* is it an animation ? */
    MNG_ERROR (pData, MNG_NOTANANIMATION);
                                       /* can we expect this call ? */
  if ((!pData->bDisplaying) || (pData->bRunning))
    MNG_ERROR ((mng_datap)hHandle, MNG_FUNCTIONINVALID)

  if (!pData->bCacheplayback)          /* must store playback info to work!! */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)

  if (iFramenr > pData->iFramecount)   /* is the parameter within bounds ? */
    MNG_ERROR (pData, MNG_FRAMENRTOOHIGH);

  cleanup_errors (pData);              /* cleanup previous errors */

  pData->iRequestframe = iFramenr;     /* go find the requested frame then */
  iRetcode = process_display (pData);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_DISPLAY_GOFRAME, MNG_LC_END)
#endif

  return MNG_NOERROR;
}
#endif /* MNG_SUPPORT_DISPLAY */

/* ************************************************************************** */

#ifdef MNG_SUPPORT_DISPLAY
mng_retcode MNG_DECL mng_display_golayer (mng_handle hHandle,
                                          mng_uint32 iLayernr)
{
  mng_datap   pData;
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_DISPLAY_GOLAYER, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = ((mng_datap)hHandle);        /* and make it addressable */

  if (pData->eImagetype != mng_it_mng) /* is it an animation ? */
    MNG_ERROR (pData, MNG_NOTANANIMATION)
                                       /* can we expect this call ? */
  if ((!pData->bDisplaying) || (pData->bRunning))
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)

  if (!pData->bCacheplayback)          /* must store playback info to work!! */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)

  if (iLayernr > pData->iLayercount)   /* is the parameter within bounds ? */
    MNG_ERROR (pData, MNG_LAYERNRTOOHIGH)

  cleanup_errors (pData);              /* cleanup previous errors */

  pData->iRequestlayer = iLayernr;     /* go find the requested layer then */
  iRetcode = process_display (pData);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_DISPLAY_GOLAYER, MNG_LC_END)
#endif

  return MNG_NOERROR;
}
#endif /* MNG_SUPPORT_DISPLAY */

/* ************************************************************************** */

#ifdef MNG_SUPPORT_DISPLAY
mng_retcode MNG_DECL mng_display_gotime (mng_handle hHandle,
                                         mng_uint32 iPlaytime)
{
  mng_datap   pData;
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_DISPLAY_GOTIME, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = ((mng_datap)hHandle);        /* and make it addressable */

  if (pData->eImagetype != mng_it_mng) /* is it an animation ? */
    MNG_ERROR (pData, MNG_NOTANANIMATION)
                                       /* can we expect this call ? */
  if ((!pData->bDisplaying) || (pData->bRunning))
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)

  if (!pData->bCacheplayback)          /* must store playback info to work!! */
    MNG_ERROR (pData, MNG_FUNCTIONINVALID)

  if (iPlaytime > pData->iPlaytime)    /* is the parameter within bounds ? */
    MNG_ERROR (pData, MNG_PLAYTIMETOOHIGH)

  cleanup_errors (pData);              /* cleanup previous errors */

  pData->iRequesttime = iPlaytime;     /* go find the requested playtime then */
  iRetcode = process_display (pData);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_DISPLAY_GOTIME, MNG_LC_END)
#endif

  return MNG_NOERROR;
}
#endif /* MNG_SUPPORT_DISPLAY */

/* ************************************************************************** */

mng_retcode MNG_DECL mng_getlasterror (mng_handle   hHandle,
                                       mng_int8*    iSeverity,
                                       mng_chunkid* iChunkname,
                                       mng_uint32*  iChunkseq,
                                       mng_int32*   iExtra1,
                                       mng_int32*   iExtra2,
                                       mng_pchar*   zErrortext)
{
  mng_datap pData;                     /* local vars */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETLASTERROR, MNG_LC_START)
#endif

  MNG_VALIDHANDLE (hHandle)            /* check validity handle */
  pData = ((mng_datap)hHandle);        /* and make it addressable */

  *iSeverity  = pData->iSeverity;      /* return the appropriate fields */

#if defined(MNG_SUPPORT_READ) || defined(MNG_SUPPORT_WRITE)
  *iChunkname = pData->iChunkname;
  *iChunkseq  = pData->iChunkseq;
#else
  *iChunkname = MNG_UINT_HUH;
  *iChunkseq  = 0;
#endif

  *iExtra1    = pData->iErrorx1;
  *iExtra2    = pData->iErrorx2;
  *zErrortext = pData->zErrortext;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (((mng_datap)hHandle), MNG_FN_GETLASTERROR, MNG_LC_END)
#endif

  return pData->iErrorcode;            /* and the errorcode */
}

/* ************************************************************************** */
/* * end of file                                                            * */
/* ************************************************************************** */

