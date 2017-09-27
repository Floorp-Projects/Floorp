/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_extensions_ChannelWrapper_h
#define mozilla_extensions_ChannelWrapper_h

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/ChannelWrapperBinding.h"

#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"

#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIChannel.h"
#include "nsIHttpChannel.h"
#include "nsWeakPtr.h"
#include "nsWrapperCache.h"

#define NS_CHANNELWRAPPER_IID \
{ 0xc06162d2, 0xb803, 0x43b4, \
  { 0xaa, 0x31, 0xcf, 0x69, 0x7f, 0x93, 0x68, 0x1c } }

class nsIDOMElement;
class nsILoadContext;

namespace mozilla {
namespace extensions {

namespace detail {

  // We need to store our wrapped channel as a weak reference, since channels
  // are not cycle collected, and we're going to be hanging this wrapper
  // instance off the channel in order to ensure the same channel always has
  // the same wrapper.
  //
  // But since performance matters here, and we don't want to have to
  // QueryInterface the channel every time we touch it, we store separate
  // nsIChannel and nsIHttpChannel weak references, and check that the WeakPtr
  // is alive before returning it.
  //
  // This holder class prevents us from accidentally touching the weak pointer
  // members directly from our ChannelWrapper class.
  struct ChannelHolder
  {
    explicit ChannelHolder(nsIChannel* aChannel)
      : mChannel(do_GetWeakReference(aChannel))
      , mWeakChannel(aChannel)
    {}

    bool HaveChannel() const { return mChannel && mChannel->IsAlive(); }

    void SetChannel(nsIChannel* aChannel)
    {
      mChannel = do_GetWeakReference(aChannel);
      mWeakChannel = aChannel;
      mWeakHttpChannel.reset();
    }

    already_AddRefed<nsIChannel> MaybeChannel() const
    {
      if (!HaveChannel()) {
        mWeakChannel = nullptr;
      }
      return do_AddRef(mWeakChannel);
    }

    already_AddRefed<nsIHttpChannel> MaybeHttpChannel() const
    {
      if (mWeakHttpChannel.isNothing()) {
        nsCOMPtr<nsIHttpChannel> chan = QueryChannel();
        mWeakHttpChannel.emplace(chan.get());
      }

      if (!HaveChannel()) {
        mWeakHttpChannel.ref() = nullptr;
      }
      return do_AddRef(mWeakHttpChannel.value());
    }

    const nsQueryReferent QueryChannel() const { return do_QueryReferent(mChannel); }

  private:
    nsWeakPtr mChannel;

    mutable nsIChannel* MOZ_NON_OWNING_REF mWeakChannel;
    mutable Maybe<nsIHttpChannel*> MOZ_NON_OWNING_REF mWeakHttpChannel;
  };
}

class ChannelWrapper final : public nsISupports
                           , public nsWrapperCache
                           , private detail::ChannelHolder
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(ChannelWrapper)

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_CHANNELWRAPPER_IID)

  static already_AddRefed<extensions::ChannelWrapper> Get(const dom::GlobalObject& global, nsIChannel* channel);


  uint64_t Id() const { return mId; }

  already_AddRefed<nsIChannel> GetChannel() const { return MaybeChannel(); }

  void SetChannel(nsIChannel* aChannel);


  void Cancel(uint32_t result, ErrorResult& aRv);

  void RedirectTo(nsIURI* uri, ErrorResult& aRv);


  bool Suspended() const { return mSuspended; }

  void SetSuspended(bool aSuspended, ErrorResult& aRv);


  void GetContentType(nsCString& aContentType) const;
  void SetContentType(const nsACString& aContentType);


  void GetMethod(nsCString& aRetVal) const;

  dom::MozContentPolicyType Type() const;


  uint32_t StatusCode() const;

  void GetStatusLine(nsCString& aRetVal) const;


  already_AddRefed<nsIURI> GetFinalURI(ErrorResult& aRv) const;

  void GetFinalURL(nsCString& aRetVal, ErrorResult& aRv) const;


  already_AddRefed<nsILoadInfo> GetLoadInfo() const
  {
    nsCOMPtr<nsIChannel> chan = MaybeChannel();
    if (chan) {
      return chan->GetLoadInfo();
    }
    return nullptr;
  }

  int64_t WindowId() const;

  int64_t ParentWindowId() const;

  void GetFrameAncestors(dom::Nullable<nsTArray<dom::MozFrameAncestorInfo>>& aFrameAncestors, ErrorResult& aRv) const;

  bool IsSystemLoad() const;

  void GetOriginURL(nsCString& aRetVal) const;

  void GetDocumentURL(nsCString& aRetVal) const;

  already_AddRefed<nsIURI> GetOriginURI() const;

  already_AddRefed<nsIURI> GetDocumentURI() const;


  already_AddRefed<nsILoadContext> GetLoadContext() const;

  already_AddRefed<nsIDOMElement> GetBrowserElement() const;


  bool GetCanModify(ErrorResult& aRv) const;


  void GetProxyInfo(dom::Nullable<dom::MozProxyInfo>& aRetVal, ErrorResult& aRv) const;

  void GetRemoteAddress(nsCString& aRetVal) const;

  void GetRequestHeaders(JSContext* cx, JS::MutableHandle<JSObject*> aRetVal, ErrorResult& aRv) const;

  void GetResponseHeaders(JSContext* cx, JS::MutableHandle<JSObject*> aRetVal, ErrorResult& aRv) const;

  void SetRequestHeader(const nsCString& header, const nsCString& value, ErrorResult& aRv);

  void SetResponseHeader(const nsCString& header, const nsCString& value, ErrorResult& aRv);


  nsISupports* GetParentObject() const { return mParent; }

  JSObject* WrapObject(JSContext* aCx, JS::HandleObject aGivenProto) override;

protected:
  ~ChannelWrapper() = default;

private:
  ChannelWrapper(nsISupports* aParent, nsIChannel* aChannel)
    : ChannelHolder(aChannel)
    , mParent(aParent)
  {}

  void ClearCachedAttributes();

  bool CheckAlive(ErrorResult& aRv) const
  {
    if (!HaveChannel()) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return false;
    }
    return true;
  }

  uint64_t WindowId(nsILoadInfo* aLoadInfo) const;

  nsresult GetFrameAncestors(nsILoadInfo* aLoadInfo, nsTArray<dom::MozFrameAncestorInfo>& aFrameAncestors) const;

  static uint64_t GetNextId()
  {
    static uint64_t sNextId = 1;
    return ++sNextId;
  }

  const uint64_t mId = GetNextId();
  nsCOMPtr<nsISupports> mParent;

  bool mSuspended = false;
};

NS_DEFINE_STATIC_IID_ACCESSOR(ChannelWrapper,
                              NS_CHANNELWRAPPER_IID)

} // namespace extensions
} // namespace mozilla

#endif // mozilla_extensions_ChannelWrapper_h
