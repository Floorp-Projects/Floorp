/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MacWebAppService.h"
#include "MacWebAppWidget.h"

#import <Cocoa/Cocoa.h>
#import <CoreServices/CoreServices.h>
#import <Security/Security.h>
#include <bsm/libbsm.h>
#include <servers/bootstrap.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <unordered_map>

#include "MachTransport.h"
#include "BundleTransaction.h"
#include "Protocol.h"
#include "Session.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Services.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPtr.h"
#include "nsIObserverService.h"
#include "nsGlobalWindowOuter.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"

namespace mozilla::widget {
namespace {

using floorp::shim::Message;
using floorp::shim::MessageType;
using floorp::shim::Port;

StaticMutex sSingletonMutex;
StaticRefPtr<MacWebAppService> sSingleton;
bool sShuttingDown = false;
nsCString sPendingAppId;
bool sInWindowFactory = false;
constexpr size_t kMaxApps = 64;

NSString* ToNSString(const nsACString& aValue) {
  return [[NSString alloc] initWithBytes:aValue.BeginReading()
                                length:aValue.Length()
                              encoding:NSUTF8StringEncoding];
}

NSString* ToNSString(const nsAString& aValue) {
  return [[NSString alloc]
      initWithCharacters:reinterpret_cast<const unichar*>(aValue.BeginReading())
                  length:aValue.Length()];
}

std::string AppKey(NSString* aValue) {
  return std::string(aValue.UTF8String ?: "");
}

bool IsHostControl(uint32_t aType) {
  return floorp::shim::IsKnownMessage(aType) &&
         aType >= static_cast<uint32_t>(MessageType::CreateWindow) &&
         aType <= static_cast<uint32_t>(MessageType::CancelQuit);
}

bool IsScriptControl(uint32_t aType) {
  switch (static_cast<MessageType>(aType)) {
    case MessageType::CloseWindow:
    case MessageType::SetWindowTitle:
    case MessageType::ActivateWindow:
    case MessageType::Terminate:
    case MessageType::CancelQuit:
      return true;
    default:
      return false;
  }
}

NSDictionary* StaticSigningInfo(NSURL* aURL) {
  SecStaticCodeRef code = nullptr;
  if (SecStaticCodeCreateWithPath((__bridge CFURLRef)aURL,
                                  kSecCSDefaultFlags, &code) != errSecSuccess) {
    return nil;
  }
  CFDictionaryRef information = nullptr;
  const bool valid =
      SecStaticCodeCheckValidity(code, kSecCSStrictValidate, nullptr) == errSecSuccess &&
      SecCodeCopySigningInformation(code, kSecCSSigningInformation,
                                   &information) == errSecSuccess;
  CFRelease(code);
  if (!valid) {
    if (information) CFRelease(information);
    return nil;
  }
  return CFBridgingRelease(information);
}

bool HostMatchesRequirement(NSString* aIdentifier, NSString* aRequirement) {
  if (!aIdentifier.length || !aRequirement.length) return false;
  SecRequirementRef requirement = nullptr;
  if (SecRequirementCreateWithString((__bridge CFStringRef)aRequirement,
                                     kSecCSDefaultFlags, &requirement) != errSecSuccess) {
    return false;
  }
  SecCodeRef self = nullptr;
  CFDictionaryRef information = nullptr;
  bool valid = SecCodeCopySelf(kSecCSDefaultFlags, &self) == errSecSuccess;
  if (valid) {
    valid = SecCodeCheckValidity(self, kSecCSStrictValidate, requirement) == errSecSuccess &&
            SecCodeCopySigningInformation(self, kSecCSSigningInformation,
                                         &information) == errSecSuccess;
  }
  if (self) CFRelease(self);
  CFRelease(requirement);
  if (!valid) {
    if (information) CFRelease(information);
    return false;
  }
  NSDictionary* info = CFBridgingRelease(information);
  return [info[(__bridge NSString*)kSecCodeInfoIdentifier] isEqual:aIdentifier];
}

NSDictionary* ValidatedSigningInfo(NSString* appId, NSString* profileId,
                                   NSString* bundlePath) {
  if (!floorp::shim::ValidIdentity(appId) || !bundlePath.isAbsolutePath ||
      ![bundlePath.pathExtension isEqual:@"app"] ||
      ![bundlePath isEqual:bundlePath.stringByStandardizingPath]) return nil;
  struct stat status;
  if (lstat(bundlePath.fileSystemRepresentation, &status) != 0 ||
      !S_ISDIR(status.st_mode) || status.st_uid != geteuid() ||
      (status.st_mode & (S_IWGRP | S_IWOTH))) return nil;
  NSURL* bundleURL = [NSURL fileURLWithPath:bundlePath isDirectory:YES];
  NSDictionary* signing = StaticSigningInfo(bundleURL);
  NSString* identifier = signing[(__bridge NSString*)kSecCodeInfoIdentifier];
  NSData* cdHash = signing[(__bridge NSString*)kSecCodeInfoUnique];
  NSDictionary* info = [NSDictionary dictionaryWithContentsOfURL:
      [bundleURL URLByAppendingPathComponent:@"Contents/Info.plist"]];
  if (![identifier isKindOfClass:NSString.class] || !identifier.length ||
      ![cdHash isKindOfClass:NSData.class] || !cdHash.length ||
      ![info[@"CFBundleIdentifier"] isEqual:identifier] ||
      ![info[@"FloorpAppShimAppId"] isEqual:appId] ||
      ![info[@"FloorpAppShimProfileId"] isEqual:profileId] ||
      !HostMatchesRequirement(
          floorp::shim::ReadString(info, @"FloorpAppShimHostBundleIdentifier", 512),
          floorp::shim::ReadString(info, @"FloorpAppShimHostCodeRequirement", 8192))) return nil;
  return signing;
}

}  // namespace

struct MacWebAppService::Impl {
  struct App {
    struct WindowSurfaces {
      bool closed = false;
      bool retired = false;
      std::map<uint32_t, RefPtr<layers::NativeLayerSurface>> surfaces;
    };
    std::map<uint32_t, WindowSurfaces> windows;
    __strong NSString* id = nil;
    __strong NSURL* bundleURL = nil;
    __strong NSString* signingIdentifier = nil;
    __strong NSData* cdHash = nil;
    __strong NSString* launchToken = nil;
    __strong NSRunningApplication* running = nil;
    dispatch_source_t processSource = nil;
    Port replyPort;
    std::unique_ptr<floorp::shim::MessageSender> sender;
    std::unique_ptr<Message> deferredHello;
    audit_token_t auditToken = {};
    uint64_t sentSequence = 0;
    uint64_t receivedSequence = 0;
    uint64_t connectionGeneration = 0;
    pid_t pid = 0;
    bool launching = false;
    bool cancelling = false;
    bool connected = false;

