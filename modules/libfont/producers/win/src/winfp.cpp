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
 * Implements the FontDisplayer
 */

// forward declear for .h files. if not decleared, 16bit compiler
// reports overload nor allowed for _winfp_init() 
struct nffbp;
struct winfp;

#include "Pwinfp.h"
#include "Pwinrf.h"
//#include "Pcstrm.h"		/* for cstrmFactory_Create() */

#include "nf.h"     // for RenderingContextData.type
#include "Mnfrf.h"      // for nfrf_ID
#include "Mwinrf.h"       // for winrfFactory_Create
#include "Mnffbu.h"
#include "Mnff.h"      // for nfSpacingProportional
#include "Mnffmi.h"

#include "Mnffbp.h"
//#include "Mcfb.h"     // in ns\modules\libfont\src\_jmc  

#include "coremem.h"

/****************************************************************************
 *				 Implementation of string-int mapping functions
 *  Those can be shared by others, may be moved to cross platform XP_
 ****************************************************************************/

typedef struct strIntMap_s {
	char * str;
	int    id;
}  strIntMap_t, *pStrIntMap_t;

enum strIntMapFlag {
	strIntMapNoCase = 0x01,
};

// get (int) id
EXTERN_C
BOOL mapStrToInt( pStrIntMap_t pt, const char *ss, int *nn, int flag )
{
	const char *ps;
	BOOL found;

	found = FALSE;
	while( NULL != pt && NULL != (ps = pt->str) ) {
		if( flag & strIntMapNoCase )
			found = (BOOL) (stricmp( ps, ss ) == 0 );
		else
			found = (BOOL) (strcmp( ps, ss ) == 0 ) ;
		
		if( found ) {
			*nn = pt->id;
			return( TRUE );
		}
		// try next 
		pt++;
	}

	return(FALSE);
}

// get string 
EXTERN_C
BOOL mapIntToStr( pStrIntMap_t pt,  int nn, char **ss)
{
	while( NULL != pt && NULL != (*ss = pt->str) ) {
		if( pt->id == nn ) {
			return( TRUE );
		}
		// try next 
		pt++;
	}

	return(FALSE);
}
/****************************************************************************
 *		End of Implementation of string-int mapping functions
 ****************************************************************************/

static strIntMap_t charSetIdMap[] = {
	// from msdev\include\windgi.h
    { "iso-8859-1"        , ANSI_CHARSET },
    { "DEFAULT_CHARSET"     , DEFAULT_CHARSET },    
	{ "adobe-symbol-encoding"      , SYMBOL_CHARSET },    
	{ "Shift_JIS"			, SHIFTJIS_CHARSET },
	{ "EUC-KR"				, HANGEUL_CHARSET },
	{ "big5"				 ,	CHINESEBIG5_CHARSET },
	{ "OEM_CHARSET"         , OEM_CHARSET },

	//	We cannot use xx_CHARSET here because Win16 does not define them
	{ "JOHAB_CHARSET"       , 130 },
	{ "gb2312"				, 134 },		// not defined for 16 bit
	{ "HEBREW_CHARSET"      , 177 },
	{ "ARABIC_CHARSET"      , 178 },
	{ "windows-1253"		, 161 },
	{ "iso-8859-9"			, 162 },
	{ "x-tis620"			, 222 },
	{ "windows-1250"		, 238 },
	{ "windows-1251"		, 204 },
	{ NULL, 0 },							// terminator.
#if 0
	// from msdev\include\windgi.h
    { "ANSI_CHARSET"        , ANSI_CHARSET },
    { "DEFAULT_CHARSET"     , DEFAULT_CHARSET },    
	{ "SYMBOL_CHARSET"      , SYMBOL_CHARSET },    
	{ "SHIFTJIS_CHARSET"    , SHIFTJIS_CHARSET },
	{ "HANGEUL_CHARSET"     , HANGEUL_CHARSET },
	{ "CHINESEBIG5_CHARSET" , CHINESEBIG5_CHARSET },
	{ "OEM_CHARSET"         , OEM_CHARSET },

	//	We cannot use xx_CHARSET here because Win16 does not define them
	{ "JOHAB_CHARSET"       , 130 },
	{ "GB2312_CHARSET"      , 134 },		// not defined for 16 bit
	{ "HEBREW_CHARSET"      , 177 },
	{ "ARABIC_CHARSET"      , 178 },
	{ "GREEK_CHARSET"       , 161 },
	{ "TURKISH_CHARSET"     , 162 },
	{ "THAI_CHARSET"        , 222 },
	{ "EASTEUROPE_CHARSET"  , 238 },
	{ "RUSSIAN_CHARSET"     , 204 },
	{ NULL, 0 },							// terminator.
#endif

	//	default:  return ("DEFAULT_CHARSET");
};


// todo: get rid of this global!!
struct nffbp    *global_pBrokerObj;

