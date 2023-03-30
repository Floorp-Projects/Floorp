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
#include "nsIInputStream.h"
#include "nsIParser.h"
#include "nsCycleCollectionParticipant.h"
#include "nsScanner.h"

#include "rlbox_expat.h"
#include "nsRLBoxExpatDriver.h"
#include "mozilla/UniquePtr.h"

class nsIExpatSink;
struct nsCatalogData;
class RLBoxExpatSandboxData;
namespace mozilla {
template <typename, size_t>
class Array;
}

class nsExpatDriver : public nsIDTD {
  virtual ~nsExpatDriver();

 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS_FINAL
  NS_DECL_NSIDTD
  NS_DECL_CYCLE_COLLECTION_CLASS(nsExpatDriver)

  nsExpatDriver();

  nsresult Initialize(nsIURI* aURI, nsIContentSink* aSink);

  nsresult ResumeParse(nsScanner& aScanner, bool aIsFinalChunk);

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
                                          nsIURI* aBaseURI,
                                          nsIInputStream** aStream,
                                          nsIURI** aAbsURI);

  enum class ChunkOrBufferIsFinal {
    None,
    FinalChunk,
    FinalChunkAndBuffer,
  };

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
   * @param aIsFinal whether this is the last chunk in a row passed to
   *                 ParseChunk, and if so whether it's the last chunk and
   *                 buffer passed to ParseChunk (meaning there will be no more
   *                 calls to ParseChunk for the document being parsed).
   * @param aConsumed [out] the number of PRUnichars that Expat consumed. This
   *                        doesn't include the PRUnichars that Expat stored in
   *                        its buffer but didn't parse yet.
   * @param aLastLineLength [out] the length of the last line that Expat has
   *                              consumed. This will only be computed if
   *                              aIsFinal is not None or mInternalState is set
   *                              to a failure.
   */
  void ParseChunk(const char16_t* aBuffer, uint32_t aLength,
                  ChunkOrBufferIsFinal aIsFinal, uint32_t* aConsumed,
                  XML_Size* aLastLineLength);
  /**
   * Wrapper for ParseBuffer. If the buffer is too large to be copied into the
   * sandbox all at once, splits it into chunks and invokes ParseBuffer in a
   * loop.
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
   * @param aLastLineLength [out] the length of the last line that Expat has
   *                              consumed.
   */
  void ChunkAndParseBuffer(const char16_t* aBuffer, uint32_t aLength,
                           bool aIsFinal, uint32_t* aPassedToExpat,
                           uint32_t* aConsumed, XML_Size* aLastLineLength);

  nsresult HandleError();

  void MaybeStopParser(nsresult aState);

  bool BlockedOrInterrupted() {
    return mInternalState == NS_ERROR_HTMLPARSER_BLOCK ||
           mInternalState == NS_ERROR_HTMLPARSER_INTERRUPTED;
  }

  // Expat allows us to set the base URI for entities. It doesn't use the base
  // URI itself, but just passes it along to all the entity handlers (just the
  // external entity reference handler for us). It does expect the base URI as a
  // null-terminated string, with the same character type as the parsed buffers
  // (char16_t in our case). Because nsIURI stores a UTF-8 string we have to do
  // a conversion to UTF-16 for Expat. We also RLBox the Expat parser, so we
  // also do 2 copies (into RLBox sandbox, and Expat does a copy into its pool).
  // Most of the time this base URI is unused (the external entity handler is
  // rarely called), but when it is we also convert it back to a nsIURI, so we
  // convert the string back to UTF-8.
  //
  // We'd rather not do any of these conversions and copies, so we use a (hacky)
  // workaround. We store all base URIs in an array of nsIURIs. Instead of
  // passing the real URI to Expat as a string, we pass it a null-terminated
  // 2-character buffer. The first character of that buffer stores the index of
  // the corresponding nsIURI in the array (incremented with 1 because 0 is used
  // to terminate a string). The entity handler can then use the index from the
  // base URI that Expat passes it to look up the right nsIURI from the array.
  //
  // GetExpatBaseURI pushes the nsIURI to the array, and creates the
  // two-character buffer for it.
  //
  // GetBaseURI looks up the right nsIURI in the array, based on the index from
  // the two-character buffer.
  using ExpatBaseURI = mozilla::Array<XML_Char, 2>;
  ExpatBaseURI GetExpatBaseURI(nsIURI* aURI);
  nsIURI* GetBaseURI(const XML_Char* aBase) const;

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
  nsTArray<nsCOMPtr<nsIURI>> mURIs;

  // Used for error reporting.
  uint64_t mInnerWindowID;
};

class RLBoxExpatSandboxData : public mozilla::RLBoxSandboxDataBase {
  friend class RLBoxExpatSandboxPool;
  friend class nsExpatDriver;

 public:
  explicit RLBoxExpatSandboxData(uint64_t aSize)
      : mozilla::RLBoxSandboxDataBase(aSize) {
    MOZ_COUNT_CTOR(RLBoxExpatSandboxData);
  }
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
