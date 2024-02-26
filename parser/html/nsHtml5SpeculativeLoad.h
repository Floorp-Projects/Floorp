/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHtml5SpeculativeLoad_h
#define nsHtml5SpeculativeLoad_h

#include "nsString.h"
#include "nsContentUtils.h"
#include "nsHtml5DocumentMode.h"
#include "nsHtml5String.h"
#include "ReferrerInfo.h"

class nsHtml5TreeOpExecutor;

enum eHtml5SpeculativeLoad {
  eSpeculativeLoadUninitialized,
  eSpeculativeLoadBase,
  eSpeculativeLoadCSP,
  eSpeculativeLoadMetaReferrer,
  eSpeculativeLoadImage,
  eSpeculativeLoadOpenPicture,
  eSpeculativeLoadEndPicture,
  eSpeculativeLoadPictureSource,
  eSpeculativeLoadScript,
  eSpeculativeLoadScriptFromHead,
  eSpeculativeLoadStyle,
  eSpeculativeLoadManifest,
  eSpeculativeLoadSetDocumentCharset,
  eSpeculativeLoadSetDocumentMode,
  eSpeculativeLoadPreconnect,
  eSpeculativeLoadFont,
  eSpeculativeLoadFetch,
  eSpeculativeLoadMaybeComplainAboutCharset
};

class nsHtml5SpeculativeLoad {
  using Encoding = mozilla::Encoding;
  template <typename T>
  using NotNull = mozilla::NotNull<T>;

 public:
  nsHtml5SpeculativeLoad();
  ~nsHtml5SpeculativeLoad();

  inline void InitBase(nsHtml5String aUrl) {
    MOZ_ASSERT(mOpCode == eSpeculativeLoadUninitialized,
               "Trying to reinitialize a speculative load!");
    mOpCode = eSpeculativeLoadBase;
    aUrl.ToString(mUrlOrSizes);
  }

  inline void InitMetaCSP(nsHtml5String aCSP) {
    MOZ_ASSERT(mOpCode == eSpeculativeLoadUninitialized,
               "Trying to reinitialize a speculative load!");
    mOpCode = eSpeculativeLoadCSP;
    nsString csp;  // Not Auto, because using it to hold nsStringBuffer*
    aCSP.ToString(csp);
    mTypeOrCharsetSourceOrDocumentModeOrMetaCSPOrSizesOrIntegrity.Assign(
        nsContentUtils::TrimWhitespace<nsContentUtils::IsHTMLWhitespace>(csp));
  }

  inline void InitMetaReferrerPolicy(nsHtml5String aReferrerPolicy) {
    MOZ_ASSERT(mOpCode == eSpeculativeLoadUninitialized,
               "Trying to reinitialize a speculative load!");
    mOpCode = eSpeculativeLoadMetaReferrer;
    nsString
        referrerPolicy;  // Not Auto, because using it to hold nsStringBuffer*
    aReferrerPolicy.ToString(referrerPolicy);
    mReferrerPolicyOrIntegrity.Assign(
        nsContentUtils::TrimWhitespace<nsContentUtils::IsHTMLWhitespace>(
            referrerPolicy));
  }

  inline void InitImage(nsHtml5String aUrl, nsHtml5String aCrossOrigin,
                        nsHtml5String aMedia, nsHtml5String aReferrerPolicy,
                        nsHtml5String aSrcset, nsHtml5String aSizes,
                        bool aLinkPreload) {
    MOZ_ASSERT(mOpCode == eSpeculativeLoadUninitialized,
               "Trying to reinitialize a speculative load!");
    mOpCode = eSpeculativeLoadImage;
    aUrl.ToString(mUrlOrSizes);
    aCrossOrigin.ToString(mCrossOrigin);
    aMedia.ToString(mMedia);
    nsString
        referrerPolicy;  // Not Auto, because using it to hold nsStringBuffer*
    aReferrerPolicy.ToString(referrerPolicy);
    mReferrerPolicyOrIntegrity.Assign(
        nsContentUtils::TrimWhitespace<nsContentUtils::IsHTMLWhitespace>(
            referrerPolicy));
    aSrcset.ToString(mCharsetOrSrcset);
    aSizes.ToString(
        mTypeOrCharsetSourceOrDocumentModeOrMetaCSPOrSizesOrIntegrity);
    mIsLinkPreload = aLinkPreload;
  }

