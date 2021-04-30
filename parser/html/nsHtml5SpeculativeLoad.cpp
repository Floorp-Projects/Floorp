/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHtml5SpeculativeLoad.h"
#include "mozilla/Encoding.h"
#include "nsHtml5TreeOpExecutor.h"

using namespace mozilla;

nsHtml5SpeculativeLoad::nsHtml5SpeculativeLoad()
    : mOpCode(eSpeculativeLoadUninitialized),
      mIsAsync(false),
      mIsDefer(false),
      mIsLinkPreload(false),
      mEncoding(nullptr) {
  MOZ_COUNT_CTOR(nsHtml5SpeculativeLoad);
  new (&mCharsetOrSrcset) nsString;
}

nsHtml5SpeculativeLoad::~nsHtml5SpeculativeLoad() {
  MOZ_COUNT_DTOR(nsHtml5SpeculativeLoad);
  NS_ASSERTION(mOpCode != eSpeculativeLoadUninitialized,
               "Uninitialized speculative load.");
  if (mOpCode != eSpeculativeLoadSetDocumentCharset) {
    mCharsetOrSrcset.~nsString();
  }
}

void nsHtml5SpeculativeLoad::Perform(nsHtml5TreeOpExecutor* aExecutor) {
  switch (mOpCode) {
    case eSpeculativeLoadBase:
      aExecutor->SetSpeculationBase(mUrlOrSizes);
      break;
    case eSpeculativeLoadCSP:
      aExecutor->AddSpeculationCSP(
          mTypeOrCharsetSourceOrDocumentModeOrMetaCSPOrSizesOrIntegrity);
      break;
    case eSpeculativeLoadMetaReferrer:
      aExecutor->UpdateReferrerInfoFromMeta(mReferrerPolicyOrIntegrity);
      break;
    case eSpeculativeLoadImage:
      aExecutor->PreloadImage(
          mUrlOrSizes, mCrossOrigin, mMedia, mCharsetOrSrcset,
          mTypeOrCharsetSourceOrDocumentModeOrMetaCSPOrSizesOrIntegrity,
          mReferrerPolicyOrIntegrity, mIsLinkPreload);
      break;
    case eSpeculativeLoadOpenPicture:
      aExecutor->PreloadOpenPicture();
      break;
    case eSpeculativeLoadEndPicture:
      aExecutor->PreloadEndPicture();
      break;
    case eSpeculativeLoadPictureSource:
      aExecutor->PreloadPictureSource(
          mCharsetOrSrcset, mUrlOrSizes,
          mTypeOrCharsetSourceOrDocumentModeOrMetaCSPOrSizesOrIntegrity,
          mMedia);
      break;
    case eSpeculativeLoadScript:
      aExecutor->PreloadScript(
          mUrlOrSizes, mCharsetOrSrcset,
          mTypeOrCharsetSourceOrDocumentModeOrMetaCSPOrSizesOrIntegrity,
          mCrossOrigin, mMedia, mReferrerPolicyOrIntegrity,
          mScriptReferrerPolicy, false, mIsAsync, mIsDefer, false,
          mIsLinkPreload);
      break;
    case eSpeculativeLoadScriptFromHead:
      aExecutor->PreloadScript(
          mUrlOrSizes, mCharsetOrSrcset,
          mTypeOrCharsetSourceOrDocumentModeOrMetaCSPOrSizesOrIntegrity,
          mCrossOrigin, mMedia, mReferrerPolicyOrIntegrity,
          mScriptReferrerPolicy, true, mIsAsync, mIsDefer, false,
          mIsLinkPreload);
      break;
    case eSpeculativeLoadNoModuleScript:
      aExecutor->PreloadScript(
          mUrlOrSizes, mCharsetOrSrcset,
          mTypeOrCharsetSourceOrDocumentModeOrMetaCSPOrSizesOrIntegrity,
          mCrossOrigin, mMedia, mReferrerPolicyOrIntegrity,
          mScriptReferrerPolicy, false, mIsAsync, mIsDefer, true,
          mIsLinkPreload);
      break;
    case eSpeculativeLoadNoModuleScriptFromHead:
      aExecutor->PreloadScript(
          mUrlOrSizes, mCharsetOrSrcset,
          mTypeOrCharsetSourceOrDocumentModeOrMetaCSPOrSizesOrIntegrity,
          mCrossOrigin, mMedia, mReferrerPolicyOrIntegrity,
          mScriptReferrerPolicy, true, mIsAsync, mIsDefer, true,
          mIsLinkPreload);
      break;
    case eSpeculativeLoadStyle:
      aExecutor->PreloadStyle(
          mUrlOrSizes, mCharsetOrSrcset, mCrossOrigin, mMedia,
          mReferrerPolicyOrIntegrity,
          mTypeOrCharsetSourceOrDocumentModeOrMetaCSPOrSizesOrIntegrity,
          mIsLinkPreload);
      break;
    case eSpeculativeLoadManifest:
      // TODO: remove this
      break;
    case eSpeculativeLoadSetDocumentCharset: {
      NS_ASSERTION(mTypeOrCharsetSourceOrDocumentModeOrMetaCSPOrSizesOrIntegrity
                           .Length() == 1,
                   "Unexpected charset source string");
      int32_t intSource =
          (int32_t)mTypeOrCharsetSourceOrDocumentModeOrMetaCSPOrSizesOrIntegrity
              .First();
      aExecutor->SetDocumentCharsetAndSource(WrapNotNull(mEncoding), intSource);
    } break;
    case eSpeculativeLoadSetDocumentMode: {
      NS_ASSERTION(mTypeOrCharsetSourceOrDocumentModeOrMetaCSPOrSizesOrIntegrity
                           .Length() == 1,
                   "Unexpected document mode string");
      nsHtml5DocumentMode mode =
          (nsHtml5DocumentMode)
              mTypeOrCharsetSourceOrDocumentModeOrMetaCSPOrSizesOrIntegrity
                  .First();
      aExecutor->SetDocumentMode(mode);
    } break;
    case eSpeculativeLoadPreconnect:
      aExecutor->Preconnect(mUrlOrSizes, mCrossOrigin);
      break;
    case eSpeculativeLoadFont:
      aExecutor->PreloadFont(mUrlOrSizes, mCrossOrigin, mMedia,
                             mReferrerPolicyOrIntegrity);
      break;
    case eSpeculativeLoadFetch:
      aExecutor->PreloadFetch(mUrlOrSizes, mCrossOrigin, mMedia,
                              mReferrerPolicyOrIntegrity);
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Bogus speculative load.");
      break;
  }
}
