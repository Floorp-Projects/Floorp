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


#ifndef _NLSLOC_H
#define _NLSLOC_H


#include "nlsxp.h"
#include "nlsuni.h"

#ifdef NLS_CPLUSPLUS
#include "locid.h"
#endif

NLS_BEGIN_PROTOS

/**************************** Types *********************************/

 

/* UnicodeString is the basic type which replaces the char* datatype.
    - A UnicodeString is an opaque data type, the Unicode String Accessors must be 
      used to access the data.
    */
#ifndef NLS_CPLUSPLUS
typedef struct _Locale Locale;
#endif

/********************** Library Initialization ***********************/


/* NLS_Initialize
 *
 * Initialize the libuni library. If the application is a multi-threaded 
 * application is should supply a threadInfo specification to allow 
 * the library to protect critical code sections. The dataDirectory is a full or
 * partial platform specific path specification under which the library may 
 * can find the "locales" and "converters" sub directories. 
 *
 * Status Return
 *		NLS_SUCCESS
 */
NLSUNIAPI_PUBLIC(NLS_ErrorCode)
NLS_Initialize(const NLS_ThreadInfo * threadInfo, const char * dataDirectory);

/* NLS_Terminate
 *
 * Terminate the library
 *
 * Status Return
 *		NLS_SUCCESS
 */
NLSUNIAPI_PUBLIC(NLS_ErrorCode)
NLS_Terminate(void);

/************************ Locale Methods ******************************/

/** NLS_GetDefaultLocale
 *
 * Returns the default locale setting. The default locale is based on 
 * the platform locale setting.
 *
 * Status Return
 *		NLS_SUCCESS
 */
NLSUNIAPI_PUBLIC(const Locale *)
NLS_GetDefaultLocale();

/** NLS_SetDefaultLocale
 *
 * Sets the default.
 * Normally set once at the beginning of the application,
 * then never reset. setDefault does not reset the host locale.
 *
 * Status Return
 *		NLS_SUCCESS
 */
NLSUNIAPI_PUBLIC(NLS_ErrorCode)
NLS_SetDefaultLocale(const Locale * locale);


/** NLS_NewNamedLocale
 *
 * Construct a locale from language, country, variant. Any or all of the 
 * UnicodeStrings may be NULL. 
 *
 * Status Return
 *		NLS_SUCCESS
 *		NLS_NEW_LOCALE_FAILED
 */
NLSUNIAPI_PUBLIC(NLS_ErrorCode)
NLS_NewNamedLocale(Locale ** locale,    
        const    UnicodeString *    language, 
        const    UnicodeString *    country, 
        const    UnicodeString *    variant);

/** NLS_NewNamedLocaleFromChar
 *
 * Construct a locale from language, country, variant. Any or all of the 
 * char* may be NULL. 
 *
 * Status Return
 *		NLS_SUCCESS
 *		NLS_NEW_LOCALE_FAILED
 */
NLSUNIAPI_PUBLIC(NLS_ErrorCode)
NLS_NewNamedLocaleFromChar(Locale ** locale,    
        const    char *    language, 
        const    char *    country, 
        const    char *    variant);

/** NLS_NewNamedLocaleFromLocaleSpec
 *
 * Construct a locale from a localeSpec of the form "en_US_variant"
 *
 * Status Return
 *		NLS_SUCCESS
 *		NLS_NEW_LOCALE_FAILED
 */
NLSUNIAPI_PUBLIC(NLS_ErrorCode)
NLS_NewNamedLocaleFromLocaleSpec(Locale ** locale,    
        const    char *    localeSpec);


/** NLS_DeleteLocale
 *
 * Destroy a locale 
 *
 * Status Return
 *		NLS_SUCCESS
 *		NLS_PARAM_ERROR
 */
NLSUNIAPI_PUBLIC(NLS_ErrorCode)
NLS_DeleteLocale(Locale *locale);

/** NLS_EqualLocales
 * 
 * Equality test for locales
 */
NLSUNIAPI_PUBLIC(nlsBool)
NLS_EqualLocales(const Locale * source, const Locale * target);    

