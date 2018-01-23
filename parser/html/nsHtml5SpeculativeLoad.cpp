/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHtml5SpeculativeLoad.h"
#include "nsHtml5TreeOpExecutor.h"
#include "mozilla/Encoding.h"

using namespace mozilla;

nsHtml5SpeculativeLoad::nsHtml5SpeculativeLoad()
  :
#ifdef DEBUG
  mOpCode(eSpeculativeLoadUninitialized),
#endif
  mIsAsync(false),
  mIsDefer(false)
{
  MOZ_COUNT_CTOR(nsHtml5SpeculativeLoad);
  new(&mCharsetOrSrcset) nsString;
}

nsHtml5SpeculativeLoad::~nsHtml5SpeculativeLoad()
{
  MOZ_COUNT_DTOR(nsHtml5SpeculativeLoad);
  NS_ASSERTION(mOpCode != eSpeculativeLoadUninitialized,
               "Uninitialized speculative load.");
  if (mOpCode != eSpeculativeLoadSetDocumentCharset) {
    mCharsetOrSrcset.~nsString();
  }
}

void
nsHtml5SpeculativeLoad::Perform(nsHtml5TreeOpExecutor* aExecutor)
{
  switch (mOpCode) {
    case eSpeculativeLoadBase:
      aExecutor->SetSpeculationBase(mUrlOrSizes);
      break;
    case eSpeculativeLoadCSP:
      aExecutor->AddSpeculationCSP(mTypeOrCharsetSourceOrDocumentModeOrMetaCSPOrSizesOrIntegrity);
      break;
    case eSpeculativeLoadMetaReferrer:
      aExecutor->SetSpeculationReferrerPolicy(mReferrerPolicyOrIntegrity);
      break;
    case eSpeculativeLoadImage:
      aExecutor->PreloadImage(mUrlOrSizes, mCrossOriginOrMedia, mCharsetOrSrcset,
                              mTypeOrCharsetSourceOrDocumentModeOrMetaCSPOrSizesOrIntegrity,
                              mReferrerPolicyOrIntegrity);
      break;
    case eSpeculativeLoadOpenPicture:
      aExecutor->PreloadOpenPicture();
      break;
    case eSpeculativeLoadEndPicture:
      aExecutor->PreloadEndPicture();
      break;
    case eSpeculativeLoadPictureSource:
      aExecutor->PreloadPictureSource(mCharsetOrSrcset, mUrlOrSizes,
                                      mTypeOrCharsetSourceOrDocumentModeOrMetaCSPOrSizesOrIntegrity,
                                      mCrossOriginOrMedia);
      break;
    case eSpeculativeLoadScript:
      aExecutor->PreloadScript(mUrlOrSizes, mCharsetOrSrcset,
                               mTypeOrCharsetSourceOrDocumentModeOrMetaCSPOrSizesOrIntegrity,
                               mCrossOriginOrMedia, mReferrerPolicyOrIntegrity, false,
                               mIsAsync, mIsDefer, false);
      break;
    case eSpeculativeLoadScriptFromHead:
      aExecutor->PreloadScript(mUrlOrSizes, mCharsetOrSrcset,
                               mTypeOrCharsetSourceOrDocumentModeOrMetaCSPOrSizesOrIntegrity,
                               mCrossOriginOrMedia, mReferrerPolicyOrIntegrity, true,
                               mIsAsync, mIsDefer, false);
      break;
    case eSpeculativeLoadNoModuleScript:
      aExecutor->PreloadScript(mUrlOrSizes, mCharsetOrSrcset,
                               mTypeOrCharsetSourceOrDocumentModeOrMetaCSPOrSizesOrIntegrity,
                               mCrossOriginOrMedia, mReferrerPolicyOrIntegrity, false,
                               mIsAsync, mIsDefer, true);
      break;
    case eSpeculativeLoadNoModuleScriptFromHead:
      aExecutor->PreloadScript(mUrlOrSizes, mCharsetOrSrcset,
                               mTypeOrCharsetSourceOrDocumentModeOrMetaCSPOrSizesOrIntegrity,
                               mCrossOriginOrMedia, mReferrerPolicyOrIntegrity, true,
                               mIsAsync, mIsDefer, true);
      break;
    case eSpeculativeLoadStyle:
      aExecutor->PreloadStyle(mUrlOrSizes, mCharsetOrSrcset, mCrossOriginOrMedia, mReferrerPolicyOrIntegrity,
                              mTypeOrCharsetSourceOrDocumentModeOrMetaCSPOrSizesOrIntegrity);
      break;
    case eSpeculativeLoadManifest:
      aExecutor->ProcessOfflineManifest(mUrlOrSizes);
      break;
    case eSpeculativeLoadSetDocumentCharset: {
        NS_ASSERTION(mTypeOrCharsetSourceOrDocumentModeOrMetaCSPOrSizesOrIntegrity.Length() == 1,
            "Unexpected charset source string");
        int32_t intSource = (int32_t)mTypeOrCharsetSourceOrDocumentModeOrMetaCSPOrSizesOrIntegrity.First();
        aExecutor->SetDocumentCharsetAndSource(WrapNotNull(mEncoding),
                                               intSource);
      }
      break;
    case eSpeculativeLoadSetDocumentMode: {
        NS_ASSERTION(mTypeOrCharsetSourceOrDocumentModeOrMetaCSPOrSizesOrIntegrity.Length() == 1,
            "Unexpected document mode string");
        nsHtml5DocumentMode mode =
            (nsHtml5DocumentMode)mTypeOrCharsetSourceOrDocumentModeOrMetaCSPOrSizesOrIntegrity.First();
        aExecutor->SetDocumentMode(mode);
      }
      break;
    case eSpeculativeLoadPreconnect:
      aExecutor->Preconnect(mUrlOrSizes, mCrossOriginOrMedia);
      break;
    default:
      NS_NOTREACHED("Bogus speculative load.");
      break;
  }
}
