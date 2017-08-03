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
  eSpeculativeLoadPreconnect
};

class nsHtml5SpeculativeLoad {
  public:
    nsHtml5SpeculativeLoad();
    ~nsHtml5SpeculativeLoad();

    inline void InitBase(nsHtml5String aUrl)
    {
      NS_PRECONDITION(mOpCode == eSpeculativeLoadUninitialized,
                      "Trying to reinitialize a speculative load!");
      mOpCode = eSpeculativeLoadBase;
      aUrl.ToString(mUrl);
    }

    inline void InitMetaCSP(nsHtml5String aCSP)
    {
      NS_PRECONDITION(mOpCode == eSpeculativeLoadUninitialized,
                      "Trying to reinitialize a speculative load!");
      mOpCode = eSpeculativeLoadCSP;
      nsString csp; // Not Auto, because using it to hold nsStringBuffer*
      aCSP.ToString(csp);
      mMetaCSP.Assign(
        nsContentUtils::TrimWhitespace<nsContentUtils::IsHTMLWhitespace>(csp));
    }

    inline void InitMetaReferrerPolicy(nsHtml5String aReferrerPolicy)
    {
      NS_PRECONDITION(mOpCode == eSpeculativeLoadUninitialized,
                      "Trying to reinitialize a speculative load!");
      mOpCode = eSpeculativeLoadMetaReferrer;
      nsString
        referrerPolicy; // Not Auto, because using it to hold nsStringBuffer*
      aReferrerPolicy.ToString(referrerPolicy);
      mReferrerPolicy.Assign(
        nsContentUtils::TrimWhitespace<nsContentUtils::IsHTMLWhitespace>(
          referrerPolicy));
    }

    inline void InitImage(nsHtml5String aUrl,
                          nsHtml5String aCrossOrigin,
                          nsHtml5String aReferrerPolicy,
                          nsHtml5String aSrcset,
                          nsHtml5String aSizes)
    {
      NS_PRECONDITION(mOpCode == eSpeculativeLoadUninitialized,
                      "Trying to reinitialize a speculative load!");
      mOpCode = eSpeculativeLoadImage;
      aUrl.ToString(mUrl);
      aCrossOrigin.ToString(mCrossOrigin);
      nsString
        referrerPolicy; // Not Auto, because using it to hold nsStringBuffer*
      aReferrerPolicy.ToString(referrerPolicy);
      mReferrerPolicy.Assign(
        nsContentUtils::TrimWhitespace<nsContentUtils::IsHTMLWhitespace>(
          referrerPolicy));
      aSrcset.ToString(mSrcset);
      aSizes.ToString(mSizes);
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

    inline void InitPictureSource(nsHtml5String aSrcset,
                                  nsHtml5String aSizes,
                                  nsHtml5String aType,
                                  nsHtml5String aMedia)
    {
      NS_PRECONDITION(mOpCode == eSpeculativeLoadUninitialized,
                      "Trying to reinitialize a speculative load!");
      mOpCode = eSpeculativeLoadPictureSource;
      aSrcset.ToString(mSrcset);
      aSizes.ToString(mSizes);
      aType.ToString(mTypeOrCharsetSourceOrDocumentMode);
      aMedia.ToString(mMedia);
    }

    inline void InitScript(nsHtml5String aUrl,
                           nsHtml5String aCharset,
                           nsHtml5String aType,
                           nsHtml5String aCrossOrigin,
                           nsHtml5String aIntegrity,
                           bool aParserInHead)
    {
      NS_PRECONDITION(mOpCode == eSpeculativeLoadUninitialized,
                      "Trying to reinitialize a speculative load!");
      mOpCode = aParserInHead ?
          eSpeculativeLoadScriptFromHead : eSpeculativeLoadScript;
      aUrl.ToString(mUrl);
      aCharset.ToString(mCharset);
      aType.ToString(mTypeOrCharsetSourceOrDocumentMode);
      aCrossOrigin.ToString(mCrossOrigin);
      aIntegrity.ToString(mIntegrity);
    }

    inline void InitStyle(nsHtml5String aUrl,
                          nsHtml5String aCharset,
                          nsHtml5String aCrossOrigin,
                          nsHtml5String aIntegrity)
    {
      NS_PRECONDITION(mOpCode == eSpeculativeLoadUninitialized,
                      "Trying to reinitialize a speculative load!");
      mOpCode = eSpeculativeLoadStyle;
      aUrl.ToString(mUrl);
      aCharset.ToString(mCharset);
      aCrossOrigin.ToString(mCrossOrigin);
      aIntegrity.ToString(mIntegrity);
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
    inline void InitManifest(nsHtml5String aUrl)
    {
      NS_PRECONDITION(mOpCode == eSpeculativeLoadUninitialized,
                      "Trying to reinitialize a speculative load!");
      mOpCode = eSpeculativeLoadManifest;
      aUrl.ToString(mUrl);
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
      mTypeOrCharsetSourceOrDocumentMode.Assign((char16_t)aCharsetSource);
    }

    /**
     * Speculative document mode setting isn't really speculative. Once it
     * happens, we are committed to it. However, this information needs to
     * travel in the speculation queue in order to have this information
     * available before parsing the speculatively loaded style sheets.
     */
    inline void InitSetDocumentMode(nsHtml5DocumentMode aMode)
    {
      NS_PRECONDITION(mOpCode == eSpeculativeLoadUninitialized,
                      "Trying to reinitialize a speculative load!");
      mOpCode = eSpeculativeLoadSetDocumentMode;
      mTypeOrCharsetSourceOrDocumentMode.Assign((char16_t)aMode);
    }

    inline void InitPreconnect(nsHtml5String aUrl, nsHtml5String aCrossOrigin)
    {
      NS_PRECONDITION(mOpCode == eSpeculativeLoadUninitialized,
                      "Trying to reinitialize a speculative load!");
      mOpCode = eSpeculativeLoadPreconnect;
      aUrl.ToString(mUrl);
      aCrossOrigin.ToString(mCrossOrigin);
    }

    void Perform(nsHtml5TreeOpExecutor* aExecutor);

  private:
    eHtml5SpeculativeLoad mOpCode;
    nsString mUrl;
    nsString mReferrerPolicy;
    nsString mMetaCSP;

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
     * interpreted as a charset source integer. If mOpCode is
     * eSpeculativeLoadSetDocumentMode, this is a one-character string whose
     * single character's code point is to be interpreted as an
     * nsHtml5DocumentMode. Otherwise, it is empty or the value of the type
     * attribute.
     */
    nsString mTypeOrCharsetSourceOrDocumentMode;
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
    /**
     * If mOpCode is eSpeculativeLoadScript[FromHead], this is the value of the
     * "integrity" attribute.  If the attribute is not set, this will be a void
     * string.
     */
    nsString mIntegrity;
};

#endif // nsHtml5SpeculativeLoad_h
