/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
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
#include "nsWrapperCache.h"

class nsILoadInfo;
class nsPIDOMWindowOuter;

namespace mozilla {
namespace extensions {

using dom::Nullable;
using ContentScriptInit = dom::WebExtensionContentScriptInit;

class WebExtensionPolicy;

class MOZ_STACK_CLASS DocInfo final
{
public:
  DocInfo(const URLInfo& aURL, nsILoadInfo* aLoadInfo);

  MOZ_IMPLICIT DocInfo(nsPIDOMWindowOuter* aWindow);

  const URLInfo& URL() const { return mURL; }

  // The principal of the document, or the expected principal of a request.
  // May be null for non-DOMWindow DocInfo objects unless
  // URL().InheritsPrincipal() is true.
  nsIPrincipal* Principal() const;

  // Returns the URL of the document's principal. Note that this must *only*
  // be called for codebase principals.
  const URLInfo& PrincipalURL() const;

  bool IsTopLevel() const;
  bool ShouldMatchActiveTabPermission() const;

  uint64_t FrameID() const;

  nsPIDOMWindowOuter* GetWindow() const
  {
    if (mObj.is<Window>()) {
      return mObj.as<Window>();
    }
    return nullptr;
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


class WebExtensionContentScript final : public nsISupports
                                      , public nsWrapperCache
{
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(WebExtensionContentScript)


  using MatchGlobArray = nsTArray<RefPtr<MatchGlob>>;
  using RunAtEnum = dom::ContentScriptRunAt;

  static already_AddRefed<WebExtensionContentScript>
  Constructor(dom::GlobalObject& aGlobal,
              WebExtensionPolicy& aExtension,
              const ContentScriptInit& aInit,
              ErrorResult& aRv);


  bool Matches(const DocInfo& aDoc) const;
  bool MatchesURI(const URLInfo& aURL) const;

  bool MatchesLoadInfo(const URLInfo& aURL, nsILoadInfo* aLoadInfo) const
  {
    return Matches({aURL, aLoadInfo});
  }
  bool MatchesWindow(nsPIDOMWindowOuter* aWindow) const
  {
    return Matches(aWindow);
  }


  WebExtensionPolicy* Extension() { return mExtension; }
  const WebExtensionPolicy* Extension() const { return mExtension; }

  bool AllFrames() const { return mAllFrames; }
  bool MatchAboutBlank() const { return mMatchAboutBlank; }
  RunAtEnum RunAt() const { return mRunAt; }

  Nullable<uint64_t> GetFrameID() const { return mFrameID; }

  MatchPatternSet* Matches() { return mMatches; }
  const MatchPatternSet* GetMatches() const { return mMatches; }

  MatchPatternSet* GetExcludeMatches() { return mExcludeMatches; }
  const MatchPatternSet* GetExcludeMatches() const { return mExcludeMatches; }

  void GetIncludeGlobs(Nullable<MatchGlobArray>& aGlobs)
  {
    ToNullable(mExcludeGlobs, aGlobs);
  }
  void GetExcludeGlobs(Nullable<MatchGlobArray>& aGlobs)
  {
    ToNullable(mExcludeGlobs, aGlobs);
  }

  void GetCssPaths(nsTArray<nsString>& aPaths) const
  {
    aPaths.AppendElements(mCssPaths);
  }
  void GetJsPaths(nsTArray<nsString>& aPaths) const
  {
    aPaths.AppendElements(mJsPaths);
  }


  WebExtensionPolicy* GetParentObject() const { return mExtension; }

  virtual JSObject* WrapObject(JSContext* aCx, JS::HandleObject aGivenProto) override;

protected:
  friend class WebExtensionPolicy;

  virtual ~WebExtensionContentScript() = default;

  WebExtensionContentScript(WebExtensionPolicy& aExtension,
                            const ContentScriptInit& aInit,
                            ErrorResult& aRv);

private:
  RefPtr<WebExtensionPolicy> mExtension;

  bool mHasActiveTabPermission;

  RefPtr<MatchPatternSet> mMatches;
  RefPtr<MatchPatternSet> mExcludeMatches;

  Nullable<MatchGlobSet> mIncludeGlobs;
  Nullable<MatchGlobSet> mExcludeGlobs;

  nsTArray<nsString> mCssPaths;
  nsTArray<nsString> mJsPaths;

  RunAtEnum mRunAt;

  bool mAllFrames;
  Nullable<uint64_t> mFrameID;
  bool mMatchAboutBlank;

  template <typename T, typename U>
  void
  ToNullable(const Nullable<T>& aInput, Nullable<U>& aOutput)
  {
    if (aInput.IsNull()) {
      aOutput.SetNull();
    } else {
      aOutput.SetValue(aInput.Value());
    }
  }
};

} // namespace extensions
} // namespace mozilla

#endif // mozilla_extensions_WebExtensionContentScript_h
