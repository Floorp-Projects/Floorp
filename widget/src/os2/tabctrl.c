/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst. All Rights Reserved.
 *
 * Contributor(s): Dan Holmes <dholmes@trellis.net>
 *
 *
 * TabCtrl.c - Warpzilla tab control
 *
 */

#define INCL_WIN
#define INCL_GPI
#include <os2.h>
#include <stdlib.h>
#include <string.h>

#include "tabapi.h"

/* prototype of window procedure */
MRESULT EXPENTRY fnwpTabCtrl( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);
void DrawLine (HPS hps, int x1,int x2, int y1, int y2);

/* Convenience method to get classname & register class */
static BOOL bRegistered;

#define UWC_TABCONTROL "WarpzillaTabControl"

PSZ TabGetTabControlName(void)
{
  static PSZ pszClassname;

  if(!pszClassname)
  {
    BOOL rc = WinRegisterClass( 0, /* don't actually need a hab */
                                  UWC_TABCONTROL,
                                  fnwpTabCtrl,
                                  0,  /* maybe want CS_SIZEREDRAW here? */
                                  8); /* 4 bytes spare for user, 4 for us */
    if(rc)
      pszClassname = UWC_TABCONTROL;
  }

  return pszClassname;
}

/***************************************************************************/
/*                                                                         */
/* TABDATA                                                          */
/*                                                                         */
/***************************************************************************/
typedef struct _PERTABDATA
{
  ULONG ulLength;
  PSZ   pszTabCaption;
  struct _PERTABDATA *nextPerTabdata;
} PERTABDATA,*PPERTABDATA;

typedef struct _TABDATA
{ 
  RECTL rclTabctl;
  USHORT usTabCount;
  USHORT usCurrentTab;
  USHORT usMouseX;
  USHORT usMouseY;
  PPERTABDATA perTabData;
} TABDATA, *PTABDATA;

