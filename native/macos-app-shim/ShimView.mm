// SPDX-License-Identifier: MPL-2.0

#include "ShimView.h"

#import <IOSurface/IOSurface.h>
#import <QuartzCore/QuartzCore.h>
#import <Carbon/Carbon.h>
#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <vector>

using namespace floorp::shim;

namespace {

struct Surface {
  IOSurfaceRef value;
  uint32_t identity;
  size_t bytes;
  Surface(IOSurfaceRef surface, uint32_t identifier)
      : value(surface), identity(identifier), bytes(IOSurfaceGetAllocSize(surface)) {
    IOSurfaceIncrementUseCount(value);
  }
  ~Surface() {
    IOSurfaceDecrementUseCount(value);
    CFRelease(value);
  }
};

struct LayerState {
  std::shared_ptr<Surface> surface;
  __strong CALayer* layer;
  CGRect frame;
  CGRect clip;
  bool clipped;
  double scale;
  double opacity;
  double z;
  bool raw = false;
  bool solid = false;
  double color[4] = {};
  double sizeWidth = 0;
  double sizeHeight = 0;
  CGPoint position = {};
  CGRect display = {};
  CATransform3D transform = CATransform3DIdentity;
  bool flipped = false;
  bool nearest = false;
  bool rounded = false;
  CGRect roundedRect = {};
  CGSize radii[4] = {};
};

bool ReadRect(id value, CGRect* result) {
  if (![value isKindOfClass:NSDictionary.class]) return false;
  double x, y, width, height;
  if (!ReadNumber(value, @"x", &x, -65536, 65536) ||
      !ReadNumber(value, @"y", &y, -65536, 65536) ||
      !ReadNumber(value, @"width", &width, 0, 32768) ||
      !ReadNumber(value, @"height", &height, 0, 32768)) return false;
  *result = CGRectMake(x, y, width, height);
  return true;
}

bool ReadBool(id value, bool* result) {
  if (!value || CFGetTypeID((__bridge CFTypeRef)value) != CFBooleanGetTypeID()) return false;
  *result = [value boolValue];
  return true;
}

CGRect ScaleRect(CGRect rect, double scale) {
  return CGRectMake(rect.origin.x / scale, rect.origin.y / scale,
                    rect.size.width / scale, rect.size.height / scale);
}

CGPathRef RoundedPath(const LayerState& state) CF_RETURNS_RETAINED {
  CGRect rect = ScaleRect(state.roundedRect, state.scale);
  CGSize r[4];
  for (int i = 0; i < 4; ++i) r[i] = CGSizeMake(state.radii[i].width / state.scale, state.radii[i].height / state.scale);
  double x = rect.origin.x, y = rect.origin.y, right = CGRectGetMaxX(rect), bottom = CGRectGetMaxY(rect);
  constexpr double k = 0.5522847498307936;
  CGMutablePathRef path = CGPathCreateMutable();
  CGPathMoveToPoint(path, nullptr, x + r[0].width, y);
  CGPathAddLineToPoint(path, nullptr, right - r[1].width, y);
  CGPathAddCurveToPoint(path, nullptr, right - r[1].width * (1-k), y,
      right, y + r[1].height * (1-k), right, y + r[1].height);
  CGPathAddLineToPoint(path, nullptr, right, bottom - r[2].height);
  CGPathAddCurveToPoint(path, nullptr, right, bottom - r[2].height * (1-k),
      right - r[2].width * (1-k), bottom, right - r[2].width, bottom);
  CGPathAddLineToPoint(path, nullptr, x + r[3].width, bottom);
  CGPathAddCurveToPoint(path, nullptr, x + r[3].width * (1-k), bottom,
      x, bottom - r[3].height * (1-k), x, bottom - r[3].height);
  CGPathAddLineToPoint(path, nullptr, x, y + r[0].height);
  CGPathAddCurveToPoint(path, nullptr, x, y + r[0].height * (1-k),
      x + r[0].width * (1-k), y, x + r[0].width, y);
  CGPathCloseSubpath(path);
  return path;
}

CALayer* GeometryLayer() {
  CALayer* layer = [CALayer layer];
  layer.anchorPoint = CGPointZero;
  layer.position = CGPointZero;
  layer.bounds = CGRectZero;
  layer.edgeAntialiasingMask = 0;
  return layer;
}

CALayer* BuildRawLayer(const LayerState& state, CGRect viewBounds) {
  CALayer* root = GeometryLayer();
  root.bounds = viewBounds;
  root.opacity = state.opacity;
  root.zPosition = state.z;
  if (state.clipped) {
    CAShapeLayer* mask = [CAShapeLayer layer];
    CGPathRef path = CGPathCreateWithRect(ScaleRect(state.clip, state.scale), nullptr);
    mask.path = path;
    CGPathRelease(path);
    root.mask = mask;
  }
  CALayer* rounded = GeometryLayer();
  rounded.bounds = viewBounds;
  if (state.rounded) {
    CAShapeLayer* mask = [CAShapeLayer layer];
    CGPathRef path = RoundedPath(state);
    mask.path = path;
    CGPathRelease(path);
    rounded.mask = mask;
  }
  [root addSublayer:rounded];
  CALayer* transformed = GeometryLayer();
  transformed.transform = state.transform;
  [rounded addSublayer:transformed];
  CALayer* crop = GeometryLayer();
  CGRect display = ScaleRect(state.display, state.scale);
  crop.position = display.origin;
  crop.bounds = CGRectMake(0, 0, display.size.width, display.size.height);
  crop.masksToBounds = YES;
  [transformed addSublayer:crop];
  CALayer* content = GeometryLayer();
  content.position = CGPointMake(-display.origin.x, -display.origin.y);
  content.bounds = CGRectMake(0, 0, state.sizeWidth / state.scale, state.sizeHeight / state.scale);
  content.contentsScale = state.scale;
  content.contentsGravity = kCAGravityTopLeft;
  content.minificationFilter = state.nearest ? kCAFilterNearest : kCAFilterLinear;
  content.magnificationFilter = content.minificationFilter;
  if (state.flipped) {
    content.transform = CATransform3DMakeScale(1, -1, 1);
    content.position = CGPointMake(-display.origin.x, state.sizeHeight / state.scale - display.origin.y);
  }
  if (state.solid) {
    CGColorRef color = CGColorCreateSRGB(state.color[0], state.color[1], state.color[2], state.color[3]);
    content.backgroundColor = color;
    CGColorRelease(color);
  } else {
    content.contents = (__bridge id)state.surface->value;
  }
  [crop addSublayer:content];
  return root;
}

bool ParseRawLayer(NSDictionary* payload, LayerState* state) {
  for (NSString* key in @[@"isHDR", @"isDRM"]) {
    bool value = false;
    if (payload[key] && (!ReadBool(payload[key], &value) || value)) return false;
  }
  double x, y;
  if (!ReadNumber(payload, @"positionX", &x, -65536, 65536) ||
      !ReadNumber(payload, @"positionY", &y, -65536, 65536) ||
      !ReadNumber(payload, @"sizeWidth", &state->sizeWidth, 0, 32768) ||
      !ReadNumber(payload, @"sizeHeight", &state->sizeHeight, 0, 32768) ||
      !ReadRect(payload[@"displayRect"], &state->display)) return false;
  state->raw = true;
  state->position = CGPointMake(x, y);
  double matrix[16] = {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
  if (payload[@"transform16"]) {
    NSArray* values = payload[@"transform16"];
    if (![values isKindOfClass:NSArray.class] || values.count != 16) return false;
    for (NSUInteger i = 0; i < 16; ++i) {
      if (!ReadNumber(@{@"v":values[i]}, @"v", &matrix[i], -1e6, 1e6)) return false;
    }
  }
  for (int column = 0; column < 4; ++column) {
    matrix[12 + column] += x * matrix[column] + y * matrix[4 + column];
  }
  // Match NativeLayerCA::Representation's CATransform3D conversion exactly:
  // only X/Y translation converts device pixels to Cocoa points. Perspective
  // coefficients and Z translation keep the compositor's original units.
  state->transform = CATransform3D{
      matrix[0], matrix[1], matrix[2], matrix[3],
      matrix[4], matrix[5], matrix[6], matrix[7],
      matrix[8], matrix[9], matrix[10], matrix[11],
      matrix[12] / state->scale, matrix[13] / state->scale, matrix[14], matrix[15]};
  if (payload[@"flipped"] && !ReadBool(payload[@"flipped"], &state->flipped)) return false;
  if (payload[@"sampling"]) {
    if (![payload[@"sampling"] isEqual:@"nearest"] && ![payload[@"sampling"] isEqual:@"linear"]) return false;
    state->nearest = [payload[@"sampling"] isEqual:@"nearest"];
  }
  if (payload[@"color"]) {
    NSDictionary* color = payload[@"color"];
    if (![color isKindOfClass:NSDictionary.class] ||
        !ReadNumber(color, @"r", &state->color[0], 0, 1) ||
        !ReadNumber(color, @"g", &state->color[1], 0, 1) ||
        !ReadNumber(color, @"b", &state->color[2], 0, 1) ||
        !ReadNumber(color, @"a", &state->color[3], 0, 1)) return false;
    state->solid = true;
    if (!state->sizeWidth) state->sizeWidth = CGRectGetMaxX(state->display);
    if (!state->sizeHeight) state->sizeHeight = CGRectGetMaxY(state->display);
  }
  if (state->sizeWidth <= 0 || state->sizeHeight <= 0) return false;
  if (payload[@"roundedClip"]) {
    NSDictionary* rounded = payload[@"roundedClip"];
    if (![rounded isKindOfClass:NSDictionary.class] || !ReadRect(rounded[@"rect"], &state->roundedRect)) return false;
    NSArray* radii = rounded[@"radii"];
    if (![radii isKindOfClass:NSArray.class] || radii.count != 4) return false;
    for (NSUInteger i = 0; i < 4; ++i) {
      NSDictionary* radius = radii[i];
      double width, height;
      if (![radius isKindOfClass:NSDictionary.class] ||
          !ReadNumber(radius, @"width", &width, 0, state->roundedRect.size.width) ||
          !ReadNumber(radius, @"height", &height, 0, state->roundedRect.size.height)) return false;
      state->radii[i] = CGSizeMake(width, height);
    }
    auto* r = state->radii;
    if (r[0].width + r[1].width > state->roundedRect.size.width ||
        r[3].width + r[2].width > state->roundedRect.size.width ||
        r[0].height + r[3].height > state->roundedRect.size.height ||
        r[1].height + r[2].height > state->roundedRect.size.height) return false;
    state->rounded = true;
  }
  return true;
}

NSDictionary* RangePayload(NSRange range) {
  return @{@"location": range.location == NSNotFound ? @(-1) : @(range.location),
           @"length": @(range.length)};
}

NSString* PlainText(id value) {
  if ([value isKindOfClass:NSAttributedString.class]) return [value string];
  return [value isKindOfClass:NSString.class] ? value : nil;
}

bool ReadRange(NSDictionary* payload, NSString* startKey, NSString* lengthKey,
               NSUInteger bound, NSRange* range) {
  uint32_t start, length;
  if (!ReadUInt(payload, startKey, &start, true) ||
      !ReadUInt(payload, lengthKey, &length, true) ||
      start > bound || length > bound - start) return false;
  *range = NSMakeRange(start, length);
  return true;
}

}  // namespace

@implementation FPAShimView {
  uint32_t _windowID;
  FPAShimEventSink _sink;
  NSTrackingArea* _tracking;
  std::map<uint32_t, LayerState> _layers;
  std::map<uint32_t, LayerState> _pending;
  std::set<uint32_t> _removed;
  std::set<uint32_t> _surfaceIDs;
  size_t _surfaceBytes;
  uint32_t _pendingFrame;
  uint32_t _committedFrameID;
  uint32_t _editorRevision;
  uint32_t _textOffset;
  NSString* _editorText;
  NSRange _selectedRange;
  NSRange _markedRange;
  NSRect _caret;
  BOOL _editable;
  BOOL _inputEnabled;
  BOOL _password;
  BOOL _secureInput;
  NSCursor* _cursor;
}

- (instancetype)initWithFrame:(NSRect)frame windowID:(uint32_t)windowID
                         sink:(FPAShimEventSink)sink {
  if ((self = [super initWithFrame:frame])) {
    _windowID = windowID;
    _inputEnabled = YES;
    _cursor = NSCursor.arrowCursor;
    _sink = [sink copy];
    _editorText = @"";
    _selectedRange = NSMakeRange(0, 0);
    _markedRange = NSMakeRange(NSNotFound, 0);
    self.wantsLayer = YES;
    self.layer.backgroundColor = NSColor.windowBackgroundColor.CGColor;
    self.layer.masksToBounds = YES;
    NSNotificationCenter* notifications = NSNotificationCenter.defaultCenter;
    for (NSNotificationName name in @[NSWindowDidBecomeKeyNotification, NSWindowDidResignKeyNotification,
        NSApplicationDidBecomeActiveNotification, NSApplicationDidResignActiveNotification]) {
      [notifications addObserver:self selector:@selector(secureInputChanged:) name:name object:nil];
    }
  }
  return self;
}

- (BOOL)isFlipped { return YES; }
- (BOOL)acceptsFirstResponder { return _inputEnabled; }
- (BOOL)inputEnabled { return _inputEnabled; }
- (void)setInputEnabled:(BOOL)value {
  _inputEnabled = value;
  [self.window invalidateCursorRectsForView:self];
  [self updateSecureInput];
}
- (NSUInteger)presentedLayerCount { return _layers.size(); }
- (uint32_t)committedFrameID { return _committedFrameID; }
- (NSCursor*)nativeCursor { return _cursor; }

- (BOOL)setCursorState:(NSDictionary*)payload {
  NSString* name = ReadString(payload, @"cursor", 32);
  NSCursor* cursor = nil;
  if ([name isEqual:@"default"]) cursor = NSCursor.arrowCursor;
  else if ([name isEqual:@"text"]) cursor = NSCursor.IBeamCursor;
  else if ([name isEqual:@"vertical-text"]) cursor = NSCursor.IBeamCursorForVerticalLayout;
  else if ([name isEqual:@"pointer"]) cursor = NSCursor.pointingHandCursor;
  else if ([name isEqual:@"crosshair"]) cursor = NSCursor.crosshairCursor;
  else if ([name isEqual:@"grab"]) cursor = NSCursor.openHandCursor;
  else if ([name isEqual:@"grabbing"] || [name isEqual:@"move"]) cursor = NSCursor.closedHandCursor;
  else if ([name isEqual:@"copy"]) cursor = NSCursor.dragCopyCursor;
  else if ([name isEqual:@"alias"]) cursor = NSCursor.dragLinkCursor;
  else if ([name isEqual:@"context-menu"]) cursor = NSCursor.contextualMenuCursor;
  else if ([name isEqual:@"not-allowed"]) cursor = NSCursor.operationNotAllowedCursor;
  else if ([name isEqual:@"ew-resize"]) {
    if (@available(macOS 15.0, *)) cursor = NSCursor.columnResizeCursor;
    else cursor = NSCursor.resizeLeftRightCursor;
  } else if ([name isEqual:@"ns-resize"]) {
    if (@available(macOS 15.0, *)) cursor = NSCursor.rowResizeCursor;
    else cursor = NSCursor.resizeUpDownCursor;
  } else if ([name isEqual:@"nwse-resize"] || [name isEqual:@"nesw-resize"]) {
    if (@available(macOS 15.0, *)) {
      NSCursorFrameResizePosition position = [name isEqual:@"nwse-resize"]
          ? NSCursorFrameResizePositionTopLeft : NSCursorFrameResizePositionTopRight;
      cursor = [NSCursor frameResizeCursorFromPosition:position inDirections:NSCursorFrameResizeDirectionsAll];
    } else cursor = NSCursor.crosshairCursor;
  } else if ([name isEqual:@"zoom-in"] || [name isEqual:@"zoom-out"]) {
    if (@available(macOS 15.0, *)) cursor = [name isEqual:@"zoom-in"] ? NSCursor.zoomInCursor : NSCursor.zoomOutCursor;
    else cursor = NSCursor.arrowCursor;
  } else if ([name isEqual:@"none"]) {
    static NSCursor* invisible;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
      invisible = [[NSCursor alloc] initWithImage:[[NSImage alloc] initWithSize:NSMakeSize(1, 1)] hotSpot:NSZeroPoint];
    });
    cursor = invisible;
  }
  if (!cursor) return NO;
  _cursor = cursor;
  [self.window invalidateCursorRectsForView:self];
  [self applyCursor];
  return YES;
}

