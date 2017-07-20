/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ExtensionProtocolHandler.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/ExtensionPolicyService.h"
#include "mozilla/FileUtils.h"
#include "mozilla/ipc/IPCStreamUtils.h"
#include "mozilla/ipc/URIParams.h"
#include "mozilla/ipc/URIUtils.h"
#include "mozilla/net/NeckoChild.h"
#include "mozilla/RefPtr.h"

#include "FileDescriptor.h"
#include "FileDescriptorFile.h"
#include "LoadInfo.h"
#include "nsContentUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsContentUtils.h"
#include "nsIFile.h"
#include "nsIFileChannel.h"
#include "nsIFileStreams.h"
#include "nsIFileURL.h"
#include "nsIJARChannel.h"
#include "nsIMIMEService.h"
#include "nsIURL.h"
#include "nsIChannel.h"
#include "nsIInputStreamPump.h"
#include "nsIJARURI.h"
#include "nsIStreamListener.h"
#include "nsIThread.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIStreamConverterService.h"
#include "nsNetUtil.h"
#include "prio.h"
#include "SimpleChannel.h"

#if defined(XP_WIN)
#include "nsILocalFileWin.h"
#include "WinUtils.h"
#endif

#if !defined(XP_WIN) && defined(MOZ_CONTENT_SANDBOX)
#include "mozilla/SandboxSettings.h"
#endif

#define EXTENSION_SCHEME "moz-extension"
using mozilla::ipc::FileDescriptor;
using OptionalIPCStream = mozilla::ipc::OptionalIPCStream;

namespace mozilla {

template <>
class MOZ_MUST_USE_TYPE GenericErrorResult<nsresult>
{
  nsresult mErrorValue;

  template<typename V, typename E2> friend class Result;

public:
  explicit GenericErrorResult(nsresult aErrorValue) : mErrorValue(aErrorValue) {}

  operator nsresult() { return mErrorValue; }
};

namespace net {

using extensions::URLInfo;

LazyLogModule gExtProtocolLog("ExtProtocol");
#undef LOG
#define LOG(...) MOZ_LOG(gExtProtocolLog, LogLevel::Debug, (__VA_ARGS__))

StaticRefPtr<ExtensionProtocolHandler> ExtensionProtocolHandler::sSingleton;

static inline Result<Ok, nsresult>
WrapNSResult(PRStatus aRv)
{
    if (aRv != PR_SUCCESS) {
        return Err(NS_ERROR_FAILURE);
    }
    return Ok();
}

static inline Result<Ok, nsresult>
WrapNSResult(nsresult aRv)
{
    if (NS_FAILED(aRv)) {
        return Err(aRv);
    }
    return Ok();
}

#define NS_TRY(expr) MOZ_TRY(WrapNSResult(expr))

/**
 * Helper class used with SimpleChannel to asynchronously obtain an input
 * stream or file descriptor from the parent for a remote moz-extension load
 * from the child.
 */
class ExtensionStreamGetter : public RefCounted<ExtensionStreamGetter>
{
  public:
    // To use when getting a remote input stream for a resource
    // in an unpacked extension.
    ExtensionStreamGetter(nsIURI* aURI, nsILoadInfo* aLoadInfo)
      : mURI(aURI)
      , mLoadInfo(aLoadInfo)
      , mIsJarChannel(false)
    {
      MOZ_ASSERT(aURI);
      MOZ_ASSERT(aLoadInfo);

      SetupEventTarget();
    }

    // To use when getting an FD for a packed extension JAR file
    // in order to load a resource.
    ExtensionStreamGetter(nsIURI* aURI, nsILoadInfo* aLoadInfo,
                          already_AddRefed<nsIJARChannel>&& aJarChannel,
                          nsIFile* aJarFile)
      : mURI(aURI)
      , mLoadInfo(aLoadInfo)
      , mJarChannel(Move(aJarChannel))
      , mJarFile(aJarFile)
      , mIsJarChannel(true)
    {
      MOZ_ASSERT(aURI);
      MOZ_ASSERT(aLoadInfo);
      MOZ_ASSERT(mJarChannel);
      MOZ_ASSERT(aJarFile);

      SetupEventTarget();
    }

