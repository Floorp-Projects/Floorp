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

  // Returns the URL to use for matching against the content script's match
  // patterns. For content principals, this is usually equal to URL().
  // Similarly for null principals when IsNonOpaqueURL() is true.
  // For null principals with a precursor, their precursor is used.
  // In all other cases, URL() is returned.
  const URLInfo& PrincipalURL() const;

  // Whether match_origin_as_fallback must be set in order for PrincipalURL()
  // to be eligible for matching the document.
  bool RequiresMatchOriginAsFallback() const;

  bool IsTopLevel() const;
  bool IsTopLevelOpaqueAboutBlank() const;
  bool IsSameOriginWithTop() const;
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
  mutable Maybe<bool> mRequiresMatchOriginAsFallback;

  mutable Maybe<bool> mIsTopLevel;
  mutable Maybe<bool> mIsTopLevelOpaqueAboutBlank;

  mutable Maybe<nsCOMPtr<nsIPrincipal>> mPrincipal;
  mutable Maybe<uint64_t> mFrameID;

  using Window = nsPIDOMWindowOuter*;
  using LoadInfo = nsILoadInfo*;

  const Variant<LoadInfo, Window> mObj;
};

class MozDocumentMatcher : public nsISupports, public nsWrapperCache {
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(MozDocumentMatcher)

  using MatchGlobArray = nsTArray<RefPtr<MatchGlob>>;

  static already_AddRefed<MozDocumentMatcher> Constructor(
      dom::GlobalObject& aGlobal, const dom::MozDocumentMatcherInit& aInit,
      ErrorResult& aRv);

  bool Matches(const DocInfo& aDoc, bool aIgnorePermissions) const;
  bool Matches(const DocInfo& aDoc) const { return Matches(aDoc, false); }

  bool MatchesURI(const URLInfo& aURL, bool aIgnorePermissions) const;
  bool MatchesURI(const URLInfo& aURL) const { return MatchesURI(aURL, false); }

  bool MatchesWindowGlobal(dom::WindowGlobalChild& aWindow,
                           bool aIgnorePermissions) const;

  WebExtensionPolicy* GetExtension() { return mExtension; }

  WebExtensionPolicy* Extension() { return mExtension; }
  const WebExtensionPolicy* Extension() const { return mExtension; }

  bool AllFrames() const { return mAllFrames; }
  bool CheckPermissions() const { return mCheckPermissions; }
  bool MatchAboutBlank() const { return mMatchAboutBlank; }
  bool MatchOriginAsFallback() const { return mMatchOriginAsFallback; }

  MatchPatternSet* Matches() { return mMatches; }
  const MatchPatternSet* GetMatches() const { return mMatches; }

  MatchPatternSet* GetExcludeMatches() { return mExcludeMatches; }
  const MatchPatternSet* GetExcludeMatches() const { return mExcludeMatches; }

  Nullable<uint64_t> GetFrameID() const { return mFrameID; }

  void GetOriginAttributesPatterns(JSContext* aCx,
                                   JS::MutableHandle<JS::Value> aVal,
                                   ErrorResult& aError) const;

  WebExtensionPolicy* GetParentObject() const { return mExtension; }
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

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
  bool mMatchOriginAsFallback;
  Nullable<dom::Sequence<OriginAttributesPattern>> mOriginAttributesPatterns;
};

class WebExtensionContentScript final : public MozDocumentMatcher {
 public:
  using RunAtEnum = dom::ContentScriptRunAt;
  using ExecutionWorld = dom::ContentScriptExecutionWorld;

  static already_AddRefed<WebExtensionContentScript> Constructor(
      dom::GlobalObject& aGlobal, WebExtensionPolicy& aExtension,
      const ContentScriptInit& aInit, ErrorResult& aRv);

  RunAtEnum RunAt() const { return mRunAt; }
  ExecutionWorld World() const { return mWorld; }

  void GetCssPaths(nsTArray<nsString>& aPaths) const {
    aPaths.AppendElements(mCssPaths);
  }
  void GetJsPaths(nsTArray<nsString>& aPaths) const {
    aPaths.AppendElements(mJsPaths);
  }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

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
  ExecutionWorld mWorld;
};

}  // namespace extensions
}  // namespace mozilla

#endif  // mozilla_extensions_WebExtensionContentScript_h
