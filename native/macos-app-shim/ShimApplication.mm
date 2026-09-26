// SPDX-License-Identifier: MPL-2.0

#include "ShimApplication.h"
#include "Session.h"
#include "ShimView.h"

#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>
#include <map>
#include <memory>

using namespace floorp::shim;

@interface FPAShimApplicationDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate>
- (instancetype)initWithIdentity:(AppIdentity)identity host:(Port)host
                         hostPID:(pid_t)hostPID verifier:(PeerVerifier)verifier;
@property(nonatomic, readonly) int exitStatus;
@end

@implementation FPAShimApplicationDelegate {
  AppIdentity _identity;
  Port _host;
  Port _receive;
  pid_t _hostPID;
  std::unique_ptr<SessionGate> _gate;
  std::unique_ptr<MessageSender> _sender;
  NSString* _nonce;
  dispatch_source_t _source;
  dispatch_source_t _hostExit;
  uint64_t _outgoing;
  std::map<uint32_t, __strong NSWindow*> _windows;
  std::map<uint32_t, uint32_t> _geometryRequests;
  BOOL _terminating;
  BOOL _quitPending;
  int _exitStatus;
}

- (instancetype)initWithIdentity:(AppIdentity)identity host:(Port)host
                         hostPID:(pid_t)hostPID verifier:(PeerVerifier)verifier {
  if ((self = [super init])) {
    _identity = std::move(identity);
    _host = std::move(host);
    _hostPID = hostPID;
    _receive = Port::Receive();
    _nonce = NewNonce();
    if (!_receive || !_host || !_nonce || !verifier) return nil;
    _gate = std::make_unique<SessionGate>(_identity.appID, _identity.profileID, _nonce, std::move(verifier));
    __weak FPAShimApplicationDelegate* weakSelf = self;
    _sender = std::make_unique<MessageSender>(_host.get(), [weakSelf] { [weakSelf stop:1]; });
  }
  return self;
}

- (int)exitStatus { return _exitStatus; }

- (void)closeWindow:(NSWindow*)window {
  // Gecko clears widget parent links without destroying independent dialogs.
  // Popup destruction can also be queued after the parent's CloseWindow. Keep
  // each child endpoint alive until its own close so queued frames stay valid.
  for (NSWindow* child in [window.childWindows copy]) {
    [window removeChildWindow:child];
    if (child.styleMask & NSWindowStyleMaskNonactivatingPanel) [child orderOut:nil];
  }
  uint32_t identifier = [self identityForWindow:window];
  [window.parentWindow removeChildWindow:window];
  // Acknowledge only after the window is hidden and its layer removal has
  // committed. The host retains surfaces even when its widget is already gone.
  [CATransaction begin];
  [CATransaction setDisableActions:YES];
  __weak FPAShimApplicationDelegate* weakSelf = self;
  [CATransaction setCompletionBlock:^{
    if (identifier) [weakSelf send:MessageType::WindowChanged
        payload:@{@"windowId": @(identifier), @"kind": @"closed"}];
  }];
  [window orderOut:nil];
  [(FPAShimView*)window.contentView disconnect];
  window.delegate = nil;
  [window close];
  [CATransaction commit];
  if (identifier) {
    _windows.erase(identifier);
    _geometryRequests.erase(identifier);
  }
}

- (void)stop:(int)status {
  if (_terminating) return;
  _terminating = YES;
  _exitStatus = status;
  if (_sender) _sender->Stop();
  if (_source) dispatch_source_cancel(_source);
  if (_hostExit) dispatch_source_cancel(_hostExit);
  while (!_windows.empty()) [self closeWindow:_windows.begin()->second];
  [NSApp stop:nil];
  [NSApp postEvent:[NSEvent otherEventWithType:NSEventTypeApplicationDefined
      location:NSZeroPoint modifierFlags:0 timestamp:0 windowNumber:0 context:nil
      subtype:0 data1:0 data2:0] atStart:NO];
}

- (BOOL)send:(MessageType)type payload:(NSDictionary*)payload {
  if (_terminating) return NO;
  if (!_sender) {
    [self stop:1];
    return NO;
  }
  const EnqueueStatus status = _sender->Enqueue(type, _outgoing + 1, payload);
  if (status == EnqueueStatus::Backpressure) {
    // A full bounded queue is backpressure, not a broken transport. Pointer
    // motion and scrolling are coalescible, so dropping them keeps the app
    // alive; other input is ordered, so losing it would corrupt the page.
    NSString* kind = payload[@"kind"];
    if (type != MessageType::Input ||
        (![kind isEqual:@"mouseMove"] && ![kind isEqual:@"scroll"])) {
      [self stop:1];
      return NO;
    }
    return NO;
  }
  if (status != EnqueueStatus::Accepted) {
    [self stop:1];
    return NO;
  }
  ++_outgoing;
  return YES;
}

