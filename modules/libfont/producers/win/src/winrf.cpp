/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
/*
 * Implements the Native windows RenderableFont
 */

#include "Pwinrf.h"

#include "nf.h"
#include "Mnfrc.h"
#include "Mnffbp.h"

#include "coremem.h"

// todo: get rid of this global!!
extern struct nffbp    *global_pBrokerObj;

/****************************************************************************
 *				 Implementation of common interface methods
 ****************************************************************************/

#ifdef OVERRIDE_winrf_getInterface
#include "Mnfrf.h"
#ifdef __cplusplus
extern "C"
#endif
JMC_PUBLIC_API(void*)
_winrf_getInterface(struct winrf* self, jint op, const JMCInterfaceID* iid, JMCException* *exc)
{
	if (memcmp(iid, &nfrf_ID, sizeof(JMCInterfaceID)) == 0)
		return winrfImpl2winrf(winrf2winrfImpl(self));
	return _winrf_getBackwardCompatibleInterface(self, iid, exc);
}
#endif /* OVERRIDE_winrf_getInterface */

#ifdef __cplusplus
extern "C"
#endif
JMC_PUBLIC_API(void*)
_winrf_getBackwardCompatibleInterface(struct winrf* self,
									const JMCInterfaceID* iid,
									struct JMCException* *exceptionThrown)
{
	return (NULL);
}

#ifdef __cplusplus
extern "C"
#endif
JMC_PUBLIC_API(void)
_winrf_init(struct winrf* self, struct JMCException* *exceptionThrown)
{
    /* FONTDISPLAYER developers:
	 * This is supposed to do initialization that is required for the 
     * font Displayer object.
     */

	struct winrfImpl *pSampleRenderableFontData = winrf2winrfImpl(self);

	if( pSampleRenderableFontData ) {
		pSampleRenderableFontData->mRF_tmDescent		= 0;
		pSampleRenderableFontData->mRF_tmMaxCharWidth	= 0;
		pSampleRenderableFontData->mRF_tmAscent			= 0;
		pSampleRenderableFontData->mRF_pointSize		= 0.0;
		pSampleRenderableFontData->mRF_hFont			= NULL;
	}
}

#ifdef OVERRIDE_winrf_finalize
#ifdef __cplusplus
extern "C"
#endif
JMC_PUBLIC_API(void)
_winrf_finalize(struct winrf* self, jint op, JMCException* *exception)
{
	/* FONTDISPLAYER developer:
	 * Free any private data for this object here.
	 */
	struct winrfImpl *pSampleRenderableFontData = winrf2winrfImpl(self);

	if( pSampleRenderableFontData ) {
		if( pSampleRenderableFontData->mRF_hFont )
			DeleteObject( pSampleRenderableFontData->mRF_hFont );
		pSampleRenderableFontData->mRF_hFont = NULL;
	}
	
	/*
	 * Tell the fontDisplayer that this renderable font is going away.
	 */
	nffbp_RfDone(global_pBrokerObj, (struct nfrf *)self, NULL);

	/* Finally, free the memory for the object containter. */
	XP_FREEIF(self);
}
#endif /* OVERRIDE_winrf_finalize */


/****************************************************************************
 *				 Implementation of Object specific methods
 *
 * FONTDISPLAYER developers:
 * Implement your RenderableFont methods here.
 ****************************************************************************/


#ifdef __cplusplus
extern "C"
#endif
JMC_PUBLIC_API(struct nffmi*)
_winrf_GetMatchInfo(struct winrf* self, jint op,
				  struct JMCException* *exceptionThrown)
{
	struct winrfImpl *pSampleRenderableFontData = winrf2winrfImpl(self);
	return NULL;
}


#ifdef __cplusplus
extern "C"
#endif
JMC_PUBLIC_API(jdouble)
_winrf_GetPointSize(struct winrf* self, jint op,
				  struct JMCException* *exceptionThrown)
{
	struct winrfImpl *pSampleRenderableFontData = winrf2winrfImpl(self);
	return( pSampleRenderableFontData->mRF_pointSize );
	//return (0);
}


#ifdef __cplusplus
extern "C"
#endif
JMC_PUBLIC_API(jint)
_winrf_GetMaxWidth(struct winrf* self, jint op,
				 struct JMCException* *exceptionThrown)
{
	struct winrfImpl  *pSampleRenderableFontData;

	pSampleRenderableFontData = winrf2winrfImpl(self);

	// fetched and buffered in _cfp_GetRenderableFont()
	return( pSampleRenderableFontData->mRF_tmMaxCharWidth );

}


#ifdef __cplusplus
extern "C"
#endif
JMC_PUBLIC_API(jint)
_winrf_GetFontAscent(struct winrf* self, jint op,
				  struct JMCException* *exceptionThrown)
{
	struct winrfImpl  *pSampleRenderableFontData;

