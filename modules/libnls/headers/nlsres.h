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


#ifndef _NLSRES_H
#define _NLSRES_H

#include "nlsxp.h"

#include "nlsuni.h"
#include "nlsloc.h"
#include "nlsmsg.h"
#include "nlsutl.h"

#ifdef NLS_CPLUSPLUS
#include "resbdl.h"
#include "msgfmt.h"
#include "msgstr.h"
#endif

NLS_BEGIN_PROTOS

/**************************** Types *********************************/

#ifndef NLS_CPLUSPLUS
typedef struct _PropertyResourceBundle			PropertyResourceBundle;
typedef struct _MessageString					MessageString;
#endif

/********************** Library Initialization ***********************/


/* NLS_ResInitialize
 *
 *	Initialize the NLS Resource library. 
 *
 * Status Return
 *		NLS_SUCCESS
 */
NLSRESAPI_PUBLIC(NLS_ErrorCode)
NLS_ResInitialize(const NLS_ThreadInfo* threadInfo, const char* resourceDirectory);

/* NLS_ResTerminate
 *
 * Terminate the library.
 *
 * Status Return
 *		NLS_SUCCESS
 */
NLSRESAPI_PUBLIC(NLS_ErrorCode)
NLS_ResTerminate(void);

/************************ Search Path APIs ******************************/

/* NLS_ResAddSearchPath
 *
 * Add a new search path to the NLS Resource library for
 *  searching for resource files
 *
 * Status Return
 *      NLS_SUCCES - completed
 *      NLS_PARAM_ERROR - bad search path
 *      NLS_INTERNAL_PROGRAM_ERROR - internal failure, could not add path
 *
 */
NLSRESAPI_PUBLIC(NLS_ErrorCode)
NLS_ResAddSearchPath(const char* searchPath);

/* NLS_GetSearchPath
 *
 * Returns the current search path as a const char*
 *
 */
NLSRESAPI_PUBLIC(const char*)
NLS_ResGetSearchPath(void);


/************************ Simple Resource APIs ******************************/

/** NLS_PropertyResourceGetUnicodeResource
 *
 * Gets a UnioceString resource from the specified property file, given the user accept-language 
 * specification. The UnicodeString must be release by call NLS_DeleteUnicodeString
 *
 * Status Return
 *		If the resource is not located a NULL pointer will return. 
 */
NLSRESAPI_PUBLIC(UnicodeString*)
NLS_PropertyResourceGetUnicodeResource(const char* packageName, const char* acceptLanguage,
						    const char* key);

/** NLS_PropertyResourceGetCharResource
 *
 * Gets a char* resource from the specified property file, given the user accept-language 
 * specification in the specified encoding.
 * If the to_charset is not specified, the property $TARGET_ENCODING$ in the resouce 
 * bundle will be used to specify the encoding. The cvhar* must be release by call free()
 *
 * Status Return
 *		If the resource is not located a NULL pointer will return. 
 */
NLSRESAPI_PUBLIC(char*)
NLS_PropertyResourceGetCharResource(const char* packageName, const char* acceptLanguage,
						    const char* key, const char* to_charset);



/************************ Resource APIs ******************************/

/** NLS_PropertyResourceBundleExists
 *
 *	Determine if the specified resource bundle (or something close to it) exists.
 *
 */
NLSRESAPI_PUBLIC(nlsBool) 
NLS_PropertyResourceBundleExists(const char* packageName,
								 const Locale* locale);
 
/** NLS_NewPropertyResourceBundle
 *
 *	Creates a new PropertyResourceBundle from the specified locale and
 *  package name.
 *
 * Status Return
 *		NLS_SUCCESS
 *		NLS_USING_FALLBACK_LOCALE
 *		NLS_PARAM_ERROR
 *		NLS_RESOURCE_OPEN_ERROR
 */
NLSRESAPI_PUBLIC(NLS_ErrorCode) 
NLS_NewPropertyResourceBundle(PropertyResourceBundle** resourceBundle,
					  const char* packageName,
					  const Locale* locale);
 
/** NLS_NewPropertyResourceBundleFromAcceptLanguage
 *
 *	Creates a new PropertyResourceBundle from the specified accept-language string and
 *  package name.
 *
 * Status Return
 *		NLS_SUCCESS
 *		NLS_USING_FALLBACK_LOCALE
 *		NLS_PARAM_ERROR
 *		NLS_RESOURCE_OPEN_ERROR
 */
NLSRESAPI_PUBLIC(NLS_ErrorCode) 
NLS_NewPropertyResourceBundleFromAcceptLanguage(PropertyResourceBundle** resourceBundle,
					  const char* packageName,
					  const char* acceptLanguage);
 
/** NLS_DeletePropertyResourceBundle
 *
 * Destroy a ResourceBundle.
 *
 * Status Return
 *		NLS_SUCCESS
 *		NLS_PARAM_ERROR
 */
NLSRESAPI_PUBLIC(NLS_ErrorCode)
NLS_DeletePropertyResourceBundle(PropertyResourceBundle* resourceBundle);


