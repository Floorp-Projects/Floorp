/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ExtensionProtocolHandler_h___
#define ExtensionProtocolHandler_h___

#include "mozilla/net/NeckoParent.h"
#include "mozilla/LazyIdleThread.h"
#include "SubstitutingProtocolHandler.h"

namespace mozilla {
namespace net {

class ExtensionProtocolHandler final : public nsISubstitutingProtocolHandler,
                                       public nsIProtocolHandlerWithDynamicFlags,
                                       public SubstitutingProtocolHandler,
                                       public nsSupportsWeakReference
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIPROTOCOLHANDLERWITHDYNAMICFLAGS
  NS_FORWARD_NSIPROTOCOLHANDLER(SubstitutingProtocolHandler::)
  NS_FORWARD_NSISUBSTITUTINGPROTOCOLHANDLER(SubstitutingProtocolHandler::)

  static already_AddRefed<ExtensionProtocolHandler> GetSingleton();

  /**
   * To be called in the parent process to obtain an input stream for a
   * a web accessible resource from an unpacked WebExtension dir.
   *
   * @param aChildURI a moz-extension URI sent from the child that refers
   *        to a web accessible resource file in an enabled unpacked extension
   * @param aChildLoadInfo the loadinfo for the request sent from the child
   * @param aTerminateSender out param set to true when the params are invalid
   *        and indicate the child should be terminated. If |aChildURI| is
   *        not a moz-extension URI, the child is in an invalid state and
   *        should be terminated.
   * @return NS_OK with |aTerminateSender| set to false on success. On
   *         failure, returns an error and sets |aTerminateSender| to indicate
   *         whether or not the child process should be terminated.
   *         A moz-extension URI from the child that doesn't resolve to a
   *         resource file within the extension could be the result of a bug
   *         in the extension and doesn't result in |aTerminateSender| being
   *         set to true.
   */
  Result<nsCOMPtr<nsIInputStream>, nsresult> NewStream(nsIURI* aChildURI,
                                                       nsILoadInfo* aChildLoadInfo,
                                                       bool* aTerminateSender);

  /**
   * To be called in the parent process to obtain a file descriptor for an
   * enabled WebExtension JAR file.
   *
   * @param aChildURI a moz-extension URI sent from the child that refers
   *        to a web accessible resource file in an enabled unpacked extension
   * @param aChildLoadInfo the loadinfo for the request sent from the child
   * @param aTerminateSender out param set to true when the params are invalid
   *        and indicate the child should be terminated. If |aChildURI| is
   *        not a moz-extension URI, the child is in an invalid state and
   *        should be terminated.
   * @param aPromise a promise that will be resolved asynchronously when the
   *        file descriptor is available.
   * @return NS_OK with |aTerminateSender| set to false on success. On
   *         failure, returns an error and sets |aTerminateSender| to indicate
   *         whether or not the child process should be terminated.
   *         A moz-extension URI from the child that doesn't resolve to an
   *         enabled WebExtension JAR could be the result of a bug in the
   *         extension and doesn't result in |aTerminateSender| being
   *         set to true.
   */
  Result<Ok, nsresult> NewFD(nsIURI* aChildURI,
                             nsILoadInfo* aChildLoadInfo,
                             bool* aTerminateSender,
                             NeckoParent::GetExtensionFDResolver& aResolve);

protected:
  ~ExtensionProtocolHandler() {}

private:
  explicit ExtensionProtocolHandler();

  MOZ_MUST_USE bool ResolveSpecialCases(const nsACString& aHost,
                                        const nsACString& aPath,
                                        const nsACString& aPathname,
                                        nsACString& aResult) override;

  // |result| is an inout param.  On entry to this function, *result
  // is expected to be non-null and already addrefed.  This function
  // may release the object stored in *result on entry and write
  // a new pointer to an already addrefed channel to *result.
  virtual MOZ_MUST_USE nsresult SubstituteChannel(nsIURI* uri,
                                                  nsILoadInfo* aLoadInfo,
                                                  nsIChannel** result) override;

  /**
   * For moz-extension URI's that resolve to file or JAR URI's, replaces
   * the provided channel with a channel that will proxy the load to the
   * parent process. For moz-extension URI's that resolve to other types
   * of URI's (not file or JAR), the provide channel is not replaced and
   * NS_OK is returned.
   *
   * @param aURI the moz-extension URI
   * @param aLoadInfo the loadinfo for the request
   * @param aRetVal in/out channel param referring to the channel that
   *        might need to be substituted with a remote channel.
   * @return NS_OK if the channel does not need to be substituted or
   *         or the replacement channel was created successfully.
   *         Otherwise returns an error.
   */
  Result<Ok, nsresult> SubstituteRemoteChannel(nsIURI* aURI,
                                               nsILoadInfo* aLoadInfo,
                                               nsIChannel** aRetVal);

  /**
   * Replaces a file channel with a remote file channel for loading a
   * web accessible resource for an unpacked extension from the parent.
   *
   * @param aURI the moz-extension URI
   * @param aLoadInfo the loadinfo for the request
   * @param aResolvedFileSpec the resolved URI spec for the file.
   * @param aRetVal in/out param referring to the new remote channel.
   *        The reference to the input param file channel is dropped and
   *        replaced with a reference to a new channel that remotes
   *        the file access. The new channel encapsulates a request to
   *        the parent for an IPCStream for the file.
   */
  void SubstituteRemoteFileChannel(nsIURI* aURI,
                                   nsILoadInfo* aLoadinfo,
                                   nsACString& aResolvedFileSpec,
                                   nsIChannel** aRetVal);

  /**
   * Replaces a JAR channel with a remote JAR channel for loading a
   * an extension JAR file from the parent.
   *
   * @param aURI the moz-extension URI
   * @param aLoadInfo the loadinfo for the request
   * @param aResolvedFileSpec the resolved URI spec for the file.
   * @param aRetVal in/out param referring to the new remote channel.
   *        The input param JAR channel is replaced with a new channel
   *        that remotes the JAR file access. The new channel encapsulates
   *        a request to the parent for the JAR file FD.
   */
  Result<Ok, nsresult> SubstituteRemoteJarChannel(nsIURI* aURI,
                                                  nsILoadInfo* aLoadinfo,
                                                  nsACString& aResolvedSpec,
                                                  nsIChannel** aRetVal);

  // Used for opening JAR files off the main thread when we just need to
  // obtain a file descriptor to send back to the child.
  RefPtr<mozilla::LazyIdleThread> mFileOpenerThread;

  // To allow parent IPDL actors to invoke methods on this handler when
  // handling moz-extension requests from the child.
  static StaticRefPtr<ExtensionProtocolHandler> sSingleton;

  // Set to true when this instance of the handler must proxy loads of
  // extension web-accessible resources to the parent process.
  bool mUseRemoteFileChannels;
};

} // namespace net
} // namespace mozilla

#endif /* ExtensionProtocolHandler_h___ */
