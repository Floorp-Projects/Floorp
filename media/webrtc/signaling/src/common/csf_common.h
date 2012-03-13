/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Cisco Systems SIP Stack.
 *
 * The Initial Developer of the Original Code is
 * Cisco Systems (CSCO).
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Enda Mannion <emannion@cisco.com>
 *  Suhas Nandakumar <snandaku@cisco.com>
 *  Ethan Hugg <ehugg@cisco.com>
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

#ifndef _CSF_COMMON_E58E5677_950A_424c_B6C2_CA180092E6A2_H
#define _CSF_COMMON_E58E5677_950A_424c_B6C2_CA180092E6A2_H

#include <assert.h>
#include <memory>
#include <vector>
#include <stdlib.h>

/*

This header file defines:

csf_countof
csf_strcpy
csf_sprintf
csf_vsprintf

*/

/*
  General security tip: Ensure that "format" is never a user-defined string. Format should ALWAYS be something that's built into your code, not
  user supplied. For example: never write:

  csf_sprintf(buffer, csf_countof(buffer), pUserSuppliedString);

  Instead write:

  csf_sprintf(buffer, csf_countof(buffer), "%s", pUserSuppliedString);

*/

#ifdef WIN32
    #if !defined(_countof)
        #if !defined(__cplusplus)
            #define _countof(_Array) (sizeof(_Array) / sizeof(_Array[0]))
        #else
            extern "C++"
            {
            template <typename _CountofType, size_t _SizeOfArray>
            char (*_csf_countof_helper(_CountofType (&_Array)[_SizeOfArray]))[_SizeOfArray];
            #define _countof(_Array) sizeof(*_csf_countof_helper(_Array))
            }
        #endif
    #endif
#else
    #define _countof(_Array) (sizeof(_Array) / sizeof(_Array[0]))
#endif
//csf_countof

#define csf_countof(anArray) _countof(anArray)

//csf_strcpy

#ifdef _WIN32
  #define csf_strcpy(/* char* */ pDestination, /* size_t */ sizeOfBufferInCharsInclNullTerm, /* const char * */ pSource)\
    strcpy_s(pDestination, sizeOfBufferInCharsInclNullTerm, pSource)
#else
  #define csf_strcpy(/* char * */ pDestination, /* size_t */ sizeOfBufferInCharsInclNullTerm, /* const char * */ pSource)\
    strncpy(pDestination, pSource, sizeOfBufferInCharsInclNullTerm);\
    pDestination[sizeOfBufferInCharsInclNullTerm-1] = '\0'
#endif


//csf_sprintf

#ifdef _WIN32
  //Unlike snprintf, sprintf_s guarantees that the buffer will be null-terminated (unless the buffer size is zero).
  #define csf_sprintf(/* char* */ buffer, /* size_t */ sizeOfBufferInCharsInclNullTerm, /* const char * */ format, ...)\
    _snprintf_s (buffer, sizeOfBufferInCharsInclNullTerm, _TRUNCATE, format, __VA_ARGS__)
#else
  #define csf_sprintf(/* char */ buffer, /* size_t */ sizeOfBufferInCharsInclNullTerm, /* const char * */ format, ...)\
    snprintf (buffer, sizeOfBufferInCharsInclNullTerm, format, __VA_ARGS__);\
    buffer[sizeOfBufferInCharsInclNullTerm-1] = '\0'
#endif

//csf_vsprintf

#ifdef _WIN32
  #define csf_vsprintf(/* char* */ buffer, /* size_t */ sizeOfBufferInCharsInclNullTerm, /* const char * */ format, /* va_list */ vaList)\
    vsnprintf_s (buffer, sizeOfBufferInCharsInclNullTerm, _TRUNCATE, format, vaList);\
    buffer[sizeOfBufferInCharsInclNullTerm-1] = '\0'
#else
  #define csf_vsprintf(/* char */ buffer, /* size_t */ sizeOfBufferInCharsInclNullTerm, /* const char * */ format, /* va_list */ vaList)\
    vsprintf (buffer, format, vaList);\
    buffer[sizeOfBufferInCharsInclNullTerm-1] = '\0'
#endif

#endif