    ~ExtensionStreamGetter() {}

    void SetupEventTarget()
    {
      mMainThreadEventTarget =
        nsContentUtils::GetEventTargetByLoadInfo(mLoadInfo, TaskCategory::Other);
      if (!mMainThreadEventTarget) {
        mMainThreadEventTarget = GetMainThreadSerialEventTarget();
      }
    }

    // Get an input stream or file descriptor from the parent asynchronously.
    Result<Ok, nsresult> GetAsync(nsIStreamListener* aListener,
                                  nsIChannel* aChannel);

    // Handle an input stream being returned from the parent
    void OnStream(nsIInputStream* aStream);

    // Handle file descriptor being returned from the parent
    void OnFD(const FileDescriptor& aFD);

    MOZ_DECLARE_REFCOUNTED_TYPENAME(ExtensionStreamGetter)

  private:
    nsCOMPtr<nsIURI> mURI;
    nsCOMPtr<nsILoadInfo> mLoadInfo;
    nsCOMPtr<nsIJARChannel> mJarChannel;
    nsCOMPtr<nsIFile> mJarFile;
    nsCOMPtr<nsIStreamListener> mListener;
    nsCOMPtr<nsIChannel> mChannel;
    nsCOMPtr<nsISerialEventTarget> mMainThreadEventTarget;
    bool mIsJarChannel;
};

class ExtensionJARFileOpener : public nsISupports
{
public:
  ExtensionJARFileOpener(nsIFile* aFile,
                         NeckoParent::GetExtensionFDResolver& aResolve) :
    mFile(aFile),
    mResolve(aResolve)
  {
    MOZ_ASSERT(aFile);
    MOZ_ASSERT(aResolve);
  }

  NS_IMETHOD OpenFile()
  {
    MOZ_ASSERT(!NS_IsMainThread());
    AutoFDClose prFileDesc;

#if defined(XP_WIN)
    nsresult rv;
    nsCOMPtr<nsILocalFileWin> winFile = do_QueryInterface(mFile, &rv);
    MOZ_ASSERT(winFile);
    if (NS_SUCCEEDED(rv)) {
      rv = winFile->OpenNSPRFileDescShareDelete(PR_RDONLY, 0,
                                                &prFileDesc.rwget());
    }
#else
    nsresult rv = mFile->OpenNSPRFileDesc(PR_RDONLY, 0, &prFileDesc.rwget());
#endif /* XP_WIN */

    if (NS_SUCCEEDED(rv)) {
      mFD = FileDescriptor(FileDescriptor::PlatformHandleType(
                           PR_FileDesc2NativeHandle(prFileDesc)));
    }

    nsCOMPtr<nsIRunnable> event =
      mozilla::NewRunnableMethod("ExtensionJarFileFDResolver",
        this, &ExtensionJARFileOpener::SendBackFD);

    rv = NS_DispatchToMainThread(event, nsIEventTarget::DISPATCH_NORMAL);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "NS_DispatchToMainThread");
    return NS_OK;
  }

  NS_IMETHOD SendBackFD()
  {
    MOZ_ASSERT(NS_IsMainThread());
    mResolve(mFD);
    return NS_OK;
  }

  NS_DECL_THREADSAFE_ISUPPORTS

private:
  virtual ~ExtensionJARFileOpener() {}

