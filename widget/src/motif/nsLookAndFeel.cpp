/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsLookAndFeel.h"

#include "nsXPLookAndFeel.h"
 
NS_IMPL_ISUPPORTS1(nsLookAndFeel, nsILookAndFeel)

//-------------------------------------------------------------------------
//
// Query interface implementation
//
//-------------------------------------------------------------------------
nsLookAndFeel::nsLookAndFeel()
{
  NS_INIT_REFCNT();
}

nsLookAndFeel::~nsLookAndFeel()
{
}

NS_IMETHODIMP nsLookAndFeel::GetColor(const nsColorID aID, nscolor &aColor)
{
    nsresult res = NS_OK;

    if (mXPLookAndFeel)
    {
        res = mXPLookAndFeel->GetColor(aID, aColor);
        if (NS_SUCCEEDED(res))
            return res;
        res = NS_OK;
    }

    switch (aID) {
    case eColor_WindowBackground:
        aColor = NS_RGB(0xff,0xff,0xff);
        break;
    case eColor_WindowForeground:
        aColor = NS_RGB(0x00,0x00,0x00);        
        break;
    case eColor_WidgetBackground:
        aColor = NS_RGB(192, 192, 192);
        break;
    case eColor_WidgetForeground:
        aColor = NS_RGB(0x00,0x00,0x00);        
        break;
    case eColor_WidgetSelectBackground:
        aColor = NS_RGB(0x80,0x80,0x80);
        break;
    case eColor_WidgetSelectForeground:
        aColor = NS_RGB(0x00,0x00,0x80);
        break;
    case eColor_Widget3DHighlight:
        aColor = NS_RGB(0xa0,0xa0,0xa0);
        break;
    case eColor_Widget3DShadow:
        aColor = NS_RGB(0x40,0x40,0x40);
        break;
    case eColor_TextBackground:
        aColor = NS_RGB(0xff,0xff,0xff);
        break;
    case eColor_TextForeground: 
        aColor = NS_RGB(0x00,0x00,0x00);
	      break;
    case eColor_TextSelectBackground:
        res = NS_RGB(0x00,0x00,0x80);
        break;
    case eColor_TextSelectForeground:
        aColor = NS_RGB(0xff,0xff,0xff);
        break;
    default:
        aColor = NS_RGB(0xff,0xff,0xff);
        res    = NS_ERROR_FAILURE;
        break;
    }

    return res;
}

NS_IMETHODIMP nsLookAndFeel::GetMetric(const nsMetricID aID, PRInt32 & aMetric)
{
    nsresult res = NS_OK;

    if (mXPLookAndFeel)
    {
        res = mXPLookAndFeel->GetMetric(aID, aMetric);
        if (NS_SUCCEEDED(res))
            return res;
        res = NS_OK;
    }

    switch (aID) {
    case eMetric_WindowTitleHeight:
        aMetric = 0;
        break;
    case eMetric_WindowBorderWidth:
        aMetric = 4;
        break;
    case eMetric_WindowBorderHeight:
        aMetric = 4;
        break;
    case eMetric_Widget3DBorder:
        aMetric = 4;
        break;
    case eMetric_TextFieldHeight:
        aMetric = 30;
        break;
    case eMetric_CheckboxSize:
        aMetric = 12;
        break;
    case eMetric_RadioboxSize:
        aMetric = 12;
        break;
    case eMetric_CaretBlinkTime:
        aMetric = 500;
        break;
    case eMetric_SingleLineCaretWidth:
    case eMetric_MultiLineCaretWidth:
        aMetric = 1;
        break;
    case eMetric_SubmenuDelay:
        aMetric = 200;
        break;
    default:
        aMetric = 0;
        res     = NS_ERROR_FAILURE;
    }
    return res;
}

NS_METHOD nsLookAndFeel::GetMetric(const nsMetricFloatID aID, float & aMetric)
{

  if (mXPLookAndFeel)
  {
    res = mXPLookAndFeel->GetMetric(aID, aMetric);
    if (NS_SUCCEEDED(res))
      return res;
  }

  // FIXME: Need to implement.  --ZuperDee
  return NS_OK;
}

#ifdef NS_DEBUG
NS_IMETHODIMP nsLookAndFeel::GetNavSize(const nsMetricNavWidgetID aWidgetID,
                                        const nsMetricNavFontID   aFontID, 
                                        const PRInt32             aFontSize, 
                                        nsSize &aSize)
{
  if (mXPLookAndFeel)
  {
    nsresult rv = mXPLookAndFeel->GetNavSize(aWidgetID, aFontID, aFontSize, aSize);
    if (NS_SUCCEEDED(rv))
      return rv;
  }

  aSize.width  = 0;
  aSize.height = 0;
  return NS_ERROR_NOT_IMPLEMENTED;
}
#endif
