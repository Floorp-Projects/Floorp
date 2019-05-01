/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIMIMEHeaderParam.h"

#ifndef __nsmimeheaderparamimpl_h___
#  define __nsmimeheaderparamimpl_h___
class nsMIMEHeaderParamImpl : public nsIMIMEHeaderParam {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMIMEHEADERPARAM

  nsMIMEHeaderParamImpl() = default;

  /**
   * Identical to calling
   * GetParameterHTTP(aHeaderVal, aParameterName, EmptyCString(), false,
   * nullptr, aResult) See nsIMIMEHeaderParam.idl for more information.
   */
  static nsresult GetParameterHTTP(const nsACString& aHeaderVal,
                                   const char* aParamName, nsAString& aResult);

 private:
  virtual ~nsMIMEHeaderParamImpl() = default;
  enum ParamDecoding { MIME_FIELD_ENCODING = 1, HTTP_FIELD_ENCODING };

  static nsresult DoGetParameter(const nsACString& aHeaderVal,
                                 const char* aParamName,
                                 ParamDecoding aDecoding,
                                 const nsACString& aFallbackCharset,
                                 bool aTryLocaleCharset, char** aLang,
                                 nsAString& aResult);

  static nsresult DoParameterInternal(const char* aHeaderValue,
                                      const char* aParamName,
                                      ParamDecoding aDecoding, char** aCharset,
                                      char** aLang, char** aResult);
};

#endif