  nsCOMPtr<nsIFile> mFile;
  NeckoParent::GetExtensionFDResolver mResolve;
  FileDescriptor mFD;
};

NS_IMPL_ISUPPORTS(ExtensionJARFileOpener, nsISupports)

// The amount of time, in milliseconds, that the file opener thread will remain
// allocated after it is used. This value chosen because to match other uses
// of LazyIdleThread.
#define DEFAULT_THREAD_TIMEOUT_MS 30000

// Request an FD or input stream from the parent.
Result<Ok, nsresult>
ExtensionStreamGetter::GetAsync(nsIStreamListener* aListener,
                                nsIChannel* aChannel)
{
  MOZ_ASSERT(IsNeckoChild());
  MOZ_ASSERT(mMainThreadEventTarget);

  mListener = aListener;
  mChannel = aChannel;

  // Serialize the URI to send to parent
  mozilla::ipc::URIParams uri;
  SerializeURI(mURI, uri);

  RefPtr<ExtensionStreamGetter> self = this;
  if (mIsJarChannel) {
    // Request an FD for this moz-extension URI
    gNeckoChild->SendGetExtensionFD(uri)->Then(
      mMainThreadEventTarget,
      __func__,
      [self] (const FileDescriptor& fd) {
        self->OnFD(fd);
      },
      [self] (const mozilla::ipc::PromiseRejectReason) {
        self->OnFD(FileDescriptor());
      }
    );
    return Ok();
  }

  // Request an input stream for this moz-extension URI
  gNeckoChild->SendGetExtensionStream(uri)->Then(
    mMainThreadEventTarget,
    __func__,
    [self] (const OptionalIPCStream& stream) {
      nsCOMPtr<nsIInputStream> inputStream;
      if (stream.type() == OptionalIPCStream::OptionalIPCStream::TIPCStream) {
        inputStream = ipc::DeserializeIPCStream(stream);
      }
      self->OnStream(inputStream);
    },
    [self] (const mozilla::ipc::PromiseRejectReason) {
      self->OnStream(nullptr);
    }
  );
  return Ok();
}

// Handle an input stream sent from the parent.
void
ExtensionStreamGetter::OnStream(nsIInputStream* aStream)
{
  MOZ_ASSERT(IsNeckoChild());
  MOZ_ASSERT(mListener);
  MOZ_ASSERT(mMainThreadEventTarget);

  // We must keep an owning reference to the listener
  // until we pass it on to AsyncRead.
  nsCOMPtr<nsIStreamListener> listener = mListener.forget();

  MOZ_ASSERT(mChannel);

  if (!aStream) {
    // The parent didn't send us back a stream.
    listener->OnStartRequest(mChannel, nullptr);
    listener->OnStopRequest(mChannel, nullptr, NS_ERROR_FILE_ACCESS_DENIED);
    mChannel->Cancel(NS_BINDING_ABORTED);
    return;
  }

  nsCOMPtr<nsIInputStreamPump> pump;
  nsresult rv = NS_NewInputStreamPump(getter_AddRefs(pump), aStream, -1, -1, 0,
                                      0, false, mMainThreadEventTarget);
  if (NS_FAILED(rv)) {
    mChannel->Cancel(NS_BINDING_ABORTED);
    return;
  }

  rv = pump->AsyncRead(listener, nullptr);
  if (NS_FAILED(rv)) {
    mChannel->Cancel(NS_BINDING_ABORTED);
  }
}

// Handle an FD sent from the parent.
void
ExtensionStreamGetter::OnFD(const FileDescriptor& aFD)
{
  MOZ_ASSERT(IsNeckoChild());
  MOZ_ASSERT(mListener);
  MOZ_ASSERT(mChannel);

  if (!aFD.IsValid()) {
    OnStream(nullptr);
    return;
  }

  // We must keep an owning reference to the listener
  // until we pass it on to AsyncOpen2.
  nsCOMPtr<nsIStreamListener> listener = mListener.forget();

  RefPtr<FileDescriptorFile> fdFile = new FileDescriptorFile(aFD, mJarFile);
  mJarChannel->SetJarFile(fdFile);
  nsresult rv = mJarChannel->AsyncOpen2(listener);
  if (NS_FAILED(rv)) {
    mChannel->Cancel(NS_BINDING_ABORTED);
  }
}

NS_IMPL_QUERY_INTERFACE(ExtensionProtocolHandler, nsISubstitutingProtocolHandler,
                        nsIProtocolHandler, nsIProtocolHandlerWithDynamicFlags,
                        nsISupportsWeakReference)
NS_IMPL_ADDREF_INHERITED(ExtensionProtocolHandler, SubstitutingProtocolHandler)
NS_IMPL_RELEASE_INHERITED(ExtensionProtocolHandler, SubstitutingProtocolHandler)

already_AddRefed<ExtensionProtocolHandler>
ExtensionProtocolHandler::GetSingleton()
{
  if (!sSingleton) {
    sSingleton = new ExtensionProtocolHandler();
    ClearOnShutdown(&sSingleton);
  }
  return do_AddRef(sSingleton.get());
}

ExtensionProtocolHandler::ExtensionProtocolHandler()
  : SubstitutingProtocolHandler(EXTENSION_SCHEME)
#if !defined(XP_WIN) && defined(MOZ_CONTENT_SANDBOX)
  , mAlreadyCheckedDevRepo(false)
#endif
{
  mUseRemoteFileChannels = IsNeckoChild() &&
    Preferences::GetBool("extensions.webextensions.protocol.remote");
}

static inline ExtensionPolicyService&
EPS()
{
  return ExtensionPolicyService::GetSingleton();
}

nsresult
ExtensionProtocolHandler::GetFlagsForURI(nsIURI* aURI, uint32_t* aFlags)
{
  // In general a moz-extension URI is only loadable by chrome, but a whitelisted
  // subset are web-accessible (and cross-origin fetchable). Check that whitelist.
  bool loadableByAnyone = false;

  URLInfo url(aURI);
  if (auto* policy = EPS().GetByURL(url)) {
    loadableByAnyone = policy->IsPathWebAccessible(url.FilePath());
  }

  *aFlags = URI_STD | URI_IS_LOCAL_RESOURCE | (loadableByAnyone ? (URI_LOADABLE_BY_ANYONE | URI_FETCHABLE_BY_ANYONE) : URI_DANGEROUS_TO_LOAD);
  return NS_OK;
}

bool
ExtensionProtocolHandler::ResolveSpecialCases(const nsACString& aHost,
                                              const nsACString& aPath,
                                              const nsACString& aPathname,
                                              nsACString& aResult)
{
  // Create special moz-extension:-pages such as moz-extension://foo/_blank.html
  // for all registered extensions. We can't just do this as a substitution
  // because substitutions can only match on host.
  if (!SubstitutingProtocolHandler::HasSubstitution(aHost)) {
    return false;
  }

  if (aPathname.EqualsLiteral("/_blank.html")) {
    aResult.AssignLiteral("about:blank");
    return true;
  }

  if (aPathname.EqualsLiteral("/_generated_background_page.html")) {
    Unused << EPS().GetGeneratedBackgroundPageUrl(aHost, aResult);
    return !aResult.IsEmpty();
  }

  return false;
}

// For file or JAR URI's, substitute in a remote channel.
Result<Ok, nsresult>
ExtensionProtocolHandler::SubstituteRemoteChannel(nsIURI* aURI,
                                                  nsILoadInfo* aLoadInfo,
                                                  nsIChannel** aRetVal)
{
  MOZ_ASSERT(IsNeckoChild());
  NS_TRY(aURI ? NS_OK : NS_ERROR_INVALID_ARG);
  NS_TRY(aLoadInfo ? NS_OK : NS_ERROR_INVALID_ARG);

  nsAutoCString unResolvedSpec;
  NS_TRY(aURI->GetSpec(unResolvedSpec));

  nsAutoCString resolvedSpec;
  NS_TRY(ResolveURI(aURI, resolvedSpec));

  // Use the target URI scheme to determine if this is a packed or unpacked
  // extension URI. For unpacked extensions, we'll request an input stream
  // from the parent. For a packed extension, we'll request a file descriptor
  // for the JAR file.
  nsAutoCString scheme;
  NS_TRY(net_ExtractURLScheme(resolvedSpec, scheme));

  if (scheme.EqualsLiteral("file")) {
    // Unpacked extension
    SubstituteRemoteFileChannel(aURI, aLoadInfo, resolvedSpec, aRetVal);
    return Ok();
  }

  if (scheme.EqualsLiteral("jar")) {
    // Packed extension
    return SubstituteRemoteJarChannel(aURI, aLoadInfo, resolvedSpec, aRetVal);
  }

  // Only unpacked resource files and JAR files are remoted.
  // No other moz-extension loads should be reading from the filesystem.
  return Ok();
}

nsresult
ExtensionProtocolHandler::SubstituteChannel(nsIURI* aURI,
                                            nsILoadInfo* aLoadInfo,
                                            nsIChannel** result)
{
  nsresult rv;
  nsCOMPtr<nsIURL> url = do_QueryInterface(aURI, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mUseRemoteFileChannels) {
    MOZ_TRY(SubstituteRemoteChannel(aURI, aLoadInfo, result));
  }

  nsAutoCString ext;
  rv = url->GetFileExtension(ext);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!ext.LowerCaseEqualsLiteral("css")) {
    return NS_OK;
  }

