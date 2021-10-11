/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PageThumbProtocolHandler_h___
#define PageThumbProtocolHandler_h___

#include "mozilla/Result.h"
#include "SubstitutingProtocolHandler.h"

namespace mozilla {
namespace net {

using PageThumbStreamPromise =
    mozilla::MozPromise<nsCOMPtr<nsIInputStream>, nsresult, false>;

class PageThumbStreamGetter;

class PageThumbProtocolHandler final
    : public nsISubstitutingProtocolHandler,
      public nsIProtocolHandlerWithDynamicFlags,
      public SubstitutingProtocolHandler,
      public nsSupportsWeakReference {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIPROTOCOLHANDLERWITHDYNAMICFLAGS
  NS_FORWARD_NSIPROTOCOLHANDLER(SubstitutingProtocolHandler::)
  NS_FORWARD_NSISUBSTITUTINGPROTOCOLHANDLER(SubstitutingProtocolHandler::)

  static already_AddRefed<PageThumbProtocolHandler> GetSingleton();

  /**
   * To be called in the parent process to obtain an input stream for the
   * given thumbnail.
   *
   * @param aChildURI a moz-page-thumb URI sent from the child.
   * @param aTerminateSender out param set to true when the params are invalid
   *        and indicate the child should be terminated. If |aChildURI| is
   *        not a moz-page-thumb URI, the child is in an invalid state and
   *        should be terminated. This outparam will be set synchronously.
   *
   * @return PageThumbStreamPromise
   *        The PageThumbStreamPromise will resolve with an nsIInputStream on
   *        success, and reject with an nsresult on failure.
   */
  RefPtr<PageThumbStreamPromise> NewStream(nsIURI* aChildURI,
                                           bool* aTerminateSender);

 protected:
  ~PageThumbProtocolHandler() = default;

 private:
  explicit PageThumbProtocolHandler();

  [[nodiscard]] bool ResolveSpecialCases(const nsACString& aHost,
                                         const nsACString& aPath,
                                         const nsACString& aPathname,
                                         nsACString& aResult) override;

  /**
   * On entry to this function, *aRetVal is expected to be non-null and already
   * addrefed. This function may release the object stored in *aRetVal on entry
   * and write a new pointer to an already addrefed channel to *aRetVal.
   *
   * @param aURI the moz-page-thumb URI.
   * @param aLoadInfo the loadinfo for the request.
   * @param aRetVal in/out channel param referring to the channel that
   *        might need to be substituted with a remote channel.
   * @return NS_OK if channel has been substituted successfully or no
   *         substitution at all. Otherwise, returns an error. This function
   *         will return NS_ERROR_NO_INTERFACE if the URI resolves to a
   *         non file:// URI.
   */
  [[nodiscard]] virtual nsresult SubstituteChannel(
      nsIURI* aURI, nsILoadInfo* aLoadInfo, nsIChannel** aRetVal) override;

  /**
   * This replaces the provided channel with a channel that will proxy the load
   * to the parent process.
   *
   * @param aURI the moz-page-thumb URI.
   * @param aLoadInfo the loadinfo for the request.
   * @param aRetVal in/out channel param referring to the channel that
   *        might need to be substituted with a remote channel.
   * @return NS_OK if the replacement channel was created successfully.
   *         Otherwise, returns an error.
   */
  Result<Ok, nsresult> SubstituteRemoteChannel(nsIURI* aURI,
                                               nsILoadInfo* aLoadInfo,
                                               nsIChannel** aRetVal);

  /*
   * Extracts the URL from the query string in the given moz-page-thumb URI
   * and queries PageThumbsStorageService using the extracted URL to obtain
   * the local file path of the screenshot. This should only be called from
   * the parent because PageThumbsStorageService relies on the path of the
   * profile directory, which is unavailable in the child.
   *
   * @param aPath the path of the moz-page-thumb URI.
   * @param aThumbnailPath in/out string param referring to the thumbnail path.
   * @return NS_OK if the thumbnail path was obtained successfully. Otherwise
   *         returns an error.
   */
  nsresult GetThumbnailPath(const nsACString& aPath, nsString& aThumbnailPath);

  // To allow parent IPDL actors to invoke methods on this handler when
  // handling moz-page-thumb requests from the child.
  static StaticRefPtr<PageThumbProtocolHandler> sSingleton;

  // Set the channel's content type using the provided URI's type.
  static void SetContentType(nsIURI* aURI, nsIChannel* aChannel);

  // Gets a SimpleChannel that wraps the provided channel.
  static void NewSimpleChannel(nsIURI* aURI, nsILoadInfo* aLoadinfo,
                               PageThumbStreamGetter* aStreamGetter,
                               nsIChannel** aRetVal);
};

}  // namespace net
}  // namespace mozilla

#endif /* PageThumbProtocolHandler_h___ */
