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


#ifndef _NLSMSG_H
#define _NLSMSG_H


#include "nlsxp.h"
#include "nlsuni.h"
#include "nlsfmt.h"
#include "nlsloc.h"
#include "nlscal.h"

#ifdef NLS_CPLUSPLUS
#include "fmtable.h"
#include "choicfmt.h"
#include "msgfmt.h"
#endif


NLS_BEGIN_PROTOS

/******************** Formatting Data Types ************************/

#ifndef NLS_CPLUSPLUS
typedef struct _MessageFormat			MessageFormat;
#endif

/******************** Simple Formatting functions ************************/

/* NLS_FormatMessage
 *
 * varg simple message formatting. 
 * varg's are value pairs composed of NLS_FormattableType followed by 
 * argument. The last parameter to the routine should be -1 to indicate the
 * end of the parameters.
 * eg:
 *	
 * err = NLS_FormatMessage(result, 
 *			"There {0,choice,0#are no files|1#is one file|1<are {0,number} files}.", 
 *			NLS_TYPE_STRING, "A Disk", 
 *			NLS_TYPE_LONG, 5, -1);
 */
NLSFMTAPI_PUBLIC(NLS_ErrorCode) 
NLS_FormatMessage(const Locale *locale, UnicodeString *result, const UnicodeString* pattern, ...);


/******************** Formatting Constructor functions ************************/

/* NLS_NewMessageFormat
 *
 * Create a MessageFormat from a pattern. The patern is a choice format 
 * specification of the form 
 *
 * "There {0,choice,0#are no files|1#is one file|1<are {0,number} files}."
 *
 * Status Return
 *		NLS_SUCCESS
 *		NLS_PARAM_ERROR				- parameter verification failed   
 */
NLSFMTAPI_PUBLIC(NLS_ErrorCode) 
NLS_NewMessageFormat(const Locale *locale, MessageFormat ** result, const UnicodeString* pattern);

/********************* Formatting Destructor functions *******************************/

/* NLS_DeleteMessageFormat
 *
 * Delete a MessageFormat object.
 *
 * Status Return
 *		NLS_SUCCESS
 *		NLS_PARAM_ERROR				- parameter verification failed   
 */
NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_DeleteMessageFormat(MessageFormat * that);

/******************** Formatting API ************************/

/* NLS_MessageFormatSetPattern
 *
 * Change the patter associated with the MessageFormat object
 *
 * Status Return
 *		NLS_SUCCESS
 *		NLS_PARAM_ERROR				- parameter verification failed   
 */
NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_MessageFormatSetPattern(MessageFormat * that, const UnicodeString* pattern);

/* NLS_MessageFormatGetPattern
 *
 * Get the pattern associated with a choice format object
 *
 * Status Return
 *		NLS_SUCCESS
 *		NLS_PARAM_ERROR				- parameter verification failed   
 */

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_MessageFormatGetPattern(MessageFormat * that, UnicodeString* pattern);

/* NLS_MessageFormat
 *
 * Generate a formatted string using the pattern associated 
 * with the choice format, based on the specified parameters
 *
 * varg's are value pairs composed of NLS_FormattableType followed by 
 * argument. The last parameter to the routine should be -1 to indicate the
 * end of the parameters.
 * eg:
 *	
 * err = NLS_MessageFormat(MessageFormat, result,  
 *			NLS_TYPE_STRING, "A Disk", 
 *			NLS_TYPE_LONG, 5, -1);
 *
 * Status Return
 *		NLS_SUCCESS
 *		NLS_PARAM_ERROR				- parameter verification failed   
 */

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_MessageFormat(const MessageFormat * that, UnicodeString* result, ...);


/************************ End ******************************/

NLS_END_PROTOS
#endif /* _NLSMSG_H */

