// SPDX-License-Identifier: MPL-2.0

#include "Protocol.h"

#include <cmath>

namespace floorp::shim {

bool IsKnownMessage(uint32_t type) {
  switch (static_cast<MessageType>(type)) {
    case MessageType::Hello:
    case MessageType::HelloAccepted:
    case MessageType::CreateWindow:
    case MessageType::CloseWindow:
    case MessageType::SetWindowTitle:
    case MessageType::ActivateWindow:
    case MessageType::ConfigureWindow:
    case MessageType::BeginFrame:
    case MessageType::SetLayer:
    case MessageType::RemoveLayer:
    case MessageType::CommitFrame:
    case MessageType::EditorState:
    case MessageType::SetCursor:
    case MessageType::Terminate:
    case MessageType::CancelQuit:
    case MessageType::Input:
    case MessageType::WindowChanged:
    case MessageType::QuitRequested:
    case MessageType::ReopenRequested:
    case MessageType::MenuCommand:
    case MessageType::SurfaceReleased:
    case MessageType::FramePresented:
      return true;
  }
  return false;
}

bool ReadNumber(NSDictionary* object, NSString* key, double* value,
                double minimum, double maximum) {
  id candidate = object[key];
  if (![candidate isKindOfClass:NSNumber.class] ||
      CFGetTypeID((__bridge CFTypeRef)candidate) == CFBooleanGetTypeID()) {
    return false;
  }
  double result = [candidate doubleValue];
  if (!std::isfinite(result) || result < minimum || result > maximum) {
    return false;
  }
  *value = result;
  return true;
}

bool ReadUInt(NSDictionary* object, NSString* key, uint32_t* value,
              bool allowZero) {
  double candidate = 0;
  if (!ReadNumber(object, key, &candidate, allowZero ? 0 : 1, UINT32_MAX) ||
      std::floor(candidate) != candidate) {
    return false;
  }
  *value = static_cast<uint32_t>(candidate);
  return true;
}

NSString* ReadString(NSDictionary* object, NSString* key, NSUInteger maximum) {
  id candidate = object[key];
  if (![candidate isKindOfClass:NSString.class] ||
      [candidate length] > maximum || [candidate rangeOfCharacterFromSet:
          [NSCharacterSet characterSetWithRange:NSMakeRange(0, 1)]].location != NSNotFound) {
    return nil;
  }
  return candidate;
}

bool ValidIdentity(NSString* value) {
  if (!value.length || value.length > 128) return false;
  NSCharacterSet* allowed = [NSCharacterSet characterSetWithCharactersInString:
      @"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-{}"];
  return [value rangeOfCharacterFromSet:allowed.invertedSet].location == NSNotFound;
}

NSDictionary* DecodePayload(NSData* bytes) {
  if (!bytes.length || bytes.length > kMaxPayloadBytes) return nil;
  id result = [NSJSONSerialization JSONObjectWithData:bytes options:0 error:nil];
  return [result isKindOfClass:NSDictionary.class] ? result : nil;
}

NSData* EncodePayload(NSDictionary* object) {
  if (![NSJSONSerialization isValidJSONObject:object]) return nil;
  NSData* data = [NSJSONSerialization dataWithJSONObject:object options:0 error:nil];
  return data.length && data.length <= kMaxPayloadBytes ? data : nil;
}

}  // namespace floorp::shim