- (void)installMenus {
  NSMenu* menu = [[NSMenu alloc] initWithTitle:@""];
  NSMenuItem* appItem = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
  NSMenu* appMenu = [[NSMenu alloc] initWithTitle:_identity.displayName];
  NSMenuItem* hide = [[NSMenuItem alloc] initWithTitle:[NSString stringWithFormat:@"Hide %@", _identity.displayName]
      action:@selector(hide:) keyEquivalent:@"h"];
  hide.target = NSApp;
  [appMenu addItem:hide];
  NSMenuItem* hideOthers = [[NSMenuItem alloc] initWithTitle:@"Hide Others" action:@selector(hideOtherApplications:) keyEquivalent:@"h"];
  hideOthers.target = NSApp;
  hideOthers.keyEquivalentModifierMask = NSEventModifierFlagCommand | NSEventModifierFlagOption;
  [appMenu addItem:hideOthers];
  NSMenuItem* showAll = [[NSMenuItem alloc] initWithTitle:@"Show All" action:@selector(unhideAllApplications:) keyEquivalent:@""];
  showAll.target = NSApp;
  [appMenu addItem:showAll];
  [appMenu addItem:NSMenuItem.separatorItem];
  NSString* quitTitle = [NSString stringWithFormat:@"Quit %@", _identity.displayName];
  NSMenuItem* quit = [[NSMenuItem alloc] initWithTitle:quitTitle action:@selector(terminate:) keyEquivalent:@"q"];
  quit.target = NSApp;
  [appMenu addItem:quit];
  appItem.submenu = appMenu;
  [menu addItem:appItem];
  NSMenuItem* fileItem = [[NSMenuItem alloc] initWithTitle:@"File" action:nil keyEquivalent:@""];
  NSMenu* file = [[NSMenu alloc] initWithTitle:@"File"];
  [file addItemWithTitle:@"Close Window" action:@selector(performClose:) keyEquivalent:@"w"];
  fileItem.submenu = file;
  [menu addItem:fileItem];
  NSMenuItem* editItem = [[NSMenuItem alloc] initWithTitle:@"Edit" action:nil keyEquivalent:@""];
  NSMenu* edit = [[NSMenu alloc] initWithTitle:@"Edit"];
  NSArray<NSArray<NSString*>*>* commands = @[@[@"Undo", @"z", @"undo"], @[@"Cut", @"x", @"cut"],
      @[@"Copy", @"c", @"copy"], @[@"Paste", @"v", @"paste"], @[@"Select All", @"a", @"selectAll"]];
  for (NSArray<NSString*>* command in commands) {
    NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:command[0] action:@selector(menuCommand:) keyEquivalent:command[1]];
    item.target = self;
    item.representedObject = command[2];
    [edit addItem:item];
  }
  editItem.submenu = edit;
  [menu addItem:editItem];
  NSMenuItem* windowItem = [[NSMenuItem alloc] initWithTitle:@"Window" action:nil keyEquivalent:@""];
  NSMenu* windowMenu = [[NSMenu alloc] initWithTitle:@"Window"];
  [windowMenu addItemWithTitle:@"Minimize" action:@selector(performMiniaturize:) keyEquivalent:@"m"];
  windowItem.submenu = windowMenu;
  [menu addItem:windowItem];
  NSApp.mainMenu = menu;
  NSApp.windowsMenu = windowMenu;
}

- (void)applicationDidFinishLaunching:(NSNotification*)notification {
  [self installMenus];
  __weak FPAShimApplicationDelegate* weakSelf = self;
  _source = dispatch_source_create(DISPATCH_SOURCE_TYPE_MACH_RECV, _receive.get(), 0, dispatch_get_main_queue());
  dispatch_source_set_event_handler(_source, ^{ [weakSelf receiveMessages]; });
  dispatch_resume(_source);
  _hostExit = dispatch_source_create(DISPATCH_SOURCE_TYPE_PROC, _hostPID, DISPATCH_PROC_EXIT, dispatch_get_main_queue());
  if (_hostExit) {
    dispatch_source_set_event_handler(_hostExit, ^{ [weakSelf stop:1]; });
    dispatch_resume(_hostExit);
  }
  NSDictionary* hello = @{@"appId": _identity.appID, @"profileId": _identity.profileID,
      @"nonce": _nonce, @"bundleIdentifier": _identity.bundleIdentifier,
      @"launchToken": _identity.launchToken,
      @"capabilities": @[@"iosurface-bgra-layers-v1", @"text-input-v1", @"native-window-v1"]};
  if (!SendMessage(_host.get(), MessageType::Hello, ++_outgoing, hello,
                   _receive.get(), MACH_MSG_TYPE_MAKE_SEND)) {
    [self stop:1];
    return;
  }
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC), dispatch_get_main_queue(), ^{
    FPAShimApplicationDelegate* app = weakSelf;
    if (app && !app->_gate->active()) [app stop:1];
  });
}