/** NLS_LocaleGetLanguage
 *
 * Getter for programmatic name of field,
 * an lowercased two-letter ISO-639 code.
 *
 * Status Return
 *		NLS_SUCCESS
 *		NLS_PARAM_ERROR
 */
NLSUNIAPI_PUBLIC(NLS_ErrorCode)
NLS_LocaleGetLanguage(const Locale * locale, UnicodeString * lang);

/** NLS_LocaleGetCountry
 *
 * Getter for programmatic name of field,
 * an uppercased two-letter ISO-3166 code.
 *
 * Status Return
 *		NLS_SUCCESS
 *		NLS_PARAM_ERROR
 */
NLSUNIAPI_PUBLIC(NLS_ErrorCode)
NLS_LocaleGetCountry(const Locale * locale, UnicodeString * cntry);

/** NLS_LocaleGetVariant
 *
 * Getter for programmatic name of field.
 *
 * Status Return
 *		NLS_SUCCESS
 *		NLS_PARAM_ERROR
 */
NLSUNIAPI_PUBLIC(NLS_ErrorCode)
NLS_LocaleGetVariant(const Locale * locale, UnicodeString * var);

/** NLS_LocaleGetName
 *
 * Getter for the programmatic name of the entire locale,
 * with the language, country and variant separated by underbars.
 * If a field is missing, at most one underbar will occur.
 * Example: "en", "de_DE", "en_US_WIN", "de_POSIX", "fr_MAC"
 *
 * Status Return
 *		NLS_SUCCESS
 *		NLS_PARAM_ERROR
 */
NLSUNIAPI_PUBLIC(NLS_ErrorCode)
NLS_LocaleGetName(const Locale * locale, UnicodeString * name);

/** NLS_LocaleGetISO3Language
 *
 * Getter for the three-letter ISO language abbreviation
 * of the locale.
 *
 * Status Return
 *		NLS_SUCCESS
 *		NLS_PARAM_ERROR
 */
NLSUNIAPI_PUBLIC(NLS_ErrorCode)
NLS_LocaleGetISO3Language(const Locale * locale, UnicodeString * name);

/** NLS_LocaleGetISO3Country
 *
 * Getter for the three-letter ISO country abbreviation
 * of the locale.
 *
 * Status Return
 *		NLS_SUCCESS
 *		NLS_PARAM_ERROR
 */
NLSUNIAPI_PUBLIC(NLS_ErrorCode)
NLS_LocaleGetISO3Country(const Locale * locale, UnicodeString * name);

/** NLS_LocaleGetDisplayLanguage
 *
 * Getter for display of field to user.
 * If the localized name is not found, returns the ISO code.
 * The desired user language is from the default locale.
 *
 * Status Return
 *		NLS_SUCCESS
 *		NLS_PARAM_ERROR
 */
NLSUNIAPI_PUBLIC(NLS_ErrorCode)
NLS_LocaleGetDisplayLanguage(const Locale * locale, UnicodeString * dispLang);

/** NLS_LocaleGetDisplayLanguageInLocale
 *
 * Getter for display of field to user.
 * If the localized name is not found, returns the ISO codes.
 * Example: "English (UK)", "Deutch", "Germany"
 *
 * Status Return
 *		NLS_SUCCESS
 *		NLS_PARAM_ERROR
 */
NLSUNIAPI_PUBLIC(NLS_ErrorCode)
NLS_LocaleGetDisplayLanguageInLocale(const Locale * locale, 
        const Locale * inLocale, UnicodeString *    dispLang);

/** NLS_LocaleGetDisplayCountry
 *
 * Getter for display of field to user.
 * If the localized name is not found, returns the ISO code.
 * The default locale is used for the presentation language.
 *
 * Status Return
 *		NLS_SUCCESS
 *		NLS_PARAM_ERROR
 */
NLSUNIAPI_PUBLIC(NLS_ErrorCode)
NLS_LocaleGetDisplayCountry(const Locale * locale, 
        UnicodeString * dispCountry);