EXTERN_C 
struct nffbu *
getFontBrokerUtilityInterface( struct nffbp    *pBrokerObj )
{
    struct nffbu *pBrokerUtilityInterface;
	pBrokerUtilityInterface = (struct nffbu *) nffbp_getInterface( pBrokerObj, &nffbu_ID, NULL);

    return( pBrokerUtilityInterface );
};




/****************************************************************************
 *				 Implementation of common interface methods
 ****************************************************************************/

#ifdef OVERRIDE_winfp_getInterface
#include "Mnffp.h"
#ifdef __cplusplus
extern "C"
#endif
JMC_PUBLIC_API(void*)
_winfp_getInterface(struct winfp* self, jint op, const JMCInterfaceID* iid, JMCException* *exc)
{
	if (memcmp(iid, &nffp_ID, sizeof(JMCInterfaceID)) == 0)
		return winfpImpl2winfp(winfp2winfpImpl(self));
	return _winfp_getBackwardCompatibleInterface(self, iid, exc);
}
#endif /* OVERRIDE_winfp_getInterface */

#ifdef __cplusplus
extern "C"
#endif
JMC_PUBLIC_API(void*)
_winfp_getBackwardCompatibleInterface(struct winfp* self,
									const JMCInterfaceID* iid,
									struct JMCException* *exceptionThrown)
{
	return (NULL);
}

#ifdef __cplusplus
extern "C"
#endif
JMC_PUBLIC_API(void)
_winfp_init(struct winfp* self, struct JMCException* *exceptionThrown,
          struct nffbp *broker)
{
    /* FONTDISPLAYER developers:
	 * This is supposed to do initialization that is required for the 
     * font Displayer object.
     */
    // get a reference to my implementation data.

    struct winfpImpl *pSampleDisplayerData = winfp2winfpImpl(self);

    
    pSampleDisplayerData->m_pBrokerObj   = broker;
    global_pBrokerObj                   = broker;

	/*** The following list need not be maintained at all. - dp
    pSampleDisplayerData->m_pPrimeFontList   = NULL;
	***/

}

#ifdef OVERRIDE_winfp_finalize
#ifdef __cplusplus
extern "C"
#endif
JMC_PUBLIC_API(void)
_winfp_finalize(struct winfp* self, jint op, JMCException* *exception)
{
	/* FONTDISPLAYER developer:
	 * Free any private data for this object here.
	 */

	struct winfpImpl *oimpl = winfp2winfpImpl(self);
	
	/* Finally, free the memory for the object containter. */
	XP_FREEIF(self);
}
#endif /* OVERRIDE_winfp_finalize */

/****************************************************************************
 *				 Implementation of Object specific methods
 *
 * FONTDISPLAYER developers:
 ****************************************************************************/

enum  { 
	MatchMask_faceName	= 0x01,
	MatchMask_italic	= 0x02,
    MatchMask_weight	= 0x04,
    MatchMask_charSet   = 0x08,
	MatchMask_underline = 0x10,
	MatchMask_strikeOut = 0x20,
};

static BOOL matchItalic( LOGFONT *pWanted,	LOGFONT	*pFound)
{
	//if( pFound->lfPitchAndFamily & TMPF_VECTOR )	// 0x02
	//	return( TRUE );

	if( pFound->lfPitchAndFamily & TMPF_TRUETYPE )  // 0x04
		return( TRUE );

	if( pWanted->lfItalic && pFound->lfItalic )
		return( TRUE );

	if( (! pWanted->lfItalic ) && (! pFound->lfItalic) ) 
		return(TRUE);
	
	return(FALSE);
}

typedef struct fontMatch_s {
    int			matchRating;
	int			mask;
	LOGFONT		*pWantedFont;
}  *pFontMatch_t;


#ifdef not_always_return_a_font
Because we don't have closest-matching now, I, the default font displayer,
always return a closest font.

I use Windows font matching mechanism.

When the font broker can do closest-matching, remove this comment-out.
#endif // not_always_return_a_font

#ifdef __cplusplus
extern "C"
#endif
int CALLBACK 
#ifndef XP_WIN32 
_export 
#endif // no _export in windows 32

#ifdef WIN32
sampleEnumFontMatchProc(
    ENUMLOGFONT FAR *lpelf,		// pointer to logical-font data 
    NEWTEXTMETRIC FAR *lpntm,	// pointer to physical-font data 
    int FontType,				// type of font 
    LPARAM lParam 				// address of application-defined data  
    )
#else
sampleEnumFontMatchProc(
	LOGFONT FAR* lpelf,			/* address of structure with logical-font data	*/
	TEXTMETRIC FAR* lpntm,		/* address of structure with physical-font data	*/
	int FontType,				/* type of font	*/
	LPARAM lParam				/* address of application-defined data	*/
    )
