/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsError_h__
#define nsError_h__

#ifndef __cplusplus
#  error nsError.h no longer supports C sources
#endif

#include "mozilla/Attributes.h"
#include "mozilla/Likely.h"

#include <stdint.h>

#define NS_ERROR_SEVERITY_SUCCESS 0
#define NS_ERROR_SEVERITY_ERROR 1

#include "ErrorList.h"  // IWYU pragma: export

/**
 * @name Standard Error Handling Macros
 * @return 0 or 1 (false/true with bool type for C++)
 */

inline uint32_t NS_FAILED_impl(nsresult aErr) {
  return static_cast<uint32_t>(aErr) & 0x80000000;
}
#define NS_FAILED(_nsresult) ((bool)MOZ_UNLIKELY(NS_FAILED_impl(_nsresult)))
#define NS_SUCCEEDED(_nsresult) ((bool)MOZ_LIKELY(!NS_FAILED_impl(_nsresult)))

/* Check that our enum type is actually uint32_t as expected */
static_assert(((nsresult)0) < ((nsresult)-1),
              "nsresult must be an unsigned type");
static_assert(sizeof(nsresult) == sizeof(uint32_t), "nsresult must be 32 bits");

#define MOZ_ALWAYS_SUCCEEDS(expr) MOZ_ALWAYS_TRUE(NS_SUCCEEDED(expr))

/**
 * @name Standard Error Generating Macros
 */

#define NS_ERROR_GENERATE(sev, module, code)                            \
  (nsresult)(((uint32_t)(sev) << 31) |                                  \
             ((uint32_t)(module + NS_ERROR_MODULE_BASE_OFFSET) << 16) | \
             ((uint32_t)(code)))

#define NS_ERROR_GENERATE_SUCCESS(module, code) \
  NS_ERROR_GENERATE(NS_ERROR_SEVERITY_SUCCESS, module, code)

#define NS_ERROR_GENERATE_FAILURE(module, code) \
  NS_ERROR_GENERATE(NS_ERROR_SEVERITY_ERROR, module, code)

/*
 * This will return the nsresult corresponding to the most recent NSPR failure
 * returned by PR_GetError.
 *
 ***********************************************************************
 *      Do not depend on this function. It will be going away!
 ***********************************************************************
 */
extern nsresult NS_ErrorAccordingToNSPR();

/**
 * @name Standard Macros for retrieving error bits
 */

inline constexpr uint16_t NS_ERROR_GET_CODE(nsresult aErr) {
  return uint32_t(aErr) & 0xffff;
}
inline constexpr uint16_t NS_ERROR_GET_MODULE(nsresult aErr) {
  return ((uint32_t(aErr) >> 16) - NS_ERROR_MODULE_BASE_OFFSET) & 0x1fff;
}
inline bool NS_ERROR_GET_SEVERITY(nsresult aErr) {
  return uint32_t(aErr) >> 31;
}

#ifdef _MSC_VER
#  pragma warning(disable : 4251) /* 'nsCOMPtr<class nsIInputStream>' needs to \
                                     have dll-interface to be used by clients  \
                                     of class 'nsInputStream' */
#  pragma warning(                                                          \
      disable : 4275) /* non dll-interface class 'nsISupports' used as base \
                         for dll-interface class 'nsIRDFNode' */
#endif

#endif