/***************************************************************************/
/*                                                                         */
/* Window procedure - add whatever messages you need                       */
/*                                                                         */
/* When you want to define new messages to send to yourself, start at      */
/* TABM_LASTPUBLIC + 1                                                     */
/*                                                                         */
/***************************************************************************/
MRESULT EXPENTRY fnwpTabCtrl( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
  PTABDATA pData = (PTABDATA) WinQueryWindowPtr( hwnd, 4);
  switch( msg)
  {
    case WM_CREATE:
      /* allocate and set up default TABDATA structure */
      pData = (PTABDATA) calloc( sizeof(TABDATA), 1);
      pData->usTabCount=0;
      pData->usCurrentTab=0;
      pData->usMouseX=0;
      pData->usMouseY=0;
      pData->rclTabctl.yTop=0;
      pData->rclTabctl.yBottom=0;
      pData->rclTabctl.xLeft=0;
      pData->rclTabctl.xRight=0;
      WinSetWindowPtr( hwnd, 4, pData);
      break;

    case WM_DESTROY:
    {
      /* free up contents of TABDATA structure */
      /* then free the structure itself */
      PPERTABDATA pPTD, pPTD1;
      pPTD=pData->perTabData;
      while (pPTD) {
        if (pPTD->pszTabCaption)
          free(pPTD->pszTabCaption);
        pPTD1=pPTD->nextPerTabdata;
        free(pPTD);
        pPTD=pPTD1;
      } /* endwhile */
      free( pData);
      break;
    }
    case WM_PAINT:
    {
      PPERTABDATA pPTD, pPTD1;
      int x1,i,j,k,numChars;
      POINTL ptl;
      RECTL rcl,rcl1;
      HPS hps = WinBeginPaint( hwnd, NULLHANDLE, &rcl);
      WinQueryWindowRect(hwnd,&rcl);
      pData->rclTabctl=rcl;
      WinFillRect( hps, &rcl, CLR_PALEGRAY);
      WinDrawBorder(hps,&rcl,1,1,0L,0L,DB_AREAATTRS);
      GpiSetColor (hps,CLR_BLACK);
      if (pData->usTabCount>0) {
        if (pData->usCurrentTab==0)
          pData->usCurrentTab=1;
        if (pData->usCurrentTab>0) {
          x1=(rcl.xRight-rcl.xLeft) / pData->usTabCount;
          rcl1.xLeft=rcl.xLeft+(x1*(pData->usCurrentTab-1));
          rcl1.xRight=rcl1.xLeft+x1;
          rcl1.yTop=rcl.yTop;
          rcl1.yBottom=rcl.yBottom;
//          WinFillRect( hps, &rcl1, CLR_DARKGRAY);
        } /* endif */
        j=(rcl.xRight-rcl.xLeft) / pData->usTabCount;
        i=k=0;
          /* draw left white line */
          if (pData->usCurrentTab==1) {
            GpiSetColor (hps,CLR_DARKGRAY);
          } else {
            GpiSetColor (hps,CLR_WHITE);
          } /* endif */
          DrawLine(hps,i+1,i+1,rcl.yBottom+1,rcl.yTop-2);
          DrawLine(hps,i+2,i+2,rcl.yBottom+1,rcl.yTop-2);
          /* draw top white line */
          DrawLine(hps,i+1,rcl.xRight-2,rcl.yTop-2,rcl.yTop-2);
          DrawLine(hps,i+1,rcl.xRight-2,rcl.yTop-3,rcl.yTop-3);
          GpiSetColor (hps,CLR_BLACK);
          /* draw left dark gray line */
          if (pData->usCurrentTab!=1) {
            GpiSetColor (hps,CLR_DARKGRAY);
          } else {
            GpiSetColor (hps,CLR_WHITE);
          } /* endif */
          DrawLine(hps,rcl1.xRight-2,rcl1.xRight-2,rcl1.yBottom+1,rcl1.yTop-2);
          DrawLine(hps,rcl1.xRight-3,rcl1.xRight-3,rcl1.yBottom+1,rcl1.yTop-2);
          /* draw bottom dark gray line */
          DrawLine(hps,rcl1.xLeft+1,rcl1.xRight-2,rcl1.yBottom+1,rcl1.yBottom+1);
          DrawLine(hps,rcl1.xLeft+1,rcl1.xRight-2,rcl1.yBottom+2,rcl1.yBottom+2);
          GpiSetColor (hps,CLR_BLACK);
        rcl1.xLeft=rcl.xLeft+i;
        rcl1.xRight=rcl1.xLeft+j-2;
        rcl1.yTop=rcl.yTop-3;
        rcl1.yBottom=rcl.yBottom;
        pPTD=pData->perTabData;
        numChars=WinDrawText(hps,(LONG)strlen(pPTD->pszTabCaption),pPTD->pszTabCaption,&rcl1,0,0,DT_CENTER|DT_TEXTATTRS|DT_WORDBREAK|DT_VCENTER);
        for (i=j;i<rcl.xRight ;i+=(rcl.xRight-rcl.xLeft) / pData->usTabCount) {
          GpiSetColor (hps,CLR_BLACK);
          k++;
          rcl1.xLeft=rcl.xLeft+i;
          rcl1.xRight=rcl1.xLeft+j-2;
          rcl1.yTop=rcl.yTop-3;
          rcl1.yBottom=rcl.yBottom;
          if (!pPTD->nextPerTabdata) break;
          pPTD=pPTD->nextPerTabdata;
          numChars=WinDrawText(hps,(LONG)strlen(pPTD->pszTabCaption),pPTD->pszTabCaption,&rcl1,0,0,DT_CENTER|DT_TEXTATTRS|DT_WORDBREAK|DT_VCENTER);
          DrawLine(hps,i,i,rcl.yBottom,rcl.yTop);
          /* draw highlites */
          if (pData->usCurrentTab==(k+1)) {
            GpiSetColor (hps,CLR_DARKGRAY);
          } else {
            GpiSetColor (hps,CLR_WHITE);
          } /* endif */
          /* draw left white line */
          DrawLine(hps,i+1,i+1,rcl.yBottom+1,rcl.yTop-2);
          DrawLine(hps,i+2,i+2,rcl.yBottom+1,rcl.yTop-2);
          /* draw top white line */
          DrawLine(hps,i+1,rcl.xRight-2,rcl.yTop-2,rcl.yTop-2);
          DrawLine(hps,i+1,rcl.xRight-2,rcl.yTop-3,rcl.yTop-3);
          if (pData->usCurrentTab!=(k+1)) {
            GpiSetColor (hps,CLR_DARKGRAY);
          } else {
            GpiSetColor (hps,CLR_WHITE);
          } /* endif */
          /* draw right dark gray line */
          DrawLine(hps,rcl1.xRight,rcl1.xRight,rcl1.yBottom+1,rcl1.yTop-2);
          DrawLine(hps,rcl1.xRight-1,rcl1.xRight-1,rcl1.yBottom+1,rcl1.yTop-2);
          /* draw bottom dark gray line */
          DrawLine(hps,rcl1.xLeft+1,rcl1.xRight-2,rcl1.yBottom+1,rcl1.yBottom+1);
          DrawLine(hps,rcl1.xLeft+1,rcl1.xRight-2,rcl1.yBottom+2,rcl1.yBottom+2);
          GpiSetColor (hps,CLR_BLACK);
        } /* endfor */
      } /* endif */
      WinEndPaint( hps);
      return 0;
    }
    case WM_BUTTON1DOWN:
    {
      int x1;
      RECTL rcl;
      if (pData->usTabCount>0) {
        WinQueryWindowRect(hwnd,&rcl);
        pData->usMouseX=SHORT1FROMMP(mp1);
        pData->usMouseY=SHORT2FROMMP(mp1);
        x1=pData->usMouseX / (rcl.xRight / pData->usTabCount)+1;
        if (x1!=pData->usCurrentTab) {
          pData->usCurrentTab=x1;
          WinSendMsg(WinQueryWindow(hwnd,QW_OWNER),
                    WM_CONTROL,
                    MPFROM2SHORT(WinQueryWindowUShort(hwnd,QWS_ID),TTN_TABCHANGE),
                    MPFROMSHORT(x1));
          WinInvalidateRect( hwnd, 0, TRUE);
        } /* endif */
      } /* endif */
      break;
    }
    case TABM_INSERTMULTTABS:
    {
      PPERTABDATA pFirstTabData=0, pPrevious=0;
      int k;
      /* create new tabs */
      for (k=1;k<=SHORT1FROMMP(mp1);k++) {
        PPERTABDATA pTabData = calloc( sizeof(PERTABDATA), 1);
        if( pPrevious != 0)
        {
          pPrevious->nextPerTabdata = pTabData;
          pPrevious = pTabData;
        }
        if( pFirstTabData == 0)
        {
          pFirstTabData = pTabData;
          pPrevious = pTabData;
        }
        pTabData->pszTabCaption=strdup(((char **)mp2)[k-1]);
        pTabData->ulLength=strlen(pTabData->pszTabCaption);
        pData->usTabCount++;
      } /* end for */
      if (pData->usTabCount==SHORT1FROMMP(mp1))
        pData->perTabData=pFirstTabData;
      else
      {
        PPERTABDATA pPTD=0;
        pPTD=pData->perTabData;
        while (pPTD->nextPerTabdata)
          pPTD=pPTD->nextPerTabdata;
        pPTD->nextPerTabdata=pFirstTabData;
      }
      WinInvalidateRect( hwnd, 0, TRUE);
      return (MRESULT)TRUE;
    }
    case TABM_INSERTTAB:
    {
      int i,j;
      PPERTABDATA pTempTabData, pTabData = calloc(sizeof(PERTABDATA),1);
      pTabData->ulLength=strlen((char *)mp2);
      pTabData->pszTabCaption=calloc(pTabData->ulLength+1,1);
      strcpy(pTabData->pszTabCaption,(char *)mp2);
      if (SHORT1FROMMP(mp1)==1) {
        pData->perTabData=pTabData;
        pData->usTabCount=1;
        WinInvalidateRect( hwnd, 0, TRUE);
        return  (MRESULT)1;
      } else {
        j=SHORT1FROMMP(mp1);
        if (j<=pData->usTabCount+1) {
          pTempTabData=pData->perTabData;
          for (i=2;i<j;i++)
            pTempTabData=pTempTabData->nextPerTabdata;
          pTempTabData->nextPerTabdata=pTabData;
          if (pData->usTabCount<SHORT1FROMMP(mp1)) {
            pData->usTabCount++;
          } /* endif */
          WinInvalidateRect( hwnd, 0, TRUE);
          return mp1;
        } else {
          free(pTabData);
          return  (MRESULT)TABC_FAILURE;
        } /* endif */
      } /* endif */
    } /* end case */
    case TABM_QUERYHILITTAB:
      return MRFROMSHORT((pData->usTabCount?pData->usCurrentTab:TABC_FAILURE));

    case TABM_QUERYTABCOUNT:
      return MRFROMSHORT(pData->usTabCount);

    case TABM_QUERYTABTEXT:
    {
      int i;
      PPERTABDATA pTabData=pData->perTabData;
      if (pData->usTabCount>0) {
        for (i=1;i<SHORT1FROMMP(mp1);i++) {
          pTabData=pTabData->nextPerTabdata;
        } /* endfor */
        strncpy((char *)mp2,pTabData->pszTabCaption,SHORT1FROMMP(mp2));
        return MRFROMSHORT(((SHORT2FROMMP(mp1)<strlen(pTabData->pszTabCaption))?strlen(pTabData->pszTabCaption)-SHORT2FROMMP(mp1):0));
      } else {
        return MRFROMSHORT(TABC_FAILURE);
      } /* endif */
    }
    case TABM_QUERYTABTEXTLENGTH:
    {
      int i;
      PPERTABDATA pTabData=pData->perTabData;
      if (pData->usTabCount>0) {
        for (i=1;i<SHORT1FROMMP(mp1);i++) {
          pTabData=pTabData->nextPerTabdata;
        } /* endfor */
        return (MRESULT)(strlen(pTabData->pszTabCaption));
      } else {
        return (MRESULT)TABC_FAILURE;
      } /* endif */
    }
    case TABM_REMOVETAB:
    {
      int i;
      PPERTABDATA pPTD, pPTD1;
      if (SHORT1FROMMP(mp1)>pData->usTabCount)
        return (MRESULT)TABC_FAILURE;
      if (SHORT1FROMMP(mp1)==1) {
        pPTD=pData->perTabData->nextPerTabdata;
        free(pData->perTabData->pszTabCaption);
        free(pData->perTabData);
        pData->perTabData=pPTD;
      } else if (SHORT1FROMMP(mp1)==pData->usTabCount) {
        pPTD=pData->perTabData;
        for (i=1;i<SHORT1FROMMP(mp1)-1;i++)
          pPTD=pPTD->nextPerTabdata;
        free(pPTD->nextPerTabdata->pszTabCaption);
        free(pPTD->nextPerTabdata);
        pPTD->nextPerTabdata='\0';
        if (SHORT1FROMMP(mp1)==pData->usCurrentTab)
          pData->usCurrentTab--;
      } else {
        pPTD=pData->perTabData;  /* first tab */
        for (i=1;i<SHORT1FROMMP(mp1)-1;i++)
          pPTD=pPTD->nextPerTabdata; /* at end of loop tab before the one to delete */
        pPTD1=pPTD->nextPerTabdata;  /* the tab to remove */
        if (pPTD1->pszTabCaption)
          free(pPTD1->pszTabCaption);
        pPTD->nextPerTabdata=pPTD1->nextPerTabdata;
        free(pPTD1);
      }
      pData->usTabCount--;
      WinSendMsg(WinQueryWindow(hwnd,QW_OWNER),
                 WM_CONTROL,
                 MPFROM2SHORT(WinQueryWindowUShort(hwnd,QWS_ID),TTN_TABCHANGE),
                 MPFROMSHORT(pData->usCurrentTab));
      WinInvalidateRect( hwnd, 0, TRUE);
      return (MRESULT) TABC_END;
    }
    case TABM_SETHILITTAB:
    {
      if (pData->usCurrentTab!=SHORT1FROMMP(mp1)) {
        pData->usCurrentTab=SHORT1FROMMP(mp1);
        WinInvalidateRect( hwnd, 0, TRUE);
      }
      return (MRESULT) TABC_END;
    }
    case TABM_SETCAPTION:
    {
      int i;
      PPERTABDATA pTabData=pData->perTabData;
      if (LONGFROMMP(mp1)<=pData->usTabCount) {
        for (i=1;i<LONGFROMMP(mp1);i++)
          pTabData=pTabData->nextPerTabdata;
        free(pTabData->pszTabCaption);
        pTabData->pszTabCaption=calloc(LONGFROMMP(mp1),1);
        strcpy(pTabData->pszTabCaption,(char *)mp2);
        WinInvalidateRect( hwnd, 0, TRUE);
        return  (MRESULT)1;
      } else {
        return (MRESULT)TABC_FAILURE;
      } /* endif */
    }
    }
  /* call the default window procedure so normal things still work */
  return WinDefWindowProc( hwnd, msg, mp1, mp2);
}
void DrawLine (HPS hps, int x1,int x2, int y1, int y2)
{
  POINTL ptl;
  ptl.x=x1;
  ptl.y=y1;
  GpiMove(hps,&ptl);
  ptl.x=x2;
  ptl.y=y2;
  GpiLine(hps,&ptl);
}