	pSampleRenderableFontData = winrf2winrfImpl(self);

	// fetched and buffered in _cfp_GetRenderableFont()
	return( pSampleRenderableFontData->mRF_tmAscent );
}


#ifdef __cplusplus
extern "C"
#endif
JMC_PUBLIC_API(jint)
_winrf_GetFontDescent(struct winrf* self, jint op,
				   struct JMCException* *exceptionThrown)
{
	struct winrfImpl  *pSampleRenderableFontData;
	pSampleRenderableFontData = winrf2winrfImpl(self);

	// fetched and buffered in _cfp_GetRenderableFont()
	return( pSampleRenderableFontData->mRF_tmDescent );

}


#ifdef __cplusplus
extern "C"
#endif
JMC_PUBLIC_API(jint)
_winrf_GetMaxLeftBearing(struct winrf* self, jint op,
					   struct JMCException* *exceptionThrown)
{
	struct winrfImpl *pSampleRenderableFontData = winrf2winrfImpl(self);
	return (0);
}


#ifdef __cplusplus
extern "C"
#endif
JMC_PUBLIC_API(jint)
_winrf_GetMaxRightBearing(struct winrf* self, jint op,
						struct JMCException* *exceptionThrown)
{
	struct winrfImpl  *pSampleRenderableFontData;

	pSampleRenderableFontData = winrf2winrfImpl(self);


	return (0);
}


#ifdef __cplusplus
extern "C"
#endif
JMC_PUBLIC_API(void)
_winrf_SetTransformMatrix(struct winrf* self, jint op,
                        void ** matrix, jsize matrix_length,
						struct JMCException* *exceptionThrown)
{
	struct winrfImpl *pSampleRenderableFontData = winrf2winrfImpl(self);
	return;
}


#ifdef __cplusplus
extern "C"
#endif
JMC_PUBLIC_API(void *)
_winrf_GetTransformMatrix(struct winrf* self, jint op,
						struct JMCException* *exceptionThrown)
{
	struct winrfImpl *pSampleRenderableFontData = winrf2winrfImpl(self);
	return NULL;
}

#ifndef _WIN32
static BOOL GetTextExtentExPoint(HDC hDC, LPCSTR cp, int len, 
              int rightBoundary, LPINT charCountInRow, LPINT charPos, LPSIZE zzz )
{                     
  
  DWORD dd;

  LPCSTR  lcp = cp;
  int     nn,  xx  = 0;  
  
  for( nn = 0; nn < len && xx < rightBoundary ; nn++ )
  {
    dd = GetTextExtent( hDC,  lcp, 1);
    charPos[ nn ] = xx   += LOWORD(dd); 
    lcp++ ;
    }
  
  *charCountInRow = nn;    
  zzz->cx = xx;
  zzz->cy = HIWORD(dd);    
    return(TRUE);
}              
#endif

#ifdef __cplusplus
extern "C"
#endif
JMC_PUBLIC_API(jint)
_winrf_MeasureText(struct winrf* self, jint op, struct nfrc* rc,
				 jint charSpacing, /* XXX Arthur needs to implement this - dp */
				 jbyte* bytes, jsize nbytes,
				 jint *charLocs, jsize charLocs_length,
				 struct JMCException* *exceptionThrown)
{
	struct winrfImpl *pSampleRenderableFontData = winrf2winrfImpl(self);

    HDC     hDC;
    // HFONT   hFont;
    // HFONT   hOldFont;
    jsize   bytesToMeasure;         //  unsigned long
    SIZE    theSize;
	BOOL	bb;
#ifndef NO_PERFORMANCE_HACK
	struct rc_data *DisplayRCData = NF_GetRCNativeData(rc);
#else
    struct rc_data  RenderingContextData;
#endif /* NO_PERFORMANCE_HACK */
	jint	nn;
	int		ii;
    int     temp_encoding   = pSampleRenderableFontData->mRF_encoding;
	
	// GetTextExtentExPoint uses int_array not long(jint)_array
	int		nCharLoc[1024];        // must > charLocs_length

	if( charLocs_length > 1024 )
		return(0);		// error

#ifndef NO_PERFORMANCE_HACK
	hDC = (HDC) DisplayRCData->t.directRc.dc;
#else
    RenderingContextData = nfrc_GetPlatformData( rc, NULL/*exception*/ ); 
    if( RenderingContextData.majorType != NF_RC_DIRECT )
        return(-1);		// error
    
    // get the passed-in HDC, 
    hDC     = (HDC) RenderingContextData.t.directRc.dc ;
#endif /* NO_PERFORMANCE_HACK */


    // todo compare saved HDC with passed-in HDC, they must be compitable.

