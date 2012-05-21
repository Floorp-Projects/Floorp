/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIMIMEHeaderParam.h"

#ifndef __nsmimeheaderparamimpl_h___
#define __nsmimeheaderparamimpl_h___
class nsMIMEHeaderParamImpl : public nsIMIMEHeaderParam
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMIMEHEADERPARAM

  nsMIMEHeaderParamImpl() {}
  virtual ~nsMIMEHeaderParamImpl() {}
private:
  // Toggles support for RFC 2231 decoding, or RFC 5987 (5987 profiles 2231
  // for use in HTTP, and, for instance, drops support for continuations)
  enum ParamDecoding {
    RFC_2231_DECODING = 1,
    RFC_5987_DECODING
  }; 

  nsresult DoGetParameter(const nsACString& aHeaderVal, 
                          const char *aParamName,
                          ParamDecoding aDecoding,
                          const nsACString& aFallbackCharset, 
                          bool aTryLocaleCharset, 
                          char **aLang, 
                          nsAString& aResult);

  nsresult DoParameterInternal(const char *aHeaderValue, 
                               const char *aParamName,
                               ParamDecoding aDecoding,
                               char **aCharset,
                               char **aLang,
                               char **aResult);

};

#endif 