    void StopSending() {
      if (!sender) return;
      sender->Stop();
      sender.reset();
    }

    ~App() {
      StopSending();
      if (processSource) dispatch_source_cancel(processSource);
    }
  };

  explicit Impl(MacWebAppService* aOwner) : owner(aOwner) {}

  MacWebAppService* owner;
  __strong NSString* profileId = nil;
  __strong NSString* serviceName = nil;
  Port receivePort;
  dispatch_source_t receiveSource = nil;
  std::unordered_map<std::string, std::unique_ptr<App>> apps;

  App* Find(NSString* aAppId) {
    auto it = apps.find(AppKey(aAppId));
    return it == apps.end() ? nullptr : it->second.get();
  }

  void Notify(NSString* aAppId, id aType, NSDictionary* aPayload) {
    MOZ_ASSERT(NS_IsMainThread());
    NSData* data = floorp::shim::EncodePayload(@{
      @"appId": aAppId, @"type": aType, @"payload": aPayload
    });
    if (!data) return;
    nsDependentCSubstring json(static_cast<const char*>(data.bytes), data.length);
    NS_ConvertUTF8toUTF16 value(json);
    // Observer callbacks may stop the service or register another application.
    NS_DispatchToMainThread(NS_NewRunnableFunction(
        "MacWebAppService::Notify", [value = nsString(value)] {
          nsCOMPtr<nsIObserverService> observers = services::GetObserverService();
          if (observers) {
            observers->NotifyObservers(nullptr, "floorp-web-app-shim-event", value.get());
          }
        }));
  }

  bool VerifyPeer(const App& aApp, const audit_token_t& aToken) {
    const pid_t pid = audit_token_to_pid(aToken);
    if (pid <= 0 || audit_token_to_euid(aToken) != geteuid() ||
        (aApp.pid && aApp.pid != pid)) {
      return false;
    }
    NSData* audit = [NSData dataWithBytes:&aToken length:sizeof(aToken)];
    NSDictionary* attributes = @{(__bridge NSString*)kSecGuestAttributeAudit: audit};
    SecCodeRef guest = nullptr;
    CFDictionaryRef signing = nullptr;
    if (SecCodeCopyGuestWithAttributes(nullptr, (__bridge CFDictionaryRef)attributes,
                                      kSecCSDefaultFlags, &guest) != errSecSuccess) {
      return false;
    }
    bool valid = SecCodeCheckValidity(guest, kSecCSStrictValidate, nullptr) == errSecSuccess &&
                 SecCodeCopySigningInformation(guest, kSecCSSigningInformation,
                                              &signing) == errSecSuccess;
    CFRelease(guest);
    if (!valid) {
      if (signing) CFRelease(signing);
      return false;
    }
    NSDictionary* info = CFBridgingRelease(signing);
    return [info[(__bridge NSString*)kSecCodeInfoIdentifier] isEqual:aApp.signingIdentifier] &&
           [info[(__bridge NSString*)kSecCodeInfoUnique] isEqual:aApp.cdHash];
  }

