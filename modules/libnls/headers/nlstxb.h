/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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


#ifndef _NLSTXB_H
#define _NLSTXB_H


#include "nlsxp.h"
#include "nlsuni.h"
#include "nlsloc.h"

#ifdef NLS_CPLUSPLUS
#include "txtbdry.h"
#endif


NLS_BEGIN_PROTOS

/******************** TextBoundary Data Types ************************/

#ifndef NLS_CPLUSPLUS
typedef struct _TextBoundary TextBoundary;
#endif

enum _NLS_TextBoundaryDone {
	NLS_TextBoundaryDone = (TextOffset)-1L
};

/******************** TextBoundary Constructor functions ************************/

/**
 * Create a Character Break Text Boundry for the given locale
 */
NLSBRKAPI_PUBLIC(NLS_ErrorCode) 
NLS_NewCharacterTextBoundary(TextBoundary ** result,
								const Locale* locale);

/**
 * Create a Word break text boundry for the given locale.
 */
NLSBRKAPI_PUBLIC(NLS_ErrorCode) 
NLS_NewWordTextBoundary(TextBoundary ** result,
								const Locale* locale);

/**
 * Create a Line Break Text Boundry for the given locale
 */
NLSBRKAPI_PUBLIC(NLS_ErrorCode) 
NLS_NewLineTextBoundary(TextBoundary ** result,
								const Locale* locale);

/**
 * Create a Sentence Break Text Boundry for the given locale
 */
NLSBRKAPI_PUBLIC(NLS_ErrorCode) 
NLS_NewSentenceTextBoundary(TextBoundary ** result,
								const Locale* locale);

/********************* TextBoundary Destructor functions *******************************/

/* Destructor for an TextBoundary */
NLSBRKAPI_PUBLIC(NLS_ErrorCode)
NLS_DeleteTextBoundary(TextBoundary * that);

/******************** TextBoundary API ************************/

/**
 * Set the text for the text boundry object. Replaces any text
 * that may be set, and resets the state of the text boundry
 */
NLSBRKAPI_PUBLIC(NLS_ErrorCode)
NLS_TextBoundarySetText(TextBoundary * that,
								const UnicodeString* text);

/**
 * Gets the text that is currently set on the text boundry.
 */
NLSBRKAPI_PUBLIC(const UnicodeString*)
NLS_TextBoundaryGetText(TextBoundary * that);

/**
 * Gets the offset of the first boundary. Moves the current break 
 * state to the new offset. Will return the offset of the text boundary
 * or NLS_TextBoundryDone if there are no more boundry positions.
 */
NLSBRKAPI_PUBLIC(TextOffset)
NLS_TextBoundaryFirst(TextBoundary * that);

/**
 * Gets the offset of the Last boundary. Moves the current break 
 * state to the new offset. Will return the offset of the text boundary
 * or NLS_TextBoundryDone if there are no more boundry positions.
 */
NLSBRKAPI_PUBLIC(TextOffset)
NLS_TextBoundaryLast(TextBoundary * that);

/**
 * Gets the offset of the Next boundary. Moves the current break 
 * state to the new offset. Will return the offset of the text boundary
 * or NLS_TextBoundryDone if there are no more boundry positions.
 */
NLSBRKAPI_PUBLIC(TextOffset)
NLS_TextBoundaryNext(TextBoundary * that);

/**
 * Gets the offset of the Previous boundary. Moves the current break 
 * state to the new offset. Will return the offset of the text boundary
 * or NLS_TextBoundryDone if there are no more boundry positions.
 */
NLSBRKAPI_PUBLIC(TextOffset)
NLS_TextBoundaryPrevious(TextBoundary * that);

/**
 * Gets the offset of the Current boundary. Moves the current break 
 * state to the new offset. Will return the offset of the text boundary
 * or NLS_TextBoundryDone if there are no more boundry positions.
 */
NLSBRKAPI_PUBLIC(TextOffset)
NLS_TextBoundaryCurrent(TextBoundary * that);

/**
 * Gets the offset of the Next boundary after the TextOffset specified. 
 * Moves the current break state to the new offset. Will return the offset 
 * of the text boundary or NLS_TextBoundryDone if there are no more 
 * boundry positions.
 */
NLSBRKAPI_PUBLIC(TextOffset)
NLS_TextBoundaryNextAfter(TextBoundary * that,
						  TextOffset offset);

/******************** TextBoundary Locale API ************************/

/**
 * Get the count of the set of Locales are installed
 */
NLSBRKAPI_PUBLIC(size_t)
NLS_TextBoundaryCountAvailableLocales();

/**
 * Get the set of Locales for which, the array must be 
 * allocated by the caller, and count must specify how many entries exist.
 */
NLSBRKAPI_PUBLIC(NLS_ErrorCode)
NLS_TextBoundaryGetAvailableLocales(Locale * LocaleArray, 
                    size_t inCount,
					size_t *count);

/**
 * Getter for display of the entire locale to user.
 * If the localized name is not found, uses the ISO codes.
 * The default locale is used for the presentation language.
 */
NLSBRKAPI_PUBLIC(NLS_ErrorCode)
NLS_TextBoundaryGetDisplayName(UnicodeString * name);

/**
 * Getter for display of the entire locale to user.
 * If the localized name is not found, uses the ISO codes
 * @param inLocale specifies the desired user language.
 */
NLSBRKAPI_PUBLIC(NLS_ErrorCode)
NLS_TextBoundaryGetDisplayNameInLocale(const Locale * locale, 
        const Locale * inLocale, UnicodeString * name);

/************************ End ******************************/
NLS_END_PROTOS
#endif /* _NLSTXB_H */