#endif
{
    pFontMatch_t	pMatchData;
	LOGFONT			*pWanted;
	LOGFONT			*pFound;
    pMatchData = ( pFontMatch_t ) lParam;

    if( pMatchData == NULL )
        return(0);          //stop enumerating, todo error handling

	pMatchData->matchRating = 0;		// assuming no match 
	pWanted					= pMatchData->pWantedFont;

#ifdef WIN32
	pFound					= &(lpelf->elfLogFont);
#else
    pFound 					= lpelf;
#endif
    
    // no need to check faceName, if it doesn't match EnumFontFamilies()
    // would not call this.
    // some fonts can draw as italic, but not shown in the LOGFONT 

#ifdef match_all_webfont_attributes
	// For Navigator 4.0, 2/28/1997
	// Navigator goes through a font face name list, calls HasFaceName(
	// fontFaceName ) to see the font is avaliable, if not use default
	// font specified in preference.
	// HasFaceName() only asks for the face name. 
	// The face name is there, so the return value of HasFaceName is
	// positive. Navigator stop the font search.
	// Then Navigator asks to create font with all attributes it wants,
	// if is asks for italic, but the given font doesn't has italic,
	// no font is returned to Navigator, and the text will not draw.

	//  "Arial blak" font doesn't have italic returned here.
	// see below for more reasons, not checking them.
    if( pMatchData->mask & MatchMask_italic ) {
		if( ! matchItalic(pWanted, pFound ) ) {
			return(1);		                    // continue enumerating
		// else, fall through
		}
	}

	// When "script" font is the default font in user's  preference,
	// We must return a font for it.
	// "script" font doesn't have underLine returned here,
	// but if use LOGFONT with underLine to create a script font,
	// it will draw with underLine.
	// So, we cannot check underLine here, otherwise, we will
	// miss script-underLine
	// It also happens for script weight.
	// TODO, need to investigate other font, and other attributes.
    if( pMatchData->mask & MatchMask_underline ) {
		if( ! (FontType & TRUETYPE_FONTTYPE ) 
			&& pWanted->lfUnderline != pFound->lfUnderline  ) {
			return(1);		                    // continue enumerating
		// else, fall through
		}
	}

    if( pMatchData->mask & MatchMask_strikeOut ) {
		if( ! ( FontType & TRUETYPE_FONTTYPE ) 
			&& pWanted->lfStrikeOut != pFound->lfStrikeOut  ) {
			return(1);		                    // continue enumerating
		// else, fall through
		}
	}

    if( pMatchData->mask & MatchMask_weight ) {
		if( ! (FontType & TRUETYPE_FONTTYPE ) 
			&& ( pWanted->lfWeight != pFound->lfWeight ) ){
            // match failed
		    return(1);		                    // continue enumerating
		}
	}
#endif	// match_all_webfont_attributes

    if( pMatchData->mask & MatchMask_charSet ) {
		if( pWanted->lfCharSet != pFound->lfCharSet ) {
            // match failed
		    return(1);		                    // continue enumerating
		}
	}

    // reaching this point means we passed all required match!
	pMatchData->matchRating	= 1;				// got it

	// save the returned font attributs.
	// todo close-match flag
	// memcpy( pWanted, pFound, sizeof(LOGFONT) ); 

	return(0);          // no more enumerating
}
// #endif // not_always_return_a_font


static BYTE
convertCharSetToID( char *pStr)
{
	// refer to:
	//          d:\ns\include\csid.h
	//			d:\ns\lib\libi18n\csnametb.c
	//          d:\ns\cmd\winfe\intlwin.cpp(1185):
	//			ns\lib\libi18n\csnamefv.c, int16 INTL_CharSetNameToID(char	*charset)
    //			ns\cmd\winfe\intlwin.cpp,	charsetlist = "iso-8859-1,x-cp1250,x-cp1251,
    //			ns\lib\libi18n\csnamefn.c   int16 INTL_CharSetNameToID(char	*charset)

	// For the round-trip conversion between 
	//	convertCharSetToID() and convertToFmiCharset() 

	int	nn;
	if( FALSE == mapStrToInt( charSetIdMap, pStr, &nn, strIntMapNoCase /*flag*/ ) ) {
		return( DEFAULT_CHARSET );
	}
	return( (BYTE)nn );

}

EXTERN_C
static char *
convertToFmiCharset( LOGFONT *pLogFont )
{
	char * pStr;
	if( FALSE == mapIntToStr(charSetIdMap, pLogFont->lfCharSet, &pStr) ) {
		pStr = "DEFAULT_CHARSET";
	}

	return( pStr );
}


#ifdef __cplusplus
extern "C"
#endif
JMC_PUBLIC_API(void *)
_winfp_LookupFont(struct winfp* self, jint op, struct nfrc* rc,
				struct nffmi* pFmi, const char *accessor,
				struct JMCException* *exceptionThrown)
{
    struct winfpImpl		*pSampleDisplayerData;
    struct rc_data		RenderingContextData;
    char				*pFMIfontName;
	char                *pStr;
    struct fontMatch_s	matchData;
    LONG                nTemp;
    pPrimeFont_t        pPrmFont;
    LOGFONT             tempLogFont;
	
