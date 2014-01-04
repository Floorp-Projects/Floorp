/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

  int HandleExternalEntityRef(const char16_t *aOpenEntityNames,
                              const char16_t *aBase,
                              const char16_t *aSystemId,
                              const char16_t *aPublicId);
  nsresult HandleStartElement(const char16_t *aName, const char16_t **aAtts);
  nsresult HandleEndElement(const char16_t *aName);
  nsresult HandleCharacterData(const char16_t *aCData, const uint32_t aLength);
  nsresult HandleComment(const char16_t *aName);
  nsresult HandleProcessingInstruction(const char16_t *aTarget,
                                       const char16_t *aData);
  nsresult HandleXMLDeclaration(const char16_t *aVersion,
                                const char16_t *aEncoding,
                                int32_t aStandalone);
  nsresult HandleDefault(const char16_t *aData, const uint32_t aLength);
  nsresult HandleStartCdataSection();
  nsresult HandleEndCdataSection();
  nsresult HandleStartDoctypeDecl(const char16_t* aDoctypeName,
                                  const char16_t* aSysid,
                                  const char16_t* aPubid,
                                  bool aHasInternalSubset);
  nsresult HandleEndDoctypeDecl();
  nsresult HandleStartNamespaceDecl(const char16_t* aPrefix,
                                    const char16_t* aUri);
  nsresult HandleEndNamespaceDecl(const char16_t* aPrefix);
  nsresult HandleNotationDecl(const char16_t* aNotationName,
                              const char16_t* aBase,
                              const char16_t* aSysid,
                              const char16_t* aPubid);
  nsresult HandleUnparsedEntityDecl(const char16_t* aEntityName,
                                    const char16_t* aBase,
                                    const char16_t* aSysid,
                                    const char16_t* aPubid,
                                    const char16_t* aNotationName);

private:
  // Load up an external stream to get external entity information
  nsresult OpenInputStreamFromExternalDTD(const char16_t* aFPIStr,
                                          const char16_t* aURLStr,
                                          const char16_t* aBaseURL,
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
   *                char16_t's). Must be 0 if aBuffer is null and > 0 if
   *                aBuffer is not null.
   * @param aIsFinal whether there will definitely not be any more new buffers
   *                 passed in to ParseBuffer
   * @param aConsumed [out] the number of PRUnichars that Expat consumed. This
   *                        doesn't include the PRUnichars that Expat stored in
   *                        its buffer but didn't parse yet.
   */
  void ParseBuffer(const char16_t *aBuffer, uint32_t aLength, bool aIsFinal,
                   uint32_t *aConsumed);
  nsresult HandleError();

  void MaybeStopParser(nsresult aState);

  bool BlockedOrInterrupted()
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
  bool             mInCData;
  bool             mInInternalSubset;
  bool             mInExternalDTD;
  bool             mMadeFinalCallToExpat;

  // Whether we're sure that we won't be getting more buffers to parse from
  // Necko
  bool             mIsFinalChunk;

  nsresult         mInternalState;

  // The length of the data in Expat's buffer (in number of PRUnichars).
  uint32_t         mExpatBuffered;

  // These sinks all refer the same conceptual object. mOriginalSink is
  // identical with the nsIContentSink* passed to WillBuildModel, and exists
  // only to avoid QI-ing back to nsIContentSink*.
  nsCOMPtr<nsIContentSink> mOriginalSink;
  nsCOMPtr<nsIExpatSink> mSink;
  nsCOMPtr<nsIExtendedExpatSink> mExtendedSink;

  const nsCatalogData* mCatalogData; // weak
  nsString         mURISpec;

  // Used for error reporting.
  uint64_t         mInnerWindowID;
};

#endif
