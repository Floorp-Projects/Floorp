/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_extensions_WebExtensionContentScript_h
#define mozilla_extensions_WebExtensionContentScript_h

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/WebExtensionContentScriptBinding.h"

#include "jspubtd.h"

#include "mozilla/Maybe.h"
#include "mozilla/Variant.h"
#include "mozilla/extensions/MatchGlob.h"
#include "mozilla/extensions/MatchPattern.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupports.h"
#include "nsIDocShell.h"
#include "nsPIDOMWindow.h"
#include "nsWrapperCache.h"

class nsILoadInfo;
class nsPIDOMWindowOuter;

namespace mozilla {
namespace dom {
class WindowGlobalChild;
}

namespace extensions {

using dom::Nullable;
using ContentScriptInit = dom::WebExtensionContentScriptInit;

class WebExtensionPolicy;

class MOZ_STACK_CLASS DocInfo final {
 public:
  DocInfo(const URLInfo& aURL, nsILoadInfo* aLoadInfo);

  MOZ_IMPLICIT DocInfo(nsPIDOMWindowOuter* aWindow);

  const URLInfo& URL() const { return mURL; }

  // The principal of the document, or the expected principal of a request.
  // May be null for non-DOMWindow DocInfo objects unless
  // URL().InheritsPrincipal() is true.
  nsIPrincipal* Principal() const;

  // Returns the URL of the document's principal. Note that this must *only*
  // be called for content principals.
  const URLInfo& PrincipalURL() const;

  bool IsTopLevel() const;
  bool ShouldMatchActiveTabPermission() const;

  uint64_t FrameID() const;

  nsPIDOMWindowOuter* GetWindow() const {
    if (mObj.is<Window>()) {
      return mObj.as<Window>();
    }
    return nullptr;
  }

  nsILoadInfo* GetLoadInfo() const {
    if (mObj.is<LoadInfo>()) {
      return mObj.as<LoadInfo>();
    }
    return nullptr;
  }

  already_AddRefed<nsILoadContext> GetLoadContext() const {
    nsCOMPtr<nsILoadContext> loadContext;
    if (nsPIDOMWindowOuter* window = GetWindow()) {
      nsIDocShell* docShell = window->GetDocShell();
      loadContext = do_QueryInterface(docShell);
    } else if (nsILoadInfo* loadInfo = GetLoadInfo()) {
      nsCOMPtr<nsISupports> requestingContext = loadInfo->GetLoadingContext();
      loadContext = do_QueryInterface(requestingContext);
    }
    return loadContext.forget();
  }

 private:
  void SetURL(const URLInfo& aURL);

  const URLInfo mURL;
  mutable Maybe<const URLInfo> mPrincipalURL;

  mutable Maybe<bool> mIsTopLevel;

  mutable Maybe<nsCOMPtr<nsIPrincipal>> mPrincipal;
  mutable Maybe<uint64_t> mFrameID;

  using Window = nsPIDOMWindowOuter*;
  using LoadInfo = nsILoadInfo*;

  const Variant<LoadInfo, Window> mObj;
};

class MozDocumentMatcher : public nsISupports, public nsWrapperCache {
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(MozDocumentMatcher)

  using MatchGlobArray = nsTArray<RefPtr<MatchGlob>>;

  static already_AddRefed<MozDocumentMatcher> Constructor(
      dom::GlobalObject& aGlobal, const dom::MozDocumentMatcherInit& aInit,
      ErrorResult& aRv);

  bool Matches(const DocInfo& aDoc) const;
  bool MatchesURI(const URLInfo& aURL) const;

  bool MatchesWindowGlobal(dom::WindowGlobalChild& aWindow) const;

  WebExtensionPolicy* GetExtension() { return mExtension; }

  WebExtensionPolicy* Extension() { return mExtension; }
  const WebExtensionPolicy* Extension() const { return mExtension; }