	int					nEncode			= encode_notUnicode;	
	int					csID			= DEFAULT_CHARSET;
    int					wanted_Weight	= FW_DONTCARE;

	// get a reference to my implementation data.
    pSampleDisplayerData     = winfp2winfpImpl(self);

    // Get platform specific RenderingContext data.
    // For Windows, it is a HDC.
    RenderingContextData = nfrc_GetPlatformData( rc, NULL/*exception*/ ); 

    if( RenderingContextData.majorType != NF_RC_DIRECT )
        return(NULL);

	// prepare the structure for sampleEnumFontMatchProc
	matchData.matchRating   = 0;
    matchData.mask          = 0;
    matchData.pWantedFont   = &tempLogFont;		// pass in the wanted, and pass back the found.

	// get asked font attributes from nffmi , and fill in LOGFONT structure.
    // fmi attribute names are defined in nf.h
	memset( &tempLogFont, 0, sizeof( LOGFONT) );

    pFMIfontName               = (char *) nffmi_GetValue(pFmi, nfFmiName, NULL );
	if( pFMIfontName )
		strncpy(tempLogFont.lfFaceName, pFMIfontName, LF_FACESIZE );
    // no need to match faceName, it is handled by EnumFontFamilies()
    // matchData.mask          |= MatchMask_faceName;

	// encoding is not a LOGFONT attribute on Windows, so it is not part of matching.
	// It is used at draw time to decide using ::DrawText() or ::DrawTextW()
    pStr = (char *)nffmi_GetValue(pFmi, nfFmiEncoding, NULL );
    if( pStr && strcmp(pStr, "Unicode") == 0 ) {
		nEncode	 |=  encode_unicode;  //todo 
    }

    nTemp = (int) nffmi_GetValue(pFmi, nfFmiStyle, NULL );
    if( nTemp == nfStyleItalic ) {
        tempLogFont.lfItalic  = 1;
        matchData.mask        |= MatchMask_italic ;
    }

	// Because win95 unicode cannot draw underline correctly, 
	// the font displayer ignore the attribute, and draw a line under text.
	// For a quick fix, we don't care it is NT or 95.
	// remove this hack when win95 bug is fixed.

	// use underline when it is not uncode.
	nTemp = (int) nffmi_GetValue(pFmi, nfFmiUnderline, NULL );
	if( nTemp != nfUnderlineDontCare ) {
		if( nTemp == nfUnderlineYes) {
			if( nEncode & encode_unicode ) {
				nEncode |= encode_needDrawUnderline;
			} else {
				tempLogFont.lfUnderline  =  1 ;
				matchData.mask        |= MatchMask_underline ;
			}
		}
	}


    nTemp = (int) nffmi_GetValue(pFmi, nfFmiStrikeOut, NULL );
    if( nTemp != nfStrikeOutDontCare ) {
        tempLogFont.lfStrikeOut  = (nTemp == nfStrikeOutYes) ? 1 : 0;
        matchData.mask        |= MatchMask_strikeOut ;
    }

    wanted_Weight = (int) nffmi_GetValue(pFmi, nfFmiWeight, NULL );
    if( wanted_Weight > nfWeightDontCare ) {
        tempLogFont.lfWeight  = wanted_Weight;
        matchData.mask        |= MatchMask_weight ;
    }

	tempLogFont.lfCharSet  = DEFAULT_CHARSET ;
#ifdef WIN32			// DP_TRYING_WIN16_JAPANESE
    pStr = (char *) nffmi_GetValue(pFmi, nfFmiCharset, NULL );
    if( pStr != NULL ) {
        csID	= convertCharSetToID( pStr);
        tempLogFont.lfCharSet    =  csID;
		//todo matchData.mask                      |= MatchMask_charSet ;
    }
#endif

#ifdef not_always_return_a_font
Because we don't have closest-matching now, I, the default font displayer,
always return a closest font.

I use Windows font matching mechanism.

When the font broker can do closest-matching, remove this comment-out.
I will return null if asked font are not found.
#endif	// not_always_return_a_font

#ifdef WIN32
    ::EnumFontFamilies( 
		(HDC) RenderingContextData.t.directRc.dc,
		(LPCTSTR)	pFMIfontName, 
		(FONTENUMPROC) sampleEnumFontMatchProc, 
		(LPARAM) &matchData
		);
#else
    ::EnumFontFamilies( 
		(HDC) RenderingContextData.t.directRc.dc,
		(LPCSTR) pFMIfontName, 
		(FONTENUMPROC) sampleEnumFontMatchProc, 
		(char FAR *)&matchData
		);
#endif

	if( matchData.matchRating == 0 ) {	
		// failed to match the asked font.
		return(NULL);   // return NULL means we don't support the font asked in pFmi.
	}
// #endif	// not_always_return_a_font