  // Filter CSS files to replace locale message tokens with localized strings.

  bool haveLoadInfo = aLoadInfo;
  nsCOMPtr<nsIChannel> channel = NS_NewSimpleChannel(
    aURI, aLoadInfo, *result,
    [haveLoadInfo] (nsIStreamListener* listener, nsIChannel* channel, nsIChannel* origChannel) -> RequestOrReason {
      nsresult rv;
      nsCOMPtr<nsIStreamConverterService> convService =
        do_GetService(NS_STREAMCONVERTERSERVICE_CONTRACTID, &rv);
      NS_TRY(rv);

      nsCOMPtr<nsIURI> uri;
      NS_TRY(channel->GetURI(getter_AddRefs(uri)));

      const char* kFromType = "application/vnd.mozilla.webext.unlocalized";
      const char* kToType = "text/css";

      nsCOMPtr<nsIStreamListener> converter;
      NS_TRY(convService->AsyncConvertData(kFromType, kToType, listener,
                                        uri, getter_AddRefs(converter)));
      if (haveLoadInfo) {
        NS_TRY(origChannel->AsyncOpen2(converter));
      } else {
        NS_TRY(origChannel->AsyncOpen(converter, nullptr));
      }

      return RequestOrReason(origChannel);
    });
  NS_ENSURE_TRUE(channel, NS_ERROR_OUT_OF_MEMORY);

