// SPDX-License-Identifier: MPL-2.0

#ifndef FLOORP_SHIM_NATIVE_TESTS
#error "Test peer policy must never be compiled into the production executable"
#endif

#include "Session.h"
#include "ShimView.h"
#include "ShimApplication.h"
#include "ColdLaunch.h"

#import <IOSurface/IOSurface.h>
#import <QuartzCore/QuartzCore.h>
#import <Security/Security.h>
#include <bsm/libbsm.h>
#include <servers/bootstrap.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <thread>

extern char** environ;
using namespace floorp::shim;

@interface FPAShimApplicationDelegate : NSObject
- (instancetype)initWithIdentity:(AppIdentity)identity host:(Port)host
                         hostPID:(pid_t)hostPID verifier:(PeerVerifier)verifier;
- (BOOL)handle:(Message&)message;
@end

namespace {

pid_t gChild = 0;
int gAssertions = 0;

void Expect(bool condition, const char* message) {
  ++gAssertions;
  if (condition) return;
  fprintf(stderr, "FAIL: %s\n", message);
  if (gChild > 0) {
    kill(gChild, SIGKILL);
    waitpid(gChild, nullptr, 0);
  }
  std::exit(1);
}

PeerVerifier TestPeer(pid_t expected) {
  uid_t uid = geteuid();
  return [expected, uid](const audit_token_t& token) {
    return audit_token_to_pid(token) == expected && audit_token_to_euid(token) == uid;
  };
}

IOSurfaceRef MakeSurface(uint32_t color) {
  NSDictionary* attributes = @{
      (__bridge NSString*)kIOSurfaceWidth: @8,
      (__bridge NSString*)kIOSurfaceHeight: @8,
      (__bridge NSString*)kIOSurfaceBytesPerElement: @4,
      (__bridge NSString*)kIOSurfaceBytesPerRow: @32,
      (__bridge NSString*)kIOSurfacePixelFormat: @(static_cast<uint32_t>('BGRA'))};
  IOSurfaceRef surface = IOSurfaceCreate((__bridge CFDictionaryRef)attributes);
  Expect(surface != nullptr, "IOSurface allocation");
  Expect(IOSurfaceLock(surface, 0, nullptr) == kIOReturnSuccess, "IOSurface write lock");
  auto* pixels = static_cast<uint32_t*>(IOSurfaceGetBaseAddress(surface));
  for (size_t index = 0; index < 64; ++index) pixels[index] = color;
  IOSurfaceUnlock(surface, 0, nullptr);
  return surface;
}

NSDictionary* LayerPayload(uint32_t frame, uint32_t layer, uint32_t surface) {
  return @{@"windowId": @1, @"frameId": @(frame), @"layerId": @(layer), @"surfaceId": @(surface),
      @"x": @8, @"y": @12, @"width": @128, @"height": @128,
      @"scale": @2, @"opacity": @0.75, @"zOrder": @(layer),
      @"clip": @{@"x": @4, @"y": @4, @"width": @120, @"height": @120}};
}

int PeerMain(NSString* service, pid_t parent) {
  Port host = Port::Lookup(service);
  Port incoming = Port::Receive();
  if (!host || !incoming) return 10;
  NSString* nonce = NewNonce();
  NSDictionary* hello = @{@"appId": @"{test-app}", @"profileId": @"test-profile", @"nonce": nonce};
  if (!SendMessage(host.get(), MessageType::Hello, 1, hello, incoming.get(), MACH_MSG_TYPE_MAKE_SEND)) return 11;
  SessionGate gate(@"{test-app}", @"test-profile", nonce, TestPeer(parent));
  ReceiveResult accepted = ReceiveMessage(incoming.get(), 5000);
  if (!accepted.message || !gate.Accept(*accepted.message)) return 12;
  Message wrongPeer;
  wrongPeer.header.sequence = accepted.message->header.sequence + 1;
  wrongPeer.header.type = static_cast<uint32_t>(MessageType::CreateWindow);
  wrongPeer.payload = @{};
  if (gate.Accept(wrongPeer)) return 13;
  if (gate.Accept(*accepted.message)) return 14;
  ReceiveResult frame = ReceiveMessage(incoming.get(), 5000);
  if (!frame.message || !gate.Accept(*frame.message) || !frame.message->attachment) return 15;
  IOSurfaceRef surface = IOSurfaceLookupFromMachPort(frame.message->attachment.get());
  if (!surface) return 16;
  bool matches = IOSurfaceGetWidth(surface) == 8 && IOSurfaceGetHeight(surface) == 8;
  IOSurfaceLock(surface, kIOSurfaceLockReadOnly, nullptr);
  matches = matches && static_cast<uint32_t*>(IOSurfaceGetBaseAddress(surface))[0] == 0xFF336699;
  IOSurfaceUnlock(surface, kIOSurfaceLockReadOnly, nullptr);
  CFRelease(surface);
  if (!matches) return 17;
  if (!SendMessage(host.get(), MessageType::FramePresented, 2, @{@"windowId": @1, @"frameId": @1})) return 18;
  return 0;
}

void TestSeparateProcess(const char* executable) {
  Port incoming = Port::Receive();
  Expect(static_cast<bool>(incoming), "host receive port");
  Expect(mach_port_insert_right(mach_task_self(), incoming.get(), incoming.get(), MACH_MSG_TYPE_MAKE_SEND) == KERN_SUCCESS,
         "host send right");
  Port registrationRight(incoming.get());
  NSString* service = [NSString stringWithFormat:@"org.floorp.appshim.test.%d.%@", getpid(), NewNonce()];
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  Expect(bootstrap_register(bootstrap_port, const_cast<char*>(service.UTF8String), incoming.get()) == KERN_SUCCESS, "register ephemeral Mach service");
#pragma clang diagnostic pop
  std::string pid = std::to_string(getpid());
  char* arguments[] = {const_cast<char*>(executable), const_cast<char*>("--peer"),
      const_cast<char*>(service.UTF8String), pid.data(), nullptr};
  Expect(posix_spawn(&gChild, executable, nullptr, nullptr, arguments, environ) == 0, "spawn separate native peer");
  ReceiveResult hello = ReceiveMessage(incoming.get(), 5000);
  Expect(hello.message.has_value(), "receive separate-process Hello");
  Expect(TestPeer(gChild)(hello.message->auditToken), "kernel audit token pins peer PID and user");
  Expect(!TestPeer(getpid())(hello.message->auditToken), "wrong peer PID rejected");
  Expect(hello.message->header.type == static_cast<uint32_t>(MessageType::Hello) && hello.message->attachment,
         "Hello carries receive-port capability");
  NSMutableDictionary* acceptance = [hello.message->payload mutableCopy];
  acceptance[@"accepted"] = @YES;
  Expect(SendMessage(hello.message->attachment.get(), MessageType::HelloAccepted, 1, acceptance), "send acceptance");
  IOSurfaceRef surface = MakeSurface(0xFF336699);
  Port surfacePort(IOSurfaceCreateMachPort(surface));
  Expect(static_cast<bool>(surfacePort), "IOSurface Mach send right");
  Expect(SendMessage(hello.message->attachment.get(), MessageType::SetLayer, 2, LayerPayload(1, 1, 1), surfacePort.get()),
         "transfer compositor surface right across process boundary");
  ReceiveResult reply = ReceiveMessage(incoming.get(), 5000);
  Expect(reply.message && TestPeer(gChild)(reply.message->auditToken) &&
      reply.message->header.type == static_cast<uint32_t>(MessageType::FramePresented), "remote peer read shared surface");
  int status = 0;
  Expect(waitpid(gChild, &status, 0) == gChild, "reap native peer");
  gChild = 0;
  Expect(WIFEXITED(status) && WEXITSTATUS(status) == 0, "separate-process protocol checks");
  CFRelease(surface);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  kern_return_t unregisterResult = bootstrap_register(bootstrap_port, const_cast<char*>(service.UTF8String), MACH_PORT_NULL);
  (void)unregisterResult;
#pragma clang diagnostic pop
}

void TestSignedPeer() {
  SecCodeRef self = nullptr;
  CFDictionaryRef signing = nullptr;
  SecRequirementRef requirement = nullptr;
  CFStringRef requirementText = nullptr;
  Expect(SecCodeCopySelf(kSecCSDefaultFlags, &self) == errSecSuccess, "test process code identity");
  Expect(SecCodeCopySigningInformation(self, kSecCSSigningInformation, &signing) == errSecSuccess,
         "test process signature metadata");
  Expect(SecCodeCopyDesignatedRequirement(self, kSecCSDefaultFlags, &requirement) == errSecSuccess,
         "test process designated requirement");
  Expect(SecRequirementCopyString(requirement, kSecCSDefaultFlags, &requirementText) == errSecSuccess,
         "test process requirement string");
  NSDictionary* info = CFBridgingRelease(signing);
  NSString* text = CFBridgingRelease(requirementText);
  NSString* identifier = info[(__bridge NSString*)kSecCodeInfoIdentifier];
  CFRelease(requirement);
  CFRelease(self);
  Port receiving = Port::Receive();
  Expect(mach_port_insert_right(mach_task_self(), receiving.get(), receiving.get(), MACH_MSG_TYPE_MAKE_SEND) == KERN_SUCCESS,
         "signature test send right");
  Port sending(receiving.get());
  Expect(SendMessage(sending.get(), MessageType::Hello, 1, @{}), "signature test kernel audit message");
  ReceiveResult result = ReceiveMessage(receiving.get(), 5000);
  Expect(bool(result.message), "signature test receives kernel audit token");
  PeerVerifier verifier = SignedHostVerifier(getpid(), identifier, text);
  Expect(bool(verifier) && verifier(result.message->auditToken), "production verifier authenticates signed designated host");
  Expect(verifier(result.message->auditToken), "production verifier accepts cached exact kernel identity");
  audit_token_t altered = result.message->auditToken;
  altered.val[7] ^= 1;
  Expect(!verifier(altered), "cached verifier rejects changed process generation");
  PeerVerifier wrongIdentifier = SignedHostVerifier(getpid(), @"org.floorp.wrong-host", text);
  Expect(bool(wrongIdentifier) && !wrongIdentifier(result.message->auditToken), "production verifier rejects wrong signing identifier");
}

void TestColdBootstrap(const char* executable) {
  NSString* root = [NSTemporaryDirectory() stringByAppendingPathComponent:
      [@"floorp-cold-launch-" stringByAppendingString:NewNonce()]];
  NSString* bundle = [root stringByAppendingPathComponent:@"Host $() ' ;.app"];
  NSString* macos = [bundle stringByAppendingPathComponent:@"Contents/MacOS"];
  NSString* profile = [root stringByAppendingPathComponent:@"Profile $() ' ;"];
  NSFileManager* manager = NSFileManager.defaultManager;
  Expect([manager createDirectoryAtPath:macos withIntermediateDirectories:YES attributes:nil error:nil],
         "cold host bundle directory");
  Expect([manager createDirectoryAtPath:profile withIntermediateDirectories:YES attributes:nil error:nil],
         "cold existing profile directory");
  Expect([manager copyItemAtPath:[NSString stringWithUTF8String:executable]
      toPath:[macos stringByAppendingPathComponent:@"host"] error:nil], "cold host executable copy");
  NSDictionary* metadata = @{@"CFBundleIdentifier": @"org.floorp.appshim.cold-host-test",
      @"CFBundleExecutable": @"host", @"CFBundlePackageType": @"APPL", @"CFBundleVersion": @"1"};
  Expect([metadata writeToFile:[bundle stringByAppendingPathComponent:@"Contents/Info.plist"] atomically:YES],
         "cold host bundle metadata");
  char* signingArguments[] = {const_cast<char*>("/usr/bin/codesign"), const_cast<char*>("--force"),
      const_cast<char*>("--sign"), const_cast<char*>("-"), const_cast<char*>(bundle.fileSystemRepresentation), nullptr};
  pid_t signer = 0;
  Expect(!posix_spawn(&signer, signingArguments[0], nullptr, nullptr, signingArguments, environ), "cold host signer spawn");
  int signingStatus = 0;
  Expect(waitpid(signer, &signingStatus, 0) == signer && WIFEXITED(signingStatus) && !WEXITSTATUS(signingStatus),
         "cold host signed without shell");
  SecStaticCodeRef code = nullptr;
  SecRequirementRef requirement = nullptr;
  CFStringRef requirementText = nullptr;
  Expect(SecStaticCodeCreateWithPath((__bridge CFURLRef)[NSURL fileURLWithPath:bundle], kSecCSDefaultFlags, &code) == errSecSuccess,
         "cold host static identity");
  Expect(SecCodeCopyDesignatedRequirement(code, kSecCSDefaultFlags, &requirement) == errSecSuccess &&
      SecRequirementCopyString(requirement, kSecCSDefaultFlags, &requirementText) == errSecSuccess,
      "cold host sealed code requirement");
  CFRelease(code);
  CFRelease(requirement);
  NSString* text = CFBridgingRelease(requirementText);
  NSDictionary* sealed = @{@"FloorpAppShimHostPath": bundle, @"FloorpAppShimProfilePath": profile,
      @"FloorpAppShimHostBundleIdentifier": metadata[@"CFBundleIdentifier"],
      @"FloorpAppShimHostCodeRequirement": text, @"FloorpAppShimAppId": @"{cold-app-id}"};
  NSMutableDictionary* invalid = [sealed mutableCopy];
  invalid[@"FloorpAppShimHostBundleIdentifier"] = @"org.floorp.wrong-host";
  Expect(LaunchVerifiedHost(invalid) == 1, "cold bootstrap rejects wrong signed identity");
  NSArray<NSString*>* changedNames = @[@"MOZ_NO_REMOTE", @"MOZ_NEW_INSTANCE",
      @"XRE_PROFILE_PATH", @"XRE_PROFILE_LOCAL_PATH", @"SELECTABLE_PROFILE_RESET_PATH",
      @"SELECTABLE_PROFILE_RESET_STORE_ID", @"XRE_PROFILE_PATH_EXTRA"];
  NSMutableDictionary<NSString*, NSString*>* originalEnvironment = [NSMutableDictionary dictionary];
  for (NSString* name in changedNames) {
    const char* previous = getenv(name.UTF8String);
    if (previous) originalEnvironment[name] = [NSString stringWithUTF8String:previous];
    setenv(name.UTF8String, "inherited-override", 1);
  }
  int result = LaunchVerifiedHost(sealed);
  for (NSString* name in changedNames) {
    NSString* previous = originalEnvironment[name];
    if (previous) setenv(name.UTF8String, previous.UTF8String, 1);
    else unsetenv(name.UTF8String);
  }
  Expect(result == 0, "cold bootstrap launches verified executable");
  NSString* output = [profile stringByAppendingPathComponent:@"observed.plist"];
  NSDate* deadline = [NSDate dateWithTimeIntervalSinceNow:5];
  while (![manager fileExistsAtPath:output] && deadline.timeIntervalSinceNow > 0) usleep(10000);
  NSDictionary* observed = [NSDictionary dictionaryWithContentsOfFile:output];
  Expect([observed[@"profile"] isEqual:profile] && [observed[@"appId"] isEqual:@"{cold-app-id}"],
         "cold profile path and braced app ID passed literally without shell expansion");
  Expect(![observed[@"noRemote"] boolValue] && ![observed[@"newInstance"] boolValue],
         "cold bootstrap clears remoting override environment");
  Expect([observed[@"profileOverrides"] isKindOfClass:NSArray.class] &&
      [observed[@"profileOverrides"] count] == 0,
      "cold bootstrap clears restart and reset profile overrides that precede --profile");
  Expect([observed[@"preservedEnvironment"] isEqual:@"inherited-override"],
      "cold bootstrap filters exact environment names only");
  pid_t child = [observed[@"pid"] intValue];
  int childStatus = 0;
  Expect(child > 0 && waitpid(child, &childStatus, 0) == child && WIFEXITED(childStatus) && !WEXITSTATUS(childStatus),
         "cold bootstrap helper completed");
  Expect([manager removeItemAtPath:root error:nil], "cold bootstrap temporary artifacts removed");
}

void TestProtocol() {
  Expect(ValidIdentity(@"{12345678-1234-5678-1234-567812345678}"), "legacy ssb UUID preserved");
  Expect(!ValidIdentity(@"../profile") && !ValidIdentity(@""), "path and empty identities rejected");
  uint32_t value;
  Expect(!ReadUInt(@{@"value": @YES}, @"value", &value), "boolean is not an identifier");
  Expect(!ReadUInt(@{@"value": @1.5}, @"value", &value), "fractional identifier rejected");
  Expect(ReadUInt(@{@"value": @UINT32_MAX}, @"value", &value) && value == UINT32_MAX, "bounded integer accepted");
  Expect(DecodePayload([@"[]" dataUsingEncoding:NSUTF8StringEncoding]) == nil, "non-object control rejected");
  Expect(!SignedHostVerifier(getpid(), @"org.floorp.invalid", nil), "production verifier requires sealed host requirement");
  SessionGate gate(@"app", @"profile", @"nonce", [](const audit_token_t&) { return true; });
  Message accepted;
  accepted.header.type = static_cast<uint32_t>(MessageType::HelloAccepted);
  accepted.header.sequence = 1;
  accepted.payload = @{@"appId": @"other", @"profileId": @"profile", @"nonce": @"nonce", @"accepted": @YES};
  Expect(!gate.Accept(accepted), "app identity mismatch rejected");
  accepted.payload = @{@"appId": @"app", @"profileId": @"profile", @"nonce": @"wrong", @"accepted": @YES};
  Expect(!gate.Accept(accepted), "nonce mismatch rejected");
  accepted.payload = @{@"appId": @"app", @"profileId": @"profile", @"nonce": @"nonce", @"accepted": @YES};
  Expect(gate.Accept(accepted) && gate.active(), "authenticated session activates");
  Expect(!gate.Accept(accepted), "replay rejected");
}

void Pump() {
  [CATransaction flush];
  NSDate* deadline = [NSDate dateWithTimeIntervalSinceNow:0.15];
  while ([deadline timeIntervalSinceNow] > 0) {
    [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:deadline];
  }
}

void TestNativeView() {
  [NSApplication sharedApplication];
  NSApp.activationPolicy = NSApplicationActivationPolicyAccessory;
  [NSApp finishLaunching];
  NSMutableArray<NSDictionary*>* events = [NSMutableArray array];
  NSWindow* window = [[NSWindow alloc] initWithContentRect:NSMakeRect(100, 100, 360, 240)
      styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskResizable backing:NSBackingStoreBuffered defer:NO];
  window.releasedWhenClosed = NO;
  window.title = @"Floorp App Shim native validation";
  FPAShimView* view = [[FPAShimView alloc] initWithFrame:NSMakeRect(0, 0, 360, 240) windowID:1
      sink:^(MessageType type, NSDictionary* payload) {
        NSMutableDictionary* value = [payload mutableCopy];
        value[@"type"] = @(static_cast<uint32_t>(type));
        [events addObject:value];
      }];
  window.contentView = view;
  [window orderBack:nil];
  Pump();
  Expect(window.windowNumber > 0 && window.contentView == view, "real native window and view");
  CFArrayRef windows = CGWindowListCopyWindowInfo(kCGWindowListOptionIncludingWindow, static_cast<CGWindowID>(window.windowNumber));
  NSArray* metadata = CFBridgingRelease(windows);
  if (metadata.count != 1) NSLog(@"Window metadata: %@, window number: %ld", metadata, (long)window.windowNumber);
  Expect(metadata.count == 1 && [metadata[0][(__bridge NSString*)kCGWindowOwnerPID] intValue] == getpid(),
         "WindowServer attributes native window to shim test process");
  Expect([view beginFrame:@{@"frameId": @1}], "begin atomic composition frame");
  IOSurfaceRef first = MakeSurface(0xFF112233);
  IOSurfaceRef second = MakeSurface(0xFF445566);
  Port firstPort(IOSurfaceCreateMachPort(first));
  Port secondPort(IOSurfaceCreateMachPort(second));
  Expect([view setLayer:LayerPayload(1, 1, 1) surfacePort:firstPort.get()], "accept first compositor layer");
  Expect([view setLayer:LayerPayload(1, 2, 2) surfacePort:secondPort.get()], "accept second compositor layer");
  Expect(view.presentedLayerCount == 0, "pending frame is not prematurely displayed");
  Expect([view commitFrame:@{@"frameId": @1}], "commit multi-layer frame");
  Expect(view.presentedLayerCount == 2 && view.committedFrameID == 1, "multi-layer frame applied");
  Expect(view.layer.sublayers.count == 2 && view.layer.sublayers[0].mask != nil &&
      view.layer.sublayers[0].contentsScale == 2, "native clip and scale applied");
  Expect(![view beginFrame:@{@"frameId": @1}], "stale frame rejected");
  Expect([view beginFrame:@{@"frameId": @2}], "begin next frame");
  Expect(![view setLayer:LayerPayload(2, 3, 2) surfacePort:secondPort.get()], "live surface identity cannot be reused");
  Expect([view removeLayer:@{@"frameId": @2, @"layerId": @1}], "remove compositor layer");
  Expect([view commitFrame:@{@"frameId": @2}], "commit layer removal");
  Pump();
  Expect(view.presentedLayerCount == 1, "removed layer no longer displayed");
  NSMutableDictionary* raw = [@{@"windowId": @1, @"frameId": @3, @"layerId": @3,
      @"positionX": @10, @"positionY": @20, @"sizeWidth": @8, @"sizeHeight": @8,
      @"scale": @2, @"opacity": @1, @"zOrder": @3, @"flipped": @YES, @"sampling": @"nearest",
      @"transform16": @[@2,@0,@0,@0, @0,@3,@0,@0, @0,@0,@1,@0, @4,@6,@0,@1],
      @"displayRect": @{@"x": @1, @"y": @2, @"width": @6, @"height": @5},
      @"clip": @{@"x": @0, @"y": @0, @"width": @300, @"height": @200},
      @"roundedClip": @{@"rect": @{@"x": @0, @"y": @0, @"width": @300, @"height": @200},
          @"radii": @[@{@"width":@4,@"height":@6}, @{@"width":@8,@"height":@3},
                        @{@"width":@0,@"height":@0}, @{@"width":@5,@"height":@7}]},
      @"color": @{@"r": @0.2, @"g": @0.4, @"b": @0.8, @"a": @1}} mutableCopy];
  Expect([view beginFrame:@{@"frameId": @3}], "begin raw Gecko geometry frame");
  Expect([view setLayer:raw surfacePort:MACH_PORT_NULL], "color-only layer requires no surface attachment");
  Expect([view commitFrame:@{@"frameId": @3}], "commit transformed color layer");
  CALayer* root = view.layer.sublayers.lastObject;
  CALayer* rounded = root.sublayers.firstObject;
  CALayer* transformed = rounded.sublayers.firstObject;
  CALayer* crop = transformed.sublayers.firstObject;
  CALayer* color = crop.sublayers.firstObject;
  Expect(root.mask && rounded.mask && transformed.transform.m11 == 2 && transformed.transform.m22 == 3 &&
      transformed.transform.m41 == 12 && transformed.transform.m42 == 33,
      "raw matrix preserves pretranslation and backing-scale conversion");
  Expect(crop.position.x == 0.5 && crop.position.y == 1 && crop.bounds.size.width == 3 &&
      color.transform.m22 == -1 && [color.minificationFilter isEqual:kCAFilterNearest] && color.backgroundColor,
      "displayRect, flipped contents, sampling, and color preserved");
  NSMutableDictionary* perspective = [raw mutableCopy];
  perspective[@"frameId"] = @4;
  perspective[@"layerId"] = @4;
  perspective[@"flipped"] = @NO;
  perspective[@"transform16"] = @[@2,@0,@0.5,@0.01, @0,@3,@0.25,@0.02,
                                    @0.1,@0.2,@1,@0.03, @4,@6,@7,@1];
  Expect([view beginFrame:@{@"frameId": @4}] && [view setLayer:perspective surfacePort:MACH_PORT_NULL] &&
      [view commitFrame:@{@"frameId": @4}], "commit raw perspective and Z transform");
  CATransform3D nativeTransform = view.layer.sublayers.lastObject.sublayers.firstObject.sublayers.firstObject.transform;
  Expect(nativeTransform.m14 == 0.01 && nativeTransform.m24 == 0.02 && nativeTransform.m34 == 0.03 &&
      nativeTransform.m43 == 17 && nativeTransform.m41 == 12 && nativeTransform.m42 == 33 && nativeTransform.m44 == 1.5,
      "perspective and Z units match authoritative NativeLayerCA conversion");
  Expect([view setEditorState:@{@"revision": @1, @"text": @"日本語", @"selectionStart": @3,
      @"selectionLength": @0, @"caretX": @20, @"caretY": @30, @"caretWidth": @1,
      @"caretHeight": @18, @"editable": @YES}], "editor cache accepts Japanese text");
  [view setMarkedText:@"変換" selectedRange:NSMakeRange(0, 2) replacementRange:NSMakeRange(NSNotFound, 0)];
  Expect(view.hasMarkedText && view.markedRange.length == 2, "IME marked text state");
  [view insertText:@"変換" replacementRange:NSMakeRange(NSNotFound, 0)];
  Expect(!view.hasMarkedText, "IME composition commit clears marked text");
  NSDictionary* last = events.lastObject;
  Expect([last[@"kind"] isEqual:@"insertText"] && [last[@"text"] isEqual:@"変換"] &&
      [last[@"windowId"] intValue] == 1 && [last[@"editorRevision"] intValue] == 1,
      "IME protocol preserves text, revision, and window identity");
  NSRange actual;
  NSAttributedString* substring = [view attributedSubstringForProposedRange:NSMakeRange(1, 100) actualRange:&actual];
  Expect([substring.string isEqual:@"本語"] && actual.length == 2, "IME surrounding text cache bounds range");
  NSRect caret = [view firstRectForCharacterRange:NSMakeRange(3, 0) actualRange:&actual];
  Expect(caret.size.height == 18 && caret.size.width == 1, "IME caret screen geometry");
  Expect([view setEditorState:@{@"revision": @2, @"text": @"日本語", @"textOffset": @40000,
      @"selectionStart": @40003, @"selectionLength": @0, @"caretX": @20, @"caretY": @30,
      @"caretWidth": @1, @"caretHeight": @18, @"editable": @YES, @"discardMarkedText": @YES}],
      "bounded surrounding text supports absolute offsets");
  substring = [view attributedSubstringForProposedRange:NSMakeRange(40001, 10) actualRange:&actual];
  Expect([substring.string isEqual:@"本語"] && actual.location == 40001 && view.selectedRange.location == 40003,
      "IME absolute text offsets round trip");
  view.inputEnabled = NO;
  NSUInteger eventCount = events.count;
  [view insertText:@"ignored" replacementRange:NSMakeRange(NSNotFound, 0)];
  Expect(events.count == eventCount, "disabled native content suppresses input");
  view.inputEnabled = YES;
  Expect([view setCursorState:@{@"cursor": @"pointer"}] && view.nativeCursor == NSCursor.pointingHandCursor,
         "native pointer cursor whitelist");
  Expect([view setCursorState:@{@"cursor": @"text"}] && view.nativeCursor == NSCursor.IBeamCursor,
         "native text cursor whitelist");
  Expect(![view setCursorState:@{@"cursor": @"file:///tmp/arbitrary-image"}], "cursor refuses image path");
  Expect(![view setCursorState:@{@"cursor": @"unrecognized"}], "cursor rejects unknown name");
  for (NSString* cursor in @[@"default", @"vertical-text", @"crosshair", @"grab", @"grabbing", @"move",
      @"copy", @"alias", @"context-menu", @"not-allowed", @"ew-resize", @"ns-resize", @"nwse-resize",
      @"nesw-resize", @"zoom-in", @"zoom-out", @"none"]) {
    Expect([view setCursorState:@{@"cursor": cursor}], "supported native cursor mapping");
  }
  Expect([view setCursorState:@{@"cursor": @"default"}], "restore native cursor");
  NSEvent* mouse = [NSEvent mouseEventWithType:NSEventTypeLeftMouseDown location:NSMakePoint(12, 20)
      modifierFlags:0 timestamp:1 windowNumber:window.windowNumber context:nil eventNumber:1 clickCount:1 pressure:0.5];
  [view mouseDown:mouse];
  Expect([events.lastObject[@"kind"] isEqual:@"mouseDown"], "native input routed through protocol hook");
  [view disconnect];
  [window close];
  Pump();
  CFRelease(first);
  CFRelease(second);
}

void TestAsyncSender() {
  Port receiving = Port::Receive();
  Expect(mach_port_insert_right(mach_task_self(), receiving.get(), receiving.get(), MACH_MSG_TYPE_MAKE_SEND) == KERN_SUCCESS,
      "async test obtains send right");
  bool failed = false;
  MessageSender sender(receiving.get(), [&] { failed = true; });
  IOSurfaceRef surface = MakeSurface(0xff21a584);
  Port surfacePort(IOSurfaceCreateMachPort(surface));
  auto start = std::chrono::steady_clock::now();
  constexpr uint32_t fullReplacementMessages = 2 + 2 * kMaxLayers;
  for (uint32_t i = 1; i <= fullReplacementMessages; ++i) {
    Expect(sender.Enqueue(MessageType::WindowChanged, i, @{@"index": @(i)}, i == fullReplacementMessages ? surfacePort.get() : MACH_PORT_NULL),
        "async burst admitted without receiver draining");
  }
  surfacePort = Port();
  CFRelease(surface);
  Expect(std::chrono::steady_clock::now() - start < std::chrono::milliseconds(500),
      "enqueue does not wait for kernel queue capacity");
  // AppKit can take longer than the synchronous transport's 100ms deadline.
  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  for (uint32_t i = 1; i <= fullReplacementMessages; ++i) {
    auto result = ReceiveMessage(receiving.get(), 2000);
    Expect(result.status == ReceiveStatus::Message && result.message->header.sequence == i &&
        [result.message->payload[@"index"] unsignedIntValue] == i,
        "stalled receiver eventually gets complete ordered burst");
    if (i == fullReplacementMessages) {
      IOSurfaceRef imported = IOSurfaceLookupFromMachPort(result.message->attachment.get());
      Expect(imported != nullptr, "pending surface right survives caller release");
      IOSurfaceLock(imported, kIOSurfaceLockReadOnly, nullptr);
      Expect(*static_cast<uint32_t*>(IOSurfaceGetBaseAddress(imported)) == 0xff21a584,
          "retained queued IOSurface preserves pixels");
      IOSurfaceUnlock(imported, kIOSurfaceLockReadOnly, nullptr);
      CFRelease(imported);
    }
  }
  Expect(!sender.Enqueue(MessageType::WindowChanged, fullReplacementMessages, @{}), "async sender rejects sequence replay");
  IOSurfaceRef cancelledSurface = MakeSurface(0xff00abcd);
  Port cancelledPort(IOSurfaceCreateMachPort(cancelledSurface));
  CFRelease(cancelledSurface);
  mach_port_urefs_t referencesBefore = 0, referencesQueued = 0, referencesAfter = 0;
  Expect(mach_port_get_refs(mach_task_self(), cancelledPort.get(), MACH_PORT_RIGHT_SEND, &referencesBefore) == KERN_SUCCESS,
      "read caller surface send-right count");
  for (uint32_t i = fullReplacementMessages + 1; i < fullReplacementMessages + 33; ++i)
    Expect(sender.Enqueue(MessageType::WindowChanged, i, @{}), "fill receiver before cancellation");
  Expect(sender.Enqueue(MessageType::SetLayer, fullReplacementMessages + 33, @{}, cancelledPort.get()),
      "retain surface queued behind stalled receiver");
  Expect(mach_port_get_refs(mach_task_self(), cancelledPort.get(), MACH_PORT_RIGHT_SEND, &referencesQueued) == KERN_SUCCESS &&
      referencesQueued == referencesBefore + 1, "queue independently retains attachment right");
  sender.Stop();
  Expect(mach_port_get_refs(mach_task_self(), cancelledPort.get(), MACH_PORT_RIGHT_SEND, &referencesAfter) == KERN_SUCCESS &&
      referencesAfter == referencesBefore, "Stop releases pending attachment while another send waits");
  MessageSender bounded(receiving.get(), [&] { failed = true; });
  uint32_t admitted = 0;
  for (uint32_t i = 1; i < 1000 && bounded.Enqueue(MessageType::WindowChanged, i, @{}); ++i) ++admitted;
  Expect(admitted > 0 && admitted <= 517, "async pending message count is bounded including in-flight send");
  bounded.Stop();
  Expect(!sender.Enqueue(MessageType::WindowChanged, 1000, @{}), "stopped sender rejects work");
  Pump();
  Expect(!failed, "explicit cancellation suppresses failure callback");
  mach_port_deallocate(mach_task_self(), receiving.get());

  Port deadReceiver = Port::Receive();
  Expect(mach_port_insert_right(mach_task_self(), deadReceiver.get(), deadReceiver.get(), MACH_MSG_TYPE_MAKE_SEND) == KERN_SUCCESS,
      "dead-peer test obtains send right");
  MessageSender deadSender(deadReceiver.get(), [&] { failed = true; });
  mach_port_deallocate(mach_task_self(), deadReceiver.get());
  deadReceiver = Port();
  Expect(deadSender.Enqueue(MessageType::WindowChanged, 1, @{}), "dead-peer delivery is asynchronous");
  for (int i = 0; i < 50 && !failed; ++i) Pump();
  Expect(failed, "dead peer reports delivery failure on main queue");
}

void TestRelatedWindows() {
  Port receiving = Port::Receive();
  Expect(bool(receiving), "window test receive port");
  Expect(mach_port_insert_right(mach_task_self(), receiving.get(), receiving.get(), MACH_MSG_TYPE_MAKE_SEND) == KERN_SUCCESS,
         "window test send right");
  mach_port_limits_t limits = {MACH_PORT_QLIMIT_MAX};
  Expect(mach_port_set_attributes(mach_task_self(), receiving.get(), MACH_PORT_LIMITS_INFO,
      reinterpret_cast<mach_port_info_t>(&limits), MACH_PORT_LIMITS_INFO_COUNT) == KERN_SUCCESS,
      "window test queue capacity");
  AppIdentity identity;
  identity.appID = @"native-window-test";
  identity.profileID = @"native-window-profile";
  identity.displayName = @"Related Windows Test";
  identity.bundleIdentifier = @"org.floorp.window-test";
  FPAShimApplicationDelegate* controller = [[FPAShimApplicationDelegate alloc]
      initWithIdentity:std::move(identity) host:Port(receiving.get()) hostPID:getpid() verifier:TestPeer(getpid())];
  Expect(controller != nil, "window controller initialized");
  NSMutableArray<NSDictionary*>* notifications = [NSMutableArray array];
  auto command = [&](MessageType type, NSDictionary* payload) {
    [notifications removeAllObjects];
    Message message;
    message.header.type = uint32_t(type);
    message.payload = payload;
    bool result = [controller handle:message];
    NSString* acknowledgement = !result ? nil : type == MessageType::CreateWindow ? @"created"
        : type == MessageType::ConfigureWindow ? @"configured" : nil;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    for (;;) {
      auto received = ReceiveMessage(receiving.get(), acknowledgement ? 10 : 0);
      if (received.status != ReceiveStatus::Message) {
        if (acknowledgement && std::chrono::steady_clock::now() < deadline) continue;
        break;
      }
      if (received.message->header.type == uint32_t(MessageType::WindowChanged)) {
        [notifications addObject:received.message->payload];
        if ([received.message->payload[@"kind"] isEqual:acknowledgement]) break;
      }
    }
    return result;
  };
  Expect(command(MessageType::CreateWindow, @{@"windowId": @71, @"title": @"Native Parent", @"width": @200, @"height": @120,
      @"geometryRequestId": @7}),
         "controller creates native parent");
  Expect([notifications.lastObject[@"geometryRequestId"] isEqual:@7], "creation acknowledges geometry generation");
  Expect(command(MessageType::ConfigureWindow, @{@"windowId": @71, @"width": @200, @"height": @120,
      @"geometryRequestId": @8}), "unchanged frame accepts a new geometry generation");
  Expect([notifications.lastObject[@"kind"] isEqual:@"configured"] &&
      [notifications.lastObject[@"geometryRequestId"] isEqual:@8], "unchanged frame explicitly acknowledges new generation");
  Expect(!command(MessageType::ConfigureWindow, @{@"windowId": @71, @"width": @300, @"geometryRequestId": @8}),
      "replayed geometry generation is rejected");
  Expect(command(MessageType::ConfigureWindow, @{@"windowId": @71, @"width": @220, @"geometryRequestId": @9}),
      "new geometry generation changes frame");
  for (NSDictionary* notification in notifications) {
    Expect([notification[@"geometryRequestId"] isEqual:@9], "synchronous native resize reports latest generation");
  }
  Expect(!command(MessageType::CreateWindow, @{@"windowId": @72, @"title": @"Invalid Popup", @"width": @20, @"height": @20,
      @"kind": @"popup", @"parentWindowId": @999}), "popup rejects unknown parent");
  Expect(command(MessageType::CreateWindow, @{@"windowId": @72, @"title": @"Native Popup", @"width": @20, @"height": @20,
      @"kind": @"popup", @"parentWindowId": @71}), "controller creates small native popup");
  Expect(command(MessageType::CreateWindow, @{@"windowId": @73, @"title": @"Native Dialog", @"width": @160, @"height": @100,
      @"kind": @"dialog", @"parentWindowId": @71}), "controller creates native dialog");
  NSWindow* parent = nil;
  NSWindow* popup = nil;
  NSWindow* dialog = nil;
  for (NSWindow* window in NSApp.windows) {
    if ([window.title isEqual:@"Native Parent"]) parent = window;
    if ([window.title isEqual:@"Native Popup"]) popup = window;
    if ([window.title isEqual:@"Native Dialog"]) dialog = window;
  }
  Expect(parent && popup && dialog, "all related native windows exist");
  Expect([popup isKindOfClass:NSPanel.class] && !(popup.styleMask & NSWindowStyleMaskTitled), "popup uses borderless native panel");
  Expect(popup.parentWindow == parent && dialog.parentWindow == parent, "native parent-child ownership is preserved");
  Expect(command(MessageType::CloseWindow, @{@"windowId": @71}), "parent closure succeeds");
  Expect(!parent.isVisible && !popup.isVisible && dialog.isVisible,
      "parent closure hides transient popup but preserves independent dialog");
  Expect(!popup.parentWindow && !dialog.parentWindow, "surviving children detach from closed parent");
  Expect(command(MessageType::ConfigureWindow, @{@"windowId": @73, @"width": @180, @"geometryRequestId": @1}),
      "live Gecko dialog still accepts controls after its parent closes");
  for (NSNumber* windowId in @[@72, @73]) {
    Expect(command(MessageType::BeginFrame, @{@"windowId": windowId, @"frameId": @1}),
        "queued child frame remains valid after parent closure");
    Expect(command(MessageType::CommitFrame, @{@"windowId": windowId, @"frameId": @1}),
        "queued child frame can commit before its own teardown");
  }
  Expect(command(MessageType::CloseWindow, @{@"windowId": @72}), "popup closes on its own Gecko destruction");
  Expect(command(MessageType::CloseWindow, @{@"windowId": @73}), "dialog closes on its own Gecko destruction");
  Expect(!dialog.isVisible, "dialog is hidden after its own close");
  Expect(!command(MessageType::ConfigureWindow, @{@"windowId": @73, @"width": @190, @"geometryRequestId": @2}),
      "child endpoint is removed only after its own close");
  Expect(command(MessageType::CloseWindow, @{@"windowId": @72}), "repeated child destruction is idempotent");
}

}  // namespace

