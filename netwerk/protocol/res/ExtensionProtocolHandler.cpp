/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ExtensionProtocolHandler.h"

#include "mozilla/BinarySearch.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/Promise-inl.h"
#include "mozilla/ExtensionPolicyService.h"
#include "mozilla/FileUtils.h"
#include "mozilla/ipc/IPCStreamUtils.h"
#include "mozilla/ipc/URIParams.h"
#include "mozilla/ipc/URIUtils.h"
#include "mozilla/net/NeckoChild.h"
#include "mozilla/RefPtr.h"
#include "mozilla/ResultExtensions.h"

#include "FileDescriptor.h"
#include "FileDescriptorFile.h"
#include "LoadInfo.h"
#include "nsContentUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsDirectoryServiceDefs.h"
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
#include "nsReadableUtils.h"
#include "nsURLHelper.h"
#include "prio.h"
#include "SimpleChannel.h"

#if defined(XP_WIN)
#  include "nsILocalFileWin.h"
#  include "WinUtils.h"
#endif

#define EXTENSION_SCHEME "moz-extension"
using mozilla::dom::Promise;
using mozilla::ipc::FileDescriptor;

// A list of file extensions containing purely static data, which can be loaded
// from an extension before the extension is fully ready. The main purpose of
// this is to allow image resources from theme XPIs to load quickly during
// browser startup.
//
// The layout of this array is chosen in order to prevent the need for runtime
// relocation, which an array of char* pointers would require. It also has the
// benefit of being more compact when the difference in length between the
// longest and average string is less than 8 bytes. The length of the
// char[] array must match the size of the longest entry in the list.
//
// This list must be kept sorted.
static const char sStaticFileExtensions[][5] = {
    // clang-format off
  "bmp",
  "gif",
  "ico",
  "jpeg",
  "jpg",
  "png",
  "svg",
    // clang-format on
};

namespace mozilla {

namespace net {

using extensions::URLInfo;

LazyLogModule gExtProtocolLog("ExtProtocol");
#undef LOG
#define LOG(...) MOZ_LOG(gExtProtocolLog, LogLevel::Debug, (__VA_ARGS__))

StaticRefPtr<ExtensionProtocolHandler> ExtensionProtocolHandler::sSingleton;

/**
 * Helper class used with SimpleChannel to asynchronously obtain an input
 * stream or file descriptor from the parent for a remote moz-extension load
 * from the child.
 */
class ExtensionStreamGetter : public RefCounted<ExtensionStreamGetter> {
 public:
  // To use when getting a remote input stream for a resource
  // in an unpacked extension.
  ExtensionStreamGetter(nsIURI* aURI, nsILoadInfo* aLoadInfo)
      : mURI(aURI), mLoadInfo(aLoadInfo), mIsJarChannel(false) {
    MOZ_ASSERT(aURI);
    MOZ_ASSERT(aLoadInfo);

    SetupEventTarget();
  }

  // To use when getting an FD for a packed extension JAR file
  // in order to load a resource.
  ExtensionStreamGetter(nsIURI* aURI, nsILoadInfo* aLoadInfo,
                        already_AddRefed<nsIJARChannel>&& aJarChannel,
                        nsIFile* aJarFile)
      : mURI(aURI),
        mLoadInfo(aLoadInfo),
        mJarChannel(std::move(aJarChannel)),
        mJarFile(aJarFile),
        mIsJarChannel(true) {
    MOZ_ASSERT(aURI);
    MOZ_ASSERT(aLoadInfo);
    MOZ_ASSERT(mJarChannel);
    MOZ_ASSERT(aJarFile);

    SetupEventTarget();
  }

  ~ExtensionStreamGetter() = default;

  void SetupEventTarget() {
    mMainThreadEventTarget = nsContentUtils::GetEventTargetByLoadInfo(
        mLoadInfo, TaskCategory::Other);
    if (!mMainThreadEventTarget) {
      mMainThreadEventTarget = GetMainThreadSerialEventTarget();
    }
  }

  // Get an input stream or file descriptor from the parent asynchronously.
  Result<Ok, nsresult> GetAsync(nsIStreamListener* aListener,
                                nsIChannel* aChannel);