- (void)applyCursor {
  NSPoint screen = NSEvent.mouseLocation;
  if (!_sink || !_inputEnabled || !NSApp.isActive || !self.window.isVisible ||
      [NSWindow windowNumberAtPoint:screen belowWindowWithWindowNumber:0] != self.window.windowNumber) return;
  NSPoint point = [self convertPoint:[self.window convertPointFromScreen:screen] fromView:nil];
  if (NSPointInRect(point, self.visibleRect)) [_cursor set];
}

- (void)resetCursorRects {
  [self addCursorRect:self.visibleRect cursor:_inputEnabled && _sink ? _cursor : NSCursor.arrowCursor];
}
- (void)cursorUpdate:(NSEvent*)event { [self applyCursor]; }

- (void)emit:(MessageType)type payload:(NSDictionary*)payload {
  if (!_sink || (type == MessageType::Input && !_inputEnabled)) return;
  NSMutableDictionary* value = [payload mutableCopy];
  value[@"windowId"] = @(_windowID);
  _sink(type, value);
}

- (BOOL)matchesFrame:(NSDictionary*)payload {
  uint32_t frame;
  return _pendingFrame && ReadUInt(payload, @"frameId", &frame) && frame == _pendingFrame;
}

- (BOOL)beginFrame:(NSDictionary*)payload {
  uint32_t frame;
  if (_pendingFrame || !ReadUInt(payload, @"frameId", &frame) || frame <= _committedFrameID) return NO;
  _pendingFrame = frame;
  return YES;
}