  void PollCancelledExit(NSString* aAppId, NSString* aToken) {
    RefPtr<MacWebAppService> self = owner;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 250 * NSEC_PER_MSEC),
                   dispatch_get_main_queue(), ^{
      App* app = self->mImpl->Find(aAppId);
      if (!app || !app->cancelling || ![app->launchToken isEqual:aToken]) return;
      if (app->running.terminated) {
        self->mImpl->Disconnect(*app, @"process-exited");
      } else {
        self->mImpl->PollCancelledExit(aAppId, aToken);
      }
    });
  }

  void Disconnect(App& aApp, NSString* aReason) {
    // Stop is nonblocking and suppresses queued failure callbacks. An in-flight
    // kernel send can finish, but never becomes part of a later launch.
    aApp.StopSending();
    if (aApp.running && !aApp.running.terminated &&
        ![aReason isEqual:@"process-exited"]) {
      aApp.connected = false;
      aApp.launching = true;
      aApp.cancelling = true;
      aApp.replyPort = Port();
      aApp.deferredHello.reset();
      [aApp.running forceTerminate];
      if (!aApp.processSource) PollCancelledExit(aApp.id, aApp.launchToken);
      return;
    }
    aApp.connected = false;
    aApp.launching = false;
    aApp.cancelling = false;
    aApp.replyPort = Port();
    aApp.deferredHello.reset();
    aApp.launchToken = nil;
    aApp.running = nil;
    aApp.pid = 0;
    aApp.sentSequence = 0;
    aApp.receivedSequence = 0;
    // No consumer can still display these buffers after verified process exit.
    aApp.windows.clear();
    if (aApp.processSource) {
      dispatch_source_cancel(aApp.processSource);
      aApp.processSource = nil;
    }
    Notify(aApp.id, @"disconnected", @{@"reason": aReason});
  }

  bool Send(App& aApp, MessageType aType, NSDictionary* aPayload,
            mach_port_t aSurface = MACH_PORT_NULL) {
    MOZ_ASSERT(NS_IsMainThread());
    if (!aApp.connected || aApp.cancelling || !aApp.sender ||
        aApp.sentSequence == std::numeric_limits<uint64_t>::max()) {
      return false;
    }
    if (aType == MessageType::CreateWindow) {
      uint32_t windowId;
      if (!floorp::shim::ReadUInt(aPayload, @"windowId", &windowId) ||
          aApp.windows.count(windowId)) return false;
      // Closing windows remain accounted for until their completion arrives.
      if (aApp.windows.size() >= floorp::shim::kMaxWindows * 2) {
        Disconnect(aApp, @"window-retirement-limit");
        return false;
      }
      aApp.windows.emplace(windowId, App::WindowSurfaces{});
    }
    // The bounded per-connection worker owns copies of the payload and rights.
    // Never wait for another application's main queue while on Gecko's main
    // thread: AppKit activation can temporarily fill the peer's Mach queue.
    const bool success = aApp.sender->Enqueue(
        aType, ++aApp.sentSequence, aPayload, aSurface);
    if (!success) Disconnect(aApp, @"send-failed");
    return success;
  }

  void AcceptHello(App& aApp, Message aMessage) {
    NSString* nonce = floorp::shim::ReadString(aMessage.payload, @"nonce", 64);
    if (!aApp.launching || aApp.cancelling || aApp.connected || !aMessage.attachment ||
        aMessage.header.sequence != 1 || nonce.length != 64 ||
        !floorp::shim::ValidIdentity(nonce) ||
        ![aMessage.payload[@"appId"] isEqual:aApp.id] ||
        ![aMessage.payload[@"profileId"] isEqual:profileId] ||
        ![aMessage.payload[@"launchToken"] isEqual:aApp.launchToken] ||
        ![aMessage.payload[@"bundleIdentifier"] isEqual:aApp.signingIdentifier] ||
        !VerifyPeer(aApp, aMessage.auditToken)) {
      return;
    }
    // LaunchServices may deliver the launch callback after the child's Hello.
    if (!aApp.pid) {
      if (!aApp.deferredHello) {
        aApp.deferredHello = std::make_unique<Message>(std::move(aMessage));
      }
      return;
    }
    if (aApp.connectionGeneration == std::numeric_limits<uint64_t>::max()) {
      Disconnect(aApp, @"connection-generation-exhausted");
      return;
    }
    aApp.auditToken = aMessage.auditToken;
    aApp.replyPort = std::move(aMessage.attachment);
    aApp.receivedSequence = aMessage.header.sequence;
    const uint64_t generation = ++aApp.connectionGeneration;
    NSString* appId = aApp.id;
    NSString* launchToken = aApp.launchToken;
    RefPtr<MacWebAppService> self = owner;
    aApp.sender = std::make_unique<floorp::shim::MessageSender>(
        aApp.replyPort.get(), [self, appId, launchToken, generation] {
          MOZ_ASSERT(NS_IsMainThread());
          App* current = self->mImpl->Find(appId);
          if (!current || !current->connected || current->cancelling ||
              current->connectionGeneration != generation ||
              ![current->launchToken isEqual:launchToken]) {
            return;
          }
          self->mImpl->Disconnect(*current, @"send-failed");
        });
    aApp.connected = true;
    aApp.launching = false;
    if (Send(aApp, MessageType::HelloAccepted, @{
          @"appId": aApp.id, @"profileId": profileId, @"nonce": nonce,
          @"accepted": @YES
        })) {
      Notify(aApp.id, @"connected", @{@"pid": @(aApp.pid)});
    }
  }

  void Receive() {
    MOZ_ASSERT(NS_IsMainThread());
    for (size_t count = 0; count < 32 && receivePort; ++count) {
      auto result = floorp::shim::ReceiveMessage(receivePort.get(), 0);
      if (result.status == floorp::shim::ReceiveStatus::Timeout) return;
      if (result.status == floorp::shim::ReceiveStatus::Failed) return;
      if (!result.message) continue;
      Message message = std::move(*result.message);
      if (message.header.type == static_cast<uint32_t>(MessageType::Hello)) {
        NSString* appId = floorp::shim::ReadString(message.payload, @"appId", 128);
        if (App* app = Find(appId)) AcceptHello(*app, std::move(message));
        continue;
      }
      if (message.attachment || message.header.type < static_cast<uint32_t>(MessageType::Input)) {
        continue;
      }
      for (auto& [key, app] : apps) {
        if (!app->connected ||
            std::memcmp(&message.auditToken, &app->auditToken, sizeof(audit_token_t)) ||
            message.header.sequence <= app->receivedSequence) {
          continue;
        }
        app->receivedSequence = message.header.sequence;
        if (message.header.type == uint32_t(MessageType::WindowChanged) &&
            [message.payload[@"kind"] isEqual:@"closed"]) {
          uint32_t windowId;
          if (floorp::shim::ReadUInt(message.payload, @"windowId", &windowId)) {
            auto window = app->windows.find(windowId);
            if (window != app->windows.end()) {
              if (window->second.retired) app->windows.erase(window);
              else window->second.closed = true;
            }
          }
        }
        Notify(app->id, @(message.header.type), message.payload);
        break;
      }
    }
  }

  void Stop() {
    for (auto& [key, app] : apps) {
      // Graceful app quits enqueue Terminate before service teardown. At global
      // teardown the queue must stop immediately; terminate only the exact
      // launch instances we own instead of dropping a newly queued Terminate.
      if (app->launching && !app->running) {
        app->StopSending();
        app->cancelling = true;
        app->deferredHello.reset();
      } else {
        Disconnect(*app, @"service-stopped");
      }
    }
    // Keep registrations and process monitors until exit is confirmed. Widget
    // teardown can transfer its final surface leases after stop() returns.
    if (receiveSource) {
      dispatch_source_cancel(receiveSource);
      receiveSource = nil;
    }
    if (serviceName) {
      const kern_return_t ignored = bootstrap_register(
          bootstrap_port, const_cast<char*>(serviceName.UTF8String), MACH_PORT_NULL);
      (void)ignored;
    }
    receivePort = Port();
    profileId = nil;
    serviceName = nil;
  }
};

