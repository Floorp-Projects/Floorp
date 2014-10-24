/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHTMLFormatConverter_h__
#define nsHTMLFormatConverter_h__

#include "nsIFormatConverter.h"
#include "nsString.h"

class nsHTMLFormatConverter : public nsIFormatConverter
{
public:

  nsHTMLFormatConverter();

  // nsISupports
  NS_DECL_ISUPPORTS
  NS_DECL_NSIFORMATCONVERTER

protected:
  virtual ~nsHTMLFormatConverter();

  nsresult AddFlavorToList ( nsISupportsArray* inList, const char* inFlavor ) ;

  NS_IMETHOD ConvertFromHTMLToUnicode(const nsAutoString & aFromStr, nsAutoString & aToStr);
  NS_IMETHOD ConvertFromHTMLToAOLMail(const nsAutoString & aFromStr, nsAutoString & aToStr);

};

#endif // nsHTMLFormatConverter_h__