- (BOOL)setLayer:(NSDictionary*)payload surfacePort:(mach_port_t)port {
  uint32_t layerID, surfaceID;
  bool solid = payload[@"color"] != nil;
  if (![self matchesFrame:payload] ||
      !ReadUInt(payload, @"layerId", &layerID) ||
      (!solid && (!MACH_PORT_VALID(port) || !ReadUInt(payload, @"surfaceId", &surfaceID) || _surfaceIDs.count(surfaceID))) ||
      (solid && (MACH_PORT_VALID(port) || payload[@"surfaceId"])) ||
      _pending.count(layerID) || _removed.count(layerID)) return NO;
  if (_pending.size() >= kMaxLayers) return NO;
  LayerState state{};
  double x, y, width, height;
  if (!ReadNumber(payload, @"scale", &state.scale, 0.25, 8) ||
      !ReadNumber(payload, @"opacity", &state.opacity, 0, 1) ||
      !ReadNumber(payload, @"zOrder", &state.z, -65536, 65536)) return NO;
  if (payload[@"positionX"]) {
    if (!ParseRawLayer(payload, &state)) return NO;
  } else if (solid || !ReadNumber(payload, @"x", &x, -65536, 65536) ||
      !ReadNumber(payload, @"y", &y, -65536, 65536) ||
      !ReadNumber(payload, @"width", &width, 0.01, 32768) ||
      !ReadNumber(payload, @"height", &height, 0.01, 32768)) return NO;
  if (!state.raw) state.frame = CGRectMake(x, y, width, height);
  if (payload[@"clip"]) {
    NSDictionary* clip = payload[@"clip"];
    if (![clip isKindOfClass:NSDictionary.class] ||
        !ReadNumber(clip, @"x", &x, -65536, 65536) ||
        !ReadNumber(clip, @"y", &y, -65536, 65536) ||
        !ReadNumber(clip, @"width", &width, 0, 32768) ||
        !ReadNumber(clip, @"height", &height, 0, 32768)) return NO;
    state.clipped = true;
    state.clip = CGRectMake(x, y, width, height);
  }
  if (solid) {
    _pending.emplace(layerID, std::move(state));
    return YES;
  }
  IOSurfaceRef surface = IOSurfaceLookupFromMachPort(port);
  if (!surface) return NO;
  size_t bytes = IOSurfaceGetAllocSize(surface);
  size_t pixelWidth = IOSurfaceGetWidth(surface);
  size_t pixelHeight = IOSurfaceGetHeight(surface);
  size_t stride = IOSurfaceGetBytesPerRow(surface);
  bool valid = pixelWidth && pixelHeight && pixelWidth <= 16384 && pixelHeight <= 16384 &&
      IOSurfaceGetPlaneCount(surface) == 0 && IOSurfaceGetPixelFormat(surface) == 'BGRA' &&
      IOSurfaceGetBytesPerElement(surface) == 4 && stride >= pixelWidth * 4 &&
      bytes && bytes <= kMaxSurfaceBytes && pixelHeight <= bytes / stride;
  valid = valid && (!state.raw || (state.sizeWidth == pixelWidth && state.sizeHeight == pixelHeight));
  if (!valid || bytes > kMaxWindowSurfaceBytes - _surfaceBytes) {
    CFRelease(surface);
    return NO;
  }
  state.surface = std::make_shared<Surface>(surface, surfaceID);
  _surfaceBytes += bytes;
  state.layer = [CALayer layer];
  _surfaceIDs.insert(surfaceID);
  _pending.emplace(layerID, std::move(state));
  return YES;
}