NS_IMPL_ISUPPORTS(MacWebAppService, nsIMacWebAppService, nsIObserver)

MacWebAppService::MacWebAppService() : mImpl(MakeUnique<Impl>(this)) {}
MacWebAppService::~MacWebAppService() = default;

already_AddRefed<MacWebAppService> MacWebAppService::GetSingleton() {
  StaticMutexAutoLock lock(sSingletonMutex);
  if (sShuttingDown || !XRE_IsParentProcess()) return nullptr;
  if (!sSingleton) {
    if (!NS_IsMainThread()) return nullptr;
    nsCOMPtr<nsIObserverService> observers = services::GetObserverService();
    if (!observers) return nullptr;
    sSingleton = new MacWebAppService();
    observers->AddObserver(sSingleton, "xpcom-shutdown", false);
  }
  RefPtr<MacWebAppService> service = sSingleton;
  return service.forget();
}

nsCString MacWebAppService::TakePendingAppId() {
  MOZ_ASSERT(NS_IsMainThread());
  nsCString result = std::move(sPendingAppId);
  sPendingAppId.Truncate();
  return result;
}

NS_IMETHODIMP MacWebAppService::GetProtocolVersion(uint32_t* aVersion) {
  *aVersion = floorp::shim::kProtocolMajor;
  return NS_OK;
}

NS_IMETHODIMP MacWebAppService::GetCapabilitiesJSON(nsACString& aJSON) {
  aJSON.AssignLiteral(
      "{\"protocolVersion\":1,\"authenticatedTransport\":true,"
      "\"sharedBrowserProfile\":true,\"nativeWindowOwnership\":true,"
      "\"transactionalInstall\":true,\"productionReady\":false}");
  return NS_OK;
}

