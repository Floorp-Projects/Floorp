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

#include "expat_config.h"
#include "expat.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIDTD.h"
#include "nsITokenizer.h"
#include "nsIInputStream.h"
#include "nsIParser.h"
#include "nsCycleCollectionParticipant.h"

class nsIExpatSink;
class nsIExtendedExpatSink;
struct nsCatalogData;

class nsExpatDriver : public nsIDTD,
                      public nsITokenizer
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSIDTD
  NS_DECL_NSITOKENIZER
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsExpatDriver, nsIDTD)

  nsExpatDriver();
  virtual ~nsExpatDriver();

  int HandleExternalEntityRef(const PRUnichar *aOpenEntityNames,
                              const PRUnichar *aBase,
                              const PRUnichar *aSystemId,
                              const PRUnichar *aPublicId);
  nsresult HandleStartElement(const PRUnichar *aName, const PRUnichar **aAtts);
  nsresult HandleEndElement(const PRUnichar *aName);
  nsresult HandleCharacterData(const PRUnichar *aCData, const PRUint32 aLength);
  nsresult HandleComment(const PRUnichar *aName);
  nsresult HandleProcessingInstruction(const PRUnichar *aTarget,
                                       const PRUnichar *aData);
  nsresult HandleXMLDeclaration(const PRUnichar *aVersion,
                                const PRUnichar *aEncoding,
                                PRInt32 aStandalone);
  nsresult HandleDefault(const PRUnichar *aData, const PRUint32 aLength);
  nsresult HandleStartCdataSection();
  nsresult HandleEndCdataSection();
  nsresult HandleStartDoctypeDecl(const PRUnichar* aDoctypeName,
                                  const PRUnichar* aSysid,
                                  const PRUnichar* aPubid,
                                  PRBool aHasInternalSubset);
  nsresult HandleEndDoctypeDecl();
  nsresult HandleStartNamespaceDecl(const PRUnichar* aPrefix,
                                    const PRUnichar* aUri);
  nsresult HandleEndNamespaceDecl(const PRUnichar* aPrefix);
  nsresult HandleNotationDecl(const PRUnichar* aNotationName,
                              const PRUnichar* aBase,
                              const PRUnichar* aSysid,
                              const PRUnichar* aPubid);
  nsresult HandleUnparsedEntityDecl(const PRUnichar* aEntityName,
                                    const PRUnichar* aBase,
                                    const PRUnichar* aSysid,
                                    const PRUnichar* aPubid,
                                    const PRUnichar* aNotationName);

private:
  nsresult HandleToken(CToken* aToken);

  // Load up an external stream to get external entity information
  nsresult OpenInputStreamFromExternalDTD(const PRUnichar* aFPIStr,
                                          const PRUnichar* aURLStr,
                                          const PRUnichar* aBaseURL,
                                          nsIInputStream** aStream,
                                          nsAString& aAbsURL);

  /**
   * Pass a buffer to Expat. If Expat is blocked aBuffer should be null and
   * aLength should be 0. The result of the call will be stored in
   * mInternalState. Expat will parse as much of the buffer as it can and store
   * the rest in its internal buffer.
   *
   * @param aBuffer the buffer to pass to Expat. May be null.
   * @param aLength the length of the buffer to pass to Expat (in number of
   *                PRUnichar's). Must be 0 if aBuffer is null and > 0 if
   *                aBuffer is not null.
   * @param aIsFinal whether there will definitely not be any more new buffers
   *                 passed in to ParseBuffer
   * @param aConsumed [out] the number of PRUnichars that Expat consumed. This
   *                        doesn't include the PRUnichars that Expat stored in
   *                        its buffer but didn't parse yet.
   */
  void ParseBuffer(const PRUnichar *aBuffer, PRUint32 aLength, PRBool aIsFinal,
                   PRUint32 *aConsumed);
  nsresult HandleError();

  void MaybeStopParser(nsresult aState);

  PRBool BlockedOrInterrupted()
  {
    return mInternalState == NS_ERROR_HTMLPARSER_BLOCK ||
           mInternalState == NS_ERROR_HTMLPARSER_INTERRUPTED;
  }

  XML_Parser       mExpatParser;
  nsString         mLastLine;
  nsString         mCDataText;
  // Various parts of a doctype
  nsString         mDoctypeName;
  nsString         mSystemID;
  nsString         mPublicID;
  nsString         mInternalSubset;
  PRPackedBool     mInCData;
  PRPackedBool     mInInternalSubset;
  PRPackedBool     mInExternalDTD;
  PRPackedBool     mMadeFinalCallToExpat;

  // Whether we're sure that we won't be getting more buffers to parse from
  // Necko
  PRPackedBool     mIsFinalChunk;

  nsresult         mInternalState;

  // The length of the data in Expat's buffer (in number of PRUnichars).
  PRUint32         mExpatBuffered;

  // These sinks all refer the same conceptual object. mOriginalSink is
  // identical with the nsIContentSink* passed to WillBuildModel, and exists
  // only to avoid QI-ing back to nsIContentSink*.
  nsCOMPtr<nsIContentSink> mOriginalSink;
  nsCOMPtr<nsIExpatSink> mSink;
  nsCOMPtr<nsIExtendedExpatSink> mExtendedSink;

  const nsCatalogData* mCatalogData; // weak
  nsString         mURISpec;

  // Used for error reporting.
  PRUint64         mInnerWindowID;
};

#endif
