/* ************************************************************************** */
/* *             For conditions of distribution and use,                    * */
/* *                see copyright notice in libmng.h                        * */
/* ************************************************************************** */
/* *                                                                        * */
/* * project   : libmng                                                     * */
/* * file      : mng_display.c             copyright (c) 2000 G.Juyn        * */
/* * version   : 0.5.3                                                      * */
/* *                                                                        * */
/* * purpose   : Display management (implementation)                        * */
/* *                                                                        * */
/* * author    : G.Juyn                                                     * */
/* * web       : http://www.3-t.com                                         * */
/* * email     : mailto:info@3-t.com                                        * */
/* *                                                                        * */
/* * comment   : implementation of the display management routines          * */
/* *                                                                        * */
/* * changes   : 0.5.1 - 05/08/2000 - G.Juyn                                * */
/* *             - changed strict-ANSI stuff                                * */
/* *             0.5.1 - 05/11/2000 - G.Juyn                                * */
/* *             - added callback error-reporting support                   * */
/* *             - fixed frame_delay misalignment                           * */
/* *             0.5.1 - 05/12/2000 - G.Juyn                                * */
/* *             - added sanity check for frozen status                     * */
/* *             0.5.1 - 05/12/2000 - G.Juyn                                * */
/* *             - changed trace to macro for callback error-reporting      * */
/* *             0.5.1 - 05/13/2000 - G.Juyn                                * */
/* *             - changed display_mend to reset state to initial or SAVE   * */
/* *             - added eMNGma hack (will be removed in 1.0.0 !!!)         * */
/* *             - added TERM animation object pointer (easier reference)   * */
/* *             - added process_save & process_seek routines               * */
/* *             0.5.1 - 05/14/2000 - G.Juyn                                * */
/* *             - added save_state and restore_state for SAVE/SEEK/TERM    * */
/* *               processing                                               * */
/* *                                                                        * */
/* *             0.5.2 - 05/20/2000 - G.Juyn                                * */
/* *             - added JNG support (JHDR/JDAT)                            * */
/* *             0.5.2 - 05/23/2000 - G.Juyn                                * */
/* *             - fixed problem with DEFI clipping                         * */
/* *             0.5.2 - 05/30/2000 - G.Juyn                                * */
/* *             - added delta-image support (DHDR,PROM,IPNG,IJNG)          * */
/* *             0.5.2 - 05/31/2000 - G.Juyn                                * */
/* *             - fixed pointer confusion (contributed by Tim Rowley)      * */
/* *             0.5.2 - 06/03/2000 - G.Juyn                                * */
/* *             - fixed makeup for Linux gcc compile                       * */
/* *             0.5.2 - 06/05/2000 - G.Juyn                                * */
/* *             - added support for RGB8_A8 canvasstyle                    * */
/* *             0.5.2 - 06/09/2000 - G.Juyn                                * */
/* *             - fixed timer-handling to run with Mozilla (Tim Rowley)    * */
/* *             0.5.2 - 06/10/2000 - G.Juyn                                * */
/* *             - fixed some compilation-warnings (contrib Jason Morris)   * */
/* *                                                                        * */
/* *             0.5.3 - 06/12/2000 - G.Juyn                                * */
/* *             - fixed display of stored JNG images                       * */
/* *             0.5.3 - 06/13/2000 - G.Juyn                                * */
/* *             - fixed problem with BASI-IEND as object 0                 * */
/* *             0.5.3 - 06/16/2000 - G.Juyn                                * */
/* *             - changed progressive-display processing                   * */
/* *             0.5.3 - 06/17/2000 - G.Juyn                                * */
/* *             - changed delta-image processing                           * */
/* *             0.5.3 - 06/20/2000 - G.Juyn                                * */
/* *             - fixed some minor stuff                                   * */
/* *             0.5.3 - 06/21/2000 - G.Juyn                                * */
/* *             - added speed-modifier to timing routine                   * */
/* *             0.5.3 - 06/22/2000 - G.Juyn                                * */
/* *             - added support for PPLT chunk processing                  * */
/* *                                                                        * */
/* ************************************************************************** */

#include "libmng.h"
#include "mng_data.h"
#include "mng_error.h"
#include "mng_trace.h"
#ifdef __BORLANDC__
#pragma hdrstop
#endif
#include "mng_objects.h"
#include "mng_object_prc.h"
#include "mng_memory.h"
#include "mng_zlib.h"
#include "mng_jpeg.h"
#include "mng_cms.h"
#include "mng_pixels.h"
#include "mng_display.h"

#if defined(__BORLANDC__) && defined(MNG_STRICT_ANSI)
#pragma option -A                      /* force ANSI-C */
#endif

/* ************************************************************************** */

#ifdef MNG_INCLUDE_DISPLAY_PROCS

/* ************************************************************************** */
/* *                                                                        * */
/* * Progressive display refresh - does the actual call to refresh and sets * */
/* * the timer to allow the app to perform the actual refresh to the screen * */
/* * (eg. process its main message-loop)                                    * */
/* *                                                                        * */
/* ************************************************************************** */

mng_retcode display_progressive_refresh (mng_datap  pData,
                                         mng_uint32 iInterval)
{                                      /* tell the app to refresh */
  if ((pData->iUpdatetop < pData->iUpdatebottom) && (pData->iUpdateleft < pData->iUpdateright))
  {
    if (!pData->fRefresh (((mng_handle)pData),
                          pData->iUpdatetop,    pData->iUpdateleft,
                          pData->iUpdatebottom, pData->iUpdateright))
      MNG_ERROR (pData, MNG_APPMISCERROR)

    pData->iUpdateleft   = 0;          /* reset update-region */
    pData->iUpdateright  = 0;
    pData->iUpdatetop    = 0;
    pData->iUpdatebottom = 0;
    pData->bNeedrefresh  = MNG_FALSE;  /* reset refreshneeded indicator */

    if (iInterval)                     /* interval requested ? */
    {                                  /* setup the timer */
      if (!pData->fSettimer ((mng_handle)pData, iInterval))
        MNG_ERROR (pData, MNG_APPTIMERERROR)

      pData->bTimerset = MNG_TRUE;     /* and indicate so */
    }
  }

  return MNG_NOERROR;
}

/* ************************************************************************** */
/* *                                                                        * */
/* * Generic display routines                                               * */
/* *                                                                        * */
/* ************************************************************************** */

/* TODO: this routine needs some redesigning; make sure a delay is called at least
   every 10th of a second to prevent hanging; also provide for slow reading
   bandwidth & multiple timer-wraparounds */

mng_retcode interframe_delay (mng_datap pData)
{
  mng_uint32 iWaitfor;
  mng_uint32 iInterval;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INTERFRAME_DELAY, MNG_LC_START)
#endif

  if (!pData->bRunning)                /* sanity check for frozen status */
    MNG_WARNING (pData, MNG_IMAGEFROZEN)

  if (pData->iFramedelay > 0)          /* let the app refresh first */
  {
    if ((pData->iUpdatetop < pData->iUpdatebottom) && (pData->iUpdateleft < pData->iUpdateright))
      if (!pData->fRefresh (((mng_handle)pData),
                            pData->iUpdatetop,    pData->iUpdateleft,
                            pData->iUpdatebottom, pData->iUpdateright))
        MNG_ERROR (pData, MNG_APPMISCERROR)

    pData->iUpdateleft   = 0;          /* reset update-region */
    pData->iUpdateright  = 0;
    pData->iUpdatetop    = 0;
    pData->iUpdatebottom = 0;
    pData->bNeedrefresh  = MNG_FALSE;  /* reset refreshneeded indicator */
  }
                                       /* get current tickcount */
  pData->iRuntime = pData->fGettickcount ((mng_handle)pData);
                                       /* tickcount wrapped around ? */
  if (pData->iRuntime < pData->iStarttime)
    pData->iRuntime = pData->iRuntime + ~pData->iStarttime + 1;
  else
    pData->iRuntime = pData->iRuntime - pData->iStarttime;

  /* NOTE that a second wraparound of the tickcount will seriously f**k things
     up here! (but that's somewhere past at least 48 days on Windoze...) */
  /* TODO: yeah, what to do ??? */

  if (pData->iTicks)                   /* what are we aiming for */
  {
    switch (pData->iSpeed)             /* honor speed modifier */
    {
      case mng_st_fast :
        {
          iWaitfor = (mng_uint32)(( 500 * pData->iFramedelay) / pData->iTicks);
          break;
        }
      case mng_st_slow :
        {
          iWaitfor = (mng_uint32)((3000 * pData->iFramedelay) / pData->iTicks);
          break;
        }
      case mng_st_slowest :
        {
          iWaitfor = (mng_uint32)((8000 * pData->iFramedelay) / pData->iTicks);
          break;
        }
      default :
        {
          iWaitfor = (mng_uint32)((1000 * pData->iFramedelay) / pData->iTicks);
        }
    }

    iWaitfor = pData->iFrametime + iWaitfor;
  }
  else
    iWaitfor = pData->iFrametime;

  if (pData->iRuntime < iWaitfor)      /* delay necessary ? */
    iInterval = iWaitfor - pData->iRuntime;
  else
    iInterval = 1;                     /* force app to process messageloop */
                                       /* set the timer */
  if (!pData->fSettimer ((mng_handle)pData, iInterval))
    MNG_ERROR (pData, MNG_APPTIMERERROR)

  pData->bTimerset   = MNG_TRUE;       /* and indicate so */
  pData->iFrametime  = iWaitfor;       /* increase frametime in advance */
                                       /* setup for next delay */
  pData->iFramedelay = pData->iNextdelay;

  /* TODO: some provision to compensate during slow file-access (such as
     on low-bandwidth network connections);
     only necessary during read-processing! */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INTERFRAME_DELAY, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

void set_display_routine (mng_datap pData)
{
  switch (pData->iCanvasstyle)         /* determine display routine */
  {
    case MNG_CANVAS_RGB8    : { pData->fDisplayrow = (mng_ptr)display_rgb8;    break; }
    case MNG_CANVAS_RGBA8   : { pData->fDisplayrow = (mng_ptr)display_rgba8;   break; }
    case MNG_CANVAS_ARGB8   : { pData->fDisplayrow = (mng_ptr)display_argb8;   break; }
    case MNG_CANVAS_RGB8_A8 : { pData->fDisplayrow = (mng_ptr)display_rgb8_a8; break; }
    case MNG_CANVAS_BGR8    : { pData->fDisplayrow = (mng_ptr)display_bgr8;    break; }
    case MNG_CANVAS_BGRA8   : { pData->fDisplayrow = (mng_ptr)display_bgra8;   break; }
    case MNG_CANVAS_ABGR8   : { pData->fDisplayrow = (mng_ptr)display_abgr8;   break; }
/*    case MNG_CANVAS_RGB16   : { pData->fDisplayrow = (mng_ptr)display_rgb16;   break; } */
/*    case MNG_CANVAS_RGBA16  : { pData->fDisplayrow = (mng_ptr)display_rgba16;  break; } */
/*    case MNG_CANVAS_ARGB16  : { pData->fDisplayrow = (mng_ptr)display_argb16;  break; } */
/*    case MNG_CANVAS_BGR16   : { pData->fDisplayrow = (mng_ptr)display_bgr16;   break; } */
/*    case MNG_CANVAS_BGRA16  : { pData->fDisplayrow = (mng_ptr)display_bgra16;  break; } */
/*    case MNG_CANVAS_ABGR16  : { pData->fDisplayrow = (mng_ptr)display_abgr16;  break; } */
/*    case MNG_CANVAS_INDEX8  : { pData->fDisplayrow = (mng_ptr)display_index8;  break; } */
/*    case MNG_CANVAS_INDEXA8 : { pData->fDisplayrow = (mng_ptr)display_indexa8; break; } */
/*    case MNG_CANVAS_AINDEX8 : { pData->fDisplayrow = (mng_ptr)display_aindex8; break; } */
/*    case MNG_CANVAS_GRAY8   : { pData->fDisplayrow = (mng_ptr)display_gray8;   break; } */
/*    case MNG_CANVAS_GRAY16  : { pData->fDisplayrow = (mng_ptr)display_gray16;  break; } */
/*    case MNG_CANVAS_GRAYA8  : { pData->fDisplayrow = (mng_ptr)display_graya8;  break; } */
/*    case MNG_CANVAS_GRAYA16 : { pData->fDisplayrow = (mng_ptr)display_graya16; break; } */
/*    case MNG_CANVAS_AGRAY8  : { pData->fDisplayrow = (mng_ptr)display_agray8;  break; } */
/*    case MNG_CANVAS_AGRAY16 : { pData->fDisplayrow = (mng_ptr)display_agray16; break; } */
/*    case MNG_CANVAS_DX15    : { pData->fDisplayrow = (mng_ptr)display_dx15;    break; } */
/*    case MNG_CANVAS_DX16    : { pData->fDisplayrow = (mng_ptr)display_dx16;    break; } */
  }

  return;
}

/* ************************************************************************** */