- (BOOL)removeLayer:(NSDictionary*)payload {
  uint32_t layerID;
  if (![self matchesFrame:payload] || !ReadUInt(payload, @"layerId", &layerID) ||
      !_layers.count(layerID) || _pending.count(layerID) || !_removed.insert(layerID).second) return NO;
  return YES;
}

- (BOOL)commitFrame:(NSDictionary*)payload {
  if (![self matchesFrame:payload]) return NO;
  size_t finalCount = _layers.size() - _removed.size();
  for (const auto& [identity, state] : _pending) {
    if (!_layers.count(identity)) ++finalCount;
  }
  if (finalCount > kMaxLayers) return NO;
  auto retired = std::make_shared<std::vector<std::shared_ptr<Surface>>>();
  [CATransaction begin];
  [CATransaction setDisableActions:YES];
  for (uint32_t identity : _removed) {
    auto& old = _layers.at(identity);
    [old.layer removeFromSuperlayer];
    if (old.surface) retired->push_back(old.surface);
    _layers.erase(identity);
  }
  for (auto& [identity, state] : _pending) {
    auto old = _layers.find(identity);
    if (old != _layers.end()) {
      [old->second.layer removeFromSuperlayer];
      if (old->second.surface) retired->push_back(old->second.surface);
    }
    if (state.raw) {
      state.layer = BuildRawLayer(state, self.bounds);
      [self.layer addSublayer:state.layer];
      _layers.insert_or_assign(identity, std::move(state));
      continue;
    }
    CALayer* layer = state.layer;
    layer.frame = state.frame;
    layer.contents = (__bridge id)state.surface->value;
    layer.contentsScale = state.scale;
    layer.opacity = state.opacity;
    layer.zPosition = state.z;
    layer.geometryFlipped = YES;
    if (state.clipped) {
      CAShapeLayer* mask = [CAShapeLayer layer];
      CGPathRef path = CGPathCreateWithRect(state.clip, nullptr);
      mask.path = path;
      CGPathRelease(path);
      layer.mask = mask;
    }
    [self.layer addSublayer:layer];
    _layers.insert_or_assign(identity, std::move(state));
  }
  uint32_t frame = _pendingFrame;
  __weak FPAShimView* weakSelf = self;
  [CATransaction setCompletionBlock:^{
    FPAShimView* view = weakSelf;
    if (!view || !view->_sink) return;
    for (const auto& surface : *retired) {
      view->_surfaceIDs.erase(surface->identity);
      view->_surfaceBytes -= surface->bytes;
      [view emit:MessageType::SurfaceReleased payload:@{@"surfaceId": @(surface->identity)}];
    }
    [view emit:MessageType::FramePresented payload:@{@"frameId": @(frame)}];
  }];
  [CATransaction commit];
  _pending.clear();
  _removed.clear();
  _committedFrameID = frame;
  _pendingFrame = 0;
  return YES;
}