    // we have the font in tempLogFont, matched or not.
	// save it and pass the pointer to caller.

    // A font struct is created here, and return the a fontHandle to 
    // Font broker.
    // Font broker doesn't touch the handle, but will pass me(Displayer)
    // the handle to create a renderable font.
    // release font object/struct in _winfp_ReleaseFontHandle

    pPrmFont = (pPrimeFont_t) malloc ( sizeof( struct NetscapePrimeFont_s ) );
    
    pPrmFont->csIDInPrimeFont		= csID;
	pPrmFont->encordingInPrimeFont	= nEncode;  
	nTemp                           = (int) nffmi_GetValue(pFmi, nfFmiResolutionY, NULL );
	pPrmFont->YPixelPerInch        = nTemp > 0 ? nTemp : 96 ;    // 96 is the best guest 

    // save the font info
    memcpy( &(pPrmFont->logFontInPrimeFont), &tempLogFont, sizeof(LOGFONT) ); 
    
	// todo best-match flag
	// pPrmFont->logFontInPrimeFont.lfWeight = wanted_Weight;

	/*** The following list need not be maintained at all. - dp
    // link it (for release memory in _winfp_ReleaseFontHandle )
    pPrmFont->nextFont = pSampleDisplayerData->m_pPrimeFontList;
    pSampleDisplayerData->m_pPrimeFontList = pPrmFont;
	***/

    return( pPrmFont );
}

#ifdef __cplusplus
extern "C"
#endif
JMC_PUBLIC_API(struct nfstrm *)
_winfp_CreateFontStreamHandler(struct winfp* self, jint op, struct nfrc *rc,
							   const char *urlOfPage,
							   struct JMCException* *exceptionThrown)
{
	struct winfpImpl *pSampleDisplayerData = winfp2winfpImpl(self);
	/* XXX Need to pass the fontname and url here */
	//struct nfstrm *strm = (struct nfstrm *)cstrmFactory_Create(NULL, rc, NULL, NULL);
	//return strm;
	return NULL;
}


#ifdef __cplusplus
extern "C"
#endif
JMC_PUBLIC_API(void *)
_winfp_CreateFontFromFile(struct winfp* self, jint op,
						  struct nfrc *rc, const char *mimetype,
						  const char *fontfilename,
						  const char *urlOfPage,
						  struct JMCException* *exceptionThrown)
{
	// This is required only for Displayers that do webfonts
	return (NULL);
}


#ifdef __cplusplus
extern "C"
#endif
JMC_PUBLIC_API(jint)    
_winfp_ReleaseFontHandle(struct winfp* self, jint op, void *fh,
					   struct JMCException* *exceptionThrown)
{
	struct winfpImpl *pSampleDisplayerData = winfp2winfpImpl(self);

	if (fh)
		free(fh);

	/*** The following list need not be maintained at all. - dp
    // free the font list
    pPrimeFont_t        pPrmFont, pPrmFont2;
    pPrmFont = pPrmFont2 = pSampleDisplayerData->m_pPrimeFontList;
    while( pPrmFont != NULL ) {
        pPrmFont2 = pPrmFont2->nextFont;
        free( pPrmFont );
        pPrmFont = pPrmFont2;
    }
    pSampleDisplayerData->m_pPrimeFontList = NULL;
	***/

    return (0);
}

// assuming a font can have 50 sizes
#define MAXSIZECOUNT    50
typedef struct fontSize_s {
    int  sizeTable[ MAXSIZECOUNT +1 ];   // +1 for terminator
    int     sizeCount;
}  *pFontSize_t;

#ifdef __cplusplus
extern "C"
#endif
int CALLBACK 
#ifndef WIN32 
_export 
#endif // no _export in windows 32

#ifdef WIN32
sampleEnumFontSizeProc(
    ENUMLOGFONT FAR *lpelf,		// pointer to logical-font data 
    NEWTEXTMETRIC FAR *lpntm,	// pointer to physical-font data 
    int FontType,				// type of font 
    LPARAM lParam 				// address of application-defined data  
    )
#else
sampleEnumFontSizeProc(
	LOGFONT FAR* lpelf,			/* address of structure with logical-font data	*/
	TEXTMETRIC FAR* lpntm,		/* address of structure with physical-font data	*/
	int FontType,				/* type of font	*/
	LPARAM lParam				/* address of application-defined data	*/
    )
#endif
{
    pFontSize_t  pfs;
    pfs = ( pFontSize_t ) lParam;
    if( pfs == NULL )
        return(0);          //stop enumerating

	LOGFONT			*pFound;
#ifdef WIN32
	pFound					= &(lpelf->elfLogFont);
#else
    pFound 					= lpelf;
#endif

    if( pfs->sizeCount >= MAXSIZECOUNT )
        return(0);          //stop enumerating

	// todo, convert to pointsize !!
    // save the size
    pfs->sizeTable[ pfs->sizeCount++ ] = pFound->lfHeight;

    return(1);          // continue enumerating
}