  inline void InitFont(nsHtml5String aUrl, nsHtml5String aCrossOrigin,
                       nsHtml5String aMedia, nsHtml5String aReferrerPolicy,
                       nsHtml5String aFetchPriority) {
    MOZ_ASSERT(mOpCode == eSpeculativeLoadUninitialized,
               "Trying to reinitialize a speculative load!");
    mOpCode = eSpeculativeLoadFont;
    aUrl.ToString(mUrlOrSizes);
    aCrossOrigin.ToString(mCrossOrigin);
    aMedia.ToString(mMedia);
    nsString
        referrerPolicy;  // Not Auto, because using it to hold nsStringBuffer*
    aReferrerPolicy.ToString(referrerPolicy);
    mReferrerPolicyOrIntegrity.Assign(
        nsContentUtils::TrimWhitespace<nsContentUtils::IsHTMLWhitespace>(
            referrerPolicy));
    aFetchPriority.ToString(mFetchPriority);
    // This can be only triggered by <link rel=preload type=font>
    mIsLinkPreload = true;
  }

  inline void InitFetch(nsHtml5String aUrl, nsHtml5String aCrossOrigin,
                        nsHtml5String aMedia, nsHtml5String aReferrerPolicy,
                        nsHtml5String aFetchPriority) {
    MOZ_ASSERT(mOpCode == eSpeculativeLoadUninitialized,
               "Trying to reinitialize a speculative load!");
    mOpCode = eSpeculativeLoadFetch;
    aUrl.ToString(mUrlOrSizes);
    aCrossOrigin.ToString(mCrossOrigin);
    aMedia.ToString(mMedia);
    nsString
        referrerPolicy;  // Not Auto, because using it to hold nsStringBuffer*
    aReferrerPolicy.ToString(referrerPolicy);
    mReferrerPolicyOrIntegrity.Assign(
        nsContentUtils::TrimWhitespace<nsContentUtils::IsHTMLWhitespace>(
            referrerPolicy));
    aFetchPriority.ToString(mFetchPriority);

    // This method can be only be triggered by <link rel=preload type=fetch>,
    // hence this operation is always a preload.
    mIsLinkPreload = true;
  }

  // <picture> elements have multiple <source> nodes followed by an <img>,
  // where we use the first valid source, which may be the img. Because we
  // can't determine validity at this point without parsing CSS and getting
  // main thread state, we push preload operations for picture pushed and
  // popped, so that the target of the preload ops can determine what picture
  // and nesting level each source/img from the main preloading code exists
  // at.
  inline void InitOpenPicture() {
    MOZ_ASSERT(mOpCode == eSpeculativeLoadUninitialized,
               "Trying to reinitialize a speculative load!");
    mOpCode = eSpeculativeLoadOpenPicture;
  }

  inline void InitEndPicture() {
    MOZ_ASSERT(mOpCode == eSpeculativeLoadUninitialized,
               "Trying to reinitialize a speculative load!");
    mOpCode = eSpeculativeLoadEndPicture;
  }

  inline void InitPictureSource(nsHtml5String aSrcset, nsHtml5String aSizes,
                                nsHtml5String aType, nsHtml5String aMedia) {
    MOZ_ASSERT(mOpCode == eSpeculativeLoadUninitialized,
               "Trying to reinitialize a speculative load!");
    mOpCode = eSpeculativeLoadPictureSource;
    aSrcset.ToString(mCharsetOrSrcset);
    aSizes.ToString(mUrlOrSizes);
    aType.ToString(
        mTypeOrCharsetSourceOrDocumentModeOrMetaCSPOrSizesOrIntegrity);
    aMedia.ToString(mMedia);
  }