/** NLS_LocaleGetDisplayCountryInLocale
 *
 * Getter for display of field to user.
 * If the localized name is not found, returns the ISO code.
 * @param inLocale specifies the desired user language.
 *
 * Status Return
 *		NLS_SUCCESS
 *		NLS_PARAM_ERROR
 */
NLSUNIAPI_PUBLIC(NLS_ErrorCode)
NLS_LocaleGetDisplayCountryInLocale(const Locale * locale, 
        const Locale * inLocale, UnicodeString * dispCountry);

/** NLS_LocaleGetDisplayVariant
 *
 * Getter for display of field to user.
 * If the localized name is not found, returns the variant code.
 * The default locale is used for the presentation language.
 *
 * Status Return
 *		NLS_SUCCESS
 *		NLS_PARAM_ERROR
 */
NLSUNIAPI_PUBLIC(NLS_ErrorCode)
NLS_LocaleGetDisplayVariant(const Locale * locale, UnicodeString * dispVar);

/** NLS_LocaleGetDisplayVariantInLocale
 *
 * Getter for display of field to user
 * If the localized name is not found, returns the variant code.
 * @param inLocale specifies the desired user language.
 *
 * Status Return
 *		NLS_SUCCESS
 *		NLS_PARAM_ERROR
 */
NLSUNIAPI_PUBLIC(NLS_ErrorCode)
NLS_LocaleGetDisplayVariantInLocale(const Locale * locale, 
        const Locale * inLocale, UnicodeString * dispVar) ;

/** NLS_LocaleGetDisplayName
 *
 * Getter for display of the entire locale to user.
 * If the localized name is not found, uses the ISO codes.
 * The default locale is used for the presentation language.
 *
 * Status Return
 *		NLS_SUCCESS
 *		NLS_PARAM_ERROR
 */
NLSUNIAPI_PUBLIC(NLS_ErrorCode)
NLS_LocaleGetDisplayName(const Locale * locale, UnicodeString * name);

/** NLS_LocaleGetDisplayNameInLocale
 *
 * Getter for display of the entire locale to user.
 * If the localized name is not found, uses the ISO codes
 * @param inLocale specifies the desired user language.
 *
 * Status Return
 *		NLS_SUCCESS
 *		NLS_PARAM_ERROR
 */
NLSUNIAPI_PUBLIC(NLS_ErrorCode)
NLS_LocaleGetDisplayNameInLocale(const Locale * locale, 
        const Locale * inLocale, UnicodeString * name);

/** NLS_LocaleCountAvailableLocales
 *
 * Get the count of the set of Locales are installed
 *
 * Status Return
 *		NLS_SUCCESS
 *		NLS_PARAM_ERROR
 */
NLSUNIAPI_PUBLIC(size_t)
NLS_LocaleCountAvailableLocales();

/** NLS_LocaleGetAvailableLocales
 *
 * Get the set of Locales for which, the array must be 
 * allocated by the caller, and count must specify how many entries exist.
 *
 * Status Return
 *		NLS_SUCCESS
 *		NLS_PARAM_ERROR
 */
NLSUNIAPI_PUBLIC(NLS_ErrorCode)
NLS_LocaleGetAvailableLocales(Locale * LocaleArray, 
                    size_t inCount,
					size_t *count);

/********************* UnicodeString Accessor functions *******************************/


/** NLS_LocaleGetAvailableLocales
 *
 * Upper case a string according to specific Locale rules
 *
 * Status Return
 *		NLS_SUCCESS
 *		NLS_PARAM_ERROR
 */
NLSUNIAPI_PUBLIC(NLS_ErrorCode)
NLS_UnicodeStringtoUpperForLocale(UnicodeString * target, const Locale * locale);

/** NLS_UnicodeStringtoLowerForLocale
 *
 * Lower case a string according to specific Locale rules
 *
 * Status Return
 *		NLS_SUCCESS
 *		NLS_PARAM_ERROR
 */
NLSUNIAPI_PUBLIC(NLS_ErrorCode)
NLS_UnicodeStringtoLowerForLocale(UnicodeString * target, const Locale * locale);


/************************ End ******************************/
NLS_END_PROTOS
#endif /* _NLSLOC_H */