  if (aLoadInfo) {
    nsCOMPtr<nsILoadInfo> loadInfo =
        static_cast<LoadInfo*>(aLoadInfo)->CloneForNewRequest();
    (*result)->SetLoadInfo(loadInfo);
  }

  channel.swap(*result);

  return NS_OK;
}

#if !defined(XP_WIN) && defined(MOZ_CONTENT_SANDBOX)
// The |aRequestedFile| argument must already be Normalize()'d
Result<Ok, nsresult>
ExtensionProtocolHandler::DevRepoContains(nsIFile* aRequestedFile,
                                          bool *aResult)
{
  MOZ_ASSERT(!IsNeckoChild());
  MOZ_ASSERT(aResult);
  *aResult = false;

  // On the first invocation, set mDevRepo if this is a development build
  if (!mAlreadyCheckedDevRepo) {
    mAlreadyCheckedDevRepo = true;
    if (mozilla::IsDevelopmentBuild()) {
      NS_TRY(mozilla::GetRepoDir(getter_AddRefs(mDevRepo)));
    }
  }

  if (mDevRepo) {
    // This is a development build
    NS_TRY(mDevRepo->Contains(aRequestedFile, aResult));
  }

  return Ok();
}
#endif /* !defined(XP_WIN) && defined(MOZ_CONTENT_SANDBOX) */

