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

#include "rlbox_expat.h"
#include "nsRLBoxExpatDriver.h"
#include "mozilla/UniquePtr.h"

class nsIExpatSink;
struct nsCatalogData;
class RLBoxExpatSandboxData;

class nsExpatDriver : public nsIDTD, public nsITokenizer {
  virtual ~nsExpatDriver();

 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSIDTD
  NS_DECL_NSITOKENIZER
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsExpatDriver, nsIDTD)

  nsExpatDriver();

  int HandleExternalEntityRef(const char16_t* aOpenEntityNames,
                              const char16_t* aBase, const char16_t* aSystemId,
                              const char16_t* aPublicId);
  static void HandleStartElement(rlbox_sandbox_expat& aSandbox,
                                 tainted_expat<void*> aUserData,
                                 tainted_expat<const char16_t*> aName,
                                 tainted_expat<const char16_t**> aAtts);
  static void HandleStartElementForSystemPrincipal(
      rlbox_sandbox_expat& aSandbox, tainted_expat<void*> aUserData,
      tainted_expat<const char16_t*> aName,
      tainted_expat<const char16_t**> aAtts);
  static void HandleEndElement(rlbox_sandbox_expat& aSandbox,
                               tainted_expat<void*> aUserData,
                               tainted_expat<const char16_t*> aName);
  static void HandleEndElementForSystemPrincipal(
      rlbox_sandbox_expat& aSandbox, tainted_expat<void*> aUserData,
      tainted_expat<const char16_t*> aName);
  nsresult HandleCharacterData(const char16_t* aCData, const uint32_t aLength);
  nsresult HandleComment(const char16_t* aName);
  nsresult HandleProcessingInstruction(const char16_t* aTarget,
                                       const char16_t* aData);
  nsresult HandleXMLDeclaration(const char16_t* aVersion,
                                const char16_t* aEncoding, int32_t aStandalone);
  nsresult HandleDefault(const char16_t* aData, const uint32_t aLength);
  nsresult HandleStartCdataSection();
  nsresult HandleEndCdataSection();
  nsresult HandleStartDoctypeDecl(const char16_t* aDoctypeName,
                                  const char16_t* aSysid,
                                  const char16_t* aPubid,
                                  bool aHasInternalSubset);
  nsresult HandleEndDoctypeDecl();

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
  void ParseBuffer(const char16_t* aBuffer, uint32_t aLength, bool aIsFinal,
                   uint32_t* aConsumed);
  nsresult HandleError();

  void MaybeStopParser(nsresult aState);

  bool BlockedOrInterrupted() {
    return mInternalState == NS_ERROR_HTMLPARSER_BLOCK ||
           mInternalState == NS_ERROR_HTMLPARSER_INTERRUPTED;
  }

  RLBoxExpatSandboxData* SandboxData() const;
  rlbox_sandbox_expat* Sandbox() const;

  // Destroy expat parser and return sandbox to pool
  void Destroy();

  mozilla::UniquePtr<mozilla::RLBoxSandboxPoolData> mSandboxPoolData;
  tainted_expat<XML_Parser> mExpatParser;

  nsString mLastLine;
  nsString mCDataText;
  // Various parts of a doctype
  nsString mDoctypeName;
  nsString mSystemID;
  nsString mPublicID;
  nsString mInternalSubset;
  bool mInCData;
  bool mInInternalSubset;
  bool mInExternalDTD;
  bool mMadeFinalCallToExpat;

  // Used to track if we're in the parser.
  bool mInParser;
  // Whether we're sure that we won't be getting more buffers to parse from
  // Necko
  bool mIsFinalChunk;

  nsresult mInternalState;

  // The length of the data in Expat's buffer (in number of PRUnichars).
  uint32_t mExpatBuffered;

  uint16_t mTagDepth;

  // These sinks all refer the same conceptual object. mOriginalSink is
  // identical with the nsIContentSink* passed to WillBuildModel, and exists
  // only to avoid QI-ing back to nsIContentSink*.
  nsCOMPtr<nsIContentSink> mOriginalSink;
  nsCOMPtr<nsIExpatSink> mSink;

  const nsCatalogData* mCatalogData;  // weak
  nsString mURISpec;

  // Used for error reporting.
  uint64_t mInnerWindowID;
};

class RLBoxExpatSandboxData : public mozilla::RLBoxSandboxDataBase {
  friend class RLBoxExpatSandboxPool;
  friend class nsExpatDriver;

 public:
  MOZ_COUNTED_DEFAULT_CTOR(RLBoxExpatSandboxData);
  ~RLBoxExpatSandboxData();
  rlbox_sandbox_expat* Sandbox() const { return mSandbox.get(); }
  // After getting a sandbox from the pool we need to register the
  // Handle{Start,End}Element callbacks and associate the driver with the
  // sandbox.
  void AttachDriver(bool IsSystemPrincipal, void* aDriver);
  void DetachDriver();

 private:
  mozilla::UniquePtr<rlbox_sandbox_expat> mSandbox;
  // Common expat callbacks that persist across calls to {Attach,Detach}Driver,
  // and consequently across sandbox reuses.
  sandbox_callback_expat<XML_XmlDeclHandler> mHandleXMLDeclaration;
  sandbox_callback_expat<XML_CharacterDataHandler> mHandleCharacterData;
  sandbox_callback_expat<XML_ProcessingInstructionHandler>
      mHandleProcessingInstruction;
  sandbox_callback_expat<XML_DefaultHandler> mHandleDefault;
  sandbox_callback_expat<XML_ExternalEntityRefHandler> mHandleExternalEntityRef;
  sandbox_callback_expat<XML_CommentHandler> mHandleComment;
  sandbox_callback_expat<XML_StartCdataSectionHandler> mHandleStartCdataSection;
  sandbox_callback_expat<XML_EndCdataSectionHandler> mHandleEndCdataSection;
  sandbox_callback_expat<XML_StartDoctypeDeclHandler> mHandleStartDoctypeDecl;
  sandbox_callback_expat<XML_EndDoctypeDeclHandler> mHandleEndDoctypeDecl;
  // Expat callbacks specific to each driver, and thus (re)set across sandbox
  // reuses.
  sandbox_callback_expat<XML_StartElementHandler> mHandleStartElement;
  sandbox_callback_expat<XML_EndElementHandler> mHandleEndElement;
};

#endif