  inline void InitScript(nsHtml5String aUrl, nsHtml5String aCharset,
                         nsHtml5String aType, nsHtml5String aCrossOrigin,
                         nsHtml5String aMedia, nsHtml5String aNonce,
                         nsHtml5String aFetchPriority, nsHtml5String aIntegrity,
                         nsHtml5String aReferrerPolicy, bool aParserInHead,
                         bool aAsync, bool aDefer, bool aLinkPreload) {
    MOZ_ASSERT(mOpCode == eSpeculativeLoadUninitialized,
               "Trying to reinitialize a speculative load!");
    mOpCode =
        aParserInHead ? eSpeculativeLoadScriptFromHead : eSpeculativeLoadScript;
    aUrl.ToString(mUrlOrSizes);
    aCharset.ToString(mCharsetOrSrcset);
    aType.ToString(
        mTypeOrCharsetSourceOrDocumentModeOrMetaCSPOrSizesOrIntegrity);
    aCrossOrigin.ToString(mCrossOrigin);
    aMedia.ToString(mMedia);
    aNonce.ToString(mNonce);
    aFetchPriority.ToString(mFetchPriority);
    aIntegrity.ToString(mReferrerPolicyOrIntegrity);
    nsAutoString referrerPolicy;
    aReferrerPolicy.ToString(referrerPolicy);
    referrerPolicy =
        nsContentUtils::TrimWhitespace<nsContentUtils::IsHTMLWhitespace>(
            referrerPolicy);
    mScriptReferrerPolicy =
        mozilla::dom::ReferrerInfo::ReferrerPolicyAttributeFromString(
            referrerPolicy);

    mIsAsync = aAsync;
    mIsDefer = aDefer;
    mIsLinkPreload = aLinkPreload;
  }

  inline void InitImportStyle(nsString&& aUrl) {
    MOZ_ASSERT(mOpCode == eSpeculativeLoadUninitialized,
               "Trying to reinitialize a speculative load!");
    mOpCode = eSpeculativeLoadStyle;
    mUrlOrSizes = std::move(aUrl);
    mCharsetOrSrcset.SetIsVoid(true);
    mCrossOrigin.SetIsVoid(true);
    mMedia.SetIsVoid(true);
    mReferrerPolicyOrIntegrity.SetIsVoid(true);
    mNonce.SetIsVoid(true);
    mTypeOrCharsetSourceOrDocumentModeOrMetaCSPOrSizesOrIntegrity.SetIsVoid(
        true);
  }

  inline void InitStyle(nsHtml5String aUrl, nsHtml5String aCharset,
                        nsHtml5String aCrossOrigin, nsHtml5String aMedia,
                        nsHtml5String aReferrerPolicy, nsHtml5String aNonce,
                        nsHtml5String aIntegrity, bool aLinkPreload,
                        nsHtml5String aFetchPriority) {
    MOZ_ASSERT(mOpCode == eSpeculativeLoadUninitialized,
               "Trying to reinitialize a speculative load!");
    mOpCode = eSpeculativeLoadStyle;
    aUrl.ToString(mUrlOrSizes);
    aCharset.ToString(mCharsetOrSrcset);
    aCrossOrigin.ToString(mCrossOrigin);
    aMedia.ToString(mMedia);
    nsString
        referrerPolicy;  // Not Auto, because using it to hold nsStringBuffer*
    aReferrerPolicy.ToString(referrerPolicy);
    mReferrerPolicyOrIntegrity.Assign(
        nsContentUtils::TrimWhitespace<nsContentUtils::IsHTMLWhitespace>(
            referrerPolicy));
    aNonce.ToString(mNonce);
    aIntegrity.ToString(
        mTypeOrCharsetSourceOrDocumentModeOrMetaCSPOrSizesOrIntegrity);
    mIsLinkPreload = aLinkPreload;
    aFetchPriority.ToString(mFetchPriority);
  }

