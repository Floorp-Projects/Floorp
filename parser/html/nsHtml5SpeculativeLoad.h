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
  eSpeculativeLoadNoModuleScript,
  eSpeculativeLoadNoModuleScriptFromHead,
  eSpeculativeLoadStyle,
  eSpeculativeLoadManifest,
  eSpeculativeLoadSetDocumentCharset,
  eSpeculativeLoadSetDocumentMode,
  eSpeculativeLoadPreconnect,
  eSpeculativeLoadFont,
  eSpeculativeLoadFetch
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
    mInitTimestamp = mozilla::TimeStamp::Now();
  }

  inline void InitFont(nsHtml5String aUrl, nsHtml5String aCrossOrigin,
                       nsHtml5String aMedia, nsHtml5String aReferrerPolicy) {
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
    // This can be only triggered by <link rel=preload type=font>
    mIsLinkPreload = true;
  }

  inline void InitFetch(nsHtml5String aUrl, nsHtml5String aCrossOrigin,
                        nsHtml5String aMedia, nsHtml5String aReferrerPolicy) {
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
                         nsHtml5String aMedia, nsHtml5String aIntegrity,
                         nsHtml5String aReferrerPolicy, bool aParserInHead,
                         bool aAsync, bool aDefer, bool aNoModule,
                         bool aLinkPreload) {
    MOZ_ASSERT(mOpCode == eSpeculativeLoadUninitialized,
               "Trying to reinitialize a speculative load!");
    if (aNoModule) {
      mOpCode = aParserInHead ? eSpeculativeLoadNoModuleScriptFromHead
                              : eSpeculativeLoadNoModuleScript;
    } else {
      mOpCode = aParserInHead ? eSpeculativeLoadScriptFromHead
                              : eSpeculativeLoadScript;
    }
    aUrl.ToString(mUrlOrSizes);
    aCharset.ToString(mCharsetOrSrcset);
    aType.ToString(
        mTypeOrCharsetSourceOrDocumentModeOrMetaCSPOrSizesOrIntegrity);
    aCrossOrigin.ToString(mCrossOrigin);
    aMedia.ToString(mMedia);
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
    mTypeOrCharsetSourceOrDocumentModeOrMetaCSPOrSizesOrIntegrity.SetIsVoid(
        true);
  }

  inline void InitStyle(nsHtml5String aUrl, nsHtml5String aCharset,
                        nsHtml5String aCrossOrigin, nsHtml5String aMedia,
                        nsHtml5String aReferrerPolicy, nsHtml5String aIntegrity,
                        bool aLinkPreload) {
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
    aIntegrity.ToString(
        mTypeOrCharsetSourceOrDocumentModeOrMetaCSPOrSizesOrIntegrity);
    mIsLinkPreload = aLinkPreload;
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
   * "Speculative" charset setting isn't truly speculative. If the charset
   * is set via this operation, we are committed to it unless chardet or
   * a late meta cause a reload. The reason why a parser
   * thread-discovered charset gets communicated via the speculative load
   * queue as opposed to tree operation queue is that the charset change
   * must get processed before any actual speculative loads such as style
   * sheets. Thus, encoding decisions by the parser thread have to maintain
   * the queue order relative to true speculative loads. See bug 675499.
   */
  inline void InitSetDocumentCharset(NotNull<const Encoding*> aEncoding,
                                     int32_t aCharsetSource) {
    MOZ_ASSERT(mOpCode == eSpeculativeLoadUninitialized,
               "Trying to reinitialize a speculative load!");
    mOpCode = eSpeculativeLoadSetDocumentCharset;
    mCharsetOrSrcset.~nsString();
    mEncoding = aEncoding;
    mTypeOrCharsetSourceOrDocumentModeOrMetaCSPOrSizesOrIntegrity.Assign(
        (char16_t)aCharsetSource);
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
   * Whether the refering element has async and/or defer attributes.
   */
  bool mIsAsync;
  bool mIsDefer;

  /**
   * True if and only if this is a speculative load initiated by <link
   * rel="preload"> tag encounter.  Passed to the handling loader as an
   * indication to raise the priority.
   */
  bool mIsLinkPreload;

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
   */
  union {
    nsString mCharsetOrSrcset;
    const Encoding* mEncoding;
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
   * this will be a void string. Otherwise it is empty or the value of the
   * referrer policy. Otherwise, it is empty or the value of the type attribute.
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
   * of the "referrerpolicy" attribute. This field holds one of the values
   * (REFERRER_POLICY_*) defined in nsIHttpChannel.
   */
  mozilla::dom::ReferrerPolicy mScriptReferrerPolicy;

  mozilla::TimeStamp mInitTimestamp;
};

#endif  // nsHtml5SpeculativeLoad_h