- (BOOL)setEditorState:(NSDictionary*)payload {
  uint32_t revision;
  uint32_t textOffset = 0;
  NSString* text = ReadString(payload, @"text", 32768);
  NSRange selected;
  double x, y, width, height;
  if (!text || (payload[@"textOffset"] && !ReadUInt(payload, @"textOffset", &textOffset, true)) ||
      text.length > UINT32_MAX - textOffset ||
      !ReadUInt(payload, @"revision", &revision) || revision <= _editorRevision ||
      !ReadRange(payload, @"selectionStart", @"selectionLength", textOffset + text.length, &selected) ||
      selected.location < textOffset ||
      !ReadNumber(payload, @"caretX", &x, -65536, 65536) ||
      !ReadNumber(payload, @"caretY", &y, -65536, 65536) ||
      !ReadNumber(payload, @"caretWidth", &width, 0, 32768) ||
      !ReadNumber(payload, @"caretHeight", &height, 0, 32768) ||
      CFGetTypeID((__bridge CFTypeRef)(payload[@"editable"] ?: NSNull.null)) != CFBooleanGetTypeID()) return NO;
  NSRange marked = NSMakeRange(NSNotFound, 0);
  if (payload[@"markedStart"] && (!ReadRange(payload, @"markedStart", @"markedLength", textOffset + text.length, &marked) ||
      marked.location < textOffset)) return NO;
  bool discard = false;
  bool password = false;
  if (payload[@"discardMarkedText"] && !ReadBool(payload[@"discardMarkedText"], &discard)) return NO;
  if (payload[@"password"] && !ReadBool(payload[@"password"], &password)) return NO;
  _editorRevision = revision;
  _editorText = [text copy];
  _textOffset = textOffset;
  _selectedRange = selected;
  _markedRange = marked;
  _caret = NSMakeRect(x, y, width, height);
  _editable = [payload[@"editable"] boolValue];
  _password = password;
  if (discard) {
    _markedRange = NSMakeRange(NSNotFound, 0);
    [self.inputContext discardMarkedText];
  }
  [self.inputContext invalidateCharacterCoordinates];
  [self updateSecureInput];
  return YES;
}