Result<nsCOMPtr<nsIInputStream>, nsresult>
ExtensionProtocolHandler::NewStream(nsIURI* aChildURI, bool* aTerminateSender)
{
  MOZ_ASSERT(!IsNeckoChild());
  NS_TRY(aChildURI ? NS_OK : NS_ERROR_INVALID_ARG);
  NS_TRY(aTerminateSender ? NS_OK : NS_ERROR_INVALID_ARG);

  *aTerminateSender = true;
  nsresult rv;

  // We should never receive a URI that isn't for a moz-extension because
  // these requests ordinarily come from the child's ExtensionProtocolHandler.
  // Ensure this request is for a moz-extension URI. A rogue child process
  // could send us any URI.
  bool isExtScheme = false;
  if (NS_FAILED(aChildURI->SchemeIs(EXTENSION_SCHEME, &isExtScheme)) ||
      !isExtScheme) {
    return Err(NS_ERROR_UNKNOWN_PROTOCOL);
  }

  // For errors after this point, we want to propagate the error to
  // the child, but we don't force the child to be terminated because
  // the error is likely to be due to a bug in the extension.
  *aTerminateSender = false;

  /*
   * Make sure there is a substitution installed for the host found
   * in the child's request URI and make sure the host resolves to
   * a directory.
   */

  nsAutoCString host;
  NS_TRY(aChildURI->GetAsciiHost(host));

  // Lookup the directory this host string resolves to
  nsCOMPtr<nsIURI> baseURI;
  NS_TRY(GetSubstitution(host, getter_AddRefs(baseURI)));

  // The result should be a file URL for the extension base dir
  nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(baseURI, &rv);
  NS_TRY(rv);

  nsCOMPtr<nsIFile> extensionDir;
  NS_TRY(fileURL->GetFile(getter_AddRefs(extensionDir)));

  bool isDirectory = false;
  NS_TRY(extensionDir->IsDirectory(&isDirectory));
  if (!isDirectory) {
    // The host should map to a directory for unpacked extensions
    return Err(NS_ERROR_FILE_NOT_DIRECTORY);
  }

  /*
   * Now get a channel for the resolved child URI and make sure the
   * channel is a file channel.
   */

  // We use the system principal to get a file channel for the request,
  // but only after we've checked (above) that the child URI is of
  // moz-extension scheme and that the URI host maps to a directory.
  nsCOMPtr<nsIChannel> channel;
  NS_TRY(NS_NewChannel(getter_AddRefs(channel),
                       aChildURI,
                       nsContentUtils::GetSystemPrincipal(),
                       nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
                       nsIContentPolicy::TYPE_OTHER));

  // Channel should be a file channel. It should never be a JAR
  // channel because we only request remote streams for unpacked
  // extension resource loads where the URI resolves to a file.
  nsCOMPtr<nsIFileChannel> fileChannel = do_QueryInterface(channel, &rv);
  NS_TRY(rv);

  nsCOMPtr<nsIFile> requestedFile;
  NS_TRY(fileChannel->GetFile(getter_AddRefs(requestedFile)));

  /*
   * Make sure the file we resolved to is within the extension directory.
   */

  // Normalize paths for sane comparisons. nsIFile::Contains depends on
  // it for reliable subpath checks.
  NS_TRY(extensionDir->Normalize());
  NS_TRY(requestedFile->Normalize());
#if defined(XP_WIN)
  if (!widget::WinUtils::ResolveJunctionPointsAndSymLinks(extensionDir) ||
      !widget::WinUtils::ResolveJunctionPointsAndSymLinks(requestedFile)) {
    return Err(NS_ERROR_FILE_ACCESS_DENIED);
  }
#endif

  bool isResourceFromExtensionDir = false;
  NS_TRY(extensionDir->Contains(requestedFile, &isResourceFromExtensionDir));
  if (!isResourceFromExtensionDir) {
#if defined(XP_WIN)
    return Err(NS_ERROR_FILE_ACCESS_DENIED);
#elif defined(MOZ_CONTENT_SANDBOX)
    // On a dev build, we allow an unpacked resource that isn't
    // from the extension directory as long as it is from the repo.
    bool isResourceFromDevRepo = false;
    MOZ_TRY(DevRepoContains(requestedFile, &isResourceFromDevRepo));
    if (!isResourceFromDevRepo) {
      return Err(NS_ERROR_FILE_ACCESS_DENIED);
    }
#endif /* defined(XP_WIN) */
  }

  nsCOMPtr<nsIInputStream> inputStream;
  NS_TRY(NS_NewLocalFileInputStream(getter_AddRefs(inputStream),
                                    requestedFile,
                                    PR_RDONLY,
                                    -1,
                                    nsIFileInputStream::DEFER_OPEN));

  return inputStream;
}