NS_IMETHODIMP MacWebAppService::GetHostIdentityJSON(nsACString& aJSON) {
  if (!NS_IsMainThread() || !XRE_IsParentProcess()) return NS_ERROR_NOT_AVAILABLE;
  SecCodeRef self = nullptr;
  SecRequirementRef requirement = nullptr;
  CFStringRef requirementString = nullptr;
  CFDictionaryRef information = nullptr;
  bool valid = SecCodeCopySelf(kSecCSDefaultFlags, &self) == errSecSuccess;
  if (valid) {
    valid = SecCodeCheckValidity(self, kSecCSStrictValidate, nullptr) == errSecSuccess &&
            SecCodeCopySigningInformation(self, kSecCSSigningInformation,
                                         &information) == errSecSuccess &&
            SecCodeCopyDesignatedRequirement(self, kSecCSDefaultFlags,
                                            &requirement) == errSecSuccess &&
            SecRequirementCopyString(requirement, kSecCSDefaultFlags,
                                     &requirementString) == errSecSuccess;
  }
  if (self) CFRelease(self);
  if (requirement) CFRelease(requirement);
  NSDictionary* info = CFBridgingRelease(information);
  NSString* text = CFBridgingRelease(requirementString);
  NSString* identifier = info[(__bridge NSString*)kSecCodeInfoIdentifier];
  if (!valid || ![identifier isKindOfClass:NSString.class] || !identifier.length ||
      !text.length || !NSBundle.mainBundle.bundlePath.length) return NS_ERROR_NOT_AVAILABLE;
  NSData* result = floorp::shim::EncodePayload(@{
    @"identifier": identifier,
    @"designatedRequirement": text,
    @"bundlePath": NSBundle.mainBundle.bundlePath
  });
  if (!result) return NS_ERROR_FAILURE;
  aJSON.Assign(static_cast<const char*>(result.bytes), result.length);
  return NS_OK;
}

NS_IMETHODIMP MacWebAppService::FingerprintBundle(const nsAString& aBundlePath,
                                                 nsACString& aFingerprint) {
  if (!NS_IsMainThread() || !mImpl->profileId) return NS_ERROR_NOT_INITIALIZED;
  NSString* value = floorp::shim::FingerprintOwnedBundle(ToNSString(aBundlePath));
  if (!value) return NS_ERROR_FILE_ACCESS_DENIED;
  aFingerprint.Assign(value.UTF8String);
  return NS_OK;
}

NS_IMETHODIMP MacWebAppService::VerifyAppBundle(const nsACString& aAppId,
                                               const nsAString& aBundlePath,
                                               nsACString& aJSON) {
  if (!NS_IsMainThread() || !mImpl->profileId) return NS_ERROR_NOT_INITIALIZED;
  NSString* bundlePath = ToNSString(aBundlePath);
  NSDictionary* signing = ValidatedSigningInfo(ToNSString(aAppId), mImpl->profileId,
                                               bundlePath);
  NSString* fingerprint = floorp::shim::FingerprintOwnedBundle(bundlePath);
  if (!signing || !fingerprint) return NS_ERROR_DOM_SECURITY_ERR;
  NSData* cdHash = signing[(__bridge NSString*)kSecCodeInfoUnique];
  NSMutableString* hash = [NSMutableString stringWithCapacity:cdHash.length * 2];
  const unsigned char* bytes = static_cast<const unsigned char*>(cdHash.bytes);
  for (NSUInteger i = 0; i < cdHash.length; ++i) [hash appendFormat:@"%02x", bytes[i]];
  NSData* json = floorp::shim::EncodePayload(@{
    @"bundleId": signing[(__bridge NSString*)kSecCodeInfoIdentifier],
    @"cdHash": hash, @"fingerprint": fingerprint
  });
  if (!json) return NS_ERROR_FAILURE;
  aJSON.Assign(static_cast<const char*>(json.bytes), json.length);
  return NS_OK;
}

NS_IMETHODIMP MacWebAppService::MoveStagedBundle(
    const nsAString& aStagedPath, const nsAString& aBackupPath,
    const nsACString& aExpectedFingerprint) {
  if (!NS_IsMainThread() || !mImpl->profileId) return NS_ERROR_NOT_INITIALIZED;
  return floorp::shim::MoveStagedBundle(ToNSString(aStagedPath), ToNSString(aBackupPath),
                                      ToNSString(aExpectedFingerprint))
             ? NS_OK : NS_ERROR_FILE_ACCESS_DENIED;
}

NS_IMETHODIMP MacWebAppService::RetireAppBundle(
    const nsAString& aLivePath, const nsAString& aStagedPath,
    const nsACString& aExpectedFingerprint) {
  if (!NS_IsMainThread()) return NS_ERROR_NOT_AVAILABLE;
  return floorp::shim::RetireAppBundle(ToNSString(aLivePath), ToNSString(aStagedPath),
      ToNSString(aExpectedFingerprint)) ? NS_OK : NS_ERROR_FILE_ACCESS_DENIED;
}