- (BOOL)handle:(Message&)message {
  MessageType type = static_cast<MessageType>(message.header.type);
  NSDictionary* payload = message.payload;
  bool surfaceLayer = type == MessageType::SetLayer && !payload[@"color"];
  if (surfaceLayer != static_cast<bool>(message.attachment)) return NO;
  if (type == MessageType::HelloAccepted) return YES;
  if (type == MessageType::Terminate) { [self stop:0]; return YES; }
  if (type == MessageType::CancelQuit) { _quitPending = NO; return YES; }
  uint32_t identifier;
  if (!ReadUInt(payload, @"windowId", &identifier)) return NO;
  auto existing = _windows.find(identifier);
  if (type == MessageType::CreateWindow) {
    NSString* title = ReadString(payload, @"title", 512);
    NSString* kind = payload[@"kind"] ? ReadString(payload, @"kind", 16) : @"window";
    bool popup = [kind isEqual:@"popup"];
    bool dialog = [kind isEqual:@"dialog"];
    if (!kind || (!popup && !dialog && ![kind isEqual:@"window"])) return NO;
    NSWindow* parent = nil;
    if (payload[@"parentWindowId"]) {
      uint32_t parentIdentifier;
      if (!ReadUInt(payload, @"parentWindowId", &parentIdentifier)) return NO;
      auto candidate = _windows.find(parentIdentifier);
      if (candidate == _windows.end()) return NO;
      parent = candidate->second;
    }
    if (popup && !parent) return NO;
    uint32_t geometryRequest = 0;
    if (payload[@"geometryRequestId"] &&
        !ReadUInt(payload, @"geometryRequestId", &geometryRequest, true)) return NO;
    double width, height;
    if (existing != _windows.end() || _windows.size() >= kMaxWindows || !title ||
        !ReadNumber(payload, @"width", &width, popup ? 1 : 64, 16384) ||
        !ReadNumber(payload, @"height", &height, popup ? 1 : 64, 16384)) return NO;
    NSWindowStyleMask style = popup ? NSWindowStyleMaskBorderless | NSWindowStyleMaskNonactivatingPanel
        : NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable;
    if (!popup && !dialog) style |= NSWindowStyleMaskMiniaturizable;
    NSWindow* window = [[popup ? NSPanel.class : NSWindow.class alloc]
        initWithContentRect:NSMakeRect(0, 0, width, height) styleMask:style
        backing:NSBackingStoreBuffered defer:NO];
    window.releasedWhenClosed = NO;
    window.title = title;
    window.delegate = self;
    window.acceptsMouseMovedEvents = YES;
    window.collectionBehavior = parent || popup || dialog
        ? NSWindowCollectionBehaviorFullScreenAuxiliary : NSWindowCollectionBehaviorFullScreenPrimary;
    if (popup) {
      NSPanel* panel = (NSPanel*)window;
      panel.becomesKeyOnlyIfNeeded = YES;
      panel.hidesOnDeactivate = NO;
    }
    __weak FPAShimApplicationDelegate* weakSelf = self;
    window.contentView = [[FPAShimView alloc] initWithFrame:NSMakeRect(0, 0, width, height)
        windowID:identifier sink:^(MessageType event, NSDictionary* value) { [weakSelf send:event payload:value]; }];
    _windows.emplace(identifier, window);
    _geometryRequests.emplace(identifier, geometryRequest);
    [window center];
    if (parent) [parent addChildWindow:window ordered:NSWindowAbove];
    [window makeFirstResponder:window.contentView];
    if (popup) [window orderFront:nil];
    else {
      [window makeKeyAndOrderFront:nil];
      if (@available(macOS 14.0, *)) [NSApp activate];
      else [NSApp activateIgnoringOtherApps:YES];
    }
    [self windowChanged:window kind:@"created"];
    return YES;
  }
  if (existing == _windows.end()) return type == MessageType::CloseWindow;
  NSWindow* window = existing->second;
  FPAShimView* view = (FPAShimView*)window.contentView;
  switch (type) {
    case MessageType::CloseWindow:
      [self closeWindow:window];
      return YES;
    case MessageType::SetWindowTitle: {
      NSString* title = ReadString(payload, @"title", 512);
      if (!title) return NO;
      window.title = title;
      return YES;
    }
    case MessageType::ActivateWindow:
      [window deminiaturize:nil];
      [window makeKeyAndOrderFront:nil];
      if (@available(macOS 14.0, *)) {
        // The authenticated host yields activation before sending this command.
        [NSApp activate];
      } else {
        [NSApp activateIgnoringOtherApps:YES];
      }
      return YES;
    case MessageType::ConfigureWindow: {
      NSRect content = [window contentRectForFrameRect:window.frame];
      double top = NSMaxY(NSScreen.screens.firstObject.frame);
      double x = content.origin.x;
      double y = top - NSMaxY(content);
      double width = content.size.width;
      double height = content.size.height;
      bool visible = window.isVisible;
      bool enabled = view.inputEnabled;
      bool fullscreen = (window.styleMask & NSWindowStyleMaskFullScreen) != 0;
      bool minimized = window.isMiniaturized;
      bool maximized = window.isZoomed;
      uint32_t geometryRequest = _geometryRequests.at(identifier);
      if (payload[@"geometryRequestId"] &&
          (!ReadUInt(payload, @"geometryRequestId", &geometryRequest, true) ||
           geometryRequest <= _geometryRequests.at(identifier))) return NO;
      bool geometryChanged = payload[@"x"] || payload[@"y"] || payload[@"width"] || payload[@"height"];
      double minimumSize = (window.styleMask & NSWindowStyleMaskTitled) ? 64 : 1;
      auto boolean = [&](NSString* key, bool* value) {
        if (!payload[key]) return true;
        if (CFGetTypeID((__bridge CFTypeRef)payload[key]) != CFBooleanGetTypeID()) return false;
        *value = [payload[key] boolValue];
        return true;
      };
      if ((payload[@"x"] && !ReadNumber(payload, @"x", &x, -65536, 65536)) ||
          (payload[@"y"] && !ReadNumber(payload, @"y", &y, -65536, 65536)) ||
          (payload[@"width"] && !ReadNumber(payload, @"width", &width, minimumSize, 16384)) ||
          (payload[@"height"] && !ReadNumber(payload, @"height", &height, minimumSize, 16384)) ||
          !boolean(@"visible", &visible) || !boolean(@"enabled", &enabled) ||
          !boolean(@"fullscreen", &fullscreen) || !boolean(@"minimized", &minimized) ||
          !boolean(@"maximized", &maximized)) return NO;
      // Set before AppKit can synchronously emit move/resize notifications.
      _geometryRequests.at(identifier) = geometryRequest;
      if (geometryChanged) {
        NSRect target = NSMakeRect(x, top - y - height, width, height);
        [window setFrame:[window frameRectForContentRect:target] display:YES];
      }
      window.ignoresMouseEvents = !enabled;
      view.inputEnabled = enabled;
      if (payload[@"visible"]) {
        if (visible) [window orderFront:nil];
        else [window orderOut:nil];
      }
      if (fullscreen != ((window.styleMask & NSWindowStyleMaskFullScreen) != 0)) [window toggleFullScreen:nil];
      if (payload[@"minimized"] && minimized != window.isMiniaturized) {
        if (minimized) [window miniaturize:nil];
        else [window deminiaturize:nil];
      }
      if (payload[@"maximized"] && maximized != window.isZoomed) [window zoom:nil];
      // Even an unchanged native frame acknowledges the host's latest request.
      if (geometryChanged || payload[@"geometryRequestId"]) [self windowChanged:window kind:@"configured"];
      return YES;
    }
    case MessageType::BeginFrame: return [view beginFrame:payload];
    case MessageType::SetLayer: return [view setLayer:payload surfacePort:message.attachment.get()];
    case MessageType::RemoveLayer: return [view removeLayer:payload];
    case MessageType::CommitFrame: return [view commitFrame:payload];
    case MessageType::EditorState: return [view setEditorState:payload];
    case MessageType::SetCursor: return [view setCursorState:payload];
    default: return NO;
  }
}

