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