  /**
   * "Speculative" manifest loads aren't truly speculative--if a manifest
   * gets loaded, we are committed to it. There can never be a <script>
   * before the manifest, so the situation of having to undo a manifest due
   * to document.write() never arises. The reason why a parser
   * thread-discovered manifest gets loaded via the speculative load queue
   * as opposed to tree operation queue is that the manifest must get
   * processed before any actual speculative loads such as scripts. Thus,
   * manifests seen by the parser thread have to maintain the queue order
   * relative to true speculative loads. See bug 541079.
   */
  inline void InitManifest(nsHtml5String aUrl) {
    MOZ_ASSERT(mOpCode == eSpeculativeLoadUninitialized,
               "Trying to reinitialize a speculative load!");
    mOpCode = eSpeculativeLoadManifest;
    aUrl.ToString(mUrlOrSizes);
  }

  /**
   * We communicate the encoding change via the speculative operation
   * queue in order to act upon it as soon as possible and so as not to
   * have speculative loads generated after an encoding change fail to
   * make use of the encoding change.
   */
  inline void InitSetDocumentCharset(NotNull<const Encoding*> aEncoding,
                                     int32_t aCharsetSource,
                                     bool aCommitEncodingSpeculation) {
    MOZ_ASSERT(mOpCode == eSpeculativeLoadUninitialized,
               "Trying to reinitialize a speculative load!");
    mOpCode = eSpeculativeLoadSetDocumentCharset;
    mCharsetOrSrcset.~nsString();
    mEncoding = aEncoding;
    mTypeOrCharsetSourceOrDocumentModeOrMetaCSPOrSizesOrIntegrity.Assign(
        (char16_t)aCharsetSource);
    mCommitEncodingSpeculation = aCommitEncodingSpeculation;
  }

  inline void InitMaybeComplainAboutCharset(const char* aMsgId, bool aError,
                                            int32_t aLineNumber) {
    MOZ_ASSERT(mOpCode == eSpeculativeLoadUninitialized,
               "Trying to reinitialize a speculative load!");
    mOpCode = eSpeculativeLoadMaybeComplainAboutCharset;
    mCharsetOrSrcset.~nsString();
    mMsgId = aMsgId;
    mIsError = aError;
    // Transport a 32-bit integer as two 16-bit code units of a string
    // in order to avoid adding an integer field to the object.
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=1733043 for a better
    // eventual approach.
    char16_t high = (char16_t)(((uint32_t)aLineNumber) >> 16);
    char16_t low = (char16_t)(((uint32_t)aLineNumber) & 0xFFFF);
    mTypeOrCharsetSourceOrDocumentModeOrMetaCSPOrSizesOrIntegrity.Assign(high);
    mTypeOrCharsetSourceOrDocumentModeOrMetaCSPOrSizesOrIntegrity.Append(low);
  }

  /**
   * Speculative document mode setting isn't really speculative. Once it
   * happens, we are committed to it. However, this information needs to
   * travel in the speculation queue in order to have this information
   * available before parsing the speculatively loaded style sheets.
   */
  inline void InitSetDocumentMode(nsHtml5DocumentMode aMode) {
    MOZ_ASSERT(mOpCode == eSpeculativeLoadUninitialized,
               "Trying to reinitialize a speculative load!");
    mOpCode = eSpeculativeLoadSetDocumentMode;
    mTypeOrCharsetSourceOrDocumentModeOrMetaCSPOrSizesOrIntegrity.Assign(
        (char16_t)aMode);
  }

  inline void InitPreconnect(nsHtml5String aUrl, nsHtml5String aCrossOrigin) {
    MOZ_ASSERT(mOpCode == eSpeculativeLoadUninitialized,
               "Trying to reinitialize a speculative load!");
    mOpCode = eSpeculativeLoadPreconnect;
    aUrl.ToString(mUrlOrSizes);
    aCrossOrigin.ToString(mCrossOrigin);
  }

  void Perform(nsHtml5TreeOpExecutor* aExecutor);

 private:
  nsHtml5SpeculativeLoad(const nsHtml5SpeculativeLoad&) = delete;
  nsHtml5SpeculativeLoad& operator=(const nsHtml5SpeculativeLoad&) = delete;

  eHtml5SpeculativeLoad mOpCode;

  /**
   * Whether the refering element has async attribute.
   */
  bool mIsAsync;

  /**
   * Whether the refering element has defer attribute.
   */
  bool mIsDefer;

