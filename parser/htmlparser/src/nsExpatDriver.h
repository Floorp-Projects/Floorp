/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef NS_EXPAT_DRIVER__
#define NS_EXPAT_DRIVER__

#include "xmlparse.h"
#include "nsString.h"
#include "nsIDTD.h"
#include "nsITokenizer.h"
#include "nsIInputStream.h"

class nsIExpatSink;
struct nsCatalogData;

class nsExpatDriver : public nsIDTD,
                      public nsITokenizer
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDTD
  NS_DECL_NSITOKENIZER

  nsExpatDriver();
  virtual ~nsExpatDriver();

  int HandleExternalEntityRef(const PRUnichar *openEntityNames,
                              const PRUnichar *base,
                              const PRUnichar *systemId,
                              const PRUnichar *publicId);
  nsresult HandleStartElement(const PRUnichar *aName, const PRUnichar **aAtts);
  nsresult HandleEndElement(const PRUnichar *aName);
  nsresult HandleCharacterData(const PRUnichar *aCData, const PRUint32 aLength);
  nsresult HandleComment(const PRUnichar *aName);
  nsresult HandleProcessingInstruction(const PRUnichar *aTarget, const PRUnichar *aData);
  nsresult HandleXMLDeclaration(const PRUnichar *aData, const PRUint32 aLength);
  nsresult HandleDefault(const PRUnichar *aData, const PRUint32 aLength);
  nsresult HandleStartCdataSection();
  nsresult HandleEndCdataSection();
  nsresult HandleStartDoctypeDecl();
  nsresult HandleEndDoctypeDecl();

protected:

  // Load up an external stream to get external entity information
  nsresult OpenInputStreamFromExternalDTD(const PRUnichar* aFPIStr,
                                          const PRUnichar* aURLStr, 
                                          const PRUnichar* aBaseURL,
                                          nsIInputStream** in, 
                                          nsAString& aAbsURL);

  nsresult ParseBuffer(const char* aBuffer, PRUint32 aLength, PRBool aIsFinal);
  nsresult HandleError(const char *aBuffer, PRUint32 aLength, PRBool aIsFinal);
  void     GetLine(const char* aSourceBuffer, PRUint32 aLength, PRUint32 aOffset, nsString& aLine);

  XML_Parser       mExpatParser;  
  nsString         mLastLine;
  nsString         mCDataText;
  nsString         mDoctypeText;
  PRPackedBool     mInCData;
  PRPackedBool     mInDoctype;
  PRPackedBool     mInExternalDTD;
  PRPackedBool     mHandledXMLDeclaration;
  PRInt32          mBytePosition;
  nsresult         mInternalState;
  PRUint32         mBytesParsed;
  nsIExpatSink*    mSink;
  const nsCatalogData* mCatalogData; // weak

};
nsresult NS_NewExpatDriver(nsIDTD** aDriver);

#endif