  // Handle an input stream being returned from the parent
  void OnStream(already_AddRefed<nsIInputStream> aStream);

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

class ExtensionJARFileOpener final : public nsISupports {
 public:
  ExtensionJARFileOpener(nsIFile* aFile,
                         NeckoParent::GetExtensionFDResolver& aResolve)
      : mFile(aFile), mResolve(aResolve) {
    MOZ_ASSERT(aFile);
    MOZ_ASSERT(aResolve);
  }

  NS_IMETHOD OpenFile() {
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
        mozilla::NewRunnableMethod("ExtensionJarFileFDResolver", this,
                                   &ExtensionJARFileOpener::SendBackFD);

    rv = NS_DispatchToMainThread(event, nsIEventTarget::DISPATCH_NORMAL);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "NS_DispatchToMainThread");
    return NS_OK;
  }

  NS_IMETHOD SendBackFD() {
    MOZ_ASSERT(NS_IsMainThread());
    mResolve(mFD);
    mResolve = nullptr;
    return NS_OK;
  }

  NS_DECL_THREADSAFE_ISUPPORTS

 private:
  virtual ~ExtensionJARFileOpener() = default;

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
Result<Ok, nsresult> ExtensionStreamGetter::GetAsync(
    nsIStreamListener* aListener, nsIChannel* aChannel) {
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
        mMainThreadEventTarget, __func__,
        [self](const FileDescriptor& fd) { self->OnFD(fd); },
        [self](const mozilla::ipc::ResponseRejectReason) {
          self->OnFD(FileDescriptor());
        });
    return Ok();
  }

