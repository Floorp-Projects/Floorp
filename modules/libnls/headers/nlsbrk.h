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

#ifndef _NLSBRK_H
#define _NLSBRK_H


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

/** NLS_NewCharacterTextBoundary
 *
 * Create a Character Break Text Boundry for the given locale. Locale
 * may be NULL, at which point the default locale will be used.
 *
 * Status Returns
 *		NLS_SUCCESS
 *		NLS_NEW_TEXT_BOUNDARY_FAILED
 */
NLSBRKAPI_PUBLIC(NLS_ErrorCode)
NLS_NewCharacterTextBoundary(TextBoundary ** result,
								const Locale* locale);

/** NLS_NewWordTextBoundary
 *
 * Create a Word break text boundry for the given locale.  Locale
 * may be NULL, at which point the default locale will be used.
 *
 * Status Returns
 *		NLS_SUCCESS
 *		NLS_NEW_TEXT_BOUNDARY_FAILED
 */
NLSBRKAPI_PUBLIC(NLS_ErrorCode) 
NLS_NewWordTextBoundary(TextBoundary ** result,
								const Locale* locale);

/** NLS_NewLineTextBoundary
 *
 * Create a Line Break Text Boundry for the given locale.  Locale
 * may be NULL, at which point the default locale will be used.
 *
 * Status Returns
 *		NLS_SUCCESS
 *		NLS_NEW_TEXT_BOUNDARY_FAILED
 */
NLSBRKAPI_PUBLIC(NLS_ErrorCode)
NLS_NewLineTextBoundary(TextBoundary ** result,
								const Locale* locale);

/** NLS_NewSentenceTextBoundary
 *
 * Create a Sentence Break Text Boundry for the given locale.  Locale
 * may be NULL, at which point the default locale will be used.
 *
 * Status Returns
 *		NLS_SUCCESS
 *		NLS_NEW_TEXT_BOUNDARY_FAILED
 */
NLSBRKAPI_PUBLIC(NLS_ErrorCode) 
NLS_NewSentenceTextBoundary(TextBoundary ** result,
								const Locale* locale);

/********************* TextBoundary Destructor functions *******************************/

/* NLS_DeleteTextBoundary
 *
 * Destructor for an TextBoundary 
 *
 * Status Returns
 *		NLS_SUCCESS
 *		NLS_PARAM_ERROR
 */
NLSBRKAPI_PUBLIC(NLS_ErrorCode)
NLS_DeleteTextBoundary(TextBoundary * that);

/******************** TextBoundary API ************************/

/** NLS_TextBoundarySetText
 *
 * Set the text for the text boundry object. Replaces any text
 * that may be set, and resets the state of the text boundry
 *
 * Status Returns
 *		NLS_SUCCESS
 *		NLS_PARAM_ERROR
 */
NLSBRKAPI_PUBLIC(NLS_ErrorCode)
NLS_TextBoundarySetText(TextBoundary * that,
								const UnicodeString* text);

/** NLS_TextBoundarySetTextUniChar
 * Set the text for the text boundry object. Replaces any text
 * that may be set, and resets the state of the text boundry
 *
 * Status Returns
 *		NLS_SUCCESS
 *		NLS_PARAM_ERROR
 *		NLS_NEW_UNICODESTRING_FAILED
*/
NLSBRKAPI_PUBLIC(NLS_ErrorCode)
NLS_TextBoundarySetTextUniChar(TextBoundary * that,
								const UniChar* text,
								size_t length);


/** NLS_TextBoundaryGetText
 *
 * Gets the text that is currently set on the text boundry.
 * May return NULL in the case of an error.
 *
 */
NLSBRKAPI_PUBLIC(const UnicodeString*)
NLS_TextBoundaryGetText(TextBoundary * that);

/** NLS_TextBoundaryGetTextUniChar
 *
 * Gets the text that is currently set on the text boundry.
 * May return NULL in the case of an error.
*
 */
NLSBRKAPI_PUBLIC(const UniChar*)
NLS_TextBoundaryGetTextUniChar(TextBoundary * that);

/** NLS_TextBoundaryFirst
 *
 * Gets the offset of the first boundary. Moves the current break 
 * state to the new offset. Will return the offset of the text boundary
 * or NLS_TextBoundryDone if there are no more boundry positions.
 */
NLSBRKAPI_PUBLIC(TextOffset)
NLS_TextBoundaryFirst(TextBoundary * that);

/** NLS_TextBoundaryLast
 *
 * Gets the offset of the Last boundary. Moves the current break 
 * state to the new offset. Will return the offset of the text boundary
 * or NLS_TextBoundryDone if there are no more boundry positions.
 */
NLSBRKAPI_PUBLIC(TextOffset)
NLS_TextBoundaryLast(TextBoundary * that);

/** NLS_TextBoundaryNext
 *
 * Gets the offset of the Next boundary. Moves the current break 
 * state to the new offset. Will return the offset of the text boundary
 * or NLS_TextBoundryDone if there are no more boundry positions.
 */
NLSBRKAPI_PUBLIC(TextOffset)
NLS_TextBoundaryNext(TextBoundary * that);

/** NLS_TextBoundryDone
 *
 * Gets the offset of the Previous boundary. Moves the current break 
 * state to the new offset. Will return the offset of the text boundary
 * or NLS_TextBoundryDone if there are no more boundry positions.
 */
NLSBRKAPI_PUBLIC(TextOffset)
NLS_TextBoundaryPrevious(TextBoundary * that);

/** NLS_TextBoundaryCurrent
 *
 * Gets the offset of the Current boundary. Moves the current break 
 * state to the new offset. Will return the offset of the text boundary
 * or NLS_TextBoundryDone if there are no more boundry positions.
 */
NLSBRKAPI_PUBLIC(TextOffset)
NLS_TextBoundaryCurrent(TextBoundary * that);

/** NLS_TextBoundaryNextAfter
 *
 * Gets the offset of the Next boundary after the TextOffset specified. 
 * Moves the current break state to the new offset. Will return the offset 
 * of the text boundary or NLS_TextBoundryDone if there are no more 
 * boundry positions.
 */
NLSBRKAPI_PUBLIC(TextOffset)
NLS_TextBoundaryNextAfter(TextBoundary * that,
						  TextOffset offset);


/************************ End ******************************/

NLS_END_PROTOS
#endif /* _NLSBRK_H */

