/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsHTMLFormatConverter_h__
#define nsHTMLFormatConverter_h__

#include "nsIFormatConverter.h"
#include "nsString.h"

class nsHTMLFormatConverter : public nsIFormatConverter
{
public:

  nsHTMLFormatConverter();
  virtual ~nsHTMLFormatConverter();

    // nsISupports
  NS_DECL_ISUPPORTS
  
    // nsIHTMLConverter
  NS_IMETHOD GetInputDataFlavors(nsISupportsArray **_retval) ;
  NS_IMETHOD GetOutputDataFlavors(nsISupportsArray **_retval) ;
  NS_IMETHOD CanConvert(const char *aFromDataFlavor, const char *aToDataFlavor, PRBool *_retval) ;
  NS_IMETHOD Convert(const char *aFromDataFlavor, nsISupports *aFromData, PRUint32 aDataLen, 
                      const char *aToDataFlavor, nsISupports **aToData, PRUint32 *aDataToLen) ;

protected:

  nsresult AddFlavorToList ( nsISupportsArray* inList, const char* inFlavor ) ;

  NS_IMETHOD ConvertFromHTMLToUnicode(const nsAutoString & aFromStr, nsAutoString & aToStr);
  NS_IMETHOD ConvertFromHTMLToAOLMail(const nsAutoString & aFromStr, nsAutoString & aToStr);

};

#endif // nsHTMLFormatConverter_h__