  // Request an input stream for this moz-extension URI
  gNeckoChild->SendGetExtensionStream(uri)->Then(
      mMainThreadEventTarget, __func__,
      [self](const RefPtr<nsIInputStream>& stream) {
        self->OnStream(do_AddRef(stream));
      },
      [self](const mozilla::ipc::ResponseRejectReason) {
        self->OnStream(nullptr);
      });
  return Ok();
}

static void CancelRequest(nsIStreamListener* aListener, nsIChannel* aChannel,
                          nsresult aResult) {
  MOZ_ASSERT(aListener);
  MOZ_ASSERT(aChannel);

  aListener->OnStartRequest(aChannel);
  aListener->OnStopRequest(aChannel, aResult);
  aChannel->Cancel(NS_BINDING_ABORTED);
}

// Handle an input stream sent from the parent.
void ExtensionStreamGetter::OnStream(already_AddRefed<nsIInputStream> aStream) {
  MOZ_ASSERT(IsNeckoChild());
  MOZ_ASSERT(mListener);
  MOZ_ASSERT(mMainThreadEventTarget);

  nsCOMPtr<nsIInputStream> stream = std::move(aStream);

  // We must keep an owning reference to the listener
  // until we pass it on to AsyncRead.
  nsCOMPtr<nsIStreamListener> listener = mListener.forget();

  MOZ_ASSERT(mChannel);

  if (!stream) {
    // The parent didn't send us back a stream.
    CancelRequest(listener, mChannel, NS_ERROR_FILE_ACCESS_DENIED);
    return;
  }

  nsCOMPtr<nsIInputStreamPump> pump;
  nsresult rv = NS_NewInputStreamPump(getter_AddRefs(pump), stream.forget(), 0,
                                      0, false, mMainThreadEventTarget);
  if (NS_FAILED(rv)) {
    CancelRequest(listener, mChannel, rv);
    return;
  }

  rv = pump->AsyncRead(listener, nullptr);
  if (NS_FAILED(rv)) {
    CancelRequest(listener, mChannel, rv);
  }
}

// Handle an FD sent from the parent.
void ExtensionStreamGetter::OnFD(const FileDescriptor& aFD) {
  MOZ_ASSERT(IsNeckoChild());
  MOZ_ASSERT(mListener);
  MOZ_ASSERT(mChannel);

  if (!aFD.IsValid()) {
    OnStream(nullptr);
    return;
  }

  // We must keep an owning reference to the listener
  // until we pass it on to AsyncOpen.
  nsCOMPtr<nsIStreamListener> listener = mListener.forget();

  RefPtr<FileDescriptorFile> fdFile = new FileDescriptorFile(aFD, mJarFile);
  mJarChannel->SetJarFile(fdFile);
  nsresult rv = mJarChannel->AsyncOpen(listener);
  if (NS_FAILED(rv)) {
    CancelRequest(listener, mChannel, rv);
  }
}

NS_IMPL_QUERY_INTERFACE(ExtensionProtocolHandler,
                        nsISubstitutingProtocolHandler, nsIProtocolHandler,
                        nsIProtocolHandlerWithDynamicFlags,
                        nsISupportsWeakReference)
NS_IMPL_ADDREF_INHERITED(ExtensionProtocolHandler, SubstitutingProtocolHandler)
NS_IMPL_RELEASE_INHERITED(ExtensionProtocolHandler, SubstitutingProtocolHandler)

already_AddRefed<ExtensionProtocolHandler>
ExtensionProtocolHandler::GetSingleton() {
  if (!sSingleton) {
    sSingleton = new ExtensionProtocolHandler();
    ClearOnShutdown(&sSingleton);
  }
  return do_AddRef(sSingleton);
}

ExtensionProtocolHandler::ExtensionProtocolHandler()
    : SubstitutingProtocolHandler(EXTENSION_SCHEME)
#if !defined(XP_WIN)
#  if defined(XP_MACOSX)
      ,
      mAlreadyCheckedDevRepo(false)
#  endif /* XP_MACOSX */
      ,
      mAlreadyCheckedAppDir(false)
#endif /* ! XP_WIN */
{
  // Note, extensions.webextensions.protocol.remote=false is for
  // debugging purposes only. With process-level sandboxing, child
  // processes (specifically content and extension processes), will
  // not be able to load most moz-extension URI's when the pref is
  // set to false.
  mUseRemoteFileChannels =
      IsNeckoChild() &&
      Preferences::GetBool("extensions.webextensions.protocol.remote");
}

static inline ExtensionPolicyService& EPS() {
  return ExtensionPolicyService::GetSingleton();
}

nsresult ExtensionProtocolHandler::GetFlagsForURI(nsIURI* aURI,
                                                  uint32_t* aFlags) {
  uint32_t flags =
      URI_STD | URI_IS_LOCAL_RESOURCE | URI_IS_POTENTIALLY_TRUSTWORTHY;

  URLInfo url(aURI);
  if (auto* policy = EPS().GetByURL(url)) {
    // In general a moz-extension URI is only loadable by chrome, but a
    // whitelisted subset are web-accessible (and cross-origin fetchable). Check
    // that whitelist.
    if (policy->IsPathWebAccessible(url.FilePath())) {
      flags |= URI_LOADABLE_BY_ANYONE | URI_FETCHABLE_BY_ANYONE;
    } else {
      flags |= URI_DANGEROUS_TO_LOAD;
    }

    // Disallow in private windows if the extension does not have permission.
    if (!policy->PrivateBrowsingAllowed()) {
      flags |= URI_DISALLOW_IN_PRIVATE_CONTEXT;
    }
  }

  *aFlags = flags;
  return NS_OK;
}

bool ExtensionProtocolHandler::ResolveSpecialCases(const nsACString& aHost,
                                                   const nsACString& aPath,
                                                   const nsACString& aPathname,
                                                   nsACString& aResult) {
  // Create special moz-extension://foo/_generated_background_page.html page
  // for all registered extensions. We can't just do this as a substitution
  // because substitutions can only match on host.
  if (!SubstitutingProtocolHandler::HasSubstitution(aHost)) {
    return false;
  }

  if (aPathname.EqualsLiteral("/_generated_background_page.html")) {
    Unused << EPS().GetGeneratedBackgroundPageUrl(aHost, aResult);
    return !aResult.IsEmpty();
  }

  return false;
}

// For file or JAR URI's, substitute in a remote channel.
Result<Ok, nsresult> ExtensionProtocolHandler::SubstituteRemoteChannel(
    nsIURI* aURI, nsILoadInfo* aLoadInfo, nsIChannel** aRetVal) {
  MOZ_ASSERT(IsNeckoChild());
  MOZ_TRY(aURI ? NS_OK : NS_ERROR_INVALID_ARG);
  MOZ_TRY(aLoadInfo ? NS_OK : NS_ERROR_INVALID_ARG);

  nsAutoCString unResolvedSpec;
  MOZ_TRY(aURI->GetSpec(unResolvedSpec));

  nsAutoCString resolvedSpec;
  MOZ_TRY(ResolveURI(aURI, resolvedSpec));

  // Use the target URI scheme to determine if this is a packed or unpacked
  // extension URI. For unpacked extensions, we'll request an input stream
  // from the parent. For a packed extension, we'll request a file descriptor
  // for the JAR file.
  nsAutoCString scheme;
  MOZ_TRY(net_ExtractURLScheme(resolvedSpec, scheme));

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

void OpenWhenReady(
    Promise* aPromise, nsIStreamListener* aListener, nsIChannel* aChannel,
    const std::function<nsresult(nsIStreamListener*, nsIChannel*)>& aCallback) {
  nsCOMPtr<nsIStreamListener> listener(aListener);
  nsCOMPtr<nsIChannel> channel(aChannel);

  Unused << aPromise->ThenWithCycleCollectedArgs(
      [channel, aCallback](
          JSContext* aCx, JS::HandleValue aValue,
          nsIStreamListener* aListener) -> already_AddRefed<Promise> {
        nsresult rv = aCallback(aListener, channel);
        if (NS_FAILED(rv)) {
          CancelRequest(aListener, channel, rv);
        }
        return nullptr;
      },
      listener);
}

nsresult ExtensionProtocolHandler::SubstituteChannel(nsIURI* aURI,
                                                     nsILoadInfo* aLoadInfo,
                                                     nsIChannel** result) {
  if (mUseRemoteFileChannels) {
    MOZ_TRY(SubstituteRemoteChannel(aURI, aLoadInfo, result));
  }

  auto* policy = EPS().GetByURL(aURI);
  NS_ENSURE_TRUE(policy, NS_ERROR_UNEXPECTED);

  RefPtr<dom::Promise> readyPromise(policy->ReadyPromise());

  nsresult rv;
  nsCOMPtr<nsIURL> url = do_QueryInterface(aURI, &rv);
  MOZ_TRY(rv);

  nsAutoCString ext;
  MOZ_TRY(url->GetFileExtension(ext));
  ToLowerCase(ext);

  nsCOMPtr<nsIChannel> channel;
  if (ext.EqualsLiteral("css")) {
    // Filter CSS files to replace locale message tokens with localized strings.
    static const auto convert = [](nsIStreamListener* listener,
                                   nsIChannel* channel,
                                   nsIChannel* origChannel) -> nsresult {
      nsresult rv;
      nsCOMPtr<nsIStreamConverterService> convService =
          do_GetService(NS_STREAMCONVERTERSERVICE_CONTRACTID, &rv);
      MOZ_TRY(rv);

      nsCOMPtr<nsIURI> uri;
      MOZ_TRY(channel->GetURI(getter_AddRefs(uri)));

      const char* kFromType = "application/vnd.mozilla.webext.unlocalized";
      const char* kToType = "text/css";

      nsCOMPtr<nsIStreamListener> converter;
      MOZ_TRY(convService->AsyncConvertData(kFromType, kToType, listener, uri,
                                            getter_AddRefs(converter)));

      return origChannel->AsyncOpen(converter);
    };

    channel = NS_NewSimpleChannel(
        aURI, aLoadInfo, *result,
        [readyPromise](nsIStreamListener* listener, nsIChannel* channel,
                       nsIChannel* origChannel) -> RequestOrReason {
          if (readyPromise) {
            nsCOMPtr<nsIChannel> chan(channel);
            OpenWhenReady(
                readyPromise, listener, origChannel,
                [chan](nsIStreamListener* aListener, nsIChannel* aChannel) {
                  return convert(aListener, chan, aChannel);
                });
          } else {
            MOZ_TRY(convert(listener, channel, origChannel));
          }
          return RequestOrReason(origChannel);
        });
  } else if (readyPromise) {
    size_t matchIdx;
    if (BinarySearchIf(
            sStaticFileExtensions, 0, ArrayLength(sStaticFileExtensions),
            [&ext](const char* aOther) { return ext.Compare(aOther); },
            &matchIdx)) {
      // This is a static resource that shouldn't depend on the extension being
      // ready. Don't bother waiting for it.
      return NS_OK;
    }

    channel = NS_NewSimpleChannel(
        aURI, aLoadInfo, *result,
        [readyPromise](nsIStreamListener* listener, nsIChannel* channel,
                       nsIChannel* origChannel) -> RequestOrReason {
          OpenWhenReady(readyPromise, listener, origChannel,
                        [](nsIStreamListener* aListener, nsIChannel* aChannel) {
                          return aChannel->AsyncOpen(aListener);
                        });

          return RequestOrReason(origChannel);
        });
  } else {
    return NS_OK;
  }

  NS_ENSURE_TRUE(channel, NS_ERROR_OUT_OF_MEMORY);
  if (aLoadInfo) {
    nsCOMPtr<nsILoadInfo> loadInfo =
        static_cast<LoadInfo*>(aLoadInfo)->CloneForNewRequest();
    (*result)->SetLoadInfo(loadInfo);
  }

  channel.swap(*result);
  return NS_OK;
}

Result<bool, nsresult> ExtensionProtocolHandler::AllowExternalResource(
    nsIFile* aExtensionDir, nsIFile* aRequestedFile) {
  MOZ_ASSERT(!IsNeckoChild());

#if defined(XP_WIN)
  // On Windows, dev builds don't use symlinks so we never need to
  // allow a resource from outside of the extension dir.
  return false;
#else
  if (!mozilla::IsDevelopmentBuild()) {
    return false;
  }

  // On Mac and Linux unpackaged dev builds, system extensions use
  // symlinks to point to resources in the repo dir which we have to
  // allow loading. Before we allow an unpacked extension to load a
  // resource outside of the extension dir, we make sure the extension
  // dir is within the app directory.
  bool result;
  MOZ_TRY_VAR(result, AppDirContains(aExtensionDir));
  if (!result) {
    return false;
  }

#  if defined(XP_MACOSX)
  // Additionally, on Mac dev builds, we make sure that the requested
  // resource is within the repo dir. We don't perform this check on Linux
  // because we don't have a reliable path to the repo dir on Linux.
  return DevRepoContains(aRequestedFile);
#  else /* XP_MACOSX */
  return true;
#  endif
#endif /* defined(XP_WIN) */
}

#if defined(XP_MACOSX)
// The |aRequestedFile| argument must already be Normalize()'d
Result<bool, nsresult> ExtensionProtocolHandler::DevRepoContains(
    nsIFile* aRequestedFile) {
  MOZ_ASSERT(mozilla::IsDevelopmentBuild());
  MOZ_ASSERT(!IsNeckoChild());

  // On the first invocation, set mDevRepo
  if (!mAlreadyCheckedDevRepo) {
    mAlreadyCheckedDevRepo = true;
    MOZ_TRY(mozilla::GetRepoDir(getter_AddRefs(mDevRepo)));
    if (MOZ_LOG_TEST(gExtProtocolLog, LogLevel::Debug)) {
      nsAutoCString repoPath;
      Unused << mDevRepo->GetNativePath(repoPath);
      LOG("Repo path: %s", repoPath.get());
    }
  }

  bool result = false;
  if (mDevRepo) {
    MOZ_TRY(mDevRepo->Contains(aRequestedFile, &result));
  }
  return result;
}
#endif /* XP_MACOSX */

#if !defined(XP_WIN)
Result<bool, nsresult> ExtensionProtocolHandler::AppDirContains(
    nsIFile* aExtensionDir) {
  MOZ_ASSERT(mozilla::IsDevelopmentBuild());
  MOZ_ASSERT(!IsNeckoChild());

  // On the first invocation, set mAppDir
  if (!mAlreadyCheckedAppDir) {
    mAlreadyCheckedAppDir = true;
    MOZ_TRY(NS_GetSpecialDirectory(NS_GRE_DIR, getter_AddRefs(mAppDir)));
    if (MOZ_LOG_TEST(gExtProtocolLog, LogLevel::Debug)) {
      nsAutoCString appDirPath;
      Unused << mAppDir->GetNativePath(appDirPath);
      LOG("AppDir path: %s", appDirPath.get());
    }
  }

  bool result = false;
  if (mAppDir) {
    MOZ_TRY(mAppDir->Contains(aExtensionDir, &result));
  }
  return result;
}
#endif /* !defined(XP_WIN) */

static void LogExternalResourceError(nsIFile* aExtensionDir,
                                     nsIFile* aRequestedFile) {
  MOZ_ASSERT(aExtensionDir);
  MOZ_ASSERT(aRequestedFile);

  LOG("Rejecting external unpacked extension resource [%s] from "
      "extension directory [%s]",
      aRequestedFile->HumanReadablePath().get(),
      aExtensionDir->HumanReadablePath().get());
}

Result<nsCOMPtr<nsIInputStream>, nsresult> ExtensionProtocolHandler::NewStream(
    nsIURI* aChildURI, bool* aTerminateSender) {
  MOZ_ASSERT(!IsNeckoChild());
  MOZ_TRY(aChildURI ? NS_OK : NS_ERROR_INVALID_ARG);
  MOZ_TRY(aTerminateSender ? NS_OK : NS_ERROR_INVALID_ARG);

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
  MOZ_TRY(aChildURI->GetAsciiHost(host));

  // Lookup the directory this host string resolves to
  nsCOMPtr<nsIURI> baseURI;
  MOZ_TRY(GetSubstitution(host, getter_AddRefs(baseURI)));

  // The result should be a file URL for the extension base dir
  nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(baseURI, &rv);
  MOZ_TRY(rv);

  nsCOMPtr<nsIFile> extensionDir;
  MOZ_TRY(fileURL->GetFile(getter_AddRefs(extensionDir)));

  bool isDirectory = false;
  MOZ_TRY(extensionDir->IsDirectory(&isDirectory));
  if (!isDirectory) {
    // The host should map to a directory for unpacked extensions
    return Err(NS_ERROR_FILE_NOT_DIRECTORY);
  }

  // Make sure the child URI resolves to a file URI then get a file
  // channel for the request. The resultant channel should be a
  // file channel because we only request remote streams for unpacked
  // extension resource loads where the URI resolves to a file.
  nsAutoCString resolvedSpec;
  MOZ_TRY(ResolveURI(aChildURI, resolvedSpec));

  nsAutoCString resolvedScheme;
  MOZ_TRY(net_ExtractURLScheme(resolvedSpec, resolvedScheme));
  if (!resolvedScheme.EqualsLiteral("file")) {
    return Err(NS_ERROR_UNEXPECTED);
  }

  nsCOMPtr<nsIIOService> ioService = do_GetIOService(&rv);
  MOZ_TRY(rv);

  nsCOMPtr<nsIURI> resolvedURI;
  MOZ_TRY(ioService->NewURI(resolvedSpec, nullptr, nullptr,
                            getter_AddRefs(resolvedURI)));

  // We use the system principal to get a file channel for the request,
  // but only after we've checked (above) that the child URI is of
  // moz-extension scheme and that the URI host maps to a directory.
  nsCOMPtr<nsIChannel> channel;
  MOZ_TRY(NS_NewChannel(getter_AddRefs(channel), resolvedURI,
                        nsContentUtils::GetSystemPrincipal(),
                        nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
                        nsIContentPolicy::TYPE_OTHER));

  nsCOMPtr<nsIFileChannel> fileChannel = do_QueryInterface(channel, &rv);
  MOZ_TRY(rv);

  nsCOMPtr<nsIFile> requestedFile;
  MOZ_TRY(fileChannel->GetFile(getter_AddRefs(requestedFile)));

  /*
   * Make sure the file we resolved to is within the extension directory.
   */

  // Normalize paths for sane comparisons. nsIFile::Contains depends on
  // it for reliable subpath checks.
  MOZ_TRY(extensionDir->Normalize());
  MOZ_TRY(requestedFile->Normalize());
#if defined(XP_WIN)
  if (!widget::WinUtils::ResolveJunctionPointsAndSymLinks(extensionDir) ||
      !widget::WinUtils::ResolveJunctionPointsAndSymLinks(requestedFile)) {
    return Err(NS_ERROR_FILE_ACCESS_DENIED);
  }
#endif

  bool isResourceFromExtensionDir = false;
  MOZ_TRY(extensionDir->Contains(requestedFile, &isResourceFromExtensionDir));
  if (!isResourceFromExtensionDir) {
    bool isAllowed;
    MOZ_TRY_VAR(isAllowed, AllowExternalResource(extensionDir, requestedFile));
    if (!isAllowed) {
      LogExternalResourceError(extensionDir, requestedFile);
      return Err(NS_ERROR_FILE_ACCESS_DENIED);
    }
  }

  nsCOMPtr<nsIInputStream> inputStream;
  MOZ_TRY(NS_NewLocalFileInputStream(getter_AddRefs(inputStream), requestedFile,
                                     PR_RDONLY, -1,
                                     nsIFileInputStream::DEFER_OPEN));

  return inputStream;
}

Result<Ok, nsresult> ExtensionProtocolHandler::NewFD(
    nsIURI* aChildURI, bool* aTerminateSender,
    NeckoParent::GetExtensionFDResolver& aResolve) {
  MOZ_ASSERT(!IsNeckoChild());
  MOZ_TRY(aChildURI ? NS_OK : NS_ERROR_INVALID_ARG);
  MOZ_TRY(aTerminateSender ? NS_OK : NS_ERROR_INVALID_ARG);

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
  MOZ_TRY(aChildURI->GetAsciiHost(host));

  // We expect the host string to map to a JAR file because the URI
  // should refer to a web accessible resource for an enabled extension.
  nsCOMPtr<nsIURI> subURI;
  MOZ_TRY(GetSubstitution(host, getter_AddRefs(subURI)));

  nsCOMPtr<nsIJARURI> jarURI = do_QueryInterface(subURI, &rv);
  MOZ_TRY(rv);

  nsCOMPtr<nsIURI> innerFileURI;
  MOZ_TRY(jarURI->GetJARFile(getter_AddRefs(innerFileURI)));

  nsCOMPtr<nsIFileURL> innerFileURL = do_QueryInterface(innerFileURI, &rv);
  MOZ_TRY(rv);

  nsCOMPtr<nsIFile> jarFile;
  MOZ_TRY(innerFileURL->GetFile(getter_AddRefs(jarFile)));

  if (!mFileOpenerThread) {
    mFileOpenerThread =
        new LazyIdleThread(DEFAULT_THREAD_TIMEOUT_MS,
                           NS_LITERAL_CSTRING("ExtensionProtocolHandler"));
  }

  RefPtr<ExtensionJARFileOpener> fileOpener =
      new ExtensionJARFileOpener(jarFile, aResolve);

  nsCOMPtr<nsIRunnable> event = mozilla::NewRunnableMethod(
      "ExtensionJarFileOpener", fileOpener, &ExtensionJARFileOpener::OpenFile);

  MOZ_TRY(mFileOpenerThread->Dispatch(event, nsIEventTarget::DISPATCH_NORMAL));

  return Ok();
}

// Set the channel's content type using the provided URI's type
void SetContentType(nsIURI* aURI, nsIChannel* aChannel) {
  nsresult rv;
  nsCOMPtr<nsIMIMEService> mime = do_GetService("@mozilla.org/mime;1", &rv);
  if (NS_SUCCEEDED(rv)) {
    nsAutoCString contentType;
    rv = mime->GetTypeFromURI(aURI, contentType);
    if (NS_SUCCEEDED(rv)) {
      Unused << aChannel->SetContentType(contentType);
    }
  }
}

// Gets a SimpleChannel that wraps the provided ExtensionStreamGetter
static void NewSimpleChannel(nsIURI* aURI, nsILoadInfo* aLoadinfo,
                             ExtensionStreamGetter* aStreamGetter,
                             nsIChannel** aRetVal) {
  nsCOMPtr<nsIChannel> channel = NS_NewSimpleChannel(
      aURI, aLoadinfo, aStreamGetter,
      [](nsIStreamListener* listener, nsIChannel* simpleChannel,
         ExtensionStreamGetter* getter) -> RequestOrReason {
        MOZ_TRY(getter->GetAsync(listener, simpleChannel));
        return RequestOrReason(nullptr);
      });

  SetContentType(aURI, channel);
  channel.swap(*aRetVal);
}

// Gets a SimpleChannel that wraps the provided channel
static void NewSimpleChannel(nsIURI* aURI, nsILoadInfo* aLoadinfo,
                             nsIChannel* aChannel, nsIChannel** aRetVal) {
  nsCOMPtr<nsIChannel> channel = NS_NewSimpleChannel(
      aURI, aLoadinfo, aChannel,
      [](nsIStreamListener* listener, nsIChannel* simpleChannel,
         nsIChannel* origChannel) -> RequestOrReason {
        nsresult rv = origChannel->AsyncOpen(listener);
        if (NS_FAILED(rv)) {
          simpleChannel->Cancel(NS_BINDING_ABORTED);
          return RequestOrReason(rv);
        }
        return RequestOrReason(origChannel);
      });

  SetContentType(aURI, channel);
  channel.swap(*aRetVal);
}

void ExtensionProtocolHandler::SubstituteRemoteFileChannel(
    nsIURI* aURI, nsILoadInfo* aLoadinfo, nsACString& aResolvedFileSpec,
    nsIChannel** aRetVal) {
  MOZ_ASSERT(IsNeckoChild());

  RefPtr<ExtensionStreamGetter> streamGetter =
      new ExtensionStreamGetter(aURI, aLoadinfo);

  NewSimpleChannel(aURI, aLoadinfo, streamGetter, aRetVal);
}

static Result<Ok, nsresult> LogCacheCheck(const nsIJARChannel* aJarChannel,
                                          nsIJARURI* aJarURI, bool aIsCached) {
  nsresult rv;

  nsCOMPtr<nsIURI> innerFileURI;
  MOZ_TRY(aJarURI->GetJARFile(getter_AddRefs(innerFileURI)));

  nsCOMPtr<nsIFileURL> innerFileURL = do_QueryInterface(innerFileURI, &rv);
  MOZ_TRY(rv);

  nsCOMPtr<nsIFile> jarFile;
  MOZ_TRY(innerFileURL->GetFile(getter_AddRefs(jarFile)));

  nsAutoCString uriSpec, jarSpec;
  Unused << aJarURI->GetSpec(uriSpec);
  Unused << innerFileURI->GetSpec(jarSpec);
  LOG("[JARChannel %p] Cache %s: %s (%s)", aJarChannel,
      aIsCached ? "hit" : "miss", uriSpec.get(), jarSpec.get());

  return Ok();
}

Result<Ok, nsresult> ExtensionProtocolHandler::SubstituteRemoteJarChannel(
    nsIURI* aURI, nsILoadInfo* aLoadinfo, nsACString& aResolvedSpec,
    nsIChannel** aRetVal) {
  MOZ_ASSERT(IsNeckoChild());
  nsresult rv;

  // Build a JAR URI for this jar:file:// URI and use it to extract the
  // inner file URI.
  nsCOMPtr<nsIURI> uri;
  MOZ_TRY(NS_NewURI(getter_AddRefs(uri), aResolvedSpec));

  nsCOMPtr<nsIJARURI> jarURI = do_QueryInterface(uri, &rv);
  MOZ_TRY(rv);

  nsCOMPtr<nsIJARChannel> jarChannel = do_QueryInterface(*aRetVal, &rv);
  MOZ_TRY(rv);

  bool isCached = false;
  MOZ_TRY(jarChannel->EnsureCached(&isCached));
  if (MOZ_LOG_TEST(gExtProtocolLog, LogLevel::Debug)) {
    Unused << LogCacheCheck(jarChannel, jarURI, isCached);
  }

  if (isCached) {
    // Using a SimpleChannel with an ExtensionStreamGetter here (like the
    // non-cached JAR case) isn't needed to load the extension resource
    // because we don't need to ask the parent for an FD for the JAR, but
    // wrapping the JARChannel in a SimpleChannel allows HTTP forwarding to
    // moz-extension URI's to work because HTTP forwarding requires the
    // target channel implement nsIChildChannel.
    NewSimpleChannel(aURI, aLoadinfo, jarChannel.get(), aRetVal);
    return Ok();
  }

  nsCOMPtr<nsIURI> innerFileURI;
  MOZ_TRY(jarURI->GetJARFile(getter_AddRefs(innerFileURI)));

  nsCOMPtr<nsIFileURL> innerFileURL = do_QueryInterface(innerFileURI, &rv);
  MOZ_TRY(rv);

  nsCOMPtr<nsIFile> jarFile;
  MOZ_TRY(innerFileURL->GetFile(getter_AddRefs(jarFile)));

  RefPtr<ExtensionStreamGetter> streamGetter =
      new ExtensionStreamGetter(aURI, aLoadinfo, jarChannel.forget(), jarFile);

  NewSimpleChannel(aURI, aLoadinfo, streamGetter, aRetVal);
  return Ok();
}

}  // namespace net
}  // namespace mozilla
