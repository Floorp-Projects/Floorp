/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsDeviceContextMac.h"
#include "nsRenderingContextMac.h"
#include "nsDeviceContextSpecMac.h"
#include "nsString.h"
#include "nsHashtable.h"
#include <TextEncodingConverter.h>
#include <TextCommon.h>
#include <StringCompare.h>
#include <Fonts.h>
#include <Resources.h>
#include "il_util.h"
#include <FixMath.h>

static NS_DEFINE_IID(kDeviceContextIID, NS_IDEVICE_CONTEXT_IID);

//------------------------------------------------------------------------

nsDeviceContextMac :: nsDeviceContextMac()
{

  NS_INIT_REFCNT();
  mSpec = nsnull;
  
}

//------------------------------------------------------------------------

nsDeviceContextMac :: ~nsDeviceContextMac()
{
}

//------------------------------------------------------------------------

NS_IMPL_QUERY_INTERFACE(nsDeviceContextMac, kDeviceContextIID);
NS_IMPL_ADDREF(nsDeviceContextMac);
NS_IMPL_RELEASE(nsDeviceContextMac);

//------------------------------------------------------------------------

NS_IMETHODIMP nsDeviceContextMac :: Init(nsNativeWidget aNativeWidget)
{
GDHandle			thegd;
PixMapHandle	thepix;
double				pix_inch;

	// this is a windowptr, or grafptr, native to macintosh only
	mSurface = aNativeWidget;

	// get depth and resolution
  thegd = ::GetMainDevice();
	thepix = (**thegd).gdPMap;					// dereferenced handle: don't move memory below!
	mDepth = (**thepix).pixelSize;
	pix_inch = Fix2X((**thepix).hRes);
	mTwipsToPixels = pix_inch/(float)NSIntPointsToTwips(72);
	mPixelsToTwips = 1.0f/mTwipsToPixels;
	
  return DeviceContextImpl::Init(aNativeWidget);
}


/** ---------------------------------------------------
 *  See documentation in nsIDeviceContext.h
 *	@update 12/9/98 dwc
 */