NS_IMETHODIMP MacWebAppService::ExchangeAppBundles(
    const nsAString& aLivePath, const nsAString& aBackupPath,
    const nsACString& aExpectedLiveFingerprint,
    const nsACString& aExpectedBackupFingerprint) {
  if (!NS_IsMainThread() || !mImpl->profileId) return NS_ERROR_NOT_INITIALIZED;
  if (!floorp::shim::ExchangeAppBundles(ToNSString(aLivePath), ToNSString(aBackupPath),
      ToNSString(aExpectedLiveFingerprint), ToNSString(aExpectedBackupFingerprint))) {
    return NS_ERROR_FILE_ACCESS_DENIED;
  }
  NSURL* liveURL = [NSURL fileURLWithPath:ToNSString(aLivePath) isDirectory:YES];
  return LSRegisterURL((__bridge CFURLRef)liveURL, true) == noErr
             ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP MacWebAppService::RemoveStagedBundle(
    const nsAString& aBundlePath, const nsACString& aExpectedFingerprint) {
  if (!NS_IsMainThread() || !mImpl->profileId) return NS_ERROR_NOT_INITIALIZED;
  return floorp::shim::RemoveStagedBundle(ToNSString(aBundlePath),
                                        ToNSString(aExpectedFingerprint))
             ? NS_OK : NS_ERROR_FILE_ACCESS_DENIED;
}

NS_IMETHODIMP MacWebAppService::Configure(const nsACString& aProfileId,
                                        nsACString& aServiceName) {
  if (!NS_IsMainThread() || !XRE_IsParentProcess()) return NS_ERROR_NOT_AVAILABLE;
  NSString* profileId = ToNSString(aProfileId);
  if (!floorp::shim::ValidIdentity(profileId)) return NS_ERROR_INVALID_ARG;
  if (mImpl->profileId) {
    if (![mImpl->profileId isEqual:profileId]) return NS_ERROR_ALREADY_INITIALIZED;
    aServiceName.Assign(mImpl->serviceName.UTF8String);
    return NS_OK;
  }
  for (const auto& [key, app] : mImpl->apps) {
    if (app->launching || app->running) return NS_ERROR_IN_PROGRESS;
  }
  mImpl->apps.clear();
  NSString* nonce = floorp::shim::NewNonce();
  if (!nonce) return NS_ERROR_FAILURE;
  NSString* serviceName = [NSString stringWithFormat:@"one.ablaze.floorp.webapp.%d.%@",
                                                     getpid(), nonce];
  Port port = Port::Receive();
  if (!port || mach_port_insert_right(mach_task_self(), port.get(), port.get(),
                                     MACH_MSG_TYPE_MAKE_SEND) != KERN_SUCCESS) {
    return NS_ERROR_FAILURE;
  }
  const kern_return_t result = bootstrap_register(
      bootstrap_port, const_cast<char*>(serviceName.UTF8String), port.get());
  mach_port_deallocate(mach_task_self(), port.get());
  if (result != KERN_SUCCESS) return NS_ERROR_FAILURE;
  mImpl->profileId = profileId;
  mImpl->serviceName = serviceName;
  mImpl->receivePort = std::move(port);
  mImpl->receiveSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_MACH_RECV,
      mImpl->receivePort.get(), 0, dispatch_get_main_queue());
  if (!mImpl->receiveSource) {
    mImpl->Stop();
    return NS_ERROR_FAILURE;
  }
  RefPtr<MacWebAppService> self = this;
  dispatch_source_set_event_handler(mImpl->receiveSource, ^{
    self->mImpl->Receive();
  });
  dispatch_resume(mImpl->receiveSource);
  aServiceName.Assign(serviceName.UTF8String);
  return NS_OK;
}

NS_IMETHODIMP MacWebAppService::RegisterApp(const nsACString& aAppId,
                                          const nsAString& aBundlePath) {
  if (!NS_IsMainThread() || !mImpl->profileId) return NS_ERROR_NOT_INITIALIZED;
  NSString* appId = ToNSString(aAppId);
  NSString* bundlePath = ToNSString(aBundlePath);
  if (Impl::App* existing = mImpl->Find(appId)) {
    if (existing->connected || existing->launching) return NS_ERROR_IN_PROGRESS;
  } else if (mImpl->apps.size() >= kMaxApps) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  NSURL* bundleURL = [NSURL fileURLWithPath:bundlePath isDirectory:YES];
  NSDictionary* signing = ValidatedSigningInfo(appId, mImpl->profileId, bundlePath);
  if (!signing) return NS_ERROR_DOM_SECURITY_ERR;
  NSString* identifier = signing[(__bridge NSString*)kSecCodeInfoIdentifier];
  NSData* cdHash = signing[(__bridge NSString*)kSecCodeInfoUnique];
  auto app = std::make_unique<Impl::App>();
  app->id = appId;
  app->bundleURL = bundleURL;
  app->signingIdentifier = identifier;
  app->cdHash = cdHash;
  mImpl->apps[AppKey(appId)] = std::move(app);
  return NS_OK;
}