#ifdef __cplusplus
extern "C"
#endif
JMC_PUBLIC_API(void*)
_winfp_EnumerateSizes(struct winfp* self, jint op, struct nfrc* rc, void * fh,
					struct JMCException* *exceptionThrown)
{
	struct winfpImpl  *pSampleDisplayerData;
    HDC             hDC;
    char            *faceName = NULL;
    pPrimeFont_t    pPrmFont;
	//todo re fefine the struct to use malloc space.
    static struct fontSize_s fontSizeTable;    //todo malloc space?
	jdouble			*newtable = NULL;			   // continuing the above hack...
	int				ii;
    struct rc_data	RenderingContextData;

#ifdef try_broker
    RenderingContextData = nfrc_GetPlatformData( rc, NULL/*exception*/ ); 
    if( RenderingContextData.majorType != NF_RC_DIRECT )
        return(NULL);

	hDC = (HDC) RenderingContextData.t.directRc.dc;

    pSampleDisplayerData = winfp2winfpImpl(self);
    pPrmFont             = (pPrimeFont_t) fh;
    if( pPrmFont == NULL )
        return(NULL);

    fontSizeTable.sizeCount = 0;
    faceName    = pPrmFont->logFontInPrimeFont.lfFaceName;

    if( faceName != NULL ) {
        // get all sizes the font supports


#ifdef WIN32
    ::EnumFontFamilies( 
		hDC,
		(LPCTSTR)	faceName, 
		(FONTENUMPROC) sampleEnumFontSizeProc, 
		(LPARAM) &fontSizeTable
		);
#else
    ::EnumFontFamilies( 
		hDC,
		(LPCSTR) faceName, 
		(FONTENUMPROC) sampleEnumFontSizeProc, 
		(char FAR *)&fontSizeTable
		);
#endif
	}
	
    fontSizeTable.sizeTable[ fontSizeTable.sizeCount ] = -1; // terminator of the array.
    
    if( fontSizeTable.sizeCount < 1 )
    	return(NULL);
    	
	// Allocate the sizeTable using nffbu::malloc() and return it to the caller.
	// Caller is expected to free this.
	newtable = (jdouble *)
		nffbu_malloc(getFontBrokerUtilityInterface(global_pBrokerObj),
					sizeof(*newtable)*(fontSizeTable.sizeCount+1), NULL);
	if (!newtable)
		return( NULL );
	for (ii=0; ii<=fontSizeTable.sizeCount; ii++)
		newtable[ii] = fontSizeTable.sizeTable[ii];

    // return non-NULL means we support sizes in sizeTable returned.
	return( newtable );
#endif	// try_broker

	/*
	newtable = (jdouble *)
		nffbu_malloc(getFontBrokerUtilityInterface(global_pBrokerObj),
					sizeof(*newtable)*(2), NULL);
	if (!newtable)
		return( NULL );

	newtable[0] = 10.0;
	newtable[1] = -1;
	return(	newtable );		
	*/
	return(NULL);

}


#ifdef __cplusplus
extern "C"
#endif
JMC_PUBLIC_API(struct nffmi*)
_winfp_GetMatchInfo(struct winfp* self, jint op, void * fh,
				  struct JMCException* *exceptionThrown)
{
	struct winfpImpl *pSampleDisplayerData = winfp2winfpImpl(self);
    pPrimeFont_t   pPrmFont;

    pPrmFont  = ( pPrimeFont_t ) fh;
    if( pPrmFont == NULL )
        return(NULL);

	return NULL;
}


