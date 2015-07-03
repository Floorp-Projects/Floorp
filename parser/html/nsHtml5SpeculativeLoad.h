/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 
#ifndef nsHtml5SpeculativeLoad_h
#define nsHtml5SpeculativeLoad_h

#include "nsString.h"
#include "nsContentUtils.h"

class nsHtml5TreeOpExecutor;

enum eHtml5SpeculativeLoad {
#ifdef DEBUG
  eSpeculativeLoadUninitialized,
#endif
  eSpeculativeLoadBase,
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
  eSpeculativeLoadPreconnect
};

class nsHtml5SpeculativeLoad {
  public:
    nsHtml5SpeculativeLoad();
    ~nsHtml5SpeculativeLoad();

    inline void InitBase(const nsAString& aUrl)
    {
      NS_PRECONDITION(mOpCode == eSpeculativeLoadUninitialized,
                      "Trying to reinitialize a speculative load!");
      mOpCode = eSpeculativeLoadBase;
      mUrl.Assign(aUrl);
    }

    inline void InitMetaReferrerPolicy(const nsAString& aReferrerPolicy) {
      NS_PRECONDITION(mOpCode == eSpeculativeLoadUninitialized,
                      "Trying to reinitialize a speculative load!");
      mOpCode = eSpeculativeLoadMetaReferrer;
      mReferrerPolicy.Assign(
        nsContentUtils::TrimWhitespace<nsContentUtils::IsHTMLWhitespace>(aReferrerPolicy));
    }

    inline void InitImage(const nsAString& aUrl,
                          const nsAString& aCrossOrigin,
                          const nsAString& aReferrerPolicy,
                          const nsAString& aSrcset,
                          const nsAString& aSizes)
    {
      NS_PRECONDITION(mOpCode == eSpeculativeLoadUninitialized,
                      "Trying to reinitialize a speculative load!");
      mOpCode = eSpeculativeLoadImage;
      mUrl.Assign(aUrl);
      mCrossOrigin.Assign(aCrossOrigin);
      mReferrerPolicy.Assign(
        nsContentUtils::TrimWhitespace<nsContentUtils::IsHTMLWhitespace>(aReferrerPolicy));
      mSrcset.Assign(aSrcset);
      mSizes.Assign(aSizes);
    }

    // <picture> elements have multiple <source> nodes followed by an <img>,
    // where we use the first valid source, which may be the img. Because we
    // can't determine validity at this point without parsing CSS and getting
    // main thread state, we push preload operations for picture pushed and
    // popped, so that the target of the preload ops can determine what picture
    // and nesting level each source/img from the main preloading code exists
    // at.
    inline void InitOpenPicture()
    {
      NS_PRECONDITION(mOpCode == eSpeculativeLoadUninitialized,
                      "Trying to reinitialize a speculative load!");
      mOpCode = eSpeculativeLoadOpenPicture;
    }

    inline void InitEndPicture()
    {
      NS_PRECONDITION(mOpCode == eSpeculativeLoadUninitialized,
                      "Trying to reinitialize a speculative load!");
      mOpCode = eSpeculativeLoadEndPicture;
    }

    inline void InitPictureSource(const nsAString& aSrcset,
                                  const nsAString& aSizes,
                                  const nsAString& aType,
                                  const nsAString& aMedia)
    {
      NS_PRECONDITION(mOpCode == eSpeculativeLoadUninitialized,
                      "Trying to reinitialize a speculative load!");
      mOpCode = eSpeculativeLoadPictureSource;
      mSrcset.Assign(aSrcset);
      mSizes.Assign(aSizes);
      mTypeOrCharsetSource.Assign(aType);
      mMedia.Assign(aMedia);
    }