NS_IMETHODIMP MacWebAppService::LaunchApp(const nsACString& aAppId) {
  if (!NS_IsMainThread() || !mImpl->profileId) return NS_ERROR_NOT_INITIALIZED;
  NSString* appId = ToNSString(aAppId);
  Impl::App* app = mImpl->Find(appId);
  if (!app) return NS_ERROR_NOT_AVAILABLE;
  if (app->connected) {
    return [app->running activateWithOptions:NSApplicationActivateAllWindows]
               ? NS_OK : NS_ERROR_FAILURE;
  }
  if (app->launching) return NS_ERROR_IN_PROGRESS;
  NSString* token = floorp::shim::NewNonce();
  if (!token) return NS_ERROR_FAILURE;
  app->launchToken = token;
  app->launching = true;
  app->cancelling = false;
  NSWorkspaceOpenConfiguration* configuration = [NSWorkspaceOpenConfiguration configuration];
  configuration.createsNewApplicationInstance = YES;
  configuration.activates = YES;
  configuration.arguments = @[
    @"--host-service", mImpl->serviceName,
    @"--host-pid", [NSString stringWithFormat:@"%d", getpid()],
    @"--launch-token", token
  ];
  RefPtr<MacWebAppService> self = this;
  [[NSWorkspace sharedWorkspace] openApplicationAtURL:app->bundleURL
      configuration:configuration completionHandler:^(NSRunningApplication* running, NSError* error) {
    dispatch_async(dispatch_get_main_queue(), ^{
      Impl::App* current = self->mImpl->Find(appId);
      if (!current || ![current->launchToken isEqual:token]) {
        // This completion belongs to the exact new instance started above,
        // even if stop() removed its registration before LaunchServices replied.
        if (running && running.processIdentifier > 0) [running forceTerminate];
        return;
      }
      if (!running || running.processIdentifier <= 0) {
        self->mImpl->Disconnect(*current, @"launch-failed");
        return;
      }
      current->running = running;
      current->pid = running.processIdentifier;
      current->processSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_PROC,
          current->pid, DISPATCH_PROC_EXIT, dispatch_get_main_queue());
      if (!current->processSource) {
        self->mImpl->Disconnect(*current, @"process-monitor-failed");
        return;
      }
      dispatch_source_set_event_handler(current->processSource, ^{
        Impl::App* exited = self->mImpl->Find(appId);
        if (exited && [exited->launchToken isEqual:token]) {
          self->mImpl->Disconnect(*exited, @"process-exited");
        }
      });
      dispatch_resume(current->processSource);
      if (error) {
        self->mImpl->Disconnect(*current, @"launch-failed");
        return;
      }
      if (current->cancelling) {
        [running forceTerminate];
        return;
      }
      if (current->deferredHello) {
        Message hello = std::move(*current->deferredHello);
        current->deferredHello.reset();
        self->mImpl->AcceptHello(*current, std::move(hello));
      }
    });
  }];
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 15 * NSEC_PER_SEC),
                 dispatch_get_main_queue(), ^{
    Impl::App* pending = self->mImpl->Find(appId);
    if (pending && pending->launching && !pending->cancelling &&
        [pending->launchToken isEqual:token]) {
      pending->cancelling = true;
      pending->deferredHello.reset();
      if (pending->running) [pending->running forceTerminate];
    }
  });
  return NS_OK;
}

NS_IMETHODIMP MacWebAppService::CancelLaunch(const nsACString& aAppId) {
  if (!NS_IsMainThread()) return NS_ERROR_NOT_AVAILABLE;
  Impl::App* app = mImpl->Find(ToNSString(aAppId));
  if (!app) return NS_ERROR_NOT_AVAILABLE;
  if (app->connected) {
    return mImpl->Send(*app, MessageType::Terminate, @{}) ? NS_OK : NS_ERROR_FAILURE;
  }
  if (!app->launching) {
    mImpl->Notify(app->id, @"disconnected", @{@"reason": @"not-running"});
    return NS_OK;
  }
  app->cancelling = true;
  app->deferredHello.reset();
  if (app->running && app->pid > 0) [app->running forceTerminate];
  return NS_OK;
}

NS_IMETHODIMP MacWebAppService::AdoptAppBundlePath(const nsACString& aAppId,
                                                 const nsAString& aBundlePath) {
  if (!NS_IsMainThread() || !mImpl->profileId) return NS_ERROR_NOT_INITIALIZED;
  NSString* appId = ToNSString(aAppId);
  NSString* bundlePath = ToNSString(aBundlePath);
  Impl::App* app = mImpl->Find(appId);
  if (!app || !app->connected) return NS_ERROR_NOT_AVAILABLE;
  NSDictionary* signing = ValidatedSigningInfo(appId, mImpl->profileId, bundlePath);
  if (![signing[(__bridge NSString*)kSecCodeInfoIdentifier] isEqual:app->signingIdentifier] ||
      ![signing[(__bridge NSString*)kSecCodeInfoUnique] isEqual:app->cdHash]) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }
  NSURL* bundleURL = [NSURL fileURLWithPath:bundlePath isDirectory:YES];
  if (LSRegisterURL((__bridge CFURLRef)bundleURL, true) != noErr) {
    return NS_ERROR_FAILURE;
  }
  app->bundleURL = bundleURL;
  return NS_OK;
}

NS_IMETHODIMP MacWebAppService::GetAppIdForWindow(
    mozIDOMWindowProxy* aWindow, nsACString& aAppId) {
  aAppId.Truncate();
  if (!NS_IsMainThread()) return NS_ERROR_NOT_AVAILABLE;
  if (!aWindow) return NS_ERROR_INVALID_ARG;
  nsCOMPtr<nsIWidget> widget = nsGlobalWindowOuter::Cast(aWindow)->GetMainWidget();
  if (widget && widget->IsMacWebAppWidget()) {
    aAppId = static_cast<MacWebAppWidget*>(widget.get())->GetMacWebAppId();
  }
  return NS_OK;
}