#ifdef __cplusplus
extern "C"
#endif
JMC_PUBLIC_API(struct nfrf*)
_winfp_GetRenderableFont(struct winfp* self, jint op, struct nfrc* rc, void *fh, 
					   jdouble inputPointSize, struct JMCException* *exceptionThrown)
{
	struct winfpImpl      *pSampleDisplayerData;
    struct winrf          *pSampleRenderableFont;
    struct winrfImpl      *pSampleRFdata ;
    struct nfrf         *pSampleRenderableFontInterface;
    pPrimeFont_t        pPrmFont;
	HDC					hDC;
    struct rc_data		RenderingContextData;
	jdouble				pointSize = inputPointSize;
    int					pixelSize = 0;
	HFONT           hFont;
	HFONT           hOldFont;
	TEXTMETRIC      textMetric;
	int				reCode;

    RenderingContextData = nfrc_GetPlatformData( rc, NULL/*exception*/ ); 
    if( RenderingContextData.majorType != NF_RC_DIRECT )
        return(NULL);

	hDC = (HDC) RenderingContextData.t.directRc.dc;

    // get a pointor to my implement data of FontDisplayer.
    pSampleDisplayerData = winfp2winfpImpl(self);

    pPrmFont             = ( pPrimeFont_t ) fh;
    if( pPrmFont == NULL )
        return(NULL);

    //TODO hard code font
    // create a renderable font
    pSampleRenderableFont = winrfFactory_Create(NULL);

    if (! pSampleRenderableFont ) {
        return( NULL );
	}

    // get a pointor to my implement data in renderable font
    pSampleRFdata = winrf2winrfImpl(pSampleRenderableFont);

    // create font, need to call DeleteObject for pSampleRFdata->m_hFontInRF
	// in _winrf_finalize()

	// make sure pointsize is positive	
	if( pointSize < 0 ) {
		pointSize = - pointSize;	// webfont use positive size. see winfe\nsfont.cpp
	}
	// pointsize = -MulDiv(pointsize,::GetDeviceCaps(hDC, LOGPIXELSY), 72);

	// for passing to Windows, convert pointsize to pixel size and negative.
	if( pointSize > 0 )
		pixelSize = (int) - pointSize * pPrmFont->YPixelPerInch / 72.0;

	POINT size;
	size.x = 0;
	size.y = pixelSize;
	::DPtoLP(hDC, &size, 1);

	pPrmFont->logFontInPrimeFont.lfHeight = (int) size.y;
	pPrmFont->logFontInPrimeFont.lfWidth  = 0;
    pSampleRFdata->mRF_hFont = CreateFontIndirect( & pPrmFont->logFontInPrimeFont );    

	pSampleRFdata->mRF_csID		= pPrmFont->csIDInPrimeFont;
	pSampleRFdata->mRF_encoding = pPrmFont->encordingInPrimeFont;

	// cache attributes now, when consumer asking a sttribute, no rc
	// is passed in.
		// HDC             hDC;

		hFont	= pSampleRFdata->mRF_hFont;

		hOldFont = (HFONT)SelectObject( hDC, hFont);

		reCode = GetTextMetrics( hDC, &textMetric );

		// put the old font back after drawtext.
		SelectObject( hDC, hOldFont);

		if( reCode )  {   // got it
			pSampleRFdata->mRF_tmDescent		= textMetric.tmDescent;
			pSampleRFdata->mRF_tmMaxCharWidth	= textMetric.tmMaxCharWidth;
			pSampleRFdata->mRF_tmAscent			= textMetric.tmAscent;
			pSampleRFdata->mRF_pointSize		= inputPointSize;
		}
	
    // get the renderable font interace for return value.
    pSampleRenderableFontInterface = (struct nfrf*)winrf_getInterface( pSampleRenderableFont, &nfrf_ID, NULL );

    // return the renderable font interface.
    // return NULL if we cannot create the renderable font.
    return( pSampleRenderableFontInterface );     
}


#ifdef __cplusplus
extern "C"
#endif
JMC_PUBLIC_API(const char*)
_winfp_Name(struct winfp* self, jint op, struct JMCException* *exceptionThrown)
{
	struct winfpImpl *pSampleDisplayerData = winfp2winfpImpl(self);
	return (NF_NATIVE_FONT_DISPLAYER);
}


#ifdef __cplusplus
extern "C"
#endif
JMC_PUBLIC_API(const char*)
_winfp_Description(struct winfp* self, jint op,
				 struct JMCException* *exceptionThrown)
{
	struct winfpImpl *pSampleDisplayerData = winfp2winfpImpl(self);
    //TODO
	return NULL;
}


int
convertToFmiPitch( LOGFONT *pLogFont )
{
    if( pLogFont->lfPitchAndFamily == FIXED_PITCH )
        return( nfSpacingMonospaced );

    if( pLogFont->lfPitchAndFamily == VARIABLE_PITCH )
        return( nfSpacingProportional );

    return( nfSpacingDontCare );
}

EXTERN_C struct nffmi*
createFMIfromLOGFONT( LOGFONT *pLogFont )
{

    nffmi*  pFmi;
    char    *FmiName;
    char    *FmiCharset;
    char    *FmiEncoding;
    int     FmiWeight;
    int     FmiPitch;
    int     FmiStyle;
	int		FmiUnderline;
	int		FmiStrikeOut;
   //int     *nfFmiPanose"           int[]
    int     FmiPixelPerInchX;
    int     FmiPixelPerInchY;

    FmiName         = pLogFont->lfFaceName;
    FmiCharset      = convertToFmiCharset( pLogFont );
    FmiEncoding     = "1";				// only "unicode" is used for select TextOuuW
    FmiWeight       = pLogFont->lfWeight;
    FmiPitch        = convertToFmiPitch( pLogFont );
    FmiStyle        = (pLogFont->lfItalic )? nfStyleItalic : nfStyleDontCare ;
	FmiUnderline	= (pLogFont->lfUnderline) ? nfUnderlineYes : nfUnderlineDontCare ;
	FmiStrikeOut	= (pLogFont->lfStrikeOut) ? nfStrikeOutYes : nfStrikeOutDontCare ;
    FmiPixelPerInchX  = 0;
    FmiPixelPerInchY  = 0;

    pFmi = nffbu_CreateFontMatchInfo( 
		getFontBrokerUtilityInterface( global_pBrokerObj ),
        FmiName,
        FmiCharset,
        FmiEncoding,
        FmiWeight,
		FmiPitch,
        FmiStyle,
		FmiUnderline,
		FmiStrikeOut,
		FmiPixelPerInchX,                          
        FmiPixelPerInchY,                          
		NULL /* exception */
		);

    return( pFmi );        
}

