/* ************************************************************************** */
/* *             For conditions of distribution and use,                    * */
/* *                see copyright notice in libmng.h                        * */
/* ************************************************************************** */
/* *                                                                        * */
/* * project   : libmng                                                     * */
/* * file      : libmng_pixels.c           copyright (c) 2000 G.Juyn        * */
/* * version   : 1.0.2                                                      * */
/* *                                                                        * */
/* * purpose   : Pixel-row management routines (implementation)             * */
/* *                                                                        * */
/* * author    : G.Juyn                                                     * */
/* * web       : http://www.3-t.com                                         * */
/* * email     : mailto:info@3-t.com                                        * */
/* *                                                                        * */
/* * comment   : implementation of the pixel-row management routines        * */
/* *                                                                        * */
/* *             the dual alpha-composing for RGBA/BGRA/etc output-canvas'  * */
/* *             is based on the Note on Compositing chapter of the         * */
/* *             DOH-3 draft, noted to me by Adam M. Costello               * */
/* *                                                                        * */
/* * changes   : 0.5.1 - 05/08/2000 - G.Juyn                                * */
/* *             - changed strict-ANSI stuff                                * */
/* *             0.5.1 - 05/11/2000 - G.Juyn                                * */
/* *             - added callback error-reporting support                   * */
/* *             0.5.1 - 05/12/2000 - G.Juyn                                * */
/* *             - changed trace to macro for callback error-reporting      * */
/* *                                                                        * */
/* *             0.5.2 - 05/22/2000 - G.Juyn                                * */
/* *             - added JNG support                                        * */
/* *             0.5.2 - 05/30/2000 - G.Juyn                                * */
/* *             - fixed minor bugs 16-bit pixel-handling                   * */
/* *             - added delta-image row-processing routines                * */
/* *             0.5.2 - 06/02/2000 - G.Juyn                                * */
/* *             - fixed endian support (hopefully)                         * */
/* *             0.5.2 - 06/03/2000 - G.Juyn                                * */
/* *             - fixed makeup for Linux gcc compile                       * */
/* *             0.5.2 - 06/05/2000 - G.Juyn                                * */
/* *             - implemented app bkgd restore routines                    * */
/* *             - implemented RGBA8, ARGB8, BGRA8 & ABGR8 display routines * */
/* *             - added support for RGB8_A8 canvasstyle                    * */
/* *             0.5.2 - 06/09/2000 - G.Juyn                                * */
/* *             - fixed alpha-handling for alpha canvasstyles              * */
/* *                                                                        * */
/* *             0.5.3 - 06/16/2000 - G.Juyn                                * */
/* *             - changed progressive-display processing                   * */
/* *             0.5.3 - 06/17/2000 - G.Juyn                                * */
/* *             - changed to support delta-images                          * */
/* *             - optimized some store_xxx routines                        * */
/* *             0.5.3 - 06/20/2000 - G.Juyn                                * */
/* *             - fixed nasty bug with embedded PNG after delta-image      * */
/* *             0.5.3 - 06/24/2000 - G.Juyn                                * */
/* *             - fixed problem with 16-bit GA format                      * */
/* *             0.5.3 - 06/25/2000 - G.Juyn                                * */
/* *             - fixed problem with cheap transparency for 4-bit gray     * */
/* *             - fixed display_xxxx routines for interlaced images        * */
/* *             0.5.3 - 06/28/2000 - G.Juyn                                * */
/* *             - fixed compiler-warning for non-initialized iB variable   * */
/* *                                                                        * */
/* *             0.9.1 - 07/05/2000 - G.Juyn                                * */
/* *             - fixed mandatory BACK color to be opaque                  * */
/* *                                                                        * */
/* *             0.9.2 - 07/31/2000 - G.Juyn                                * */
/* *             - B110547 - fixed bug in interlace code                    * */
/* *             0.9.2 - 08/05/2000 - G.Juyn                                * */
/* *             - changed file-prefixes                                    * */
/* *                                                                        * */
/* *             0.9.3 - 08/20/2000 - G.Juyn                                * */
/* *             - fixed app-supplied background restore                    * */
/* *             0.9.3 - 08/26/2000 - G.Juyn                                * */
/* *             - added MAGN chunk                                         * */
/* *             0.9.3 - 09/07/2000 - G.Juyn                                * */
/* *             - added support for new filter_types                       * */
/* *             0.9.3 - 09/30/2000 - G.Juyn                                * */
/* *             - fixed MAGN rounding errors (thanks Matthias!)            * */
/* *             0.9.3 - 10/10/2000 - G.Juyn                                * */
/* *             - fixed alpha-blending for RGBA canvasstyle                * */
/* *             0.9.3 - 10/11/2000 - G.Juyn                                * */
/* *             - fixed alpha-blending for other alpha-canvasstyles        * */
/* *             0.9.3 - 10/16/2000 - G.Juyn                                * */
/* *             - added optional support for bKGD for PNG images           * */
/* *             - added support for JDAA                                   * */
/* *             0.9.3 - 10/17/2000 - G.Juyn                                * */
/* *             - fixed support for bKGD                                   * */
/* *             0.9.3 - 10/19/2000 - G.Juyn                                * */
/* *             - implemented delayed delta-processing                     * */
/* *             0.9.3 - 10/28/2000 - G.Juyn                                * */
/* *             - fixed tRNS processing for gray-image < 8-bits            * */
/* *                                                                        * */
/* *             0.9.4 - 12/16/2000 - G.Juyn                                * */
/* *             - fixed mixup of data- & function-pointers (thanks Dimitri)* */
/* *             0.9.4 -  1/18/2001 - G.Juyn                                * */
/* *             - removed "old" MAGN methods 3 & 4                         * */
/* *             - added "new" MAGN methods 3, 4 & 5                        * */
/* *             - removed test filter-methods 1 & 65                       * */
/* *                                                                        * */
/* *             1.0.1 - 04/21/2001 - G.Juyn (code by G.Kelly)              * */
/* *             - added BGRA8 canvas with premultiplied alpha              * */
/* *             1.0.1 - 04/25/2001 - G.Juyn                                * */
/* *             - moved mng_clear_cms to libmng_cms                        * */
/* *                                                                        * */
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
#include "libmng_memory.h"
#include "libmng_cms.h"
#include "libmng_filter.h"
#include "libmng_pixels.h"

#if defined(__BORLANDC__) && defined(MNG_STRICT_ANSI)
#pragma option -A                      /* force ANSI-C */
#endif

/* ************************************************************************** */

#ifdef MNG_INCLUDE_DISPLAY_PROCS

/* TODO: magnification & canvas-positioning/-clipping */

/* TODO: major optimization of pixel-loops by using assembler (?) */

/* ************************************************************************** */
/* *                                                                        * */
/* * Interlace tables                                                       * */
/* *                                                                        * */
/* ************************************************************************** */

mng_uint32 const interlace_row      [7] = { 0, 0, 4, 0, 2, 0, 1 };
mng_uint32 const interlace_rowskip  [7] = { 8, 8, 8, 4, 4, 2, 2 };
mng_uint32 const interlace_col      [7] = { 0, 4, 0, 2, 0, 1, 0 };
mng_uint32 const interlace_colskip  [7] = { 8, 8, 4, 4, 2, 2, 1 };
mng_uint32 const interlace_roundoff [7] = { 7, 7, 3, 3, 1, 1, 0 };
mng_uint32 const interlace_divider  [7] = { 3, 3, 2, 2, 1, 1, 0 };

/* ************************************************************************** */
/* *                                                                        * */
/* * Alpha composing macros                                                 * */
/* * the code below is slightly modified from the libpng package            * */
/* * the original was last optimized by Greg Roelofs & Mark Adler           * */
/* *                                                                        * */
/* ************************************************************************** */

#define MNG_COMPOSE8(RET,FG,ALPHA,BG) {                                    \
       mng_uint16 iH = (mng_uint16)((mng_uint16)(FG) * (mng_uint16)(ALPHA) \
                        + (mng_uint16)(BG)*(mng_uint16)(255 -              \
                          (mng_uint16)(ALPHA)) + (mng_uint16)128);         \
       (RET) = (mng_uint8)((iH + (iH >> 8)) >> 8); }

#define MNG_COMPOSE16(RET,FG,ALPHA,BG) {                                   \
       mng_uint32 iH = (mng_uint32)((mng_uint32)(FG) * (mng_uint32)(ALPHA) \
                        + (mng_uint32)(BG)*(mng_uint32)(65535L -           \
                          (mng_uint32)(ALPHA)) + (mng_uint32)32768L);      \
       (RET) = (mng_uint16)((iH + (iH >> 16)) >> 16); }

/* ************************************************************************** */
/* *                                                                        * */
/* * Alpha blending macros                                                  * */
/* * this code is based on Adam Costello's "Note on Compositing" from the   * */
/* * mng-list which gives the following formula:                            * */
/* *                                                                        * */
/* * top pixel       = (Rt, Gt, Bt, At)                                     * */
/* * bottom pixel    = (Rb, Gb, Bb, Ab)                                     * */
/* * composite pixel = (Rc, Gc, Bc, Ac)                                     * */
/* *                                                                        * */
/* * all values in the range 0..1                                           * */
/* *                                                                        * */
/* * Ac = 1 - (1 - At)(1 - Ab)                                              * */
/* * s = At / Ac                                                            * */
/* * t = (1 - At) Ab / Ac                                                   * */
/* * Rc = s Rt + t Rb                                                       * */
/* * Gc = s Gt + t Gb                                                       * */
/* * Bc = s Bt + t Bb                                                       * */
/* *                                                                        * */
/* * (I just hope I coded it correctly in integer arithmetic...)            * */
/* *                                                                        * */
/* ************************************************************************** */

#define MNG_BLEND8(RT, GT, BT, AT, RB, GB, BB, AB, RC, GC, BC, AC) {         \
       mng_uint32 S, T;                                                      \
       (AC) = (mng_uint8)((mng_uint32)255 -                                  \
                          ((((mng_uint32)255 - (mng_uint32)(AT)) *           \
                            ((mng_uint32)255 - (mng_uint32)(AB))   ) >> 8)); \
       S    = (mng_uint32)(((mng_uint32)(AT) << 8) /                         \
                           (mng_uint32)(AC));                                \
       T    = (mng_uint32)(((mng_uint32)255 - (mng_uint32)(AT)) *            \
                            (mng_uint32)(AB) / (mng_uint32)(AC));            \
       (RC) = (mng_uint8)((S * (mng_uint32)(RT) +                            \
                           T * (mng_uint32)(RB) + (mng_uint32)127) >> 8);    \
       (GC) = (mng_uint8)((S * (mng_uint32)(GT) +                            \
                           T * (mng_uint32)(GB) + (mng_uint32)127) >> 8);    \
       (BC) = (mng_uint8)((S * (mng_uint32)(BT) +                            \
                           T * (mng_uint32)(BB) + (mng_uint32)127) >> 8); }

#define MNG_BLEND16(RT, GT, BT, AT, RB, GB, BB, AB, RC, GC, BC, AC) {            \
       mng_uint32 S, T;                                                          \
       (AC) = (mng_uint16)((mng_uint32)65525 -                                   \
                           ((((mng_uint32)65535 - (mng_uint32)(AT)) *            \
                             ((mng_uint32)65535 - (mng_uint32)(AB))   ) >> 16)); \
       S    = (mng_uint32)(((mng_uint32)(AT) << 16) /                            \
                            (mng_uint32)(AC));                                   \
       T    = (mng_uint32)(((mng_uint32)65535 - (mng_uint32)(AT)) *              \
                            (mng_uint32)(AB) / (mng_uint32)(AC));                \
       (RC) = (mng_uint16)((S * (mng_uint32)(RT) +                               \
                            T * (mng_uint32)(RB) + (mng_uint32)32767) >> 16);    \
       (GC) = (mng_uint16)((S * (mng_uint32)(GT) +                               \
                            T * (mng_uint32)(GB) + (mng_uint32)32767) >> 16);    \
       (BC) = (mng_uint16)((S * (mng_uint32)(BT) +                               \
                            T * (mng_uint32)(BB) + (mng_uint32)32767) >> 16); }

/* ************************************************************************** */

/* note a good optimizing compiler will optimize this */
#define DIV255B8(x) (mng_uint8)(((x) + 127) / 255)
#define DIV255B16(x) (mng_uint16)(((x) + 32767) / 65535)

/* ************************************************************************** */
/* *                                                                        * */
/* * Progressive display check - checks to see if progressive display is    * */
/* * in order & indicates so                                                * */
/* *                                                                        * */
/* * The routine is called after a call to one of the display_xxx routines  * */
/* * if appropriate                                                         * */
/* *                                                                        * */
/* * The refresh is warrented in the read_chunk routine (mng_read.c)        * */
/* * and only during read&display processing, since there's not much point  * */
/* * doing it from memory!                                                  * */
/* *                                                                        * */
/* ************************************************************************** */

mng_retcode display_progressive_check (mng_datap pData)
{
  if ((pData->bDoProgressive) &&       /* need progressive display? */
      ((pData->eImagetype != mng_it_mng) || (pData->iDataheight > 300)) &&
      (pData->iDestb - pData->iDestt > 50) && (!pData->pCurraniobj))
  {
    mng_int32 iC = pData->iRow + pData->iDestt - pData->iSourcet;

    if (iC % 20 == 0)                  /* every 20th line */
      pData->bNeedrefresh = MNG_TRUE;

  }

  return MNG_NOERROR;
}

/* ************************************************************************** */
/* *                                                                        * */
/* * Display routines - convert rowdata (which is already color-corrected)  * */
/* * to the output canvas, respecting the opacity information               * */
/* *                                                                        * */
/* ************************************************************************** */

void check_update_region (mng_datap pData)
{                                      /* determine actual canvas row */
  mng_int32 iRow = pData->iRow + pData->iDestt - pData->iSourcet;
                                       /* check for change in update-region */
  if ((pData->iDestl < (mng_int32)pData->iUpdateleft) || (pData->iUpdateright == 0))
    pData->iUpdateleft   = pData->iDestl;

  if (pData->iDestr > (mng_int32)pData->iUpdateright)
    pData->iUpdateright  = pData->iDestr;

  if ((iRow < (mng_int32)pData->iUpdatetop) || (pData->iUpdatebottom == 0))
    pData->iUpdatetop    = iRow;

  if (iRow+1 > (mng_int32)pData->iUpdatebottom)
    pData->iUpdatebottom = iRow+1;

  return;
}

/* ************************************************************************** */

mng_retcode display_rgb8 (mng_datap pData)
{
  mng_uint8p pScanline;
  mng_uint8p pDataline;
  mng_int32  iX;
  mng_uint16 iA16;
  mng_uint16 iFGr16, iFGg16, iFGb16;
  mng_uint16 iBGr16, iBGg16, iBGb16;
  mng_uint8  iA8;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DISPLAY_RGB8, MNG_LC_START)
#endif
                                       /* viewable row ? */
  if ((pData->iRow >= pData->iSourcet) && (pData->iRow < pData->iSourceb))
  {                                    /* address destination row */
    pScanline = (mng_uint8p)pData->fGetcanvasline (((mng_handle)pData),
                                                   pData->iRow + pData->iDestt -
                                                   pData->iSourcet);
                                       /* adjust destination row starting-point */
    pScanline = pScanline + (pData->iCol * 3) + (pData->iDestl * 3);
    pDataline = pData->pRGBArow;       /* address source row */

    if (pData->bIsRGBA16)              /* adjust source row starting-point */
      pDataline = pDataline + ((pData->iSourcel / pData->iColinc) << 3);
    else
      pDataline = pDataline + ((pData->iSourcel / pData->iColinc) << 2);

    if (pData->bIsOpaque)              /* forget about transparency ? */
    {
      if (pData->bIsRGBA16)            /* 16-bit input row ? */
      {
        for (iX = pData->iSourcel + pData->iCol; iX < pData->iSourcer; iX += pData->iColinc)
        {                              /* scale down by dropping the LSB */
          *pScanline     = *pDataline;
          *(pScanline+1) = *(pDataline+2);
          *(pScanline+2) = *(pDataline+4);

          pScanline += (pData->iColinc * 3);
          pDataline += 8;
        }
      }
      else
      {
        for (iX = pData->iSourcel + pData->iCol; iX < pData->iSourcer; iX += pData->iColinc)
        {                              /* copy the values */
          *pScanline     = *pDataline;
          *(pScanline+1) = *(pDataline+1);
          *(pScanline+2) = *(pDataline+2);

          pScanline += (pData->iColinc * 3);
          pDataline += 4;
        }
      }
    }
    else
    {
      if (pData->bIsRGBA16)            /* 16-bit input row ? */
      {
        for (iX = pData->iSourcel + pData->iCol; iX < pData->iSourcer; iX += pData->iColinc)
        {
          iA16 = mng_get_uint16 (pDataline+6);

          if (iA16)                    /* any opacity at all ? */
          {
            if (iA16 == 0xFFFF)        /* fully opaque ? */
            {                          /* scale down by dropping the LSB */
              *pScanline     = *pDataline;
              *(pScanline+1) = *(pDataline+2);
              *(pScanline+2) = *(pDataline+4);
            }
            else
            {                          /* get the proper values */
              iFGr16 = mng_get_uint16 (pDataline  );
              iFGg16 = mng_get_uint16 (pDataline+2);
              iFGb16 = mng_get_uint16 (pDataline+4);
                                       /* scale background up */
              iBGr16 = (mng_uint16)(*pScanline    );
              iBGg16 = (mng_uint16)(*(pScanline+1));
              iBGb16 = (mng_uint16)(*(pScanline+2));
              iBGr16 = (mng_uint16)((mng_uint32)iBGr16 << 8) | iBGr16;
              iBGg16 = (mng_uint16)((mng_uint32)iBGg16 << 8) | iBGg16;
              iBGb16 = (mng_uint16)((mng_uint32)iBGb16 << 8) | iBGb16;
                                       /* now compose */
              MNG_COMPOSE16(iFGr16, iFGr16, iA16, iBGr16)
              MNG_COMPOSE16(iFGg16, iFGg16, iA16, iBGg16)
              MNG_COMPOSE16(iFGb16, iFGb16, iA16, iBGb16)
                                       /* and return the composed values */
              *pScanline     = (mng_uint8)(iFGr16 >> 8);
              *(pScanline+1) = (mng_uint8)(iFGg16 >> 8);
              *(pScanline+2) = (mng_uint8)(iFGb16 >> 8);
            }
          }

          pScanline += (pData->iColinc * 3);
          pDataline += 8;
        }
      }
      else
      {
        for (iX = pData->iSourcel + pData->iCol; iX < pData->iSourcer; iX += pData->iColinc)
        {
          iA8 = *(pDataline+3);        /* get alpha value */

          if (iA8)                     /* any opacity at all ? */
          {
            if (iA8 == 0xFF)           /* fully opaque ? */
            {                          /* then simply copy the values */
              *pScanline     = *pDataline;
              *(pScanline+1) = *(pDataline+1);
              *(pScanline+2) = *(pDataline+2);
            }
            else
            {                          /* do alpha composing */
              MNG_COMPOSE8 (*pScanline,     *pDataline,     iA8, *pScanline    )
              MNG_COMPOSE8 (*(pScanline+1), *(pDataline+1), iA8, *(pScanline+1))
              MNG_COMPOSE8 (*(pScanline+2), *(pDataline+2), iA8, *(pScanline+2))
            }
          }

          pScanline += (pData->iColinc * 3);
          pDataline += 4;
        }
      }
    }
  }

  check_update_region (pData);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DISPLAY_RGB8, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode display_rgba8 (mng_datap pData)
{
  mng_uint8p pScanline;
  mng_uint8p pDataline;
  mng_int32  iX;
  mng_uint16 iFGa16, iBGa16, iCa16;
  mng_uint8  iFGa8, iBGa8, iCa8;
  mng_uint16 iFGr16, iFGg16, iFGb16;
  mng_uint16 iBGr16, iBGg16, iBGb16;
  mng_uint16 iCr16, iCg16, iCb16;
  mng_uint8  iCr8, iCg8, iCb8;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DISPLAY_RGBA8, MNG_LC_START)
#endif
                                       /* viewable row ? */
  if ((pData->iRow >= pData->iSourcet) && (pData->iRow < pData->iSourceb))
  {                                    /* address destination row */
    pScanline = (mng_uint8p)pData->fGetcanvasline (((mng_handle)pData),
                                                   pData->iRow + pData->iDestt -
                                                   pData->iSourcet);
                                       /* adjust destination row starting-point */
    pScanline = pScanline + (pData->iCol << 2) + (pData->iDestl << 2);
    pDataline = pData->pRGBArow;       /* address source row */

    if (pData->bIsRGBA16)              /* adjust source row starting-point */
      pDataline = pDataline + ((pData->iSourcel / pData->iColinc) << 3);
    else
      pDataline = pDataline + ((pData->iSourcel / pData->iColinc) << 2);

    if (pData->bIsOpaque)              /* forget about transparency ? */
    {
      if (pData->bIsRGBA16)            /* 16-bit input row ? */
      {
        for (iX = pData->iSourcel + pData->iCol; iX < pData->iSourcer; iX += pData->iColinc)
        {                              /* scale down by dropping the LSB */
          *pScanline     = *pDataline;
          *(pScanline+1) = *(pDataline+2);
          *(pScanline+2) = *(pDataline+4);
          *(pScanline+3) = *(pDataline+6);

          pScanline += (pData->iColinc << 2);
          pDataline += 8;
        }
      }
      else
      {
        for (iX = pData->iSourcel + pData->iCol; iX < pData->iSourcer; iX += pData->iColinc)
        {                              /* copy the values */
          *pScanline     = *pDataline;
          *(pScanline+1) = *(pDataline+1);
          *(pScanline+2) = *(pDataline+2);
          *(pScanline+3) = *(pDataline+3);

          pScanline += (pData->iColinc << 2);
          pDataline += 4;
        }
      }
    }
    else
    {
      if (pData->bIsRGBA16)            /* 16-bit input row ? */
      {
        for (iX = pData->iSourcel + pData->iCol; iX < pData->iSourcer; iX += pData->iColinc)
        {                              /* get alpha values */
          iFGa16 = mng_get_uint16 (pDataline+6);
          iBGa16 = (mng_uint16)(*(pScanline+3));
          iBGa16 = (mng_uint16)(iBGa16 << 8) | iBGa16;

          if (iFGa16)                  /* any opacity at all ? */
          {                            /* fully opaque or background fully transparent ? */
            if ((iFGa16 == 0xFFFF) || (iBGa16 == 0))
            {                          /* plain copy it */
              *pScanline     = *pDataline;
              *(pScanline+1) = *(pDataline+2);
              *(pScanline+2) = *(pDataline+4);
              *(pScanline+3) = *(pDataline+6);
            }
            else
            {
              if (iBGa16 == 0xFFFF)    /* background fully opaque ? */
              {                        /* get the proper values */
                iFGr16 = mng_get_uint16 (pDataline  );
                iFGg16 = mng_get_uint16 (pDataline+2);
                iFGb16 = mng_get_uint16 (pDataline+4);
                                       /* scale background up */
                iBGr16 = (mng_uint16)(*pScanline    );
                iBGg16 = (mng_uint16)(*(pScanline+1));
                iBGb16 = (mng_uint16)(*(pScanline+2));
                iBGr16 = (mng_uint16)((mng_uint32)iBGr16 << 8) | iBGr16;
                iBGg16 = (mng_uint16)((mng_uint32)iBGg16 << 8) | iBGg16;
                iBGb16 = (mng_uint16)((mng_uint32)iBGb16 << 8) | iBGb16;
                                       /* now compose */
                MNG_COMPOSE16(iFGr16, iFGr16, iFGa16, iBGr16)
                MNG_COMPOSE16(iFGg16, iFGg16, iFGa16, iBGg16)
                MNG_COMPOSE16(iFGb16, iFGb16, iFGa16, iBGb16)
                                       /* and return the composed values */
                *pScanline     = (mng_uint8)(iFGr16 >> 8);
                *(pScanline+1) = (mng_uint8)(iFGg16 >> 8);
                *(pScanline+2) = (mng_uint8)(iFGb16 >> 8);
                                       /* alpha remains fully opaque !!! */
              }
              else
              {                        /* scale background up */
                iBGr16 = (mng_uint16)(*pScanline    );
                iBGg16 = (mng_uint16)(*(pScanline+1));
                iBGb16 = (mng_uint16)(*(pScanline+2));
                iBGr16 = (mng_uint16)((mng_uint32)iBGr16 << 8) | iBGr16;
                iBGg16 = (mng_uint16)((mng_uint32)iBGg16 << 8) | iBGg16;
                iBGb16 = (mng_uint16)((mng_uint32)iBGb16 << 8) | iBGb16;
                                       /* let's blend */
                MNG_BLEND16 (mng_get_uint16 (pDataline  ),
                             mng_get_uint16 (pDataline+2),
                             mng_get_uint16 (pDataline+4), iFGa16,
                             iBGr16, iBGg16, iBGb16, iBGa16,
                             iCr16,  iCg16,  iCb16,  iCa16)
                                       /* and return the composed values */
                *pScanline     = (mng_uint8)(iCr16 >> 8);
                *(pScanline+1) = (mng_uint8)(iCg16 >> 8);
                *(pScanline+2) = (mng_uint8)(iCb16 >> 8);
                *(pScanline+3) = (mng_uint8)(iCa16 >> 8);
              }
            }
          }

          pScanline += (pData->iColinc << 2);
          pDataline += 8;
        }
      }
      else
      {
        for (iX = pData->iSourcel + pData->iCol; iX < pData->iSourcer; iX += pData->iColinc)
        {
          iFGa8 = *(pDataline+3);      /* get alpha values */
          iBGa8 = *(pScanline+3);

          if (iFGa8)                   /* any opacity at all ? */
          {                            /* fully opaque or background fully transparent ? */
            if ((iFGa8 == 0xFF) || (iBGa8 == 0))
            {                          /* then simply copy the values */
              *pScanline     = *pDataline;
              *(pScanline+1) = *(pDataline+1);
              *(pScanline+2) = *(pDataline+2);
              *(pScanline+3) = *(pDataline+3);
            }
            else
            {
              if (iBGa8 == 0xFF)       /* background fully opaque ? */
              {                        /* do alpha composing */
                MNG_COMPOSE8 (*pScanline,     *pDataline,     iFGa8, *pScanline    )
                MNG_COMPOSE8 (*(pScanline+1), *(pDataline+1), iFGa8, *(pScanline+1))
                MNG_COMPOSE8 (*(pScanline+2), *(pDataline+2), iFGa8, *(pScanline+2))
                                       /* alpha remains fully opaque !!! */
              }
              else
              {                        /* now blend */
                MNG_BLEND8 (*pDataline, *(pDataline+1), *(pDataline+2), iFGa8,
                            *pScanline, *(pScanline+1), *(pScanline+2), iBGa8,
                            iCr8, iCg8, iCb8, iCa8)
                                       /* and return the composed values */
                *pScanline     = iCr8;
                *(pScanline+1) = iCg8;
                *(pScanline+2) = iCb8;
                *(pScanline+3) = iCa8;
              }
            }
          }

          pScanline += (pData->iColinc << 2);
          pDataline += 4;
        }
      }
    }
  }

  check_update_region (pData);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DISPLAY_RGBA8, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode display_argb8 (mng_datap pData)
{
  mng_uint8p pScanline;
  mng_uint8p pDataline;
  mng_int32  iX;
  mng_uint16 iFGa16, iBGa16, iCa16;
  mng_uint8  iFGa8, iBGa8, iCa8;
  mng_uint16 iFGr16, iFGg16, iFGb16;
  mng_uint16 iBGr16, iBGg16, iBGb16;
  mng_uint16 iCr16, iCg16, iCb16;
  mng_uint8  iCr8, iCg8, iCb8;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DISPLAY_ARGB8, MNG_LC_START)
#endif

                                       /* viewable row ? */
  if ((pData->iRow >= pData->iSourcet) && (pData->iRow < pData->iSourceb))
  {                                    /* address destination row */
    pScanline = (mng_uint8p)pData->fGetcanvasline (((mng_handle)pData),
                                                   pData->iRow + pData->iDestt -
                                                   pData->iSourcet);
                                       /* adjust destination row starting-point */
    pScanline = pScanline + (pData->iCol << 2) + (pData->iDestl << 2);
    pDataline = pData->pRGBArow;       /* address source row */

    if (pData->bIsRGBA16)              /* adjust source row starting-point */
      pDataline = pDataline + ((pData->iSourcel / pData->iColinc) << 3);
    else
      pDataline = pDataline + ((pData->iSourcel / pData->iColinc) << 2);

    if (pData->bIsOpaque)              /* forget about transparency ? */
    {
      if (pData->bIsRGBA16)            /* 16-bit input row ? */
      {
        for (iX = pData->iSourcel + pData->iCol; iX < pData->iSourcer; iX += pData->iColinc)
        {                              /* scale down by dropping the LSB */
          *pScanline     = *(pDataline+6);
          *(pScanline+1) = *pDataline;
          *(pScanline+2) = *(pDataline+2);
          *(pScanline+3) = *(pDataline+4);

          pScanline += (pData->iColinc << 2);
          pDataline += 8;
        }
      }
      else
      {
        for (iX = pData->iSourcel + pData->iCol; iX < pData->iSourcer; iX += pData->iColinc)
        {                              /* copy the values */
          *pScanline     = *(pDataline+2);
          *(pScanline+1) = *pDataline;
          *(pScanline+2) = *(pDataline+1);
          *(pScanline+3) = *(pDataline+2);

          pScanline += (pData->iColinc << 2);
          pDataline += 4;
        }
      }
    }
    else
    {
      if (pData->bIsRGBA16)            /* 16-bit input row ? */
      {
        for (iX = pData->iSourcel + pData->iCol; iX < pData->iSourcer; iX += pData->iColinc)
        {                              /* get alpha values */
          iFGa16 = mng_get_uint16 (pDataline+6);
          iBGa16 = (mng_uint16)(*pScanline);
          iBGa16 = (mng_uint16)(iBGa16 << 8) | iBGa16;

          if (iFGa16)                  /* any opacity at all ? */
          {                            /* fully opaque or background fully transparent ? */
            if ((iFGa16 == 0xFFFF) || (iBGa16 == 0))
            {                          /* plain copy it */
              *pScanline     = *(pDataline+6);
              *(pScanline+1) = *pDataline;
              *(pScanline+2) = *(pDataline+2);
              *(pScanline+3) = *(pDataline+4);
            }
            else
            {
              if (iBGa16 == 0xFFFF)    /* background fully opaque ? */
              {                        /* get the proper values */
                iFGr16 = mng_get_uint16 (pDataline  );
                iFGg16 = mng_get_uint16 (pDataline+2);
                iFGb16 = mng_get_uint16 (pDataline+4);
                                       /* scale background up */
                iBGr16 = (mng_uint16)(*(pScanline+1));
                iBGg16 = (mng_uint16)(*(pScanline+2));
                iBGb16 = (mng_uint16)(*(pScanline+3));
                iBGr16 = (mng_uint16)((mng_uint32)iBGr16 << 8) | iBGr16;
                iBGg16 = (mng_uint16)((mng_uint32)iBGg16 << 8) | iBGg16;
                iBGb16 = (mng_uint16)((mng_uint32)iBGb16 << 8) | iBGb16;
                                       /* now compose */
                MNG_COMPOSE16(iFGr16, iFGr16, iFGa16, iBGr16)
                MNG_COMPOSE16(iFGg16, iFGg16, iFGa16, iBGg16)
                MNG_COMPOSE16(iFGb16, iFGb16, iFGa16, iBGb16)
                                       /* and return the composed values */
                                       /* alpha remains fully opaque !!! */
                *(pScanline+1) = (mng_uint8)(iFGr16 >> 8);
                *(pScanline+2) = (mng_uint8)(iFGg16 >> 8);
                *(pScanline+3) = (mng_uint8)(iFGb16 >> 8);
              }
              else
              {                        /* scale background up */
                iBGr16 = (mng_uint16)(*(pScanline+1));
                iBGg16 = (mng_uint16)(*(pScanline+2));
                iBGb16 = (mng_uint16)(*(pScanline+3));
                iBGr16 = (mng_uint16)((mng_uint32)iBGr16 << 8) | iBGr16;
                iBGg16 = (mng_uint16)((mng_uint32)iBGg16 << 8) | iBGg16;
                iBGb16 = (mng_uint16)((mng_uint32)iBGb16 << 8) | iBGb16;
                                       /* let's blend */
                MNG_BLEND16 (mng_get_uint16 (pDataline  ),
                             mng_get_uint16 (pDataline+2),
                             mng_get_uint16 (pDataline+4), iFGa16,
                             iBGr16, iBGg16, iBGb16, iBGa16,
                             iCr16,  iCg16,  iCb16,  iCa16)
                                       /* and return the composed values */
                *pScanline     = (mng_uint8)(iCa16 >> 8);
                *(pScanline+1) = (mng_uint8)(iCr16 >> 8);
                *(pScanline+2) = (mng_uint8)(iCg16 >> 8);
                *(pScanline+3) = (mng_uint8)(iCb16 >> 8);
              }
            }
          }

          pScanline += (pData->iColinc << 2);
          pDataline += 8;
        }
      }
      else
      {
        for (iX = pData->iSourcel + pData->iCol; iX < pData->iSourcer; iX += pData->iColinc)
        {
          iFGa8 = *(pDataline+3);      /* get alpha values */
          iBGa8 = *pScanline;

          if (iFGa8)                   /* any opacity at all ? */
          {                            /* fully opaque or background fully transparent ? */
            if ((iFGa8 == 0xFF) || (iBGa8 == 0))
            {                          /* then simply copy the values */
              *pScanline     = *(pDataline+3);
              *(pScanline+1) = *pDataline;
              *(pScanline+2) = *(pDataline+1);
              *(pScanline+3) = *(pDataline+2);
            }
            else
            {
              if (iBGa8 == 0xFF)       /* background fully opaque ? */
              {                        /* do simple alpha composing */
                                       /* alpha itself remains fully opaque !!! */
                MNG_COMPOSE8 (*(pScanline+1), *pDataline,     iFGa8, *(pScanline+1))
                MNG_COMPOSE8 (*(pScanline+2), *(pDataline+1), iFGa8, *(pScanline+2))
                MNG_COMPOSE8 (*(pScanline+3), *(pDataline+2), iFGa8, *(pScanline+3))
              }
              else
              {                        /* now blend */
                MNG_BLEND8 (*pDataline,     *(pDataline+1), *(pDataline+2), iFGa8,
                            *(pScanline+1), *(pScanline+2), *(pScanline+3), iBGa8,
                            iCr8, iCg8, iCb8, iCa8)
                                       /* and return the composed values */
                *pScanline     = iCa8;
                *(pScanline+1) = iCr8;
                *(pScanline+2) = iCg8;
                *(pScanline+3) = iCb8;
              }
            }
          }

          pScanline += (pData->iColinc << 2);
          pDataline += 4;
        }
      }
    }
  }

  check_update_region (pData);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DISPLAY_ARGB8, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode display_rgb8_a8 (mng_datap pData)
{
  mng_uint8p pScanline;
  mng_uint8p pAlphaline;
  mng_uint8p pDataline;
  mng_int32  iX;
  mng_uint16 iFGa16, iBGa16, iCa16;
  mng_uint8  iFGa8, iBGa8, iCa8;
  mng_uint16 iFGr16, iFGg16, iFGb16;
  mng_uint16 iBGr16, iBGg16, iBGb16;
  mng_uint16 iCr16, iCg16, iCb16;
  mng_uint8  iCr8, iCg8, iCb8;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DISPLAY_RGB8_A8, MNG_LC_START)
#endif
                                       /* viewable row ? */
  if ((pData->iRow >= pData->iSourcet) && (pData->iRow < pData->iSourceb))
  {                                    /* address destination rows */
    pScanline  = (mng_uint8p)pData->fGetcanvasline (((mng_handle)pData),
                                                    pData->iRow + pData->iDestt -
                                                    pData->iSourcet);
    pAlphaline = (mng_uint8p)pData->fGetalphaline  (((mng_handle)pData),
                                                    pData->iRow + pData->iDestt -
                                                    pData->iSourcet);
                                       /* adjust destination rows starting-point */
    pScanline  = pScanline  + (pData->iCol * 3) + (pData->iDestl * 3);
    pAlphaline = pAlphaline + pData->iCol + pData->iDestl;

    pDataline  = pData->pRGBArow;      /* address source row */

    if (pData->bIsRGBA16)              /* adjust source row starting-point */
      pDataline = pDataline + ((pData->iSourcel / pData->iColinc) << 3);
    else
      pDataline = pDataline + ((pData->iSourcel / pData->iColinc) << 2);

    if (pData->bIsOpaque)              /* forget about transparency ? */
    {
      if (pData->bIsRGBA16)            /* 16-bit input row ? */
      {
        for (iX = pData->iSourcel + pData->iCol; iX < pData->iSourcer; iX += pData->iColinc)
        {                              /* scale down by dropping the LSB */
          *pScanline     = *pDataline;
          *(pScanline+1) = *(pDataline+2);
          *(pScanline+2) = *(pDataline+4);
          *pAlphaline    = *(pDataline+6);

          pScanline  += (pData->iColinc * 3);
          pAlphaline += pData->iColinc;
          pDataline  += 8;
        }
      }
      else
      {
        for (iX = pData->iSourcel + pData->iCol; iX < pData->iSourcer; iX += pData->iColinc)
        {                              /* copy the values */
          *pScanline     = *pDataline;
          *(pScanline+1) = *(pDataline+1);
          *(pScanline+2) = *(pDataline+2);
          *pAlphaline    = *(pDataline+3);

          pScanline  += (pData->iColinc * 3);
          pAlphaline += pData->iColinc;
          pDataline  += 4;
        }
      }
    }
    else
    {
      if (pData->bIsRGBA16)            /* 16-bit input row ? */
      {
        for (iX = pData->iSourcel + pData->iCol; iX < pData->iSourcer; iX += pData->iColinc)
        {                              /* get alpha values */
          iFGa16 = mng_get_uint16 (pDataline+6);
          iBGa16 = (mng_uint16)(*pAlphaline);
          iBGa16 = (mng_uint16)(iBGa16 << 8) | iBGa16;

          if (iFGa16)                  /* any opacity at all ? */
          {                            /* fully opaque or background fully transparent ? */
            if ((iFGa16 == 0xFFFF) || (iBGa16 == 0))
            {                          /* plain copy it */
              *pScanline     = *pDataline;
              *(pScanline+1) = *(pDataline+2);
              *(pScanline+2) = *(pDataline+4);
              *pAlphaline    = *(pDataline+6);
            }
            else
            {
              if (iBGa16 == 0xFFFF)    /* background fully opaque ? */
              {                        /* get the proper values */
                iFGr16 = mng_get_uint16 (pDataline  );
                iFGg16 = mng_get_uint16 (pDataline+2);
                iFGb16 = mng_get_uint16 (pDataline+4);
                                       /* scale background up */
                iBGr16 = (mng_uint16)(*pScanline    );
                iBGg16 = (mng_uint16)(*(pScanline+1));
                iBGb16 = (mng_uint16)(*(pScanline+2));
                iBGr16 = (mng_uint16)((mng_uint32)iBGr16 << 8) | iBGr16;
                iBGg16 = (mng_uint16)((mng_uint32)iBGg16 << 8) | iBGg16;
                iBGb16 = (mng_uint16)((mng_uint32)iBGb16 << 8) | iBGb16;
                                       /* now compose */
                MNG_COMPOSE16(iFGr16, iFGr16, iFGa16, iBGr16)
                MNG_COMPOSE16(iFGg16, iFGg16, iFGa16, iBGg16)
                MNG_COMPOSE16(iFGb16, iFGb16, iFGa16, iBGb16)
                                       /* and return the composed values */
                *pScanline     = (mng_uint8)(iFGr16 >> 8);
                *(pScanline+1) = (mng_uint8)(iFGg16 >> 8);
                *(pScanline+2) = (mng_uint8)(iFGb16 >> 8);
                                       /* alpha remains fully opaque !!! */
              }
              else
              {                        /* scale background up */
                iBGr16 = (mng_uint16)(*pScanline    );
                iBGg16 = (mng_uint16)(*(pScanline+1));
                iBGb16 = (mng_uint16)(*(pScanline+2));
                iBGr16 = (mng_uint16)((mng_uint32)iBGr16 << 8) | iBGr16;
                iBGg16 = (mng_uint16)((mng_uint32)iBGg16 << 8) | iBGg16;
                iBGb16 = (mng_uint16)((mng_uint32)iBGb16 << 8) | iBGb16;
                                       /* let's blend */
                MNG_BLEND16 (mng_get_uint16 (pDataline  ),
                             mng_get_uint16 (pDataline+2),
                             mng_get_uint16 (pDataline+4), iFGa16,
                             iBGr16, iBGg16, iBGb16, iBGa16,
                             iCr16,  iCg16,  iCb16,  iCa16)
                                       /* and return the composed values */
                *pScanline     = (mng_uint8)(iCr16 >> 8);
                *(pScanline+1) = (mng_uint8)(iCg16 >> 8);
                *(pScanline+2) = (mng_uint8)(iCb16 >> 8);
                *pAlphaline    = (mng_uint8)(iCa16 >> 8);
              }
            }
          }

          pScanline  += (pData->iColinc * 3);
          pAlphaline += pData->iColinc;
          pDataline  += 8;
        }
      }
      else
      {
        for (iX = pData->iSourcel + pData->iCol; iX < pData->iSourcer; iX += pData->iColinc)
        {
          iFGa8 = *(pDataline+3);      /* get alpha values */
          iBGa8 = *(pScanline+3);

          if (iFGa8)                   /* any opacity at all ? */
          {                            /* fully opaque or background fully transparent ? */
            if ((iFGa8 == 0xFF) || (iBGa8 == 0))
            {                          /* then simply copy the values */
              *pScanline     = *pDataline;
              *(pScanline+1) = *(pDataline+1);
              *(pScanline+2) = *(pDataline+2);
              *pAlphaline    = *(pDataline+3);
            }
            else
            {
              if (iBGa8 == 0xFF)       /* background fully opaque ? */
              {                        /* do alpha composing */
                MNG_COMPOSE8 (*pScanline,     *pDataline,     iFGa8, *pScanline    )
                MNG_COMPOSE8 (*(pScanline+1), *(pDataline+1), iFGa8, *(pScanline+1))
                MNG_COMPOSE8 (*(pScanline+2), *(pDataline+2), iFGa8, *(pScanline+2))
                                       /* alpha remains fully opaque !!! */
              }
              else
              {                        /* now blend */
                MNG_BLEND8 (*pDataline, *(pDataline+1), *(pDataline+2), iFGa8,
                            *pScanline, *(pScanline+1), *(pScanline+2), iBGa8,
                            iCr8, iCg8, iCb8, iCa8)
                                       /* and return the composed values */
                *pScanline     = iCr8;
                *(pScanline+1) = iCg8;
                *(pScanline+2) = iCb8;
                *pAlphaline    = iCa8;
              }
            }
          }

          pScanline  += (pData->iColinc * 3);
          pAlphaline += pData->iColinc;
          pDataline  += 4;
        }
      }
    }
  }

  check_update_region (pData);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DISPLAY_RGB8_A8, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode display_bgr8 (mng_datap pData)
{
  mng_uint8p pScanline;
  mng_uint8p pDataline;
  mng_int32  iX;
  mng_uint16 iA16;
  mng_uint16 iFGr16, iFGg16, iFGb16;
  mng_uint16 iBGr16, iBGg16, iBGb16;
  mng_uint8  iA8;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DISPLAY_BGR8, MNG_LC_START)
#endif
                                       /* viewable row ? */
  if ((pData->iRow >= pData->iSourcet) && (pData->iRow < pData->iSourceb))
  {                                    /* address destination row */
    pScanline = (mng_uint8p)pData->fGetcanvasline (((mng_handle)pData),
                                                   pData->iRow + pData->iDestt -
                                                   pData->iSourcet);
                                       /* adjust destination row starting-point */
    pScanline = pScanline + (pData->iCol * 3) + (pData->iDestl * 3);
    pDataline = pData->pRGBArow;       /* address source row */

    if (pData->bIsRGBA16)              /* adjust source row starting-point */
      pDataline = pDataline + (pData->iSourcel / pData->iColinc) * 8;
    else
      pDataline = pDataline + (pData->iSourcel / pData->iColinc) * 4;

    if (pData->bIsOpaque)              /* forget about transparency ? */
    {
      if (pData->bIsRGBA16)            /* 16-bit input row ? */
      {
        for (iX = pData->iSourcel + pData->iCol; iX < pData->iSourcer; iX += pData->iColinc)
        {                              /* scale down by dropping the LSB */
          *pScanline     = *(pDataline+4);
          *(pScanline+1) = *(pDataline+2);
          *(pScanline+2) = *pDataline;

          pScanline += (pData->iColinc * 3);
          pDataline += 8;
        }
      }
      else
      {
        for (iX = pData->iSourcel + pData->iCol; iX < pData->iSourcer; iX += pData->iColinc)
        {                              /* copy the values */
          *pScanline     = *(pDataline+2);
          *(pScanline+1) = *(pDataline+1);
          *(pScanline+2) = *pDataline;

          pScanline += (pData->iColinc * 3);
          pDataline += 4;
        }
      }
    }
    else
    {
      if (pData->bIsRGBA16)            /* 16-bit input row ? */
      {
        for (iX = pData->iSourcel + pData->iCol; iX < pData->iSourcer; iX += pData->iColinc)
        {                              /* get alpha value */
          iA16 = mng_get_uint16 (pDataline+6);

          if (iA16)                    /* any opacity at all ? */
          {
            if (iA16 == 0xFFFF)        /* fully opaque ? */
            {                          /* scale down by dropping the LSB */
              *pScanline     = *(pDataline+4);
              *(pScanline+1) = *(pDataline+2);
              *(pScanline+2) = *pDataline;
            }
            else
            {                          /* get the proper values */
              iFGr16 = mng_get_uint16 (pDataline  );
              iFGg16 = mng_get_uint16 (pDataline+2);
              iFGb16 = mng_get_uint16 (pDataline+4);
                                       /* scale background up */
              iBGr16 = (mng_uint16)(*(pScanline+2));
              iBGg16 = (mng_uint16)(*(pScanline+1));
              iBGb16 = (mng_uint16)(*pScanline    );
              iBGr16 = (mng_uint16)((mng_uint32)iBGr16 << 8) | iBGr16;
              iBGg16 = (mng_uint16)((mng_uint32)iBGg16 << 8) | iBGg16;
              iBGb16 = (mng_uint16)((mng_uint32)iBGb16 << 8) | iBGb16;
                                       /* now compose */
              MNG_COMPOSE16(iFGr16, iFGr16, iA16, iBGr16)
              MNG_COMPOSE16(iFGg16, iFGg16, iA16, iBGg16)
              MNG_COMPOSE16(iFGb16, iFGb16, iA16, iBGb16)
                                       /* and return the composed values */
              *pScanline     = (mng_uint8)(iFGb16 >> 8);
              *(pScanline+1) = (mng_uint8)(iFGg16 >> 8);
              *(pScanline+2) = (mng_uint8)(iFGr16 >> 8);
            }
          }

          pScanline += (pData->iColinc * 3);
          pDataline += 8;
        }
      }
      else
      {
        for (iX = pData->iSourcel + pData->iCol; iX < pData->iSourcer; iX += pData->iColinc)
        {
          iA8 = *(pDataline+3);        /* get alpha value */

          if (iA8)                     /* any opacity at all ? */
          {
            if (iA8 == 0xFF)           /* fully opaque ? */
            {                          /* then simply copy the values */
              *pScanline     = *(pDataline+2);
              *(pScanline+1) = *(pDataline+1);
              *(pScanline+2) = *pDataline;
            }
            else
            {                          /* do alpha composing */
              MNG_COMPOSE8 (*pScanline,     *(pDataline+2), iA8, *pScanline    )
              MNG_COMPOSE8 (*(pScanline+1), *(pDataline+1), iA8, *(pScanline+1))
              MNG_COMPOSE8 (*(pScanline+2), *pDataline,     iA8, *(pScanline+2))
            }
          }

          pScanline += (pData->iColinc * 3);
          pDataline += 4;
        }
      }
    }
  }

  check_update_region (pData);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DISPLAY_BGR8, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode display_bgra8 (mng_datap pData)
{
  mng_uint8p pScanline;
  mng_uint8p pDataline;
  mng_int32  iX;
  mng_uint16 iFGa16, iBGa16, iCa16;
  mng_uint8  iFGa8, iBGa8, iCa8;
  mng_uint16 iFGr16, iFGg16, iFGb16;
  mng_uint16 iBGr16, iBGg16, iBGb16;
  mng_uint16 iCr16, iCg16, iCb16;
  mng_uint8  iCr8, iCg8, iCb8;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DISPLAY_BGRA8, MNG_LC_START)
#endif
                                       /* viewable row ? */
  if ((pData->iRow >= pData->iSourcet) && (pData->iRow < pData->iSourceb))
  {                                    /* address destination row */
    pScanline = (mng_uint8p)pData->fGetcanvasline (((mng_handle)pData),
                                                   pData->iRow + pData->iDestt -
                                                   pData->iSourcet);
                                       /* adjust destination row starting-point */
    pScanline = pScanline + (pData->iCol << 2) + (pData->iDestl << 2);
    pDataline = pData->pRGBArow;       /* address source row */

    if (pData->bIsRGBA16)              /* adjust source row starting-point */
      pDataline = pDataline + ((pData->iSourcel / pData->iColinc) << 3);
    else
      pDataline = pDataline + ((pData->iSourcel / pData->iColinc) << 2);

    if (pData->bIsOpaque)              /* forget about transparency ? */
    {
      if (pData->bIsRGBA16)            /* 16-bit input row ? */
      {
        for (iX = pData->iSourcel + pData->iCol; iX < pData->iSourcer; iX += pData->iColinc)
        {                              /* scale down by dropping the LSB */
          *pScanline     = *(pDataline+4);
          *(pScanline+1) = *(pDataline+2);
          *(pScanline+2) = *pDataline;
          *(pScanline+3) = *(pDataline+6);

          pScanline += (pData->iColinc << 2);
          pDataline += 8;
        }
      }
      else
      {
        for (iX = pData->iSourcel + pData->iCol; iX < pData->iSourcer; iX += pData->iColinc)
        {                              /* copy the values */
          *pScanline     = *(pDataline+2);
          *(pScanline+1) = *(pDataline+1);
          *(pScanline+2) = *pDataline;
          *(pScanline+3) = *(pDataline+3);

          pScanline += (pData->iColinc << 2);
          pDataline += 4;
        }
      }
    }
    else
    {
      if (pData->bIsRGBA16)            /* 16-bit input row ? */
      {
        for (iX = pData->iSourcel + pData->iCol; iX < pData->iSourcer; iX += pData->iColinc)
        {                              /* get alpha values */
          iFGa16 = mng_get_uint16 (pDataline+6);
          iBGa16 = (mng_uint16)(*(pScanline+3));
          iBGa16 = (mng_uint16)(iBGa16 << 8) | iBGa16;

          if (iFGa16)                  /* any opacity at all ? */
          {                            /* fully opaque or background fully transparent ? */
            if ((iFGa16 == 0xFFFF) || (iBGa16 == 0))
            {                          /* plain copy it */
              *pScanline     = *(pDataline+4);
              *(pScanline+1) = *(pDataline+2);
              *(pScanline+2) = *pDataline;
              *(pScanline+3) = *(pDataline+6);
            }
            else
            {
              if (iBGa16 == 0xFFFF)    /* background fully opaque ? */
              {                        /* get the proper values */
                iFGr16 = mng_get_uint16 (pDataline  );
                iFGg16 = mng_get_uint16 (pDataline+2);
                iFGb16 = mng_get_uint16 (pDataline+4);
                                       /* scale background up */
                iBGr16 = (mng_uint16)(*(pScanline+2));
                iBGg16 = (mng_uint16)(*(pScanline+1));
                iBGb16 = (mng_uint16)(*pScanline    );
                iBGr16 = (mng_uint16)((mng_uint32)iBGr16 << 8) | iBGr16;
                iBGg16 = (mng_uint16)((mng_uint32)iBGg16 << 8) | iBGg16;
                iBGb16 = (mng_uint16)((mng_uint32)iBGb16 << 8) | iBGb16;
                                       /* now compose */
                MNG_COMPOSE16(iFGr16, iFGr16, iFGa16, iBGr16)
                MNG_COMPOSE16(iFGg16, iFGg16, iFGa16, iBGg16)
                MNG_COMPOSE16(iFGb16, iFGb16, iFGa16, iBGb16)
                                       /* and return the composed values */
                *pScanline     = (mng_uint8)(iFGb16 >> 8);
                *(pScanline+1) = (mng_uint8)(iFGg16 >> 8);
                *(pScanline+2) = (mng_uint8)(iFGr16 >> 8);
                                       /* alpha remains fully opaque !!! */
              }
              else
              {                        /* scale background up */
                iBGr16 = (mng_uint16)(*(pScanline+2));
                iBGg16 = (mng_uint16)(*(pScanline+1));
                iBGb16 = (mng_uint16)(*pScanline    );
                iBGr16 = (mng_uint16)((mng_uint32)iBGr16 << 8) | iBGr16;
                iBGg16 = (mng_uint16)((mng_uint32)iBGg16 << 8) | iBGg16;
                iBGb16 = (mng_uint16)((mng_uint32)iBGb16 << 8) | iBGb16;
                                       /* let's blend */
                MNG_BLEND16 (mng_get_uint16 (pDataline  ),
                             mng_get_uint16 (pDataline+2),
                             mng_get_uint16 (pDataline+4), iFGa16,
                             iBGr16, iBGg16, iBGb16, iBGa16,
                             iCr16,  iCg16,  iCb16,  iCa16)
                                       /* and return the composed values */
                *pScanline     = (mng_uint8)(iCb16 >> 8);
                *(pScanline+1) = (mng_uint8)(iCg16 >> 8);
                *(pScanline+2) = (mng_uint8)(iCr16 >> 8);
                *(pScanline+3) = (mng_uint8)(iCa16 >> 8);
              }
            }
          }

          pScanline += (pData->iColinc << 2);
          pDataline += 8;
        }
      }
      else
      {
        for (iX = pData->iSourcel + pData->iCol; iX < pData->iSourcer; iX += pData->iColinc)
        {
          iFGa8 = *(pDataline+3);      /* get alpha values */
          iBGa8 = *(pScanline+3);

          if (iFGa8)                   /* any opacity at all ? */
          {                            /* fully opaque or background fully transparent ? */
            if ((iFGa8 == 0xFF) || (iBGa8 == 0))
            {                          /* then simply copy the values */
              *pScanline     = *(pDataline+2);
              *(pScanline+1) = *(pDataline+1);
              *(pScanline+2) = *pDataline;
              *(pScanline+3) = *(pDataline+3);
            }
            else
            {
              if (iBGa8 == 0xFF)       /* background fully opaque ? */
              {                        /* do alpha composing */
                MNG_COMPOSE8 (*pScanline,     *(pDataline+2), iFGa8, *pScanline    )
                MNG_COMPOSE8 (*(pScanline+1), *(pDataline+1), iFGa8, *(pScanline+1))
                MNG_COMPOSE8 (*(pScanline+2), *pDataline,     iFGa8, *(pScanline+2))
                                       /* alpha remains fully opaque !!! */
              }
              else
              {                        /* now blend */
                MNG_BLEND8 (*pDataline,     *(pDataline+1), *(pDataline+2), iFGa8,
                            *(pScanline+2), *(pScanline+1), *pScanline,     iBGa8,
                            iCr8, iCg8, iCb8, iCa8)
                                       /* and return the composed values */
                *pScanline     = iCb8;
                *(pScanline+1) = iCg8;
                *(pScanline+2) = iCr8;
                *(pScanline+3) = iCa8;
              }
            }
          }

          pScanline += (pData->iColinc << 2);
          pDataline += 4;
        }
      }
    }
  }

  check_update_region (pData);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DISPLAY_BGRA8, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode display_bgra8_pm (mng_datap pData)
{
  mng_uint8p pScanline;
  mng_uint8p pDataline;
  mng_int32  iX;
  mng_uint32 s, t;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DISPLAY_BGRA8PM, MNG_LC_START)
#endif
                                       /* viewable row ? */
  if ((pData->iRow >= pData->iSourcet) && (pData->iRow < pData->iSourceb))
  {                                    /* address destination row */
    pScanline = (mng_uint8p)pData->fGetcanvasline (((mng_handle)pData),
                                                   pData->iRow + pData->iDestt -
                                                   pData->iSourcet);
                                       /* adjust destination row starting-point */
    pScanline = pScanline + (pData->iCol << 2) + (pData->iDestl << 2);
    pDataline = pData->pRGBArow;       /* address source row */

    if (pData->bIsRGBA16)              /* adjust source row starting-point */
      pDataline = pDataline + ((pData->iSourcel / pData->iColinc) << 3);
    else
      pDataline = pDataline + ((pData->iSourcel / pData->iColinc) << 2);

    if (pData->bIsOpaque)              /* forget about transparency ? */
    {
      if (pData->bIsRGBA16)            /* 16-bit input row ? */
      {
        for (iX = pData->iSourcel + pData->iCol; iX < pData->iSourcer; iX += pData->iColinc)
        {                              /* scale down by dropping the LSB */
		  if ((s = pDataline[6]) == 0)
			*(mng_uint32*) pScanline = 0; /* set all components = 0 */
		  else
		  {
			if (s == 255)
			{
		      pScanline[0] = pDataline[4];
              pScanline[1] = pDataline[2];
              pScanline[2] = pDataline[0];
              pScanline[3] = 255;
			}
			else
			{
              pScanline[0] = DIV255B8(s * pDataline[4]);
              pScanline[1] = DIV255B8(s * pDataline[2]);
              pScanline[2] = DIV255B8(s * pDataline[0]);
              pScanline[3] = (mng_uint8)s;
			}
		  }
          pScanline += (pData->iColinc << 2);
          pDataline += 8;
        }
      }
      else
      {
        for (iX = pData->iSourcel + pData->iCol; iX < pData->iSourcer; iX += pData->iColinc)
        {                              /* copy the values and premultiply */
		  if ((s = pDataline[3]) == 0)
			*(mng_uint32*) pScanline = 0; /* set all components = 0 */
		  else
		  {
			if (s == 255)
			{
		      pScanline[0] = pDataline[2];
              pScanline[1] = pDataline[1];
              pScanline[2] = pDataline[0];
              pScanline[3] = 255;
			}
			else
			{
		      pScanline[0] = DIV255B8(s * pDataline[2]);
              pScanline[1] = DIV255B8(s * pDataline[1]);
              pScanline[2] = DIV255B8(s * pDataline[0]);
              pScanline[3] = (mng_uint8)s;
			}
		  }

          pScanline += (pData->iColinc << 2);
          pDataline += 4;
        }
      }
    }
    else
    {
      if (pData->bIsRGBA16)            /* 16-bit input row ? */
      {
        for (iX = pData->iSourcel + pData->iCol; iX < pData->iSourcer; iX += pData->iColinc)
        {                              /* get alpha values */
          if ((s = pDataline[6]) != 0)       /* any opacity at all ? */
          {                            /* fully opaque or background fully transparent ? */
            if (s == 255)
            {                          /* plain copy it */
              pScanline[0] = pDataline[4];
              pScanline[1] = pDataline[2];
              pScanline[2] = pDataline[0];
              pScanline[3] = 255;
            }
            else
            {                          /* now blend (premultiplied) */
			  t = 255 - s;
			  pScanline[0] = DIV255B8(s * pDataline[4] + t * pScanline[0]);
              pScanline[1] = DIV255B8(s * pDataline[2] + t * pScanline[1]);
              pScanline[2] = DIV255B8(s * pDataline[0] + t * pScanline[2]);
              pScanline[3] = (mng_uint8)(255 - DIV255B8(t * (255 - pScanline[3])));
            }
          }

          pScanline += (pData->iColinc << 2);
          pDataline += 8;
        }
      }
      else
      {
        for (iX = pData->iSourcel + pData->iCol; iX < pData->iSourcer; iX += pData->iColinc)
        {
          if ((s = pDataline[3]) != 0)       /* any opacity at all ? */
          {                            /* fully opaque ? */
            if (s == 255)
            {                          /* then simply copy the values */
              pScanline[0] = pDataline[2];
              pScanline[1] = pDataline[1];
              pScanline[2] = pDataline[0];
              pScanline[3] = 255;
            }
            else
            {                          /* now blend (premultiplied) */
			  t = 255 - s;
			  pScanline[0] = DIV255B8(s * pDataline[2] + t * pScanline[0]);
              pScanline[1] = DIV255B8(s * pDataline[1] + t * pScanline[1]);
              pScanline[2] = DIV255B8(s * pDataline[0] + t * pScanline[2]);
              pScanline[3] = (mng_uint8)(255 - DIV255B8(t * (255 - pScanline[3])));
            }
          }

          pScanline += (pData->iColinc << 2);
          pDataline += 4;
        }
      }
    }
  }

  check_update_region (pData);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DISPLAY_BGRA8PM, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode display_abgr8 (mng_datap pData)
{
  mng_uint8p pScanline;
  mng_uint8p pDataline;
  mng_int32  iX;
  mng_uint16 iFGa16, iBGa16, iCa16;
  mng_uint8  iFGa8, iBGa8, iCa8;
  mng_uint16 iFGr16, iFGg16, iFGb16;
  mng_uint16 iBGr16, iBGg16, iBGb16;
  mng_uint16 iCr16, iCg16, iCb16;
  mng_uint8  iCr8, iCg8, iCb8;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DISPLAY_ABGR8, MNG_LC_START)
#endif
                                       /* viewable row ? */
  if ((pData->iRow >= pData->iSourcet) && (pData->iRow < pData->iSourceb))
  {                                    /* address destination row */
    pScanline = (mng_uint8p)pData->fGetcanvasline (((mng_handle)pData),
                                                   pData->iRow + pData->iDestt -
                                                   pData->iSourcet);
                                       /* adjust destination row starting-point */
    pScanline = pScanline + (pData->iCol << 2) + (pData->iDestl << 2);
    pDataline = pData->pRGBArow;       /* address source row */

    if (pData->bIsRGBA16)              /* adjust source row starting-point */
      pDataline = pDataline + ((pData->iSourcel / pData->iColinc) << 3);
    else
      pDataline = pDataline + ((pData->iSourcel / pData->iColinc) << 2);

    if (pData->bIsOpaque)              /* forget about transparency ? */
    {
      if (pData->bIsRGBA16)            /* 16-bit input row ? */
      {
        for (iX = pData->iSourcel + pData->iCol; iX < pData->iSourcer; iX += pData->iColinc)
        {                              /* scale down by dropping the LSB */
          *pScanline     = *(pDataline+6);
          *(pScanline+1) = *(pDataline+4);
          *(pScanline+2) = *(pDataline+2);
          *(pScanline+3) = *pDataline;

          pScanline += (pData->iColinc << 2);
          pDataline += 8;
        }
      }
      else
      {
        for (iX = pData->iSourcel + pData->iCol; iX < pData->iSourcer; iX += pData->iColinc)
        {                              /* copy the values */
          *pScanline     = *(pDataline+3);
          *(pScanline+1) = *(pDataline+2);
          *(pScanline+2) = *(pDataline+1);
          *(pScanline+3) = *pDataline;

          pScanline += (pData->iColinc << 2);
          pDataline += 4;
        }
      }
    }
    else
    {
      if (pData->bIsRGBA16)            /* 16-bit input row ? */
      {
        for (iX = pData->iSourcel + pData->iCol; iX < pData->iSourcer; iX += pData->iColinc)
        {                              /* get alpha values */
          iFGa16 = mng_get_uint16 (pDataline+6);
          iBGa16 = (mng_uint16)(*pScanline);
          iBGa16 = (mng_uint16)(iBGa16 << 8) | iBGa16;

          if (iFGa16)                  /* any opacity at all ? */
          {                            /* fully opaque or background fully transparent ? */
            if ((iFGa16 == 0xFFFF) || (iBGa16 == 0))
            {                          /* plain copy it */
              *pScanline     = *(pDataline+6);
              *(pScanline+1) = *(pDataline+4);
              *(pScanline+2) = *(pDataline+2);
              *(pScanline+3) = *pDataline;
            }
            else
            {
              if (iBGa16 == 0xFFFF)    /* background fully opaque ? */
              {                        /* get the proper values */
                iFGr16 = mng_get_uint16 (pDataline  );
                iFGg16 = mng_get_uint16 (pDataline+2);
                iFGb16 = mng_get_uint16 (pDataline+4);
                                       /* scale background up */
                iBGr16 = (mng_uint16)(*(pScanline+3));
                iBGg16 = (mng_uint16)(*(pScanline+2));
                iBGb16 = (mng_uint16)(*(pScanline+1));
                iBGr16 = (mng_uint16)((mng_uint32)iBGr16 << 8) | iBGr16;
                iBGg16 = (mng_uint16)((mng_uint32)iBGg16 << 8) | iBGg16;
                iBGb16 = (mng_uint16)((mng_uint32)iBGb16 << 8) | iBGb16;
                                       /* now compose */
                MNG_COMPOSE16(iFGr16, iFGr16, iFGa16, iBGr16)
                MNG_COMPOSE16(iFGg16, iFGg16, iFGa16, iBGg16)
                MNG_COMPOSE16(iFGb16, iFGb16, iFGa16, iBGb16)
                                       /* and return the composed values */
                                       /* alpha itself remains fully opaque !!! */
                *(pScanline+1) = (mng_uint8)(iFGb16 >> 8);
                *(pScanline+2) = (mng_uint8)(iFGg16 >> 8);
                *(pScanline+3) = (mng_uint8)(iFGr16 >> 8);
              }
              else
              {                        /* scale background up */
                iBGr16 = (mng_uint16)(*(pScanline+3));
                iBGg16 = (mng_uint16)(*(pScanline+2));
                iBGb16 = (mng_uint16)(*(pScanline+1));
                iBGr16 = (mng_uint16)((mng_uint32)iBGr16 << 8) | iBGr16;
                iBGg16 = (mng_uint16)((mng_uint32)iBGg16 << 8) | iBGg16;
                iBGb16 = (mng_uint16)((mng_uint32)iBGb16 << 8) | iBGb16;
                                       /* let's blend */
                MNG_BLEND16 (mng_get_uint16 (pDataline  ),
                             mng_get_uint16 (pDataline+2),
                             mng_get_uint16 (pDataline+4), iFGa16,
                             iBGr16, iBGg16, iBGb16, iBGa16,
                             iCr16,  iCg16,  iCb16,  iCa16)
                                       /* and return the composed values */
                *pScanline     = (mng_uint8)(iCa16 >> 8);
                *(pScanline+1) = (mng_uint8)(iCb16 >> 8);
                *(pScanline+2) = (mng_uint8)(iCg16 >> 8);
                *(pScanline+3) = (mng_uint8)(iCr16 >> 8);
              }
            }
          }

          pScanline += (pData->iColinc << 2);
          pDataline += 8;
        }
      }
      else
      {
        for (iX = pData->iSourcel + pData->iCol; iX < pData->iSourcer; iX += pData->iColinc)
        {
          iFGa8 = *(pDataline+3);      /* get alpha values */
          iBGa8 = *pScanline;

          if (iFGa8)                   /* any opacity at all ? */
          {                            /* fully opaque or background fully transparent ? */
            if ((iFGa8 == 0xFF) || (iBGa8 == 0))
            {                          /* then simply copy the values */
              *pScanline     = *(pDataline+3);
              *(pScanline+1) = *(pDataline+2);
              *(pScanline+2) = *(pDataline+1);
              *(pScanline+3) = *pDataline;
            }
            else
            {
              if (iBGa8 == 0xFF)       /* background fully opaque ? */
              {                        /* do simple alpha composing */
                                       /* alpha itself remains fully opaque !!! */
                MNG_COMPOSE8 (*(pScanline+1), *(pDataline+2), iFGa8, *(pScanline+1))
                MNG_COMPOSE8 (*(pScanline+2), *(pDataline+1), iFGa8, *(pScanline+2))
                MNG_COMPOSE8 (*(pScanline+3), *pDataline,     iFGa8, *(pScanline+3))
              }
              else
              {                        /* now blend */
                MNG_BLEND8 (*pDataline,     *(pDataline+1), *(pDataline+2), iFGa8,
                            *(pScanline+3), *(pScanline+2), *(pScanline+1), iBGa8,
                            iCr8, iCg8, iCb8, iCa8)
                                       /* and return the composed values */
                *pScanline     = iCa8;
                *(pScanline+1) = iCb8;
                *(pScanline+2) = iCg8;
                *(pScanline+3) = iCr8;
              }
            }
          }

          pScanline += (pData->iColinc << 2);
          pDataline += 4;
        }
      }
    }
  }

  check_update_region (pData);

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DISPLAY_ABGR8, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */
/* *                                                                        * */
/* * Background restore routines - restore the background with info from    * */
/* * the BACK and/or bKGD chunk or the app's background canvas              * */
/* *                                                                        * */
/* ************************************************************************** */

mng_retcode restore_bkgd_backimage (mng_datap pData)
{
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_RESTORE_BACKIMAGE, MNG_LC_START)
#endif
                                       /* make it easy on yourself */
  iRetcode = restore_bkgd_backcolor (pData);

  if (iRetcode)                        /* on error bail out */
    return iRetcode;

    
  /* TODO: loading the background-image */


#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_RESTORE_BACKIMAGE, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode restore_bkgd_backcolor (mng_datap pData)
{
  mng_int32  iX;
  mng_uint8p pWork = pData->pRGBArow;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_RESTORE_BACKCOLOR, MNG_LC_START)
#endif

  for (iX = pData->iSourcel; iX < pData->iSourcer; iX++)
  {                                    /* ok; drop the background-color in there */
    *pWork     = (mng_uint8)(pData->iBACKred   >> 8);
    *(pWork+1) = (mng_uint8)(pData->iBACKgreen >> 8);
    *(pWork+2) = (mng_uint8)(pData->iBACKblue  >> 8);
    *(pWork+3) = 0xFF;                 /* opaque! it's mandatory */

    pWork += 4;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_RESTORE_BACKCOLOR, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode restore_bkgd_bkgd (mng_datap pData)
{
  mng_int32      iX;
  mng_uint8p     pWork = pData->pRGBArow;
  mng_imagep     pImage;
  mng_imagedatap pBuf;
  mng_uint8      iRed   = 0;
  mng_uint8      iGreen = 0;
  mng_uint8      iBlue  = 0;


#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_RESTORE_BKGD, MNG_LC_START)
#endif
                                       /* determine the correct image buffer */
  pImage = (mng_imagep)pData->pCurrentobj;

  if (!pImage)
    pImage = (mng_imagep)pData->pObjzero;

  pBuf = pImage->pImgbuf;

  switch (pBuf->iColortype)
  {
    case 0 : ;                         /* gray types */
    case 4 : {
               mng_uint8 iGray;

               if (pBuf->iBitdepth > 8)
                 iGray = (mng_uint8)(pBuf->iBKGDgray >> 8);
               else
               {
                 iGray = (mng_uint8)pBuf->iBKGDgray;
                                       /* please note how tricky the next code is !! */
                 switch (pBuf->iBitdepth)
                 {
                   case 1 : iGray = (mng_uint8)((iGray << 1) + iGray);
                   case 2 : iGray = (mng_uint8)((iGray << 2) + iGray);
                   case 4 : iGray = (mng_uint8)((iGray << 4) + iGray);
                 }
               }

               iRed   = iGray;
               iGreen = iGray;
               iBlue  = iGray;

               break;
             }
             
    case 3 : {                         /* indexed type */
               iRed   = pBuf->aPLTEentries [pBuf->iBKGDindex].iRed;
               iGreen = pBuf->aPLTEentries [pBuf->iBKGDindex].iGreen;
               iBlue  = pBuf->aPLTEentries [pBuf->iBKGDindex].iBlue;

               break;
             }

    case 2 : ;                         /* rgb types */
    case 6 : {
               if (pBuf->iBitdepth > 8)
               {
                 iRed   = (mng_uint8)(pBuf->iBKGDred   >> 8);
                 iGreen = (mng_uint8)(pBuf->iBKGDgreen >> 8);
                 iBlue  = (mng_uint8)(pBuf->iBKGDblue  >> 8);
               }
               else
               {
                 iRed   = (mng_uint8)(pBuf->iBKGDred  );
                 iGreen = (mng_uint8)(pBuf->iBKGDgreen);
                 iBlue  = (mng_uint8)(pBuf->iBKGDblue );
               }

               break;
             }
  }

  for (iX = pData->iSourcel; iX < pData->iSourcer; iX++)
  {
    *pWork     = iRed;
    *(pWork+1) = iGreen;
    *(pWork+2) = iBlue;
    *(pWork+3) = 0x00;                 /* transparant for alpha-canvasses */

    pWork += 4;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_RESTORE_BKGD, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode restore_bkgd_bgcolor (mng_datap pData)
{
  mng_int32  iX;
  mng_uint8p pWork = pData->pRGBArow;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_RESTORE_BGCOLOR, MNG_LC_START)
#endif

  for (iX = pData->iSourcel; iX < pData->iSourcer; iX++)
  {                                    /* ok; drop the background-color in there */
    *pWork     = (mng_uint8)(pData->iBGred   >> 8);
    *(pWork+1) = (mng_uint8)(pData->iBGgreen >> 8);
    *(pWork+2) = (mng_uint8)(pData->iBGblue  >> 8);
    *(pWork+3) = 0x00;                 /* transparant for alpha-canvasses */

    pWork += 4;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_RESTORE_BGCOLOR, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode restore_bkgd_rgb8 (mng_datap pData)
{
  mng_int32  iX;
  mng_uint8p pBkgd;
  mng_uint8p pWork = pData->pRGBArow;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_RESTORE_RGB8, MNG_LC_START)
#endif

  if (pData->fGetbkgdline)             /* can we access the background ? */
  {                                    /* point to the right pixel then */
    pBkgd = (mng_uint8p)pData->fGetbkgdline ((mng_handle)pData,
                                             pData->iRow + pData->iDestt) +
            (3 * pData->iDestl);

    for (iX = pData->iSourcel; iX < pData->iSourcer; iX++)
    {
      *pWork     = *pBkgd;             /* ok; copy the pixel */
      *(pWork+1) = *(pBkgd+1);
      *(pWork+2) = *(pBkgd+2);
      *(pWork+3) = 0x00;               /* transparant for alpha-canvasses */

      pWork += 4;
      pBkgd += 3;
    }
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_RESTORE_RGB8, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode restore_bkgd_bgr8 (mng_datap pData)
{
  mng_int32  iX;
  mng_uint8p pBkgd;
  mng_uint8p pWork = pData->pRGBArow;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_RESTORE_BGR8, MNG_LC_START)
#endif

  if (pData->fGetbkgdline)             /* can we access the background ? */
  {                                    /* point to the right pixel then */
    pBkgd = (mng_uint8p)pData->fGetbkgdline ((mng_handle)pData,
                                             pData->iRow + pData->iDestt) +
            (3 * pData->iDestl);

    for (iX = pData->iSourcel; iX < pData->iSourcer; iX++)
    {
      *pWork     = *(pBkgd+2);         /* ok; copy the pixel */
      *(pWork+1) = *(pBkgd+1);
      *(pWork+2) = *pBkgd;
      *(pWork+3) = 0x00;               /* transparant for alpha-canvasses */

      pWork += 4;
      pBkgd += 3;
    }
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_RESTORE_BGR8, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */
/* *                                                                        * */
/* * Row retrieval routines - retrieve processed & uncompressed row-data    * */
/* * from the current "object"                                              * */
/* *                                                                        * */
/* ************************************************************************** */

/* TODO: a serious optimization is to retrieve only those pixels that will
         actually be displayed; this would require changes in
         the "display_image" routine (in mng_display.c) &
         all the "retrieve_xxx" routines below &
         the "display_xxx" routines above !!!!!
         NOTE that "correct_xxx" routines would not require modification */

mng_retcode retrieve_g8 (mng_datap pData)
{
  mng_imagedatap pBuf = ((mng_imagep)pData->pRetrieveobj)->pImgbuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pRGBArow;
  mng_int32      iX;
  mng_uint8      iG;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_RETRIEVE_G8, MNG_LC_START)
#endif

  pRGBArow = pData->pRGBArow;          /* temporary work pointers */
  pWorkrow = pBuf->pImgdata + (pData->iRow * pBuf->iRowsize);

  if (pBuf->bHasTRNS)                  /* tRNS in buffer ? */
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      iG = *pWorkrow;                  /* get the gray-value */
                                       /* is it transparent ? */
      if ((mng_uint16)iG == pBuf->iTRNSgray)
      {
        *pRGBArow     = 0x00;          /* nuttin to display */
        *(pRGBArow+1) = 0x00;
        *(pRGBArow+2) = 0x00;
        *(pRGBArow+3) = 0x00;
      }
      else
      {
        switch (pBuf->iBitdepth)       /* LBR to 8-bits ! */
        {
          case 1 : iG = (mng_uint8)((iG << 1) + iG);
          case 2 : iG = (mng_uint8)((iG << 2) + iG);
          case 4 : iG = (mng_uint8)((iG << 4) + iG);
        }

        *pRGBArow     = iG;            /* put in intermediate row */
        *(pRGBArow+1) = iG;
        *(pRGBArow+2) = iG;
        *(pRGBArow+3) = 0xFF;
      }

      pWorkrow++;                      /* next pixel */
      pRGBArow += 4;
    }
  }
  else
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      iG = *pWorkrow;                  /* get the gray-value */

      switch (pBuf->iBitdepth)         /* LBR to 8-bits ! */
      {
        case 1 : iG = (mng_uint8)((iG << 1) + iG);
        case 2 : iG = (mng_uint8)((iG << 2) + iG);
        case 4 : iG = (mng_uint8)((iG << 4) + iG);
      }

      *pRGBArow     = iG;              /* put in intermediate row */
      *(pRGBArow+1) = iG;
      *(pRGBArow+2) = iG;
      *(pRGBArow+3) = 0xFF;

      pWorkrow++;                      /* next pixel */
      pRGBArow += 4;
    }
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_RETRIEVE_G8, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode retrieve_g16 (mng_datap pData)
{
  mng_imagedatap pBuf = ((mng_imagep)pData->pRetrieveobj)->pImgbuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pRGBArow;
  mng_int32      iX;
  mng_uint16     iG;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_RETRIEVE_G16, MNG_LC_START)
#endif
                                       /* temporary work pointers */
  pRGBArow = pData->pRGBArow;
  pWorkrow = pBuf->pImgdata + (pData->iRow * pBuf->iRowsize);

  if (pBuf->bHasTRNS)                  /* tRNS in buffer ? */
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      iG = mng_get_uint16 (pWorkrow);  /* get the gray-value */
                                       /* is it transparent ? */
      if (iG == pBuf->iTRNSgray)
      {                                /* nuttin to display */
        mng_put_uint16 (pRGBArow,   0x0000);
        mng_put_uint16 (pRGBArow+2, 0x0000);
        mng_put_uint16 (pRGBArow+4, 0x0000);
        mng_put_uint16 (pRGBArow+6, 0x0000);
      }
      else
      {                                /* put in intermediate row */
        mng_put_uint16 (pRGBArow,   iG);
        mng_put_uint16 (pRGBArow+2, iG);
        mng_put_uint16 (pRGBArow+4, iG);
        mng_put_uint16 (pRGBArow+6, 0xFFFF);
      }

      pWorkrow += 2;                   /* next pixel */
      pRGBArow += 8;
    }
  }
  else
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      iG = mng_get_uint16 (pWorkrow);  /* get the gray-value */

      mng_put_uint16 (pRGBArow,   iG); /* and put in intermediate row */
      mng_put_uint16 (pRGBArow+2, iG);
      mng_put_uint16 (pRGBArow+4, iG);
      mng_put_uint16 (pRGBArow+6, 0xFFFF);

      pWorkrow += 2;                  /* next pixel */
      pRGBArow += 8;
    }
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_RETRIEVE_G16, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode retrieve_rgb8 (mng_datap pData)
{
  mng_imagedatap pBuf = ((mng_imagep)pData->pRetrieveobj)->pImgbuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pRGBArow;
  mng_int32      iX;
  mng_uint8      iR, iG, iB;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_RETRIEVE_RGB8, MNG_LC_START)
#endif

  pRGBArow = pData->pRGBArow;          /* temporary work pointers */
  pWorkrow = pBuf->pImgdata + (pData->iRow * pBuf->iRowsize);

  if (pBuf->bHasTRNS)                  /* tRNS in buffer ? */
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      iR = *pWorkrow;                  /* get the rgb-values */
      iG = *(pWorkrow+1);
      iB = *(pWorkrow+2);
                                       /* is it transparent ? */
      if (((mng_uint16)iR == pBuf->iTRNSred  ) &&
          ((mng_uint16)iG == pBuf->iTRNSgreen) &&
          ((mng_uint16)iB == pBuf->iTRNSblue )    )
      {
        *pRGBArow     = 0x00;          /* nothing to display */
        *(pRGBArow+1) = 0x00;
        *(pRGBArow+2) = 0x00;
        *(pRGBArow+3) = 0x00;
      }
      else
      {
        *pRGBArow     = iR;            /* put in intermediate row */
        *(pRGBArow+1) = iG;
        *(pRGBArow+2) = iB;
        *(pRGBArow+3) = 0xFF;
      }

      pWorkrow += 3;                   /* next pixel */
      pRGBArow += 4;
    }
  }
  else
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      *pRGBArow     = *pWorkrow;       /* just copy the pixel */
      *(pRGBArow+1) = *(pWorkrow+1);
      *(pRGBArow+2) = *(pWorkrow+2);
      *(pRGBArow+3) = 0xFF;

      pWorkrow += 3;                   /* next pixel */
      pRGBArow += 4;
    }
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_RETRIEVE_RGB8, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode retrieve_rgb16 (mng_datap pData)
{
  mng_imagedatap pBuf = ((mng_imagep)pData->pRetrieveobj)->pImgbuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pRGBArow;
  mng_int32      iX;
  mng_uint16     iR, iG, iB;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_RETRIEVE_RGB16, MNG_LC_START)
#endif
                                       /* temporary work pointers */
  pRGBArow = pData->pRGBArow;
  pWorkrow = pBuf->pImgdata + (pData->iRow * pBuf->iRowsize);

  if (pBuf->bHasTRNS)                  /* tRNS in buffer ? */
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      iR = mng_get_uint16 (pWorkrow);  /* get the rgb-values */
      iG = mng_get_uint16 (pWorkrow+2);
      iB = mng_get_uint16 (pWorkrow+4);
                                       /* is it transparent ? */
      if ((iR == pBuf->iTRNSred  ) &&
          (iG == pBuf->iTRNSgreen) &&
          (iB == pBuf->iTRNSblue )    )
      {                                /* nothing to display */
        mng_put_uint16 (pRGBArow,   0x0000);
        mng_put_uint16 (pRGBArow+2, 0x0000);
        mng_put_uint16 (pRGBArow+4, 0x0000);
        mng_put_uint16 (pRGBArow+6, 0x0000);
      }
      else
      {                                /* put in intermediate row */
        mng_put_uint16 (pRGBArow,   iR);
        mng_put_uint16 (pRGBArow+2, iG);
        mng_put_uint16 (pRGBArow+4, iB);
        mng_put_uint16 (pRGBArow+6, 0xFFFF);
      }

      pWorkrow += 6;                   /* next pixel */
      pRGBArow += 8;
    }
  }
  else
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {                                  /* just copy the pixel */
      mng_put_uint16 (pRGBArow,   mng_get_uint16 (pWorkrow  ));
      mng_put_uint16 (pRGBArow+2, mng_get_uint16 (pWorkrow+2));
      mng_put_uint16 (pRGBArow+4, mng_get_uint16 (pWorkrow+4));
      mng_put_uint16 (pRGBArow+6, 0xFFFF);

      pWorkrow += 6;                   /* next pixel */
      pRGBArow += 8;
    }
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_RETRIEVE_RGB16, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode retrieve_idx8 (mng_datap pData)
{
  mng_imagedatap pBuf = ((mng_imagep)pData->pRetrieveobj)->pImgbuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pRGBArow;
  mng_int32      iX;
  mng_uint8      iQ;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_RETRIEVE_IDX8, MNG_LC_START)
#endif

  pRGBArow = pData->pRGBArow;          /* temporary work pointers */
  pWorkrow = pBuf->pImgdata + (pData->iRow * pBuf->iRowsize);

  if (pBuf->bHasTRNS)                  /* tRNS in buffer ? */
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      iQ = *pWorkrow;                  /* get the index */
                                       /* is it valid ? */
      if ((mng_uint32)iQ < pBuf->iPLTEcount)
      {                                /* put in intermediate row */
        *pRGBArow     = pBuf->aPLTEentries [iQ].iRed;
        *(pRGBArow+1) = pBuf->aPLTEentries [iQ].iGreen;
        *(pRGBArow+2) = pBuf->aPLTEentries [iQ].iBlue;
                                       /* transparency for this index ? */
        if ((mng_uint32)iQ < pBuf->iTRNScount)
          *(pRGBArow+3) = pBuf->aTRNSentries [iQ];
        else
          *(pRGBArow+3) = 0xFF;
      }
      else
        MNG_ERROR (pData, MNG_PLTEINDEXERROR)

      pWorkrow++;                      /* next pixel */
      pRGBArow += 4;
    }
  }
  else
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      iQ = *pWorkrow;                  /* get the index */
                                       /* is it valid ? */
      if ((mng_uint32)iQ < pBuf->iPLTEcount)
      {                                /* put in intermediate row */
        *pRGBArow     = pBuf->aPLTEentries [iQ].iRed;
        *(pRGBArow+1) = pBuf->aPLTEentries [iQ].iGreen;
        *(pRGBArow+2) = pBuf->aPLTEentries [iQ].iBlue;
        *(pRGBArow+3) = 0xFF;
      }
      else
        MNG_ERROR (pData, MNG_PLTEINDEXERROR)

      pWorkrow++;                      /* next pixel */
      pRGBArow += 4;
    }
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_RETRIEVE_IDX8, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode retrieve_ga8 (mng_datap pData)
{
  mng_imagedatap pBuf = ((mng_imagep)pData->pRetrieveobj)->pImgbuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pRGBArow;
  mng_int32      iX;
  mng_uint8      iG;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_RETRIEVE_GA8, MNG_LC_START)
#endif

  pRGBArow = pData->pRGBArow;          /* temporary work pointers */
  pWorkrow = pBuf->pImgdata + (pData->iRow * pBuf->iRowsize);

  for (iX = 0; iX < pData->iRowsamples; iX++)
  {
    iG = *pWorkrow;                    /* get the gray-value */
    *pRGBArow     = iG;                /* put in intermediate row */
    *(pRGBArow+1) = iG;
    *(pRGBArow+2) = iG;
    *(pRGBArow+3) = *(pWorkrow+1);

    pWorkrow += 2;                     /* next pixel */
    pRGBArow += 4;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_RETRIEVE_GA8, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode retrieve_ga16 (mng_datap pData)
{
  mng_imagedatap pBuf = ((mng_imagep)pData->pRetrieveobj)->pImgbuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pRGBArow;
  mng_int32      iX;
  mng_uint16     iG;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_RETRIEVE_GA16, MNG_LC_START)
#endif
                                       /* temporary work pointers */
  pRGBArow = pData->pRGBArow;
  pWorkrow = pBuf->pImgdata + (pData->iRow * pBuf->iRowsize);

  for (iX = 0; iX < pData->iRowsamples; iX++)
  {
    iG = mng_get_uint16 (pWorkrow);    /* get the gray-value */

    mng_put_uint16 (pRGBArow,   iG);   /* and put in intermediate row */
    mng_put_uint16 (pRGBArow+2, iG);
    mng_put_uint16 (pRGBArow+4, iG);
    mng_put_uint16 (pRGBArow+6, mng_get_uint16 (pWorkrow+2));

    pWorkrow += 4;                     /* next pixel */
    pRGBArow += 8;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_RETRIEVE_GA16, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode retrieve_rgba8 (mng_datap pData)
{
  mng_imagedatap pBuf = ((mng_imagep)pData->pRetrieveobj)->pImgbuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pRGBArow;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_RETRIEVE_RGBA8, MNG_LC_START)
#endif

  pRGBArow = pData->pRGBArow;          /* temporary work pointers */
  pWorkrow = pBuf->pImgdata + (pData->iRow * pBuf->iRowsize);
                                       /* can't be easier than this ! */
  MNG_COPY (pRGBArow, pWorkrow, pBuf->iRowsize)

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_RETRIEVE_RGBA8, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode retrieve_rgba16 (mng_datap pData)
{
  mng_imagedatap pBuf = ((mng_imagep)pData->pRetrieveobj)->pImgbuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pRGBArow;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_RETRIEVE_RGBA16, MNG_LC_START)
#endif
                                       /* temporary work pointers */
  pRGBArow = pData->pRGBArow;
  pWorkrow = pBuf->pImgdata + (pData->iRow * pBuf->iRowsize);
                                       /* can't be easier than this ! */
  MNG_COPY (pRGBArow, pWorkrow, pBuf->iRowsize)

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_RETRIEVE_RGBA16, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */
/* *                                                                        * */
/* * Row storage routines - store processed & uncompressed row-data         * */
/* * into the current "object"                                              * */
/* *                                                                        * */
/* ************************************************************************** */

mng_retcode store_g1 (mng_datap pData)
{
  mng_imagedatap pBuf = (mng_imagedatap)pData->pStorebuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;
  mng_uint8      iB;
  mng_uint8      iM;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_G1, MNG_LC_START)
#endif
                                       /* temporary work pointers */
  pWorkrow = pData->pWorkrow + pData->iPixelofs;
  pOutrow  = pBuf->pImgdata + (pData->iRow * pBuf->iRowsize   ) +
                              (pData->iCol * pBuf->iSamplesize);
  iM       = 0;                        /* start at pixel 0 */
  iB       = 0;

  for (iX = 0; iX < pData->iRowsamples; iX++)
  {
    if (!iM)                           /* mask underflow ? */
    {
      iB = *pWorkrow;                  /* get next input-byte */
      pWorkrow++;
      iM = 0x80;
    }

    if (iB & iM)                       /* is it white ? */
      *pOutrow = 0x01;                 /* white */
    else
      *pOutrow = 0x00;                 /* black */

    pOutrow += pData->iColinc;         /* next pixel */
    iM >>= 1;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_G1, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode store_g2 (mng_datap pData)
{
  mng_imagedatap pBuf = (mng_imagedatap)pData->pStorebuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;
  mng_uint8      iB;
  mng_uint8      iM;
  mng_uint32     iS;
  mng_uint8      iQ;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_G2, MNG_LC_START)
#endif
                                       /* temporary work pointers */
  pWorkrow = pData->pWorkrow + pData->iPixelofs;
  pOutrow  = pBuf->pImgdata + (pData->iRow * pBuf->iRowsize   ) +
                              (pData->iCol * pBuf->iSamplesize);
  iM       = 0;                        /* start at pixel 0 */
  iB       = 0;
  iS       = 0;

  for (iX = 0; iX < pData->iRowsamples; iX++)
  {
    if (!iM)                           /* mask underflow ? */
    {
      iB = *pWorkrow;                  /* get next input-byte */
      pWorkrow++;
      iM = 0xC0;
      iS = 6;
    }

    iQ = (mng_uint8)((iB & iM) >> iS); /* get the gray level */
    *pOutrow = iQ;                     /* put in object buffer */

    pOutrow += pData->iColinc;         /* next pixel */
    iM >>= 2;
    iS -= 2;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_G2, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode store_g4 (mng_datap pData)
{
  mng_imagedatap pBuf = (mng_imagedatap)pData->pStorebuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;
  mng_uint8      iB;
  mng_uint8      iM;
  mng_uint32     iS;
  mng_uint8      iQ;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_G4, MNG_LC_START)
#endif
                                       /* temporary work pointers */
  pWorkrow = pData->pWorkrow + pData->iPixelofs;
  pOutrow  = pBuf->pImgdata + (pData->iRow * pBuf->iRowsize   ) +
                              (pData->iCol * pBuf->iSamplesize);
  iM       = 0;                        /* start at pixel 0 */
  iB       = 0;
  iS       = 0;

  for (iX = 0; iX < pData->iRowsamples; iX++)
  {
    if (!iM)                           /* mask underflow ? */
    {
      iB = *pWorkrow;                  /* get next input-byte */
      pWorkrow++;
      iM = 0xF0;
      iS = 4;
    }

    iQ = (mng_uint8)((iB & iM) >> iS); /* get the gray level */
    *pOutrow = iQ;                     /* put in object buffer */

    pOutrow += pData->iColinc;         /* next pixel */
    iM >>= 4;
    iS -= 4;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_G4, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode store_g8 (mng_datap pData)
{
  mng_imagedatap pBuf = (mng_imagedatap)pData->pStorebuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_G8, MNG_LC_START)
#endif
                                       /* temporary work pointers */
  pWorkrow = pData->pWorkrow + pData->iPixelofs;
  pOutrow  = pBuf->pImgdata + (pData->iRow * pBuf->iRowsize   ) +
                              (pData->iCol * pBuf->iSamplesize);

  for (iX = 0; iX < pData->iRowsamples; iX++)
  {
    *pOutrow = *pWorkrow;              /* put in object buffer */

    pOutrow += pData->iColinc;         /* next pixel */
    pWorkrow++;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_G8, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode store_g16 (mng_datap pData)
{
  mng_imagedatap pBuf = (mng_imagedatap)pData->pStorebuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_G16, MNG_LC_START)
#endif
                                       /* temporary work pointers */
  pWorkrow = pData->pWorkrow + pData->iPixelofs;
  pOutrow  = pBuf->pImgdata + (pData->iRow * pBuf->iRowsize   ) +
                              (pData->iCol * pBuf->iSamplesize);

  for (iX = 0; iX < pData->iRowsamples; iX++)
  {                                    /* copy into object buffer */
    mng_put_uint16 (pOutrow, mng_get_uint16 (pWorkrow));

    pOutrow  += (pData->iColinc << 1); /* next pixel */
    pWorkrow += 2;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_G16, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode store_rgb8 (mng_datap pData)
{
  mng_imagedatap pBuf = (mng_imagedatap)pData->pStorebuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_RGB8, MNG_LC_START)
#endif
                                       /* temporary work pointers */
  pWorkrow = pData->pWorkrow + pData->iPixelofs;
  pOutrow  = pBuf->pImgdata + (pData->iRow * pBuf->iRowsize   ) +
                              (pData->iCol * pBuf->iSamplesize);

  for (iX = 0; iX < pData->iRowsamples; iX++)
  {
    *pOutrow     = *pWorkrow;          /* copy the RGB bytes */
    *(pOutrow+1) = *(pWorkrow+1);
    *(pOutrow+2) = *(pWorkrow+2);

    pWorkrow += 3;                     /* next pixel */
    pOutrow  += (pData->iColinc * 3);
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_RGB8, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode store_rgb16 (mng_datap pData)
{
  mng_imagedatap pBuf = (mng_imagedatap)pData->pStorebuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_RGB16, MNG_LC_START)
#endif
                                       /* temporary work pointers */
  pWorkrow = pData->pWorkrow + pData->iPixelofs;
  pOutrow  = pBuf->pImgdata + (pData->iRow * pBuf->iRowsize   ) +
                              (pData->iCol * pBuf->iSamplesize);

  for (iX = 0; iX < pData->iRowsamples; iX++)
  {
    MNG_COPY (pOutrow, pWorkrow, 6)    /* copy the RGB bytes */

    pWorkrow += 6;                     /* next pixel */
    pOutrow  += (pData->iColinc * 6);
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_RGB16, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode store_idx1 (mng_datap pData)
{
  mng_imagedatap pBuf = (mng_imagedatap)pData->pStorebuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;
  mng_uint8      iB;
  mng_uint8      iM;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_IDX1, MNG_LC_START)
#endif
                                       /* temporary work pointers */
  pWorkrow = pData->pWorkrow + pData->iPixelofs;
  pOutrow  = pBuf->pImgdata + (pData->iRow * pBuf->iRowsize   ) +
                              (pData->iCol * pBuf->iSamplesize);
  iM       = 0;                        /* start at pixel 0 */
  iB       = 0;

  for (iX = 0; iX < pData->iRowsamples; iX++)
  {
    if (!iM)                           /* mask underflow ? */
    {
      iB = *pWorkrow;                  /* get next input-byte */
      pWorkrow++;
      iM = 0x80;
    }

    if (iB & iM)                       /* store the index */
      *pOutrow = 0x01;
    else
      *pOutrow = 0x00;

    pOutrow += pData->iColinc;         /* next pixel */
    iM >>= 1;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_IDX1, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode store_idx2 (mng_datap pData)
{
  mng_imagedatap pBuf = (mng_imagedatap)pData->pStorebuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;
  mng_uint8      iB;
  mng_uint8      iM;
  mng_uint32     iS;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_IDX2, MNG_LC_START)
#endif
                                       /* temporary work pointers */
  pWorkrow = pData->pWorkrow + pData->iPixelofs;
  pOutrow  = pBuf->pImgdata + (pData->iRow * pBuf->iRowsize   ) +
                              (pData->iCol * pBuf->iSamplesize);
  iM       = 0;                        /* start at pixel 0 */
  iB       = 0;
  iS       = 0;

  for (iX = 0; iX < pData->iRowsamples; iX++)
  {
    if (!iM)                           /* mask underflow ? */
    {
      iB = *pWorkrow;                  /* get next input-byte */
      pWorkrow++;
      iM = 0xC0;
      iS = 6;
    }
                                       /* store the index */
    *pOutrow = (mng_uint8)((iB & iM) >> iS);

    pOutrow += pData->iColinc;         /* next pixel */
    iM >>= 2;
    iS -= 2;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_IDX2, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode store_idx4 (mng_datap pData)
{
  mng_imagedatap pBuf = (mng_imagedatap)pData->pStorebuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;
  mng_uint8      iB;
  mng_uint8      iM;
  mng_uint32     iS;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_IDX4, MNG_LC_START)
#endif
                                       /* temporary work pointers */
  pWorkrow = pData->pWorkrow + pData->iPixelofs;
  pOutrow  = pBuf->pImgdata + (pData->iRow * pBuf->iRowsize   ) +
                              (pData->iCol * pBuf->iSamplesize);
  iM       = 0;                        /* start at pixel 0 */
  iB       = 0;
  iS       = 0;

  for (iX = 0; iX < pData->iRowsamples; iX++)
  {
    if (!iM)                           /* mask underflow ? */
    {
      iB = *pWorkrow;                  /* get next input-byte */
      pWorkrow++;
      iM = 0xF0;
      iS = 4;
    }
                                       /* store the index */
    *pOutrow = (mng_uint8)((iB & iM) >> iS);

    pOutrow += pData->iColinc;         /* next pixel */
    iM >>= 4;
    iS -= 4;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_IDX4, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode store_idx8 (mng_datap pData)
{
  mng_imagedatap pBuf = (mng_imagedatap)pData->pStorebuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_IDX8, MNG_LC_START)
#endif
                                       /* temporary work pointers */
  pWorkrow = pData->pWorkrow + pData->iPixelofs;
  pOutrow  = pBuf->pImgdata + (pData->iRow * pBuf->iRowsize   ) +
                              (pData->iCol * pBuf->iSamplesize);

  for (iX = 0; iX < pData->iRowsamples; iX++)
  {
    *pOutrow = *pWorkrow;              /* put in object buffer */

    pOutrow += pData->iColinc;         /* next pixel */
    pWorkrow++;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_IDX8, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode store_ga8 (mng_datap pData)
{
  mng_imagedatap pBuf = (mng_imagedatap)pData->pStorebuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_GA8, MNG_LC_START)
#endif
                                       /* temporary work pointers */
  pWorkrow = pData->pWorkrow + pData->iPixelofs;
  pOutrow  = pBuf->pImgdata + (pData->iRow * pBuf->iRowsize   ) +
                              (pData->iCol * pBuf->iSamplesize);

  for (iX = 0; iX < pData->iRowsamples; iX++)
  {
    *pOutrow     = *pWorkrow;          /* copy the GA bytes */
    *(pOutrow+1) = *(pWorkrow+1);

    pWorkrow += 2;                     /* next pixel */
    pOutrow  += (pData->iColinc << 1);
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_GA8, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode store_ga16 (mng_datap pData)
{
  mng_imagedatap pBuf = (mng_imagedatap)pData->pStorebuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_GA16, MNG_LC_START)
#endif
                                       /* temporary work pointers */
  pWorkrow = pData->pWorkrow + pData->iPixelofs;
  pOutrow  = pBuf->pImgdata + (pData->iRow * pBuf->iRowsize   ) +
                              (pData->iCol * pBuf->iSamplesize);

  for (iX = 0; iX < pData->iRowsamples; iX++)
  {
    MNG_COPY (pOutrow, pWorkrow, 4)    /* copy the GA bytes */

    pWorkrow += 4;                     /* next pixel */
    pOutrow  += (pData->iColinc << 2);
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_GA16, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode store_rgba8 (mng_datap pData)
{
  mng_imagedatap pBuf = (mng_imagedatap)pData->pStorebuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_RGBA8, MNG_LC_START)
#endif
                                       /* temporary work pointers */
  pWorkrow = pData->pWorkrow + pData->iPixelofs;
  pOutrow  = pBuf->pImgdata + (pData->iRow * pBuf->iRowsize   ) +
                              (pData->iCol * pBuf->iSamplesize);

  for (iX = 0; iX < pData->iRowsamples; iX++)
  {
    *pOutrow     = *pWorkrow;          /* copy the RGBA bytes */
    *(pOutrow+1) = *(pWorkrow+1);
    *(pOutrow+2) = *(pWorkrow+2);
    *(pOutrow+3) = *(pWorkrow+3);

    pWorkrow += 4;                     /* next pixel */
    pOutrow  += (pData->iColinc << 2);
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_RGBA8, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode store_rgba16 (mng_datap pData)
{
  mng_imagedatap pBuf = (mng_imagedatap)pData->pStorebuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_RGBA16, MNG_LC_START)
#endif
                                       /* temporary work pointers */
  pWorkrow = pData->pWorkrow + pData->iPixelofs;
  pOutrow  = pBuf->pImgdata + (pData->iRow * pBuf->iRowsize   ) +
                              (pData->iCol * pBuf->iSamplesize);

  for (iX = 0; iX < pData->iRowsamples; iX++)
  {
    MNG_COPY (pOutrow, pWorkrow, 8)    /* copy the RGBA bytes */

    pWorkrow += 8;                     /* next pixel */
    pOutrow  += (pData->iColinc << 3);
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_RGBA16, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */
/* *                                                                        * */
/* * Row storage routines (JPEG) - store processed & uncompressed row-data  * */
/* * into the current "object"                                              * */
/* *                                                                        * */
/* ************************************************************************** */

#ifdef MNG_INCLUDE_JNG

/* ************************************************************************** */

mng_retcode store_jpeg_g8 (mng_datap pData)
{
  mng_imagedatap pBuf = (mng_imagedatap)pData->pStorebuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_JPEG_G8, MNG_LC_START)
#endif

  pWorkrow = pData->pJPEGrow;          /* temporary work pointers */
  pOutrow  = pBuf->pImgdata + (pData->iJPEGrow * pBuf->iRowsize);
                                       /* easy as pie ... */
  MNG_COPY (pOutrow, pWorkrow, pData->iRowsamples)

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_JPEG_G8, MNG_LC_END)
#endif

  return next_jpeg_row (pData);        /* we've got one more row of gray-samples */
}

/* ************************************************************************** */

mng_retcode store_jpeg_rgb8 (mng_datap pData)
{
  mng_imagedatap pBuf = (mng_imagedatap)pData->pStorebuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_JPEG_RGB8, MNG_LC_START)
#endif

  pWorkrow = pData->pJPEGrow;          /* temporary work pointers */
  pOutrow  = pBuf->pImgdata + (pData->iJPEGrow * pBuf->iRowsize);
                                       /* easy as pie ... */
  MNG_COPY (pOutrow, pWorkrow, pData->iRowsamples * 3)

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_JPEG_RGB8, MNG_LC_END)
#endif

  return next_jpeg_row (pData);        /* we've got one more row of rgb-samples */
}

/* ************************************************************************** */

mng_retcode store_jpeg_ga8 (mng_datap pData)
{
  mng_imagedatap pBuf = (mng_imagedatap)pData->pStorebuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_JPEG_GA8, MNG_LC_START)
#endif

  pWorkrow = pData->pJPEGrow;          /* temporary work pointers */
  pOutrow  = pBuf->pImgdata + (pData->iJPEGrow * pBuf->iRowsize);

  for (iX = 0; iX < pData->iRowsamples; iX++)
  {
    *pOutrow = *pWorkrow;              /* copy into object buffer */

    pOutrow += 2;                      /* next pixel */
    pWorkrow++;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_JPEG_GA8, MNG_LC_END)
#endif

  return next_jpeg_row (pData);        /* we've got one more row of gray-samples */
}

/* ************************************************************************** */

mng_retcode store_jpeg_rgba8 (mng_datap pData)
{
  mng_imagedatap pBuf = (mng_imagedatap)pData->pStorebuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_JPEG_RGBA8, MNG_LC_START)
#endif

  pWorkrow = pData->pJPEGrow;          /* temporary work pointers */
  pOutrow  = pBuf->pImgdata + (pData->iJPEGrow * pBuf->iRowsize);

  for (iX = 0; iX < pData->iRowsamples; iX++)
  {
    *pOutrow     = *pWorkrow;          /* copy pixel into object buffer */
    *(pOutrow+1) = *(pWorkrow+1);
    *(pOutrow+2) = *(pWorkrow+2);

    pOutrow  += 4;                     /* next pixel */
    pWorkrow += 3;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_JPEG_RGBA8, MNG_LC_END)
#endif

  return next_jpeg_row (pData);        /* we've got one more row of rgb-samples */
}

/* ************************************************************************** */

mng_retcode store_jpeg_g8_alpha (mng_datap pData)
{
  mng_imagedatap pBuf = (mng_imagedatap)pData->pStorebuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_JPEG_G8_ALPHA, MNG_LC_START)
#endif
                                       /* temporary work pointers */
  pWorkrow = pData->pJPEGrow2;
  pOutrow  = pBuf->pImgdata + (pData->iJPEGalpharow * pBuf->iRowsize) + 1;

  for (iX = 0; iX < pData->iRowsamples; iX++)
  {
    *pOutrow = *pWorkrow;              /* put in object buffer */

    pOutrow += 2;                      /* next pixel */
    pWorkrow++;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_JPEG_G8_ALPHA, MNG_LC_END)
#endif

  return next_jpeg_alpharow (pData);   /* we've got one more row of alpha-samples */
}

/* ************************************************************************** */

mng_retcode store_jpeg_rgb8_alpha (mng_datap pData)
{
  mng_imagedatap pBuf = (mng_imagedatap)pData->pStorebuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_JPEG_RGB8_ALPHA, MNG_LC_START)
#endif
                                       /* temporary work pointers */
  pWorkrow = pData->pJPEGrow2;
  pOutrow  = pBuf->pImgdata + (pData->iJPEGalpharow * pBuf->iRowsize) + 3;

  for (iX = 0; iX < pData->iRowsamples; iX++)
  {
    *pOutrow = *pWorkrow;              /* put in object buffer */

    pOutrow += 4;                      /* next pixel */
    pWorkrow++;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_JPEG_RGB8_ALPHA, MNG_LC_END)
#endif

  return next_jpeg_alpharow (pData);   /* we've got one more row of alpha-samples */
}

/* ************************************************************************** */

mng_retcode store_jpeg_g8_a1 (mng_datap pData)
{
  mng_imagedatap pBuf = (mng_imagedatap)pData->pStorebuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;
  mng_uint8      iB;
  mng_uint8      iM;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_JPEG_G8_A1, MNG_LC_START)
#endif
                                       /* temporary work pointers */
  pWorkrow = pData->pWorkrow + pData->iPixelofs;
  pOutrow  = pBuf->pImgdata + (pData->iRow * pBuf->iRowsize   ) +
                              (pData->iCol * pBuf->iSamplesize) + 1;
  iM       = 0;                        /* start at pixel 0 */
  iB       = 0;

  for (iX = 0; iX < pData->iRowsamples; iX++)
  {
    if (!iM)                           /* mask underflow ? */
    {
      iB = *pWorkrow;                  /* get next input-byte */
      pWorkrow++;
      iM = 0x80;
    }

    if (iB & iM)                       /* is it opaque ? */
      *pOutrow = 0xFF;                 /* opaque */
    else
      *pOutrow = 0x00;                 /* transparent */

    pOutrow += 2;                      /* next pixel */
    iM >>= 1;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_JPEG_G8_A1, MNG_LC_END)
#endif

  return next_jpeg_alpharow (pData);   /* we've got one more row of alpha-samples */
}

/* ************************************************************************** */

mng_retcode store_jpeg_g8_a2 (mng_datap pData)
{
  mng_imagedatap pBuf = (mng_imagedatap)pData->pStorebuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;
  mng_uint8      iB;
  mng_uint8      iM;
  mng_uint32     iS;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_JPEG_G8_A2, MNG_LC_START)
#endif
                                       /* temporary work pointers */
  pWorkrow = pData->pWorkrow + pData->iPixelofs;
  pOutrow  = pBuf->pImgdata + (pData->iRow * pBuf->iRowsize   ) +
                              (pData->iCol * pBuf->iSamplesize) + 1;
  iM       = 0;                        /* start at pixel 0 */
  iB       = 0;
  iS       = 0;

  for (iX = 0; iX < pData->iRowsamples; iX++)
  {
    if (!iM)                           /* mask underflow ? */
    {
      iB = *pWorkrow;                  /* get next input-byte */
      pWorkrow++;
      iM = 0xC0;
      iS = 6;
    }

    switch ((iB & iM) >> iS)           /* determine the alpha level */
    {
      case 0x03 : { *pOutrow = 0xFF; break; }
      case 0x02 : { *pOutrow = 0xAA; break; }
      case 0x01 : { *pOutrow = 0x55; break; }
      default   : { *pOutrow = 0x00; }
    }

    pOutrow += 2;                      /* next pixel */
    iM >>= 2;
    iS -= 2;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_JPEG_G8_A2, MNG_LC_END)
#endif

  return next_jpeg_alpharow (pData);   /* we've got one more row of alpha-samples */
}

/* ************************************************************************** */

mng_retcode store_jpeg_g8_a4 (mng_datap pData)
{
  mng_imagedatap pBuf = (mng_imagedatap)pData->pStorebuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;
  mng_uint8      iB;
  mng_uint8      iM;
  mng_uint32     iS;
  mng_uint8      iQ;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_JPEG_G8_A4, MNG_LC_START)
#endif
                                       /* temporary work pointers */
  pWorkrow = pData->pWorkrow + pData->iPixelofs;
  pOutrow  = pBuf->pImgdata + (pData->iRow * pBuf->iRowsize   ) +
                              (pData->iCol * pBuf->iSamplesize) + 1;
  iM       = 0;                        /* start at pixel 0 */
  iB       = 0;
  iS       = 0;

  for (iX = 0; iX < pData->iRowsamples; iX++)
  {
    if (!iM)                           /* mask underflow ? */
    {
      iB = *pWorkrow;                  /* get next input-byte */
      pWorkrow++;
      iM = 0xF0;
      iS = 4;
    }
                                       /* get the alpha level */
    iQ = (mng_uint8)((iB & iM) >> iS);
    iQ = (mng_uint8)(iQ + (iQ << 4));  /* expand to 8-bit by replication */

    *pOutrow = iQ;                     /* put in object buffer */

    pOutrow += 2;                      /* next pixel */
    iM >>= 4;
    iS -= 4;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_JPEG_G8_A4, MNG_LC_END)
#endif

  return next_jpeg_alpharow (pData);   /* we've got one more row of alpha-samples */
}

/* ************************************************************************** */

mng_retcode store_jpeg_g8_a8 (mng_datap pData)
{
  mng_imagedatap pBuf = (mng_imagedatap)pData->pStorebuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_JPEG_G8_A8, MNG_LC_START)
#endif
                                       /* temporary work pointers */
  pWorkrow = pData->pWorkrow + pData->iPixelofs;
  pOutrow  = pBuf->pImgdata + (pData->iRow * pBuf->iRowsize   ) +
                              (pData->iCol * pBuf->iSamplesize) + 1;

  for (iX = 0; iX < pData->iRowsamples; iX++)
  {
    *pOutrow = *pWorkrow;              /* put in object buffer */

    pOutrow += 2;                      /* next pixel */
    pWorkrow++;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_JPEG_G8_A8, MNG_LC_END)
#endif

  return next_jpeg_alpharow (pData);   /* we've got one more row of alpha-samples */
}

/* ************************************************************************** */

mng_retcode store_jpeg_g8_a16 (mng_datap pData)
{
  mng_imagedatap pBuf = (mng_imagedatap)pData->pStorebuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_JPEG_G8_A16, MNG_LC_START)
#endif
                                       /* temporary work pointers */
  pWorkrow = pData->pWorkrow + pData->iPixelofs;
  pOutrow  = pBuf->pImgdata + (pData->iRow * pBuf->iRowsize   ) +
                              (pData->iCol * pBuf->iSamplesize) + 1;

  for (iX = 0; iX < pData->iRowsamples; iX++)
  {
    *pOutrow = *pWorkrow;              /* only high-order byte! */

    pOutrow  += 2;                     /* next pixel */
    pWorkrow += 2;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_JPEG_G8_A16, MNG_LC_END)
#endif

  return next_jpeg_alpharow (pData);   /* we've got one more row of alpha-samples */
}

/* ************************************************************************** */

mng_retcode store_jpeg_rgb8_a1 (mng_datap pData)
{
  mng_imagedatap pBuf = (mng_imagedatap)pData->pStorebuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;
  mng_uint8      iB;
  mng_uint8      iM;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_JPEG_RGB8_A1, MNG_LC_START)
#endif
                                       /* temporary work pointers */
  pWorkrow = pData->pWorkrow + pData->iPixelofs;
  pOutrow  = pBuf->pImgdata + (pData->iRow * pBuf->iRowsize   ) +
                              (pData->iCol * pBuf->iSamplesize) + 3;
  iM       = 0;                        /* start at pixel 0 */
  iB       = 0;

  for (iX = 0; iX < pData->iRowsamples; iX++)
  {
    if (!iM)                           /* mask underflow ? */
    {
      iB = *pWorkrow;                  /* get next input-byte */
      pWorkrow++;
      iM = 0x80;
    }

    if (iB & iM)                       /* is it opaque ? */
      *pOutrow = 0xFF;                 /* opaque */
    else
      *pOutrow = 0x00;                 /* transparent */

    pOutrow += 4;                      /* next pixel */
    iM >>= 1;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_JPEG_RGB8_A1, MNG_LC_END)
#endif

  return next_jpeg_alpharow (pData);   /* we've got one more row of alpha-samples */
}

/* ************************************************************************** */

mng_retcode store_jpeg_rgb8_a2 (mng_datap pData)
{
  mng_imagedatap pBuf = (mng_imagedatap)pData->pStorebuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;
  mng_uint8      iB;
  mng_uint8      iM;
  mng_uint32     iS;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_JPEG_RGB8_A2, MNG_LC_START)
#endif
                                       /* temporary work pointers */
  pWorkrow = pData->pWorkrow + pData->iPixelofs;
  pOutrow  = pBuf->pImgdata + (pData->iRow * pBuf->iRowsize   ) +
                              (pData->iCol * pBuf->iSamplesize) + 3;
  iM       = 0;                        /* start at pixel 0 */
  iB       = 0;
  iS       = 0;

  for (iX = 0; iX < pData->iRowsamples; iX++)
  {
    if (!iM)                           /* mask underflow ? */
    {
      iB = *pWorkrow;                  /* get next input-byte */
      pWorkrow++;
      iM = 0xC0;
      iS = 6;
    }

    switch ((iB & iM) >> iS)           /* determine the alpha level */
    {
      case 0x03 : { *pOutrow = 0xFF; break; }
      case 0x02 : { *pOutrow = 0xAA; break; }
      case 0x01 : { *pOutrow = 0x55; break; }
      default   : { *pOutrow = 0x00; }
    }

    pOutrow += 4;                      /* next pixel */
    iM >>= 2;
    iS -= 2;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_JPEG_RGB8_A2, MNG_LC_END)
#endif

  return next_jpeg_alpharow (pData);   /* we've got one more row of alpha-samples */
}

/* ************************************************************************** */

mng_retcode store_jpeg_rgb8_a4 (mng_datap pData)
{
  mng_imagedatap pBuf = (mng_imagedatap)pData->pStorebuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;
  mng_uint8      iB;
  mng_uint8      iM;
  mng_uint32     iS;
  mng_uint8      iQ;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_JPEG_RGB8_A4, MNG_LC_START)
#endif
                                       /* temporary work pointers */
  pWorkrow = pData->pWorkrow + pData->iPixelofs;
  pOutrow  = pBuf->pImgdata + (pData->iRow * pBuf->iRowsize   ) +
                              (pData->iCol * pBuf->iSamplesize) + 3;
  iM       = 0;                        /* start at pixel 0 */
  iB       = 0;
  iS       = 0;

  for (iX = 0; iX < pData->iRowsamples; iX++)
  {
    if (!iM)                           /* mask underflow ? */
    {
      iB = *pWorkrow;                  /* get next input-byte */
      pWorkrow++;
      iM = 0xF0;
      iS = 4;
    }
                                       /* get the alpha level */
    iQ = (mng_uint8)((iB & iM) >> iS);
    iQ = (mng_uint8)(iQ + (iQ << 4));  /* expand to 8-bit by replication */

    *pOutrow = iQ;                     /* put in object buffer */

    pOutrow += 4;                      /* next pixel */
    iM >>= 4;
    iS -= 4;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_JPEG_RGB8_A4, MNG_LC_END)
#endif

  return next_jpeg_alpharow (pData);   /* we've got one more row of alpha-samples */
}

/* ************************************************************************** */

mng_retcode store_jpeg_rgb8_a8 (mng_datap pData)
{
  mng_imagedatap pBuf = (mng_imagedatap)pData->pStorebuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_JPEG_RGB8_A8, MNG_LC_START)
#endif
                                       /* temporary work pointers */
  pWorkrow = pData->pWorkrow + pData->iPixelofs;
  pOutrow  = pBuf->pImgdata + (pData->iRow * pBuf->iRowsize   ) +
                              (pData->iCol * pBuf->iSamplesize) + 3;

  for (iX = 0; iX < pData->iRowsamples; iX++)
  {
    *pOutrow = *pWorkrow;              /* put in buffer */

    pOutrow += 4;                      /* next pixel */
    pWorkrow++;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_JPEG_RGB8_A8, MNG_LC_END)
#endif

  return next_jpeg_alpharow (pData);   /* we've got one more row of alpha-samples */
}

/* ************************************************************************** */

mng_retcode store_jpeg_rgb8_a16 (mng_datap pData)
{
  mng_imagedatap pBuf = (mng_imagedatap)pData->pStorebuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_JPEG_RGB8_A16, MNG_LC_START)
#endif
                                       /* temporary work pointers */
  pWorkrow = pData->pWorkrow + pData->iPixelofs;
  pOutrow  = pBuf->pImgdata + (pData->iRow * pBuf->iRowsize   ) +
                              (pData->iCol * pBuf->iSamplesize) + 3;

  for (iX = 0; iX < pData->iRowsamples; iX++)
  {
    *pOutrow = *pWorkrow;              /* only high-order byte */

    pOutrow  += 4;                     /* next pixel */
    pWorkrow += 2;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_JPEG_RGB8_A16, MNG_LC_END)
#endif

  return next_jpeg_alpharow (pData);   /* we've got one more row of alpha-samples */
}

/* ************************************************************************** */

mng_retcode store_jpeg_g12_a1 (mng_datap pData)
{
  mng_imagedatap pBuf = (mng_imagedatap)pData->pStorebuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;
  mng_uint8      iB;
  mng_uint8      iM;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_JPEG_G12_A1, MNG_LC_START)
#endif
                                       /* temporary work pointers */
  pWorkrow = pData->pWorkrow + pData->iPixelofs;
  pOutrow  = pBuf->pImgdata + (pData->iRow * pBuf->iRowsize   ) +
                              (pData->iCol * pBuf->iSamplesize) + 2;
  iM       = 0;                        /* start at pixel 0 */
  iB       = 0;

  for (iX = 0; iX < pData->iRowsamples; iX++)
  {
    if (!iM)                           /* mask underflow ? */
    {
      iB = *pWorkrow;                  /* get next input-byte */
      pWorkrow++;
      iM = 0x80;
    }

    if (iB & iM)                       /* opaque ? */
      mng_put_uint16 (pOutrow, 0xFFFF);/* opaque */
    else
      mng_put_uint16 (pOutrow, 0x0000);/* transparent */

    pOutrow += 4;                      /* next pixel */
    iM >>= 1;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_JPEG_G12_A1, MNG_LC_END)
#endif

  return next_jpeg_alpharow (pData);   /* we've got one more row of alpha-samples */
}

/* ************************************************************************** */

mng_retcode store_jpeg_g12_a2 (mng_datap pData)
{
  mng_imagedatap pBuf = (mng_imagedatap)pData->pStorebuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;
  mng_uint8      iB;
  mng_uint8      iM;
  mng_uint32     iS;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_JPEG_G12_A2, MNG_LC_START)
#endif
                                       /* temporary work pointers */
  pWorkrow = pData->pWorkrow + pData->iPixelofs;
  pOutrow  = pBuf->pImgdata + (pData->iRow * pBuf->iRowsize   ) +
                              (pData->iCol * pBuf->iSamplesize) + 2;
  iM       = 0;                        /* start at pixel 0 */
  iB       = 0;
  iS       = 0;

  for (iX = 0; iX < pData->iRowsamples; iX++)
  {
    if (!iM)                           /* mask underflow ? */
    {
      iB = *pWorkrow;                  /* get next input-byte */
      pWorkrow++;
      iM = 0xC0;
      iS = 6;
    }

    switch ((iB & iM) >> iS)           /* determine the gray level */
    {
      case 0x03 : { mng_put_uint16 (pOutrow, 0xFFFF); break; }
      case 0x02 : { mng_put_uint16 (pOutrow, 0xAAAA); break; }
      case 0x01 : { mng_put_uint16 (pOutrow, 0x5555); break; }
      default   : { mng_put_uint16 (pOutrow, 0x0000); }
    }

    pOutrow += 4;                      /* next pixel */
    iM >>= 2;
    iS -= 2;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_JPEG_G12_A2, MNG_LC_END)
#endif

  return next_jpeg_alpharow (pData);   /* we've got one more row of alpha-samples */
}

/* ************************************************************************** */

mng_retcode store_jpeg_g12_a4 (mng_datap pData)
{
  mng_imagedatap pBuf = (mng_imagedatap)pData->pStorebuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;
  mng_uint8      iB;
  mng_uint8      iM;
  mng_uint32     iS;
  mng_uint16     iQ;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_JPEG_G12_A4, MNG_LC_START)
#endif
                                       /* temporary work pointers */
  pWorkrow = pData->pWorkrow + pData->iPixelofs;
  pOutrow  = pBuf->pImgdata + (pData->iRow * pBuf->iRowsize   ) +
                              (pData->iCol * pBuf->iSamplesize) + 2;
  iM       = 0;                        /* start at pixel 0 */
  iB       = 0;
  iS       = 0;

  for (iX = 0; iX < pData->iRowsamples; iX++)
  {
    if (!iM)                           /* mask underflow ? */
    {
      iB = *pWorkrow;                  /* get next input-byte */
      pWorkrow++;
      iM = 0xF0;
      iS = 4;
    }
                                       /* get the gray level */
    iQ = (mng_uint16)((iB & iM) >> iS);
    iQ = (mng_uint16)(iQ + (iQ << 4)); /* expand to 16-bit by replication */
    iQ = (mng_uint16)(iQ + (iQ << 8));
                                       /* put in object buffer */
    mng_put_uint16 (pOutrow, iQ);

    pOutrow += 4;                      /* next pixel */
    iM >>= 4;
    iS -= 4;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_JPEG_G12_A4, MNG_LC_END)
#endif

  return next_jpeg_alpharow (pData);   /* we've got one more row of alpha-samples */
}

/* ************************************************************************** */

mng_retcode store_jpeg_g12_a8 (mng_datap pData)
{
  mng_imagedatap pBuf = (mng_imagedatap)pData->pStorebuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;
  mng_uint16     iW;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_JPEG_G12_A8, MNG_LC_START)
#endif
                                       /* temporary work pointers */
  pWorkrow = pData->pWorkrow + pData->iPixelofs;
  pOutrow  = pBuf->pImgdata + (pData->iRow * pBuf->iRowsize   ) +
                              (pData->iCol * pBuf->iSamplesize) + 2;

  for (iX = 0; iX < pData->iRowsamples; iX++)
  {
    iW = (mng_uint16)(*pWorkrow);      /* get input byte */
    iW = (mng_uint16)(iW + (iW << 8)); /* expand to 16-bit by replication */

    mng_put_uint16 (pOutrow, iW);      /* put in object buffer */

    pOutrow += 4;                      /* next pixel */
    pWorkrow++;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_JPEG_G12_A8, MNG_LC_END)
#endif

  return next_jpeg_alpharow (pData);   /* we've got one more row of alpha-samples */
}

/* ************************************************************************** */

mng_retcode store_jpeg_g12_a16 (mng_datap pData)
{
  mng_imagedatap pBuf = (mng_imagedatap)pData->pStorebuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_JPEG_G12_A16, MNG_LC_START)
#endif
                                       /* temporary work pointers */
  pWorkrow = pData->pWorkrow + pData->iPixelofs;
  pOutrow  = pBuf->pImgdata + (pData->iRow * pBuf->iRowsize   ) +
                              (pData->iCol * pBuf->iSamplesize) + 2;

  for (iX = 0; iX < pData->iRowsamples; iX++)
  {                                    /* copy it */
    mng_put_uint16 (pOutrow, mng_get_uint16 (pWorkrow));

    pOutrow  += 4;                     /* next pixel */
    pWorkrow += 2;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_STORE_JPEG_G12_A16, MNG_LC_END)
#endif

  return next_jpeg_alpharow (pData);   /* we've got one more row of alpha-samples */
}

/* ************************************************************************** */

#endif /* MNG_INCLUDE_JNG */

/* ************************************************************************** */
/* *                                                                        * */
/* * Delta-image row routines - apply the processed & uncompressed row-data * */
/* * onto the target "object"                                               * */
/* *                                                                        * */
/* ************************************************************************** */

mng_retcode delta_g1 (mng_datap pData)
{
  mng_imagedatap pBuf = ((mng_imagep)pData->pDeltaImage)->pImgbuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;
  mng_uint8      iB;
  mng_uint8      iM;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_G1, MNG_LC_START)
#endif
                                       /* temporary work pointers */
  pWorkrow = pData->pWorkrow + pData->iPixelofs;
  pOutrow  = pBuf->pImgdata + (pData->iRow         * pBuf->iRowsize   ) +
                              (pData->iDeltaBlocky * pBuf->iRowsize   ) +
                              (pData->iCol         * pBuf->iSamplesize) +
                              (pData->iDeltaBlockx * pBuf->iSamplesize);
  iM       = 0;                        /* start at pixel 0 */
  iB       = 0;
                                       /* pixel replace ? */
  if (pData->iDeltatype == MNG_DELTATYPE_BLOCKPIXELREPLACE)
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      if (!iM)                         /* mask underflow ? */
      {
        iB = *pWorkrow;                /* get next input-byte */
        pWorkrow++;
        iM = 0x80;
      }

      if (iB & iM)                     /* is it white ? */
        *pOutrow = 0xFF;               /* white */
      else
        *pOutrow = 0x00;               /* black */

      pOutrow += pData->iColinc;       /* next pixel */
      iM >>= 1;
    }
  }
  else
  {                                    /* pixel add ! */
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      if (!iM)                         /* mask underflow ? */
      {
        iB = *pWorkrow;                /* get next input-byte */
        pWorkrow++;
        iM = 0x80;
      }

      if (iB & iM)                     /* invert if it is white ? */
        *pOutrow = (mng_uint8)(*pOutrow ^ 0xFF);

      pOutrow += pData->iColinc;       /* next pixel */
      iM >>= 1;
    }
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_G1, MNG_LC_END)
#endif

  return store_g1 (pData);
}

/* ************************************************************************** */

mng_retcode delta_g2 (mng_datap pData)
{
  mng_imagedatap pBuf = ((mng_imagep)pData->pDeltaImage)->pImgbuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;
  mng_uint8      iB;
  mng_uint8      iM;
  mng_uint32     iS;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_G2, MNG_LC_START)
#endif
                                       /* temporary work pointers */
  pWorkrow = pData->pWorkrow + pData->iPixelofs;
  pOutrow  = pBuf->pImgdata + (pData->iRow         * pBuf->iRowsize   ) +
                              (pData->iDeltaBlocky * pBuf->iRowsize   ) +
                              (pData->iCol         * pBuf->iSamplesize) +
                              (pData->iDeltaBlockx * pBuf->iSamplesize);
  iM       = 0;                        /* start at pixel 0 */
  iB       = 0;
  iS       = 0;
                                       /* pixel replace ? */
  if (pData->iDeltatype == MNG_DELTATYPE_BLOCKPIXELREPLACE)
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      if (!iM)                         /* mask underflow ? */
      {
        iB = *pWorkrow;                /* get next input-byte */
        pWorkrow++;
        iM = 0xC0;
        iS = 6;
      }

      switch ((iB & iM) >> iS)         /* determine the gray level */
      {
        case 0x03 : { *pOutrow = 0xFF; break; }
        case 0x02 : { *pOutrow = 0xAA; break; }
        case 0x01 : { *pOutrow = 0x55; break; }
        default   : { *pOutrow = 0x00; }
      }

      pOutrow += pData->iColinc;       /* next pixel */
      iM >>= 2;
      iS -= 2;
    }
  }
  else
  {                                    /* pixel add ! */
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      if (!iM)                         /* mask underflow ? */
      {
        iB = *pWorkrow;                /* get next input-byte */
        pWorkrow++;
        iM = 0xC0;
        iS = 6;
      }
                                       /* determine the gray level */
      switch (((*pOutrow >> 6) + ((iB & iM) >> iS)) & 0x03)
      {
        case 0x03 : { *pOutrow = 0xFF; break; }
        case 0x02 : { *pOutrow = 0xAA; break; }
        case 0x01 : { *pOutrow = 0x55; break; }
        default   : { *pOutrow = 0x00; }
      }

      pOutrow += pData->iColinc;       /* next pixel */
      iM >>= 2;
      iS -= 2;
    }
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_G2, MNG_LC_END)
#endif

  return store_g2 (pData);
}

/* ************************************************************************** */

mng_retcode delta_g4 (mng_datap pData)
{
  mng_imagedatap pBuf = ((mng_imagep)pData->pDeltaImage)->pImgbuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;
  mng_uint8      iB;
  mng_uint8      iM;
  mng_uint32     iS;
  mng_uint8      iQ;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_G4, MNG_LC_START)
#endif
                                       /* temporary work pointers */
  pWorkrow = pData->pWorkrow + pData->iPixelofs;
  pOutrow  = pBuf->pImgdata + (pData->iRow         * pBuf->iRowsize   ) +
                              (pData->iDeltaBlocky * pBuf->iRowsize   ) +
                              (pData->iCol         * pBuf->iSamplesize) +
                              (pData->iDeltaBlockx * pBuf->iSamplesize);
  iM       = 0;                        /* start at pixel 0 */
  iB       = 0;
  iS       = 0;
                                       /* pixel replace ? */
  if (pData->iDeltatype == MNG_DELTATYPE_BLOCKPIXELREPLACE)
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      if (!iM)                         /* mask underflow ? */
      {
        iB = *pWorkrow;                /* get next input-byte */
        pWorkrow++;
        iM = 0xF0;
        iS = 4;
      }
                                       /* get the gray level */
      iQ = (mng_uint8)((iB & iM) >> iS);
                                       /* expand to 8-bit by replication */
      iQ = (mng_uint8)(iQ + (iQ << 4));

      *pOutrow = iQ;                   /* put in object buffer */

      pOutrow += pData->iColinc;       /* next pixel */
      iM >>= 4;
      iS -= 4;
    }
  }
  else
  {                                    /* pixel add ! */
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      if (!iM)                         /* mask underflow ? */
      {
        iB = *pWorkrow;                /* get next input-byte */
        pWorkrow++;
        iM = 0xF0;
        iS = 4;
      }
                                       /* get the gray level */
      iQ = (mng_uint8)(((*pOutrow >> 4) + ((iB & iM) >> iS)) & 0x0F);
                                       /* expand to 8-bit by replication */
      iQ = (mng_uint8)(iQ + (iQ << 4));

      *pOutrow = iQ;                   /* put in object buffer */

      pOutrow += pData->iColinc;       /* next pixel */
      iM >>= 4;
      iS -= 4;
    }
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_G4, MNG_LC_END)
#endif

  return store_g4 (pData);
}

/* ************************************************************************** */

mng_retcode delta_g8 (mng_datap pData)
{
  mng_imagedatap pBuf = ((mng_imagep)pData->pDeltaImage)->pImgbuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_G8, MNG_LC_START)
#endif
                                       /* temporary work pointers */
  pWorkrow = pData->pWorkrow + pData->iPixelofs;
  pOutrow  = pBuf->pImgdata + (pData->iRow         * pBuf->iRowsize   ) +
                              (pData->iDeltaBlocky * pBuf->iRowsize   ) +
                              (pData->iCol         * pBuf->iSamplesize) +
                              (pData->iDeltaBlockx * pBuf->iSamplesize);
                                       /* pixel replace ? */
  if (pData->iDeltatype == MNG_DELTATYPE_BLOCKPIXELREPLACE)
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      *pOutrow = *pWorkrow;            /* put in object buffer */

      pOutrow += pData->iColinc;       /* next pixel */
      pWorkrow++;
    }
  }
  else
  {                                    /* pixel add ! */
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {                                  /* add to object buffer */
      *pOutrow = (mng_uint8)(*pOutrow + *pWorkrow);

      pOutrow += pData->iColinc;       /* next pixel */
      pWorkrow++;
    }
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_G8, MNG_LC_END)
#endif

  return store_g8 (pData);
}

/* ************************************************************************** */

mng_retcode delta_g16 (mng_datap pData)
{
  mng_imagedatap pBuf = ((mng_imagep)pData->pDeltaImage)->pImgbuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_G16, MNG_LC_START)
#endif
                                       /* temporary work pointers */
  pWorkrow = pData->pWorkrow + pData->iPixelofs;
  pOutrow  = pBuf->pImgdata + (pData->iRow         * pBuf->iRowsize   ) +
                              (pData->iDeltaBlocky * pBuf->iRowsize   ) +
                              (pData->iCol         * pBuf->iSamplesize) +
                              (pData->iDeltaBlockx * pBuf->iSamplesize);
                                       /* pixel replace ? */
  if (pData->iDeltatype == MNG_DELTATYPE_BLOCKPIXELREPLACE)
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      *pOutrow     = *pWorkrow;        /* put in object buffer */
      *(pOutrow+1) = *(pWorkrow+1);
                                       /* next pixel */
      pOutrow  += (pData->iColinc << 1);
      pWorkrow += 2;
    }
  }
  else
  {                                    /* pixel add ! */
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {                                  /* add to object buffer */
      mng_put_uint16 (pOutrow, (mng_uint16)(mng_get_uint16 (pOutrow ) +
                                            mng_get_uint16 (pWorkrow)   ));
                                       /* next pixel */
      pOutrow  += (pData->iColinc << 1);
      pWorkrow += 2;
    }
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_G16, MNG_LC_END)
#endif

  return store_g16 (pData);
}

/* ************************************************************************** */

mng_retcode delta_rgb8 (mng_datap pData)
{
  mng_imagedatap pBuf = ((mng_imagep)pData->pDeltaImage)->pImgbuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_RGB8, MNG_LC_START)
#endif
                                       /* temporary work pointers */
  pWorkrow = pData->pWorkrow + pData->iPixelofs;
  pOutrow  = pBuf->pImgdata + (pData->iRow         * pBuf->iRowsize   ) +
                              (pData->iDeltaBlocky * pBuf->iRowsize   ) +
                              (pData->iCol         * pBuf->iSamplesize) +
                              (pData->iDeltaBlockx * pBuf->iSamplesize);
                                       /* pixel replace ? */
  if (pData->iDeltatype == MNG_DELTATYPE_BLOCKPIXELREPLACE)
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      *pOutrow     = *pWorkrow;        /* put in object buffer */
      *(pOutrow+1) = *(pWorkrow+1);
      *(pOutrow+2) = *(pWorkrow+2);
                                       /* next pixel */
      pOutrow  += (pData->iColinc * 3);
      pWorkrow += 3;
    }
  }
  else
  {                                    /* pixel add ! */
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {                                  /* add to object buffer */
      *pOutrow     = (mng_uint8)(*pOutrow     + *pWorkrow    );
      *(pOutrow+1) = (mng_uint8)(*(pOutrow+1) + *(pWorkrow+1));
      *(pOutrow+2) = (mng_uint8)(*(pOutrow+2) + *(pWorkrow+2));
                                       /* next pixel */
      pOutrow  += (pData->iColinc * 3);
      pWorkrow += 3;
    }
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_RGB8, MNG_LC_END)
#endif

  return store_rgb8 (pData);
}

/* ************************************************************************** */

mng_retcode delta_rgb16 (mng_datap pData)
{
  mng_imagedatap pBuf = ((mng_imagep)pData->pDeltaImage)->pImgbuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_RGB16, MNG_LC_START)
#endif
                                       /* temporary work pointers */
  pWorkrow = pData->pWorkrow + pData->iPixelofs;
  pOutrow  = pBuf->pImgdata + (pData->iRow         * pBuf->iRowsize   ) +
                              (pData->iDeltaBlocky * pBuf->iRowsize   ) +
                              (pData->iCol         * pBuf->iSamplesize) +
                              (pData->iDeltaBlockx * pBuf->iSamplesize);
                                       /* pixel replace ? */
  if (pData->iDeltatype == MNG_DELTATYPE_BLOCKPIXELREPLACE)
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      *pOutrow     = *pWorkrow;        /* put in object buffer */
      *(pOutrow+1) = *(pWorkrow+1);
      *(pOutrow+2) = *(pWorkrow+2);
      *(pOutrow+3) = *(pWorkrow+3);
      *(pOutrow+4) = *(pWorkrow+4);
      *(pOutrow+5) = *(pWorkrow+5);
                                       /* next pixel */
      pOutrow  += (pData->iColinc * 6);
      pWorkrow += 6;
    }
  }
  else
  {                                    /* pixel add ! */
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {                                  /* add to object buffer */
      mng_put_uint16 (pOutrow,   (mng_uint16)(mng_get_uint16 (pOutrow   ) +
                                              mng_get_uint16 (pWorkrow  )   ));
      mng_put_uint16 (pOutrow+2, (mng_uint16)(mng_get_uint16 (pOutrow+2 ) +
                                              mng_get_uint16 (pWorkrow+2)   ));
      mng_put_uint16 (pOutrow+4, (mng_uint16)(mng_get_uint16 (pOutrow+4 ) +
                                              mng_get_uint16 (pWorkrow+4)   ));
                                       /* next pixel */
      pOutrow  += (pData->iColinc * 6);
      pWorkrow += 6;
    }
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_RGB16, MNG_LC_END)
#endif

  return store_rgb16 (pData);
}

/* ************************************************************************** */

mng_retcode delta_idx1 (mng_datap pData)
{
  mng_imagedatap pBuf = ((mng_imagep)pData->pDeltaImage)->pImgbuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;
  mng_uint8      iB;
  mng_uint8      iM;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_IDX1, MNG_LC_START)
#endif
                                       /* temporary work pointers */
  pWorkrow = pData->pWorkrow + pData->iPixelofs;
  pOutrow  = pBuf->pImgdata + (pData->iRow         * pBuf->iRowsize   ) +
                              (pData->iDeltaBlocky * pBuf->iRowsize   ) +
                              (pData->iCol         * pBuf->iSamplesize) +
                              (pData->iDeltaBlockx * pBuf->iSamplesize);
  iM       = 0;                        /* start at pixel 0 */
  iB       = 0;
                                       /* pixel replace ? */
  if (pData->iDeltatype == MNG_DELTATYPE_BLOCKPIXELREPLACE)
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      if (!iM)                         /* mask underflow ? */
      {
        iB = *pWorkrow;                /* get next input-byte */
        pWorkrow++;
        iM = 0x80;
      }

      if (iB & iM)                     /* put the right index value */
        *pOutrow = 1;
      else
        *pOutrow = 0;

      pOutrow += pData->iColinc;       /* next pixel */
      iM >>= 1;
    }
  }
  else
  {                                    /* pixel add ! */
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      if (!iM)                         /* mask underflow ? */
      {
        iB = *pWorkrow;                /* get next input-byte */
        pWorkrow++;
        iM = 0x80;
      }

      if (iB & iM)                     /* invert if it is non-zero index */
        *pOutrow = (mng_uint8)(*pOutrow ^ 0x01);

      pOutrow += pData->iColinc;       /* next pixel */
      iM >>= 1;
    }
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_IDX1, MNG_LC_END)
#endif

  return store_idx1 (pData);
}

/* ************************************************************************** */

mng_retcode delta_idx2 (mng_datap pData)
{
  mng_imagedatap pBuf = ((mng_imagep)pData->pDeltaImage)->pImgbuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;
  mng_uint8      iB;
  mng_uint8      iM;
  mng_uint32     iS;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_IDX2, MNG_LC_START)
#endif
                                       /* temporary work pointers */
  pWorkrow = pData->pWorkrow + pData->iPixelofs;
  pOutrow  = pBuf->pImgdata + (pData->iRow         * pBuf->iRowsize   ) +
                              (pData->iDeltaBlocky * pBuf->iRowsize   ) +
                              (pData->iCol         * pBuf->iSamplesize) +
                              (pData->iDeltaBlockx * pBuf->iSamplesize);
  iM       = 0;                        /* start at pixel 0 */
  iB       = 0;
  iS       = 0;
                                       /* pixel replace ? */
  if (pData->iDeltatype == MNG_DELTATYPE_BLOCKPIXELREPLACE)
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      if (!iM)                         /* mask underflow ? */
      {
        iB = *pWorkrow;                /* get next input-byte */
        pWorkrow++;
        iM = 0xC0;
        iS = 6;
      }
                                       /* put the index */
      *pOutrow = (mng_uint8)((iB & iM) >> iS);

      pOutrow += pData->iColinc;       /* next pixel */
      iM >>= 2;
      iS -= 2;
    }
  }
  else
  {                                    /* pixel add ! */
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      if (!iM)                         /* mask underflow ? */
      {
        iB = *pWorkrow;                /* get next input-byte */
        pWorkrow++;
        iM = 0xC0;
        iS = 6;
      }
                                       /* calculate the index */
      *pOutrow = (mng_uint8)((*pOutrow + ((iB & iM) >> iS)) & 0x03);

      pOutrow += pData->iColinc;       /* next pixel */
      iM >>= 2;
      iS -= 2;
    }
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_IDX2, MNG_LC_END)
#endif

  return store_idx2 (pData);
}

/* ************************************************************************** */

mng_retcode delta_idx4 (mng_datap pData)
{
  mng_imagedatap pBuf = ((mng_imagep)pData->pDeltaImage)->pImgbuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;
  mng_uint8      iB;
  mng_uint8      iM;
  mng_uint32     iS;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_IDX4, MNG_LC_START)
#endif
                                       /* temporary work pointers */
  pWorkrow = pData->pWorkrow + pData->iPixelofs;
  pOutrow  = pBuf->pImgdata + (pData->iRow         * pBuf->iRowsize   ) +
                              (pData->iDeltaBlocky * pBuf->iRowsize   ) +
                              (pData->iCol         * pBuf->iSamplesize) +
                              (pData->iDeltaBlockx * pBuf->iSamplesize);
  iM       = 0;                        /* start at pixel 0 */
  iB       = 0;
  iS       = 0;
                                       /* pixel replace ? */
  if (pData->iDeltatype == MNG_DELTATYPE_BLOCKPIXELREPLACE)
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      if (!iM)                         /* mask underflow ? */
      {
        iB = *pWorkrow;                /* get next input-byte */
        pWorkrow++;
        iM = 0xF0;
        iS = 4;
      }
                                       /* put the index */
      *pOutrow = (mng_uint8)((iB & iM) >> iS);

      pOutrow += pData->iColinc;       /* next pixel */
      iM >>= 4;
      iS -= 4;
    }
  }
  else
  {                                    /* pixel add ! */
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      if (!iM)                         /* mask underflow ? */
      {
        iB = *pWorkrow;                /* get next input-byte */
        pWorkrow++;
        iM = 0xF0;
        iS = 4;
      }
                                       /* calculate the index */
      *pOutrow = (mng_uint8)((*pOutrow + ((iB & iM) >> iS)) & 0x0F);

      pOutrow += pData->iColinc;       /* next pixel */
      iM >>= 4;
      iS -= 4;
    }
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_IDX4, MNG_LC_END)
#endif

  return store_idx4 (pData);
}

/* ************************************************************************** */

mng_retcode delta_idx8 (mng_datap pData)
{
  mng_imagedatap pBuf = ((mng_imagep)pData->pDeltaImage)->pImgbuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_IDX8, MNG_LC_START)
#endif
                                       /* temporary work pointers */
  pWorkrow = pData->pWorkrow + pData->iPixelofs;
  pOutrow  = pBuf->pImgdata + (pData->iRow         * pBuf->iRowsize   ) +
                              (pData->iDeltaBlocky * pBuf->iRowsize   ) +
                              (pData->iCol         * pBuf->iSamplesize) +
                              (pData->iDeltaBlockx * pBuf->iSamplesize);
                                       /* pixel replace ? */
  if (pData->iDeltatype == MNG_DELTATYPE_BLOCKPIXELREPLACE)
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      *pOutrow = *pWorkrow;            /* put in object buffer */

      pOutrow += pData->iColinc;       /* next pixel */
      pWorkrow++;
    }
  }
  else
  {                                    /* pixel add ! */
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {                                  /* add to object buffer */
      *pOutrow = (mng_uint8)(*pOutrow + *pWorkrow);

      pOutrow += pData->iColinc;       /* next pixel */
      pWorkrow++;
    }
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_IDX8, MNG_LC_END)
#endif

  return store_idx8 (pData);
}

/* ************************************************************************** */

mng_retcode delta_ga8 (mng_datap pData)
{
  mng_imagedatap pBuf = ((mng_imagep)pData->pDeltaImage)->pImgbuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_GA8, MNG_LC_START)
#endif
                                       /* temporary work pointers */
  pWorkrow = pData->pWorkrow + pData->iPixelofs;
  pOutrow  = pBuf->pImgdata + (pData->iRow         * pBuf->iRowsize   ) +
                              (pData->iDeltaBlocky * pBuf->iRowsize   ) +
                              (pData->iCol         * pBuf->iSamplesize) +
                              (pData->iDeltaBlockx * pBuf->iSamplesize);
                                       /* pixel replace ? */
  if (pData->iDeltatype == MNG_DELTATYPE_BLOCKPIXELREPLACE)
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      *pOutrow     = *pWorkrow;        /* put in object buffer */
      *(pOutrow+1) = *(pWorkrow+1);
                                       /* next pixel */
      pOutrow  += (pData->iColinc << 1);
      pWorkrow += 2;
    }
  }
  else
  {                                    /* pixel add ! */
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {                                  /* add to object buffer */
      *pOutrow     = (mng_uint8)(*pOutrow     + *pWorkrow    );
      *(pOutrow+1) = (mng_uint8)(*(pOutrow+1) + *(pWorkrow+1));
                                       /* next pixel */
      pOutrow  += (pData->iColinc << 1);
      pWorkrow += 2;
    }
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_GA8, MNG_LC_END)
#endif

  return store_ga8 (pData);
}

/* ************************************************************************** */

mng_retcode delta_ga16 (mng_datap pData)
{
  mng_imagedatap pBuf = ((mng_imagep)pData->pDeltaImage)->pImgbuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_GA16, MNG_LC_START)
#endif
                                       /* temporary work pointers */
  pWorkrow = pData->pWorkrow + pData->iPixelofs;
  pOutrow  = pBuf->pImgdata + (pData->iRow         * pBuf->iRowsize   ) +
                              (pData->iDeltaBlocky * pBuf->iRowsize   ) +
                              (pData->iCol         * pBuf->iSamplesize) +
                              (pData->iDeltaBlockx * pBuf->iSamplesize);
                                       /* pixel replace ? */
  if (pData->iDeltatype == MNG_DELTATYPE_BLOCKPIXELREPLACE)
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      *pOutrow     = *pWorkrow;        /* put in object buffer */
      *(pOutrow+1) = *(pWorkrow+1);
      *(pOutrow+2) = *(pWorkrow+2);
      *(pOutrow+3) = *(pWorkrow+3);
                                       /* next pixel */
      pOutrow  += (pData->iColinc << 2);
      pWorkrow += 4;
    }
  }
  else
  {                                    /* pixel add ! */
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {                                  /* add to object buffer */
      mng_put_uint16 (pOutrow,   (mng_uint16)(mng_get_uint16 (pOutrow   ) +
                                              mng_get_uint16 (pWorkrow  )   ));
      mng_put_uint16 (pOutrow+2, (mng_uint16)(mng_get_uint16 (pOutrow+2 ) +
                                              mng_get_uint16 (pWorkrow+2)   ));
                                       /* next pixel */
      pOutrow  += (pData->iColinc << 2);
      pWorkrow += 4;
    }
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_GA16, MNG_LC_END)
#endif

  return store_ga16 (pData);
}

/* ************************************************************************** */

mng_retcode delta_rgba8 (mng_datap pData)
{
  mng_imagedatap pBuf = ((mng_imagep)pData->pDeltaImage)->pImgbuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_RGBA8, MNG_LC_START)
#endif
                                       /* temporary work pointers */
  pWorkrow = pData->pWorkrow + pData->iPixelofs;
  pOutrow  = pBuf->pImgdata + (pData->iRow         * pBuf->iRowsize   ) +
                              (pData->iDeltaBlocky * pBuf->iRowsize   ) +
                              (pData->iCol         * pBuf->iSamplesize) +
                              (pData->iDeltaBlockx * pBuf->iSamplesize);
                                       /* pixel replace ? */
  if (pData->iDeltatype == MNG_DELTATYPE_BLOCKPIXELREPLACE)
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      *pOutrow     = *pWorkrow;        /* put in object buffer */
      *(pOutrow+1) = *(pWorkrow+1);
      *(pOutrow+2) = *(pWorkrow+2);
      *(pOutrow+3) = *(pWorkrow+3);
                                       /* next pixel */
      pOutrow  += (pData->iColinc << 2);
      pWorkrow += 4;
    }
  }
  else
  {                                    /* pixel add ! */
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {                                  /* add to object buffer */
      *pOutrow     = (mng_uint8)(*pOutrow     + *pWorkrow    );
      *(pOutrow+1) = (mng_uint8)(*(pOutrow+1) + *(pWorkrow+1));
      *(pOutrow+2) = (mng_uint8)(*(pOutrow+2) + *(pWorkrow+2));
      *(pOutrow+3) = (mng_uint8)(*(pOutrow+3) + *(pWorkrow+3));
                                       /* next pixel */
      pOutrow  += (pData->iColinc << 2);
      pWorkrow += 4;
    }
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_RGBA8, MNG_LC_END)
#endif

  return store_rgba8 (pData);
}

/* ************************************************************************** */

mng_retcode delta_rgba16 (mng_datap pData)
{
  mng_imagedatap pBuf = ((mng_imagep)pData->pDeltaImage)->pImgbuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_RGBA16, MNG_LC_START)
#endif
                                       /* temporary work pointers */
  pWorkrow = pData->pWorkrow + pData->iPixelofs;
  pOutrow  = pBuf->pImgdata + (pData->iRow         * pBuf->iRowsize   ) +
                              (pData->iDeltaBlocky * pBuf->iRowsize   ) +
                              (pData->iCol         * pBuf->iSamplesize) +
                              (pData->iDeltaBlockx * pBuf->iSamplesize);
                                       /* pixel replace ? */
  if (pData->iDeltatype == MNG_DELTATYPE_BLOCKPIXELREPLACE)
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      MNG_COPY (pOutrow, pWorkrow, 8)  /* put in object buffer */
                                       /* next pixel */
      pOutrow  += (pData->iColinc << 3);
      pWorkrow += 8;
    }
  }
  else
  {                                    /* pixel add ! */
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {                                  /* add to object buffer */
      mng_put_uint16 (pOutrow,   (mng_uint16)(mng_get_uint16 (pOutrow   ) +
                                              mng_get_uint16 (pWorkrow  )   ));
      mng_put_uint16 (pOutrow+2, (mng_uint16)(mng_get_uint16 (pOutrow+2 ) +
                                              mng_get_uint16 (pWorkrow+2)   ));
      mng_put_uint16 (pOutrow+4, (mng_uint16)(mng_get_uint16 (pOutrow+4 ) +
                                              mng_get_uint16 (pWorkrow+4)   ));
      mng_put_uint16 (pOutrow+6, (mng_uint16)(mng_get_uint16 (pOutrow+6 ) +
                                              mng_get_uint16 (pWorkrow+6)   ));
                                       /* next pixel */
      pOutrow  += (pData->iColinc << 3);
      pWorkrow += 8;
    }
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_RGBA16, MNG_LC_END)
#endif

  return store_rgba16 (pData);
}

/* ************************************************************************** */
/* *                                                                        * */
/* * Delta-image row routines - apply the source row onto the target        * */
/* *                                                                        * */
/* ************************************************************************** */

mng_retcode delta_g1_g1 (mng_datap pData)
{
  mng_imagedatap pBuf = ((mng_imagep)pData->pStoreobj)->pImgbuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_G1_G1, MNG_LC_START)
#endif

  pWorkrow = pData->pRGBArow;
  pOutrow  = pBuf->pImgdata + (pData->iRow * pBuf->iRowsize   ) +
                              (pData->iCol * pBuf->iSamplesize);

  if ((pData->iDeltatype == MNG_DELTATYPE_REPLACE          ) ||
      (pData->iDeltatype == MNG_DELTATYPE_BLOCKPIXELREPLACE)    )
  {
    MNG_COPY (pOutrow, pWorkrow, pData->iRowsamples)
  }
  else
  if (pData->iDeltatype == MNG_DELTATYPE_BLOCKPIXELADD)
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      *pOutrow = (mng_uint8)(((mng_uint16)*pOutrow +
                              (mng_uint16)*pWorkrow) & 0x01);

      pOutrow++;
      pWorkrow++;
    }
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_G1_G1, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode delta_g2_g2 (mng_datap pData)
{
  mng_imagedatap pBuf = ((mng_imagep)pData->pStoreobj)->pImgbuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_G2_G2, MNG_LC_START)
#endif

  pWorkrow = pData->pRGBArow;
  pOutrow  = pBuf->pImgdata + (pData->iRow * pBuf->iRowsize   ) +
                              (pData->iCol * pBuf->iSamplesize);

  if ((pData->iDeltatype == MNG_DELTATYPE_REPLACE          ) ||
      (pData->iDeltatype == MNG_DELTATYPE_BLOCKPIXELREPLACE)    )
  {
    MNG_COPY (pOutrow, pWorkrow, pData->iRowsamples)
  }
  else
  if (pData->iDeltatype == MNG_DELTATYPE_BLOCKPIXELADD)
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      *pOutrow = (mng_uint8)(((mng_uint16)*pOutrow +
                              (mng_uint16)*pWorkrow) & 0x03);

      pOutrow++;
      pWorkrow++;
    }
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_G2_G2, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode delta_g4_g4 (mng_datap pData)
{
  mng_imagedatap pBuf = ((mng_imagep)pData->pStoreobj)->pImgbuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_G4_G4, MNG_LC_START)
#endif

  pWorkrow = pData->pRGBArow;
  pOutrow  = pBuf->pImgdata + (pData->iRow * pBuf->iRowsize   ) +
                              (pData->iCol * pBuf->iSamplesize);

  if ((pData->iDeltatype == MNG_DELTATYPE_REPLACE          ) ||
      (pData->iDeltatype == MNG_DELTATYPE_BLOCKPIXELREPLACE)    )
  {
    MNG_COPY (pOutrow, pWorkrow, pData->iRowsamples)
  }
  else
  if (pData->iDeltatype == MNG_DELTATYPE_BLOCKPIXELADD)
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      *pOutrow = (mng_uint8)(((mng_uint16)*pOutrow +
                              (mng_uint16)*pWorkrow) & 0x0F);

      pOutrow++;
      pWorkrow++;
    }
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_G4_G4, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode delta_g8_g8 (mng_datap pData)
{
  mng_imagedatap pBuf = ((mng_imagep)pData->pStoreobj)->pImgbuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_G8_G8, MNG_LC_START)
#endif

  pWorkrow = pData->pRGBArow;
  pOutrow  = pBuf->pImgdata + (pData->iRow * pBuf->iRowsize   ) +
                              (pData->iCol * pBuf->iSamplesize);

  if ((pData->iDeltatype == MNG_DELTATYPE_REPLACE          ) ||
      (pData->iDeltatype == MNG_DELTATYPE_BLOCKPIXELREPLACE)    )
  {
    MNG_COPY (pOutrow, pWorkrow, pData->iRowsamples)
  }
  else
  if (pData->iDeltatype == MNG_DELTATYPE_BLOCKPIXELADD)
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      *pOutrow = (mng_uint8)(((mng_uint16)*pOutrow +
                              (mng_uint16)*pWorkrow) & 0xFF);

      pOutrow++;
      pWorkrow++;
    }
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_G8_G8, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode delta_g16_g16 (mng_datap pData)
{
  mng_imagedatap pBuf = ((mng_imagep)pData->pStoreobj)->pImgbuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_G16_G16, MNG_LC_START)
#endif

  pWorkrow = pData->pRGBArow;
  pOutrow  = pBuf->pImgdata + (pData->iRow * pBuf->iRowsize   ) +
                              (pData->iCol * pBuf->iSamplesize);

  if ((pData->iDeltatype == MNG_DELTATYPE_REPLACE          ) ||
      (pData->iDeltatype == MNG_DELTATYPE_BLOCKPIXELREPLACE)    )
  {
    MNG_COPY (pOutrow, pWorkrow, (pData->iRowsamples << 1))
  }
  else
  if (pData->iDeltatype == MNG_DELTATYPE_BLOCKPIXELADD)
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      mng_put_uint16 (pOutrow, (mng_uint16)((mng_get_uint16 (pOutrow) +
                                             mng_get_uint16 (pWorkrow)) & 0xFFFF));

      pOutrow  += 2;
      pWorkrow += 2;
    }
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_G16_G16, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode delta_rgb8_rgb8 (mng_datap pData)
{
  mng_imagedatap pBuf = ((mng_imagep)pData->pStoreobj)->pImgbuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_RGB8_RGB8, MNG_LC_START)
#endif

  pWorkrow = pData->pRGBArow;
  pOutrow  = pBuf->pImgdata + (pData->iRow * pBuf->iRowsize   ) +
                              (pData->iCol * pBuf->iSamplesize);

  if ((pData->iDeltatype == MNG_DELTATYPE_REPLACE          ) ||
      (pData->iDeltatype == MNG_DELTATYPE_BLOCKPIXELREPLACE)    )
  {
    MNG_COPY (pOutrow, pWorkrow, pData->iRowsamples * 3)
  }
  else
  if (pData->iDeltatype == MNG_DELTATYPE_BLOCKPIXELADD)
  {
    for (iX = 0; iX < (pData->iRowsamples * 3); iX++)
    {
      *pOutrow = (mng_uint8)(((mng_uint16)*pOutrow +
                              (mng_uint16)*pWorkrow) & 0xFF);

      pOutrow++;
      pWorkrow++;
    }
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_RGB8_RGB8, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode delta_rgb16_rgb16 (mng_datap pData)
{
  mng_imagedatap pBuf = ((mng_imagep)pData->pStoreobj)->pImgbuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_RGB16_RGB16, MNG_LC_START)
#endif

  pWorkrow = pData->pRGBArow;
  pOutrow  = pBuf->pImgdata + (pData->iRow * pBuf->iRowsize   ) +
                              (pData->iCol * pBuf->iSamplesize);

  if ((pData->iDeltatype == MNG_DELTATYPE_REPLACE          ) ||
      (pData->iDeltatype == MNG_DELTATYPE_BLOCKPIXELREPLACE)    )
  {
    MNG_COPY (pOutrow, pWorkrow, (pData->iRowsamples * 6))
  }
  else
  if (pData->iDeltatype == MNG_DELTATYPE_BLOCKPIXELADD)
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      mng_put_uint16 (pOutrow,   (mng_uint16)((mng_get_uint16 (pOutrow  ) +
                                               mng_get_uint16 (pWorkrow  )) & 0xFFFF));
      mng_put_uint16 (pOutrow+2, (mng_uint16)((mng_get_uint16 (pOutrow+2) +
                                               mng_get_uint16 (pWorkrow+2)) & 0xFFFF));
      mng_put_uint16 (pOutrow+4, (mng_uint16)((mng_get_uint16 (pOutrow+4) +
                                               mng_get_uint16 (pWorkrow+4)) & 0xFFFF));

      pOutrow  += 6;
      pWorkrow += 6;
    }
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_RGB16_RGB16, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode delta_ga8_ga8 (mng_datap pData)
{
  mng_imagedatap pBuf = ((mng_imagep)pData->pStoreobj)->pImgbuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_GA8_GA8, MNG_LC_START)
#endif

  pWorkrow = pData->pRGBArow;
  pOutrow  = pBuf->pImgdata + (pData->iRow * pBuf->iRowsize   ) +
                              (pData->iCol * pBuf->iSamplesize);

  if ((pData->iDeltatype == MNG_DELTATYPE_REPLACE          ) ||
      (pData->iDeltatype == MNG_DELTATYPE_BLOCKPIXELREPLACE)    )
  {
    MNG_COPY (pOutrow, pWorkrow, pData->iRowsamples << 1)
  }
  else
  if (pData->iDeltatype == MNG_DELTATYPE_BLOCKPIXELADD)
  {
    for (iX = 0; iX < (pData->iRowsamples << 1); iX++)
    {
      *pOutrow = (mng_uint8)(((mng_uint16)*pOutrow +
                              (mng_uint16)*pWorkrow) & 0xFF);

      pOutrow++;
      pWorkrow++;
    }
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_GA8_GA8, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode delta_ga8_g8 (mng_datap pData)
{
  mng_imagedatap pBuf = ((mng_imagep)pData->pStoreobj)->pImgbuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_GA8_G8, MNG_LC_START)
#endif

  pWorkrow = pData->pRGBArow;
  pOutrow  = pBuf->pImgdata + (pData->iRow * pBuf->iRowsize   ) +
                              (pData->iCol * pBuf->iSamplesize);

  if ((pData->iDeltatype == MNG_DELTATYPE_REPLACE          ) ||
      (pData->iDeltatype == MNG_DELTATYPE_BLOCKPIXELREPLACE)    )
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      *pOutrow = *pWorkrow;

      pOutrow += 2;
      pWorkrow++;
    }
  }
  else
  if (pData->iDeltatype == MNG_DELTATYPE_BLOCKPIXELADD)
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      *pOutrow = (mng_uint8)(((mng_uint16)*pOutrow +
                              (mng_uint16)*pWorkrow) & 0xFF);

      pOutrow += 2;
      pWorkrow++;
    }
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_GA8_G8, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode delta_ga8_a8 (mng_datap pData)
{
  mng_imagedatap pBuf = ((mng_imagep)pData->pStoreobj)->pImgbuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_GA8_A8, MNG_LC_START)
#endif

  pWorkrow = pData->pRGBArow;
  pOutrow  = pBuf->pImgdata + (pData->iRow * pBuf->iRowsize   ) +
                              (pData->iCol * pBuf->iSamplesize) + 1;

  if ((pData->iDeltatype == MNG_DELTATYPE_REPLACE          ) ||
      (pData->iDeltatype == MNG_DELTATYPE_BLOCKPIXELREPLACE)    )
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      *pOutrow = *pWorkrow;

      pOutrow += 2;
      pWorkrow++;
    }
  }
  else
  if (pData->iDeltatype == MNG_DELTATYPE_BLOCKPIXELADD)
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      *pOutrow = (mng_uint8)(((mng_uint16)*pOutrow +
                              (mng_uint16)*pWorkrow) & 0xFF);

      pOutrow += 2;
      pWorkrow++;
    }
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_GA8_A8, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode delta_ga16_ga16 (mng_datap pData)
{
  mng_imagedatap pBuf = ((mng_imagep)pData->pStoreobj)->pImgbuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_GA16_GA16, MNG_LC_START)
#endif

  pWorkrow = pData->pRGBArow;
  pOutrow  = pBuf->pImgdata + (pData->iRow * pBuf->iRowsize   ) +
                              (pData->iCol * pBuf->iSamplesize);

  if ((pData->iDeltatype == MNG_DELTATYPE_REPLACE          ) ||
      (pData->iDeltatype == MNG_DELTATYPE_BLOCKPIXELREPLACE)    )
  {
    MNG_COPY (pOutrow, pWorkrow, (pData->iRowsamples << 2))
  }
  else
  if (pData->iDeltatype == MNG_DELTATYPE_BLOCKPIXELADD)
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      mng_put_uint16 (pOutrow,   (mng_uint16)((mng_get_uint16 (pOutrow  ) +
                                               mng_get_uint16 (pWorkrow  )) & 0xFFFF));
      mng_put_uint16 (pOutrow+2, (mng_uint16)((mng_get_uint16 (pOutrow+2) +
                                               mng_get_uint16 (pWorkrow+2)) & 0xFFFF));

      pOutrow  += 4;
      pWorkrow += 4;
    }
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_GA16_GA16, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode delta_ga16_g16 (mng_datap pData)
{
  mng_imagedatap pBuf = ((mng_imagep)pData->pStoreobj)->pImgbuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_GA16_A16, MNG_LC_START)
#endif

  pWorkrow = pData->pRGBArow;
  pOutrow  = pBuf->pImgdata + (pData->iRow * pBuf->iRowsize   ) +
                              (pData->iCol * pBuf->iSamplesize);

  if ((pData->iDeltatype == MNG_DELTATYPE_REPLACE          ) ||
      (pData->iDeltatype == MNG_DELTATYPE_BLOCKPIXELREPLACE)    )
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      mng_put_uint16 (pOutrow, mng_get_uint16 (pWorkrow));

      pOutrow  += 4;
      pWorkrow += 2;
    }
  }
  else
  if (pData->iDeltatype == MNG_DELTATYPE_BLOCKPIXELADD)
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      mng_put_uint16 (pOutrow, (mng_uint16)((mng_get_uint16 (pOutrow) +
                                             mng_get_uint16 (pWorkrow)) & 0xFFFF));

      pOutrow  += 4;
      pWorkrow += 2;
    }
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_GA16_A16, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode delta_ga16_a16 (mng_datap pData)
{
  mng_imagedatap pBuf = ((mng_imagep)pData->pStoreobj)->pImgbuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_GA16_G16, MNG_LC_START)
#endif

  pWorkrow = pData->pRGBArow;
  pOutrow  = pBuf->pImgdata + (pData->iRow * pBuf->iRowsize   ) +
                              (pData->iCol * pBuf->iSamplesize);

  if ((pData->iDeltatype == MNG_DELTATYPE_REPLACE          ) ||
      (pData->iDeltatype == MNG_DELTATYPE_BLOCKPIXELREPLACE)    )
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      mng_put_uint16 (pOutrow+2, mng_get_uint16 (pWorkrow));

      pOutrow  += 4;
      pWorkrow += 2;
    }
  }
  else
  if (pData->iDeltatype == MNG_DELTATYPE_BLOCKPIXELADD)
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      mng_put_uint16 (pOutrow+2, (mng_uint16)((mng_get_uint16 (pOutrow+2) +
                                               mng_get_uint16 (pWorkrow)) & 0xFFFF));

      pOutrow  += 4;
      pWorkrow += 2;
    }
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_GA16_G16, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode delta_rgba8_rgba8 (mng_datap pData)
{
  mng_imagedatap pBuf = ((mng_imagep)pData->pStoreobj)->pImgbuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_RGBA8_RGBA8, MNG_LC_START)
#endif

  pWorkrow = pData->pRGBArow;
  pOutrow  = pBuf->pImgdata + (pData->iRow * pBuf->iRowsize   ) +
                              (pData->iCol * pBuf->iSamplesize);

  if ((pData->iDeltatype == MNG_DELTATYPE_REPLACE          ) ||
      (pData->iDeltatype == MNG_DELTATYPE_BLOCKPIXELREPLACE)    )
  {
    MNG_COPY (pOutrow, pWorkrow, pData->iRowsamples << 2)
  }
  else
  if (pData->iDeltatype == MNG_DELTATYPE_BLOCKPIXELADD)
  {
    for (iX = 0; iX < (pData->iRowsamples << 2); iX++)
    {
      *pOutrow = (mng_uint8)(((mng_uint16)*pOutrow +
                              (mng_uint16)*pWorkrow) & 0xFF);

      pOutrow++;
      pWorkrow++;
    }
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_RGBA8_RGBA8, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode delta_rgba8_rgb8 (mng_datap pData)
{
  mng_imagedatap pBuf = ((mng_imagep)pData->pStoreobj)->pImgbuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_RGBA8_RGB8, MNG_LC_START)
#endif

  pWorkrow = pData->pRGBArow;
  pOutrow  = pBuf->pImgdata + (pData->iRow * pBuf->iRowsize   ) +
                              (pData->iCol * pBuf->iSamplesize);

  if ((pData->iDeltatype == MNG_DELTATYPE_REPLACE          ) ||
      (pData->iDeltatype == MNG_DELTATYPE_BLOCKPIXELREPLACE)    )
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      *pOutrow     = *pWorkrow;
      *(pOutrow+1) = *(pWorkrow+1);
      *(pOutrow+2) = *(pWorkrow+2);

      pOutrow  += 4;
      pWorkrow += 3;
    }
  }
  else
  if (pData->iDeltatype == MNG_DELTATYPE_BLOCKPIXELADD)
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      *pOutrow     = (mng_uint8)(((mng_uint16)*pOutrow     +
                                  (mng_uint16)*pWorkrow    ) & 0xFF);
      *(pOutrow+1) = (mng_uint8)(((mng_uint16)*(pOutrow+1) +
                                  (mng_uint16)*(pWorkrow+1)) & 0xFF);
      *(pOutrow+2) = (mng_uint8)(((mng_uint16)*(pOutrow+2) +
                                  (mng_uint16)*(pWorkrow+2)) & 0xFF);

      pOutrow  += 4;
      pWorkrow += 3;
    }
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_RGBA8_RGB8, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode delta_rgba8_a8 (mng_datap pData)
{
  mng_imagedatap pBuf = ((mng_imagep)pData->pStoreobj)->pImgbuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_RGBA8_A8, MNG_LC_START)
#endif

  pWorkrow = pData->pRGBArow;
  pOutrow  = pBuf->pImgdata + (pData->iRow * pBuf->iRowsize   ) +
                              (pData->iCol * pBuf->iSamplesize) + 3;

  if ((pData->iDeltatype == MNG_DELTATYPE_REPLACE          ) ||
      (pData->iDeltatype == MNG_DELTATYPE_BLOCKPIXELREPLACE)    )
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      *pOutrow = *pWorkrow;

      pOutrow += 4;
      pWorkrow++;
    }
  }
  else
  if (pData->iDeltatype == MNG_DELTATYPE_BLOCKPIXELADD)
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      *pOutrow = (mng_uint8)(((mng_uint16)*pOutrow +
                              (mng_uint16)*pWorkrow) & 0xFF);

      pOutrow += 4;
      pWorkrow++;
    }
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_RGBA8_A8, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode delta_rgba16_rgba16 (mng_datap pData)
{
  mng_imagedatap pBuf = ((mng_imagep)pData->pStoreobj)->pImgbuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_RGBA16_RGBA16, MNG_LC_START)
#endif

  pWorkrow = pData->pRGBArow;
  pOutrow  = pBuf->pImgdata + (pData->iRow * pBuf->iRowsize   ) +
                              (pData->iCol * pBuf->iSamplesize);

  if ((pData->iDeltatype == MNG_DELTATYPE_REPLACE          ) ||
      (pData->iDeltatype == MNG_DELTATYPE_BLOCKPIXELREPLACE)    )
  {
    MNG_COPY (pOutrow, pWorkrow, (pData->iRowsamples << 3))
  }
  else
  if (pData->iDeltatype == MNG_DELTATYPE_BLOCKPIXELADD)
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      mng_put_uint16 (pOutrow,   (mng_uint16)((mng_get_uint16 (pOutrow  ) +
                                               mng_get_uint16 (pWorkrow  )) & 0xFFFF));
      mng_put_uint16 (pOutrow+2, (mng_uint16)((mng_get_uint16 (pOutrow+2) +
                                               mng_get_uint16 (pWorkrow+2)) & 0xFFFF));
      mng_put_uint16 (pOutrow+4, (mng_uint16)((mng_get_uint16 (pOutrow+4) +
                                               mng_get_uint16 (pWorkrow+4)) & 0xFFFF));
      mng_put_uint16 (pOutrow+6, (mng_uint16)((mng_get_uint16 (pOutrow+6) +
                                               mng_get_uint16 (pWorkrow+6)) & 0xFFFF));

      pOutrow  += 8;
      pWorkrow += 8;
    }
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_RGBA16_RGBA16, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode delta_rgba16_rgb16 (mng_datap pData)
{
  mng_imagedatap pBuf = ((mng_imagep)pData->pStoreobj)->pImgbuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_RGBA16_RGB16, MNG_LC_START)
#endif

  pWorkrow = pData->pRGBArow;
  pOutrow  = pBuf->pImgdata + (pData->iRow * pBuf->iRowsize   ) +
                              (pData->iCol * pBuf->iSamplesize);

  if ((pData->iDeltatype == MNG_DELTATYPE_REPLACE          ) ||
      (pData->iDeltatype == MNG_DELTATYPE_BLOCKPIXELREPLACE)    )
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      mng_put_uint16 (pOutrow,   mng_get_uint16 (pWorkrow  ));
      mng_put_uint16 (pOutrow+2, mng_get_uint16 (pWorkrow+2));
      mng_put_uint16 (pOutrow+4, mng_get_uint16 (pWorkrow+4));

      pOutrow  += 8;
      pWorkrow += 6;
    }
  }
  else
  if (pData->iDeltatype == MNG_DELTATYPE_BLOCKPIXELADD)
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      mng_put_uint16 (pOutrow,   (mng_uint16)((mng_get_uint16 (pOutrow  ) +
                                               mng_get_uint16 (pWorkrow  )) & 0xFFFF));
      mng_put_uint16 (pOutrow+2, (mng_uint16)((mng_get_uint16 (pOutrow+2) +
                                               mng_get_uint16 (pWorkrow+2)) & 0xFFFF));
      mng_put_uint16 (pOutrow+4, (mng_uint16)((mng_get_uint16 (pOutrow+4) +
                                               mng_get_uint16 (pWorkrow+4)) & 0xFFFF));

      pOutrow  += 8;
      pWorkrow += 6;
    }
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_RGBA16_RGB16, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode delta_rgba16_a16 (mng_datap pData)
{
  mng_imagedatap pBuf = ((mng_imagep)pData->pStoreobj)->pImgbuf;
  mng_uint8p     pWorkrow;
  mng_uint8p     pOutrow;
  mng_int32      iX;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_RGBA16_A16, MNG_LC_START)
#endif

  pWorkrow = pData->pRGBArow;
  pOutrow  = pBuf->pImgdata + (pData->iRow * pBuf->iRowsize   ) +
                              (pData->iCol * pBuf->iSamplesize);

  if ((pData->iDeltatype == MNG_DELTATYPE_REPLACE          ) ||
      (pData->iDeltatype == MNG_DELTATYPE_BLOCKPIXELREPLACE)    )
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      mng_put_uint16 (pOutrow+6, mng_get_uint16 (pWorkrow));

      pOutrow  += 8;
      pWorkrow += 2;
    }
  }
  else
  if (pData->iDeltatype == MNG_DELTATYPE_BLOCKPIXELADD)
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      mng_put_uint16 (pOutrow+6, (mng_uint16)((mng_get_uint16 (pOutrow+6) +
                                               mng_get_uint16 (pWorkrow)) & 0xFFFF));

      pOutrow  += 8;
      pWorkrow += 2;
    }
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DELTA_RGBA16_A16, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */
/* *                                                                        * */
/* * Delta-image row routines - scale the source to bitdepth of target      * */
/* *                                                                        * */
/* ************************************************************************** */



/* ************************************************************************** */
/* *                                                                        * */
/* * Row processing routines - convert uncompressed data from zlib to       * */
/* * managable row-data which serves as input to the color-management       * */
/* * routines                                                               * */
/* *                                                                        * */
/* ************************************************************************** */

mng_retcode process_g1 (mng_datap pData)
{
  mng_uint8p     pWorkrow;
  mng_uint8p     pRGBArow;
  mng_int32      iX;
  mng_uint8      iB;
  mng_uint8      iM;
  mng_imagedatap pBuf = (mng_imagedatap)pData->pStorebuf;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_G1, MNG_LC_START)
#endif

  if (!pBuf)                           /* no object? then use obj 0 */
    pBuf = ((mng_imagep)pData->pObjzero)->pImgbuf;
                                       /* temporary work pointers */
  pWorkrow = pData->pWorkrow + pData->iPixelofs;
  pRGBArow = pData->pRGBArow;
  iM       = 0;                        /* start at pixel 0 */
  iB       = 0;

  if (pBuf->bHasTRNS)                  /* tRNS encountered ? */
  {
    if (pBuf->iTRNSgray)               /* white transparent ? */
    {
      for (iX = 0; iX < pData->iRowsamples; iX++)
      {
        if (!iM)                       /* mask underflow ? */
        {
          iB = *pWorkrow;              /* get next input-byte */
          pWorkrow++;
          iM = 0x80;
        }

        if (iB & iM)                   /* is it white ? */
                                       /* transparent ! */
          mng_put_uint32 (pRGBArow, 0x00000000);
        else                           /* opaque black */
          mng_put_uint32 (pRGBArow, 0x000000FF);

        pRGBArow += 4;                 /* next pixel */
        iM >>= 1;
      }
    }
    else                               /* black transparent */
    {
      for (iX = 0; iX < pData->iRowsamples; iX++)
      {
        if (!iM)                       /* mask underflow ? */
        {
          iB = *pWorkrow;              /* get next input-byte */
          pWorkrow++;
          iM = 0x80;
        }

        if (iB & iM)                   /* is it white ? */
                                       /* opaque white */
          mng_put_uint32 (pRGBArow, 0xFFFFFFFF);
        else                           /* transparent */
          mng_put_uint32 (pRGBArow, 0x00000000);

        pRGBArow += 4;                 /* next pixel */
        iM >>= 1;
      }
    }

    pData->bIsOpaque = MNG_FALSE;      /* it's not fully opaque */
  }
  else                                 /* no transparency */
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      if (!iM)                         /* mask underflow ? */
      {
        iB = *pWorkrow;                /* get next input-byte */
        pWorkrow++;
        iM = 0x80;
      }

      if (iB & iM)                     /* is it white ? */
                                       /* opaque white */
        mng_put_uint32 (pRGBArow, 0xFFFFFFFF);
      else                             /* opaque black */
        mng_put_uint32 (pRGBArow, 0x000000FF);

      pRGBArow += 4;                   /* next pixel */
      iM >>= 1;
    }

    pData->bIsOpaque = MNG_TRUE;       /* it's fully opaque */
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_G1, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode process_g2 (mng_datap pData)
{
  mng_uint8p     pWorkrow;
  mng_uint8p     pRGBArow;
  mng_int32      iX;
  mng_uint8      iB;
  mng_uint8      iM;
  mng_uint32     iS;
  mng_uint8      iQ;
  mng_imagedatap pBuf = (mng_imagedatap)pData->pStorebuf;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_G2, MNG_LC_START)
#endif

  if (!pBuf)                           /* no object? then use obj 0 */
    pBuf = ((mng_imagep)pData->pObjzero)->pImgbuf;
                                       /* temporary work pointers */
  pWorkrow = pData->pWorkrow + pData->iPixelofs;
  pRGBArow = pData->pRGBArow;
  iM       = 0;                        /* start at pixel 0 */
  iB       = 0;
  iS       = 0;

  if (pBuf->bHasTRNS)                  /* tRNS encountered ? */
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      if (!iM)                         /* mask underflow ? */
      {
        iB = *pWorkrow;                /* get next input-byte */
        pWorkrow++;
        iM = 0xC0;
        iS = 6;
      }
                                       /* determine gray level */
      iQ = (mng_uint8)((iB & iM) >> iS);

      if (iQ == pBuf->iTRNSgray)       /* transparent ? */
        mng_put_uint32 (pRGBArow, 0x00000000);
      else
      {
        switch (iQ)                    /* determine the gray level */
        {
          case 0x03 : { mng_put_uint32 (pRGBArow, 0xFFFFFFFF); break; }
          case 0x02 : { mng_put_uint32 (pRGBArow, 0xAAAAAAFF); break; }
          case 0x01 : { mng_put_uint32 (pRGBArow, 0x555555FF); break; }
          default   : { mng_put_uint32 (pRGBArow, 0x000000FF); }
        }
      }

      pRGBArow += 4;                   /* next pixel */
      iM >>= 2;
      iS -= 2;
    }

    pData->bIsOpaque = MNG_FALSE;      /* it's not fully opaque */
  }
  else
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      if (!iM)                         /* mask underflow ? */
      {
        iB = *pWorkrow;                /* get next input-byte */
        pWorkrow++;
        iM = 0xC0;
        iS = 6;
      }

      switch ((iB & iM) >> iS)         /* determine the gray level */
      {
        case 0x03 : { mng_put_uint32 (pRGBArow, 0xFFFFFFFF); break; }
        case 0x02 : { mng_put_uint32 (pRGBArow, 0xAAAAAAFF); break; }
        case 0x01 : { mng_put_uint32 (pRGBArow, 0x555555FF); break; }
        default   : { mng_put_uint32 (pRGBArow, 0x000000FF); }
      }

      pRGBArow += 4;                   /* next pixel */
      iM >>= 2;
      iS -= 2;
    }

    pData->bIsOpaque = MNG_TRUE;       /* it's fully opaque */
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_G2, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode process_g4 (mng_datap pData)
{
  mng_uint8p     pWorkrow;
  mng_uint8p     pRGBArow;
  mng_int32      iX;
  mng_uint8      iB;
  mng_uint8      iM;
  mng_uint32     iS;
  mng_uint8      iQ;
  mng_imagedatap pBuf = (mng_imagedatap)pData->pStorebuf;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_G4, MNG_LC_START)
#endif

  if (!pBuf)                           /* no object? then use obj 0 */
    pBuf = ((mng_imagep)pData->pObjzero)->pImgbuf;
                                       /* temporary work pointers */
  pWorkrow = pData->pWorkrow + pData->iPixelofs;
  pRGBArow = pData->pRGBArow;
  iM       = 0;                        /* start at pixel 0 */
  iB       = 0;
  iS       = 0;

  if (pBuf->bHasTRNS)                  /* tRNS encountered ? */
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      if (!iM)                         /* mask underflow ? */
      {
        iB = *pWorkrow;                /* get next input-byte */
        pWorkrow++;
        iM = 0xF0;
        iS = 4;
      }
                                       /* get the gray level */
      iQ = (mng_uint8)((iB & iM) >> iS);

      if (iQ == pBuf->iTRNSgray)       /* transparent ? */
      {
        *pRGBArow     = 0;             /* put in intermediate row */
        *(pRGBArow+1) = 0;
        *(pRGBArow+2) = 0;
        *(pRGBArow+3) = 0;
      }
      else
      {                                /* expand to 8-bit by replication */
        iQ = (mng_uint8)(iQ + (iQ << 4));

        *pRGBArow     = iQ;            /* put in intermediate row */
        *(pRGBArow+1) = iQ;
        *(pRGBArow+2) = iQ;
        *(pRGBArow+3) = 0xFF;
      }

      pRGBArow += 4;                   /* next pixel */
      iM >>= 4;
      iS -= 4;
    }

    pData->bIsOpaque = MNG_FALSE;      /* it's not fully opaque */
  }
  else
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      if (!iM)                         /* mask underflow ? */
      {
        iB = *pWorkrow;                /* get next input-byte */
        pWorkrow++;
        iM = 0xF0;
        iS = 4;
      }
                                       /* get the gray level */
      iQ = (mng_uint8)((iB & iM) >> iS);
      iQ = (mng_uint8)(iQ + (iQ << 4));/* expand to 8-bit by replication */

      *pRGBArow     = iQ;              /* put in intermediate row */
      *(pRGBArow+1) = iQ;
      *(pRGBArow+2) = iQ;
      *(pRGBArow+3) = 0xFF;

      pRGBArow += 4;                   /* next pixel */
      iM >>= 4;
      iS -= 4;
    }

    pData->bIsOpaque = MNG_TRUE;       /* it's fully opaque */
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_G4, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode process_g8 (mng_datap pData)
{
  mng_uint8p     pWorkrow;
  mng_uint8p     pRGBArow;
  mng_int32      iX;
  mng_uint8      iB;
  mng_imagedatap pBuf = (mng_imagedatap)pData->pStorebuf;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_G8, MNG_LC_START)
#endif

  if (!pBuf)                           /* no object? then use obj 0 */
    pBuf = ((mng_imagep)pData->pObjzero)->pImgbuf;
                                       /* temporary work pointers */
  pWorkrow = pData->pWorkrow + pData->iPixelofs;
  pRGBArow = pData->pRGBArow;

  if (pBuf->bHasTRNS)                  /* tRNS encountered ? */
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      iB = *pWorkrow;                  /* get next input-byte */

      if (iB == pBuf->iTRNSgray)       /* transparent ? */
      {
        *pRGBArow     = 0;             /* put in intermediate row */
        *(pRGBArow+1) = 0;
        *(pRGBArow+2) = 0;
        *(pRGBArow+3) = 0;
      }
      else
      {
        *pRGBArow     = iB;            /* put in intermediate row */
        *(pRGBArow+1) = iB;
        *(pRGBArow+2) = iB;
        *(pRGBArow+3) = 0xFF;
      }

      pRGBArow += 4;                   /* next pixel */
      pWorkrow++;
    }

    pData->bIsOpaque = MNG_FALSE;      /* it's not fully opaque */
  }
  else
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      iB = *pWorkrow;                  /* get next input-byte */

      *pRGBArow     = iB;              /* put in intermediate row */
      *(pRGBArow+1) = iB;
      *(pRGBArow+2) = iB;
      *(pRGBArow+3) = 0xFF;

      pRGBArow += 4;                   /* next pixel */
      pWorkrow++;
    }

    pData->bIsOpaque = MNG_TRUE;       /* it's fully opaque */
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_G8, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode process_g16 (mng_datap pData)
{
  mng_uint8p     pWorkrow;
  mng_uint8p     pRGBArow;
  mng_int32      iX;
  mng_uint16     iW;
  mng_imagedatap pBuf = (mng_imagedatap)pData->pStorebuf;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_G16, MNG_LC_START)
#endif

  if (!pBuf)                           /* no object? then use obj 0 */
    pBuf = ((mng_imagep)pData->pObjzero)->pImgbuf;
                                       /* temporary work pointers */
  pWorkrow = pData->pWorkrow + pData->iPixelofs;
  pRGBArow = pData->pRGBArow;

  if (pBuf->bHasTRNS)                  /* tRNS encountered ? */
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      iW = mng_get_uint16 (pWorkrow);  /* get input */

      if (iW == pBuf->iTRNSgray)       /* transparent ? */
      {                                /* put in intermediate row */
        mng_put_uint16 (pRGBArow,   0);
        mng_put_uint16 (pRGBArow+2, 0);
        mng_put_uint16 (pRGBArow+4, 0);
        mng_put_uint16 (pRGBArow+6, 0);
      }
      else
      {                                /* put in intermediate row */
        mng_put_uint16 (pRGBArow,   iW);
        mng_put_uint16 (pRGBArow+2, iW);
        mng_put_uint16 (pRGBArow+4, iW);
        mng_put_uint16 (pRGBArow+6, 0xFFFF);
      }

      pRGBArow += 8;                   /* next pixel */
      pWorkrow += 2;
    }

    pData->bIsOpaque = MNG_FALSE;      /* it's not fully opaque */
  }
  else
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      iW = mng_get_uint16 (pWorkrow);  /* get input */

      mng_put_uint16 (pRGBArow,   iW); /* and put in intermediate row */
      mng_put_uint16 (pRGBArow+2, iW);
      mng_put_uint16 (pRGBArow+4, iW);
      mng_put_uint16 (pRGBArow+6, 0xFFFF);

      pRGBArow += 8;                   /* next pixel */
      pWorkrow += 2;
    }

    pData->bIsOpaque = MNG_TRUE;       /* it's fully opaque */
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_G16, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode process_rgb8 (mng_datap pData)
{
  mng_uint8p     pWorkrow;
  mng_uint8p     pRGBArow;
  mng_int32      iX;
  mng_uint8      iR, iG, iB;
  mng_imagedatap pBuf = (mng_imagedatap)pData->pStorebuf;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_RGB8, MNG_LC_START)
#endif

  if (!pBuf)                           /* no object? then use obj 0 */
    pBuf = ((mng_imagep)pData->pObjzero)->pImgbuf;
                                       /* temporary work pointers */
  pWorkrow = pData->pWorkrow + pData->iPixelofs;
  pRGBArow = pData->pRGBArow;

  if (pBuf->bHasTRNS)                  /* tRNS encountered ? */
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      iR = *pWorkrow;                  /* get the RGB values */
      iG = *(pWorkrow+1);
      iB = *(pWorkrow+2);
                                       /* transparent ? */
      if ((iR == pBuf->iTRNSred) && (iG == pBuf->iTRNSgreen) &&
          (iB == pBuf->iTRNSblue))
      {
        *pRGBArow     = 0;             /* this pixel is transparent ! */
        *(pRGBArow+1) = 0;
        *(pRGBArow+2) = 0;
        *(pRGBArow+3) = 0;
      }
      else
      {
        *pRGBArow     = iR;            /* copy the RGB values */
        *(pRGBArow+1) = iG;
        *(pRGBArow+2) = iB;
        *(pRGBArow+3) = 0xFF;          /* this one isn't transparent */
      }

      pWorkrow += 3;                   /* next pixel */
      pRGBArow += 4;
    }

    pData->bIsOpaque = MNG_FALSE;      /* it's not fully opaque */
  }
  else
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      *pRGBArow     = *pWorkrow;       /* copy the RGB bytes */
      *(pRGBArow+1) = *(pWorkrow+1);
      *(pRGBArow+2) = *(pWorkrow+2);
      *(pRGBArow+3) = 0xFF;            /* no alpha; so always fully opaque */

      pWorkrow += 3;                   /* next pixel */
      pRGBArow += 4;
    }

    pData->bIsOpaque = MNG_TRUE;       /* it's fully opaque */
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_RGB8, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode process_rgb16 (mng_datap pData)
{
  mng_uint8p     pWorkrow;
  mng_uint8p     pRGBArow;
  mng_int32      iX;
  mng_uint16     iR, iG, iB;
  mng_imagedatap pBuf = (mng_imagedatap)pData->pStorebuf;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_RGB16, MNG_LC_START)
#endif

  if (!pBuf)                           /* no object? then use obj 0 */
    pBuf = ((mng_imagep)pData->pObjzero)->pImgbuf;
                                       /* temporary work pointers */
  pWorkrow = pData->pWorkrow + pData->iPixelofs;
  pRGBArow = pData->pRGBArow;

  if (pBuf->bHasTRNS)                  /* tRNS encountered ? */
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      iR = mng_get_uint16 (pWorkrow);  /* get the RGB values */
      iG = mng_get_uint16 (pWorkrow+2);
      iB = mng_get_uint16 (pWorkrow+4);
                                       /* transparent ? */
      if ((iR == pBuf->iTRNSred) && (iG == pBuf->iTRNSgreen) &&
          (iB == pBuf->iTRNSblue))
      {                                /* transparent then */
        mng_put_uint16 (pRGBArow,   0);
        mng_put_uint16 (pRGBArow+2, 0);
        mng_put_uint16 (pRGBArow+4, 0);
        mng_put_uint16 (pRGBArow+6, 0);
      }
      else
      {                                /* put in intermediate row */
        mng_put_uint16 (pRGBArow,   iR);
        mng_put_uint16 (pRGBArow+2, iG);
        mng_put_uint16 (pRGBArow+4, iB);
        mng_put_uint16 (pRGBArow+6, 0xFFFF);
      }

      pWorkrow += 6;                   /* next pixel */
      pRGBArow += 8;
    }

    pData->bIsOpaque = MNG_FALSE;      /* it's not fully opaque */
  }
  else
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {                                  /* copy the RGB values */
      mng_put_uint16 (pRGBArow,   mng_get_uint16 (pWorkrow  ));
      mng_put_uint16 (pRGBArow+2, mng_get_uint16 (pWorkrow+2));
      mng_put_uint16 (pRGBArow+4, mng_get_uint16 (pWorkrow+4));
      mng_put_uint16 (pRGBArow+6, 0xFFFF);

      pWorkrow += 6;                   /* next pixel */
      pRGBArow += 8;
    }

    pData->bIsOpaque = MNG_TRUE;       /* it's fully opaque */
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_RGB16, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode process_idx1 (mng_datap pData)
{
  mng_uint8p     pWorkrow;
  mng_uint8p     pRGBArow;
  mng_int32      iX;
  mng_uint8      iB;
  mng_uint8      iM;
  mng_uint32     iS;
  mng_uint8      iQ;
  mng_imagedatap pBuf = (mng_imagedatap)pData->pStorebuf;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_IDX1, MNG_LC_START)
#endif

  if (!pBuf)                           /* no object? then use obj 0 */
    pBuf = ((mng_imagep)pData->pObjzero)->pImgbuf;
                                       /* temporary work pointers */
  pWorkrow = pData->pWorkrow + pData->iPixelofs;
  pRGBArow = pData->pRGBArow;
  iM       = 0;                        /* start at pixel 0 */
  iB       = 0;
  iS       = 0;

  if (pBuf->bHasTRNS)                  /* tRNS encountered ? */
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      if (!iM)                         /* mask underflow ? */
      {
        iB = *pWorkrow;                /* get next input-byte */
        pWorkrow++;
        iM = 0x80;
        iS = 7;
      }
                                       /* get the index */
      iQ = (mng_uint8)((iB & iM) >> iS);
                                       /* index valid ? */
      if ((mng_uint32)iQ < pBuf->iPLTEcount)
      {                                /* put in intermediate row */
        *pRGBArow     = pBuf->aPLTEentries [iQ].iRed;
        *(pRGBArow+1) = pBuf->aPLTEentries [iQ].iGreen;
        *(pRGBArow+2) = pBuf->aPLTEentries [iQ].iBlue;
                                       /* transparency for this index ? */
        if ((mng_uint32)iQ < pBuf->iTRNScount)
          *(pRGBArow+3) = pBuf->aTRNSentries [iQ];
        else
          *(pRGBArow+3) = 0xFF;
      }
      else
        MNG_ERROR (pData, MNG_PLTEINDEXERROR)

      pRGBArow += 4;                   /* next pixel */
      iM >>= 1;
      iS -= 1;
    }

    pData->bIsOpaque = MNG_FALSE;      /* it's not fully opaque */
  }
  else
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      if (!iM)                         /* mask underflow ? */
      {
        iB = *pWorkrow;                /* get next input-byte */
        pWorkrow++;
        iM = 0x80;
        iS = 7;
      }
                                       /* get the index */
      iQ = (mng_uint8)((iB & iM) >> iS);
                                       /* index valid ? */
      if ((mng_uint32)iQ < pBuf->iPLTEcount)
      {                                /* put in intermediate row */
        *pRGBArow     = pBuf->aPLTEentries [iQ].iRed;
        *(pRGBArow+1) = pBuf->aPLTEentries [iQ].iGreen;
        *(pRGBArow+2) = pBuf->aPLTEentries [iQ].iBlue;
        *(pRGBArow+3) = 0xFF;
      }
      else
        MNG_ERROR (pData, MNG_PLTEINDEXERROR)

      pRGBArow += 4;                   /* next pixel */
      iM >>= 1;
      iS -= 1;
    }

    pData->bIsOpaque = MNG_TRUE;       /* it's fully opaque */
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_IDX1, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode process_idx2 (mng_datap pData)
{
  mng_uint8p     pWorkrow;
  mng_uint8p     pRGBArow;
  mng_int32      iX;
  mng_uint8      iB;
  mng_uint8      iM;
  mng_uint32     iS;
  mng_uint8      iQ;
  mng_imagedatap pBuf = (mng_imagedatap)pData->pStorebuf;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_IDX2, MNG_LC_START)
#endif

  if (!pBuf)                           /* no object? then use obj 0 */
    pBuf = ((mng_imagep)pData->pObjzero)->pImgbuf;
                                       /* temporary work pointers */
  pWorkrow = pData->pWorkrow + pData->iPixelofs;
  pRGBArow = pData->pRGBArow;
  iM       = 0;                        /* start at pixel 0 */
  iB       = 0;
  iS       = 0;

  if (pBuf->bHasTRNS)                  /* tRNS encountered ? */
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      if (!iM)                         /* mask underflow ? */
      {
        iB = *pWorkrow;                /* get next input-byte */
        pWorkrow++;
        iM = 0xC0;
        iS = 6;
      }
                                       /* get the index */
      iQ = (mng_uint8)((iB & iM) >> iS);
                                       /* index valid ? */
      if ((mng_uint32)iQ < pBuf->iPLTEcount)
      {                                /* put in intermediate row */
        *pRGBArow     = pBuf->aPLTEentries [iQ].iRed;
        *(pRGBArow+1) = pBuf->aPLTEentries [iQ].iGreen;
        *(pRGBArow+2) = pBuf->aPLTEentries [iQ].iBlue;
                                       /* transparency for this index ? */
        if ((mng_uint32)iQ < pBuf->iTRNScount)
          *(pRGBArow+3) = pBuf->aTRNSentries [iQ];
        else
          *(pRGBArow+3) = 0xFF;
      }
      else
        MNG_ERROR (pData, MNG_PLTEINDEXERROR)

      pRGBArow += 4;                   /* next pixel */
      iM >>= 2;
      iS -= 2;
    }

    pData->bIsOpaque = MNG_FALSE;      /* it's not fully opaque */
  }
  else
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      if (!iM)                         /* mask underflow ? */
      {
        iB = *pWorkrow;                /* get next input-byte */
        pWorkrow++;
        iM = 0xC0;
        iS = 6;
      }
                                       /* get the index */
      iQ = (mng_uint8)((iB & iM) >> iS);
                                       /* index valid ? */
      if ((mng_uint32)iQ < pBuf->iPLTEcount)
      {                                /* put in intermediate row */
        *pRGBArow     = pBuf->aPLTEentries [iQ].iRed;
        *(pRGBArow+1) = pBuf->aPLTEentries [iQ].iGreen;
        *(pRGBArow+2) = pBuf->aPLTEentries [iQ].iBlue;
        *(pRGBArow+3) = 0xFF;
      }
      else
        MNG_ERROR (pData, MNG_PLTEINDEXERROR)

      pRGBArow += 4;                   /* next pixel */
      iM >>= 2;
      iS -= 2;
    }

    pData->bIsOpaque = MNG_TRUE;       /* it's fully opaque */
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_IDX2, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode process_idx4 (mng_datap pData)
{
  mng_uint8p     pWorkrow;
  mng_uint8p     pRGBArow;
  mng_int32      iX;
  mng_uint8      iB;
  mng_uint8      iM;
  mng_uint32     iS;
  mng_uint8      iQ;
  mng_imagedatap pBuf = (mng_imagedatap)pData->pStorebuf;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_IDX4, MNG_LC_START)
#endif

  if (!pBuf)                           /* no object? then use obj 0 */
    pBuf = ((mng_imagep)pData->pObjzero)->pImgbuf;
                                       /* temporary work pointers */
  pWorkrow = pData->pWorkrow + pData->iPixelofs;
  pRGBArow = pData->pRGBArow;
  iM       = 0;                        /* start at pixel 0 */
  iB       = 0;
  iS       = 0;

  if (pBuf->bHasTRNS)                  /* tRNS encountered ? */
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      if (!iM)                         /* mask underflow ? */
      {
        iB = pWorkrow [0];             /* get next input-byte */
        pWorkrow++;
        iM = 0xF0;
        iS = 4;
      }
                                       /* get the index */
      iQ = (mng_uint8)((iB & iM) >> iS);
                                       /* index valid ? */
      if ((mng_uint32)iQ < pBuf->iPLTEcount)
      {                                /* put in intermediate row */
        pRGBArow [0] = pBuf->aPLTEentries [iQ].iRed;
        pRGBArow [1] = pBuf->aPLTEentries [iQ].iGreen;
        pRGBArow [2] = pBuf->aPLTEentries [iQ].iBlue;
                                       /* transparency for this index ? */
        if ((mng_uint32)iQ < pBuf->iTRNScount)
          pRGBArow [3] = pBuf->aTRNSentries [iQ];
        else
          pRGBArow [3] = 0xFF;
      }
      else
        MNG_ERROR (pData, MNG_PLTEINDEXERROR)

      pRGBArow += 4;                   /* next pixel */
      iM >>= 4;
      iS -= 4;
    }

    pData->bIsOpaque = MNG_FALSE;      /* it's not fully opaque */
  }
  else
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      if (!iM)                         /* mask underflow ? */
      {
        iB = pWorkrow [0];             /* get next input-byte */
        pWorkrow++;
        iM = 0xF0;
        iS = 4;
      }
                                       /* get the index */
      iQ = (mng_uint8)((iB & iM) >> iS);
                                       /* index valid ? */
      if ((mng_uint32)iQ < pBuf->iPLTEcount)
      {                                /* put in intermediate row */
        pRGBArow [0] = pBuf->aPLTEentries [iQ].iRed;
        pRGBArow [1] = pBuf->aPLTEentries [iQ].iGreen;
        pRGBArow [2] = pBuf->aPLTEentries [iQ].iBlue;
        pRGBArow [3] = 0xFF;
      }
      else
        MNG_ERROR (pData, MNG_PLTEINDEXERROR)

      pRGBArow += 4;                   /* next pixel */
      iM >>= 4;
      iS -= 4;
    }

    pData->bIsOpaque = MNG_TRUE;       /* it's fully opaque */
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_IDX4, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode process_idx8 (mng_datap pData)
{
  mng_uint8p     pWorkrow;
  mng_uint8p     pRGBArow;
  mng_int32      iX;
  mng_uint8      iQ;
  mng_imagedatap pBuf = (mng_imagedatap)pData->pStorebuf;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_IDX8, MNG_LC_START)
#endif

  if (!pBuf)                           /* no object? then use obj 0 */
    pBuf = ((mng_imagep)pData->pObjzero)->pImgbuf;
                                       /* temporary work pointers */
  pWorkrow = pData->pWorkrow + pData->iPixelofs;
  pRGBArow = pData->pRGBArow;

  if (pBuf->bHasTRNS)                  /* tRNS encountered ? */
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      iQ = *pWorkrow;                  /* get input byte */
                                       /* index valid ? */
      if ((mng_uint32)iQ < pBuf->iPLTEcount)
      {                                /* put in intermediate row */
        pRGBArow [0] = pBuf->aPLTEentries [iQ].iRed;
        pRGBArow [1] = pBuf->aPLTEentries [iQ].iGreen;
        pRGBArow [2] = pBuf->aPLTEentries [iQ].iBlue;
                                       /* transparency for this index ? */
        if ((mng_uint32)iQ < pBuf->iTRNScount)
          pRGBArow [3] = pBuf->aTRNSentries [iQ];
        else
          pRGBArow [3] = 0xFF;
      }
      else
        MNG_ERROR (pData, MNG_PLTEINDEXERROR)

      pRGBArow += 4;                   /* next pixel */
      pWorkrow++;
    }

    pData->bIsOpaque = MNG_FALSE;      /* it's not fully opaque */
  }
  else
  {
    for (iX = 0; iX < pData->iRowsamples; iX++)
    {
      iQ = *pWorkrow;                  /* get input byte */
                                       /* index valid ? */
      if ((mng_uint32)iQ < pBuf->iPLTEcount)
      {                                /* put in intermediate row */
        pRGBArow [0] = pBuf->aPLTEentries [iQ].iRed;
        pRGBArow [1] = pBuf->aPLTEentries [iQ].iGreen;
        pRGBArow [2] = pBuf->aPLTEentries [iQ].iBlue;
        pRGBArow [3] = 0xFF;
      }
      else
        MNG_ERROR (pData, MNG_PLTEINDEXERROR)

      pRGBArow += 4;                   /* next pixel */
      pWorkrow++;
    }

    pData->bIsOpaque = MNG_TRUE;       /* it's fully opaque */
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_IDX8, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode process_ga8 (mng_datap pData)
{
  mng_uint8p pWorkrow;
  mng_uint8p pRGBArow;
  mng_int32  iX;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_GA8, MNG_LC_START)
#endif
                                       /* temporary work pointers */
  pWorkrow = pData->pWorkrow + pData->iPixelofs;
  pRGBArow = pData->pRGBArow;

  for (iX = 0; iX < pData->iRowsamples; iX++)
  {
    *pRGBArow     = *pWorkrow;         /* copy the gray value */
    *(pRGBArow+1) = *pWorkrow;
    *(pRGBArow+2) = *pWorkrow;
    *(pRGBArow+3) = *(pWorkrow+1);     /* copy the alpha value */

    pWorkrow += 2;                     /* next pixel */
    pRGBArow += 4;
  }

  pData->bIsOpaque = MNG_FALSE;        /* it's definitely not fully opaque */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_GA8, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode process_ga16 (mng_datap pData)
{
  mng_uint8p  pWorkrow;
  mng_uint8p  pRGBArow;
  mng_int32  iX;
  mng_uint16 iW;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_GA16, MNG_LC_START)
#endif
                                       /* temporary work pointers */
  pWorkrow = pData->pWorkrow + pData->iPixelofs;
  pRGBArow = pData->pRGBArow;

  for (iX = 0; iX < pData->iRowsamples; iX++)
  {
    iW = mng_get_uint16 (pWorkrow);    /* copy the gray value */
    mng_put_uint16 (pRGBArow,   iW);
    mng_put_uint16 (pRGBArow+2, iW);
    mng_put_uint16 (pRGBArow+4, iW);
                                       /* copy the alpha value */
    mng_put_uint16 (pRGBArow+6, mng_get_uint16 (pWorkrow+2));

    pWorkrow += 4;                     /* next pixel */
    pRGBArow += 8;
  }

  pData->bIsOpaque = MNG_FALSE;        /* it's definitely not fully opaque */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_GA16, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode process_rgba8 (mng_datap pData)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_RGBA8, MNG_LC_START)
#endif
                                       /* this is the easiest transform */
  MNG_COPY (pData->pRGBArow, pData->pWorkrow + pData->iPixelofs, pData->iRowsize)

  pData->bIsOpaque = MNG_FALSE;        /* it's definitely not fully opaque */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_RGBA8, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode process_rgba16 (mng_datap pData)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_RGBA16, MNG_LC_START)
#endif
                                       /* this is the easiest transform */
  MNG_COPY (pData->pRGBArow, pData->pWorkrow + pData->iPixelofs, pData->iRowsize)

  pData->bIsOpaque = MNG_FALSE;        /* it's definitely not fully opaque */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_PROCESS_RGBA16, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */
/* *                                                                        * */
/* * Row processing initialization routines - set up the variables needed   * */
/* * to process uncompressed row-data                                       * */
/* *                                                                        * */
/* ************************************************************************** */

mng_retcode init_g1_ni     (mng_datap pData)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_G1_NI, MNG_LC_START)
#endif

  if (pData->fDisplayrow)
    pData->fProcessrow = (mng_fptr)process_g1;

  if (pData->pStoreobj)                /* store in object too ? */
  {                                    /* immediate delta ? */
    if ((pData->bHasDHDR) && (pData->bDeltaimmediate))
      pData->fStorerow = (mng_fptr)delta_g1;
    else
      pData->fStorerow = (mng_fptr)store_g1;
  }

  if (pData->iFilter & 0x40)           /* leveling & differing ? */
    pData->fDifferrow  = (mng_fptr)differ_g1;

  pData->iPass       = -1;
  pData->iRow        = 0;
  pData->iRowinc     = 1;
  pData->iCol        = 0;
  pData->iColinc     = 1;
  pData->iRowsamples = pData->iDatawidth;
  pData->iSamplemul  = 1;
  pData->iSampleofs  = 7;
  pData->iSamplediv  = 3;
  pData->iRowsize    = (pData->iRowsamples + 7) >> 3;
  pData->iRowmax     = pData->iRowsize + pData->iPixelofs;
  pData->iFilterbpp  = 1;
  pData->bIsRGBA16   = MNG_FALSE;      /* intermediate row is 8-bit deep */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_G1_NI, MNG_LC_END)
#endif

  return init_rowproc (pData);
}

/* ************************************************************************** */

mng_retcode init_g1_i      (mng_datap pData)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_G1_I, MNG_LC_START)
#endif

  if (pData->fDisplayrow)
    pData->fProcessrow = (mng_fptr)process_g1;

  if (pData->pStoreobj)                /* store in object too ? */
  {                                    /* immediate delta ? */
    if ((pData->bHasDHDR) && (pData->bDeltaimmediate))
      pData->fStorerow = (mng_fptr)delta_g1;
    else
      pData->fStorerow = (mng_fptr)store_g1;
  }

  if (pData->iFilter & 0x40)           /* leveling & differing ? */
    pData->fDifferrow  = (mng_fptr)differ_g1;

  pData->iPass       = 0;              /* from 0..6; is 1..7 in specifications */
  pData->iRow        = interlace_row     [0];
  pData->iRowinc     = interlace_rowskip [0];
  pData->iCol        = interlace_col     [0];
  pData->iColinc     = interlace_colskip [0];
  pData->iRowsamples = (pData->iDatawidth + interlace_roundoff [0]) >> interlace_divider [0];
  pData->iSamplemul  = 1;
  pData->iSampleofs  = 7;
  pData->iSamplediv  = 3;
  pData->iRowsize    = ((pData->iRowsamples + 7) >> 3);
  pData->iRowmax     = ((pData->iDatawidth + 7) >> 3) + pData->iPixelofs;
  pData->iFilterbpp  = 1;
  pData->bIsRGBA16   = MNG_FALSE;      /* intermediate row is 8-bit deep */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_G1_I, MNG_LC_END)
#endif

  return init_rowproc (pData);
}

/* ************************************************************************** */

mng_retcode init_g2_ni     (mng_datap pData)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_G2_NI, MNG_LC_START)
#endif

  if (pData->fDisplayrow)
    pData->fProcessrow = (mng_fptr)process_g2;

  if (pData->pStoreobj)                /* store in object too ? */
  {                                    /* immediate delta ? */
    if ((pData->bHasDHDR) && (pData->bDeltaimmediate))
      pData->fStorerow = (mng_fptr)delta_g2;
    else
      pData->fStorerow = (mng_fptr)store_g2;
  }

  if (pData->iFilter & 0x40)           /* leveling & differing ? */
    pData->fDifferrow  = (mng_fptr)differ_g2;

  pData->iPass       = -1;
  pData->iRow        = 0;
  pData->iRowinc     = 1;
  pData->iCol        = 0;
  pData->iColinc     = 1;
  pData->iRowsamples = pData->iDatawidth;
  pData->iSamplemul  = 1;
  pData->iSampleofs  = 3;
  pData->iSamplediv  = 2;
  pData->iRowsize    = (pData->iRowsamples + 3) >> 2;
  pData->iRowmax     = pData->iRowsize + pData->iPixelofs;
  pData->iFilterbpp  = 1;
  pData->bIsRGBA16   = MNG_FALSE;      /* intermediate row is 8-bit deep */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_G2_NI, MNG_LC_END)
#endif

  return init_rowproc (pData);
}

/* ************************************************************************** */

mng_retcode init_g2_i      (mng_datap pData)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_G2_I, MNG_LC_START)
#endif

  if (pData->fDisplayrow)
    pData->fProcessrow = (mng_fptr)process_g2;

  if (pData->pStoreobj)                /* store in object too ? */
  {                                    /* immediate delta ? */
    if ((pData->bHasDHDR) && (pData->bDeltaimmediate))
      pData->fStorerow = (mng_fptr)delta_g2;
    else
      pData->fStorerow = (mng_fptr)store_g2;
  }

  if (pData->iFilter & 0x40)           /* leveling & differing ? */
    pData->fDifferrow  = (mng_fptr)differ_g2;

  pData->iPass       = 0;              /* from 0..6; is 1..7 in specifications */
  pData->iRow        = interlace_row     [0];
  pData->iRowinc     = interlace_rowskip [0];
  pData->iCol        = interlace_col     [0];
  pData->iColinc     = interlace_colskip [0];
  pData->iRowsamples = (pData->iDatawidth + interlace_roundoff [0]) >> interlace_divider [0];
  pData->iSamplemul  = 1;
  pData->iSampleofs  = 3;
  pData->iSamplediv  = 2;
  pData->iRowsize    = ((pData->iRowsamples + 3) >> 2);
  pData->iRowmax     = ((pData->iDatawidth + 3) >> 2) + pData->iPixelofs;
  pData->iFilterbpp  = 1;
  pData->bIsRGBA16   = MNG_FALSE;      /* intermediate row is 8-bit deep */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_G2_I, MNG_LC_END)
#endif

  return init_rowproc (pData);
}

/* ************************************************************************** */

mng_retcode init_g4_ni     (mng_datap pData)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_G4_NI, MNG_LC_START)
#endif

  if (pData->fDisplayrow)
    pData->fProcessrow = (mng_fptr)process_g4;

  if (pData->pStoreobj)                /* store in object too ? */
  {                                    /* immediate delta ? */
    if ((pData->bHasDHDR) && (pData->bDeltaimmediate))
      pData->fStorerow = (mng_fptr)delta_g4;
    else
      pData->fStorerow = (mng_fptr)store_g4;
  }

  if (pData->iFilter & 0x40)           /* leveling & differing ? */
    pData->fDifferrow  = (mng_fptr)differ_g4;

  pData->iPass       = -1;
  pData->iRow        = 0;
  pData->iRowinc     = 1;
  pData->iCol        = 0;
  pData->iColinc     = 1;
  pData->iRowsamples = pData->iDatawidth;
  pData->iSamplemul  = 1;
  pData->iSampleofs  = 1;
  pData->iSamplediv  = 1;
  pData->iRowsize    = (pData->iRowsamples + 1) >> 1;
  pData->iRowmax     = pData->iRowsize + pData->iPixelofs;
  pData->iFilterbpp  = 1;
  pData->bIsRGBA16   = MNG_FALSE;      /* intermediate row is 8-bit deep */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_G4_NI, MNG_LC_END)
#endif

  return init_rowproc (pData);
}

/* ************************************************************************** */

mng_retcode init_g4_i      (mng_datap pData)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_G4_I, MNG_LC_START)
#endif

  if (pData->fDisplayrow)
    pData->fProcessrow = (mng_fptr)process_g4;

  if (pData->pStoreobj)                /* store in object too ? */
  {                                    /* immediate delta ? */
    if ((pData->bHasDHDR) && (pData->bDeltaimmediate))
      pData->fStorerow = (mng_fptr)delta_g4;
    else
      pData->fStorerow = (mng_fptr)store_g4;
  }

  if (pData->iFilter & 0x40)           /* leveling & differing ? */
    pData->fDifferrow  = (mng_fptr)differ_g4;

  pData->iPass       = 0;              /* from 0..6; is 1..7 in specifications */
  pData->iRow        = interlace_row     [0];
  pData->iRowinc     = interlace_rowskip [0];
  pData->iCol        = interlace_col     [0];
  pData->iColinc     = interlace_colskip [0];
  pData->iRowsamples = (pData->iDatawidth + interlace_roundoff [0]) >> interlace_divider [0];
  pData->iSamplemul  = 1;
  pData->iSampleofs  = 1;
  pData->iSamplediv  = 1;
  pData->iRowsize    = ((pData->iRowsamples + 1) >> 1);
  pData->iRowmax     = ((pData->iDatawidth + 1) >> 1) + pData->iPixelofs;
  pData->iFilterbpp  = 1;
  pData->bIsRGBA16   = MNG_FALSE;      /* intermediate row is 8-bit deep */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_G4_I, MNG_LC_END)
#endif

  return init_rowproc (pData);
}

/* ************************************************************************** */

mng_retcode init_g8_ni     (mng_datap pData)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_G8_NI, MNG_LC_START)
#endif

  if (pData->fDisplayrow)
    pData->fProcessrow = (mng_fptr)process_g8;

  if (pData->pStoreobj)                /* store in object too ? */
  {                                    /* immediate delta ? */
    if ((pData->bHasDHDR) && (pData->bDeltaimmediate))
      pData->fStorerow = (mng_fptr)delta_g8;
    else
      pData->fStorerow = (mng_fptr)store_g8;
  }

  if (pData->iFilter & 0x40)           /* leveling & differing ? */
    pData->fDifferrow  = (mng_fptr)differ_g8;

  pData->iPass       = -1;
  pData->iRow        = 0;
  pData->iRowinc     = 1;
  pData->iCol        = 0;
  pData->iColinc     = 1;
  pData->iRowsamples = pData->iDatawidth;
  pData->iSamplemul  = 1;
  pData->iSampleofs  = 0;
  pData->iSamplediv  = 0;
  pData->iRowsize    = pData->iRowsamples;
  pData->iRowmax     = pData->iRowsize + pData->iPixelofs;
  pData->iFilterbpp  = 1;
  pData->bIsRGBA16   = MNG_FALSE;      /* intermediate row is 8-bit deep */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_G8_NI, MNG_LC_END)
#endif

  return init_rowproc (pData);
}

/* ************************************************************************** */

mng_retcode init_g8_i      (mng_datap pData)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_G8_I, MNG_LC_START)
#endif

  if (pData->fDisplayrow)
    pData->fProcessrow = (mng_fptr)process_g8;

  if (pData->pStoreobj)                /* store in object too ? */
  {                                    /* immediate delta ? */
    if ((pData->bHasDHDR) && (pData->bDeltaimmediate))
      pData->fStorerow = (mng_fptr)delta_g8;
    else
      pData->fStorerow = (mng_fptr)store_g8;
  }

  if (pData->iFilter & 0x40)           /* leveling & differing ? */
    pData->fDifferrow  = (mng_fptr)differ_g8;

  pData->iPass       = 0;              /* from 0..6; is 1..7 in specifications */
  pData->iRow        = interlace_row     [0];
  pData->iRowinc     = interlace_rowskip [0];
  pData->iCol        = interlace_col     [0];
  pData->iColinc     = interlace_colskip [0];
  pData->iRowsamples = (pData->iDatawidth + interlace_roundoff [0]) >> interlace_divider [0];
  pData->iSamplemul  = 1;
  pData->iSampleofs  = 0;
  pData->iSamplediv  = 0;
  pData->iRowsize    = pData->iRowsamples;
  pData->iRowmax     = pData->iDatawidth + pData->iPixelofs;
  pData->iFilterbpp  = 1;
  pData->bIsRGBA16   = MNG_FALSE;      /* intermediate row is 8-bit deep */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_G8_I, MNG_LC_END)
#endif

  return init_rowproc (pData);
}

/* ************************************************************************** */

mng_retcode init_g16_ni    (mng_datap pData)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_G16_NI, MNG_LC_START)
#endif

  if (pData->fDisplayrow)
    pData->fProcessrow = (mng_fptr)process_g16;

  if (pData->pStoreobj)                /* store in object too ? */
  {                                    /* immediate delta ? */
    if ((pData->bHasDHDR) && (pData->bDeltaimmediate))
      pData->fStorerow = (mng_fptr)delta_g16;
    else
      pData->fStorerow = (mng_fptr)store_g16;
  }

  if (pData->iFilter & 0x40)           /* leveling & differing ? */
    pData->fDifferrow  = (mng_fptr)differ_g16;

  pData->iPass       = -1;
  pData->iRow        = 0;
  pData->iRowinc     = 1;
  pData->iCol        = 0;
  pData->iColinc     = 1;
  pData->iRowsamples = pData->iDatawidth;
  pData->iSamplemul  = 2;
  pData->iSampleofs  = 0;
  pData->iSamplediv  = 0;
  pData->iRowsize    = pData->iRowsamples << 1;
  pData->iRowmax     = pData->iRowsize + pData->iPixelofs;
  pData->iFilterbpp  = 2;
  pData->bIsRGBA16   = MNG_TRUE;       /* intermediate row is 16-bit deep */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_G16_NI, MNG_LC_END)
#endif

  return init_rowproc (pData);
}

/* ************************************************************************** */

mng_retcode init_g16_i     (mng_datap pData)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_G16_I, MNG_LC_START)
#endif

  if (pData->fDisplayrow)
    pData->fProcessrow = (mng_fptr)process_g16;

  if (pData->pStoreobj)                /* store in object too ? */
  {                                    /* immediate delta ? */
    if ((pData->bHasDHDR) && (pData->bDeltaimmediate))
      pData->fStorerow = (mng_fptr)delta_g16;
    else
      pData->fStorerow = (mng_fptr)store_g16;
  }

  if (pData->iFilter & 0x40)           /* leveling & differing ? */
    pData->fDifferrow  = (mng_fptr)differ_g16;

  pData->iPass       = 0;              /* from 0..6; is 1..7 in specifications */
  pData->iRow        = interlace_row     [0];
  pData->iRowinc     = interlace_rowskip [0];
  pData->iCol        = interlace_col     [0];
  pData->iColinc     = interlace_colskip [0];
  pData->iRowsamples = (pData->iDatawidth + interlace_roundoff [0]) >> interlace_divider [0];
  pData->iSamplemul  = 2;
  pData->iSampleofs  = 0;
  pData->iSamplediv  = 0;
  pData->iRowsize    = pData->iRowsamples << 1;
  pData->iRowmax     = (pData->iDatawidth << 1) + pData->iPixelofs;
  pData->iFilterbpp  = 2;
  pData->bIsRGBA16   = MNG_TRUE;       /* intermediate row is 16-bit deep */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_G16_I, MNG_LC_END)
#endif

  return init_rowproc (pData);
}

/* ************************************************************************** */

mng_retcode init_rgb8_ni   (mng_datap pData)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_RGB8_NI, MNG_LC_START)
#endif

  if (pData->fDisplayrow)
    pData->fProcessrow = (mng_fptr)process_rgb8;

  if (pData->pStoreobj)                /* store in object too ? */
  {                                    /* immediate delta ? */
    if ((pData->bHasDHDR) && (pData->bDeltaimmediate))
      pData->fStorerow = (mng_fptr)delta_rgb8;
    else
      pData->fStorerow = (mng_fptr)store_rgb8;
  }

  if (pData->iFilter & 0x40)           /* leveling & differing ? */
    pData->fDifferrow  = (mng_fptr)differ_rgb8;

  pData->iPass       = -1;
  pData->iRow        = 0;
  pData->iRowinc     = 1;
  pData->iCol        = 0;
  pData->iColinc     = 1;
  pData->iRowsamples = pData->iDatawidth;
  pData->iSamplemul  = 3;
  pData->iSampleofs  = 0;
  pData->iSamplediv  = 0;
  pData->iRowsize    = pData->iRowsamples * 3;
  pData->iRowmax     = pData->iRowsize + pData->iPixelofs;
  pData->iFilterbpp  = 3;
  pData->bIsRGBA16   = MNG_FALSE;      /* intermediate row is 8-bit deep */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_RGB8_NI, MNG_LC_END)
#endif

  return init_rowproc (pData);
}

/* ************************************************************************** */

mng_retcode init_rgb8_i    (mng_datap pData)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_RGB8_I, MNG_LC_START)
#endif

  if (pData->fDisplayrow)
    pData->fProcessrow = (mng_fptr)process_rgb8;

  if (pData->pStoreobj)                /* store in object too ? */
  {                                    /* immediate delta ? */
    if ((pData->bHasDHDR) && (pData->bDeltaimmediate))
      pData->fStorerow = (mng_fptr)delta_rgb8;
    else
      pData->fStorerow = (mng_fptr)store_rgb8;
  }

  if (pData->iFilter & 0x40)           /* leveling & differing ? */
    pData->fDifferrow  = (mng_fptr)differ_rgb8;

  pData->iPass       = 0;              /* from 0..6; is 1..7 in specifications */
  pData->iRow        = interlace_row     [0];
  pData->iRowinc     = interlace_rowskip [0];
  pData->iCol        = interlace_col     [0];
  pData->iColinc     = interlace_colskip [0];
  pData->iRowsamples = (pData->iDatawidth + interlace_roundoff [0]) >> interlace_divider [0];
  pData->iSamplemul  = 3;
  pData->iSampleofs  = 0;
  pData->iSamplediv  = 0;
  pData->iRowsize    = pData->iRowsamples * 3;
  pData->iRowmax     = (pData->iDatawidth * 3) + pData->iPixelofs;
  pData->iFilterbpp  = 3;
  pData->bIsRGBA16   = MNG_FALSE;      /* intermediate row is 8-bit deep */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_RGB8_I, MNG_LC_END)
#endif

  return init_rowproc (pData);
}

/* ************************************************************************** */

mng_retcode init_rgb16_ni  (mng_datap pData)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_RGB16_NI, MNG_LC_START)
#endif

  if (pData->fDisplayrow)
    pData->fProcessrow = (mng_fptr)process_rgb16;

  if (pData->pStoreobj)                /* store in object too ? */
  {                                    /* immediate delta ? */
    if ((pData->bHasDHDR) && (pData->bDeltaimmediate))
      pData->fStorerow = (mng_fptr)delta_rgb16;
    else
      pData->fStorerow = (mng_fptr)store_rgb16;
  }

  if (pData->iFilter & 0x40)           /* leveling & differing ? */
    pData->fDifferrow  = (mng_fptr)differ_rgb16;

  pData->iPass       = -1;
  pData->iRow        = 0;
  pData->iRowinc     = 1;
  pData->iCol        = 0;
  pData->iColinc     = 1;
  pData->iRowsamples = pData->iDatawidth;
  pData->iSamplemul  = 6;
  pData->iSampleofs  = 0;
  pData->iSamplediv  = 0;
  pData->iRowsize    = pData->iRowsamples * 6;
  pData->iRowmax     = pData->iRowsize + pData->iPixelofs;
  pData->iFilterbpp  = 6;
  pData->bIsRGBA16   = MNG_TRUE;       /* intermediate row is 16-bit deep */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_RGB16_NI, MNG_LC_END)
#endif

  return init_rowproc (pData);
}

/* ************************************************************************** */

mng_retcode init_rgb16_i   (mng_datap pData)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_RGB16_I, MNG_LC_START)
#endif

  if (pData->fDisplayrow)
    pData->fProcessrow = (mng_fptr)process_rgb16;

  if (pData->pStoreobj)                /* store in object too ? */
  {                                    /* immediate delta ? */
    if ((pData->bHasDHDR) && (pData->bDeltaimmediate))
      pData->fStorerow = (mng_fptr)delta_rgb16;
    else
      pData->fStorerow = (mng_fptr)store_rgb16;
  }

  if (pData->iFilter & 0x40)           /* leveling & differing ? */
    pData->fDifferrow  = (mng_fptr)differ_rgb16;

  pData->iPass       = 0;              /* from 0..6; is 1..7 in specifications */
  pData->iRow        = interlace_row     [0];
  pData->iRowinc     = interlace_rowskip [0];
  pData->iCol        = interlace_col     [0];
  pData->iColinc     = interlace_colskip [0];
  pData->iRowsamples = (pData->iDatawidth + interlace_roundoff [0]) >> interlace_divider [0];
  pData->iSamplemul  = 6;
  pData->iSampleofs  = 0;
  pData->iSamplediv  = 0;
  pData->iRowsize    = pData->iRowsamples * 6;
  pData->iRowmax     = (pData->iDatawidth * 6) + pData->iPixelofs;
  pData->iFilterbpp  = 6;
  pData->bIsRGBA16   = MNG_TRUE;       /* intermediate row is 16-bit deep */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_RGB16_I, MNG_LC_END)
#endif

  return init_rowproc (pData);
}

/* ************************************************************************** */

mng_retcode init_idx1_ni   (mng_datap pData)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_IDX1_NI, MNG_LC_START)
#endif

  if (pData->fDisplayrow)
    pData->fProcessrow = (mng_fptr)process_idx1;

  if (pData->pStoreobj)                /* store in object too ? */
  {                                    /* immediate delta ? */
    if ((pData->bHasDHDR) && (pData->bDeltaimmediate))
      pData->fStorerow = (mng_fptr)delta_idx1;
    else
      pData->fStorerow = (mng_fptr)store_idx1;
  }

  if (pData->iFilter & 0x40)           /* leveling & differing ? */
    pData->fDifferrow  = (mng_fptr)differ_idx1;

  pData->iPass       = -1;
  pData->iRow        = 0;
  pData->iRowinc     = 1;
  pData->iCol        = 0;
  pData->iColinc     = 1;
  pData->iRowsamples = pData->iDatawidth;
  pData->iSamplemul  = 1;
  pData->iSampleofs  = 7;
  pData->iSamplediv  = 3;
  pData->iRowsize    = (pData->iRowsamples + 7) >> 3;
  pData->iRowmax     = pData->iRowsize + pData->iPixelofs;
  pData->iFilterbpp  = 1;
  pData->bIsRGBA16   = MNG_FALSE;      /* intermediate row is 8-bit deep */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_IDX1_NI, MNG_LC_END)
#endif

  return init_rowproc (pData);
}

/* ************************************************************************** */

mng_retcode init_idx1_i    (mng_datap pData)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_IDX1_I, MNG_LC_START)
#endif

  if (pData->fDisplayrow)
    pData->fProcessrow = (mng_fptr)process_idx1;

  if (pData->pStoreobj)                /* store in object too ? */
  {                                    /* immediate delta ? */
    if ((pData->bHasDHDR) && (pData->bDeltaimmediate))
      pData->fStorerow = (mng_fptr)delta_idx1;
    else
      pData->fStorerow = (mng_fptr)store_idx1;
  }

  if (pData->iFilter & 0x40)           /* leveling & differing ? */
    pData->fDifferrow  = (mng_fptr)differ_idx1;

  pData->iPass       = 0;              /* from 0..6; is 1..7 in specifications */
  pData->iRow        = interlace_row     [0];
  pData->iRowinc     = interlace_rowskip [0];
  pData->iCol        = interlace_col     [0];
  pData->iColinc     = interlace_colskip [0];
  pData->iRowsamples = (pData->iDatawidth + interlace_roundoff [0]) >> interlace_divider [0];
  pData->iSamplemul  = 1;
  pData->iSampleofs  = 7;
  pData->iSamplediv  = 3;
  pData->iRowsize    = (pData->iRowsamples + 7) >> 3;
  pData->iRowmax     = ((pData->iDatawidth + 7) >> 3) + pData->iPixelofs;
  pData->iFilterbpp  = 1;
  pData->bIsRGBA16   = MNG_FALSE;      /* intermediate row is 8-bit deep */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_IDX1_I, MNG_LC_END)
#endif

  return init_rowproc (pData);
}

/* ************************************************************************** */

mng_retcode init_idx2_ni   (mng_datap pData)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_IDX2_NI, MNG_LC_START)
#endif

  if (pData->fDisplayrow)
    pData->fProcessrow = (mng_fptr)process_idx2;

  if (pData->pStoreobj)                /* store in object too ? */
  {                                    /* immediate delta ? */
    if ((pData->bHasDHDR) && (pData->bDeltaimmediate))
      pData->fStorerow = (mng_fptr)delta_idx2;
    else
      pData->fStorerow = (mng_fptr)store_idx2;
  }

  if (pData->iFilter & 0x40)           /* leveling & differing ? */
    pData->fDifferrow  = (mng_fptr)differ_idx2;

  pData->iPass       = -1;
  pData->iRow        = 0;
  pData->iRowinc     = 1;
  pData->iCol        = 0;
  pData->iColinc     = 1;
  pData->iRowsamples = pData->iDatawidth;
  pData->iSamplemul  = 1;
  pData->iSampleofs  = 3;
  pData->iSamplediv  = 2;
  pData->iRowsize    = (pData->iRowsamples + 3) >> 2;
  pData->iRowmax     = pData->iRowsize + pData->iPixelofs;
  pData->iFilterbpp  = 1;
  pData->bIsRGBA16   = MNG_FALSE;      /* intermediate row is 8-bit deep */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_IDX2_NI, MNG_LC_END)
#endif

  return init_rowproc (pData);
}

/* ************************************************************************** */

mng_retcode init_idx2_i    (mng_datap pData)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_IDX2_I, MNG_LC_START)
#endif

  if (pData->fDisplayrow)
    pData->fProcessrow = (mng_fptr)process_idx2;

  if (pData->pStoreobj)                /* store in object too ? */
  {                                    /* immediate delta ? */
    if ((pData->bHasDHDR) && (pData->bDeltaimmediate))
      pData->fStorerow = (mng_fptr)delta_idx2;
    else
      pData->fStorerow = (mng_fptr)store_idx2;
  }

  if (pData->iFilter & 0x40)           /* leveling & differing ? */
    pData->fDifferrow  = (mng_fptr)differ_idx2;

  pData->iPass       = 0;              /* from 0..6; is 1..7 in specifications */
  pData->iRow        = interlace_row     [0];
  pData->iRowinc     = interlace_rowskip [0];
  pData->iCol        = interlace_col     [0];
  pData->iColinc     = interlace_colskip [0];
  pData->iRowsamples = (pData->iDatawidth + interlace_roundoff [0]) >> interlace_divider [0];
  pData->iSamplemul  = 1;
  pData->iSampleofs  = 3;
  pData->iSamplediv  = 2;
  pData->iRowsize    = (pData->iRowsamples + 3) >> 2;
  pData->iRowmax     = ((pData->iDatawidth + 3) >> 2) + pData->iPixelofs;
  pData->iFilterbpp  = 1;
  pData->bIsRGBA16   = MNG_FALSE;      /* intermediate row is 8-bit deep */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_IDX2_I, MNG_LC_END)
#endif

  return init_rowproc (pData);
}

/* ************************************************************************** */

mng_retcode init_idx4_ni   (mng_datap pData)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_IDX4_NI, MNG_LC_START)
#endif

  if (pData->fDisplayrow)
    pData->fProcessrow = (mng_fptr)process_idx4;

  if (pData->pStoreobj)                /* store in object too ? */
  {                                    /* immediate delta ? */
    if ((pData->bHasDHDR) && (pData->bDeltaimmediate))
      pData->fStorerow = (mng_fptr)delta_idx4;
    else
      pData->fStorerow = (mng_fptr)store_idx4;
  }

  if (pData->iFilter & 0x40)           /* leveling & differing ? */
    pData->fDifferrow  = (mng_fptr)differ_idx4;

  pData->iPass       = -1;
  pData->iRow        = 0;
  pData->iRowinc     = 1;
  pData->iCol        = 0;
  pData->iColinc     = 1;
  pData->iRowsamples = pData->iDatawidth;
  pData->iSamplemul  = 1;
  pData->iSampleofs  = 1;
  pData->iSamplediv  = 1;
  pData->iRowsize    = (pData->iRowsamples + 1) >> 1;
  pData->iRowmax     = pData->iRowsize + pData->iPixelofs;
  pData->iFilterbpp  = 1;
  pData->bIsRGBA16   = MNG_FALSE;      /* intermediate row is 8-bit deep */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_IDX4_NI, MNG_LC_END)
#endif

  return init_rowproc (pData);
}

/* ************************************************************************** */

mng_retcode init_idx4_i    (mng_datap pData)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_IDX4_I, MNG_LC_START)
#endif

  if (pData->fDisplayrow)
    pData->fProcessrow = (mng_fptr)process_idx4;

  if (pData->pStoreobj)                /* store in object too ? */
  {                                    /* immediate delta ? */
    if ((pData->bHasDHDR) && (pData->bDeltaimmediate))
      pData->fStorerow = (mng_fptr)delta_idx4;
    else
      pData->fStorerow = (mng_fptr)store_idx4;
  }

  if (pData->iFilter & 0x40)           /* leveling & differing ? */
    pData->fDifferrow  = (mng_fptr)differ_idx4;

  pData->iPass       = 0;              /* from 0..6; is 1..7 in specifications */
  pData->iRow        = interlace_row     [0];
  pData->iRowinc     = interlace_rowskip [0];
  pData->iCol        = interlace_col     [0];
  pData->iColinc     = interlace_colskip [0];
  pData->iRowsamples = (pData->iDatawidth + interlace_roundoff [0]) >> interlace_divider [0];
  pData->iSamplemul  = 1;
  pData->iSampleofs  = 1;
  pData->iSamplediv  = 1;
  pData->iRowsize    = (pData->iRowsamples + 1) >> 1;
  pData->iRowmax     = ((pData->iDatawidth + 1) >> 1) + pData->iPixelofs;
  pData->iFilterbpp  = 1;
  pData->bIsRGBA16   = MNG_FALSE;      /* intermediate row is 8-bit deep */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_IDX4_I, MNG_LC_END)
#endif

  return init_rowproc (pData);
}

/* ************************************************************************** */

mng_retcode init_idx8_ni   (mng_datap pData)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_IDX8_NI, MNG_LC_START)
#endif

  if (pData->fDisplayrow)
    pData->fProcessrow = (mng_fptr)process_idx8;

  if (pData->pStoreobj)                /* store in object too ? */
  {                                    /* immediate delta ? */
    if ((pData->bHasDHDR) && (pData->bDeltaimmediate))
      pData->fStorerow = (mng_fptr)delta_idx8;
    else
      pData->fStorerow = (mng_fptr)store_idx8;
  }

  if (pData->iFilter & 0x40)           /* leveling & differing ? */
    pData->fDifferrow  = (mng_fptr)differ_idx8;

  pData->iPass       = -1;
  pData->iRow        = 0;
  pData->iRowinc     = 1;
  pData->iCol        = 0;
  pData->iColinc     = 1;
  pData->iRowsamples = pData->iDatawidth;
  pData->iSamplemul  = 1;
  pData->iSampleofs  = 0;
  pData->iSamplediv  = 0;
  pData->iRowsize    = pData->iRowsamples;
  pData->iRowmax     = pData->iRowsize + pData->iPixelofs;
  pData->iFilterbpp  = 1;
  pData->bIsRGBA16   = MNG_FALSE;      /* intermediate row is 8-bit deep */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_IDX8_NI, MNG_LC_END)
#endif

  return init_rowproc (pData);
}

/* ************************************************************************** */

mng_retcode init_idx8_i    (mng_datap pData)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_IDX8_I, MNG_LC_START)
#endif

  if (pData->fDisplayrow)
    pData->fProcessrow = (mng_fptr)process_idx8;

  if (pData->pStoreobj)                /* store in object too ? */
  {                                    /* immediate delta ? */
    if ((pData->bHasDHDR) && (pData->bDeltaimmediate))
      pData->fStorerow = (mng_fptr)delta_idx8;
    else
      pData->fStorerow = (mng_fptr)store_idx8;
  }

  if (pData->iFilter & 0x40)           /* leveling & differing ? */
    pData->fDifferrow  = (mng_fptr)differ_idx8;

  pData->iPass       = 0;              /* from 0..6; is 1..7 in specifications */
  pData->iRow        = interlace_row     [0];
  pData->iRowinc     = interlace_rowskip [0];
  pData->iCol        = interlace_col     [0];
  pData->iColinc     = interlace_colskip [0];
  pData->iRowsamples = (pData->iDatawidth + interlace_roundoff [0]) >> interlace_divider [0];
  pData->iSamplemul  = 1;
  pData->iSampleofs  = 0;
  pData->iSamplediv  = 0;
  pData->iRowsize    = pData->iRowsamples;
  pData->iRowmax     = pData->iDatawidth + pData->iPixelofs;
  pData->iFilterbpp  = 1;
  pData->bIsRGBA16   = MNG_FALSE;      /* intermediate row is 8-bit deep */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_IDX8_I, MNG_LC_END)
#endif

  return init_rowproc (pData);
}

/* ************************************************************************** */

mng_retcode init_ga8_ni    (mng_datap pData)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_GA8_NI, MNG_LC_START)
#endif

  if (pData->fDisplayrow)
    pData->fProcessrow = (mng_fptr)process_ga8;

  if (pData->pStoreobj)                /* store in object too ? */
  {                                    /* immediate delta ? */
    if ((pData->bHasDHDR) && (pData->bDeltaimmediate))
      pData->fStorerow = (mng_fptr)delta_ga8;
    else
      pData->fStorerow = (mng_fptr)store_ga8;
  }

  if (pData->iFilter & 0x40)           /* leveling & differing ? */
    pData->fDifferrow  = (mng_fptr)differ_ga8;

  pData->iPass       = -1;
  pData->iRow        = 0;
  pData->iRowinc     = 1;
  pData->iCol        = 0;
  pData->iColinc     = 1;
  pData->iRowsamples = pData->iDatawidth;
  pData->iSamplemul  = 2;
  pData->iSampleofs  = 0;
  pData->iSamplediv  = 0;
  pData->iRowsize    = pData->iRowsamples << 1;
  pData->iRowmax     = pData->iRowsize + pData->iPixelofs;
  pData->iFilterbpp  = 2;
  pData->bIsRGBA16   = MNG_FALSE;      /* intermediate row is 8-bit deep */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_GA8_NI, MNG_LC_END)
#endif

  return init_rowproc (pData);
}

/* ************************************************************************** */

mng_retcode init_ga8_i     (mng_datap pData)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_GA8_I, MNG_LC_START)
#endif

  if (pData->fDisplayrow)
    pData->fProcessrow = (mng_fptr)process_ga8;

  if (pData->pStoreobj)                /* store in object too ? */
  {                                    /* immediate delta ? */
    if ((pData->bHasDHDR) && (pData->bDeltaimmediate))
      pData->fStorerow = (mng_fptr)delta_ga8;
    else
      pData->fStorerow = (mng_fptr)store_ga8;
  }

  if (pData->iFilter & 0x40)           /* leveling & differing ? */
    pData->fDifferrow  = (mng_fptr)differ_ga8;

  pData->iPass       = 0;              /* from 0..6; is 1..7 in specifications */
  pData->iRow        = interlace_row     [0];
  pData->iRowinc     = interlace_rowskip [0];
  pData->iCol        = interlace_col     [0];
  pData->iColinc     = interlace_colskip [0];
  pData->iRowsamples = (pData->iDatawidth + interlace_roundoff [0]) >> interlace_divider [0];
  pData->iSamplemul  = 2;
  pData->iSampleofs  = 0;
  pData->iSamplediv  = 0;
  pData->iRowsize    = pData->iRowsamples << 1;
  pData->iRowmax     = (pData->iDatawidth << 1) + pData->iPixelofs;
  pData->iFilterbpp  = 2;
  pData->bIsRGBA16   = MNG_FALSE;      /* intermediate row is 8-bit deep */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_GA8_I, MNG_LC_END)
#endif

  return init_rowproc (pData);
}

/* ************************************************************************** */

mng_retcode init_ga16_ni   (mng_datap pData)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_GA16_NI, MNG_LC_START)
#endif

  if (pData->fDisplayrow)
    pData->fProcessrow = (mng_fptr)process_ga16;

  if (pData->pStoreobj)                /* store in object too ? */
  {                                    /* immediate delta ? */
    if ((pData->bHasDHDR) && (pData->bDeltaimmediate))
      pData->fStorerow = (mng_fptr)delta_ga16;
    else
      pData->fStorerow = (mng_fptr)store_ga16;
  }

  if (pData->iFilter & 0x40)           /* leveling & differing ? */
    pData->fDifferrow  = (mng_fptr)differ_ga16;

  pData->iPass       = -1;
  pData->iRow        = 0;
  pData->iRowinc     = 1;
  pData->iCol        = 0;
  pData->iColinc     = 1;
  pData->iRowsamples = pData->iDatawidth;
  pData->iSamplemul  = 4;
  pData->iSampleofs  = 0;
  pData->iSamplediv  = 0;
  pData->iRowsize    = pData->iRowsamples << 2;
  pData->iRowmax     = pData->iRowsize + pData->iPixelofs;
  pData->iFilterbpp  = 4;
  pData->bIsRGBA16   = MNG_TRUE;       /* intermediate row is 16-bit deep */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_GA16_NI, MNG_LC_END)
#endif

  return init_rowproc (pData);
}

/* ************************************************************************** */

mng_retcode init_ga16_i    (mng_datap pData)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_GA16_I, MNG_LC_START)
#endif

  if (pData->fDisplayrow)
    pData->fProcessrow = (mng_fptr)process_ga16;

  if (pData->pStoreobj)                /* store in object too ? */
  {                                    /* immediate delta ? */
    if ((pData->bHasDHDR) && (pData->bDeltaimmediate))
      pData->fStorerow = (mng_fptr)delta_ga16;
    else
      pData->fStorerow = (mng_fptr)store_ga16;
  }

  if (pData->iFilter & 0x40)           /* leveling & differing ? */
    pData->fDifferrow  = (mng_fptr)differ_ga16;

  pData->iPass       = 0;              /* from 0..6; is 1..7 in specifications */
  pData->iRow        = interlace_row     [0];
  pData->iRowinc     = interlace_rowskip [0];
  pData->iCol        = interlace_col     [0];
  pData->iColinc     = interlace_colskip [0];
  pData->iRowsamples = (pData->iDatawidth + interlace_roundoff [0]) >> interlace_divider [0];
  pData->iSamplemul  = 4;
  pData->iSampleofs  = 0;
  pData->iSamplediv  = 0;
  pData->iRowsize    = pData->iRowsamples << 2;
  pData->iRowmax     = (pData->iDatawidth << 2) + pData->iPixelofs;
  pData->iFilterbpp  = 4;
  pData->bIsRGBA16   = MNG_TRUE;       /* intermediate row is 16-bit deep */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_GA16_I, MNG_LC_END)
#endif

  return init_rowproc (pData);
}

/* ************************************************************************** */

mng_retcode init_rgba8_ni  (mng_datap pData)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_RGBA8_NI, MNG_LC_START)
#endif

  if (pData->fDisplayrow)
    pData->fProcessrow = (mng_fptr)process_rgba8;

  if (pData->pStoreobj)                /* store in object too ? */
  {                                    /* immediate delta ? */
    if ((pData->bHasDHDR) && (pData->bDeltaimmediate))
      pData->fStorerow = (mng_fptr)delta_rgba8;
    else
      pData->fStorerow = (mng_fptr)store_rgba8;
  }

  if (pData->iFilter & 0x40)           /* leveling & differing ? */
    pData->fDifferrow  = (mng_fptr)differ_rgba8;

  pData->iPass       = -1;
  pData->iRow        = 0;
  pData->iRowinc     = 1;
  pData->iCol        = 0;
  pData->iColinc     = 1;
  pData->iRowsamples = pData->iDatawidth;
  pData->iSamplemul  = 4;
  pData->iSampleofs  = 0;
  pData->iSamplediv  = 0;
  pData->iRowsize    = pData->iRowsamples << 2;
  pData->iRowmax     = pData->iRowsize + pData->iPixelofs;
  pData->iFilterbpp  = 4;
  pData->bIsRGBA16   = MNG_FALSE;      /* intermediate row is 8-bit deep */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_RGBA8_NI, MNG_LC_END)
#endif

  return init_rowproc (pData);
}

/* ************************************************************************** */

mng_retcode init_rgba8_i   (mng_datap pData)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_RGBA8_I, MNG_LC_START)
#endif

  if (pData->fDisplayrow)
    pData->fProcessrow = (mng_fptr)process_rgba8;

  if (pData->pStoreobj)                /* store in object too ? */
  {                                    /* immediate delta ? */
    if ((pData->bHasDHDR) && (pData->bDeltaimmediate))
      pData->fStorerow = (mng_fptr)delta_rgba8;
    else
      pData->fStorerow = (mng_fptr)store_rgba8;
  }

  if (pData->iFilter & 0x40)           /* leveling & differing ? */
    pData->fDifferrow  = (mng_fptr)differ_rgba8;

  pData->iPass       = 0;              /* from 0..6; is 1..7 in specifications */
  pData->iRow        = interlace_row     [0];
  pData->iRowinc     = interlace_rowskip [0];
  pData->iCol        = interlace_col     [0];
  pData->iColinc     = interlace_colskip [0];
  pData->iRowsamples = (pData->iDatawidth + interlace_roundoff [0]) >> interlace_divider [0];
  pData->iSamplemul  = 4;
  pData->iSampleofs  = 0;
  pData->iSamplediv  = 0;
  pData->iRowsize    = pData->iRowsamples << 2;
  pData->iRowmax     = (pData->iDatawidth << 2) + pData->iPixelofs;
  pData->iFilterbpp  = 4;
  pData->bIsRGBA16   = MNG_FALSE;      /* intermediate row is 8-bit deep */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_RGBA8_I, MNG_LC_END)
#endif

  return init_rowproc (pData);
}

/* ************************************************************************** */

mng_retcode init_rgba16_ni (mng_datap pData)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_RGBA16_NI, MNG_LC_START)
#endif

  if (pData->fDisplayrow)
    pData->fProcessrow = (mng_fptr)process_rgba16;

  if (pData->pStoreobj)                /* store in object too ? */
  {                                    /* immediate delta ? */
    if ((pData->bHasDHDR) && (pData->bDeltaimmediate))
      pData->fStorerow = (mng_fptr)delta_rgba16;
    else
      pData->fStorerow = (mng_fptr)store_rgba16;
  }

  if (pData->iFilter & 0x40)           /* leveling & differing ? */
    pData->fDifferrow  = (mng_fptr)differ_rgba16;

  pData->iPass       = -1;
  pData->iRow        = 0;
  pData->iRowinc     = 1;
  pData->iCol        = 0;
  pData->iColinc     = 1;
  pData->iRowsamples = pData->iDatawidth;
  pData->iSamplemul  = 8;
  pData->iSampleofs  = 0;
  pData->iSamplediv  = 0;
  pData->iRowsize    = pData->iRowsamples << 3;
  pData->iRowmax     = pData->iRowsize + pData->iPixelofs;
  pData->iFilterbpp  = 8;
  pData->bIsRGBA16   = MNG_TRUE;       /* intermediate row is 16-bit deep */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_RGBA16_NI, MNG_LC_END)
#endif

  return init_rowproc (pData);
}

/* ************************************************************************** */

mng_retcode init_rgba16_i  (mng_datap pData)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_RGBA16_I, MNG_LC_START)
#endif

  if (pData->fDisplayrow)
    pData->fProcessrow = (mng_fptr)process_rgba16;

  if (pData->pStoreobj)                /* store in object too ? */
  {                                    /* immediate delta ? */
    if ((pData->bHasDHDR) && (pData->bDeltaimmediate))
      pData->fStorerow = (mng_fptr)delta_rgba16;
    else
      pData->fStorerow = (mng_fptr)store_rgba16;
  }

  if (pData->iFilter & 0x40)           /* leveling & differing ? */
    pData->fDifferrow  = (mng_fptr)differ_rgba16;

  pData->iPass       = 0;              /* from 0..6; (1..7 in specification) */
  pData->iRow        = interlace_row     [0];
  pData->iRowinc     = interlace_rowskip [0];
  pData->iCol        = interlace_col     [0];
  pData->iColinc     = interlace_colskip [0];
  pData->iRowsamples = (pData->iDatawidth + interlace_roundoff [0]) >> interlace_divider [0];
  pData->iSamplemul  = 8;
  pData->iSampleofs  = 0;
  pData->iSamplediv  = 0;
  pData->iRowsize    = pData->iRowsamples << 3;
  pData->iRowmax     = (pData->iDatawidth << 3) + pData->iPixelofs;
  pData->iFilterbpp  = 8;
  pData->bIsRGBA16   = MNG_TRUE;       /* intermediate row is 16-bit deep */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_RGBA16_I, MNG_LC_END)
#endif

  return init_rowproc (pData);
}

/* ************************************************************************** */
/* *                                                                        * */
/* * Row processing initialization routines (JPEG) - set up the variables   * */
/* * needed to process uncompressed row-data                                * */
/* *                                                                        * */
/* ************************************************************************** */

#ifdef MNG_INCLUDE_JNG

/* ************************************************************************** */

mng_retcode init_jpeg_a1_ni     (mng_datap pData)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_JPEG_A1_NI, MNG_LC_START)
#endif

  if (pData->pStoreobj)                /* store in object too ? */
  {
    if (pData->iJHDRimgbitdepth == 8)
    {
      switch (pData->iJHDRcolortype)
      {
        case 12 : { pData->fStorerow = (mng_fptr)store_jpeg_g8_a1;   break; }
        case 14 : { pData->fStorerow = (mng_fptr)store_jpeg_rgb8_a1; break; }
      }
    }

    /* TODO: bitdepth 12 & 20 */

  }

  if (pData->iFilter & 0x40)           /* leveling & differing ? */
    pData->fDifferrow  = (mng_fptr)differ_g1;

  pData->iPass       = -1;
  pData->iRow        = 0;
  pData->iRowinc     = 1;
  pData->iCol        = 0;
  pData->iColinc     = 1;
  pData->iRowsamples = pData->iDatawidth;
  pData->iSamplemul  = 1;
  pData->iSampleofs  = 7;
  pData->iSamplediv  = 3;
  pData->iRowsize    = (pData->iRowsamples + 7) >> 3;
  pData->iRowmax     = pData->iRowsize + pData->iPixelofs;
  pData->iFilterbpp  = 1;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_JPEG_A1_NI, MNG_LC_END)
#endif

  return init_rowproc (pData);
}

/* ************************************************************************** */

mng_retcode init_jpeg_a2_ni     (mng_datap pData)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_JPEG_A2_NI, MNG_LC_START)
#endif

  if (pData->pStoreobj)                /* store in object too ? */
  {
    if (pData->iJHDRimgbitdepth == 8)
    {
      switch (pData->iJHDRcolortype)
      {
        case 12 : { pData->fStorerow = (mng_fptr)store_jpeg_g8_a2;   break; }
        case 14 : { pData->fStorerow = (mng_fptr)store_jpeg_rgb8_a2; break; }
      }
    }

    /* TODO: bitdepth 12 & 20 */

  }

  if (pData->iFilter & 0x40)           /* leveling & differing ? */
    pData->fDifferrow  = (mng_fptr)differ_g2;

  pData->iPass       = -1;
  pData->iRow        = 0;
  pData->iRowinc     = 1;
  pData->iCol        = 0;
  pData->iColinc     = 1;
  pData->iRowsamples = pData->iDatawidth;
  pData->iSamplemul  = 1;
  pData->iSampleofs  = 3;
  pData->iSamplediv  = 2;
  pData->iRowsize    = (pData->iRowsamples + 3) >> 2;
  pData->iRowmax     = pData->iRowsize + pData->iPixelofs;
  pData->iFilterbpp  = 1;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_JPEG_A2_NI, MNG_LC_END)
#endif

  return init_rowproc (pData);
}

/* ************************************************************************** */

mng_retcode init_jpeg_a4_ni     (mng_datap pData)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_JPEG_A4_NI, MNG_LC_START)
#endif

  if (pData->pStoreobj)                /* store in object too ? */
  {
    if (pData->iJHDRimgbitdepth == 8)
    {
      switch (pData->iJHDRcolortype)
      {
        case 12 : { pData->fStorerow = (mng_fptr)store_jpeg_g8_a4;   break; }
        case 14 : { pData->fStorerow = (mng_fptr)store_jpeg_rgb8_a4; break; }
      }
    }

    /* TODO: bitdepth 12 & 20 */

  }

  if (pData->iFilter & 0x40)           /* leveling & differing ? */
    pData->fDifferrow  = (mng_fptr)differ_g4;

  pData->iPass       = -1;
  pData->iRow        = 0;
  pData->iRowinc     = 1;
  pData->iCol        = 0;
  pData->iColinc     = 1;
  pData->iRowsamples = pData->iDatawidth;
  pData->iSamplemul  = 1;
  pData->iSampleofs  = 1;
  pData->iSamplediv  = 1;
  pData->iRowsize    = (pData->iRowsamples + 1) >> 1;
  pData->iRowmax     = pData->iRowsize + pData->iPixelofs;
  pData->iFilterbpp  = 1;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_JPEG_A4_NI, MNG_LC_END)
#endif

  return init_rowproc (pData);
}

/* ************************************************************************** */

mng_retcode init_jpeg_a8_ni     (mng_datap pData)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_JPEG_A8_NI, MNG_LC_START)
#endif

  if (pData->pStoreobj)                /* store in object too ? */
  {
    if (pData->iJHDRimgbitdepth == 8)
    {
      switch (pData->iJHDRcolortype)
      {
        case 12 : { pData->fStorerow = (mng_fptr)store_jpeg_g8_a8;   break; }
        case 14 : { pData->fStorerow = (mng_fptr)store_jpeg_rgb8_a8; break; }
      }
    }

    /* TODO: bitdepth 12 & 20 */

  }

  if (pData->iFilter & 0x40)           /* leveling & differing ? */
    pData->fDifferrow  = (mng_fptr)differ_g8;

  pData->iPass       = -1;
  pData->iRow        = 0;
  pData->iRowinc     = 1;
  pData->iCol        = 0;
  pData->iColinc     = 1;
  pData->iRowsamples = pData->iDatawidth;
  pData->iSamplemul  = 1;
  pData->iSampleofs  = 0;
  pData->iSamplediv  = 0;
  pData->iRowsize    = pData->iRowsamples;
  pData->iRowmax     = pData->iRowsize + pData->iPixelofs;
  pData->iFilterbpp  = 1;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_JPEG_A8_NI, MNG_LC_END)
#endif

  return init_rowproc (pData);
}

/* ************************************************************************** */

mng_retcode init_jpeg_a16_ni    (mng_datap pData)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_JPEG_A16_NI, MNG_LC_START)
#endif

  if (pData->pStoreobj)                /* store in object too ? */
  {
    if (pData->iJHDRimgbitdepth == 8)
    {
      switch (pData->iJHDRcolortype)
      {
        case 12 : { pData->fStorerow = (mng_fptr)store_jpeg_g8_a16;   break; }
        case 14 : { pData->fStorerow = (mng_fptr)store_jpeg_rgb8_a16; break; }
      }
    }

    /* TODO: bitdepth 12 & 20 */

  }

  if (pData->iFilter & 0x40)           /* leveling & differing ? */
    pData->fDifferrow  = (mng_fptr)differ_g16;

  pData->iPass       = -1;
  pData->iRow        = 0;
  pData->iRowinc     = 1;
  pData->iCol        = 0;
  pData->iColinc     = 1;
  pData->iRowsamples = pData->iDatawidth;
  pData->iSamplemul  = 2;
  pData->iSampleofs  = 0;
  pData->iSamplediv  = 0;
  pData->iRowsize    = pData->iRowsamples << 1;
  pData->iRowmax     = pData->iRowsize + pData->iPixelofs;
  pData->iFilterbpp  = 2;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_JPEG_A16_NI, MNG_LC_END)
#endif

  return init_rowproc (pData);
}

/* ************************************************************************** */

#endif /* MNG_INCLUDE_JNG */

/* ************************************************************************** */
/* *                                                                        * */
/* * Generic row processing initialization & cleanup routines               * */
/* * - initialize the buffers used by the row processing routines           * */
/* * - cleanup the buffers used by the row processing routines              * */
/* *                                                                        * */
/* ************************************************************************** */

mng_retcode init_rowproc (mng_datap pData)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_ROWPROC, MNG_LC_START)
#endif

  if (pData->pStoreobj)                /* storage object selected ? */
  {
    pData->pStorebuf = ((mng_imagep)pData->pStoreobj)->pImgbuf;
                                       /* and so it becomes viewable ! */
    ((mng_imagep)pData->pStoreobj)->bViewable     = MNG_TRUE;
    ((mng_imagedatap)pData->pStorebuf)->bViewable = MNG_TRUE;
  }

  /* allocate the buffers; the individual init routines have already
     calculated the required maximum size; except in the case of a JNG
     without alpha!!! */
  if (pData->iRowmax)
  {
    MNG_ALLOC (pData, pData->pWorkrow, pData->iRowmax)
    MNG_ALLOC (pData, pData->pPrevrow, pData->iRowmax)
  }

  /* allocate an RGBA16 row for intermediate processing */
  MNG_ALLOC (pData, pData->pRGBArow, (pData->iDatawidth << 3));

#ifndef MNG_NO_CMS
  if (pData->fDisplayrow)              /* display "on-the-fly" ? */
  {
#if defined(MNG_FULL_CMS)              /* determine color-management initialization */
    mng_retcode iRetcode = init_full_cms   (pData);
#elif defined(MNG_GAMMA_ONLY)
    mng_retcode iRetcode = init_gamma_only (pData);
#elif defined(MNG_APP_CMS)
    mng_retcode iRetcode = init_app_cms    (pData);
#endif
    if (iRetcode)                      /* on error bail out */
      return iRetcode;
  }
#endif /* !MNG_NO_CMS */

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_INIT_ROWPROC, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode next_row (mng_datap pData)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_NEXT_ROW, MNG_LC_START)
#endif

  pData->iRow += pData->iRowinc;       /* increase the row counter */

  if (pData->iPass >= 0)               /* interlaced ? */
  {
    while ((pData->iPass < 7) &&       /* went 'outside' the image ? */
           ((pData->iRow >= (mng_int32)pData->iDataheight) ||
            (pData->iCol >= (mng_int32)pData->iDatawidth )    ))
    {
      pData->iPass++;                  /* next pass ! */

      if (pData->iPass < 7)            /* there's only 7 passes ! */
      {
        pData->iRow        = interlace_row     [pData->iPass];
        pData->iRowinc     = interlace_rowskip [pData->iPass];
        pData->iCol        = interlace_col     [pData->iPass];
        pData->iColinc     = interlace_colskip [pData->iPass];
        pData->iRowsamples = (pData->iDatawidth - pData->iCol + interlace_roundoff [pData->iPass])
                                 >> interlace_divider [pData->iPass];

        if (pData->iSamplemul > 1)     /* recalculate row dimension */
          pData->iRowsize  = pData->iRowsamples * pData->iSamplemul;
        else
        if (pData->iSamplediv > 0)
          pData->iRowsize  = (pData->iRowsamples + pData->iSampleofs) >> pData->iSamplediv;
        else
          pData->iRowsize  = pData->iRowsamples;

      }

      if ((pData->iPass < 7) &&        /* reset previous row to zeroes ? */
          (pData->iRow  < (mng_int32)pData->iDataheight) &&
          (pData->iCol  < (mng_int32)pData->iDatawidth )    )
      {                                /* making sure the filters will work properly! */
        mng_int32  iX;
        mng_uint8p pTemp = pData->pPrevrow;

        for (iX = 0; iX < pData->iRowsize; iX++)
        {
          *pTemp = 0;
          pTemp++;
        }
      }
    }
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_NEXT_ROW, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode cleanup_rowproc (mng_datap pData)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CLEANUP_ROWPROC, MNG_LC_START)
#endif

#ifdef MNG_INCLUDE_LCMS                /* cleanup cms profile/transform */
  mng_retcode iRetcode = mng_clear_cms (pData);
  
  if (iRetcode)                        /* on error bail out */
    return iRetcode;
#endif /* MNG_INCLUDE_LCMS */

  if (pData->pWorkrow)                 /* cleanup buffer for working row */
    MNG_FREE (pData, pData->pWorkrow, pData->iRowmax)

  if (pData->pPrevrow)                 /* cleanup buffer for previous row */
    MNG_FREE (pData, pData->pPrevrow, pData->iRowmax)

  if (pData->pRGBArow)                 /* cleanup buffer for intermediate row */
    MNG_FREE (pData, pData->pRGBArow, (pData->iDatawidth << 3))

  pData->pWorkrow = 0;                 /* propogate uninitialized buffers */
  pData->pPrevrow = 0;
  pData->pRGBArow = 0;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_CLEANUP_ROWPROC, MNG_LC_END)
#endif

  return MNG_NOERROR;                  /* woohiii */
}

/* ************************************************************************** */
/* *                                                                        * */
/* * Generic row processing routines for JNG                                * */
/* *                                                                        * */
/* ************************************************************************** */

#ifdef MNG_INCLUDE_JNG

/* ************************************************************************** */

mng_retcode display_jpeg_rows (mng_datap pData)
{
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DISPLAY_JPEG_ROWS, MNG_LC_START)
#endif
                                       /* any completed rows ? */
  if ((pData->iJPEGrow      > pData->iJPEGdisprow) &&
      (pData->iJPEGalpharow > pData->iJPEGdisprow)    )
  {
    mng_uint32 iX, iMax;
    mng_uint32 iSaverow = pData->iRow; /* save alpha decompression row-count */
                                       /* determine the highest complete(!) row */
    if (pData->iJPEGrow > pData->iJPEGalpharow)
      iMax = pData->iJPEGalpharow;
    else
      iMax = pData->iJPEGrow;
                                       /* display the rows */
    for (iX = pData->iJPEGdisprow; iX < iMax; iX++)
    {
      pData->iRow = iX;                /* make sure we all know which row to handle */
                                       /* makeup an intermediate row from the buffer */
      iRetcode = ((mng_retrieverow)pData->fRetrieverow) (pData);
                                       /* color-correct it if necessary */
      if ((!iRetcode) && (pData->fCorrectrow))
        iRetcode = ((mng_correctrow)pData->fCorrectrow) (pData);

      if (!iRetcode)                   /* and display it */
      {
        iRetcode = ((mng_displayrow)pData->fDisplayrow) (pData);

        if (!iRetcode)                 /* check progressive display refresh */
          iRetcode = display_progressive_check (pData);
      }

      if (iRetcode)                    /* on error bail out */
        return iRetcode;
    }

    pData->iJPEGdisprow = iMax;        /* keep track of the last displayed row */
    pData->iRow         = iSaverow;    /* restore alpha decompression row-count */
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_DISPLAY_JPEG_ROWS, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode next_jpeg_alpharow (mng_datap pData)
{
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_NEXT_JPEG_ALPHAROW, MNG_LC_START)
#endif

  pData->iJPEGalpharow++;              /* count the row */

  if (pData->fDisplayrow)              /* display "on-the-fly" ? */
  {                                    /* try to display what you can */
    iRetcode = display_jpeg_rows (pData);

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_NEXT_JPEG_ALPHAROW, MNG_LC_END)
#endif

  return MNG_NOERROR; 
}

/* ************************************************************************** */

mng_retcode next_jpeg_row (mng_datap pData)
{
  mng_retcode iRetcode;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_NEXT_JPEG_ROW, MNG_LC_START)
#endif

  pData->iJPEGrow++;                   /* increase the row-counter */
  
  if (pData->fDisplayrow)              /* display "on-the-fly" ? */
  {                                    /* has alpha channel ? */
    if ((pData->iJHDRcolortype == MNG_COLORTYPE_JPEGGRAYA ) ||
        (pData->iJHDRcolortype == MNG_COLORTYPE_JPEGCOLORA)    )
    {                                  /* try to display what you can */
      iRetcode = display_jpeg_rows (pData);
    }
    else
    {                                  /* make sure we all know which row to handle */
      pData->iRow = pData->iJPEGrow - 1;
                                       /* makeup an intermediate row from the buffer */
      iRetcode = ((mng_retrieverow)pData->fRetrieverow) (pData);
                                       /* color-correct it if necessary */
      if ((!iRetcode) && (pData->fCorrectrow))
        iRetcode = ((mng_correctrow)pData->fCorrectrow) (pData);

      if (!iRetcode)                   /* and display it */
      {
        iRetcode = ((mng_displayrow)pData->fDisplayrow) (pData);

        if (!iRetcode)                 /* check progressive display refresh */
          iRetcode = display_progressive_check (pData);
      }
    }

    if (iRetcode)                      /* on error bail out */
      return iRetcode;
  }

                                       /* surpassed last filled row ? */
  if (pData->iJPEGrow > pData->iJPEGrgbrow)
    pData->iJPEGrgbrow = pData->iJPEGrow;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_NEXT_JPEG_ROW, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

#endif /* MNG_INCLUDE_JNG */

/* ************************************************************************** */

mng_retcode magnify_g8_x1 (mng_datap  pData,
                           mng_uint16 iMX,
                           mng_uint16 iML,
                           mng_uint16 iMR,
                           mng_uint32 iWidth,
                           mng_uint8p pSrcline,
                           mng_uint8p pDstline)
{
  mng_uint32 iX, iS, iM;
  mng_uint8p pTempsrc1;
  mng_uint8p pTempdst;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_G8_X1, MNG_LC_START)
#endif

  pTempsrc1 = pSrcline;                /* initialize pixel-loop */
  pTempdst  = pDstline;

  for (iX = 0; iX < iWidth; iX++)
  {
    *pTempdst = *pTempsrc1;            /* copy original source pixel */
    pTempdst++;

    if (iX == 0)                       /* first interval ? */
      iM = iML;
    else
    if (iX == (iWidth - 1))            /* last interval ? */
      iM = iMR;
    else
      iM = iMX;

    for (iS = 1; iS < iM; iS++)        /* fill interval */
    {
      *pTempdst = *pTempsrc1;          /* copy original source pixel */
      pTempdst++;
    }

    pTempsrc1++;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_G8_X1, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode magnify_g8_x2 (mng_datap  pData,
                           mng_uint16 iMX,
                           mng_uint16 iML,
                           mng_uint16 iMR,
                           mng_uint32 iWidth,
                           mng_uint8p pSrcline,
                           mng_uint8p pDstline)
{
  mng_uint32 iX;
  mng_int32  iS, iM;
  mng_uint8p pTempsrc1;
  mng_uint8p pTempsrc2;
  mng_uint8p pTempdst;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_G8_X2, MNG_LC_START)
#endif

  pTempsrc1 = pSrcline;                /* initialize pixel-loop */
  pTempdst  = pDstline;

  for (iX = 0; iX < iWidth; iX++)
  {
    pTempsrc2 = pTempsrc1 + 1;

    *pTempdst = *pTempsrc1;            /* copy original source pixel */
    pTempdst++;

    if (iX == 0)                       /* first interval ? */
    {
      if (iWidth == 1)                 /* single pixel ? */
        pTempsrc2 = MNG_NULL;

      iM = iML;
    }
    else
    if (iX == (iWidth - 2))            /* last interval ? */
      iM = iMR;
    else
      iM = iMX;
                                       /* fill interval ? */
    if ((iX < iWidth - 1) || (iWidth == 1))
    {
      if (pTempsrc2)                   /* do we have the second pixel ? */
      {                                /* is it same as first ? */
        if (*pTempsrc1 == *pTempsrc2)
        {
          for (iS = 1; iS < iM; iS++)  /* then just repeat the first */
          {
            *pTempdst = *pTempsrc1;
            pTempdst++;
          }
        }
        else
        {
          for (iS = 1; iS < iM; iS++)  /* calculate the distances */
          {
            *pTempdst = (mng_uint8)(((2 * iS * ( (mng_int32)(*pTempsrc2)     -
                                                 (mng_int32)(*pTempsrc1)     ) + iM) /
                                     (iM * 2)) + (mng_int32)(*pTempsrc1)             );
            pTempdst++;
          }
        }
      }
      else
      {
        for (iS = 1; iS < iM; iS++)
        {
          *pTempdst = *pTempsrc1;      /* repeat first source pixel */
          pTempdst++;
        }
      }
    }

    pTempsrc1++;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_G8_X2, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode magnify_g8_x3 (mng_datap  pData,
                           mng_uint16 iMX,
                           mng_uint16 iML,
                           mng_uint16 iMR,
                           mng_uint32 iWidth,
                           mng_uint8p pSrcline,
                           mng_uint8p pDstline)
{
  mng_uint32 iX;
  mng_int32  iS, iM, iH;
  mng_uint8p pTempsrc1;
  mng_uint8p pTempsrc2;
  mng_uint8p pTempdst;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_G8_X3, MNG_LC_START)
#endif

  pTempsrc1 = pSrcline;                /* initialize pixel-loop */
  pTempdst  = pDstline;

  for (iX = 0; iX < iWidth; iX++)
  {
    pTempsrc2 = pTempsrc1 + 1;

    *pTempdst = *pTempsrc1;            /* copy original source pixel */
    pTempdst++;

    if (iX == 0)                       /* first interval ? */
    {
      if (iWidth == 1)                 /* single pixel ? */
        pTempsrc2 = MNG_NULL;

      iM = iML;
    }
    else
    if (iX == (iWidth - 2))            /* last interval ? */
      iM = iMR;
    else
      iM = iMX;
                                       /* fill interval ? */
    if ((iX < iWidth - 1) || (iWidth == 1))
    {
      if (pTempsrc2)                   /* do we have the second pixel ? */
      {                                /* is it same as first ? */
        if (*pTempsrc1 == *pTempsrc2)
        {
          for (iS = 1; iS < iM; iS++)  /* then just repeat the first */
          {
            *pTempdst = *pTempsrc1;
            pTempdst++;
          }
        }
        else
        {
          iH = (iM+1) / 2;             /* calculate halfway point */

          for (iS = 1; iS < iH; iS++)  /* replicate first half */
          {
            *pTempdst = *pTempsrc1;
            pTempdst++;
          }

          for (iS = iH; iS < iM; iS++) /* replicate second half */
          {
            *pTempdst = *pTempsrc2;
            pTempdst++;
          }
        }
      }
      else
      {
        for (iS = 1; iS < iM; iS++)
        {
          *pTempdst = *pTempsrc1;      /* repeat first source pixel */
          pTempdst++;
        }
      }
    }

    pTempsrc1++;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_G8_X3, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode magnify_rgb8_x1 (mng_datap  pData,
                             mng_uint16 iMX,
                             mng_uint16 iML,
                             mng_uint16 iMR,
                             mng_uint32 iWidth,
                             mng_uint8p pSrcline,
                             mng_uint8p pDstline)
{
  mng_uint32 iX, iS, iM;
  mng_uint8p pTempsrc1;
  mng_uint8p pTempdst;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_RGB8_X1, MNG_LC_START)
#endif

  pTempsrc1 = pSrcline;                /* initialize pixel-loop */
  pTempdst  = pDstline;

  for (iX = 0; iX < iWidth; iX++)
  {
    *pTempdst = *pTempsrc1;            /* copy original source pixel */
    pTempdst++;
    *pTempdst = *(pTempsrc1+1);
    pTempdst++;
    *pTempdst = *(pTempsrc1+2);
    pTempdst++;

    if (iX == 0)                       /* first interval ? */
      iM = iML;
    else
    if (iX == (iWidth - 1))            /* last interval ? */
      iM = iMR;
    else
      iM = iMX;

    for (iS = 1; iS < iM; iS++)        /* fill interval */
    {
      *pTempdst = *pTempsrc1;          /* copy original source pixel */
      pTempdst++;
      *pTempdst = *(pTempsrc1+1);
      pTempdst++;
      *pTempdst = *(pTempsrc1+2);
      pTempdst++;
    }

    pTempsrc1 += 3;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_RGB8_X1, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode magnify_rgb8_x2 (mng_datap  pData,
                             mng_uint16 iMX,
                             mng_uint16 iML,
                             mng_uint16 iMR,
                             mng_uint32 iWidth,
                             mng_uint8p pSrcline,
                             mng_uint8p pDstline)
{
  mng_uint32 iX;
  mng_int32  iS, iM;
  mng_uint8p pTempsrc1;
  mng_uint8p pTempsrc2;
  mng_uint8p pTempdst;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_RGB8_X2, MNG_LC_START)
#endif

  pTempsrc1 = pSrcline;                /* initialize pixel-loop */
  pTempdst  = pDstline;

  for (iX = 0; iX < iWidth; iX++)
  {
    pTempsrc2 = pTempsrc1 + 3;

    *pTempdst = *pTempsrc1;            /* copy original source pixel */
    pTempdst++;
    *pTempdst = *(pTempsrc1+1);
    pTempdst++;
    *pTempdst = *(pTempsrc1+2);
    pTempdst++;

    if (iX == 0)                       /* first interval ? */
    {
      if (iWidth == 1)                 /* single pixel ? */
        pTempsrc2 = MNG_NULL;

      iM = (mng_int32)iML;
    }
    else
    if (iX == (iWidth - 2))            /* last interval ? */
      iM = (mng_int32)iMR;
    else
      iM = (mng_int32)iMX;
                                       /* fill interval ? */
    if ((iX < iWidth - 1) || (iWidth == 1))
    {
      if (pTempsrc2)                   /* do we have the second pixel ? */
      {
        for (iS = 1; iS < iM; iS++)
        {
          if (*pTempsrc1 == *pTempsrc2)
            *pTempdst = *pTempsrc1;    /* just repeat the first */
          else                         /* calculate the distance */
            *pTempdst = (mng_uint8)(((2 * iS * ( (mng_int32)(*pTempsrc2)     -
                                                 (mng_int32)(*pTempsrc1)     ) + iM) /
                                     (iM * 2)) + (mng_int32)(*pTempsrc1)             );

          pTempdst++;

          if (*(pTempsrc1+1) == *(pTempsrc2+1))
            *pTempdst = *(pTempsrc1+1);
          else
            *pTempdst = (mng_uint8)(((2 * iS * ( (mng_int32)(*(pTempsrc2+1)) -
                                                 (mng_int32)(*(pTempsrc1+1)) ) + iM) /
                                     (iM * 2)) + (mng_int32)(*(pTempsrc1+1))         );

          pTempdst++;

          if (*(pTempsrc1+2) == *(pTempsrc2+2))
            *pTempdst = *(pTempsrc1+2);
          else
            *pTempdst = (mng_uint8)(((2 * iS * ( (mng_int32)(*(pTempsrc2+2)) -
                                                 (mng_int32)(*(pTempsrc1+2)) ) + iM) /
                                     (iM * 2)) + (mng_int32)(*(pTempsrc1+2))         );

          pTempdst++;
        }
      }
      else
      {
        for (iS = 1; iS < iM; iS++)
        {
          *pTempdst = *pTempsrc1;      /* repeat first source pixel */
          pTempdst++;
          *pTempdst = *(pTempsrc1+1);
          pTempdst++;
          *pTempdst = *(pTempsrc1+2);
          pTempdst++;
        }
      }
    }

    pTempsrc1 += 3;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_RGB8_X2, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode magnify_rgb8_x3 (mng_datap  pData,
                             mng_uint16 iMX,
                             mng_uint16 iML,
                             mng_uint16 iMR,
                             mng_uint32 iWidth,
                             mng_uint8p pSrcline,
                             mng_uint8p pDstline)
{
  mng_uint32 iX;
  mng_int32  iS, iM, iH;
  mng_uint8p pTempsrc1;
  mng_uint8p pTempsrc2;
  mng_uint8p pTempdst;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_RGB8_X3, MNG_LC_START)
#endif

  pTempsrc1 = pSrcline;                /* initialize pixel-loop */
  pTempdst  = pDstline;

  for (iX = 0; iX < iWidth; iX++)
  {
    pTempsrc2 = pTempsrc1 + 3;

    *pTempdst = *pTempsrc1;            /* copy original source pixel */
    pTempdst++;
    *pTempdst = *(pTempsrc1+1);
    pTempdst++;
    *pTempdst = *(pTempsrc1+2);
    pTempdst++;

    if (iX == 0)                       /* first interval ? */
    {
      if (iWidth == 1)                 /* single pixel ? */
        pTempsrc2 = MNG_NULL;

      iM = (mng_int32)iML;
    }
    else
    if (iX == (iWidth - 2))            /* last interval ? */
      iM = (mng_int32)iMR;
    else
      iM = (mng_int32)iMX;
                                       /* fill interval ? */
    if ((iX < iWidth - 1) || (iWidth == 1))
    {
      if (pTempsrc2)                   /* do we have the second pixel ? */
      {
        iH = (iM+1) / 2;               /* calculate halfway point */ 

        for (iS = 1; iS < iH; iS++)    /* replicate first half */
        {
          *pTempdst     = *pTempsrc1;
          *(pTempdst+1) = *(pTempsrc1+1);
          *(pTempdst+2) = *(pTempsrc1+2);

          pTempdst += 3;
        }

        for (iS = iH; iS < iM; iS++)    /* replicate second half */
        {
          *pTempdst     = *pTempsrc2;
          *(pTempdst+1) = *(pTempsrc2+1);
          *(pTempdst+2) = *(pTempsrc2+2);

          pTempdst += 3;
        }
      }
      else
      {
        for (iS = 1; iS < iM; iS++)
        {
          *pTempdst = *pTempsrc1;      /* repeat first source pixel */
          pTempdst++;
          *pTempdst = *(pTempsrc1+1);
          pTempdst++;
          *pTempdst = *(pTempsrc1+2);
          pTempdst++;
        }
      }
    }

    pTempsrc1 += 3;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_RGB8_X3, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode magnify_ga8_x1 (mng_datap  pData,
                            mng_uint16 iMX,
                            mng_uint16 iML,
                            mng_uint16 iMR,
                            mng_uint32 iWidth,
                            mng_uint8p pSrcline,
                            mng_uint8p pDstline)
{
  mng_uint32 iX, iS, iM;
  mng_uint8p pTempsrc1;
  mng_uint8p pTempdst;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_GA8_X1, MNG_LC_START)
#endif

  pTempsrc1 = pSrcline;                /* initialize pixel-loop */
  pTempdst  = pDstline;

  for (iX = 0; iX < iWidth; iX++)
  {
    *pTempdst = *pTempsrc1;            /* copy original source pixel */
    pTempdst++;
    *pTempdst = *(pTempsrc1+1);
    pTempdst++;

    if (iX == 0)                       /* first interval ? */
      iM = iML;
    else
    if (iX == (iWidth - 1))            /* last interval ? */
      iM = iMR;
    else
      iM = iMX;

    for (iS = 1; iS < iM; iS++)        /* fill interval */
    {
      *pTempdst = *pTempsrc1;          /* copy original source pixel */
      pTempdst++;
      *pTempdst = *(pTempsrc1+1);
      pTempdst++;
    }

    pTempsrc1 += 2;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_GA8_X1, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode magnify_ga8_x2 (mng_datap  pData,
                            mng_uint16 iMX,
                            mng_uint16 iML,
                            mng_uint16 iMR,
                            mng_uint32 iWidth,
                            mng_uint8p pSrcline,
                            mng_uint8p pDstline)
{
  mng_uint32 iX;
  mng_int32  iS, iM;
  mng_uint8p pTempsrc1;
  mng_uint8p pTempsrc2;
  mng_uint8p pTempdst;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_GA8_X2, MNG_LC_START)
#endif

  pTempsrc1 = pSrcline;                /* initialize pixel-loop */
  pTempdst  = pDstline;

  for (iX = 0; iX < iWidth; iX++)
  {
    pTempsrc2 = pTempsrc1 + 2;

    *pTempdst = *pTempsrc1;            /* copy original source pixel */
    pTempdst++;
    *pTempdst = *(pTempsrc1+1);
    pTempdst++;

    if (iX == 0)                       /* first interval ? */
    {
      if (iWidth == 1)                 /* single pixel ? */
        pTempsrc2 = MNG_NULL;

      iM = iML;
    }
    else
    if (iX == (iWidth - 2))            /* last interval ? */
      iM = iMR;
    else
      iM = iMX;
                                       /* fill interval ? */
    if ((iX < iWidth - 1) || (iWidth == 1))
    {
      if (pTempsrc2)                   /* do we have the second pixel ? */
      {
        for (iS = 1; iS < iM; iS++)
        {
          if (*pTempsrc1 == *pTempsrc2)
            *pTempdst = *pTempsrc1;    /* just repeat the first */
          else                         /* calculate the distance */
            *pTempdst = (mng_uint8)(((2 * iS * ( (mng_int32)(*pTempsrc2)     -
                                                 (mng_int32)(*pTempsrc1)     ) + iM) /
                                     (iM * 2)) + (mng_int32)(*pTempsrc1)             );

          pTempdst++;

          if (*(pTempsrc1+1) == *(pTempsrc2+1))
            *pTempdst = *(pTempsrc1+1);
          else
            *pTempdst = (mng_uint8)(((2 * iS * ( (mng_int32)(*(pTempsrc2+1)) -
                                                 (mng_int32)(*(pTempsrc1+1)) ) + iM) /
                                     (iM * 2)) + (mng_int32)(*(pTempsrc1+1))         );

          pTempdst++;
        }
      }
      else
      {
        for (iS = 1; iS < iM; iS++)
        {
          *pTempdst = *pTempsrc1;      /* repeat first source pixel */
          pTempdst++;
          *pTempdst = *(pTempsrc1+1);
          pTempdst++;
        }
      }
    }

    pTempsrc1 += 2;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_GA8_X2, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode magnify_ga8_x3 (mng_datap  pData,
                            mng_uint16 iMX,
                            mng_uint16 iML,
                            mng_uint16 iMR,
                            mng_uint32 iWidth,
                            mng_uint8p pSrcline,
                            mng_uint8p pDstline)
{
  mng_uint32 iX;
  mng_int32  iS, iM, iH;
  mng_uint8p pTempsrc1;
  mng_uint8p pTempsrc2;
  mng_uint8p pTempdst;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_GA8_X3, MNG_LC_START)
#endif

  pTempsrc1 = pSrcline;                /* initialize pixel-loop */
  pTempdst  = pDstline;

  for (iX = 0; iX < iWidth; iX++)
  {
    pTempsrc2 = pTempsrc1 + 2;

    *pTempdst = *pTempsrc1;            /* copy original source pixel */
    pTempdst++;
    *pTempdst = *(pTempsrc1+1);
    pTempdst++;

    if (iX == 0)                       /* first interval ? */
    {
      if (iWidth == 1)                 /* single pixel ? */
        pTempsrc2 = MNG_NULL;

      iM = iML;
    }
    else
    if (iX == (iWidth - 2))            /* last interval ? */
      iM = iMR;
    else
      iM = iMX;
                                       /* fill interval ? */
    if ((iX < iWidth - 1) || (iWidth == 1))
    {
      if (pTempsrc2)                   /* do we have the second pixel ? */
      {
        iH = (iM+1) / 2;               /* calculate halfway point */

        for (iS = 1; iS < iH; iS++)    /* replicate first half */
        {
          *pTempdst     = *pTempsrc1;
          *(pTempdst+1) = *(pTempsrc1+1);

          pTempdst += 2;
        }

        for (iS = iH; iS < iM; iS++)   /* replicate second half */
        {
          *pTempdst     = *pTempsrc2;
          *(pTempdst+1) = *(pTempsrc2+1);

          pTempdst += 2;
        }
      }
      else
      {
        for (iS = 1; iS < iM; iS++)
        {
          *pTempdst = *pTempsrc1;      /* repeat first source pixel */
          pTempdst++;
          *pTempdst = *(pTempsrc1+1);
          pTempdst++;
        }
      }
    }

    pTempsrc1 += 2;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_GA8_X3, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode magnify_ga8_x4 (mng_datap  pData,
                            mng_uint16 iMX,
                            mng_uint16 iML,
                            mng_uint16 iMR,
                            mng_uint32 iWidth,
                            mng_uint8p pSrcline,
                            mng_uint8p pDstline)
{
  mng_uint32 iX;
  mng_int32  iS, iM, iH;
  mng_uint8p pTempsrc1;
  mng_uint8p pTempsrc2;
  mng_uint8p pTempdst;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_GA8_X4, MNG_LC_START)
#endif

  pTempsrc1 = pSrcline;                /* initialize pixel-loop */
  pTempdst  = pDstline;

  for (iX = 0; iX < iWidth; iX++)
  {
    pTempsrc2 = pTempsrc1 + 2;

    *pTempdst = *pTempsrc1;            /* copy original source pixel */
    pTempdst++;
    *pTempdst = *(pTempsrc1+1);
    pTempdst++;

    if (iX == 0)                       /* first interval ? */
    {
      if (iWidth == 1)                 /* single pixel ? */
        pTempsrc2 = MNG_NULL;

      iM = iML;
    }
    else
    if (iX == (iWidth - 2))            /* last interval ? */
      iM = iMR;
    else
      iM = iMX;
                                       /* fill interval ? */
    if ((iX < iWidth - 1) || (iWidth == 1))
    {
      if (pTempsrc2)                   /* do we have the second pixel ? */
      {
        iH = (iM+1) / 2;               /* calculate halfway point */

        for (iS = 1; iS < iH; iS++)    /* first half */
        {
          if (*pTempsrc1 == *pTempsrc2)
            *pTempdst = *pTempsrc1;    /* just repeat the first */
          else                         /* calculate the distance */
            *pTempdst = (mng_uint8)(((2 * iS * ( (mng_int32)(*pTempsrc2)     -
                                                 (mng_int32)(*pTempsrc1)     ) + iM) /
                                     (iM * 2)) + (mng_int32)(*pTempsrc1)             );

          pTempdst++;

          *pTempdst = *(pTempsrc1+1);  /* replicate alpha from left */

          pTempdst++;
        }

        for (iS = iH; iS < iM; iS++)   /* second half */
        {
          if (*pTempsrc1 == *pTempsrc2)
            *pTempdst = *pTempsrc1;    /* just repeat the first */
          else                         /* calculate the distance */
            *pTempdst = (mng_uint8)(((2 * iS * ( (mng_int32)(*pTempsrc2)     -
                                                 (mng_int32)(*pTempsrc1)     ) + iM) /
                                     (iM * 2)) + (mng_int32)(*pTempsrc1)             );

          pTempdst++;

          *pTempdst = *(pTempsrc2+1);  /* replicate alpha from right */

          pTempdst++;
        }
      }
      else
      {
        for (iS = 1; iS < iM; iS++)
        {
          *pTempdst = *pTempsrc1;      /* repeat first source pixel */
          pTempdst++;
          *pTempdst = *(pTempsrc1+1);
          pTempdst++;
        }
      }
    }

    pTempsrc1 += 2;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_GA8_X4, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode magnify_ga8_x5 (mng_datap  pData,
                            mng_uint16 iMX,
                            mng_uint16 iML,
                            mng_uint16 iMR,
                            mng_uint32 iWidth,
                            mng_uint8p pSrcline,
                            mng_uint8p pDstline)
{
  mng_uint32 iX;
  mng_int32  iS, iM, iH;
  mng_uint8p pTempsrc1;
  mng_uint8p pTempsrc2;
  mng_uint8p pTempdst;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_GA8_X5, MNG_LC_START)
#endif

  pTempsrc1 = pSrcline;                /* initialize pixel-loop */
  pTempdst  = pDstline;

  for (iX = 0; iX < iWidth; iX++)
  {
    pTempsrc2 = pTempsrc1 + 2;

    *pTempdst = *pTempsrc1;            /* copy original source pixel */
    pTempdst++;
    *pTempdst = *(pTempsrc1+1);
    pTempdst++;

    if (iX == 0)                       /* first interval ? */
    {
      if (iWidth == 1)                 /* single pixel ? */
        pTempsrc2 = MNG_NULL;

      iM = iML;
    }
    else
    if (iX == (iWidth - 2))            /* last interval ? */
      iM = iMR;
    else
      iM = iMX;
                                       /* fill interval ? */
    if ((iX < iWidth - 1) || (iWidth == 1))
    {
      if (pTempsrc2)                   /* do we have the second pixel ? */
      {
        iH = (iM+1) / 2;               /* calculate halfway point */

        for (iS = 1; iS < iH; iS++)    /* first half */
        {
          *pTempdst = *pTempsrc1;      /* replicate gray from left */

          pTempdst++;

          if (*(pTempsrc1+1) == *(pTempsrc2+1))
            *pTempdst = *(pTempsrc1+1);/* just repeat the first */
          else                         /* calculate the distance */
            *pTempdst = (mng_uint8)(((2 * iS * ( (mng_int32)(*(pTempsrc2+1))     -
                                                 (mng_int32)(*(pTempsrc1+1))     ) + iM) /
                                     (iM * 2)) + (mng_int32)(*(pTempsrc1+1))             );

          pTempdst++;
        }

        for (iS = iH; iS < iM; iS++)   /* second half */
        {
          *pTempdst = *pTempsrc2;      /* replicate gray from right */

          pTempdst++;

          if (*(pTempsrc1+1) == *(pTempsrc2+1))
            *pTempdst = *(pTempsrc1+1);/* just repeat the first */
          else                         /* calculate the distance */
            *pTempdst = (mng_uint8)(((2 * iS * ( (mng_int32)(*(pTempsrc2+1))     -
                                                 (mng_int32)(*(pTempsrc1+1))     ) + iM) /
                                     (iM * 2)) + (mng_int32)(*(pTempsrc1+1))             );

          pTempdst++;
        }
      }
      else
      {
        for (iS = 1; iS < iM; iS++)
        {
          *pTempdst = *pTempsrc1;      /* repeat first source pixel */
          pTempdst++;
          *pTempdst = *(pTempsrc1+1);
          pTempdst++;
        }
      }
    }

    pTempsrc1 += 2;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_GA8_X5, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode magnify_rgba8_x1 (mng_datap  pData,
                              mng_uint16 iMX,
                              mng_uint16 iML,
                              mng_uint16 iMR,
                              mng_uint32 iWidth,
                              mng_uint8p pSrcline,
                              mng_uint8p pDstline)
{
  mng_uint32 iX, iS, iM;
  mng_uint8p pTempsrc1;
  mng_uint8p pTempdst;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_RGBA8_X1, MNG_LC_START)
#endif

  pTempsrc1 = pSrcline;                /* initialize pixel-loop */
  pTempdst  = pDstline;

  for (iX = 0; iX < iWidth; iX++)
  {
    *pTempdst = *pTempsrc1;            /* copy original source pixel */
    pTempdst++;
    *pTempdst = *(pTempsrc1+1);
    pTempdst++;
    *pTempdst = *(pTempsrc1+2);
    pTempdst++;
    *pTempdst = *(pTempsrc1+3);
    pTempdst++;

    if (iX == 0)                       /* first interval ? */
      iM = iML;
    else
    if (iX == (iWidth - 1))            /* last interval ? */
      iM = iMR;
    else
      iM = iMX;

    for (iS = 1; iS < iM; iS++)        /* fill interval */
    {
      *pTempdst = *pTempsrc1;          /* copy original source pixel */
      pTempdst++;
      *pTempdst = *(pTempsrc1+1);
      pTempdst++;
      *pTempdst = *(pTempsrc1+2);
      pTempdst++;
      *pTempdst = *(pTempsrc1+3);
      pTempdst++;
    }

    pTempsrc1 += 4;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_RGBA8_X1, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode magnify_rgba8_x2 (mng_datap  pData,
                              mng_uint16 iMX,
                              mng_uint16 iML,
                              mng_uint16 iMR,
                              mng_uint32 iWidth,
                              mng_uint8p pSrcline,
                              mng_uint8p pDstline)
{
  mng_uint32 iX;
  mng_int32  iS, iM;
  mng_uint8p pTempsrc1;
  mng_uint8p pTempsrc2;
  mng_uint8p pTempdst;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_RGBA8_X2, MNG_LC_START)
#endif

  pTempsrc1 = pSrcline;                /* initialize pixel-loop */
  pTempdst  = pDstline;

  for (iX = 0; iX < iWidth; iX++)
  {
    pTempsrc2 = pTempsrc1 + 4;

    *pTempdst = *pTempsrc1;            /* copy original source pixel */
    pTempdst++;
    *pTempdst = *(pTempsrc1+1);
    pTempdst++;
    *pTempdst = *(pTempsrc1+2);
    pTempdst++;
    *pTempdst = *(pTempsrc1+3);
    pTempdst++;

    if (iX == 0)                       /* first interval ? */
    {
      if (iWidth == 1)                 /* single pixel ? */
        pTempsrc2 = MNG_NULL;

      iM = (mng_int32)iML;
    }
    else
    if (iX == (iWidth - 2))            /* last interval ? */
      iM = (mng_int32)iMR;
    else
      iM = (mng_int32)iMX;
                                       /* fill interval ? */
    if ((iX < iWidth - 1) || (iWidth == 1))
    {
      if (pTempsrc2)                   /* do we have the second pixel ? */
      {
        for (iS = 1; iS < iM; iS++)
        {
          if (*pTempsrc1 == *pTempsrc2)
            *pTempdst = *pTempsrc1;    /* just repeat the first */
          else                         /* calculate the distance */
            *pTempdst = (mng_uint8)(((2 * iS * ( (mng_int32)(*pTempsrc2)     -
                                                 (mng_int32)(*pTempsrc1)     ) + iM) /
                                     (iM * 2)) + (mng_int32)(*pTempsrc1)             );

          pTempdst++;

          if (*(pTempsrc1+1) == *(pTempsrc2+1))
            *pTempdst = *(pTempsrc1+1);
          else
            *pTempdst = (mng_uint8)(((2 * iS * ( (mng_int32)(*(pTempsrc2+1)) -
                                                 (mng_int32)(*(pTempsrc1+1)) ) + iM) /
                                     (iM * 2)) + (mng_int32)(*(pTempsrc1+1))         );

          pTempdst++;

          if (*(pTempsrc1+2) == *(pTempsrc2+2))
            *pTempdst = *(pTempsrc1+2);
          else
            *pTempdst = (mng_uint8)(((2 * iS * ( (mng_int32)(*(pTempsrc2+2)) -
                                                 (mng_int32)(*(pTempsrc1+2)) ) + iM) /
                                     (iM * 2)) + (mng_int32)(*(pTempsrc1+2))         );

          pTempdst++;

          if (*(pTempsrc1+3) == *(pTempsrc2+3))
            *pTempdst = *(pTempsrc1+3);
          else
            *pTempdst = (mng_uint8)(((2 * iS * ( (mng_int32)(*(pTempsrc2+3)) -
                                                 (mng_int32)(*(pTempsrc1+3)) ) + iM) /
                                     (iM * 2)) + (mng_int32)(*(pTempsrc1+3))         );

          pTempdst++;
        }
      }
      else
      {
        for (iS = 1; iS < iM; iS++)
        {
          *pTempdst = *pTempsrc1;      /* repeat first source pixel */
          pTempdst++;
          *pTempdst = *(pTempsrc1+1);
          pTempdst++;
          *pTempdst = *(pTempsrc1+2);
          pTempdst++;
          *pTempdst = *(pTempsrc1+3);
          pTempdst++;
        }
      }
    }

    pTempsrc1 += 4;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_RGBA8_X2, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode magnify_rgba8_x3 (mng_datap  pData,
                              mng_uint16 iMX,
                              mng_uint16 iML,
                              mng_uint16 iMR,
                              mng_uint32 iWidth,
                              mng_uint8p pSrcline,
                              mng_uint8p pDstline)
{
  mng_uint32 iX;
  mng_int32  iS, iM, iH;
  mng_uint8p pTempsrc1;
  mng_uint8p pTempsrc2;
  mng_uint8p pTempdst;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_RGBA8_X3, MNG_LC_START)
#endif

  pTempsrc1 = pSrcline;                /* initialize pixel-loop */
  pTempdst  = pDstline;

  for (iX = 0; iX < iWidth; iX++)
  {
    pTempsrc2 = pTempsrc1 + 4;

    *pTempdst = *pTempsrc1;            /* copy original source pixel */
    pTempdst++;
    *pTempdst = *(pTempsrc1+1);
    pTempdst++;
    *pTempdst = *(pTempsrc1+2);
    pTempdst++;
    *pTempdst = *(pTempsrc1+3);
    pTempdst++;

    if (iX == 0)                       /* first interval ? */
    {
      if (iWidth == 1)                 /* single pixel ? */
        pTempsrc2 = MNG_NULL;

      iM = (mng_int32)iML;
    }
    else
    if (iX == (iWidth - 2))            /* last interval ? */
      iM = (mng_int32)iMR;
    else
      iM = (mng_int32)iMX;
                                       /* fill interval ? */
    if ((iX < iWidth - 1) || (iWidth == 1))
    {
      if (pTempsrc2)                   /* do we have the second pixel ? */
      {
        iH = (iM+1) / 2;               /* calculate halfway point */

        for (iS = 1; iS < iH; iS++)    /* replicate first half */
        {
          *pTempdst     = *pTempsrc1;
          *(pTempdst+1) = *(pTempsrc1+1);
          *(pTempdst+2) = *(pTempsrc1+2);
          *(pTempdst+3) = *(pTempsrc1+3);

          pTempdst += 4;
        }

        for (iS = iH; iS < iM; iS++)   /* replicate second half */
        {
          *pTempdst     = *pTempsrc2;
          *(pTempdst+1) = *(pTempsrc2+1);
          *(pTempdst+2) = *(pTempsrc2+2);
          *(pTempdst+3) = *(pTempsrc2+3);

          pTempdst += 4;
        }
      }
      else
      {
        for (iS = 1; iS < iM; iS++)
        {
          *pTempdst = *pTempsrc1;      /* repeat first source pixel */
          pTempdst++;
          *pTempdst = *(pTempsrc1+1);
          pTempdst++;
          *pTempdst = *(pTempsrc1+2);
          pTempdst++;
          *pTempdst = *(pTempsrc1+3);
          pTempdst++;
        }
      }
    }

    pTempsrc1 += 4;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_RGBA8_X3, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode magnify_rgba8_x4 (mng_datap  pData,
                              mng_uint16 iMX,
                              mng_uint16 iML,
                              mng_uint16 iMR,
                              mng_uint32 iWidth,
                              mng_uint8p pSrcline,
                              mng_uint8p pDstline)
{
  mng_uint32 iX;
  mng_int32  iS, iM, iH;
  mng_uint8p pTempsrc1;
  mng_uint8p pTempsrc2;
  mng_uint8p pTempdst;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_RGBA8_X4, MNG_LC_START)
#endif

  pTempsrc1 = pSrcline;                /* initialize pixel-loop */
  pTempdst  = pDstline;

  for (iX = 0; iX < iWidth; iX++)
  {
    pTempsrc2 = pTempsrc1 + 4;

    *pTempdst = *pTempsrc1;            /* copy original source pixel */
    pTempdst++;
    *pTempdst = *(pTempsrc1+1);
    pTempdst++;
    *pTempdst = *(pTempsrc1+2);
    pTempdst++;
    *pTempdst = *(pTempsrc1+3);
    pTempdst++;

    if (iX == 0)                       /* first interval ? */
    {
      if (iWidth == 1)                 /* single pixel ? */
        pTempsrc2 = MNG_NULL;

      iM = (mng_int32)iML;
    }
    else
    if (iX == (iWidth - 2))            /* last interval ? */
      iM = (mng_int32)iMR;
    else
      iM = (mng_int32)iMX;
                                       /* fill interval ? */
    if ((iX < iWidth - 1) || (iWidth == 1))
    {
      if (pTempsrc2)                   /* do we have the second pixel ? */
      {
        iH = (iM+1) / 2;               /* calculate halfway point */

        for (iS = 1; iS < iH; iS++)    /* first half */
        {
          if (*pTempsrc1 == *pTempsrc2)
            *pTempdst = *pTempsrc1;    /* just repeat the first */
          else                         /* calculate the distance */
            *pTempdst = (mng_uint8)(((2 * iS * ( (mng_int32)(*pTempsrc2)     -
                                                 (mng_int32)(*pTempsrc1)     ) + iM) /
                                     (iM * 2)) + (mng_int32)(*pTempsrc1)             );

          pTempdst++;

          if (*(pTempsrc1+1) == *(pTempsrc2+1))
            *pTempdst = *(pTempsrc1+1);
          else
            *pTempdst = (mng_uint8)(((2 * iS * ( (mng_int32)(*(pTempsrc2+1)) -
                                                 (mng_int32)(*(pTempsrc1+1)) ) + iM) /
                                     (iM * 2)) + (mng_int32)(*(pTempsrc1+1))         );

          pTempdst++;

          if (*(pTempsrc1+2) == *(pTempsrc2+2))
            *pTempdst = *(pTempsrc1+2);
          else
            *pTempdst = (mng_uint8)(((2 * iS * ( (mng_int32)(*(pTempsrc2+2)) -
                                                 (mng_int32)(*(pTempsrc1+2)) ) + iM) /
                                     (iM * 2)) + (mng_int32)(*(pTempsrc1+2))         );

          pTempdst++;
                                       /* replicate alpha from left */
          *pTempdst     = *(pTempsrc1+3);

          pTempdst++;
        }

        for (iS = iH; iS < iM; iS++)   /* second half */
        {
          if (*pTempsrc1 == *pTempsrc2)
            *pTempdst = *pTempsrc1;    /* just repeat the first */
          else                         /* calculate the distance */
            *pTempdst = (mng_uint8)(((2 * iS * ( (mng_int32)(*pTempsrc2)     -
                                                 (mng_int32)(*pTempsrc1)     ) + iM) /
                                     (iM * 2)) + (mng_int32)(*pTempsrc1)             );

          pTempdst++;

          if (*(pTempsrc1+1) == *(pTempsrc2+1))
            *pTempdst = *(pTempsrc1+1);
          else
            *pTempdst = (mng_uint8)(((2 * iS * ( (mng_int32)(*(pTempsrc2+1)) -
                                                 (mng_int32)(*(pTempsrc1+1)) ) + iM) /
                                     (iM * 2)) + (mng_int32)(*(pTempsrc1+1))         );

          pTempdst++;

          if (*(pTempsrc1+2) == *(pTempsrc2+2))
            *pTempdst = *(pTempsrc1+2);
          else
            *pTempdst = (mng_uint8)(((2 * iS * ( (mng_int32)(*(pTempsrc2+2)) -
                                                 (mng_int32)(*(pTempsrc1+2)) ) + iM) /
                                     (iM * 2)) + (mng_int32)(*(pTempsrc1+2))         );

          pTempdst++;
                                       /* replicate alpha from right */
          *pTempdst     = *(pTempsrc2+3);

          pTempdst++;
        }
      }
      else
      {
        for (iS = 1; iS < iM; iS++)
        {
          *pTempdst = *pTempsrc1;      /* repeat first source pixel */
          pTempdst++;
          *pTempdst = *(pTempsrc1+1);
          pTempdst++;
          *pTempdst = *(pTempsrc1+2);
          pTempdst++;
          *pTempdst = *(pTempsrc1+3);
          pTempdst++;
        }
      }
    }

    pTempsrc1 += 4;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_RGBA8_X4, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode magnify_rgba8_x5 (mng_datap  pData,
                              mng_uint16 iMX,
                              mng_uint16 iML,
                              mng_uint16 iMR,
                              mng_uint32 iWidth,
                              mng_uint8p pSrcline,
                              mng_uint8p pDstline)
{
  mng_uint32 iX;
  mng_int32  iS, iM, iH;
  mng_uint8p pTempsrc1;
  mng_uint8p pTempsrc2;
  mng_uint8p pTempdst;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_RGBA8_X5, MNG_LC_START)
#endif

  pTempsrc1 = pSrcline;                /* initialize pixel-loop */
  pTempdst  = pDstline;

  for (iX = 0; iX < iWidth; iX++)
  {
    pTempsrc2 = pTempsrc1 + 4;

    *pTempdst = *pTempsrc1;            /* copy original source pixel */
    pTempdst++;
    *pTempdst = *(pTempsrc1+1);
    pTempdst++;
    *pTempdst = *(pTempsrc1+2);
    pTempdst++;
    *pTempdst = *(pTempsrc1+3);
    pTempdst++;

    if (iX == 0)                       /* first interval ? */
    {
      if (iWidth == 1)                 /* single pixel ? */
        pTempsrc2 = MNG_NULL;

      iM = (mng_int32)iML;
    }
    else
    if (iX == (iWidth - 2))            /* last interval ? */
      iM = (mng_int32)iMR;
    else
      iM = (mng_int32)iMX;
                                       /* fill interval ? */
    if ((iX < iWidth - 1) || (iWidth == 1))
    {
      if (pTempsrc2)                   /* do we have the second pixel ? */
      {
        iH = (iM+1) / 2;               /* calculate halfway point */

        for (iS = 1; iS < iH; iS++)    /* first half */
        {
          *pTempdst     = *pTempsrc1;  /* replicate color from left */
          *(pTempdst+1) = *(pTempsrc1+1);
          *(pTempdst+2) = *(pTempsrc1+2);

          if (*(pTempsrc1+3) == *(pTempsrc2+3))
            *(pTempdst+3) = *(pTempsrc1+3);
          else
            *(pTempdst+3) = (mng_uint8)(((2 * iS * ( (mng_int32)(*(pTempsrc2+3)) -
                                                   (mng_int32)(*(pTempsrc1+3)) ) + iM) /
                                       (iM * 2)) + (mng_int32)(*(pTempsrc1+3))         );

          pTempdst += 4;
        }

        for (iS = iH; iS < iM; iS++)   /* second half */
        {
          *pTempdst     = *pTempsrc2;  /* replicate color from right */
          *(pTempdst+1) = *(pTempsrc2+1);
          *(pTempdst+2) = *(pTempsrc2+2);

          if (*(pTempsrc1+3) == *(pTempsrc2+3))
            *(pTempdst+3) = *(pTempsrc1+3);
          else
            *(pTempdst+3) = (mng_uint8)(((2 * iS * ( (mng_int32)(*(pTempsrc2+3)) -
                                                   (mng_int32)(*(pTempsrc1+3)) ) + iM) /
                                       (iM * 2)) + (mng_int32)(*(pTempsrc1+3))         );

          pTempdst += 4;
        }
      }
      else
      {
        for (iS = 1; iS < iM; iS++)
        {
          *pTempdst = *pTempsrc1;      /* repeat first source pixel */
          pTempdst++;
          *pTempdst = *(pTempsrc1+1);
          pTempdst++;
          *pTempdst = *(pTempsrc1+2);
          pTempdst++;
          *pTempdst = *(pTempsrc1+3);
          pTempdst++;
        }
      }
    }

    pTempsrc1 += 4;
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_RGBA8_X4, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode magnify_g8_y1 (mng_datap  pData,
                           mng_int32  iS,
                           mng_int32  iM,
                           mng_uint32 iWidth,
                           mng_uint8p pSrcline1,
                           mng_uint8p pSrcline2,
                           mng_uint8p pDstline)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_G8_Y1, MNG_LC_START)
#endif

  MNG_COPY (pDstline, pSrcline1, iWidth)

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_G8_Y1, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode magnify_g8_y2 (mng_datap  pData,
                           mng_int32  iS,
                           mng_int32  iM,
                           mng_uint32 iWidth,
                           mng_uint8p pSrcline1,
                           mng_uint8p pSrcline2,
                           mng_uint8p pDstline)
{
  mng_uint32 iX;
  mng_uint8p pTempsrc1;
  mng_uint8p pTempsrc2;
  mng_uint8p pTempdst;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_G8_Y2, MNG_LC_START)
#endif

  pTempsrc1 = pSrcline1;               /* initialize pixel-loop */
  pTempsrc2 = pSrcline2;
  pTempdst  = pDstline;

  if (pTempsrc2)                       /* do we have a second line ? */
  {
    for (iX = 0; iX < iWidth; iX++)
    {                                  /* calculate the distances */
      if (*pTempsrc1 == *pTempsrc2)
        *pTempdst = *pTempsrc1;
      else
        *pTempdst = (mng_uint8)( ( (2 * iS * ( (mng_int32)(*pTempsrc2) -
                                               (mng_int32)(*pTempsrc1) ) + iM) /
                                   (iM * 2) ) + (mng_int32)(*pTempsrc1) );

      pTempdst++;
      pTempsrc1++;
      pTempsrc2++;
    }
  }
  else
  {                                    /* just repeat the entire line */
    MNG_COPY (pTempdst, pTempsrc1, iWidth)
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_G8_Y2, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode magnify_g8_y3 (mng_datap  pData,
                           mng_int32  iS,
                           mng_int32  iM,
                           mng_uint32 iWidth,
                           mng_uint8p pSrcline1,
                           mng_uint8p pSrcline2,
                           mng_uint8p pDstline)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_G8_Y3, MNG_LC_START)
#endif

  if (pSrcline2)                       /* do we have a second line ? */
  {
    if (iS < (iM+1) / 2)               /* top half ? */
      MNG_COPY (pDstline, pSrcline1, iWidth)
    else
      MNG_COPY (pDstline, pSrcline2, iWidth)
  }
  else
  {                                    /* just repeat the entire line */
    MNG_COPY (pDstline, pSrcline1, iWidth)
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_G8_Y3, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode magnify_rgb8_y1 (mng_datap  pData,
                             mng_int32  iS,
                             mng_int32  iM,
                             mng_uint32 iWidth,
                             mng_uint8p pSrcline1,
                             mng_uint8p pSrcline2,
                             mng_uint8p pDstline)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_RGB8_Y1, MNG_LC_START)
#endif

  MNG_COPY (pDstline, pSrcline1, iWidth * 3)

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_RGB8_Y1, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode magnify_rgb8_y2 (mng_datap  pData,
                             mng_int32  iS,
                             mng_int32  iM,
                             mng_uint32 iWidth,
                             mng_uint8p pSrcline1,
                             mng_uint8p pSrcline2,
                             mng_uint8p pDstline)
{
  mng_uint32 iX;
  mng_uint8p pTempsrc1;
  mng_uint8p pTempsrc2;
  mng_uint8p pTempdst;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_RGB8_Y2, MNG_LC_START)
#endif

  pTempsrc1 = pSrcline1;               /* initialize pixel-loop */
  pTempsrc2 = pSrcline2;
  pTempdst  = pDstline;

  if (pTempsrc2)                       /* do we have a second line ? */
  {
    for (iX = 0; iX < iWidth; iX++)
    {                                  /* calculate the distances */
      if (*pTempsrc1 == *pTempsrc2)
        *pTempdst = *pTempsrc1;
      else
        *pTempdst = (mng_uint8)( ( (2 * iS * ( (mng_int32)(*pTempsrc2) -
                                               (mng_int32)(*pTempsrc1) ) + iM) /
                                   (iM * 2) ) + (mng_int32)(*pTempsrc1) );

      pTempdst++;
      pTempsrc1++;
      pTempsrc2++;

      if (*pTempsrc1 == *pTempsrc2)
        *pTempdst = *pTempsrc1;
      else
        *pTempdst = (mng_uint8)( ( (2 * iS * ( (mng_int32)(*pTempsrc2) -
                                               (mng_int32)(*pTempsrc1) ) + iM) /
                                   (iM * 2) ) + (mng_int32)(*pTempsrc1) );

      pTempdst++;
      pTempsrc1++;
      pTempsrc2++;

      if (*pTempsrc1 == *pTempsrc2)
        *pTempdst = *pTempsrc1;
      else
        *pTempdst = (mng_uint8)( ( (2 * iS * ( (mng_int32)(*pTempsrc2) -
                                               (mng_int32)(*pTempsrc1) ) + iM) /
                                   (iM * 2) ) + (mng_int32)(*pTempsrc1) );

      pTempdst++;
      pTempsrc1++;
      pTempsrc2++;
    }
  }
  else
  {                                    /* just repeat the entire line */
    MNG_COPY (pTempdst, pTempsrc1, iWidth * 3)
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_RGB8_Y2, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode magnify_rgb8_y3 (mng_datap  pData,
                             mng_int32  iS,
                             mng_int32  iM,
                             mng_uint32 iWidth,
                             mng_uint8p pSrcline1,
                             mng_uint8p pSrcline2,
                             mng_uint8p pDstline)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_RGB8_Y3, MNG_LC_START)
#endif

  if (pSrcline2)                       /* do we have a second line ? */
  {
    if (iS < (iM+1) / 2)               /* top half ? */
      MNG_COPY (pDstline, pSrcline1, iWidth * 3)
    else
      MNG_COPY (pDstline, pSrcline2, iWidth * 3)
  }
  else
  {                                    /* just repeat the entire line */
    MNG_COPY (pDstline, pSrcline1, iWidth * 3)
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_RGB8_Y3, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode magnify_ga8_y1 (mng_datap  pData,
                            mng_int32  iS,
                            mng_int32  iM,
                            mng_uint32 iWidth,
                            mng_uint8p pSrcline1,
                            mng_uint8p pSrcline2,
                            mng_uint8p pDstline)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_GA8_Y1, MNG_LC_START)
#endif

  MNG_COPY (pDstline, pSrcline1, iWidth << 1)

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_GA8_Y1, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode magnify_ga8_y2 (mng_datap  pData,
                            mng_int32  iS,
                            mng_int32  iM,
                            mng_uint32 iWidth,
                            mng_uint8p pSrcline1,
                            mng_uint8p pSrcline2,
                            mng_uint8p pDstline)
{
  mng_uint32 iX;
  mng_uint8p pTempsrc1;
  mng_uint8p pTempsrc2;
  mng_uint8p pTempdst;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_GA8_Y2, MNG_LC_START)
#endif

  pTempsrc1 = pSrcline1;               /* initialize pixel-loop */
  pTempsrc2 = pSrcline2;
  pTempdst  = pDstline;

  if (pTempsrc2)                       /* do we have a second line ? */
  {
    for (iX = 0; iX < iWidth; iX++)
    {                                  /* calculate the distances */
      if (*pTempsrc1 == *pTempsrc2)
        *pTempdst = *pTempsrc1;
      else
        *pTempdst = (mng_uint8)( ( (2 * iS * ( (mng_int32)(*pTempsrc2) -
                                               (mng_int32)(*pTempsrc1) ) + iM) /
                                   (iM * 2) ) + (mng_int32)(*pTempsrc1) );

      pTempdst++;
      pTempsrc1++;
      pTempsrc2++;

      if (*pTempsrc1 == *pTempsrc2)
        *pTempdst = *pTempsrc1;
      else
        *pTempdst = (mng_uint8)( ( (2 * iS * ( (mng_int32)(*pTempsrc2) -
                                               (mng_int32)(*pTempsrc1) ) + iM) /
                                   (iM * 2) ) + (mng_int32)(*pTempsrc1) );

      pTempdst++;
      pTempsrc1++;
      pTempsrc2++;
    }
  }
  else
  {                                    /* just repeat the entire line */
    MNG_COPY (pTempdst, pTempsrc1, iWidth << 1)
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_GA8_Y2, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode magnify_ga8_y3 (mng_datap  pData,
                            mng_int32  iS,
                            mng_int32  iM,
                            mng_uint32 iWidth,
                            mng_uint8p pSrcline1,
                            mng_uint8p pSrcline2,
                            mng_uint8p pDstline)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_GA8_Y3, MNG_LC_START)
#endif

  if (pSrcline2)                       /* do we have a second line ? */
  {
    if (iS < (iM+1) / 2)               /* top half ? */
      MNG_COPY (pDstline, pSrcline1, iWidth << 1)
    else
      MNG_COPY (pDstline, pSrcline2, iWidth << 1)
  }
  else
  {                                    /* just repeat the entire line */
    MNG_COPY (pDstline, pSrcline1, iWidth << 1)
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_GA8_Y2, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode magnify_ga8_y4 (mng_datap  pData,
                            mng_int32  iS,
                            mng_int32  iM,
                            mng_uint32 iWidth,
                            mng_uint8p pSrcline1,
                            mng_uint8p pSrcline2,
                            mng_uint8p pDstline)
{
  mng_uint32 iX;
  mng_uint8p pTempsrc1;
  mng_uint8p pTempsrc2;
  mng_uint8p pTempdst;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_GA8_Y4, MNG_LC_START)
#endif

  pTempsrc1 = pSrcline1;               /* initialize pixel-loop */
  pTempsrc2 = pSrcline2;
  pTempdst  = pDstline;

  if (pTempsrc2)                       /* do we have a second line ? */
  {
    if (iS < (iM+1) / 2)               /* top half ? */
    {
      for (iX = 0; iX < iWidth; iX++)
      {                                /* calculate the distances */
        if (*pTempsrc1 == *pTempsrc2)
          *pTempdst = *pTempsrc1;
        else
          *pTempdst = (mng_uint8)( ( (2 * iS * ( (mng_int32)(*pTempsrc2) -
                                                 (mng_int32)(*pTempsrc1) ) + iM) /
                                     (iM * 2) ) + (mng_int32)(*pTempsrc1) );

        pTempdst++;
        pTempsrc1++;
        pTempsrc2 += 2;

        *pTempdst++ = *pTempsrc1++;    /* replicate alpha from top */
      }
    }
    else
    {
      for (iX = 0; iX < iWidth; iX++)
      {                                /* calculate the distances */
        if (*pTempsrc1 == *pTempsrc2)
          *pTempdst = *pTempsrc1;
        else
          *pTempdst = (mng_uint8)( ( (2 * iS * ( (mng_int32)(*pTempsrc2) -
                                                 (mng_int32)(*pTempsrc1) ) + iM) /
                                     (iM * 2) ) + (mng_int32)(*pTempsrc1) );

        pTempdst++;
        pTempsrc1 += 2;
        pTempsrc2++;

        *pTempdst++ = *pTempsrc2++;    /* replicate alpha from bottom */
      }
    }
  }
  else
  {                                    /* just repeat the entire line */
    MNG_COPY (pTempdst, pTempsrc1, iWidth << 1)
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_GA8_Y4, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode magnify_ga8_y5 (mng_datap  pData,
                            mng_int32  iS,
                            mng_int32  iM,
                            mng_uint32 iWidth,
                            mng_uint8p pSrcline1,
                            mng_uint8p pSrcline2,
                            mng_uint8p pDstline)
{
  mng_uint32 iX;
  mng_uint8p pTempsrc1;
  mng_uint8p pTempsrc2;
  mng_uint8p pTempdst;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_GA8_Y5, MNG_LC_START)
#endif

  pTempsrc1 = pSrcline1;               /* initialize pixel-loop */
  pTempsrc2 = pSrcline2;
  pTempdst  = pDstline;

  if (pTempsrc2)                       /* do we have a second line ? */
  {
    if (iS < (iM+1) / 2)               /* top half ? */
    {
      for (iX = 0; iX < iWidth; iX++)
      {
        *pTempdst = *pTempsrc1;        /* replicate gray from top */

        pTempdst++;
        pTempsrc1++;
        pTempsrc2++;

        if (*pTempsrc1 == *pTempsrc2)  /* calculate the distances */
          *pTempdst = *pTempsrc1;
        else
          *pTempdst = (mng_uint8)( ( (2 * iS * ( (mng_int32)(*pTempsrc2) -
                                                 (mng_int32)(*pTempsrc1) ) + iM) /
                                     (iM * 2) ) + (mng_int32)(*pTempsrc1) );

        pTempdst++;
        pTempsrc1++;
        pTempsrc2++;
      }
    }
    else
    {
      for (iX = 0; iX < iWidth; iX++)
      {
        *pTempdst = *pTempsrc2;        /* replicate gray from bottom */

        pTempdst++;
        pTempsrc1++;
        pTempsrc2++;

        if (*pTempsrc1 == *pTempsrc2)  /* calculate the distances */
          *pTempdst = *pTempsrc1;
        else
          *pTempdst = (mng_uint8)( ( (2 * iS * ( (mng_int32)(*pTempsrc2) -
                                                 (mng_int32)(*pTempsrc1) ) + iM) /
                                     (iM * 2) ) + (mng_int32)(*pTempsrc1) );

        pTempdst++;
        pTempsrc1++;
        pTempsrc2++;
      }
    }
  }
  else
  {                                    /* just repeat the entire line */
    MNG_COPY (pTempdst, pTempsrc1, iWidth << 1)
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_GA8_Y5, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode magnify_rgba8_y1 (mng_datap  pData,
                              mng_int32  iS,
                              mng_int32  iM,
                              mng_uint32 iWidth,
                              mng_uint8p pSrcline1,
                              mng_uint8p pSrcline2,
                              mng_uint8p pDstline)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_RGBA8_Y1, MNG_LC_START)
#endif

  MNG_COPY (pDstline, pSrcline1, iWidth << 2)

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_RGBA8_Y1, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode magnify_rgba8_y2 (mng_datap  pData,
                              mng_int32  iS,
                              mng_int32  iM,
                              mng_uint32 iWidth,
                              mng_uint8p pSrcline1,
                              mng_uint8p pSrcline2,
                              mng_uint8p pDstline)
{
  mng_uint32 iX;
  mng_uint8p pTempsrc1;
  mng_uint8p pTempsrc2;
  mng_uint8p pTempdst;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_RGBA8_Y2, MNG_LC_START)
#endif

  pTempsrc1 = pSrcline1;               /* initialize pixel-loop */
  pTempsrc2 = pSrcline2;
  pTempdst  = pDstline;

  if (pTempsrc2)                       /* do we have a second line ? */
  {
    for (iX = 0; iX < iWidth; iX++)
    {                                  /* calculate the distances */
      if (*pTempsrc1 == *pTempsrc2)
        *pTempdst = *pTempsrc1;
      else
        *pTempdst = (mng_uint8)( ( (2 * iS * ( (mng_int32)(*pTempsrc2) -
                                               (mng_int32)(*pTempsrc1) ) + iM) /
                                   (iM * 2) ) + (mng_int32)(*pTempsrc1) );

      pTempdst++;
      pTempsrc1++;
      pTempsrc2++;

      if (*pTempsrc1 == *pTempsrc2)
        *pTempdst = *pTempsrc1;
      else
        *pTempdst = (mng_uint8)( ( (2 * iS * ( (mng_int32)(*pTempsrc2) -
                                               (mng_int32)(*pTempsrc1) ) + iM) /
                                   (iM * 2) ) + (mng_int32)(*pTempsrc1) );

      pTempdst++;
      pTempsrc1++;
      pTempsrc2++;

      if (*pTempsrc1 == *pTempsrc2)
        *pTempdst = *pTempsrc1;
      else
        *pTempdst = (mng_uint8)( ( (2 * iS * ( (mng_int32)(*pTempsrc2) -
                                               (mng_int32)(*pTempsrc1) ) + iM) /
                                   (iM * 2) ) + (mng_int32)(*pTempsrc1) );

      pTempdst++;
      pTempsrc1++;
      pTempsrc2++;

      if (*pTempsrc1 == *pTempsrc2)
        *pTempdst = *pTempsrc1;
      else
        *pTempdst = (mng_uint8)( ( (2 * iS * ( (mng_int32)(*pTempsrc2) -
                                               (mng_int32)(*pTempsrc1) ) + iM) /
                                   (iM * 2) ) + (mng_int32)(*pTempsrc1) );

      pTempdst++;
      pTempsrc1++;
      pTempsrc2++;
    }
  }
  else
  {                                    /* just repeat the entire line */
    MNG_COPY (pTempdst, pTempsrc1, iWidth << 2)
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_RGBA8_Y2, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode magnify_rgba8_y3 (mng_datap  pData,
                              mng_int32  iS,
                              mng_int32  iM,
                              mng_uint32 iWidth,
                              mng_uint8p pSrcline1,
                              mng_uint8p pSrcline2,
                              mng_uint8p pDstline)
{
#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_RGBA8_Y3, MNG_LC_START)
#endif

  if (pSrcline2)                       /* do we have a second line ? */
  {
    if (iS < (iM+1) / 2)               /* top half ? */
      MNG_COPY (pDstline, pSrcline1, iWidth << 2)
    else
      MNG_COPY (pDstline, pSrcline2, iWidth << 2)
  }
  else
  {                                    /* just repeat the entire line */
    MNG_COPY (pDstline, pSrcline1, iWidth << 2)
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_RGBA8_Y2, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode magnify_rgba8_y4 (mng_datap  pData,
                              mng_int32  iS,
                              mng_int32  iM,
                              mng_uint32 iWidth,
                              mng_uint8p pSrcline1,
                              mng_uint8p pSrcline2,
                              mng_uint8p pDstline)
{
  mng_uint32 iX;
  mng_uint8p pTempsrc1;
  mng_uint8p pTempsrc2;
  mng_uint8p pTempdst;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_RGBA8_Y4, MNG_LC_START)
#endif

  pTempsrc1 = pSrcline1;               /* initialize pixel-loop */
  pTempsrc2 = pSrcline2;
  pTempdst  = pDstline;

  if (pTempsrc2)                       /* do we have a second line ? */
  {
    if (iS < (iM+1) / 2)               /* top half ? */
    {
      for (iX = 0; iX < iWidth; iX++)
      {                                /* calculate the distances */
        if (*pTempsrc1 == *pTempsrc2)
          *pTempdst = *pTempsrc1;
        else
          *pTempdst = (mng_uint8)( ( (2 * iS * ( (mng_int32)(*pTempsrc2) -
                                                 (mng_int32)(*pTempsrc1) ) + iM) /
                                     (iM * 2) ) + (mng_int32)(*pTempsrc1) );

        pTempdst++;
        pTempsrc1++;
        pTempsrc2++;

        if (*pTempsrc1 == *pTempsrc2)
          *pTempdst = *pTempsrc1;
        else
          *pTempdst = (mng_uint8)( ( (2 * iS * ( (mng_int32)(*pTempsrc2) -
                                                 (mng_int32)(*pTempsrc1) ) + iM) /
                                     (iM * 2) ) + (mng_int32)(*pTempsrc1) );

        pTempdst++;
        pTempsrc1++;
        pTempsrc2++;

        if (*pTempsrc1 == *pTempsrc2)
          *pTempdst = *pTempsrc1;
        else
          *pTempdst = (mng_uint8)( ( (2 * iS * ( (mng_int32)(*pTempsrc2) -
                                                 (mng_int32)(*pTempsrc1) ) + iM) /
                                     (iM * 2) ) + (mng_int32)(*pTempsrc1) );

        pTempdst++;
        pTempsrc1++;
        pTempsrc2 += 2;

        *pTempdst++ = *pTempsrc1++;    /* replicate alpha from top */
      }
    }
    else
    {
      for (iX = 0; iX < iWidth; iX++)
      {                                /* calculate the distances */
        if (*pTempsrc1 == *pTempsrc2)
          *pTempdst = *pTempsrc1;
        else
          *pTempdst = (mng_uint8)( ( (2 * iS * ( (mng_int32)(*pTempsrc2) -
                                                 (mng_int32)(*pTempsrc1) ) + iM) /
                                     (iM * 2) ) + (mng_int32)(*pTempsrc1) );

        pTempdst++;
        pTempsrc1++;
        pTempsrc2++;

        if (*pTempsrc1 == *pTempsrc2)
          *pTempdst = *pTempsrc1;
        else
          *pTempdst = (mng_uint8)( ( (2 * iS * ( (mng_int32)(*pTempsrc2) -
                                                 (mng_int32)(*pTempsrc1) ) + iM) /
                                     (iM * 2) ) + (mng_int32)(*pTempsrc1) );

        pTempdst++;
        pTempsrc1++;
        pTempsrc2++;

        if (*pTempsrc1 == *pTempsrc2)
          *pTempdst = *pTempsrc1;
        else
          *pTempdst = (mng_uint8)( ( (2 * iS * ( (mng_int32)(*pTempsrc2) -
                                                 (mng_int32)(*pTempsrc1) ) + iM) /
                                     (iM * 2) ) + (mng_int32)(*pTempsrc1) );

        pTempdst++;
        pTempsrc1 += 2;
        pTempsrc2++;

        *pTempdst++ = *pTempsrc2++;    /* replicate alpha from bottom */
      }
    }
  }
  else
  {                                    /* just repeat the entire line */
    MNG_COPY (pTempdst, pTempsrc1, iWidth << 2)
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_RGBA8_Y4, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

mng_retcode magnify_rgba8_y5 (mng_datap  pData,
                              mng_int32  iS,
                              mng_int32  iM,
                              mng_uint32 iWidth,
                              mng_uint8p pSrcline1,
                              mng_uint8p pSrcline2,
                              mng_uint8p pDstline)
{
  mng_uint32 iX;
  mng_uint8p pTempsrc1;
  mng_uint8p pTempsrc2;
  mng_uint8p pTempdst;

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_RGBA8_Y5, MNG_LC_START)
#endif

  pTempsrc1 = pSrcline1;               /* initialize pixel-loop */
  pTempsrc2 = pSrcline2;
  pTempdst  = pDstline;

  if (pTempsrc2)                       /* do we have a second line ? */
  {
    if (iS < (iM+1) / 2)               /* top half ? */
    {
      for (iX = 0; iX < iWidth; iX++)
      {
        *pTempdst++ = *pTempsrc1++;    /* replicate color from top */
        *pTempdst++ = *pTempsrc1++;
        *pTempdst++ = *pTempsrc1++;

        pTempsrc2 += 3;

        if (*pTempsrc1 == *pTempsrc2)  /* calculate the distances */
          *pTempdst = *pTempsrc1;
        else
          *pTempdst = (mng_uint8)( ( (2 * iS * ( (mng_int32)(*pTempsrc2) -
                                                 (mng_int32)(*pTempsrc1) ) + iM) /
                                     (iM * 2) ) + (mng_int32)(*pTempsrc1) );

        pTempdst++;
        pTempsrc1++;
        pTempsrc2++;
      }
    }
    else
    {
      for (iX = 0; iX < iWidth; iX++)
      {
        *pTempdst++ = *pTempsrc2++;    /* replicate color from bottom */
        *pTempdst++ = *pTempsrc2++;
        *pTempdst++ = *pTempsrc2++;

        pTempsrc1 += 3;

        if (*pTempsrc1 == *pTempsrc2)  /* calculate the distances */
          *pTempdst = *pTempsrc1;
        else
          *pTempdst = (mng_uint8)( ( (2 * iS * ( (mng_int32)(*pTempsrc2) -
                                                 (mng_int32)(*pTempsrc1) ) + iM) /
                                     (iM * 2) ) + (mng_int32)(*pTempsrc1) );

        pTempdst++;
        pTempsrc1++;
        pTempsrc2++;
      }
    }
  }
  else
  {                                    /* just repeat the entire line */
    MNG_COPY (pTempdst, pTempsrc1, iWidth << 2)
  }

#ifdef MNG_SUPPORT_TRACE
  MNG_TRACE (pData, MNG_FN_MAGNIFY_RGBA8_Y5, MNG_LC_END)
#endif

  return MNG_NOERROR;
}

/* ************************************************************************** */

#endif /* MNG_INCLUDE_DISPLAY_PROCS */

/* ************************************************************************** */
/* * end of file                                                            * */
/* ************************************************************************** */