/** NLS_PropertyResourceBundleGetString
 *
 * Get a string from the ResourceBundle given a key.
 *
 * Status Return
 *		NLS_SUCCESS
 *		NLS_PARAM_ERROR
 *		NLS_RESOURCE_NOT_FOUND
 */
NLSRESAPI_PUBLIC(NLS_ErrorCode)
NLS_PropertyResourceBundleGetString(PropertyResourceBundle* resourceBundle,
						    const char* key, UnicodeString * resource);

/** NLS_PropertyResourceBundleGetStringUniChar
 *
 * Get a string from the ResourceBundle, lengths are UniChar counts, and 
 * not byte counts.
 *
 * Status Return
 *		NLS_SUCCESS
 *		NLS_PARAM_ERROR
 *		NLS_RESOURCE_NOT_FOUND
 *		NLS_RESULT_TRUNCATED
 */
NLSRESAPI_PUBLIC(NLS_ErrorCode)
NLS_PropertyResourceBundleGetStringUniChar(PropertyResourceBundle* resourceBundle,
								   const char* key,
								   UniChar* resource,
								   size_t length,
								   size_t* resultLength);

/** NLS_PropertyResourceBundleGetChar
 *
 * Get a char* from the ResourceBundle. The memory allocated by this routine must 
 * be freed using free(); If no encoding name is specified the routine will use the 
 * default encoding specified by the property file itself to specify the output encoding. 
 *
 * Status Return
 *	If an error occurs, a NULL pointer will be returned by this routine. 
 *
 */
NLSRESAPI_PUBLIC(char*)
NLS_PropertyResourceBundleGetChar(PropertyResourceBundle* resourceBundle,
								   const char* key,
								   const char* to_charset);

/** NLS_PropertyResourceBundleGetStringLength
 *
 * Get the UniChar length from the string corresponding to the key passed in.
 *
 * Status Return
 *		NLS_SUCCESS
 *		NLS_PARAM_ERROR
 *		NLS_RESOURCE_NOT_FOUND
 */
NLSRESAPI_PUBLIC(NLS_ErrorCode)
NLS_PropertyResourceBundleGetStringLength(PropertyResourceBundle *resourceBundle,
								  const char* key,
								  size_t *resultLength);


/************************ Message String ******************************/

/** NLS_NewMessageString
 *
 *	Creates a new MessageString. A message string is a packed formatted message
 *  which includes the message key, and the parameters. A string representation 
 *  for the message string can be obtained in any locale.
 *
 * err = NLS_NewMessageString(result, 
 *			"my.package.string", "STRING_1", 
 *			NLS_TYPE_STRING, "A Disk", 
 *			NLS_TYPE_LONG, 5, -1);
 *
 * Status Return
 *		NLS_SUCCESS
 *		NLS_PARAM_ERROR
 */
NLSRESAPI_PUBLIC(NLS_ErrorCode) 
NLS_NewMessageString(MessageString** messageString,
					  const char* packageName,
					  const char* key, ...);

/** NLS_NewMessageStringFromTemplate
 *
 *	Creates a new MessageString. From the exported format.
 *
 * Status Return
 *		NLS_SUCCESS
 *		NLS_PARAM_ERROR
 */
NLSRESAPI_PUBLIC(NLS_ErrorCode) 
NLS_NewMessageStringFromExportFormat(MessageString** messageString,
					  const char* exportFormat);

/** NLS_DeleteMessageString
 *
 * Destroy a ResourceBundle.
 *
 * Status Return
 *		NLS_SUCCESS
 *		NLS_PARAM_ERROR
 */
NLSRESAPI_PUBLIC(NLS_ErrorCode)
NLS_DeleteMessageString(MessageString* messageString);


/** NLS_MessageStringGetExportFormat
 *
 * Gets an exportable format string from the MessageString. This can be used to 
 * recreade a MessageString at some future point in time.
 *
 * Status Return
 *		NLS_SUCCESS
 *		NLS_PARAM_ERROR
 *		NLS_RESULT_TRUNCATED
 */
NLSRESAPI_PUBLIC(const char*)
NLS_MessageStringGetExportFormat(MessageString *messageString);

/** NLS_MessageStringGetStringFromLocale
 *
 * Gets the user visible string from the MessageString for a given locale.
 *
 * Status Return
 *		NLS_SUCCESS
 *		NLS_PARAM_ERROR
 *		NLS_RESOURCE_NOT_FOUND
 */
NLSRESAPI_PUBLIC(NLS_ErrorCode)
NLS_MessageStringGetStringFromLocale(MessageString *messageString,
						    const Locale* locale, 
							UnicodeString * resource);

/** NLS_MessageStringGetStringFromLocale
 *
 * Gets the user visible string from the MessageString for a given accept-language.
 *
 * Status Return
 *		NLS_SUCCESS
 *		NLS_PARAM_ERROR
 *		NLS_RESOURCE_NOT_FOUND
 */
NLSRESAPI_PUBLIC(NLS_ErrorCode)
NLS_MessageStringGetStringFromAcceptLanguage(MessageString *messageString,
						    const char* acceptLanguage, 
							UnicodeString * resource);


/************************ End ******************************/
NLS_END_PROTOS
#endif /* _NLSRES_H */