- (void)receiveMessages {
  for (int count = 0; count < 64 && !_terminating; ++count) {
    ReceiveResult result = ReceiveMessage(_receive.get(), 0);
    if (result.status == ReceiveStatus::Timeout) return;
    if (result.status != ReceiveStatus::Message || !_gate->Accept(*result.message) ||
        ![self handle:*result.message]) {
      [self stop:1];
      return;
    }
  }
}

- (uint32_t)identityForWindow:(NSWindow*)window {
  for (const auto& [identity, candidate] : _windows) if (candidate == window) return identity;
  return 0;
}

- (void)windowChanged:(NSWindow*)window kind:(NSString*)kind {
  uint32_t identifier = [self identityForWindow:window];
  if (!identifier) return;
  NSSize size = window.contentView.bounds.size;
  NSRect content = [window contentRectForFrameRect:window.frame];
  double top = NSMaxY(NSScreen.screens.firstObject.frame);
  [self send:MessageType::WindowChanged payload:@{@"windowId": @(identifier), @"kind": kind,
      @"geometryRequestId": @(_geometryRequests.at(identifier)),
      @"x": @(content.origin.x), @"y": @(top - NSMaxY(content)),
      @"width": @(size.width), @"height": @(size.height), @"scale": @(window.backingScaleFactor),
      @"key": @(window.isKeyWindow), @"visible": @(window.isVisible),
      @"minimized": @(window.isMiniaturized), @"maximized": @(window.isZoomed),
      @"occluded": @(!(window.occlusionState & NSWindowOcclusionStateVisible)),
      @"fullscreen": @((window.styleMask & NSWindowStyleMaskFullScreen) != 0)}];
}
- (BOOL)windowShouldClose:(NSWindow*)window { [self windowChanged:window kind:@"closeRequested"]; return NO; }
- (void)windowDidResize:(NSNotification*)note { [self windowChanged:note.object kind:@"resized"]; }
- (void)windowDidMove:(NSNotification*)note { [self windowChanged:note.object kind:@"moved"]; }
- (void)windowDidChangeBackingProperties:(NSNotification*)note { [self windowChanged:note.object kind:@"backingChanged"]; }
- (void)windowDidBecomeKey:(NSNotification*)note { [self windowChanged:note.object kind:@"focusChanged"]; }
- (void)windowDidResignKey:(NSNotification*)note { [self windowChanged:note.object kind:@"focusChanged"]; }
- (void)windowDidChangeOcclusionState:(NSNotification*)note { [self windowChanged:note.object kind:@"occlusionChanged"]; }
- (void)windowDidEnterFullScreen:(NSNotification*)note { [self windowChanged:note.object kind:@"fullscreenChanged"]; }
- (void)windowDidExitFullScreen:(NSNotification*)note { [self windowChanged:note.object kind:@"fullscreenChanged"]; }
- (void)windowDidMiniaturize:(NSNotification*)note { [self windowChanged:note.object kind:@"minimizedChanged"]; }
- (void)windowDidDeminiaturize:(NSNotification*)note { [self windowChanged:note.object kind:@"minimizedChanged"]; }
- (void)menuCommand:(NSMenuItem*)item {
  uint32_t identifier = [self identityForWindow:NSApp.keyWindow];
  if (identifier) [self send:MessageType::MenuCommand payload:@{@"windowId": @(identifier), @"command": item.representedObject}];
}
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*)sender {
  if (_terminating) return NSTerminateNow;
  if (!_quitPending) {
    _quitPending = YES;
    [self send:MessageType::QuitRequested payload:@{@"appId": _identity.appID}];
  }
  return NSTerminateCancel;
}
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender { return NO; }
- (BOOL)applicationShouldHandleReopen:(NSApplication*)sender hasVisibleWindows:(BOOL)visible {
  if (_gate->active()) [self send:MessageType::ReopenRequested payload:@{@"appId": _identity.appID}];
  return NO;
}

@end

namespace floorp::shim {

int RunApplication(AppIdentity identity, Port host, pid_t hostPID, PeerVerifier verifier) {
  if (!ValidIdentity(identity.appID) || !ValidIdentity(identity.profileID) ||
      !identity.displayName.length || !identity.bundleIdentifier.length ||
      identity.launchToken.length != 64 || !host || !verifier) return 1;
  [NSApplication sharedApplication];
  NSApp.activationPolicy = NSApplicationActivationPolicyRegular;
  NSWindow.allowsAutomaticWindowTabbing = NO;
  FPAShimApplicationDelegate* delegate = [[FPAShimApplicationDelegate alloc]
      initWithIdentity:std::move(identity) host:std::move(host) hostPID:hostPID verifier:std::move(verifier)];
  if (!delegate) return 1;
  NSApp.delegate = delegate;
  [NSApp run];
  NSApp.delegate = nil;
  return delegate.exitStatus;
}

}  // namespace floorp::shim