NS_IMETHODIMP nsDeviceContextMac :: CreateRenderingContext(nsIRenderingContext *&aContext)
{
nsRenderingContextMac *pContext;
nsresult              rv;
GrafPtr								thePort;

	pContext = new nsRenderingContextMac();
  if (nsnull != pContext){
    NS_ADDREF(pContext);

   	::GetPort(&thePort);

    if (nsnull != thePort){
      rv = pContext->Init(this,thePort);
    }
    else
      rv = NS_ERROR_OUT_OF_MEMORY;
  }
  else
    rv = NS_ERROR_OUT_OF_MEMORY;

  if (NS_OK != rv){
    NS_IF_RELEASE(pContext);
  }
  aContext = pContext;
  return rv;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsDeviceContextMac :: SupportsNativeWidgets(PRBool &aSupportsWidgets)
{
  //XXX it is very critical that this not lie!! MMP
  
  // ¥¥¥ VERY IMPORTANT (pinkerton)
  // This routine should return true if the widgets behave like Win32
  // "windows", that is they paint themselves and the app never knows about
  // them or has to send them update events. We were returning false which
  // meant that raptor needed to make sure to redraw them. However, if we
  // return false, the widgets never get created because the CreateWidget()
  // call in nsView never creates the widget. If we return true (which makes
  // things work), we are lying because the controls really need those
  // precious update/repaint events.
  // 
  // The situation we need is the following:
  // - return false from SupportsNativeWidgets()
  // - Create() is called on widgets when the above case is true.
  // 
  // Raptor currently doesn't work this way and needs to be fixed.
  // (please remove this comment when this situation is rectified)
  
  //if (nsnull == mSurface)
    aSupportsWidgets = PR_TRUE;
  //else
   //aSupportsWidgets = PR_FALSE;

  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsDeviceContextMac :: GetScrollBarDimensions(float &aWidth, float &aHeight) const
{
  // XXX Should we push this to widget library
  aWidth = 320.0;
  aHeight = 320.0;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextMac :: GetSystemAttribute(nsSystemAttrID anID, SystemAttrStruct * aInfo) const
{
  nsresult status = NS_OK;

  switch (anID) {
    //---------
    // Colors
    //---------
    case eSystemAttr_Color_WindowBackground:
        *aInfo->mColor = NS_RGB(0xdd,0xdd,0xdd);
        break;
    case eSystemAttr_Color_WindowForeground:
        *aInfo->mColor = NS_RGB(0x00,0x00,0x00);        
        break;
    case eSystemAttr_Color_WidgetBackground:
        *aInfo->mColor = NS_RGB(0xdd,0xdd,0xdd);
        break;
    case eSystemAttr_Color_WidgetForeground:
        *aInfo->mColor = NS_RGB(0x00,0x00,0x00);        
        break;
    case eSystemAttr_Color_WidgetSelectBackground:
        *aInfo->mColor = NS_RGB(0x80,0x80,0x80);
        break;
    case eSystemAttr_Color_WidgetSelectForeground:
        *aInfo->mColor = NS_RGB(0x00,0x00,0x80);
        break;
    case eSystemAttr_Color_Widget3DHighlight:
        *aInfo->mColor = NS_RGB(0xa0,0xa0,0xa0);
        break;
    case eSystemAttr_Color_Widget3DShadow:
        *aInfo->mColor = NS_RGB(0x40,0x40,0x40);
        break;
    case eSystemAttr_Color_TextBackground:
        *aInfo->mColor = NS_RGB(0xff,0xff,0xff);
        break;
    case eSystemAttr_Color_TextForeground: 
        *aInfo->mColor = NS_RGB(0x00,0x00,0x00);
        break;
    case eSystemAttr_Color_TextSelectBackground:
        *aInfo->mColor = NS_RGB(0x00,0x00,0x80);
        break;
    case eSystemAttr_Color_TextSelectForeground:
        *aInfo->mColor = NS_RGB(0xff,0xff,0xff);
        break;

    //---------
    // Size
    //---------
    case eSystemAttr_Size_ScrollbarHeight : 
        aInfo->mSize = 21;
        break;
    case eSystemAttr_Size_ScrollbarWidth : 
        aInfo->mSize = 21;
        break;
    case eSystemAttr_Size_WindowTitleHeight:
        aInfo->mSize = 0;
        break;
    case eSystemAttr_Size_WindowBorderWidth:
        aInfo->mSize = 4;
        break;
    case eSystemAttr_Size_WindowBorderHeight:
        aInfo->mSize = 4;
        break;
    case eSystemAttr_Size_Widget3DBorder:
        aInfo->mSize = 4;
        break;

    //---------
    // Fonts
    //---------
    case eSystemAttr_Font_Caption : 
    case eSystemAttr_Font_Icon : 
    case eSystemAttr_Font_Menu : 
    case eSystemAttr_Font_MessageBox : 
    case eSystemAttr_Font_SmallCaption : 
    case eSystemAttr_Font_StatusBar : 
    case eSystemAttr_Font_Tooltips : 
      status = NS_ERROR_FAILURE;
      break;

  } // switch 

  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsDeviceContextMac :: GetDrawingSurface(nsIRenderingContext &aContext, nsDrawingSurface &aSurface)
{
  aContext.CreateDrawingSurface(nsnull, 0, aSurface);
  return nsnull == aSurface ? NS_ERROR_OUT_OF_MEMORY : NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsDeviceContextMac::GetDepth(PRUint32& aDepth)
{
  aDepth = mDepth;
  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsDeviceContextMac :: ConvertPixel(nscolor aColor, PRUint32 & aPixel)
{
  aPixel = aColor;
  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsDeviceContextMac::CreateILColorSpace(IL_ColorSpace*& aColorSpace)
{
  nsresult result = NS_OK;


  return result;
}

//------------------------------------------------------------------------


NS_IMETHODIMP nsDeviceContextMac::GetILColorSpace(IL_ColorSpace*& aColorSpace)
{

  if (nsnull == mColorSpace) {
    IL_RGBBits colorRGBBits;
  
    // Default is to create a 32-bit color space
    colorRGBBits.red_shift = 16;  
    colorRGBBits.red_bits = 8;
    colorRGBBits.green_shift = 8;
    colorRGBBits.green_bits = 8; 
    colorRGBBits.blue_shift = 0; 
    colorRGBBits.blue_bits = 8;  
  
    mColorSpace = IL_CreateTrueColorSpace(&colorRGBBits, 32);
    if (nsnull == mColorSpace) {
      aColorSpace = nsnull;
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  NS_POSTCONDITION(nsnull != mColorSpace, "null color space");
  aColorSpace = mColorSpace;
  IL_AddRefToColorSpace(aColorSpace);
  return NS_OK;
}


//------------------------------------------------------------------------

NS_IMETHODIMP nsDeviceContextMac :: CheckFontExistence(const nsString& aFontName)
{
  	short fontNum;
	if (GetMacFontNumber(aFontName, fontNum))
		return NS_OK;
	else
		return NS_ERROR_FAILURE;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsDeviceContextMac::GetDeviceSurfaceDimensions(PRInt32 &aWidth, PRInt32 &aHeight)
{
  aWidth = 1;
  aHeight = 1;

  return NS_ERROR_FAILURE;
}


//------------------------------------------------------------------------

NS_IMETHODIMP nsDeviceContextMac::GetDeviceContextFor(nsIDeviceContextSpec *aDevice,nsIDeviceContext *&aContext)
{
GrafPtr	curPort;	
double	pix_Inch;
THPrint	thePrintRecord;			// handle to print record

	aContext = new nsDeviceContextMac();
	((nsDeviceContextMac*)aContext)->mSpec = aDevice;
	NS_ADDREF(aDevice);
	
	::GetPort(&curPort);
	
	thePrintRecord = ((nsDeviceContextSpecMac*)aDevice)->mPrtRec;
	pix_Inch = (**thePrintRecord).prInfo.iHRes;
	
	((nsDeviceContextMac*)aContext)->Init(curPort);

	((nsDeviceContextMac*)aContext)->mPageRect = (**thePrintRecord).prInfo.rPage;	
	((nsDeviceContextMac*)aContext)->mTwipsToPixels = pix_Inch/(float)NSIntPointsToTwips(72);
	((nsDeviceContextMac*)aContext)->mPixelsToTwips = 1.0f/mTwipsToPixels;
  ((nsDeviceContextMac*)aContext)->mAppUnitsToDevUnits = mTwipsToPixels;
  ((nsDeviceContextMac*)aContext)->mDevUnitsToAppUnits = 1.0f / mAppUnitsToDevUnits;
	//((nsDeviceContextMac*)aContext)->Init(this);
  return NS_OK;
}


//------------------------------------------------------------------------

NS_IMETHODIMP nsDeviceContextMac::BeginDocument(void)
{
GrafPtr	thePort;
 
 	if(((nsDeviceContextSpecMac*)(this->mSpec))->mPrintManagerOpen) {
 		::GetPort(&mOldPort);
 		thePort = (GrafPtr)::PrOpenDoc(((nsDeviceContextSpecMac*)(this->mSpec))->mPrtRec,nsnull,nsnull);
  	((nsDeviceContextSpecMac*)(this->mSpec))->mPrinterPort = (TPrPort*)thePort;
  	SetDrawingSurface(((nsDeviceContextSpecMac*)(this->mSpec))->mPrtRec);
  	SetPort(thePort);
  }
  return NS_OK;
}


//------------------------------------------------------------------------

NS_IMETHODIMP nsDeviceContextMac::EndDocument(void)
{
 	if(((nsDeviceContextSpecMac*)(this->mSpec))->mPrintManagerOpen){
 		::SetPort(mOldPort);
		::PrCloseDoc(((nsDeviceContextSpecMac*)(this->mSpec))->mPrinterPort);
	}
  return NS_OK;
}


//------------------------------------------------------------------------

NS_IMETHODIMP nsDeviceContextMac::BeginPage(void)
{
 	if(((nsDeviceContextSpecMac*)(this->mSpec))->mPrintManagerOpen) 
		::PrOpenPage(((nsDeviceContextSpecMac*)(this->mSpec))->mPrinterPort,nsnull);
  return NS_OK;
}


//------------------------------------------------------------------------

NS_IMETHODIMP nsDeviceContextMac::EndPage(void)
{
 	if(((nsDeviceContextSpecMac*)(this->mSpec))->mPrintManagerOpen) {
 		::SetPort((GrafPtr)(((nsDeviceContextSpecMac*)(this->mSpec))->mPrinterPort));
		::PrClosePage(((nsDeviceContextSpecMac*)(this->mSpec))->mPrinterPort);
	}
  return NS_OK;
}


#pragma mark -

//------------------------------------------------------------------------

nsHashtable* nsDeviceContextMac :: gFontInfoList = nsnull;

class FontNameKey : public nsHashKey
{
public:
  FontNameKey(const nsString& aString);

  virtual PRUint32 HashValue(void) const;
  virtual PRBool Equals(const nsHashKey *aKey) const;
  virtual nsHashKey *Clone(void) const;

  nsAutoString  mString;
};

FontNameKey::FontNameKey(const nsString& aString)
{
	aString.ToUpperCase(mString);
}

PRUint32 FontNameKey::HashValue(void) const
{
	return nsCRT::HashValue(mString);
}

PRBool FontNameKey::Equals(const nsHashKey *aKey) const
{
  return mString.Equals(((FontNameKey*)aKey)->mString);
}

nsHashKey* FontNameKey::Clone(void) const
{
  return new FontNameKey(mString);
}

#pragma mark -

//------------------------------------------------------------------------

void nsDeviceContextMac :: InitFontInfoList()
{

	OSStatus err;
	if (!gFontInfoList) 
	{
		gFontInfoList = new nsHashtable();
		if (!gFontInfoList)
			return;

		short numFONDs = ::CountResources('FOND');
		TextEncoding unicodeEncoding = ::CreateTextEncoding(kTextEncodingUnicodeDefault, 
															kTextEncodingDefaultVariant,
													 		kTextEncodingDefaultFormat);
		TECObjectRef converter = nil;
		ScriptCode lastscript = smUninterp;
		for (short i = 1; i <= numFONDs ; i++)
		{
			Handle fond = ::GetIndResource('FOND', i);
			if (fond)
			{
				short	fondID;
				OSType	resType;
				Str255	fontName;
				::GetResInfo(fond, &fondID, &resType, fontName); 

				ScriptCode script = ::FontToScript(fondID);
				if (script != lastscript)
				{
					lastscript = script;

					TextEncoding sourceEncoding;
					err = ::UpgradeScriptInfoToTextEncoding(script, kTextLanguageDontCare, 
								kTextRegionDontCare, NULL, &sourceEncoding);
							
					if (converter)
						err = ::TECDisposeConverter(converter);

					err = ::TECCreateConverter(&converter, sourceEncoding, unicodeEncoding);
					if (err != noErr)
						converter = nil;
				}

				if (converter)
				{
					PRUnichar unicodeFontName[sizeof(fontName)];
					ByteCount actualInputLength, actualOutputLength;
					err = ::TECConvertText(converter, &fontName[1], fontName[0], &actualInputLength, 
												(TextPtr)unicodeFontName , sizeof(unicodeFontName), &actualOutputLength);	
					unicodeFontName[actualOutputLength / sizeof(PRUnichar)] = '\0';

	        		FontNameKey key(unicodeFontName);
					gFontInfoList->Put(&key, (void*)fondID);
				}
				::ReleaseResource(fond);
			}
		}
		if (converter)
			err = ::TECDisposeConverter(converter);				
	}
}



//------------------------------------------------------------------------

bool nsDeviceContextMac :: GetMacFontNumber(const nsString& aFontName, short &aFontNum)
{
	//¥TODO?: Maybe we shouldn't call that function so often. If nsFont could store the
	//				fontNum, nsFontMetricsMac::SetFont() wouldn't need to call this at all.
	InitFontInfoList();
    FontNameKey key(aFontName);
	aFontNum = (short)gFontInfoList->Get(&key);

	return (aFontNum != 0);
}


//------------------------------------------------------------------------
// Override to tweak font settings
nsresult nsDeviceContextMac::CreateFontAliasTable()
{
  nsresult result = NS_OK;

  if (nsnull == mFontAliasTable) {
    mFontAliasTable = new nsHashtable();
    if (nsnull != mFontAliasTable)
    {
			nsAutoString  fontTimes("Times");
			nsAutoString  fontTimesNewRoman("Times New Roman");
			nsAutoString  fontTimesRoman("Times Roman");
			nsAutoString  fontArial("Arial");
			nsAutoString  fontHelvetica("Helvetica");
			nsAutoString  fontCourier("Courier");
			nsAutoString  fontCourierNew("Courier New");
			nsAutoString  fontUnicode("Unicode");
			nsAutoString  fontBitstreamCyberbit("Bitstream Cyberbit");
			nsAutoString  fontNullStr;

      AliasFont(fontTimes, fontTimesNewRoman, fontTimesRoman, PR_FALSE);
      AliasFont(fontTimesRoman, fontTimesNewRoman, fontTimes, PR_FALSE);
      AliasFont(fontTimesNewRoman, fontTimesRoman, fontTimes, PR_FALSE);
      AliasFont(fontArial, fontHelvetica, fontNullStr, PR_FALSE);
      AliasFont(fontHelvetica, fontArial, fontNullStr, PR_FALSE);
      AliasFont(fontCourier, fontCourierNew, fontNullStr, PR_FALSE);		// changed from DeviceContextImpl
      AliasFont(fontCourierNew, fontCourier, fontNullStr, PR_FALSE);
      AliasFont(fontUnicode, fontBitstreamCyberbit, fontNullStr, PR_FALSE); // XXX ????
    }
    else {
      result = NS_ERROR_OUT_OF_MEMORY;
    }
  }
  return result;
}
