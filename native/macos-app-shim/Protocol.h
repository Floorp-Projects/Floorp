// SPDX-License-Identifier: MPL-2.0

#pragma once

#import <Foundation/Foundation.h>
#include <cstdint>

namespace floorp::shim {

constexpr uint32_t kProtocolMagic = 0x46415348;
constexpr uint16_t kProtocolMajor = 1;
constexpr uint16_t kProtocolMinor = 0;
constexpr size_t kMaxPayloadBytes = 64 * 1024;
// Forwarded text is limited by its encoded size, not by UTF-16 units: the JSON
// wrapper and multi-byte characters must still fit inside kMaxPayloadBytes.
constexpr size_t kMaxTextPayloadBytes = 32 * 1024;
constexpr size_t kMaxWindows = 16;
constexpr size_t kMaxLayers = 128;
constexpr size_t kMaxSurfaceBytes = 256 * 1024 * 1024;
constexpr size_t kMaxWindowSurfaceBytes = 512 * 1024 * 1024;

enum class MessageType : uint32_t {
  Hello = 1,
  HelloAccepted = 2,
  CreateWindow = 10,
  CloseWindow = 11,
  SetWindowTitle = 12,
  ActivateWindow = 13,
  ConfigureWindow = 14,
  BeginFrame = 20,
  SetLayer = 21,
  RemoveLayer = 22,
  CommitFrame = 23,
  EditorState = 30,
  SetCursor = 31,
  Terminate = 40,
  CancelQuit = 41,
  Input = 100,
  WindowChanged = 101,
  QuitRequested = 102,
  ReopenRequested = 103,
  MenuCommand = 104,
  SurfaceReleased = 110,
  FramePresented = 111,
};

struct WireHeader {
  uint32_t magic = kProtocolMagic;
  uint16_t major = kProtocolMajor;
  uint16_t minor = kProtocolMinor;
  uint32_t type = 0;
  uint32_t payloadBytes = 0;
  uint64_t sequence = 0;
  uint32_t reserved = 0;
  uint32_t reserved2 = 0;
};
static_assert(sizeof(WireHeader) == 32);

bool IsKnownMessage(uint32_t type);
bool ReadUInt(NSDictionary* object, NSString* key, uint32_t* value,
              bool allowZero = false);
bool ReadNumber(NSDictionary* object, NSString* key, double* value,
                double minimum, double maximum);
NSString* ReadString(NSDictionary* object, NSString* key, NSUInteger maximum);
bool ValidIdentity(NSString* value);
NSDictionary* DecodePayload(NSData* bytes);
NSData* EncodePayload(NSDictionary* object);

}  // namespace floorp::shim