- (void)updateSecureInput {
  BOOL desired = _sink && _editable && _password && _inputEnabled && NSApp.isActive &&
      self.window.isKeyWindow && self.window.firstResponder == self;
  if (desired && !_secureInput) {
    _secureInput = EnableSecureEventInput() == noErr;
  } else if (!desired && _secureInput) {
    DisableSecureEventInput();
    _secureInput = NO;
  }
}

- (void)secureInputChanged:(NSNotification*)notification { [self updateSecureInput]; }

- (void)dealloc {
  [NSNotificationCenter.defaultCenter removeObserver:self];
  if (_secureInput) DisableSecureEventInput();
}

- (void)disconnect {
  _cursor = NSCursor.arrowCursor;
  [self applyCursor];
  _sink = nil;
  [self updateSecureInput];
  [self.inputContext discardMarkedText];
  self.layer.sublayers = @[];
  _layers.clear();
  _pending.clear();
  _removed.clear();
  _surfaceIDs.clear();
  _surfaceBytes = 0;
  _pendingFrame = 0;
}

- (void)updateTrackingAreas {
  [super updateTrackingAreas];
  if (_tracking) [self removeTrackingArea:_tracking];
  _tracking = [[NSTrackingArea alloc] initWithRect:NSZeroRect
      options:NSTrackingMouseMoved | NSTrackingMouseEnteredAndExited | NSTrackingCursorUpdate |
              NSTrackingActiveInActiveApp | NSTrackingInVisibleRect
      owner:self userInfo:nil];
  [self addTrackingArea:_tracking];
}