// todo assuming a font can have 50 
#define MAXfmiCOUNT    80
typedef struct fmiList_s {
    struct nffmi    **fmiTable;
    int             fmiCount;
}  *pFmiList_t;


#ifdef __cplusplus
extern "C"
#endif
int CALLBACK 
#ifndef WIN32 
_export 
#endif // no _export in windows 32

#ifdef WIN32
sampleEnumFontListFmiProc(
    ENUMLOGFONT FAR *lpelf,		// pointer to logical-font data 
    NEWTEXTMETRIC FAR *lpntm,	// pointer to physical-font data 
    int FontType,				// type of font 
    LPARAM lParam 				// address of application-defined data  
    )
#else
sampleEnumFontListFmiProc(
	LOGFONT FAR* lpelf,			/* address of structure with logical-font data	*/
	TEXTMETRIC FAR* lpntm,		/* address of structure with physical-font data	*/
	int FontType,				/* type of font	*/
	LPARAM lParam				/* address of application-defined data	*/
    )
#endif
{
	LOGFONT			*pFound;
#ifdef WIN32
	pFound					= &(lpelf->elfLogFont);
#else
    pFound 					= lpelf;
#endif

    pFmiList_t  pfmiList;
    pfmiList = ( pFmiList_t ) lParam;
    if( pfmiList == NULL )
        return(0);          //stop enumerating

    if( pfmiList->fmiCount >= MAXfmiCOUNT )
        return(0);          //stop enumerating

    // (lpelf->elfLogFont).lfHeight;
    pfmiList->fmiTable[ pfmiList->fmiCount++ ] = createFMIfromLOGFONT( pFound );

    return(1);          // continue enumerating
}

#ifdef __cplusplus
extern "C"
#endif
JMC_PUBLIC_API(void*)
_winfp_ListFonts(struct winfp* self, jint op,
					struct nfrc* rc, struct nffmi* pFmi,
					struct JMCException* *exceptionThrown)
{
	struct winfpImpl      *pSampleDisplayerData = winfp2winfpImpl(self);
    struct rc_data		RenderingContextData;
    struct fmiList_s    fmiList;

    RenderingContextData = nfrc_GetPlatformData( rc, NULL/*exception*/ ); 
    if( RenderingContextData.majorType != NF_RC_DIRECT )
        return(NULL);

    fmiList.fmiTable = (struct nffmi **)   nffbu_malloc( 
		            getFontBrokerUtilityInterface(pSampleDisplayerData->m_pBrokerObj),
                    (MAXfmiCOUNT+1) * sizeof(struct nffmi *), NULL);
    fmiList.fmiCount = 0;

#ifdef WIN32
    ::EnumFontFamilies( 
		(HDC) RenderingContextData.t.directRc.dc,
		NULL, 
		(FONTENUMPROC) sampleEnumFontListFmiProc, 
		(LPARAM) &fmiList
		);
#else
    ::EnumFontFamilies( 
		(HDC) RenderingContextData.t.directRc.dc,
		NULL, 
		(FONTENUMPROC) sampleEnumFontListFmiProc, 
		(char FAR *)&fmiList
		);
#endif

    fmiList.fmiTable[ fmiList.fmiCount ] = NULL;    // terminate
    return( fmiList.fmiTable );     // caller free the memory
}


#ifdef __cplusplus
extern "C"
#endif
JMC_PUBLIC_API(void*)
_winfp_ListSizes(struct winfp* self, jint op, struct nfrc* rc, struct nffmi* pFmi,
			   struct JMCException* *exceptionThrown)
{
	struct winfpImpl *pSampleDisplayerData = winfp2winfpImpl(self);
    //TODO
	return NULL;
}


#ifdef __cplusplus
extern "C"
#endif
JMC_PUBLIC_API(const char*)
_winfp_EnumerateMimeTypes(struct winfp* self, jint op,
						struct JMCException* *exceptionThrown)
{
	struct winfpImpl *pSampleDisplayerData = winfp2winfpImpl(self);
	char *mimetypes = NULL;
    mimetypes = "Application/TestFontStream:dp:Sample Displayer testing font streaming;";
	return mimetypes;
}


#ifdef __cplusplus
extern "C"
#endif
JMC_PUBLIC_API(jint)
_winfp_CacheFontInfo(struct winfp* self, jint op,
				   struct JMCException* *exceptionThrown)
{
	struct winfpImpl *pSampleDisplayerData = winfp2winfpImpl(self);
    //TODO
	return(0);  // return NULL;
}