  /**
   * True if and only if this is a speculative load initiated by <link
   * rel="preload"> or <link rel="modulepreload"> tag encounter.  Passed to the
   * handling loader as an indication to raise the priority.
   */
  bool mIsLinkPreload;

  /**
   * Whether the charset complaint is an error.
   */
  bool mIsError;

  /**
   * Whether setting document encoding involves also committing to an encoding
   * speculation.
   */
  bool mCommitEncodingSpeculation;

  /* If mOpCode is eSpeculativeLoadPictureSource, this is the value of the
   * "sizes" attribute. If the attribute is not set, this will be a void
   * string. Otherwise it empty or the value of the url.
   */
  nsString mUrlOrSizes;
  /**
   * If mOpCode is eSpeculativeLoadScript[FromHead], this is the value of the
   * "integrity" attribute. If the attribute is not set, this will be a void
   * string. Otherwise it is empty or the value of the referrer policy.
   */
  nsString mReferrerPolicyOrIntegrity;
  /**
   * If mOpCode is eSpeculativeLoadStyle or eSpeculativeLoadScript[FromHead]
   * then this is the value of the "charset" attribute. For
   * eSpeculativeLoadSetDocumentCharset it is the charset that the
   * document's charset is being set to. If mOpCode is eSpeculativeLoadImage
   * or eSpeculativeLoadPictureSource, this is the value of the "srcset"
   * attribute. If the attribute is not set, this will be a void string.
   * Otherwise it's empty.
   * For eSpeculativeLoadMaybeComplainAboutCharset mMsgId is used.
   */
  union {
    nsString mCharsetOrSrcset;
    const Encoding* mEncoding;
    const char* mMsgId;
  };
  /**
   * If mOpCode is eSpeculativeLoadSetDocumentCharset, this is a
   * one-character string whose single character's code point is to be
   * interpreted as a charset source integer. If mOpCode is
   * eSpeculativeLoadSetDocumentMode, this is a one-character string whose
   * single character's code point is to be interpreted as an
   * nsHtml5DocumentMode. If mOpCode is eSpeculativeLoadCSP, this is a meta
   * element's CSP value. If mOpCode is eSpeculativeLoadImage, this is the
   * value of the "sizes" attribute. If the attribute is not set, this will
   * be a void string. If mOpCode is eSpeculativeLoadStyle, this
   * is the value of the "integrity" attribute. If the attribute is not set,
   * this will be a void string. Otherwise, it is empty or the value of the type
   * attribute.
   */
  nsString mTypeOrCharsetSourceOrDocumentModeOrMetaCSPOrSizesOrIntegrity;
  /**
   * If mOpCode is eSpeculativeLoadImage or eSpeculativeLoadScript[FromHead]
   * or eSpeculativeLoadPreconnect or eSpeculativeLoadStyle this is the value of
   * the "crossorigin" attribute.  If the attribute is not set, this will be a
   * void string.
   */
  nsString mCrossOrigin;
  /**
   * If mOpCode is eSpeculativeLoadPictureSource or eSpeculativeLoadStyle or
   * Fetch or Image or Media or Script this is the value of the relevant "media"
   * attribute of the <link rel="preload"> or <link rel="stylesheet">. If the
   * attribute is not set, or the preload didn't originate from a <link>, this
   * will be a void string.
   */
  nsString mMedia;
  /**
   * If mOpCode is eSpeculativeLoadScript[FromHead] this represents the value
   * of the "nonce" attribute.
   */
  nsString mNonce;
  /**
   * If mOpCode is eSpeculativeLoadNoModuleScript[FromHead] or
   * eSpeculativeLoadScript[FromHead] this represents the value of the
   * "fetchpriority" attribute.
   */
  nsString mFetchPriority;
  /**
   * If mOpCode is eSpeculativeLoadScript[FromHead] this represents the value
   * of the "referrerpolicy" attribute. This field holds one of the values
   * (REFERRER_POLICY_*) defined in nsIHttpChannel.
   */
  mozilla::dom::ReferrerPolicy mScriptReferrerPolicy;
};

#endif  // nsHtml5SpeculativeLoad_h