Result<Ok, nsresult>
ExtensionProtocolHandler::NewFD(nsIURI* aChildURI,
                                bool* aTerminateSender,
                                NeckoParent::GetExtensionFDResolver& aResolve)
{
  MOZ_ASSERT(!IsNeckoChild());
  NS_TRY(aChildURI ? NS_OK : NS_ERROR_INVALID_ARG);
  NS_TRY(aTerminateSender ? NS_OK : NS_ERROR_INVALID_ARG);

  *aTerminateSender = true;
  nsresult rv;

  // Ensure this is a moz-extension URI
  bool isExtScheme = false;
  if (NS_FAILED(aChildURI->SchemeIs(EXTENSION_SCHEME, &isExtScheme)) ||
      !isExtScheme) {
    return Err(NS_ERROR_UNKNOWN_PROTOCOL);
  }

  // For errors after this point, we want to propagate the error to
  // the child, but we don't force the child to be terminated.
  *aTerminateSender = false;

  nsAutoCString host;
  NS_TRY(aChildURI->GetAsciiHost(host));

  // We expect the host string to map to a JAR file because the URI
  // should refer to a web accessible resource for an enabled extension.
  nsCOMPtr<nsIURI> subURI;
  NS_TRY(GetSubstitution(host, getter_AddRefs(subURI)));

  nsCOMPtr<nsIJARURI> jarURI = do_QueryInterface(subURI, &rv);
  NS_TRY(rv);

  nsCOMPtr<nsIURI> innerFileURI;
  NS_TRY(jarURI->GetJARFile(getter_AddRefs(innerFileURI)));

  nsCOMPtr<nsIFileURL> innerFileURL = do_QueryInterface(innerFileURI, &rv);
  NS_TRY(rv);

  nsCOMPtr<nsIFile> jarFile;
  NS_TRY(innerFileURL->GetFile(getter_AddRefs(jarFile)));

  if (!mFileOpenerThread) {
    mFileOpenerThread =
      new LazyIdleThread(DEFAULT_THREAD_TIMEOUT_MS,
                         NS_LITERAL_CSTRING("ExtensionProtocolHandler"));
  }

  RefPtr<ExtensionJARFileOpener> fileOpener =
    new ExtensionJARFileOpener(jarFile, aResolve);

  nsCOMPtr<nsIRunnable> event =
    mozilla::NewRunnableMethod("ExtensionJarFileOpener",
        fileOpener, &ExtensionJARFileOpener::OpenFile);

  NS_TRY(mFileOpenerThread->Dispatch(event, nsIEventTarget::DISPATCH_NORMAL));

  return Ok();
}

static void
NewSimpleChannel(nsIURI* aURI,
                 nsILoadInfo* aLoadinfo,
                 ExtensionStreamGetter* aStreamGetter,
                 nsIChannel** aRetVal)
{
  nsCOMPtr<nsIChannel> channel = NS_NewSimpleChannel(
    aURI, aLoadinfo, aStreamGetter,
    [] (nsIStreamListener* listener, nsIChannel* channel,
        ExtensionStreamGetter* getter) -> RequestOrReason {
      MOZ_TRY(getter->GetAsync(listener, channel));
      return RequestOrReason(nullptr);
    });

  nsresult rv;
  nsCOMPtr<nsIMIMEService> mime = do_GetService("@mozilla.org/mime;1", &rv);
  if (NS_SUCCEEDED(rv)) {
    nsAutoCString contentType;
    rv = mime->GetTypeFromURI(aURI, contentType);
    if (NS_SUCCEEDED(rv)) {
      Unused << channel->SetContentType(contentType);
    }
  }

  channel.swap(*aRetVal);
}