mng_retcode load_bkgdlayer (mng_datap pData)
{
  mng_int32   iY;
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_LOAD_BKGDLAYER, MNG_LC_START)
#endif

  pData->iDestl   = 0;                 /* determine clipping region */
  pData->iDestt   = 0;
  pData->iDestr   = pData->iWidth;
  pData->iDestb   = pData->iHeight;

  if (pData->bFrameclipping)           /* frame clipping specified ? */
  {
    pData->iDestl = MAX_COORD (pData->iDestl,  pData->iFrameclipl);
    pData->iDestt = MAX_COORD (pData->iDestt,  pData->iFrameclipt);
    pData->iDestr = MIN_COORD (pData->iDestr,  pData->iFrameclipr);
    pData->iDestb = MIN_COORD (pData->iDestb,  pData->iFrameclipb);
  }
                                       /* anything to clear ? */
  if ((pData->iDestr >= pData->iDestl) && (pData->iDestb >= pData->iDestt))
  {
    pData->iPass       = -1;           /* these are the object's dimensions now */
    pData->iRow        = 0;
    pData->iRowinc     = 1;
    pData->iCol        = 0;
    pData->iColinc     = 1;
    pData->iRowsamples = pData->iWidth;
    pData->iRowsize    = pData->iRowsamples << 2;
    pData->bIsRGBA16   = MNG_FALSE;    /* let's keep it simple ! */
    pData->bIsOpaque   = MNG_TRUE;

    pData->iSourcel    = 0;            /* source relative to destination */
    pData->iSourcer    = pData->iDestr - pData->iDestl;
    pData->iSourcet    = 0;
    pData->iSourceb    = pData->iDestb - pData->iDestt;

    set_display_routine (pData);       /* determine display routine */
                                       /* default restore using preset BG color */
    pData->fRestbkgdrow = (mng_ptr)restore_bkgd_bgcolor;

    if (pData->fGetbkgdline)           /* background-canvas-access callback set ? */
    {
      switch (pData->iBkgdstyle)
      {
        case MNG_CANVAS_RGB8    : { pData->fRestbkgdrow = (mng_ptr)restore_bkgd_rgb8;    break; }
        case MNG_CANVAS_BGR8    : { pData->fRestbkgdrow = (mng_ptr)restore_bkgd_bgr8;    break; }
/*        case MNG_CANVAS_RGB16   : { pData->fRestbkgdrow = (mng_ptr)restore_bkgd_rgb16;   break; } */
/*        case MNG_CANVAS_BGR16   : { pData->fRestbkgdrow = (mng_ptr)restore_bkgd_bgr16;   break; } */
/*        case MNG_CANVAS_INDEX8  : { pData->fRestbkgdrow = (mng_ptr)restore_bkgd_index8;  break; } */
/*        case MNG_CANVAS_GRAY8   : { pData->fRestbkgdrow = (mng_ptr)restore_bkgd_gray8;   break; } */
/*        case MNG_CANVAS_GRAY16  : { pData->fRestbkgdrow = (mng_ptr)restore_bkgd_gray16;  break; } */
/*        case MNG_CANVAS_DX15    : { pData->fRestbkgdrow = (mng_ptr)restore_bkgd_dx15;    break; } */
/*        case MNG_CANVAS_DX16    : { pData->fRestbkgdrow = (mng_ptr)restore_bkgd_dx16;    break; } */
      }
    }

    if (pData->bHasBACK)
    {                                  /* background image ? */
      if ((pData->iBACKmandatory & 0x02) && (pData->iBACKimageid))
        pData->fRestbkgdrow = (mng_ptr)restore_bkgd_backimage;
      else                             /* background color ? */
      if (pData->iBACKmandatory & 0x01)
        pData->fRestbkgdrow = (mng_ptr)restore_bkgd_backcolor;

    }

    pData->fCorrectrow = 0;            /* default no color-correction */


    /* TODO: determine color correction; this is tricky;
       the BACK color is treated differently as the image;
       it probably requires a rewrite of the logic here... */


                                       /* get a temporary row-buffer */
    MNG_ALLOC (pData, pData->pRGBArow, pData->iRowsize)

    iY       = pData->iDestt;          /* this is where we start */
    iRetcode = MNG_NOERROR;            /* so far, so good */

    while ((!iRetcode) && (iY < pData->iDestb))
    {                                  /* restore a background row */
      iRetcode = ((mng_restbkgdrow)pData->fRestbkgdrow) (pData);
                                       /* color correction ? */
      if ((!iRetcode) && (pData->fCorrectrow))
        iRetcode = ((mng_correctrow)pData->fCorrectrow) (pData);

      if (!iRetcode)                   /* so... display it */
        iRetcode = ((mng_displayrow)pData->fDisplayrow) (pData);

      if (!iRetcode)
        iRetcode = next_row (pData);   /* adjust variables for next row */

      iY++;                            /* and next line */
    }
                                       /* drop the temporary row-buffer */
    MNG_FREE (pData, pData->pRGBArow, pData->iRowsize)

    if (iRetcode)                      /* on error bail out */
      return iRetcode;

  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_LOAD_BKGDLAYER, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode next_frame (mng_datap  pData,
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
  mng_retcode iRetcode = MNG_NOERROR;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_NEXT_FRAME, MNG_LC_START)
#endif

  if (!pData->iBreakpoint)             /* no previous break here ? */
  {
    mng_uint8 iOldmode = pData->iFramemode;
                                       /* interframe delay required ? */
    if ((iOldmode == 2) || (iOldmode == 4))
    {
      if (pData->iFrameseq)
        iRetcode = interframe_delay (pData);
      else
        pData->iFramedelay = pData->iNextdelay;
    }

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
                                       /* now we'll assume we're in the next frame! */
    if (iFramemode)                    /* save the new framing mode ? */
    {
      pData->iFRAMmode  = iFramemode;
      pData->iFramemode = iFramemode;
    }
    else                               /* reload default */
      pData->iFramemode = pData->iFRAMmode;

    if (iChangedelay)                  /* delay changed ? */
    {
      pData->iNextdelay = iDelay;      /* for *after* next subframe */

      if ((iOldmode == 2) || (iOldmode == 4))
        pData->iFramedelay = pData->iNextdelay;

      if (iChangedelay == 2)           /* also overall ? */
        pData->iFRAMdelay = iDelay;
    }
    else
    {                                  /* reload default */
      pData->iNextdelay = pData->iFRAMdelay;

      if ((iOldmode == 2) || (iOldmode == 4))
        pData->iFramedelay = pData->iNextdelay;
    }

    if (iChangetimeout)                /* timeout changed ? */
    {                                  /* for next subframe */
      pData->iFrametimeout = iTimeout;

      if ((iChangetimeout == 2) ||     /* also overall ? */
          (iChangetimeout == 4) ||
          (iChangetimeout == 6) ||
          (iChangetimeout == 8))
        pData->iFRAMtimeout = iTimeout;
    }
    else                               /* reload default */
      pData->iFrametimeout = pData->iFRAMtimeout;

    if (iChangeclipping)               /* clipping changed ? */
    {
      pData->bFrameclipping = MNG_TRUE;

      if (!iCliptype)                  /* absolute ? */
      {
        pData->iFrameclipl = iClipl;
        pData->iFrameclipr = iClipr;
        pData->iFrameclipt = iClipt;
        pData->iFrameclipb = iClipb;
      }
      else                             /* relative */
      {
        pData->iFrameclipl = pData->iFrameclipl + iClipl;
        pData->iFrameclipr = pData->iFrameclipr + iClipr;
        pData->iFrameclipt = pData->iFrameclipt + iClipt;
        pData->iFrameclipb = pData->iFrameclipb + iClipb;
      }

      if (iChangeclipping == 2)        /* also overall ? */
      {
        pData->bFRAMclipping = MNG_TRUE;

        if (!iCliptype)                /* absolute ? */
        {
          pData->iFRAMclipl = iClipl;
          pData->iFRAMclipr = iClipr;
          pData->iFRAMclipt = iClipt;
          pData->iFRAMclipb = iClipb;
        }
        else                           /* relative */
        {
          pData->iFRAMclipl = pData->iFRAMclipl + iClipl;
          pData->iFRAMclipr = pData->iFRAMclipr + iClipr;
          pData->iFRAMclipt = pData->iFRAMclipt + iClipt;
          pData->iFRAMclipb = pData->iFRAMclipb + iClipb;
        }
      }
    }
    else
    {                                  /* reload defaults */
      pData->bFrameclipping = pData->bFRAMclipping;
      pData->iFrameclipl    = pData->iFRAMclipl;
      pData->iFrameclipr    = pData->iFRAMclipr;
      pData->iFrameclipt    = pData->iFRAMclipt;
      pData->iFrameclipb    = pData->iFRAMclipb;
    }
  }

  if (!pData->bTimerset)               /* timer still off ? */
  {
    if ((pData->iFramemode == 4) ||    /* insert background layer after a new frame */
        (!pData->iLayerseq))           /* and certainly before the very first layer */
      iRetcode = load_bkgdlayer (pData);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;

    pData->iFrameseq++;                /* count the frame ! */
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_NEXT_FRAME, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode next_layer (mng_datap pData)
{
  mng_imagep  pImage;
  mng_retcode iRetcode = MNG_NOERROR;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_NEXT_LAYER, MNG_LC_START)
#endif

  if (!pData->iBreakpoint)             /* no previous break here ? */
  {
    if ((pData->iLayerseq) &&          /* interframe delay required ? */
        ((pData->iFramemode == 1) || (pData->iFramemode == 3)))
      iRetcode = interframe_delay (pData);
    else
      pData->iFramedelay = pData->iNextdelay;

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
  }

  if (!pData->bTimerset)               /* timer still off ? */
  {
    if (!pData->iLayerseq)             /* restore background for the very first layer ? */
    {
      iRetcode = load_bkgdlayer (pData);
      pData->iLayerseq++;              /* and it counts as a layer then ! */
    }
    else
    if (pData->iFramemode == 3)        /* restore background for each layer ? */
      iRetcode = load_bkgdlayer (pData);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;

    if (pData->bHasDHDR)               /* processing a delta-image ? */
      pImage = (mng_imagep)pData->pDeltaImage;
    else
      pImage = (mng_imagep)pData->pCurrentobj;

    if (!pImage)                       /* not an active object ? */
      pImage = (mng_imagep)pData->pObjzero;
                                       /* determine display rectangle */
    pData->iDestl   = MAX_COORD ((mng_int32)0,   pImage->iPosx);
    pData->iDestt   = MAX_COORD ((mng_int32)0,   pImage->iPosy);
                                       /* is it a valid buffer ? */
    if ((pImage->pImgbuf->iWidth) && (pImage->pImgbuf->iHeight))
    {
      pData->iDestr = MIN_COORD ((mng_int32)pData->iWidth,
                                 pImage->iPosx + (mng_int32)pImage->pImgbuf->iWidth );
      pData->iDestb = MIN_COORD ((mng_int32)pData->iHeight,
                                 pImage->iPosy + (mng_int32)pImage->pImgbuf->iHeight);
    }
    else                               /* it's a single image ! */
    {
      pData->iDestr = MIN_COORD ((mng_int32)pData->iWidth,
                                 (mng_int32)pData->iDatawidth );
      pData->iDestb = MIN_COORD ((mng_int32)pData->iHeight,
                                 (mng_int32)pData->iDataheight);
    }

    if (pData->bFrameclipping)         /* frame clipping specified ? */
    {
      pData->iDestl = MAX_COORD (pData->iDestl,  pData->iFrameclipl);
      pData->iDestt = MAX_COORD (pData->iDestt,  pData->iFrameclipt);
      pData->iDestr = MIN_COORD (pData->iDestr,  pData->iFrameclipr);
      pData->iDestb = MIN_COORD (pData->iDestb,  pData->iFrameclipb);
    }

    if (pImage->bClipped)              /* is the image clipped itself ? */
    {
      pData->iDestl = MAX_COORD (pData->iDestl,  pImage->iClipl);
      pData->iDestt = MAX_COORD (pData->iDestt,  pImage->iClipt);
      pData->iDestr = MIN_COORD (pData->iDestr,  pImage->iClipr);
      pData->iDestb = MIN_COORD (pData->iDestb,  pImage->iClipb);
    }
                                       /* determine source starting point */
    pData->iSourcel = MAX_COORD ((mng_int32)0,   pData->iDestl - pImage->iPosx);
    pData->iSourcet = MAX_COORD ((mng_int32)0,   pData->iDestt - pImage->iPosy);

    if ((pImage->pImgbuf->iWidth) && (pImage->pImgbuf->iHeight))
    {                                  /* and maximum size  */
      pData->iSourcer = MIN_COORD ((mng_int32)pImage->pImgbuf->iWidth,
                                   pData->iSourcel + pData->iDestr - pData->iDestl);
      pData->iSourceb = MIN_COORD ((mng_int32)pImage->pImgbuf->iHeight,
                                   pData->iSourcet + pData->iDestb - pData->iDestt);
    }
    else                               /* it's a single image ! */
    {
      pData->iSourcer = pData->iSourcel + pData->iDestr - pData->iDestl;
      pData->iSourceb = pData->iSourcet + pData->iDestb - pData->iDestt;
    }

    pData->iLayerseq++;                /* count the layer ! */
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_NEXT_LAYER, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode display_image (mng_datap  pData,
                           mng_imagep pImage,
                           mng_bool   bLayeradvanced)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DISPLAY_IMAGE, MNG_LC_START)
#endif

  pData->pRetrieveobj = pImage;        /* so retrieve-row and color-correction can find it */

  if (!bLayeradvanced)                 /* need to advance the layer ? */
  {
    mng_imagep pSave    = pData->pCurrentobj;
    pData->pCurrentobj  = pImage;
    next_layer (pData);                /* advance to next layer */
    pData->pCurrentobj  = pSave;
  }

  if (!pData->bTimerset)               /* all systems still go ? */
  {
    pData->iBreakpoint = 0;            /* let's make absolutely sure... */
                                       /* anything to display ? */
    if ((pData->iDestr >= pData->iDestl) && (pData->iDestb >= pData->iDestt))
    {
      mng_int32   iY;
#ifdef MNG_NO_CMS
      mng_retcode iRetcode = MNG_NOERROR;
#else
      mng_retcode iRetcode;
#endif

      set_display_routine (pData);     /* determine display routine */
                                       /* and image-buffer retrieval routine */
      switch (pImage->pImgbuf->iColortype)
      {
        case  0 : { if (pImage->pImgbuf->iBitdepth > 8)
                      pData->fRetrieverow = (mng_ptr)retrieve_g16;
                    else
                      pData->fRetrieverow = (mng_ptr)retrieve_g8;

                    pData->bIsOpaque      = (mng_bool)(!pImage->pImgbuf->bHasTRNS);
                    break;
                  }

        case  2 : { if (pImage->pImgbuf->iBitdepth > 8)
                      pData->fRetrieverow = (mng_ptr)retrieve_rgb16;
                    else
                      pData->fRetrieverow = (mng_ptr)retrieve_rgb8;

                    pData->bIsOpaque      = (mng_bool)(!pImage->pImgbuf->bHasTRNS);
                    break;
                  }


        case  3 : { pData->fRetrieverow   = (mng_ptr)retrieve_idx8;
                    pData->bIsOpaque      = (mng_bool)(!pImage->pImgbuf->bHasTRNS);
                    break;
                  }


        case  4 : { if (pImage->pImgbuf->iBitdepth > 8)
                      pData->fRetrieverow = (mng_ptr)retrieve_ga16;
                    else
                      pData->fRetrieverow = (mng_ptr)retrieve_ga8;

                    pData->bIsOpaque      = MNG_FALSE;
                    break;
                  }


        case  6 : { if (pImage->pImgbuf->iBitdepth > 8)
                      pData->fRetrieverow = (mng_ptr)retrieve_rgba16;
                    else
                      pData->fRetrieverow = (mng_ptr)retrieve_rgba8;

                    pData->bIsOpaque      = MNG_FALSE;
                    break;
                  }

        case  8 : { if (pImage->pImgbuf->iBitdepth > 8)
                      pData->fRetrieverow = (mng_ptr)retrieve_g16;
                    else
                      pData->fRetrieverow = (mng_ptr)retrieve_g8;

                    pData->bIsOpaque      = MNG_TRUE;
                    break;
                  }

        case 10 : { if (pImage->pImgbuf->iBitdepth > 8)
                      pData->fRetrieverow = (mng_ptr)retrieve_rgb16;
                    else
                      pData->fRetrieverow = (mng_ptr)retrieve_rgb8;

                    pData->bIsOpaque      = MNG_TRUE;
                    break;
                  }


        case 12 : { if (pImage->pImgbuf->iBitdepth > 8)
                      pData->fRetrieverow = (mng_ptr)retrieve_ga16;
                    else
                      pData->fRetrieverow = (mng_ptr)retrieve_ga8;

                    pData->bIsOpaque      = MNG_FALSE;
                    break;
                  }


        case 14 : { if (pImage->pImgbuf->iBitdepth > 8)
                      pData->fRetrieverow = (mng_ptr)retrieve_rgba16;
                    else
                      pData->fRetrieverow = (mng_ptr)retrieve_rgba8;

                    pData->bIsOpaque      = MNG_FALSE;
                    break;
                  }

      }

      pData->iPass       = -1;         /* these are the object's dimensions now */
      pData->iRow        = pData->iSourcet;
      pData->iRowinc     = 1;
      pData->iCol        = 0;
      pData->iColinc     = 1;
      pData->iRowsamples = pImage->pImgbuf->iWidth;
      pData->iRowsize    = pData->iRowsamples << 2;
      pData->bIsRGBA16   = MNG_FALSE;
                                       /* adjust for 16-bit object ? */
      if (pImage->pImgbuf->iBitdepth > 8)
      {
        pData->bIsRGBA16 = MNG_TRUE;
        pData->iRowsize  = pData->iRowsamples << 3;
      }

      pData->fCorrectrow = 0;          /* default no color-correction */

#ifndef MNG_NO_CMS
#if defined(MNG_FULL_CMS)              /* determine color-management routine */
      iRetcode = init_full_cms_object   (pData);
#elif defined(MNG_GAMMA_ONLY)
      iRetcode = init_gamma_only_object (pData);
#elif defined(MNG_APP_CMS)
      iRetcode = init_app_cms_object    (pData);
#endif
      if (iRetcode)                    /* on error bail out */
        return iRetcode;
#endif /* !MNG_NO_CMS */
                                       /* get a temporary row-buffer */
      MNG_ALLOC (pData, pData->pRGBArow, pData->iRowsize)

      iY = pData->iSourcet;            /* this is where we start */

      while ((!iRetcode) && (iY < pData->iSourceb))
      {                                /* get a row */
        iRetcode = ((mng_retrieverow)pData->fRetrieverow) (pData);
                                       /* color correction ? */
        if ((!iRetcode) && (pData->fCorrectrow))
          iRetcode = ((mng_correctrow)pData->fCorrectrow) (pData);

        if (!iRetcode)                 /* so... display it */
          iRetcode = ((mng_displayrow)pData->fDisplayrow) (pData);

        if (!iRetcode)
          iRetcode = next_row (pData); /* adjust variables for next row */

        iY++;                          /* and next line */
      }
                                       /* drop the temporary row-buffer */
      MNG_FREE (pData, pData->pRGBArow, pData->iRowsize)

      if (iRetcode)                    /* on error bail out */
        return iRetcode;
    }
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DISPLAY_IMAGE, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* whehehe, this is good ! */
}

/* ************************************************************************** */

mng_retcode execute_delta_image (mng_datap  pData,
                                 mng_imagep pTarget,
                                 mng_imagep pDelta)
{
  mng_imagedatap pBuftarget = pTarget->pImgbuf;
  mng_imagedatap pBufdelta  = pDelta->pImgbuf;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_EXECUTE_DELTA_IMAGE, MNG_LC_START)
#endif

  if (pBufdelta->bHasPLTE)             /* palette in delta ? */
  {
    mng_uint32 iX;
                                       /* new palette larger than old one ? */
    if ((!pBuftarget->bHasPLTE) || (pBuftarget->iPLTEcount < pBufdelta->iPLTEcount))
      pBuftarget->iPLTEcount = pBufdelta->iPLTEcount;
                                       /* it's definitely got a PLTE now */
    pBuftarget->bHasPLTE = MNG_TRUE;

    for (iX = 0; iX < pBufdelta->iPLTEcount; iX++)
    {
      pBuftarget->aPLTEentries[iX].iRed   = pBufdelta->aPLTEentries[iX].iRed;
      pBuftarget->aPLTEentries[iX].iGreen = pBufdelta->aPLTEentries[iX].iGreen;
      pBuftarget->aPLTEentries[iX].iBlue  = pBufdelta->aPLTEentries[iX].iBlue;
    }
  }

  if (pBufdelta->bHasTRNS)             /* cheap transparency in delta ? */
  {
    switch (pData->iColortype)         /* drop it into the target */
    {
      case 0: {                        /* gray */
                pBuftarget->iTRNSgray  = pBufdelta->iTRNSgray;
                pBuftarget->iTRNSred   = 0;
                pBuftarget->iTRNSgreen = 0;
                pBuftarget->iTRNSblue  = 0;
                pBuftarget->iTRNScount = 0;
                break;
              }
      case 2: {                        /* rgb */
                pBuftarget->iTRNSgray  = 0;
                pBuftarget->iTRNSred   = pBufdelta->iTRNSred;
                pBuftarget->iTRNSgreen = pBufdelta->iTRNSgreen;
                pBuftarget->iTRNSblue  = pBufdelta->iTRNSblue;
                pBuftarget->iTRNScount = 0;
                break;
              }
      case 3: {                        /* indexed */
                pBuftarget->iTRNSgray  = 0;
                pBuftarget->iTRNSred   = 0;
                pBuftarget->iTRNSgreen = 0;
                pBuftarget->iTRNSblue  = 0;
                                       /* existing range smaller than new one ? */
                if ((!pBuftarget->bHasTRNS) || (pBuftarget->iTRNScount < pBufdelta->iTRNScount))
                  pBuftarget->iTRNScount = pBufdelta->iTRNScount;

                MNG_COPY (&pBuftarget->aTRNSentries, &pBufdelta->aTRNSentries, pBufdelta->iTRNScount)
                break;
              }
    }

    pBuftarget->bHasTRNS = MNG_TRUE;   /* tell it it's got a tRNS now */
  }

  
  /* TODO: copy bKGD (if it exists!) */


  if (pBufdelta->bHasGAMA)             /* gamma in source ? */
  {
    pBuftarget->bHasGAMA = MNG_TRUE;   /* drop it onto the target */
    pBuftarget->iGamma   = pBufdelta->iGamma;
  }

  if (pBufdelta->bHasCHRM)             /* chroma in delta ? */
  {                                    /* drop it onto the target */
    pBuftarget->bHasCHRM       = MNG_TRUE;
    pBuftarget->iWhitepointx   = pBufdelta->iWhitepointx;
    pBuftarget->iWhitepointy   = pBufdelta->iWhitepointy;
    pBuftarget->iPrimaryredx   = pBufdelta->iPrimaryredx;
    pBuftarget->iPrimaryredy   = pBufdelta->iPrimaryredy;
    pBuftarget->iPrimarygreenx = pBufdelta->iPrimarygreenx;
    pBuftarget->iPrimarygreeny = pBufdelta->iPrimarygreeny;
    pBuftarget->iPrimarybluex  = pBufdelta->iPrimarybluex;
    pBuftarget->iPrimarybluey  = pBufdelta->iPrimarybluey;
  }

  if (pBufdelta->bHasSRGB)             /* sRGB in delta ? */
  {                                    /* drop it onto the target */
    pBuftarget->bHasSRGB         = MNG_TRUE;
    pBuftarget->iRenderingintent = pBufdelta->iRenderingintent;
  }

  if (pBufdelta->bHasICCP)             /* ICC profile in delta ? */
  {
    pBuftarget->bHasICCP = MNG_TRUE;   /* drop it onto the target */

    if (pBuftarget->pProfile)          /* profile existed ? */
      MNG_FREEX (pData, pBuftarget->pProfile, pBuftarget->iProfilesize)
                                       /* allocate a buffer & copy it */
    MNG_ALLOC (pData, pBuftarget->pProfile, pBufdelta->iProfilesize)
    MNG_COPY  (pBuftarget->pProfile, pBufdelta->pProfile, pBufdelta->iProfilesize)
                                       /* store it's length as well */
    pBuftarget->iProfilesize = pBufdelta->iProfilesize;
  }

  if (!pData->bDeltaimmediate)         /* need to execute delta pixels ? */
  {


    /* TODO: execute delta pixels (if they exist!) */


  }  

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_EXECUTE_DELTA_IMAGE, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode save_state (mng_datap pData)
{
  mng_savedatap pSave;
  mng_imagep    pImage;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_SAVE_STATE, MNG_LC_START)
#endif

  if (pData->pSavedata)                /* sanity check */
    MNG_ERROR (pData, MNG_INTERNALERROR)
                                       /* get a buffer for saving */
  MNG_ALLOC (pData, pData->pSavedata, sizeof (mng_savedata))

  pSave = pData->pSavedata;            /* address it more directly */
                                       /* and copy global data from the main struct */
#if defined(MNG_SUPPORT_READ) || defined(MNG_SUPPORT_WRITE)
  pSave->bHasglobalPLTE       = pData->bHasglobalPLTE;
  pSave->bHasglobalTRNS       = pData->bHasglobalTRNS;
  pSave->bHasglobalGAMA       = pData->bHasglobalGAMA;
  pSave->bHasglobalCHRM       = pData->bHasglobalCHRM;
  pSave->bHasglobalSRGB       = pData->bHasglobalSRGB;
  pSave->bHasglobalICCP       = pData->bHasglobalICCP;
  pSave->bHasglobalBKGD       = pData->bHasglobalBKGD;
#endif /* MNG_SUPPORT_READ || MNG_SUPPORT_WRITE */

  pSave->iDEFIobjectid        = pData->iDEFIobjectid;
  pSave->iDEFIdonotshow       = pData->iDEFIdonotshow;
  pSave->iDEFIconcrete        = pData->iDEFIconcrete;
  pSave->bDEFIhasloca         = pData->bDEFIhasloca;
  pSave->iDEFIlocax           = pData->iDEFIlocax;
  pSave->iDEFIlocay           = pData->iDEFIlocay;
  pSave->bDEFIhasclip         = pData->bDEFIhasclip;
  pSave->iDEFIclipl           = pData->iDEFIclipl;
  pSave->iDEFIclipr           = pData->iDEFIclipr;
  pSave->iDEFIclipt           = pData->iDEFIclipt;
  pSave->iDEFIclipb           = pData->iDEFIclipb;

  pSave->iBACKred             = pData->iBACKred;
  pSave->iBACKgreen           = pData->iBACKgreen;
  pSave->iBACKblue            = pData->iBACKblue;
  pSave->iBACKmandatory       = pData->iBACKmandatory;
  pSave->iBACKimageid         = pData->iBACKimageid;
  pSave->iBACKtile            = pData->iBACKtile;

  pSave->iFRAMmode            = pData->iFRAMmode;
  pSave->iFRAMdelay           = pData->iFRAMdelay;
  pSave->iFRAMtimeout         = pData->iFRAMtimeout;
  pSave->bFRAMclipping        = pData->bFRAMclipping;
  pSave->iFRAMclipl           = pData->iFRAMclipl;
  pSave->iFRAMclipr           = pData->iFRAMclipr;
  pSave->iFRAMclipt           = pData->iFRAMclipt;
  pSave->iFRAMclipb           = pData->iFRAMclipb;

  pSave->iGlobalPLTEcount     = pData->iGlobalPLTEcount;

  MNG_COPY (&pSave->aGlobalPLTEentries, &pData->aGlobalPLTEentries, sizeof (mng_rgbpaltab))

  pSave->iGlobalTRNSrawlen    = pData->iGlobalTRNSrawlen;
  MNG_COPY (&pSave->aGlobalTRNSrawdata, &pData->aGlobalTRNSrawdata, 256)

  pSave->iGlobalGamma         = pData->iGlobalGamma;

  pSave->iGlobalWhitepointx   = pData->iGlobalWhitepointx;
  pSave->iGlobalWhitepointy   = pData->iGlobalWhitepointy;
  pSave->iGlobalPrimaryredx   = pData->iGlobalPrimaryredx;
  pSave->iGlobalPrimaryredy   = pData->iGlobalPrimaryredy;
  pSave->iGlobalPrimarygreenx = pData->iGlobalPrimarygreenx;
  pSave->iGlobalPrimarygreeny = pData->iGlobalPrimarygreeny;
  pSave->iGlobalPrimarybluex  = pData->iGlobalPrimarybluex;
  pSave->iGlobalPrimarybluey  = pData->iGlobalPrimarybluey;

  pSave->iGlobalRendintent    = pData->iGlobalRendintent;

  pSave->iGlobalProfilesize   = pData->iGlobalProfilesize;

  if (pSave->iGlobalProfilesize)       /* has a profile ? */
  {                                    /* then copy that ! */
    MNG_ALLOC (pData, pSave->pGlobalProfile, pSave->iGlobalProfilesize)
    MNG_COPY (pSave->pGlobalProfile, pData->pGlobalProfile, pSave->iGlobalProfilesize)
  }

  pSave->iGlobalBKGDred       = pData->iGlobalBKGDred;
  pSave->iGlobalBKGDgreen     = pData->iGlobalBKGDgreen;
  pSave->iGlobalBKGDblue      = pData->iGlobalBKGDblue;

                                       /* freeze current image objects */
  pImage = (mng_imagep)pData->pFirstimgobj;

  while (pImage)
  {                                    /* freeze the object AND it's buffer */
    pImage->bFrozen          = MNG_TRUE;
    pImage->pImgbuf->bFrozen = MNG_TRUE;
                                       /* neeeext */
    pImage = (mng_imagep)pImage->sHeader.pNext;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_SAVE_STATE, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode restore_state (mng_datap pData)
{
  mng_savedatap pSave;
  mng_imagep    pImage;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_RESTORE_STATE, MNG_LC_START)
#endif

  if (pData->pSavedata)                /* do we have a saved state ? */
  {
    pSave = pData->pSavedata;          /* address it more directly */
                                       /* and copy it back to the main struct */
#if defined(MNG_SUPPORT_READ) || defined(MNG_SUPPORT_WRITE)
    pData->bHasglobalPLTE       = pSave->bHasglobalPLTE;
    pData->bHasglobalTRNS       = pSave->bHasglobalTRNS;
    pData->bHasglobalGAMA       = pSave->bHasglobalGAMA;
    pData->bHasglobalCHRM       = pSave->bHasglobalCHRM;
    pData->bHasglobalSRGB       = pSave->bHasglobalSRGB;
    pData->bHasglobalICCP       = pSave->bHasglobalICCP;
    pData->bHasglobalBKGD       = pSave->bHasglobalBKGD;
#endif /* MNG_SUPPORT_READ || MNG_SUPPORT_WRITE */

    pData->iDEFIobjectid        = pSave->iDEFIobjectid;
    pData->iDEFIdonotshow       = pSave->iDEFIdonotshow;
    pData->iDEFIconcrete        = pSave->iDEFIconcrete;
    pData->bDEFIhasloca         = pSave->bDEFIhasloca;
    pData->iDEFIlocax           = pSave->iDEFIlocax;
    pData->iDEFIlocay           = pSave->iDEFIlocay;
    pData->bDEFIhasclip         = pSave->bDEFIhasclip;
    pData->iDEFIclipl           = pSave->iDEFIclipl;
    pData->iDEFIclipr           = pSave->iDEFIclipr;
    pData->iDEFIclipt           = pSave->iDEFIclipt;
    pData->iDEFIclipb           = pSave->iDEFIclipb;

    pData->iBACKred             = pSave->iBACKred;
    pData->iBACKgreen           = pSave->iBACKgreen;
    pData->iBACKblue            = pSave->iBACKblue;
    pData->iBACKmandatory       = pSave->iBACKmandatory;
    pData->iBACKimageid         = pSave->iBACKimageid;
    pData->iBACKtile            = pSave->iBACKtile;

    pData->iFRAMmode            = pSave->iFRAMmode;
    pData->iFRAMdelay           = pSave->iFRAMdelay;
    pData->iFRAMtimeout         = pSave->iFRAMtimeout;
    pData->bFRAMclipping        = pSave->bFRAMclipping;
    pData->iFRAMclipl           = pSave->iFRAMclipl;
    pData->iFRAMclipr           = pSave->iFRAMclipr;
    pData->iFRAMclipt           = pSave->iFRAMclipt;
    pData->iFRAMclipb           = pSave->iFRAMclipb;
                                       /* also the next subframe parms */
    pData->iFramemode           = pSave->iFRAMmode;
    pData->iFramedelay          = pSave->iFRAMdelay;
    pData->iFrametimeout        = pSave->iFRAMtimeout;
    pData->bFrameclipping       = pSave->bFRAMclipping;
    pData->iFrameclipl          = pSave->iFRAMclipl;
    pData->iFrameclipr          = pSave->iFRAMclipr;
    pData->iFrameclipt          = pSave->iFRAMclipt;
    pData->iFrameclipb          = pSave->iFRAMclipb;

    pData->iNextdelay           = pSave->iFRAMdelay;

    pData->iGlobalPLTEcount     = pSave->iGlobalPLTEcount;
    MNG_COPY (&pData->aGlobalPLTEentries, &pSave->aGlobalPLTEentries, sizeof (mng_rgbpaltab))

    pData->iGlobalTRNSrawlen    = pSave->iGlobalTRNSrawlen;
    MNG_COPY (&pData->aGlobalTRNSrawdata, &pSave->aGlobalTRNSrawdata, 256)

    pData->iGlobalGamma         = pSave->iGlobalGamma;

    pData->iGlobalWhitepointx   = pSave->iGlobalWhitepointx;
    pData->iGlobalWhitepointy   = pSave->iGlobalWhitepointy;
    pData->iGlobalPrimaryredx   = pSave->iGlobalPrimaryredx;
    pData->iGlobalPrimaryredy   = pSave->iGlobalPrimaryredy;
    pData->iGlobalPrimarygreenx = pSave->iGlobalPrimarygreenx;
    pData->iGlobalPrimarygreeny = pSave->iGlobalPrimarygreeny;
    pData->iGlobalPrimarybluex  = pSave->iGlobalPrimarybluex;
    pData->iGlobalPrimarybluey  = pSave->iGlobalPrimarybluey;

    pData->iGlobalRendintent    = pSave->iGlobalRendintent;

    pData->iGlobalProfilesize   = pSave->iGlobalProfilesize;

    if (pData->iGlobalProfilesize)     /* has a profile ? */
    {                                  /* then copy that ! */
      MNG_ALLOC (pData, pData->pGlobalProfile, pData->iGlobalProfilesize)
      MNG_COPY (pData->pGlobalProfile, pSave->pGlobalProfile, pData->iGlobalProfilesize)
    }

    pData->iGlobalBKGDred       = pSave->iGlobalBKGDred;
    pData->iGlobalBKGDgreen     = pSave->iGlobalBKGDgreen;
    pData->iGlobalBKGDblue      = pSave->iGlobalBKGDblue;
  }
  else                                 /* no saved-data; so reset the lot */
  {
#if defined(MNG_SUPPORT_READ) || defined(MNG_SUPPORT_WRITE)
    pData->bHasglobalPLTE       = MNG_FALSE;
    pData->bHasglobalTRNS       = MNG_FALSE;
    pData->bHasglobalGAMA       = MNG_FALSE;
    pData->bHasglobalCHRM       = MNG_FALSE;
    pData->bHasglobalSRGB       = MNG_FALSE;
    pData->bHasglobalICCP       = MNG_FALSE;
    pData->bHasglobalBKGD       = MNG_FALSE;
#endif /* MNG_SUPPORT_READ || MNG_SUPPORT_WRITE */

    pData->iDEFIobjectid        = 0;
    pData->iDEFIdonotshow       = 0;
    pData->iDEFIconcrete        = 0;
    pData->bDEFIhasloca         = MNG_FALSE;
    pData->iDEFIlocax           = 0;
    pData->iDEFIlocay           = 0;
    pData->bDEFIhasclip         = MNG_FALSE;
    pData->iDEFIclipl           = 0;
    pData->iDEFIclipr           = 0;
    pData->iDEFIclipt           = 0;
    pData->iDEFIclipb           = 0;

    if (!pData->bEMNGMAhack)             /* TODO: remove line in 1.0.0 !!! */
    {                                    /* TODO: remove line in 1.0.0 !!! */
    pData->iBACKred             = 0;
    pData->iBACKgreen           = 0;
    pData->iBACKblue            = 0;
    pData->iBACKmandatory       = 0;
    pData->iBACKimageid         = 0;
    pData->iBACKtile            = 0;
    }                                    /* TODO: remove line in 1.0.0 !!! */

    pData->iFRAMmode            = 1;
    pData->iFRAMdelay           = 1;
    pData->iFRAMtimeout         = 0x7fffffffl;
    pData->bFRAMclipping        = MNG_FALSE;
    pData->iFRAMclipl           = 0;
    pData->iFRAMclipr           = 0;
    pData->iFRAMclipt           = 0;
    pData->iFRAMclipb           = 0;
                                         /* also the next subframe parms */
    pData->iFramemode           = 1;
    pData->iFramedelay          = 1;
    pData->iFrametimeout        = 0x7fffffffl;
    pData->bFrameclipping       = MNG_FALSE;
    pData->iFrameclipl          = 0;
    pData->iFrameclipr          = 0;
    pData->iFrameclipt          = 0;
    pData->iFrameclipb          = 0;

    pData->iNextdelay           = 1;

    pData->iGlobalPLTEcount     = 0;

    pData->iGlobalTRNSrawlen    = 0;

    pData->iGlobalGamma         = 0;

    pData->iGlobalWhitepointx   = 0;
    pData->iGlobalWhitepointy   = 0;
    pData->iGlobalPrimaryredx   = 0;
    pData->iGlobalPrimaryredy   = 0;
    pData->iGlobalPrimarygreenx = 0;
    pData->iGlobalPrimarygreeny = 0;
    pData->iGlobalPrimarybluex  = 0;
    pData->iGlobalPrimarybluey  = 0;

    pData->iGlobalRendintent    = 0;

    if (pData->iGlobalProfilesize)       /* free a previous profile ? */
      MNG_FREE (pData, pData->pGlobalProfile, pData->iGlobalProfilesize)

    pData->iGlobalProfilesize   = 0;

    pData->iGlobalBKGDred       = 0;
    pData->iGlobalBKGDgreen     = 0;
    pData->iGlobalBKGDblue      = 0;
  }

  if (!pData->bEMNGMAhack)             /* TODO: remove line in 1.0.0 !!! */
  {                                    /* TODO: remove line in 1.0.0 !!! */
                                       /* drop un-frozen image objects */
  pImage = (mng_imagep)pData->pFirstimgobj;

  while (pImage)
  {                                    /* is it un-frozen ? */
    if (!pImage->bFrozen)
    {
      mng_imagep  pPrev = (mng_imagep)pImage->sHeader.pPrev;
      mng_imagep  pNext = (mng_imagep)pImage->sHeader.pNext;
      mng_retcode iRetcode;

      if (pPrev)                       /* unlink it */
        pPrev->sHeader.pNext = pNext;
      else
        pData->pFirstimgobj  = pNext;

      if (pNext)
        pNext->sHeader.pPrev = pPrev;
      else
        pData->pLastimgobj   = pPrev;

      if (pImage->pImgbuf->bFrozen)    /* buffer frozen ? */
      {
        if (pImage->pImgbuf->iRefcount <= 2)
          MNG_ERROR (pData, MNG_INTERNALERROR)
                                       /* decrease ref counter */
        pImage->pImgbuf->iRefcount--;
                                       /* just cleanup the object then */
        MNG_FREEX (pData, pImage, sizeof (mng_image))
      }
      else
      {                                /* free the image buffer */
        iRetcode = free_imagedataobject (pData, pImage->pImgbuf);
                                       /* and cleanup the object */
        MNG_FREEX (pData, pImage, sizeof (mng_image))

        if (iRetcode)                  /* on error bail out */
          return iRetcode;
      }

      pImage = pNext;                  /* this is the next */
    }
    else                               /* neeeext */
      pImage = (mng_imagep)pImage->sHeader.pNext;

  }
  }                                    /* TODO: remove line in 1.0.0 !!! */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_RESTORE_STATE, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */
/* *                                                                        * */
/* * Chunk display processing routines                                      * */
/* *                                                                        * */
/* ************************************************************************** */

mng_retcode process_display_ihdr (mng_datap pData)
{                                      /* address the current "object" if any */
  mng_imagep pImage = (mng_imagep)pData->pCurrentobj;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_DISPLAY_IHDR, MNG_LC_START)
#endif

  if (!pData->bHasDHDR)
  {
    pData->fInitrowproc = 0;           /* do nothing by default */
    pData->fDisplayrow  = 0;
    pData->fCorrectrow  = 0;
    pData->fStorerow    = 0;
    pData->fProcessrow  = 0;
    pData->pStoreobj    = 0;
  }

  if (!pData->iBreakpoint)             /* not previously broken ? */
  {
    mng_retcode iRetcode = MNG_NOERROR;

    if (pData->bHasDHDR)               /* is a delta-image ? */
    {
      if (pData->iDeltatype == MNG_DELTATYPE_REPLACE)
        iRetcode = reset_object_details (pData, (mng_imagep)pData->pDeltaImage,
                                         pData->iDatawidth, pData->iDataheight,
                                         pData->iBitdepth, pData->iColortype,
                                         pData->iCompression, pData->iFilter,
                                         pData->iInterlace, MNG_TRUE);
                                       /* process immediatly if bitdepth & colortype are equal */
      pData->bDeltaimmediate =
        (mng_bool)((pData->iBitdepth  == ((mng_imagep)pData->pDeltaImage)->pImgbuf->iBitdepth ) &&
                   (pData->iColortype == ((mng_imagep)pData->pDeltaImage)->pImgbuf->iColortype)    );
    }
    else
    {
      if (pImage)                      /* update object buffer ? */
        iRetcode = reset_object_details (pData, pImage,
                                         pData->iDatawidth, pData->iDataheight,
                                         pData->iBitdepth, pData->iColortype,
                                         pData->iCompression, pData->iFilter,
                                         pData->iInterlace, MNG_TRUE);
      else                             /* update object 0 ? */
      if (pData->eImagetype != mng_it_png)
        iRetcode = reset_object_details (pData, (mng_imagep)pData->pObjzero,
                                         pData->iDatawidth, pData->iDataheight,
                                         pData->iBitdepth, pData->iColortype,
                                         pData->iCompression, pData->iFilter,
                                         pData->iInterlace, MNG_TRUE);
    }

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
  }

  if (!pData->bHasDHDR)
  {                                    /* do we need to store it ? */
    if (pData->eImagetype != mng_it_png)
    {
      if (pImage)                      /* real object ? */
        pData->pStoreobj = pImage;     /* tell the row routines */
      else                             /* otherwise use object 0 */
        pData->pStoreobj = pData->pObjzero;
    }
                                       /* display "on-the-fly" ? */
    if ( (pData->eImagetype == mng_it_png         ) ||
         (((mng_imagep)pData->pStoreobj)->bVisible)    )
    {
      next_layer (pData);              /* that's a new layer then ! */

      if (pData->bTimerset)            /* timer break ? */
        pData->iBreakpoint = 2;
      else
      {
        pData->iBreakpoint = 0;
                                       /* anything to display ? */
        if ((pData->iDestr > pData->iDestl) && (pData->iDestb > pData->iDestt))
          set_display_routine (pData); /* then determine display routine */
      }
    }
  }

  if (!pData->bTimerset)               /* no timer break ? */
  {
    switch (pData->iColortype)         /* determine row initialization routine */
    {
      case 0 : {                       /* gray */
                 switch (pData->iBitdepth)
                 {
                   case  1 : {
                               if (!pData->iInterlace)
                                 pData->fInitrowproc = (mng_ptr)init_g1_ni;
                               else
                                 pData->fInitrowproc = (mng_ptr)init_g1_i;

                               break;
                             }
                   case  2 : {
                               if (!pData->iInterlace)
                                 pData->fInitrowproc = (mng_ptr)init_g2_ni;
                               else
                                 pData->fInitrowproc = (mng_ptr)init_g2_i;

                               break;
                             }
                   case  4 : {
                               if (!pData->iInterlace)
                                 pData->fInitrowproc = (mng_ptr)init_g4_ni;
                               else
                                 pData->fInitrowproc = (mng_ptr)init_g4_i;

                               break;
                             }
                   case  8 : {
                               if (!pData->iInterlace)
                                 pData->fInitrowproc = (mng_ptr)init_g8_ni;
                               else
                                 pData->fInitrowproc = (mng_ptr)init_g8_i;

                               break;
                             }
                   case 16 : {
                               if (!pData->iInterlace)
                                 pData->fInitrowproc = (mng_ptr)init_g16_ni;
                               else
                                 pData->fInitrowproc = (mng_ptr)init_g16_i;

                               break;
                             }
                 }

                 break;
               }
      case 2 : {                       /* rgb */
                 switch (pData->iBitdepth)
                 {
                   case  8 : {
                               if (!pData->iInterlace)
                                 pData->fInitrowproc = (mng_ptr)init_rgb8_ni;
                               else
                                 pData->fInitrowproc = (mng_ptr)init_rgb8_i;

                               break;
                             }
                   case 16 : {
                               if (!pData->iInterlace)
                                 pData->fInitrowproc = (mng_ptr)init_rgb16_ni;
                               else
                                 pData->fInitrowproc = (mng_ptr)init_rgb16_i;

                               break;
                             }
                 }

                 break;
               }
      case 3 : {                       /* indexed */
                 switch (pData->iBitdepth)
                 {
                   case  1 : {
                               if (!pData->iInterlace)
                                 pData->fInitrowproc = (mng_ptr)init_idx1_ni;
                               else
                                 pData->fInitrowproc = (mng_ptr)init_idx1_i;

                               break;
                             }
                   case  2 : {
                               if (!pData->iInterlace)
                                 pData->fInitrowproc = (mng_ptr)init_idx2_ni;
                               else
                                 pData->fInitrowproc = (mng_ptr)init_idx2_i;

                               break;
                             }
                   case  4 : {
                               if (!pData->iInterlace)
                                 pData->fInitrowproc = (mng_ptr)init_idx4_ni;
                               else
                                 pData->fInitrowproc = (mng_ptr)init_idx4_i;

                               break;
                             }
                   case  8 : {
                               if (!pData->iInterlace)
                                 pData->fInitrowproc = (mng_ptr)init_idx8_ni;
                               else
                                 pData->fInitrowproc = (mng_ptr)init_idx8_i;

                               break;
                             }
                 }

                 break;
               }
      case 4 : {                       /* gray+alpha */
                 switch (pData->iBitdepth)
                 {
                   case  8 : {
                               if (!pData->iInterlace)
                                 pData->fInitrowproc = (mng_ptr)init_ga8_ni;
                               else
                                 pData->fInitrowproc = (mng_ptr)init_ga8_i;

                               break;
                             }
                   case 16 : {
                               if (!pData->iInterlace)
                                 pData->fInitrowproc = (mng_ptr)init_ga16_ni;
                               else
                                 pData->fInitrowproc = (mng_ptr)init_ga16_i;

                               break;
                             }
                 }

                 break;
               }
      case 6 : {                       /* rgb+alpha */
                 switch (pData->iBitdepth)
                 {
                   case  8 : {
                               if (!pData->iInterlace)
                                 pData->fInitrowproc = (mng_ptr)init_rgba8_ni;
                               else
                                 pData->fInitrowproc = (mng_ptr)init_rgba8_i;

                               break;
                             }
                   case 16 : {
                               if (!pData->iInterlace)
                                 pData->fInitrowproc = (mng_ptr)init_rgba16_ni;
                               else
                                 pData->fInitrowproc = (mng_ptr)init_rgba16_i;

                               break;
                             }
                 }

                 break;
               }
    }
  }  

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_DISPLAY_IHDR, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode process_display_idat (mng_datap  pData,
                                  mng_uint32 iRawlen,
                                  mng_uint8p pRawdata)
{
  mng_retcode iRetcode = MNG_NOERROR;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_DISPLAY_IDAT, MNG_LC_START)
#endif

  if (!pData->bInflating)              /* if we're not inflating already */
  {                                    /* initialize row-processing */
    iRetcode = ((mng_initrowproc)pData->fInitrowproc) (pData);

    if (!iRetcode)                     /* initialize inflate */
      iRetcode = mngzlib_inflateinit (pData);
  }

  if (!iRetcode)                       /* all ok? then inflate, my man */
    iRetcode = mngzlib_inflaterows (pData, iRawlen, pRawdata);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_DISPLAY_IDAT, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */

mng_retcode process_display_iend (mng_datap pData)
{
  mng_retcode iRetcode, iRetcode2;
  mng_bool bDodisplay = MNG_FALSE;
  mng_bool bCleanup   = (mng_bool)(pData->iBreakpoint != 0);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_DISPLAY_IEND, MNG_LC_START)
#endif

#ifdef MNG_INCLUDE_JNG                 /* progressive+alpha JNG can be displayed now */
  if ((pData->bHasJHDR) && (pData->bJPEGprogressive) &&
      ((pData->eImagetype == mng_it_jng         ) ||
       (((mng_imagep)pData->pStoreobj)->bVisible)    ) &&
      ((pData->iJHDRcolortype == MNG_COLORTYPE_JPEGGRAYA ) ||
       (pData->iJHDRcolortype == MNG_COLORTYPE_JPEGCOLORA)    ))
    bDodisplay = MNG_TRUE;
#endif    

  if ((pData->bHasBASI) ||             /* was it a BASI stream */
      (bDodisplay)      ||             /* or should we display the JNG */
                                       /* or did we get broken here last time ? */
      ((pData->iBreakpoint) && (pData->iBreakpoint != 8)))
  {
    mng_imagep pImage = (mng_imagep)pData->pCurrentobj;

    if (!pImage)                       /* or was it an "on-the-fly" image ? */
      pImage = (mng_imagep)pData->pObjzero;
                                       /* display it now then ? */
    if ((pImage->bVisible) && (pImage->bViewable))
    {                                  /* ok, so do it */
      iRetcode = display_image (pData, pImage, bDodisplay);

      if (iRetcode)                    /* on error bail out */
        return iRetcode;

      if (pData->bTimerset)            /* timer break ? */
        pData->iBreakpoint = 6;
    }
  }

  if ((pData->bHasDHDR) ||             /* was it a DHDR stream */
      (pData->iBreakpoint == 8))       /* or did we get broken here last time ? */
  {
    mng_imagep pImage = (mng_imagep)pData->pDeltaImage;
                                       /* perform the delta operations needed */
    iRetcode = execute_delta_image (pData, pImage, (mng_imagep)pData->pObjzero);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
                                       /* display it now then ? */
    if ((pImage->bVisible) && (pImage->bViewable))
    {                                  /* ok, so do it */
      iRetcode = display_image (pData, pImage, MNG_FALSE);

      if (iRetcode)                    /* on error bail out */
        return iRetcode;

      if (pData->bTimerset)            /* timer break ? */
        pData->iBreakpoint = 8;
    }
  }

  if (!pData->bTimerset)               /* can we continue ? */
  {
    pData->iBreakpoint = 0;            /* clear this flag now ! */
                                       /* cleanup object 0 */
    reset_object_details (pData, (mng_imagep)pData->pObjzero,
                          0, 0, 0, 0, 0, 0, 0, MNG_TRUE);

    if (pData->bInflating)             /* if we've been inflating */
    {                                  /* cleanup row-processing, */
      iRetcode  = cleanup_rowproc (pData);
                                       /* also cleanup inflate! */
      iRetcode2 = mngzlib_inflatefree (pData);

      if (iRetcode)                    /* on error bail out */
        return iRetcode;
      if (iRetcode2)
        return iRetcode2;
    }

#ifdef MNG_INCLUDE_JNG
    if (pData->bJPEGdecompress)        /* if we've been decompressing */
    {                                  /* cleanup row-processing, */
      iRetcode  = cleanup_rowproc (pData);
                                       /* also cleanup decompress! */
      iRetcode2 = mngjpeg_decompressfree (pData);

      if (iRetcode)                    /* on error bail out */
        return iRetcode;
      if (iRetcode2)
        return iRetcode2;
    }
#endif
                                       /* if the image was displayed on the fly, */
                                       /* we'll have to make the app refresh */
    if ((pData->eImagetype != mng_it_mng) && (pData->fDisplayrow))
      pData->bNeedrefresh = MNG_TRUE;

    if (bCleanup)                      /* if we got broken last time we need to cleanup */
    {
      pData->bHasIHDR = MNG_FALSE;     /* IEND signals the end for most ... */
      pData->bHasBASI = MNG_FALSE;
      pData->bHasDHDR = MNG_FALSE;
#ifdef MNG_INCLUDE_JNG
      pData->bHasJHDR = MNG_FALSE;
      pData->bHasJSEP = MNG_FALSE;
      pData->bHasJDAT = MNG_FALSE;
#endif
      pData->bHasPLTE = MNG_FALSE;
      pData->bHasTRNS = MNG_FALSE;
      pData->bHasGAMA = MNG_FALSE;
      pData->bHasCHRM = MNG_FALSE;
      pData->bHasSRGB = MNG_FALSE;
      pData->bHasICCP = MNG_FALSE;
      pData->bHasBKGD = MNG_FALSE;
      pData->bHasIDAT = MNG_FALSE;
    }
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_DISPLAY_IEND, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode process_display_mend (mng_datap pData)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_DISPLAY_MEND, MNG_LC_START)
#endif

  if (pData->bHasTERM)                 /* TERM processed ? */
  {
    mng_retcode   iRetcode;
    mng_ani_termp pTERM;
                                       /* get the right animation object ! */
    pTERM = (mng_ani_termp)pData->pTermaniobj;

    switch (pTERM->iTermaction)        /* determine what to do! */
    {
      case 0 : {                       /* show last frame indefinitly */
                 break;                /* piece of cake, that is... */
               }

      case 1 : {                       /* cease displaying anything */
                 pData->bFrameclipping = MNG_FALSE;
                 load_bkgdlayer (pData);
                 break;
               }

      case 2 : {                       /* show first image after TERM */

                 /* TODO: something */

                 break;
               }

      case 3 : {                       /* repeat */
                 if ((pTERM->iItermax) && (pTERM->iItermax < 0x7FFFFFFF))
                   pTERM->iItermax--;

                 if (pTERM->iItermax)  /* go back to TERM ? */
                 {                     /* restore to initial or SAVE state */
                   iRetcode = restore_state (pData);

                   if (iRetcode)       /* on error bail out */
                     return iRetcode;

                   pData->pCurraniobj = pTERM;
                 }
                 else
                 {
                   switch (pTERM->iIteraction)
                   {
                     case 0 : {        /* show last frame indefinitly */
                                break; /* piece of cake, that is... */
                              }

                     case 1 : {        /* cease displaying anything */
                                pData->bFrameclipping = MNG_FALSE;
                                load_bkgdlayer (pData);
                                break;
                              }

                     case 2 : {        /* show first image after TERM */

                                /* TODO: something */

                                break;
                              }

                   }
                 }

                 break;
               }

    }
  }

  if (!pData->pCurraniobj)             /* always let the app refresh at the end ! */
    pData->bNeedrefresh = MNG_TRUE;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_DISPLAY_MEND, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode process_display_defi (mng_datap pData)
{
  mng_imagep pImage;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_DISPLAY_DEFI, MNG_LC_START)
#endif

  if (!pData->iDEFIobjectid)           /* object id=0 ? */
  {
    pImage             = (mng_imagep)pData->pObjzero;
    pImage->bVisible   = (mng_bool)(pData->iDEFIdonotshow == 0);
    pImage->iPosx      = pData->iDEFIlocax;
    pImage->iPosy      = pData->iDEFIlocay;

    if (pData->bDEFIhasclip)
    {
      pImage->bClipped = pData->bDEFIhasclip;
      pImage->iClipl   = pData->iDEFIclipl;
      pImage->iClipr   = pData->iDEFIclipr;
      pImage->iClipt   = pData->iDEFIclipt;
      pImage->iClipb   = pData->iDEFIclipb;
    }

    pData->pCurrentobj = 0;            /* not a real object ! */
  }
  else
  {                                    /* already exists ? */
    pImage = (mng_imagep)find_imageobject (pData, pData->iDEFIobjectid);

    if (!pImage)                       /* if not; create new */
    {
      mng_retcode iRetcode = create_imageobject (pData, pData->iDEFIobjectid,
                                                 (mng_bool)(pData->iDEFIconcrete == 1),
                                                 (mng_bool)(pData->iDEFIdonotshow == 0),
                                                 MNG_FALSE, 0, 0, 0, 0, 0, 0, 0,
                                                 pData->iDEFIlocax, pData->iDEFIlocay,
                                                 pData->bDEFIhasclip,
                                                 pData->iDEFIclipl, pData->iDEFIclipr,
                                                 pData->iDEFIclipt, pData->iDEFIclipb,
                                                 &pImage);

      if (iRetcode)                    /* on error bail out */
        return iRetcode;
    }
    else
    {                                  /* exists; then set new info */
      pImage->bVisible  = (mng_bool)(pData->iDEFIdonotshow == 0);
      pImage->bViewable = MNG_FALSE;
      pImage->iPosx     = pData->iDEFIlocax;
      pImage->iPosy     = pData->iDEFIlocay;
      pImage->bClipped  = pData->bDEFIhasclip;
      pImage->iClipl    = pData->iDEFIclipl;
      pImage->iClipr    = pData->iDEFIclipr;
      pImage->iClipt    = pData->iDEFIclipt;
      pImage->iClipb    = pData->iDEFIclipb;

      pImage->pImgbuf->bConcrete = (mng_bool)(pData->iDEFIconcrete == 1);
    }

    pData->pCurrentobj = pImage;       /* others may want to know this */
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_DISPLAY_DEFI, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode process_display_basi (mng_datap  pData,
                                  mng_uint16 iRed,
                                  mng_uint16 iGreen,
                                  mng_uint16 iBlue,
                                  mng_bool   bHasalpha,
                                  mng_uint16 iAlpha,
                                  mng_uint8  iViewable)
{                                      /* address the current "object" if any */
  mng_imagep     pImage = (mng_imagep)pData->pCurrentobj;
  mng_uint8p     pWork;
  mng_uint32     iX;
  mng_imagedatap pBuf;
  mng_retcode    iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_DISPLAY_BASI, MNG_LC_START)
#endif

  if (!pImage)                         /* or is it an "on-the-fly" image ? */
    pImage = (mng_imagep)pData->pObjzero;
                                       /* address the object-buffer */
  pBuf               = pImage->pImgbuf;

  pData->fDisplayrow = 0;              /* do nothing by default */
  pData->fCorrectrow = 0;
  pData->fStorerow   = 0;
  pData->fProcessrow = 0;
                                       /* set parms now that they're known */
  iRetcode = reset_object_details (pData, pImage, pData->iDatawidth,
                                   pData->iDataheight, pData->iBitdepth,
                                   pData->iColortype, pData->iCompression,
                                   pData->iFilter, pData->iInterlace, MNG_FALSE);
  if (iRetcode)                        /* on error bail out */
    return iRetcode;
                                       /* save the viewable flag */
  pImage->bViewable = (mng_bool)(iViewable == 1);
  pBuf->bViewable   = pImage->bViewable;
  pData->pStoreobj  = pImage;          /* let row-routines know which object */

  pWork = pBuf->pImgdata;              /* fill the object-buffer with the specified
                                          "color" sample */
  switch (pData->iColortype)           /* depending on color_type & bit_depth */
  {
    case 0 : {                         /* gray */
               if (pData->iBitdepth == 16)
               {
                 for (iX = 0; iX < pData->iDatawidth * pData->iDataheight; iX++)
                 {
                   mng_put_uint16 (pWork, iRed);
                   pWork += 2;
                 }
               }
               else
               {
                 for (iX = 0; iX < pData->iDatawidth * pData->iDataheight; iX++)
                 {
                   *pWork = (mng_uint8)iRed;
                   pWork++;
                 }
               }
                                       /* force tRNS ? */
               if ((bHasalpha) && (!iAlpha))
               {
                 pBuf->bHasTRNS  = MNG_TRUE;
                 pBuf->iTRNSgray = iRed;
               }

               break;
             }

    case 2 : {                         /* rgb */
               if (pData->iBitdepth == 16)
               {
                 for (iX = 0; iX < pData->iDatawidth * pData->iDataheight; iX++)
                 {
                   mng_put_uint16 (pWork,   iRed  );
                   mng_put_uint16 (pWork+2, iGreen);
                   mng_put_uint16 (pWork+4, iBlue );
                   pWork += 6;
                 }
               }
               else
               {
                 for (iX = 0; iX < pData->iDatawidth * pData->iDataheight; iX++)
                 {
                   *pWork     = (mng_uint8)iRed;
                   *(pWork+1) = (mng_uint8)iGreen;
                   *(pWork+2) = (mng_uint8)iBlue;
                   pWork += 3;
                 }
               }
                                       /* force tRNS ? */
               if ((bHasalpha) && (!iAlpha))
               {
                 pBuf->bHasTRNS   = MNG_TRUE;
                 pBuf->iTRNSred   = iRed;
                 pBuf->iTRNSgreen = iGreen;
                 pBuf->iTRNSblue  = iBlue;
               }

               break;
             }

    case 3 : {                         /* indexed */
               pBuf->bHasPLTE = MNG_TRUE;

               switch (pData->iBitdepth)
               {
                 case 1  : { pBuf->iPLTEcount =   2; break; }
                 case 2  : { pBuf->iPLTEcount =   4; break; }
                 case 4  : { pBuf->iPLTEcount =  16; break; }
                 case 8  : { pBuf->iPLTEcount = 256; break; }
                 default : { pBuf->iPLTEcount =   1; break; }
               }

               pBuf->aPLTEentries [0].iRed   = (mng_uint8)iRed;
               pBuf->aPLTEentries [0].iGreen = (mng_uint8)iGreen;
               pBuf->aPLTEentries [0].iBlue  = (mng_uint8)iBlue;

               for (iX = 1; iX < pBuf->iPLTEcount; iX++)
               {
                 pBuf->aPLTEentries [iX].iRed   = 0;
                 pBuf->aPLTEentries [iX].iGreen = 0;
                 pBuf->aPLTEentries [iX].iBlue  = 0;
               }
                                       /* force tRNS ? */
               if ((bHasalpha) && (iAlpha < 255))
               {
                 pBuf->bHasTRNS         = MNG_TRUE;
                 pBuf->iTRNScount       = 1;
                 pBuf->aTRNSentries [0] = (mng_uint8)iAlpha;
               }

               break;
             }

    case 4 : {                         /* gray+alpha */
               if (pData->iBitdepth == 16)
               {
                 for (iX = 0; iX < pData->iDatawidth * pData->iDataheight; iX++)
                 {
                   mng_put_uint16 (pWork,   iRed);
                   mng_put_uint16 (pWork+2, iAlpha);
                   pWork += 4;
                 }
               }
               else
               {
                 for (iX = 0; iX < pData->iDatawidth * pData->iDataheight; iX++)
                 {
                   *pWork     = (mng_uint8)iRed;
                   *(pWork+1) = (mng_uint8)iAlpha;
                   pWork += 2;
                 }
               }

               break;
             }

    case 6 : {                         /* rgb+alpha */
               if (pData->iBitdepth == 16)
               {
                 for (iX = 0; iX < pData->iDatawidth * pData->iDataheight; iX++)
                 {
                   mng_put_uint16 (pWork,   iRed);
                   mng_put_uint16 (pWork+2, iGreen);
                   mng_put_uint16 (pWork+4, iBlue);
                   mng_put_uint16 (pWork+6, iAlpha);
                   pWork += 8;
                 }
               }
               else
               {
                 for (iX = 0; iX < pData->iDatawidth * pData->iDataheight; iX++)
                 {
                   *pWork     = (mng_uint8)iRed;
                   *(pWork+1) = (mng_uint8)iGreen;
                   *(pWork+2) = (mng_uint8)iBlue;
                   *(pWork+3) = (mng_uint8)iAlpha;
                   pWork += 4;
                 }
               }

               break;
             }

  }

  switch (pData->iColortype)           /* determine row initialization routine */
  {                                    /* just to accomodate IDAT if it arrives */
    case 0 : {                         /* gray */
               switch (pData->iBitdepth)
               {
                 case  1 : {
                             if (!pData->iInterlace)
                               pData->fInitrowproc = (mng_ptr)init_g1_ni;
                             else
                               pData->fInitrowproc = (mng_ptr)init_g1_i;

                             break;
                           }
                 case  2 : {
                             if (!pData->iInterlace)
                               pData->fInitrowproc = (mng_ptr)init_g2_ni;
                             else
                               pData->fInitrowproc = (mng_ptr)init_g2_i;

                             break;
                           }
                 case  4 : {
                             if (!pData->iInterlace)
                               pData->fInitrowproc = (mng_ptr)init_g4_ni;
                             else
                               pData->fInitrowproc = (mng_ptr)init_g4_i;

                             break;
                           }
                 case  8 : {
                             if (!pData->iInterlace)
                               pData->fInitrowproc = (mng_ptr)init_g8_ni;
                             else
                               pData->fInitrowproc = (mng_ptr)init_g8_i;

                             break;
                           }
                 case 16 : {
                             if (!pData->iInterlace)
                               pData->fInitrowproc = (mng_ptr)init_g16_ni;
                             else
                               pData->fInitrowproc = (mng_ptr)init_g16_i;

                             break;
                           }
               }

               break;
             }
    case 2 : {                         /* rgb */
               switch (pData->iBitdepth)
               {
                 case  8 : {
                             if (!pData->iInterlace)
                               pData->fInitrowproc = (mng_ptr)init_rgb8_ni;
                             else
                               pData->fInitrowproc = (mng_ptr)init_rgb8_i;

                             break;
                           }
                 case 16 : {
                             if (!pData->iInterlace)
                               pData->fInitrowproc = (mng_ptr)init_rgb16_ni;
                             else
                               pData->fInitrowproc = (mng_ptr)init_rgb16_i;

                             break;
                           }
               }

               break;
             }
    case 3 : {                         /* indexed */
               switch (pData->iBitdepth)
               {
                 case  1 : {
                             if (!pData->iInterlace)
                               pData->fInitrowproc = (mng_ptr)init_idx1_ni;
                             else
                               pData->fInitrowproc = (mng_ptr)init_idx1_i;

                             break;
                           }
                 case  2 : {
                             if (!pData->iInterlace)
                               pData->fInitrowproc = (mng_ptr)init_idx2_ni;
                             else
                               pData->fInitrowproc = (mng_ptr)init_idx2_i;

                             break;
                           }
                 case  4 : {
                             if (!pData->iInterlace)
                               pData->fInitrowproc = (mng_ptr)init_idx4_ni;
                             else
                               pData->fInitrowproc = (mng_ptr)init_idx4_i;

                             break;
                           }
                 case  8 : {
                             if (!pData->iInterlace)
                               pData->fInitrowproc = (mng_ptr)init_idx8_ni;
                             else
                               pData->fInitrowproc = (mng_ptr)init_idx8_i;

                             break;
                           }
               }

               break;
             }
    case 4 : {                         /* gray+alpha */
               switch (pData->iBitdepth)
               {
                 case  8 : {
                             if (!pData->iInterlace)
                               pData->fInitrowproc = (mng_ptr)init_ga8_ni;
                             else
                               pData->fInitrowproc = (mng_ptr)init_ga8_i;

                             break;
                           }
                 case 16 : {
                             if (!pData->iInterlace)
                               pData->fInitrowproc = (mng_ptr)init_ga16_ni;
                             else
                               pData->fInitrowproc = (mng_ptr)init_ga16_i;

                             break;
                           }
               }

               break;
             }
    case 6 : {                         /* rgb+alpha */
               switch (pData->iBitdepth)
               {
                 case  8 : {
                             if (!pData->iInterlace)
                               pData->fInitrowproc = (mng_ptr)init_rgba8_ni;
                             else
                               pData->fInitrowproc = (mng_ptr)init_rgba8_i;

                             break;
                           }
                 case 16 : {
                             if (!pData->iInterlace)
                               pData->fInitrowproc = (mng_ptr)init_rgba16_ni;
                             else
                               pData->fInitrowproc = (mng_ptr)init_rgba16_i;

                             break;
                           }
               }

               break;
             }
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_DISPLAY_BASI, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode process_display_clon (mng_datap  pData,
                                  mng_uint16 iSourceid,
                                  mng_uint16 iCloneid,
                                  mng_uint8  iClonetype,
                                  mng_bool   bHasdonotshow,
                                  mng_uint8  iDonotshow,
                                  mng_uint8  iConcrete,
                                  mng_bool   bHasloca,
                                  mng_uint8  iLocationtype,
                                  mng_int32  iLocationx,
                                  mng_int32  iLocationy)
{
  mng_imagep  pSource, pClone;
  mng_bool    bVisible, bAbstract;
  mng_retcode iRetcode = MNG_NOERROR;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_DISPLAY_CLON, MNG_LC_START)
#endif
                                       /* locate the source object first */
  pSource = find_imageobject (pData, iSourceid);
                                       /* check if the clone exists */
  pClone  = find_imageobject (pData, iCloneid);

  if (!pSource)                        /* source must exist ! */
    MNG_ERROR (pData, MNG_OBJECTUNKNOWN);

  if (pClone)                          /* clone must not exist ! */
    MNG_ERROR (pData, MNG_OBJECTEXISTS);

  if (bHasdonotshow)                   /* DoNotShow flag filled ? */
    bVisible = (mng_bool)(iDonotshow == 0);
  else
    bVisible = pSource->bVisible;

  bAbstract  = (mng_bool)(iConcrete == 1);

  switch (iClonetype)                  /* determine action to take */
  {
    case 0 : {                         /* full clone */
               iRetcode = clone_imageobject (pData, iCloneid, MNG_FALSE,
                                             bVisible, bAbstract, bHasloca,
                                             iLocationtype, iLocationx, iLocationy,
                                             pSource, &pClone);
               break;
             }

    case 1 : {                         /* partial clone */
               iRetcode = clone_imageobject (pData, iCloneid, MNG_TRUE,
                                             bVisible, bAbstract, bHasloca,
                                             iLocationtype, iLocationx, iLocationy,
                                             pSource, &pClone);
               break;
             }

    case 2 : {                         /* renumber object */
               iRetcode = renum_imageobject (pData, pSource, iCloneid,
                                             bVisible, bAbstract, bHasloca,
                                             iLocationtype, iLocationx, iLocationy);
               pClone   = pSource;
               break;
             }

  }

  if (iRetcode)                        /* on error bail out */
    return iRetcode;

                                       /* display on the fly ? */
  if ((pClone->bViewable) && (pClone->bVisible))
  {
    pData->pLastclone = pClone;        /* remember in case of timer break ! */
                                       /* display it */
    display_image (pData, pClone, MNG_FALSE);

    if (pData->bTimerset)              /* timer break ? */
      pData->iBreakpoint = 5;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_DISPLAY_CLON, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode process_display_clon2 (mng_datap pData)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_DISPLAY_CLON, MNG_LC_START)
#endif
                                       /* only called after timer break ! */
  display_image (pData, (mng_imagep)pData->pLastclone, MNG_FALSE);
  pData->iBreakpoint = 0;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_DISPLAY_CLON, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode process_display_disc (mng_datap   pData,
                                  mng_uint32  iCount,
                                  mng_uint16p pIds)
{
  mng_uint32 iX;
  mng_imagep pImage;
  mng_uint32 iRetcode;
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_DISPLAY_DISC, MNG_LC_START)
#endif

  if (iCount)                          /* specific list ? */
  {
    mng_uint16p pWork = pIds;

    for (iX = 0; iX < iCount; iX++)    /* iterate the list */
    {
      pImage = find_imageobject (pData, *pWork++);

      if (pImage)                      /* found the object ? */
      {                                /* then drop it */
        iRetcode = free_imageobject (pData, pImage);

        if (iRetcode)                  /* on error bail out */
          return iRetcode;
      }
    }
  }
  else                                 /* empty: drop all un-frozen objects */
  {
    mng_imagep pNext = (mng_imagep)pData->pFirstimgobj;

    while (pNext)                      /* any left ? */
    {
      pImage = pNext;
      pNext  = pImage->sHeader.pNext;

      if (!pImage->bFrozen)            /* not frozen ? */
      {                                /* then drop it */
        iRetcode = free_imageobject (pData, pImage);

        if (iRetcode)                  /* on error bail out */
          return iRetcode;
      }
    }
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_DISPLAY_DISC, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode process_display_fram (mng_datap  pData,
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
  mng_uint32 iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_DISPLAY_FRAM, MNG_LC_START)
#endif
                                       /* advance a frame then */
  iRetcode = next_frame (pData, iFramemode, iChangedelay, iDelay,
                         iChangetimeout, iTimeout, iChangeclipping,
                         iCliptype, iClipl, iClipr, iClipt, iClipb);

  if (pData->bTimerset)                /* timer break ? */
    pData->iBreakpoint = 1;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_DISPLAY_FRAM, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */

mng_retcode process_display_fram2 (mng_datap pData)
{
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_DISPLAY_FRAM, MNG_LC_START)
#endif
                                       /* again; after the break */
  iRetcode = next_frame (pData, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  pData->iBreakpoint = 0;              /* not again! */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_DISPLAY_FRAM, MNG_LC_END)
#endif

  return iRetcode;
}

/* ************************************************************************** */

mng_retcode process_display_move (mng_datap  pData,
                                  mng_uint16 iFromid,
                                  mng_uint16 iToid,
                                  mng_uint8  iMovetype,
                                  mng_int32  iMovex,
                                  mng_int32  iMovey)
{
  mng_uint16 iX;
  mng_imagep pImage;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_DISPLAY_MOVE, MNG_LC_START)
#endif
                                       /* iterate the list */
  for (iX = iFromid; iX <= iToid; iX++)
  {
    if (!iX)                           /* object id=0 ? */
      pImage = (mng_imagep)pData->pObjzero;
    else
      pImage = find_imageobject (pData, iX);

    if (pImage)                        /* object exists ? */
    {
      switch (iMovetype)
      {
        case 0 : {                     /* absolute */
                   pImage->iPosx = iMovex;
                   pImage->iPosy = iMovey;
                   break;
                 }
        case 1 : {                     /* relative */
                   pImage->iPosx = pImage->iPosx + iMovex;
                   pImage->iPosy = pImage->iPosy + iMovey;
                   break;
                 }
      }
    }
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_DISPLAY_MOVE, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode process_display_clip (mng_datap  pData,
                                  mng_uint16 iFromid,
                                  mng_uint16 iToid,
                                  mng_uint8  iCliptype,
                                  mng_int32  iClipl,
                                  mng_int32  iClipr,
                                  mng_int32  iClipt,
                                  mng_int32  iClipb)
{
  mng_uint16 iX;
  mng_imagep pImage;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_DISPLAY_CLIP, MNG_LC_START)
#endif
                                       /* iterate the list */
  for (iX = iFromid; iX <= iToid; iX++)
  {
    if (!iX)                           /* object id=0 ? */
      pImage = (mng_imagep)pData->pObjzero;
    else
      pImage = find_imageobject (pData, iX);

    if (pImage)                        /* object exists ? */
    {
      switch (iCliptype)
      {
        case 0 : {                     /* absolute */
                   pImage->bClipped = MNG_TRUE;
                   pImage->iClipl   = iClipl;
                   pImage->iClipr   = iClipr;
                   pImage->iClipt   = iClipt;
                   pImage->iClipb   = iClipb;
                   break;
                 }
        case 1 : {                    /* relative */
                   pImage->bClipped = MNG_TRUE;
                   pImage->iClipl   = pImage->iClipl + iClipl;
                   pImage->iClipr   = pImage->iClipr + iClipr;
                   pImage->iClipt   = pImage->iClipt + iClipt;
                   pImage->iClipb   = pImage->iClipb + iClipb;
                   break;
                 }
      }
    }
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_DISPLAY_CLIP, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode process_display_show (mng_datap pData)
{
  mng_int16  iX, iS, iFrom, iTo;
  mng_imagep pImage;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_DISPLAY_SHOW, MNG_LC_START)
#endif

  /* TODO: optimization for the cases where "abs (iTo - iFrom)" is rather high;
     especially where ((iFrom==1) && (iTo==65535)); eg. an empty SHOW !!! */

  if (pData->iBreakpoint == 3)         /* previously broken during cycle-mode ? */
  {
    pImage = find_imageobject (pData, pData->iSHOWnextid);

    if (pImage)                        /* still there ? */
      display_image (pData, pImage, MNG_FALSE);

    pData->iBreakpoint = 0;            /* let's not go through this again! */
  }
  else
  {
    if (pData->iBreakpoint)            /* previously broken at other point ? */
    {                                  /* restore last parms */
      iFrom = (mng_int16)pData->iSHOWfromid;
      iTo   = (mng_int16)pData->iSHOWtoid;
      iX    = (mng_int16)pData->iSHOWnextid;
      iS    = (mng_int16)pData->iSHOWskip;
    }
    else
    {                                  /* regular sequence ? */
      if (pData->iSHOWtoid >= pData->iSHOWfromid)
        iS  = 1;
      else                             /* reverse sequence ! */
        iS  = -1;

      iFrom = (mng_int16)pData->iSHOWfromid;
      iTo   = (mng_int16)pData->iSHOWtoid;
      iX    = iFrom;
      
      pData->iSHOWfromid = (mng_uint16)iFrom;
      pData->iSHOWtoid   = (mng_uint16)iTo;
      pData->iSHOWskip   = iS;
    }
                                       /* cycle mode ? */
    if ((pData->iSHOWmode == 6) || (pData->iSHOWmode == 7))
    {
      mng_uint16 iTrigger = 0;
      mng_uint16 iFound   = 0;
      mng_uint16 iPass    = 0;
      mng_imagep pFound   = 0;

      do
      {
        iPass++;                       /* lets prevent endless loops when there
                                          are no potential candidates in the list! */

        if (iS > 0)                    /* forward ? */
        {
          for (iX = iFrom; iX <= iTo; iX += iS)
          {
            pImage = find_imageobject (pData, (mng_uint16)iX);

            if (pImage)                  /* object exists ? */
            {
              if (iFound)                /* already found a candidate ? */
                pImage->bVisible = MNG_FALSE;
              else
              if (iTrigger)              /* found the trigger ? */
              {
                pImage->bVisible = MNG_TRUE;
                iFound           = iX;
                pFound           = pImage;
              }
              else
              if (pImage->bVisible)      /* ok, this is the trigger */
              {
                pImage->bVisible = MNG_FALSE;
                iTrigger         = iX;
              }
            }
          }
        }
        else
        {
          for (iX = iFrom; iX >= iTo; iX += iS)
          {
            pImage = find_imageobject (pData, (mng_uint16)iX);

            if (pImage)                  /* object exists ? */
            {
              if (iFound)                /* already found a candidate ? */
                pImage->bVisible = MNG_FALSE;
              else
              if (iTrigger)              /* found the trigger ? */
              {
                pImage->bVisible = MNG_TRUE;
                iFound           = iX;
                pFound           = pImage;
              }
              else
              if (pImage->bVisible)      /* ok, this is the trigger */
              {
                pImage->bVisible = MNG_FALSE;
                iTrigger         = iX;
              }
            }
          }
        }

        if (!iTrigger)                 /* did not find a trigger ? */
          iTrigger = 1;                /* then fake it so the first image
                                          gets nominated */
      }                                /* cycle back to beginning ? */
      while ((iPass < 2) && (iTrigger) && (!iFound));

      pData->iBreakpoint = 0;          /* just a sanity precaution */
                                       /* display it ? */
      if ((pData->iSHOWmode == 6) && (pFound))
      {
        display_image (pData, pFound, MNG_FALSE);

        if (pData->bTimerset)          /* timer set ? */
        {
          pData->iBreakpoint = 3;
          pData->iSHOWnextid = iFound; /* save it for after the break */
        }
      }
    }
    else
    {
      do
      {
        pImage = find_imageobject (pData, iX);

        if (pImage)                    /* object exists ? */
        {
          if (pData->iBreakpoint)      /* did we get broken last time ? */
          {                            /* could only happen in the display routine */
            display_image (pData, pImage, MNG_FALSE);
            pData->iBreakpoint = 0;    /* only once inside this loop please ! */
          }
          else
          {
            switch (pData->iSHOWmode)  /* do what ? */
            {
              case 0 : {
                         pImage->bVisible = MNG_TRUE;
                         display_image (pData, pImage, MNG_FALSE);
                         break;
                       }
              case 1 : {
                         pImage->bVisible = MNG_FALSE;
                         break;
                       }
              case 2 : {
                         if (pImage->bVisible)
                           display_image (pData, pImage, MNG_FALSE);
                         break;
                       }
              case 3 : {
                         pImage->bVisible = MNG_TRUE;
                         break;
                       }
              case 4 : {
                         pImage->bVisible = (mng_bool)(!pImage->bVisible);
                         if (pImage->bVisible)
                           display_image (pData, pImage, MNG_FALSE);
                         break;
                       }
              case 5 : {
                         pImage->bVisible = (mng_bool)(!pImage->bVisible);
                       }
            }
          }
        }

        if (!pData->bTimerset)         /* next ? */
          iX += iS;

      }                                /* continue ? */
      while ((!pData->bTimerset) && (((iS > 0) && (iX <= iTo)) ||
                                     ((iS < 0) && (iX >= iTo))    ));

      if (pData->bTimerset)            /* timer set ? */
      {
        pData->iBreakpoint = 4;
        pData->iSHOWnextid = iX;       /* save for next time */
      }
      else
        pData->iBreakpoint = 0;
        
    }
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_DISPLAY_SHOW, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode process_display_save (mng_datap pData)
{
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_DISPLAY_SAVE, MNG_LC_START)
#endif

  iRetcode = save_state (pData);       /* save the current state */

  if (iRetcode)                        /* on error bail out */
    return iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_DISPLAY_SAVE, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode process_display_seek (mng_datap pData)
{
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_DISPLAY_SEEK, MNG_LC_START)
#endif

  iRetcode = restore_state (pData);    /* restore the initial or SAVE state */

  if (iRetcode)                        /* on error bail out */
    return iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_DISPLAY_SEEK, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

#ifdef MNG_INCLUDE_JNG
mng_retcode process_display_jhdr (mng_datap pData)
{                                      /* address the current "object" if any */
  mng_imagep  pImage   = (mng_imagep)pData->pCurrentobj;
  mng_retcode iRetcode = MNG_NOERROR;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_DISPLAY_JHDR, MNG_LC_START)
#endif

  if (!pData->bHasDHDR)
  {
    pData->fInitrowproc  = 0;          /* do nothing by default */
    pData->fDisplayrow   = 0;
    pData->fCorrectrow   = 0;
    pData->fStorerow     = 0;
    pData->fProcessrow   = 0;
    pData->fStorerow2    = 0;

    pData->pStoreobj     = MNG_NULL;   /* initialize important work-parms */

    pData->iJPEGrow      = 0;
    pData->iJPEGalpharow = 0;
    pData->iJPEGrgbrow   = 0;
    pData->iRowmax       = 0;          /* so init_rowproc does the right thing ! */
  }

  if (!pData->iBreakpoint)             /* not previously broken ? */
  {
    if (pData->bHasDHDR)               /* delta-image ? */
    {
      if (pData->iDeltatype == MNG_DELTATYPE_REPLACE)
      {
        iRetcode = reset_object_details (pData, (mng_imagep)pData->pDeltaImage,
                                         pData->iDatawidth, pData->iDataheight,
                                         pData->iJHDRimgbitdepth, pData->iJHDRcolortype,
                                         pData->iJHDRalphacompression, pData->iJHDRalphafilter,
                                         pData->iJHDRalphainterlace, MNG_TRUE);

        ((mng_imagep)pData->pDeltaImage)->pImgbuf->iAlphabitdepth = pData->iJHDRalphabitdepth;
      }
    }
    else
    {
      if (pImage)                      /* update object buffer ? */
      {
        iRetcode = reset_object_details (pData, pImage,
                                         pData->iDatawidth, pData->iDataheight,
                                         pData->iJHDRimgbitdepth, pData->iJHDRcolortype,
                                         pData->iJHDRalphacompression, pData->iJHDRalphafilter,
                                         pData->iJHDRalphainterlace, MNG_TRUE);

        pImage->pImgbuf->iAlphabitdepth = pData->iJHDRalphabitdepth;
      }
      else                             /* update object 0 */
      {
        iRetcode = reset_object_details (pData, (mng_imagep)pData->pObjzero,
                                         pData->iDatawidth, pData->iDataheight,
                                         pData->iJHDRimgbitdepth, pData->iJHDRcolortype,
                                         pData->iJHDRalphacompression, pData->iJHDRalphafilter,
                                         pData->iJHDRalphainterlace, MNG_TRUE);

        ((mng_imagep)pData->pObjzero)->pImgbuf->iAlphabitdepth = pData->iJHDRalphabitdepth;
      }
    }

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
  }

  if (!pData->bHasDHDR)
  {                                    /* we're always storing a JPEG */
    if (pImage)                        /* real object ? */
      pData->pStoreobj = pImage;       /* tell the row routines */
    else                               /* otherwise use object 0 */
      pData->pStoreobj = pData->pObjzero;
                                       /* display "on-the-fly" ? */
    if ( (pData->eImagetype == mng_it_jng         ) ||
         (((mng_imagep)pData->pStoreobj)->bVisible)    )
    {
      next_layer (pData);              /* that's a new layer then ! */

      if (pData->bTimerset)            /* timer break ? */
        pData->iBreakpoint = 7;
      else
      {
        pData->iBreakpoint = 0;
                                       /* anything to display ? */
        if ((pData->iDestr > pData->iDestl) && (pData->iDestb > pData->iDestt))
        {
          set_display_routine (pData); /* then determine display routine */
                                       /* display from the object we store in */
          pData->pRetrieveobj = pData->pStoreobj;
        }
      }
    }
  }

  if (!pData->bTimerset)               /* no timer break ? */
  {
    if ((!pData->bHasDHDR) || (pData->iDeltatype == MNG_DELTATYPE_REPLACE))
    {                                  /* 8-bit JPEG ? */
      if (pData->iJHDRimgbitdepth == 8)
      {                                /* intermediate row is 8-bit deep */
        pData->bIsRGBA16   = MNG_FALSE;
        pData->iRowsamples = pData->iDatawidth;

        switch (pData->iJHDRcolortype) /* determine pixel processing routines */
        {
          case MNG_COLORTYPE_JPEGGRAY :
               {
                 pData->fStorerow2   = (mng_ptr)store_jpeg_g8;
                 pData->fRetrieverow = (mng_ptr)retrieve_g8;
                 pData->bIsOpaque    = MNG_TRUE;
                 break;
               }
          case MNG_COLORTYPE_JPEGCOLOR :
               {
                 pData->fStorerow2   = (mng_ptr)store_jpeg_rgb8;
                 pData->fRetrieverow = (mng_ptr)retrieve_rgb8;
                 pData->bIsOpaque    = MNG_TRUE;
                 break;
               }
          case MNG_COLORTYPE_JPEGGRAYA :
               {
                 pData->fStorerow2   = (mng_ptr)store_jpeg_ga8;
                 pData->fRetrieverow = (mng_ptr)retrieve_ga8;
                 pData->bIsOpaque    = MNG_FALSE;
                 break;
               }
          case MNG_COLORTYPE_JPEGCOLORA :
               {
                 pData->fStorerow2   = (mng_ptr)store_jpeg_rgba8;
                 pData->fRetrieverow = (mng_ptr)retrieve_rgba8;
                 pData->bIsOpaque    = MNG_FALSE;
                 break;
               }
        }
      }
      else
      {
        pData->bIsRGBA16 = MNG_TRUE;   /* intermediate row is 16-bit deep */

        /* TODO: 12-bit JPEG */
        /* TODO: 8- + 12-bit JPEG (eg. type=20) */

      }
                                       /* determine alpha processing routine */
      switch (pData->iJHDRalphabitdepth)
      {
        case  1 : { pData->fInitrowproc = (mng_ptr)init_jpeg_a1_ni;  break; }
        case  2 : { pData->fInitrowproc = (mng_ptr)init_jpeg_a2_ni;  break; }
        case  4 : { pData->fInitrowproc = (mng_ptr)init_jpeg_a4_ni;  break; }
        case  8 : { pData->fInitrowproc = (mng_ptr)init_jpeg_a8_ni;  break; }
        case 16 : { pData->fInitrowproc = (mng_ptr)init_jpeg_a16_ni; break; }
      }
                                       /* initialize JPEG library */
      iRetcode = mngjpeg_initialize (pData);

      if (iRetcode)                    /* on error bail out */
        return iRetcode;
    }
    else
    {                                  /* must be alpha add/replace !! */
      if ((pData->iDeltatype != MNG_DELTATYPE_BLOCKALPHAADD    ) &&
          (pData->iDeltatype != MNG_DELTATYPE_BLOCKALPHAREPLACE)    )
        MNG_ERROR (pData, MNG_INVDELTATYPE)
                                       /* determine alpha processing routine */
      switch (pData->iJHDRalphabitdepth)
      {
        case  1 : { pData->fInitrowproc = (mng_ptr)init_g1_ni;  break; }
        case  2 : { pData->fInitrowproc = (mng_ptr)init_g2_ni;  break; }
        case  4 : { pData->fInitrowproc = (mng_ptr)init_g4_ni;  break; }
        case  8 : { pData->fInitrowproc = (mng_ptr)init_g8_ni;  break; }
        case 16 : { pData->fInitrowproc = (mng_ptr)init_g16_ni; break; }
      }
    }
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_DISPLAY_JHDR, MNG_LC_END)
#endif

  return MNG_NOERROR;
}
#endif /* MNG_INCLUDE_JNG */

/* ************************************************************************** */

#ifdef MNG_INCLUDE_JNG
mng_retcode process_display_jdat (mng_datap  pData,
                                  mng_uint32 iRawlen,
                                  mng_uint8p pRawdata)
{
  mng_retcode iRetcode = MNG_NOERROR;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_DISPLAY_JDAT, MNG_LC_START)
#endif

  if (!pData->bJPEGdecompress)         /* if we're not decompressing already */
  {
    if (pData->fInitrowproc)           /* initialize row-processing */
      iRetcode = ((mng_initrowproc)pData->fInitrowproc) (pData);
    else
      iRetcode = init_rowproc (pData); /* this still if no alpha present ! */   

    if (!iRetcode)                     /* initialize decompress */
      iRetcode = mngjpeg_decompressinit (pData);
  }

  if (!iRetcode)                       /* all ok? then decompress, my man */
    iRetcode = mngjpeg_decompressdata (pData, iRawlen, pRawdata);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_DISPLAY_JDAT, MNG_LC_END)
#endif

  return iRetcode;
}
#endif /* MNG_INCLUDE_JNG */

/* ************************************************************************** */

mng_retcode process_display_dhdr (mng_datap  pData,
                                  mng_uint16 iObjectid,
                                  mng_uint8  iImagetype,
                                  mng_uint8  iDeltatype,
                                  mng_uint32 iBlockwidth,
                                  mng_uint32 iBlockheight,
                                  mng_uint32 iBlockx,
                                  mng_uint32 iBlocky)
{
  mng_imagep  pImage;
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_DISPLAY_DHDR, MNG_LC_START)
#endif

  pData->fInitrowproc     = 0;         /* do nothing by default */
  pData->fDisplayrow      = 0;
  pData->fCorrectrow      = 0;
  pData->fStorerow        = 0;
  pData->fProcessrow      = 0;
  pData->pStoreobj        = 0;

  pData->fDeltagetrow     = 0;
  pData->fDeltaaddrow     = 0;
  pData->fDeltareplacerow = 0;
  pData->fDeltaputrow     = 0;

  pImage = find_imageobject (pData, iObjectid);

  if (pImage)                          /* object exists ? */
  {
    if (pImage->pImgbuf->bConcrete)    /* is it concrete ? */
    {                                  /* save delta fields */
      pData->pDeltaImage       = (mng_ptr)pImage;
      pData->iDeltaImagetype   = iImagetype;
      pData->iDeltatype        = iDeltatype;
      pData->iDeltaBlockwidth  = iBlockwidth;
      pData->iDeltaBlockheight = iBlockheight;
      pData->iDeltaBlockx      = iBlockx;
      pData->iDeltaBlocky      = iBlocky;
                                       /* restore target-object fields */
      pData->iDatawidth        = pImage->pImgbuf->iWidth;
      pData->iDataheight       = pImage->pImgbuf->iHeight;
      pData->iBitdepth         = pImage->pImgbuf->iBitdepth;
      pData->iColortype        = pImage->pImgbuf->iColortype;
      pData->iCompression      = pImage->pImgbuf->iCompression;
      pData->iFilter           = pImage->pImgbuf->iFilter;
      pData->iInterlace        = pImage->pImgbuf->iInterlace;
                                       /* block size specified ? */
      if (iDeltatype != MNG_DELTATYPE_NOCHANGE)
      {
        pData->iDatawidth      = iBlockwidth;
        pData->iDataheight     = iBlockheight;
      }

      switch (iDeltatype)              /* determine nr of delta-channels */
      {
         case MNG_DELTATYPE_BLOCKALPHAADD : ;
         case MNG_DELTATYPE_BLOCKALPHAREPLACE :
              {
                if (pData->iColortype == MNG_COLORTYPE_GRAYA)
                  pData->iColortype = MNG_COLORTYPE_GRAY;
                else
                if (pData->iColortype == MNG_COLORTYPE_RGBA)
                  pData->iColortype = MNG_COLORTYPE_GRAY;
                else                   /* target has no alpha; that sucks! */
                  MNG_ERROR (pData, MNG_TARGETNOALPHA)

                break;
              }

         case MNG_DELTATYPE_BLOCKCOLORADD : ;
         case MNG_DELTATYPE_BLOCKCOLORREPLACE :
              {
                if (pData->iColortype == MNG_COLORTYPE_GRAYA)
                  pData->iColortype = MNG_COLORTYPE_GRAY;
                else
                if (pData->iColortype == MNG_COLORTYPE_RGBA)
                  pData->iColortype = MNG_COLORTYPE_RGB;
                else                   /* target has no alpha; that sucks! */
                  MNG_ERROR (pData, MNG_TARGETNOALPHA)

                break;
              }

      }
                                       /* full image replace ? */
      if (iDeltatype == MNG_DELTATYPE_REPLACE)
      {
        iRetcode = reset_object_details (pData, pImage,
                                         pData->iDatawidth, pData->iDataheight,
                                         pData->iBitdepth, pData->iColortype,
                                         pData->iCompression, pData->iFilter,
                                         pData->iInterlace, MNG_FALSE);

        if (iRetcode)                  /* on error bail out */
          return iRetcode;

        pData->pStoreobj = pImage;     /* and store straight into this object */
      }
      else
      {
        mng_imagedatap pBufzero, pBuf;
                                       /* we store in object 0 and process it later */
        pData->pStoreobj = pData->pObjzero;
                                       /* make sure to initialize object 0 then */
        iRetcode = reset_object_details (pData, (mng_imagep)pData->pObjzero,
                                         pData->iDatawidth, pData->iDataheight,
                                         pData->iBitdepth, pData->iColortype,
                                         pData->iCompression, pData->iFilter,
                                         pData->iInterlace, MNG_TRUE);

        if (iRetcode)                  /* on error bail out */
          return iRetcode;

        pBuf     = pImage->pImgbuf;    /* copy possible palette & cheap transparency */
        pBufzero = ((mng_imagep)pData->pObjzero)->pImgbuf;

        pBufzero->bHasPLTE = pBuf->bHasPLTE;
        pBufzero->bHasTRNS = pBuf->bHasTRNS;

        if (pBufzero->bHasPLTE)        /* copy palette ? */
        {
          mng_uint32 iX;

          pBufzero->iPLTEcount = pBuf->iPLTEcount;

          for (iX = 0; iX < pBuf->iPLTEcount; iX++)
          {
            pBufzero->aPLTEentries [iX].iRed   = pBuf->aPLTEentries [iX].iRed;
            pBufzero->aPLTEentries [iX].iGreen = pBuf->aPLTEentries [iX].iGreen;
            pBufzero->aPLTEentries [iX].iBlue  = pBuf->aPLTEentries [iX].iBlue;
          }
        }

        if (pBufzero->bHasTRNS)        /* copy cheap transparency ? */
        {
          pBufzero->iTRNSgray  = pBuf->iTRNSgray;
          pBufzero->iTRNSred   = pBuf->iTRNSred;
          pBufzero->iTRNSgreen = pBuf->iTRNSgreen;
          pBufzero->iTRNSblue  = pBuf->iTRNSblue;
          pBufzero->iTRNScount = pBuf->iTRNScount;

          MNG_COPY (&pBufzero->aTRNSentries, &pBuf->aTRNSentries,
                    sizeof (pBufzero->aTRNSentries))
        }
                                       /* process immediatly if bitdepth & colortype are equal */
        pData->bDeltaimmediate =
          (mng_bool)((pData->iBitdepth  == ((mng_imagep)pData->pDeltaImage)->pImgbuf->iBitdepth ) &&
                     (pData->iColortype == ((mng_imagep)pData->pDeltaImage)->pImgbuf->iColortype)    );
      }

      switch (pData->iColortype)       /* determine row initialization routine */
      {
        case 0 : {                     /* gray */
                   switch (pData->iBitdepth)
                   {
                     case  1 : {
                                 if (!pData->iInterlace)
                                   pData->fInitrowproc = (mng_ptr)init_g1_ni;
                                 else
                                   pData->fInitrowproc = (mng_ptr)init_g1_i;

                                 break;
                               }
                     case  2 : {
                                 if (!pData->iInterlace)
                                   pData->fInitrowproc = (mng_ptr)init_g2_ni;
                                 else
                                   pData->fInitrowproc = (mng_ptr)init_g2_i;

                                 break;
                               }
                     case  4 : {
                                 if (!pData->iInterlace)
                                   pData->fInitrowproc = (mng_ptr)init_g4_ni;
                                 else
                                   pData->fInitrowproc = (mng_ptr)init_g4_i;

                                 break;
                               }
                     case  8 : {
                                 if (!pData->iInterlace)
                                   pData->fInitrowproc = (mng_ptr)init_g8_ni;
                                 else
                                   pData->fInitrowproc = (mng_ptr)init_g8_i;

                                 break;
                               }
                     case 16 : {
                                 if (!pData->iInterlace)
                                   pData->fInitrowproc = (mng_ptr)init_g16_ni;
                                 else
                                   pData->fInitrowproc = (mng_ptr)init_g16_i;

                                 break;
                               }
                   }

                   break;
                 }
        case 2 : {                     /* rgb */
                   switch (pData->iBitdepth)
                   {
                     case  8 : {
                                 if (!pData->iInterlace)
                                   pData->fInitrowproc = (mng_ptr)init_rgb8_ni;
                                 else
                                   pData->fInitrowproc = (mng_ptr)init_rgb8_i;

                                 break;
                               }
                     case 16 : {
                                 if (!pData->iInterlace)
                                   pData->fInitrowproc = (mng_ptr)init_rgb16_ni;
                                 else
                                   pData->fInitrowproc = (mng_ptr)init_rgb16_i;

                                 break;
                               }
                   }

                   break;
                 }
        case 3 : {                     /* indexed */
                   switch (pData->iBitdepth)
                   {
                     case  1 : {
                                 if (!pData->iInterlace)
                                   pData->fInitrowproc = (mng_ptr)init_idx1_ni;
                                 else
                                   pData->fInitrowproc = (mng_ptr)init_idx1_i;

                                 break;
                               }
                     case  2 : {
                                 if (!pData->iInterlace)
                                   pData->fInitrowproc = (mng_ptr)init_idx2_ni;
                                 else
                                   pData->fInitrowproc = (mng_ptr)init_idx2_i;

                                 break;
                               }
                     case  4 : {
                                 if (!pData->iInterlace)
                                   pData->fInitrowproc = (mng_ptr)init_idx4_ni;
                                 else
                                   pData->fInitrowproc = (mng_ptr)init_idx4_i;

                                 break;
                               }
                     case  8 : {
                                 if (!pData->iInterlace)
                                   pData->fInitrowproc = (mng_ptr)init_idx8_ni;
                                 else
                                   pData->fInitrowproc = (mng_ptr)init_idx8_i;

                                 break;
                               }
                   }

                   break;
                 }
        case 4 : {                     /* gray+alpha */
                   switch (pData->iBitdepth)
                   {
                     case  8 : {
                                 if (!pData->iInterlace)
                                   pData->fInitrowproc = (mng_ptr)init_ga8_ni;
                                 else
                                   pData->fInitrowproc = (mng_ptr)init_ga8_i;

                                 break;
                               }
                     case 16 : {
                                 if (!pData->iInterlace)
                                   pData->fInitrowproc = (mng_ptr)init_ga16_ni;
                                 else
                                   pData->fInitrowproc = (mng_ptr)init_ga16_i;

                                 break;
                               }
                   }

                   break;
                 }
        case 6 : {                     /* rgb+alpha */
                   switch (pData->iBitdepth)
                   {
                     case  8 : {
                                 if (!pData->iInterlace)
                                   pData->fInitrowproc = (mng_ptr)init_rgba8_ni;
                                 else
                                   pData->fInitrowproc = (mng_ptr)init_rgba8_i;

                                 break;
                               }
                     case 16 : {
                                 if (!pData->iInterlace)
                                   pData->fInitrowproc = (mng_ptr)init_rgba16_ni;
                                 else
                                   pData->fInitrowproc = (mng_ptr)init_rgba16_i;

                                 break;
                               }
                   }

                   break;
                 }
      }
    }
    else
      MNG_ERROR (pData, MNG_OBJNOTCONCRETE)

  }
  else
    MNG_ERROR (pData, MNG_OBJECTUNKNOWN)

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_DISPLAY_DHDR, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode process_display_prom (mng_datap  pData,
                                  mng_uint8  iBitdepth,
                                  mng_uint8  iColortype,
                                  mng_uint8  iFilltype)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_DISPLAY_PROM, MNG_LC_START)
#endif




#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_DISPLAY_PROM, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode process_display_ipng (mng_datap pData)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_DISPLAY_IPNG, MNG_LC_START)
#endif
                                       /* indicate it for what it is now */
  pData->iDeltaImagetype = MNG_IMAGETYPE_PNG;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_DISPLAY_IPNG, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode process_display_ijng (mng_datap pData)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_DISPLAY_IJNG, MNG_LC_START)
#endif
                                       /* indicate it for what it is now */
  pData->iDeltaImagetype = MNG_IMAGETYPE_JNG;
                                       /* get the alpha-bitdepth!!! */
  pData->iBitdepth       = ((mng_imagep)pData->pDeltaImage)->pImgbuf->iAlphabitdepth;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_DISPLAY_IJNG, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode process_display_pplt (mng_datap      pData,
                                  mng_uint8      iType,
                                  mng_uint32     iCount,
                                  mng_rgbpaltab* paIndexentries,
                                  mng_uint8arr*  paAlphaentries,
                                  mng_uint8arr*  paUsedentries)
{
  mng_uint32     iX;
  mng_imagep     pImage = (mng_imagep)pData->pObjzero;
  mng_imagedatap pBuf   = pImage->pImgbuf;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_DISPLAY_PPLT, MNG_LC_START)
#endif

  switch (iType)
  {
    case MNG_DELTATYPE_REPLACERGB :
      {
        for (iX = 0; iX < iCount; iX++)
        {
          if ((*(paUsedentries)) [iX])
          {
            pBuf->aPLTEentries [iX].iRed   = (*(paIndexentries)) [iX].iRed;
            pBuf->aPLTEentries [iX].iGreen = (*(paIndexentries)) [iX].iGreen;
            pBuf->aPLTEentries [iX].iBlue  = (*(paIndexentries)) [iX].iBlue;
          }
        }

        break;
      }
    case MNG_DELTATYPE_DELTARGB :
      {
        for (iX = 0; iX < iCount; iX++)
        {
          if ((*(paUsedentries)) [iX])
          {
            pBuf->aPLTEentries [iX].iRed   =
                               (mng_uint8)(pBuf->aPLTEentries [iX].iRed   +
                                           (*(paIndexentries)) [iX].iRed  );
            pBuf->aPLTEentries [iX].iGreen =
                               (mng_uint8)(pBuf->aPLTEentries [iX].iGreen +
                                           (*(paIndexentries)) [iX].iGreen);
            pBuf->aPLTEentries [iX].iBlue  =
                               (mng_uint8)(pBuf->aPLTEentries [iX].iBlue  +
                                           (*(paIndexentries)) [iX].iBlue );
          }
        }

        break;
      }
    case MNG_DELTATYPE_REPLACEALPHA :
      {
        for (iX = 0; iX < iCount; iX++)
        {
          if ((*(paUsedentries)) [iX])
            pBuf->aTRNSentries [iX] = (*(paAlphaentries)) [iX];
        }

        break;
      }
    case MNG_DELTATYPE_DELTAALPHA :
      {
        for (iX = 0; iX < iCount; iX++)
        {
          if ((*(paUsedentries)) [iX])
            pBuf->aTRNSentries [iX] =
                               (mng_uint8)(pBuf->aTRNSentries [iX] +
                                           (*(paAlphaentries)) [iX]);
        }

        break;
      }
    case MNG_DELTATYPE_REPLACERGBA :
      {
        for (iX = 0; iX < iCount; iX++)
        {
          if ((*(paUsedentries)) [iX])
          {
            pBuf->aPLTEentries [iX].iRed   = (*(paIndexentries)) [iX].iRed;
            pBuf->aPLTEentries [iX].iGreen = (*(paIndexentries)) [iX].iGreen;
            pBuf->aPLTEentries [iX].iBlue  = (*(paIndexentries)) [iX].iBlue;
            pBuf->aTRNSentries [iX]        = (*(paAlphaentries)) [iX];
          }
        }

        break;
      }
    case MNG_DELTATYPE_DELTARGBA :
      {
        for (iX = 0; iX < iCount; iX++)
        {
          if ((*(paUsedentries)) [iX])
          {
            pBuf->aPLTEentries [iX].iRed   =
                               (mng_uint8)(pBuf->aPLTEentries [iX].iRed   +
                                           (*(paIndexentries)) [iX].iRed  );
            pBuf->aPLTEentries [iX].iGreen =
                               (mng_uint8)(pBuf->aPLTEentries [iX].iGreen +
                                           (*(paIndexentries)) [iX].iGreen);
            pBuf->aPLTEentries [iX].iBlue  =
                               (mng_uint8)(pBuf->aPLTEentries [iX].iBlue  +
                                           (*(paIndexentries)) [iX].iBlue );
            pBuf->aTRNSentries [iX] =
                               (mng_uint8)(pBuf->aTRNSentries [iX] +
                                           (*(paAlphaentries)) [iX]);
          }
        }

        break;
      }
  }

  if ((iType != MNG_DELTATYPE_REPLACERGB) && (iType != MNG_DELTATYPE_DELTARGB))
  {
    if (pBuf->bHasTRNS)
    {
      if (iCount > pBuf->iTRNScount)
        pBuf->iTRNScount = iCount;
    }
    else
    {
      pBuf->iTRNScount = iCount;
      pBuf->bHasTRNS   = MNG_TRUE;
    }
  }

  if ((iType != MNG_DELTATYPE_REPLACEALPHA) && (iType != MNG_DELTATYPE_DELTAALPHA))
  {
    if (iCount > pBuf->iPLTEcount)
      pBuf->iPLTEcount = iCount;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_DISPLAY_PPLT, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

#endif /* MNG_INCLUDE_DISPLAY_PROCS */

/* ************************************************************************** */
/* * end of file                                                            * */
/* ************************************************************************** */