int main(int argc, char* argv[]) {
  @autoreleasepool {
    if (argc == 5 && !std::strcmp(argv[1], "--profile") && !std::strcmp(argv[3], "--start-ssb")) {
      NSString* profile = [NSString stringWithUTF8String:argv[2]];
      NSMutableArray<NSString*>* profileOverrides = [NSMutableArray array];
      for (NSString* name in @[@"XRE_PROFILE_PATH", @"XRE_PROFILE_LOCAL_PATH",
          @"SELECTABLE_PROFILE_RESET_PATH", @"SELECTABLE_PROFILE_RESET_STORE_ID"]) {
        if (getenv(name.UTF8String)) [profileOverrides addObject:name];
      }
      const char* preserved = getenv("XRE_PROFILE_PATH_EXTRA");
      NSDictionary* observed = @{@"profile": profile, @"appId": [NSString stringWithUTF8String:argv[4]],
          @"pid": @(getpid()), @"noRemote": @(getenv("MOZ_NO_REMOTE") != nullptr),
          @"newInstance": @(getenv("MOZ_NEW_INSTANCE") != nullptr),
          @"profileOverrides": profileOverrides,
          @"preservedEnvironment": preserved ? [NSString stringWithUTF8String:preserved] : @""};
      return [observed writeToFile:[profile stringByAppendingPathComponent:@"observed.plist"] atomically:YES] ? 0 : 1;
    }
    if (argc == 4 && !std::strcmp(argv[1], "--peer")) {
      return PeerMain([NSString stringWithUTF8String:argv[2]], static_cast<pid_t>(std::strtol(argv[3], nullptr, 10)));
    }
    TestProtocol();
    TestSeparateProcess(argv[0]);
    TestSignedPeer();
    TestColdBootstrap(argv[0]);
    TestNativeView();
    TestAsyncSender();
    TestRelatedWindows();
    printf("PASS: %d native App Shim assertions\n", gAssertions);
    return 0;
  }
}