	// Select font should have been done in prepareDraw().
    // get the font we saved when create this rf.
	// hFont   = pSampleRenderableFontData->mRF_hFont;
    //load the font, and save the old fond
    // hOldFont = SelectObject( hDC, hFont);

	if( charLocs == NULL ) {		// only get the whole lenght, not the array
#ifdef WIN32
		if( temp_encoding & encode_unicode ) {  //todo 
			bb = ::GetTextExtentPoint32W(hDC, (const unsigned short *)bytes, nbytes/2, &theSize );
		} else {
			bb = GetTextExtentPoint32(hDC, bytes, nbytes, &theSize );
		}
#else
		bb = GetTextExtentPoint(hDC, bytes, nbytes, &theSize );
#endif
		nn = bb ? theSize.cx : -1 ;

	} else {

		// get the minimum of the length
		bytesToMeasure = nbytes;
		if( bytesToMeasure > charLocs_length ) 
			bytesToMeasure = charLocs_length;
        
		// GetTextExtentPoint for 16 bit
// TOD multi byte
		if( temp_encoding & encode_unicode ) {  
			//todo 
		} else {
			bb = GetTextExtentExPoint( 
					hDC,                // the HDC with selected Font
					bytes,              // string to measure
					bytesToMeasure,     // number of bytes in string
					2048,               // max logical width for formated string, big enough to let the measuring go
					NULL,				// count of char fix in max logical width.
					nCharLoc,           // the result, width of sub-string for each chr position.
					&theSize            // not used
			);
		}

		nn = bb ? 0 : -1 ;

		// copy the int array to long(jint) array
		for( ii=0; ii < (int)bytesToMeasure; ii++ ) {
			charLocs[ii] = nCharLoc[ii];
		}

	} 


	// Select font should have been done in prepareDraw().
    // put the old font back after drawtext.
    //SelectObject( hDC, hOldFont);

	// return value is in charLocs
    return ( nn );
}

#ifdef __cplusplus
extern "C"
#endif
JMC_PUBLIC_API(jint)
_winrf_MeasureBoundingBox(struct winrf* self, jint op, struct nfrc* rc,
				 jint charSpacing, /* XXX Arthur needs to implement this - dp */
				 jbyte* bytes, jsize nbytes, struct nf_bounding_box *bboxp,
				 struct JMCException* *exceptionThrown)
{
	/* XXX Arthur needs to look over this implementation - dp */
	if (!bboxp)
		return (-1);

	bboxp->ascent = nfrf_GetFontAscent(self, NULL);
	bboxp->descent = nfrf_GetFontDescent(self, NULL);
	bboxp->width = nfrf_MeasureText(self, rc, charSpacing, bytes, nbytes,
		NULL, 0, NULL);
	bboxp->lbearing = 0;
	bboxp->rbearing = bboxp->width;

	return (0);
}

#ifdef __cplusplus
extern "C"
#endif
JMC_PUBLIC_API(jint)
_winrf_DrawText(struct winrf* self, jint op, struct nfrc* rc,
			  jint x, jint y,
			  jint charSpacing, /* XXX Arthur needs to implement this - dp */
			  jbyte* bytes, jsize nbytes,
			  struct JMCException* *exceptionThrown)
{
	struct winrfImpl  *pSampleRenderableFontData;
#ifndef NO_PERFORMANCE_HACK
	struct rc_data *DisplayRCData = NF_GetRCNativeData(rc);
#else
    struct rc_data  RenderingContextData;
#endif /* NO_PERFORMANCE_HACK */
    HDC             hDC;
    // HFONT           hFont;
    //HFONT           hOldFont;
    int				temp_encoding;

    // get a pointer to my implementation data of renderable font.
    pSampleRenderableFontData = winrf2winrfImpl(self);
	
    // Get a pointer to platform specific RenderingContext data
    // form passed-in rc. For Windows, it has a HDC.
#ifndef NO_PERFORMANCE_HACK
	hDC = (HDC) DisplayRCData->t.directRc.dc;
#else
    RenderingContextData = nfrc_GetPlatformData( rc, NULL/*exception*/ ); 
    if( RenderingContextData.majorType != NF_RC_DIRECT )
        return(1);
    
    // get the passed-in HDC for drawing text, it may have clip info.
    hDC     = (HDC) RenderingContextData.t.directRc.dc ;
#endif /* NO_PERFORMANCE_HACK */

    // There are 2 HDC's here. One is passed in, another is
    // saved when this rf was created( pSampleRenderableFontData->mRF_hDC).
    // todo: compare those 2 HDC's to make sure they are compitable.

    temp_encoding   = pSampleRenderableFontData->mRF_encoding;