void
ExtensionProtocolHandler::SubstituteRemoteFileChannel(nsIURI* aURI,
                                                      nsILoadInfo* aLoadinfo,
                                                      nsACString& aResolvedFileSpec,
                                                      nsIChannel** aRetVal)
{
  MOZ_ASSERT(IsNeckoChild());

  RefPtr<ExtensionStreamGetter> streamGetter =
    new ExtensionStreamGetter(aURI, aLoadinfo);

  NewSimpleChannel(aURI, aLoadinfo, streamGetter, aRetVal);
}

static Result<Ok, nsresult>
LogCacheCheck(const nsIJARChannel* aJarChannel,
              nsIJARURI* aJarURI,
              bool aIsCached)
{
  nsresult rv;

  nsCOMPtr<nsIURI> innerFileURI;
  NS_TRY(aJarURI->GetJARFile(getter_AddRefs(innerFileURI)));

  nsCOMPtr<nsIFileURL> innerFileURL = do_QueryInterface(innerFileURI, &rv);
  NS_TRY(rv);

  nsCOMPtr<nsIFile> jarFile;
  NS_TRY(innerFileURL->GetFile(getter_AddRefs(jarFile)));

  nsAutoCString uriSpec, jarSpec;
  Unused << aJarURI->GetSpec(uriSpec);
  Unused << innerFileURI->GetSpec(jarSpec);
  LOG("[JARChannel %p] Cache %s: %s (%s)",
      aJarChannel, aIsCached ? "hit" : "miss", uriSpec.get(), jarSpec.get());

  return Ok();
}

Result<Ok, nsresult>
ExtensionProtocolHandler::SubstituteRemoteJarChannel(nsIURI* aURI,
                                                     nsILoadInfo* aLoadinfo,
                                                     nsACString& aResolvedSpec,
                                                     nsIChannel** aRetVal)
{
  MOZ_ASSERT(IsNeckoChild());
  nsresult rv;

  // Build a JAR URI for this jar:file:// URI and use it to extract the
  // inner file URI.
  nsCOMPtr<nsIURI> uri;
  NS_TRY(NS_NewURI(getter_AddRefs(uri), aResolvedSpec));

  nsCOMPtr<nsIJARURI> jarURI = do_QueryInterface(uri, &rv);
  NS_TRY(rv);

  nsCOMPtr<nsIJARChannel> jarChannel = do_QueryInterface(*aRetVal, &rv);
  NS_TRY(rv);

  bool isCached = false;
  NS_TRY(jarChannel->EnsureCached(&isCached));
  if (MOZ_LOG_TEST(gExtProtocolLog, LogLevel::Debug)) {
    Unused << LogCacheCheck(jarChannel, jarURI, isCached);
  }
  if (isCached) {
    return Ok();
  }

  nsCOMPtr<nsIURI> innerFileURI;
  NS_TRY(jarURI->GetJARFile(getter_AddRefs(innerFileURI)));

  nsCOMPtr<nsIFileURL> innerFileURL = do_QueryInterface(innerFileURI, &rv);
  NS_TRY(rv);

  nsCOMPtr<nsIFile> jarFile;
  NS_TRY(innerFileURL->GetFile(getter_AddRefs(jarFile)));

  RefPtr<ExtensionStreamGetter> streamGetter =
    new ExtensionStreamGetter(aURI, aLoadinfo, jarChannel.forget(), jarFile);

  NewSimpleChannel(aURI, aLoadinfo, streamGetter, aRetVal);
  return Ok();
}

#undef NS_TRY

} // namespace net
} // namespace mozilla