- (void)mouseEvent:(NSEvent*)event kind:(NSString*)kind {
  NSPoint point = [self convertPoint:event.locationInWindow fromView:nil];
  [self emit:MessageType::Input payload:@{@"kind": kind, @"x": @(point.x), @"y": @(point.y),
      @"button": @(event.buttonNumber), @"clickCount": @(event.clickCount),
      @"modifiers": @(event.modifierFlags), @"timestamp": @(event.timestamp)}];
}
- (void)mouseDown:(NSEvent*)event { [self.window makeFirstResponder:self]; [self mouseEvent:event kind:@"mouseDown"]; }
- (void)mouseUp:(NSEvent*)event { [self mouseEvent:event kind:@"mouseUp"]; }
- (void)rightMouseDown:(NSEvent*)event { [self mouseEvent:event kind:@"mouseDown"]; }
- (void)rightMouseUp:(NSEvent*)event { [self mouseEvent:event kind:@"mouseUp"]; }
- (void)otherMouseDown:(NSEvent*)event { [self mouseEvent:event kind:@"mouseDown"]; }
- (void)otherMouseUp:(NSEvent*)event { [self mouseEvent:event kind:@"mouseUp"]; }
- (void)mouseMoved:(NSEvent*)event { [self mouseEvent:event kind:@"mouseMove"]; }
- (void)mouseDragged:(NSEvent*)event { [self mouseEvent:event kind:@"mouseMove"]; }
- (void)rightMouseDragged:(NSEvent*)event { [self mouseEvent:event kind:@"mouseMove"]; }
- (void)otherMouseDragged:(NSEvent*)event { [self mouseEvent:event kind:@"mouseMove"]; }
- (void)mouseEntered:(NSEvent*)event { [self mouseEvent:event kind:@"mouseEnter"]; }
- (void)mouseExited:(NSEvent*)event { [self mouseEvent:event kind:@"mouseExit"]; }
- (void)scrollWheel:(NSEvent*)event {
  NSPoint point = [self convertPoint:event.locationInWindow fromView:nil];
  [self emit:MessageType::Input payload:@{@"kind": @"scroll", @"x": @(point.x), @"y": @(point.y),
      @"deltaX": @(event.scrollingDeltaX), @"deltaY": @(event.scrollingDeltaY),
      @"precise": @(event.hasPreciseScrollingDeltas), @"phase": @(event.phase),
      @"momentumPhase": @(event.momentumPhase), @"modifiers": @(event.modifierFlags),
      @"timestamp": @(event.timestamp)}];
}
- (void)keyEvent:(NSEvent*)event kind:(NSString*)kind {
  [self emit:MessageType::Input payload:@{@"kind": kind, @"keyCode": @(event.keyCode),
      @"characters": event.characters ?: @"", @"charactersIgnoringModifiers": event.charactersIgnoringModifiers ?: @"",
      @"modifiers": @(event.modifierFlags), @"repeat": @(event.isARepeat),
      @"timestamp": @(event.timestamp)}];
}
- (void)keyDown:(NSEvent*)event {
  [self keyEvent:event kind:@"keyDown"];
  if (_editable) [self interpretKeyEvents:@[event]];
}
- (void)keyUp:(NSEvent*)event { [self keyEvent:event kind:@"keyUp"]; }
- (void)flagsChanged:(NSEvent*)event {
  [self emit:MessageType::Input payload:@{@"kind": @"flagsChanged", @"keyCode": @(event.keyCode),
      @"modifiers": @(event.modifierFlags), @"timestamp": @(event.timestamp)}];
}
- (BOOL)becomeFirstResponder {
  [self emit:MessageType::Input payload:@{@"kind": @"focus", @"focused": @YES}];
  dispatch_async(dispatch_get_main_queue(), ^{ [self updateSecureInput]; });
  return YES;
}
- (BOOL)resignFirstResponder {
  if (_secureInput) { DisableSecureEventInput(); _secureInput = NO; }
  [self emit:MessageType::Input payload:@{@"kind": @"focus", @"focused": @NO}];
  return YES;
}
- (void)insertText:(id)value replacementRange:(NSRange)replacement {
  NSString* text = PlainText(value);
  if (!_editable || !text || text.length > 32768) return;
  [self emit:MessageType::Input payload:@{@"kind": @"insertText", @"text": text,
      @"replacement": RangePayload(replacement), @"editorRevision": @(_editorRevision)}];
  _markedRange = NSMakeRange(NSNotFound, 0);
}
- (void)setMarkedText:(id)value selectedRange:(NSRange)selected replacementRange:(NSRange)replacement {
  NSString* text = PlainText(value);
  if (!_editable || !text || text.length > 32768 || selected.location > text.length ||
      selected.length > text.length - selected.location) return;
  [self emit:MessageType::Input payload:@{@"kind": @"setMarkedText", @"text": text,
      @"selected": RangePayload(selected), @"replacement": RangePayload(replacement),
      @"editorRevision": @(_editorRevision)}];
  _markedRange = NSMakeRange(_selectedRange.location, text.length);
}
- (void)unmarkText {
  if ([self hasMarkedText]) [self emit:MessageType::Input payload:@{@"kind": @"unmarkText", @"editorRevision": @(_editorRevision)}];
  _markedRange = NSMakeRange(NSNotFound, 0);
}
- (BOOL)hasMarkedText { return _markedRange.location != NSNotFound && _markedRange.length > 0; }
- (NSRange)markedRange { return _markedRange; }
- (NSRange)selectedRange { return _selectedRange; }
- (NSArray<NSAttributedStringKey>*)validAttributesForMarkedText { return @[]; }
- (NSAttributedString*)attributedSubstringForProposedRange:(NSRange)range actualRange:(NSRangePointer)actual {
  if (range.location == NSNotFound || range.location < _textOffset ||
      range.location - _textOffset >= _editorText.length) {
    if (actual) *actual = NSMakeRange(NSNotFound, 0);
    return nil;
  }
  range.length = std::min(range.length, _editorText.length - (range.location - _textOffset));
  if (actual) *actual = range;
  NSRange local = NSMakeRange(range.location - _textOffset, range.length);
  return [[NSAttributedString alloc] initWithString:[_editorText substringWithRange:local]];
}
- (NSRect)firstRectForCharacterRange:(NSRange)range actualRange:(NSRangePointer)actual {
  if (actual) *actual = _selectedRange;
  return [self.window convertRectToScreen:[self convertRect:_caret toView:nil]];
}
- (NSUInteger)characterIndexForPoint:(NSPoint)point { return NSNotFound; }
- (void)doCommandBySelector:(SEL)selector {
  [self emit:MessageType::Input payload:@{@"kind": @"textCommand", @"selector": NSStringFromSelector(selector),
      @"editorRevision": @(_editorRevision)}];
}

@end