NS_IMETHODIMP MacWebAppService::WithWindowContext(
    const nsACString& aAppId, nsIMacWebAppWindowFactory* aFactory,
    nsISupports** aResult) {
  if (!aResult || !aFactory) return NS_ERROR_INVALID_ARG;
  *aResult = nullptr;
  if (!NS_IsMainThread() || sInWindowFactory) return NS_ERROR_NOT_AVAILABLE;
  Impl::App* app = mImpl->Find(ToNSString(aAppId));
  if (!app || !app->connected) return NS_ERROR_NOT_AVAILABLE;
  AutoRestore<bool> restoreFactory(sInWindowFactory);
  AutoRestore<nsCString> restoreContext(sPendingAppId);
  sInWindowFactory = true;
  sPendingAppId = aAppId;
  return aFactory->CreateWindow(aResult);
}

bool MacWebAppService::SendControl(const nsACString& aAppId, uint32_t aType,
                                   const nsACString& aPayload,
                                   mach_port_t aSurface) {
  if (!IsHostControl(aType) || aPayload.Length() > floorp::shim::kMaxPayloadBytes ||
      (MACH_PORT_VALID(aSurface) && aType != static_cast<uint32_t>(MessageType::SetLayer))) {
    return false;
  }
  if (!NS_IsMainThread()) {
    if (MACH_PORT_VALID(aSurface) &&
        mach_port_mod_refs(mach_task_self(), aSurface, MACH_PORT_RIGHT_SEND, 1) != KERN_SUCCESS) {
      return false;
    }
    RefPtr<MacWebAppService> self = this;
    auto surface = std::make_shared<Port>(aSurface);
    return NS_SUCCEEDED(NS_DispatchToMainThread(NS_NewRunnableFunction(
        "MacWebAppService::SendControl",
        [self, appId = nsCString(aAppId), type = aType,
         payload = nsCString(aPayload), surface] {
          self->SendControl(appId, type, payload, surface->get());
        })));
  }
  @autoreleasepool {
    Impl::App* app = mImpl->Find(ToNSString(aAppId));
    if (!app || !app->connected) return false;
    NSDictionary* payload = floorp::shim::DecodePayload(
        [NSData dataWithBytes:aPayload.BeginReading() length:aPayload.Length()]);
    if (!payload) return false;
    const auto type = static_cast<MessageType>(aType);
    id kind = payload[@"kind"] ?: @"window";
    const bool createsForegroundWindow = type == MessageType::CreateWindow &&
        ([kind isEqual:@"window"] || [kind isEqual:@"dialog"]);
    if (type == MessageType::ActivateWindow || createsForegroundWindow) {
      if (app->cancelling || !app->running || app->running.terminated ||
          app->pid <= 0 || app->running.processIdentifier != app->pid) {
        return false;
      }
      if (@available(macOS 14.0, *)) {
        // Yield to the authenticated launch instance before it calls -activate.
        [NSApp yieldActivationToApplication:app->running];
      }
    }
    return mImpl->Send(*app, type, payload, aSurface);
  }
}

void MacWebAppService::RetireSurfaces(const nsACString& aAppId,
    uint32_t aWindowId,
    std::map<uint32_t, RefPtr<layers::NativeLayerSurface>>&& aSurfaces) {
  MOZ_ASSERT(NS_IsMainThread());
  Impl::App* app = mImpl->Find(ToNSString(aAppId));
  if (!app || !app->running || app->running.terminated) return;
  auto window = app->windows.find(aWindowId);
  if (window != app->windows.end() && window->second.closed) {
    app->windows.erase(window);
    return;
  }
  auto& retired = app->windows[aWindowId];
  MOZ_RELEASE_ASSERT(!retired.retired);
  retired.retired = true;
  retired.surfaces = std::move(aSurfaces);
  NSString* appId = app->id;
  NSString* token = app->launchToken;
  RefPtr<MacWebAppService> self = this;
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC),
                 dispatch_get_main_queue(), ^{
    Impl::App* current = self->mImpl->Find(appId);
    if (!current || ![current->launchToken isEqual:token] ||
        !current->windows.count(aWindowId)) return;
    // A missing ACK must never permit the producer to overwrite a live peer's
    // surface. Terminate that exact launch, retaining its leases until exit.
    self->mImpl->Disconnect(*current, @"window-close-timeout");
  });
}

NS_IMETHODIMP MacWebAppService::SendControlFromScript(
    const nsACString& aAppId, uint32_t aType, const nsACString& aPayload) {
  if (!NS_IsMainThread() || !IsScriptControl(aType)) return NS_ERROR_INVALID_ARG;
  return SendControl(aAppId, aType, aPayload) ? NS_OK : NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP MacWebAppService::Stop() {
  if (!NS_IsMainThread()) return NS_ERROR_NOT_AVAILABLE;
  mImpl->Stop();
  return NS_OK;
}

NS_IMETHODIMP MacWebAppService::Observe(nsISupports*, const char* aTopic,
                                     const char16_t*) {
  if (!std::strcmp(aTopic, "xpcom-shutdown")) {
    Stop();
    nsCOMPtr<nsIObserverService> observers = services::GetObserverService();
    if (observers) observers->RemoveObserver(this, "xpcom-shutdown");
    StaticMutexAutoLock lock(sSingletonMutex);
    sShuttingDown = true;
    sSingleton = nullptr;
  }
  return NS_OK;
}

}  // namespace mozilla::widget