	// Select font should have been done in prepareDraw().
    // get the font we saved when create this rf.
    // hFont   = pSampleRenderableFontData->mRF_hFont;
    //load the font, and save the old fond
    // hOldFont = SelectObject( hDC, hFont);

#ifdef WIN32
	if( temp_encoding & encode_unicode ) {  //todo 
		// Now just draw text through the passed-in DC, with my Font loaded.
		TextOutW(hDC, x, y, (const unsigned short *)bytes, nbytes / 2);

		if( temp_encoding & encode_needDrawUnderline )
		{
			// Measure Text By Calling MeasureText
			// Draw the Underline
			SIZE    theSize;
			int		baseLine = 0;    

			// must get the size to decide where to draw, and the thickness of underLine
			if( TRUE == ::GetTextExtentPoint32W(hDC, (const unsigned short *)bytes, nbytes/2, &theSize ) ) {
				baseLine = y + theSize.cy - 1;		// at bottom of the box
			}

			if( baseLine > 0 ) {
				
				// try to get a better underLine position.
				/* this doesn't work on win95, see bug #49585
				OUTLINETEXTMETRIC oltm;
				if( TRUE == ::GetOutlineTextMetrics(hDC, sizeof(OUTLINETEXTMETRIC), & oltm ) ) {
					baseLine = y + oltm.otmsUnderscorePosition;
				}
				*/

				//now draw it
				HPEN cpStrike = ::CreatePen(PS_SOLID, theSize.cy / 10, ::GetTextColor(hDC) );
				HPEN pOldPen = (HPEN)::SelectObject(hDC, cpStrike);
				::MoveToEx(hDC, x, baseLine, NULL);
				::LineTo(hDC, x+theSize.cx, baseLine);
				::SelectObject(hDC, pOldPen);
				::DeleteObject(cpStrike);
			}
		}

	} else {
		// Now just draw text through the passed-in DC, with my Font loaded.
		TextOut( hDC, x, y, bytes, nbytes);
	}
#else
		TextOut( hDC, x, y, bytes, nbytes);
#endif

	// Select font should have been done in prepareDraw().
    // put the old font back after drawtext.
    // SelectObject( hDC, hOldFont);
    
    return (0);
}

#ifdef __cplusplus
extern "C"
#endif
JMC_PUBLIC_API(jint)
_winrf_PrepareDraw(struct winrf* self, jint op, struct nfrc *rc,
				 struct JMCException* *exceptionThrown)
{
	struct winrfImpl  *pSampleRenderableFontData;
    struct rc_data  RenderingContextData;
    HDC             hDC;

    // get a pointer to my implementation data of renderable font.
    pSampleRenderableFontData = winrf2winrfImpl(self);

	if( pSampleRenderableFontData->mRF_hFont == NULL )
		return(1);

    // Get a pointer to platform specific RenderingContext data
    // form passed-in rc. For Windows, it has a HDC.
    RenderingContextData = nfrc_GetPlatformData( rc, NULL/*exception*/ ); 
    if( RenderingContextData.majorType != NF_RC_DIRECT )
        return(1);

    // get the passed-in HDC for drawing text, it may have clip info.
    hDC     = (HDC) RenderingContextData.t.directRc.dc ;

	// get the font we saved when create this rf.
    // load the font, and save the old fond
    RenderingContextData.displayer_data = (void *) SelectObject( hDC, pSampleRenderableFontData->mRF_hFont);

	if( RenderingContextData.displayer_data == NULL )
		return(1);

	return 0;
}

#ifdef __cplusplus
extern "C"
#endif
JMC_PUBLIC_API(jint)
_winrf_EndDraw(struct winrf* self, jint op, struct nfrc *rc,
			 struct JMCException* *exceptionThrown)
{
	struct winrfImpl  *pSampleRenderableFontData;
    struct rc_data  RenderingContextData;
    HDC             hDC;

	// get a pointer to my implementation data of renderable font.
    pSampleRenderableFontData = winrf2winrfImpl(self);


    // Get a pointer to platform specific RenderingContext data
    // form passed-in rc. For Windows, it has a HDC.
    RenderingContextData = nfrc_GetPlatformData( rc, NULL/*exception*/ ); 
    if( RenderingContextData.majorType != NF_RC_DIRECT )
        return(1);

	// get the passed-in HDC for drawing text, it may have clip info.
    hDC     = (HDC)RenderingContextData.t.directRc.dc ;
	

	if( RenderingContextData.displayer_data == NULL ) {
		SelectObject(hDC, GetStockObject(ANSI_FIXED_FONT));
	} else {
		// put the old font back after drawtext.
		SelectObject( hDC, ( HFONT ) RenderingContextData.displayer_data );
	}
	
	RenderingContextData.displayer_data = NULL;

	return 0;
}