    inline void InitScript(const nsAString& aUrl,
                           const nsAString& aCharset,
                           const nsAString& aType,
                           const nsAString& aCrossOrigin,
                           bool aParserInHead)
    {
      NS_PRECONDITION(mOpCode == eSpeculativeLoadUninitialized,
                      "Trying to reinitialize a speculative load!");
      mOpCode = aParserInHead ?
          eSpeculativeLoadScriptFromHead : eSpeculativeLoadScript;
      mUrl.Assign(aUrl);
      mCharset.Assign(aCharset);
      mTypeOrCharsetSource.Assign(aType);
      mCrossOrigin.Assign(aCrossOrigin);
    }

    inline void InitStyle(const nsAString& aUrl, const nsAString& aCharset,
                          const nsAString& aCrossOrigin)
    {
      NS_PRECONDITION(mOpCode == eSpeculativeLoadUninitialized,
                      "Trying to reinitialize a speculative load!");
      mOpCode = eSpeculativeLoadStyle;
      mUrl.Assign(aUrl);
      mCharset.Assign(aCharset);
      mCrossOrigin.Assign(aCrossOrigin);
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
    inline void InitManifest(const nsAString& aUrl)
    {
      NS_PRECONDITION(mOpCode == eSpeculativeLoadUninitialized,
                      "Trying to reinitialize a speculative load!");
      mOpCode = eSpeculativeLoadManifest;
      mUrl.Assign(aUrl);
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
    inline void InitSetDocumentCharset(nsACString& aCharset,
                                       int32_t aCharsetSource)
    {
      NS_PRECONDITION(mOpCode == eSpeculativeLoadUninitialized,
                      "Trying to reinitialize a speculative load!");
      mOpCode = eSpeculativeLoadSetDocumentCharset;
      CopyUTF8toUTF16(aCharset, mCharset);
      mTypeOrCharsetSource.Assign((char16_t)aCharsetSource);
    }

    inline void InitPreconnect(const nsAString& aUrl,
                               const nsAString& aCrossOrigin)
    {
      NS_PRECONDITION(mOpCode == eSpeculativeLoadUninitialized,
                      "Trying to reinitialize a speculative load!");
      mOpCode = eSpeculativeLoadPreconnect;
      mUrl.Assign(aUrl);
      mCrossOrigin.Assign(aCrossOrigin);
    }

    void Perform(nsHtml5TreeOpExecutor* aExecutor);

  private:
    eHtml5SpeculativeLoad mOpCode;
    nsString mUrl;
    nsString mReferrerPolicy;
    /**
     * If mOpCode is eSpeculativeLoadStyle or eSpeculativeLoadScript[FromHead]
     * then this is the value of the "charset" attribute. For
     * eSpeculativeLoadSetDocumentCharset it is the charset that the
     * document's charset is being set to. Otherwise it's empty.
     */
    nsString mCharset;
    /**
     * If mOpCode is eSpeculativeLoadSetDocumentCharset, this is a
     * one-character string whose single character's code point is to be
     * interpreted as a charset source integer. Otherwise, it is empty or
     * the value of the type attribute.
     */
    nsString mTypeOrCharsetSource;
    /**
     * If mOpCode is eSpeculativeLoadImage or eSpeculativeLoadScript[FromHead]
     * or eSpeculativeLoadPreconnect this is the value of the "crossorigin"
     * attribute.  If the attribute is not set, this will be a void string.
     */
    nsString mCrossOrigin;
    /**
     * If mOpCode is eSpeculativeLoadImage or eSpeculativeLoadPictureSource,
     * this is the value of "srcset" attribute.  If the attribute is not set,
     * this will be a void string.
     */
    nsString mSrcset;
    /**
     * If mOpCode is eSpeculativeLoadPictureSource, this is the value of "sizes"
     * attribute.  If the attribute is not set, this will be a void string.
     */
    nsString mSizes;
    /**
     * If mOpCode is eSpeculativeLoadPictureSource, this is the value of "media"
     * attribute.  If the attribute is not set, this will be a void string.
     */
    nsString mMedia;
};

#endif // nsHtml5SpeculativeLoad_h