  bool AllFrames() const { return mAllFrames; }
  bool CheckPermissions() const { return mCheckPermissions; }
  bool MatchAboutBlank() const { return mMatchAboutBlank; }

  MatchPatternSet* Matches() { return mMatches; }
  const MatchPatternSet* GetMatches() const { return mMatches; }

  MatchPatternSet* GetExcludeMatches() { return mExcludeMatches; }
  const MatchPatternSet* GetExcludeMatches() const { return mExcludeMatches; }

  void GetIncludeGlobs(Nullable<MatchGlobArray>& aGlobs) {
    ToNullable(mExcludeGlobs, aGlobs);
  }
  void GetExcludeGlobs(Nullable<MatchGlobArray>& aGlobs) {
    ToNullable(mExcludeGlobs, aGlobs);
  }

  Nullable<uint64_t> GetFrameID() const { return mFrameID; }

  void GetOriginAttributesPatterns(JSContext* aCx,
                                   JS::MutableHandle<JS::Value> aVal,
                                   ErrorResult& aError) const;

  WebExtensionPolicy* GetParentObject() const { return mExtension; }
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::HandleObject aGivenProto) override;

 protected:
  friend class WebExtensionPolicy;

  virtual ~MozDocumentMatcher() = default;

  MozDocumentMatcher(dom::GlobalObject& aGlobal,
                     const dom::MozDocumentMatcherInit& aInit, bool aRestricted,
                     ErrorResult& aRv);

  RefPtr<WebExtensionPolicy> mExtension;

  bool mHasActiveTabPermission;
  bool mRestricted;

  RefPtr<MatchPatternSet> mMatches;
  RefPtr<MatchPatternSet> mExcludeMatches;

  Nullable<MatchGlobSet> mIncludeGlobs;
  Nullable<MatchGlobSet> mExcludeGlobs;

  bool mAllFrames;
  bool mCheckPermissions;
  Nullable<uint64_t> mFrameID;
  bool mMatchAboutBlank;
  Nullable<dom::Sequence<OriginAttributesPattern>> mOriginAttributesPatterns;

 private:
  template <typename T, typename U>
  void ToNullable(const Nullable<T>& aInput, Nullable<U>& aOutput) {
    if (aInput.IsNull()) {
      aOutput.SetNull();
    } else {
      aOutput.SetValue(aInput.Value());
    }
  }

  template <typename T, typename U>
  void ToNullable(const Nullable<T>& aInput, Nullable<nsTArray<U>>& aOutput) {
    if (aInput.IsNull()) {
      aOutput.SetNull();
    } else {
      aOutput.SetValue(aInput.Value().Clone());
    }
  }
};

class WebExtensionContentScript final : public MozDocumentMatcher {
 public:
  using RunAtEnum = dom::ContentScriptRunAt;

  static already_AddRefed<WebExtensionContentScript> Constructor(
      dom::GlobalObject& aGlobal, WebExtensionPolicy& aExtension,
      const ContentScriptInit& aInit, ErrorResult& aRv);

  RunAtEnum RunAt() const { return mRunAt; }

  void GetCssPaths(nsTArray<nsString>& aPaths) const {
    aPaths.AppendElements(mCssPaths);
  }
  void GetJsPaths(nsTArray<nsString>& aPaths) const {
    aPaths.AppendElements(mJsPaths);
  }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::HandleObject aGivenProto) override;

 protected:
  friend class WebExtensionPolicy;

  virtual ~WebExtensionContentScript() = default;

  WebExtensionContentScript(dom::GlobalObject& aGlobal,
                            WebExtensionPolicy& aExtension,
                            const ContentScriptInit& aInit, ErrorResult& aRv);

 private:
  nsTArray<nsString> mCssPaths;
  nsTArray<nsString> mJsPaths;

  RunAtEnum mRunAt;
};

}  // namespace extensions
}  // namespace mozilla

#endif  // mozilla_extensions_WebExtensionContentScript_h
