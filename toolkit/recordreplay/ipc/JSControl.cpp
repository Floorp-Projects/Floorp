/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "JSControl.h"

#include "js/CharacterEncoding.h"
#include "js/Conversions.h"
#include "js/JSON.h"
#include "ChildInternal.h"
#include "ParentInternal.h"
#include "xpcprivate.h"

using namespace JS;

namespace mozilla {
namespace recordreplay {
namespace js {

// Callback for filling CharBuffers when converting objects to JSON.
static bool
FillCharBufferCallback(const char16_t* buf, uint32_t len, void* data)
{
  CharBuffer* buffer = (CharBuffer*) data;
  MOZ_RELEASE_ASSERT(buffer->length() == 0);
  buffer->append(buf, len);
  return true;
}

static JSObject*
NonNullObject(JSContext* aCx, HandleValue aValue)
{
  if (!aValue.isObject()) {
    JS_ReportErrorASCII(aCx, "Expected object");
    return nullptr;
  }
  return &aValue.toObject();
}

template <typename T>
static bool
MaybeGetNumberProperty(JSContext* aCx, HandleObject aObject, const char* aProperty, T* aResult)
{
  RootedValue v(aCx);
  if (!JS_GetProperty(aCx, aObject, aProperty, &v)) {
    return false;
  }
  if (v.isNumber()) {
    *aResult = v.toNumber();
  }
  return true;
}

template <typename T>
static bool
GetNumberProperty(JSContext* aCx, HandleObject aObject, const char* aProperty, T* aResult)
{
  RootedValue v(aCx);
  if (!JS_GetProperty(aCx, aObject, aProperty, &v)) {
    return false;
  }
  if (!v.isNumber()) {
    JS_ReportErrorASCII(aCx, "Object missing required property");
    return false;
  }
  *aResult = v.toNumber();
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// BreakpointPosition Conversion
///////////////////////////////////////////////////////////////////////////////

// Names of properties which JS code uses to specify the contents of a BreakpointPosition.
static const char gKindProperty[] = "kind";
static const char gScriptProperty[] = "script";
static const char gOffsetProperty[] = "offset";
static const char gFrameIndexProperty[] = "frameIndex";

JSObject*
BreakpointPosition::Encode(JSContext* aCx) const
{
  RootedString kindString(aCx, JS_NewStringCopyZ(aCx, KindString()));
  RootedObject obj(aCx, JS_NewObject(aCx, nullptr));
  if (!kindString || !obj ||
      !JS_DefineProperty(aCx, obj, gKindProperty, kindString, JSPROP_ENUMERATE) ||
      (mScript != BreakpointPosition::EMPTY_SCRIPT &&
       !JS_DefineProperty(aCx, obj, gScriptProperty, mScript, JSPROP_ENUMERATE)) ||
      (mOffset != BreakpointPosition::EMPTY_OFFSET &&
       !JS_DefineProperty(aCx, obj, gOffsetProperty, mOffset, JSPROP_ENUMERATE)) ||
      (mFrameIndex != BreakpointPosition::EMPTY_FRAME_INDEX &&
       !JS_DefineProperty(aCx, obj, gFrameIndexProperty, mFrameIndex, JSPROP_ENUMERATE)))
  {
    return nullptr;
  }
  return obj;
}

bool
BreakpointPosition::Decode(JSContext* aCx, HandleObject aObject)
{
  RootedValue v(aCx);
  if (!JS_GetProperty(aCx, aObject, gKindProperty, &v)) {
    return false;
  }

  RootedString str(aCx, ToString(aCx, v));
  for (size_t i = BreakpointPosition::Invalid + 1; i < BreakpointPosition::sKindCount; i++) {
    BreakpointPosition::Kind kind = (BreakpointPosition::Kind) i;
    bool match;
    if (!JS_StringEqualsAscii(aCx, str, BreakpointPosition::StaticKindString(kind), &match))
      return false;
    if (match) {
      mKind = kind;
      break;
    }
  }
  if (mKind == BreakpointPosition::Invalid) {
    JS_ReportErrorASCII(aCx, "Could not decode breakpoint position kind");
    return false;
  }

  if (!MaybeGetNumberProperty(aCx, aObject, gScriptProperty, &mScript) ||
      !MaybeGetNumberProperty(aCx, aObject, gOffsetProperty, &mOffset) ||
      !MaybeGetNumberProperty(aCx, aObject, gFrameIndexProperty, &mFrameIndex))
  {
    return false;
  }

  return true;
}

///////////////////////////////////////////////////////////////////////////////
// ExecutionPoint Conversion
///////////////////////////////////////////////////////////////////////////////

// Names of properties which JS code uses to specify the contents of an ExecutionPoint.
static const char gCheckpointProperty[] = "checkpoint";
static const char gProgressProperty[] = "progress";
static const char gPositionProperty[] = "position";

JSObject*
ExecutionPoint::Encode(JSContext* aCx) const
{
  RootedObject obj(aCx, JS_NewObject(aCx, nullptr));
  RootedObject position(aCx, mPosition.Encode(aCx));
  if (!obj || !position ||
      !JS_DefineProperty(aCx, obj, gCheckpointProperty,
                         (double) mCheckpoint, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(aCx, obj, gProgressProperty,
                         (double) mProgress, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(aCx, obj, gPositionProperty, position, JSPROP_ENUMERATE))
  {
    return nullptr;
  }
  return obj;
}

bool
ExecutionPoint::Decode(JSContext* aCx, HandleObject aObject)
{
  RootedValue v(aCx);
  if (!JS_GetProperty(aCx, aObject, gPositionProperty, &v)) {
    return false;
  }

  RootedObject positionObject(aCx, NonNullObject(aCx, v));
  return positionObject
      && mPosition.Decode(aCx, positionObject)
      && GetNumberProperty(aCx, aObject, gCheckpointProperty, &mCheckpoint)
      && GetNumberProperty(aCx, aObject, gProgressProperty, &mProgress);
}

///////////////////////////////////////////////////////////////////////////////
// Middleman Methods
///////////////////////////////////////////////////////////////////////////////

// Keep track of all replay debuggers in existence, so that they can all be
// invalidated when the process is unpaused.
static StaticInfallibleVector<PersistentRootedObject*> gReplayDebuggers;

static bool
Middleman_RegisterReplayDebugger(JSContext* aCx, unsigned aArgc, Value* aVp)
{
  CallArgs args = CallArgsFromVp(aArgc, aVp);
  RootedObject obj(aCx, NonNullObject(aCx, args.get(0)));
  if (!obj) {
    return false;
  }

  obj = ::js::CheckedUnwrap(obj);
  if (!obj) {
    ::js::ReportAccessDenied(aCx);
    return false;
  }

  PersistentRootedObject* root = new PersistentRootedObject(aCx);
  *root = obj;
  gReplayDebuggers.append(root);

  args.rval().setUndefined();
  return true;
}

static bool
InvalidateReplayDebuggersAfterUnpause(JSContext* aCx)
{
  RootedValue rval(aCx);
  for (auto root : gReplayDebuggers) {
    JSAutoRealm ar(aCx, *root);
    if (!JS_CallFunctionName(aCx, *root, "invalidateAfterUnpause",
                             HandleValueArray::empty(), &rval))
    {
      return false;
    }
  }
  return true;
}

static bool
Middleman_CanRewind(JSContext* aCx, unsigned aArgc, Value* aVp)
{
  CallArgs args = CallArgsFromVp(aArgc, aVp);
  args.rval().setBoolean(parent::CanRewind());
  return true;
}

static bool
Middleman_Resume(JSContext* aCx, unsigned aArgc, Value* aVp)
{
  CallArgs args = CallArgsFromVp(aArgc, aVp);
  bool forward = ToBoolean(args.get(0));

  if (!InvalidateReplayDebuggersAfterUnpause(aCx)) {
    return false;
  }

  parent::Resume(forward);

  args.rval().setUndefined();
  return true;
}

static bool
Middleman_TimeWarp(JSContext* aCx, unsigned aArgc, Value* aVp)
{
  CallArgs args = CallArgsFromVp(aArgc, aVp);
  RootedObject targetObject(aCx, NonNullObject(aCx, args.get(0)));
  if (!targetObject) {
    return false;
  }

  ExecutionPoint target;
  if (!target.Decode(aCx, targetObject)) {
    return false;
  }

  if (!InvalidateReplayDebuggersAfterUnpause(aCx)) {
    return false;
  }

  parent::TimeWarp(target);

  args.rval().setUndefined();
  return true;
}

static bool
Middleman_Pause(JSContext* aCx, unsigned aArgc, Value* aVp)
{
  CallArgs args = CallArgsFromVp(aArgc, aVp);

  parent::Pause();

  args.rval().setUndefined();
  return true;
}

static bool
Middleman_SendRequest(JSContext* aCx, unsigned aArgc, Value* aVp)
{
  CallArgs args = CallArgsFromVp(aArgc, aVp);
  RootedObject requestObject(aCx, NonNullObject(aCx, args.get(0)));
  if (!requestObject) {
    return false;
  }

  CharBuffer requestBuffer;
  if (!ToJSONMaybeSafely(aCx, requestObject, FillCharBufferCallback, &requestBuffer)) {
    return false;
  }

  CharBuffer responseBuffer;
  parent::SendRequest(requestBuffer, &responseBuffer);

  return JS_ParseJSON(aCx, responseBuffer.begin(), responseBuffer.length(), args.rval());
}

struct InstalledBreakpoint
{
  PersistentRootedObject mHandler;
  BreakpointPosition mPosition;

  InstalledBreakpoint(JSContext* aCx, JSObject* aHandler, const BreakpointPosition& aPosition)
    : mHandler(aCx, aHandler), mPosition(aPosition)
  {}
};
static StaticInfallibleVector<InstalledBreakpoint*> gBreakpoints;

static bool
Middleman_SetBreakpoint(JSContext* aCx, unsigned aArgc, Value* aVp)
{
  CallArgs args = CallArgsFromVp(aArgc, aVp);

  RootedObject handler(aCx, NonNullObject(aCx, args.get(0)));
  RootedObject positionObject(aCx, NonNullObject(aCx, args.get(1)));
  if (!handler || !positionObject) {
    return false;
  }

  handler = ::js::CheckedUnwrap(handler);
  if (!handler) {
    ::js::ReportAccessDenied(aCx);
    return false;
  }

  BreakpointPosition position;
  if (!position.Decode(aCx, positionObject)) {
    return false;
  }

  size_t breakpointId;
  for (breakpointId = 0; breakpointId < gBreakpoints.length(); breakpointId++) {
    if (!gBreakpoints[breakpointId]) {
      break;
    }
  }
  if (breakpointId == gBreakpoints.length()) {
    gBreakpoints.append(nullptr);
  }

  gBreakpoints[breakpointId] = new InstalledBreakpoint(aCx, handler, position);

  parent::SetBreakpoint(breakpointId, position);

  args.rval().setInt32(breakpointId);
  return true;
}

bool
HitBreakpoint(JSContext* aCx, size_t aId)
{
  InstalledBreakpoint* breakpoint = gBreakpoints[aId];
  MOZ_RELEASE_ASSERT(breakpoint);

  JSAutoRealm ar(aCx, breakpoint->mHandler);

  RootedValue handlerValue(aCx, ObjectValue(*breakpoint->mHandler));
  RootedValue rval(aCx);
  return JS_CallFunctionValue(aCx, nullptr, handlerValue,
                              HandleValueArray::empty(), &rval)
      // The replaying process will resume after this hook returns, if it
      // hasn't already been explicitly resumed.
      && InvalidateReplayDebuggersAfterUnpause(aCx);
}

/* static */ bool
Middleman_ClearBreakpoint(JSContext* aCx, unsigned aArgc, Value* aVp)
{
  CallArgs args = CallArgsFromVp(aArgc, aVp);
  if (!args.get(0).isNumber()) {
    JS_ReportErrorASCII(aCx, "Bad breakpoint ID");
    return false;
  }

  size_t breakpointId = (size_t) args.get(0).toNumber();
  if (breakpointId >= gBreakpoints.length() || !gBreakpoints[breakpointId]) {
    JS_ReportErrorASCII(aCx, "Bad breakpoint ID");
    return false;
  }

  delete gBreakpoints[breakpointId];
  gBreakpoints[breakpointId] = nullptr;

  parent::SetBreakpoint(breakpointId, BreakpointPosition());

  args.rval().setUndefined();
  return true;
}

static bool
Middleman_MaybeSwitchToReplayingChild(JSContext* aCx, unsigned aArgc, Value* aVp)
{
  CallArgs args = CallArgsFromVp(aArgc, aVp);

  parent::MaybeSwitchToReplayingChild();

  args.rval().setUndefined();
  return true;
}

static bool
Middleman_HadRepaint(JSContext* aCx, unsigned aArgc, Value* aVp)
{
  CallArgs args = CallArgsFromVp(aArgc, aVp);

  if (!args.get(0).isNumber() || !args.get(1).isNumber()) {
    JS_ReportErrorASCII(aCx, "Bad width/height");
    return false;
  }

  size_t width = args.get(0).toNumber();
  size_t height = args.get(1).toNumber();

  PaintMessage message(CheckpointId::Invalid, width, height);
  parent::UpdateGraphicsInUIProcess(&message);

  args.rval().setUndefined();
  return true;
}

static bool
Middleman_HadRepaintFailure(JSContext* aCx, unsigned aArgc, Value* aVp)
{
  CallArgs args = CallArgsFromVp(aArgc, aVp);

  parent::UpdateGraphicsInUIProcess(nullptr);

  args.rval().setUndefined();
  return true;
}

static bool
Middleman_ChildIsRecording(JSContext* aCx, unsigned aArgc, Value* aVp)
{
  CallArgs args = CallArgsFromVp(aArgc, aVp);
  args.rval().setBoolean(parent::ActiveChildIsRecording());
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// Devtools Sandbox
///////////////////////////////////////////////////////////////////////////////

static PersistentRootedObject* gDevtoolsSandbox;

// URL of the root script that runs when recording/replaying.
#define ReplayScriptURL "resource://devtools/server/actors/replay/replay.js"

void
SetupDevtoolsSandbox()
{
  MOZ_RELEASE_ASSERT(!gDevtoolsSandbox);

  dom::AutoJSAPI jsapi;
  if (!jsapi.Init(xpc::PrivilegedJunkScope())) {
    MOZ_CRASH("SetupDevtoolsSandbox");
  }

  JSContext* cx = jsapi.cx();

  xpc::SandboxOptions options;
  options.sandboxName.AssignLiteral("Record/Replay Devtools Sandbox");
  options.invisibleToDebugger = true;
  RootedValue v(cx);
  nsresult rv = CreateSandboxObject(cx, &v, nsXPConnect::SystemPrincipal(), options);
  MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));

  gDevtoolsSandbox = new PersistentRootedObject(cx);
  *gDevtoolsSandbox = ::js::UncheckedUnwrap(&v.toObject());

  JSAutoRealm ar(cx, *gDevtoolsSandbox);

  ErrorResult er;
  dom::GlobalObject global(cx, *gDevtoolsSandbox);
  RootedObject obj(cx);
  dom::ChromeUtils::Import(global, NS_LITERAL_STRING(ReplayScriptURL),
                           dom::Optional<HandleObject>(), &obj, er);
  MOZ_RELEASE_ASSERT(!er.Failed());
}

extern "C" {

MOZ_EXPORT bool
RecordReplayInterface_IsInternalScript(const char* aURL)
{
  return !strcmp(aURL, ReplayScriptURL);
}

} // extern "C"

#undef ReplayScriptURL

void
ProcessRequest(const char16_t* aRequest, size_t aRequestLength, CharBuffer* aResponse)
{
  AutoDisallowThreadEvents disallow;
  AutoSafeJSContext cx;
  JSAutoRealm ar(cx, *gDevtoolsSandbox);

  RootedValue requestValue(cx);
  if (!JS_ParseJSON(cx, aRequest, aRequestLength, &requestValue)) {
    MOZ_CRASH("ProcessRequest: ParseJSON failed");
  }

  RootedValue responseValue(cx);
  if (!JS_CallFunctionName(cx, *gDevtoolsSandbox, "ProcessRequest",
                           HandleValueArray(requestValue), &responseValue)) {
    MOZ_CRASH("ProcessRequest: Handler failed");
  }

  // Processing the request may have called into MaybeDivergeFromRecording.
  // Now that we've finished processing it, don't tolerate future events that
  // would otherwise cause us to rewind to the last checkpoint.
  DisallowUnhandledDivergeFromRecording();

  if (!responseValue.isObject()) {
    MOZ_CRASH("ProcessRequest: Response must be an object");
  }

  RootedObject responseObject(cx, &responseValue.toObject());
  if (!ToJSONMaybeSafely(cx, responseObject, FillCharBufferCallback, aResponse)) {
    MOZ_CRASH("ProcessRequest: ToJSONMaybeSafely failed");
  }
}

void
EnsurePositionHandler(const BreakpointPosition& aPosition)
{
  AutoDisallowThreadEvents disallow;
  AutoSafeJSContext cx;
  JSAutoRealm ar(cx, *gDevtoolsSandbox);

  RootedObject obj(cx, aPosition.Encode(cx));
  if (!obj) {
    MOZ_CRASH("EnsurePositionHandler");
  }

  RootedValue objValue(cx, ObjectValue(*obj));
  RootedValue rval(cx);
  if (!JS_CallFunctionName(cx, *gDevtoolsSandbox, "EnsurePositionHandler",
                           HandleValueArray(objValue), &rval)) {
    MOZ_CRASH("EnsurePositionHandler");
  }
}

void
ClearPositionHandlers()
{
  AutoDisallowThreadEvents disallow;
  AutoSafeJSContext cx;
  JSAutoRealm ar(cx, *gDevtoolsSandbox);

  RootedValue rval(cx);
  if (!JS_CallFunctionName(cx, *gDevtoolsSandbox, "ClearPositionHandlers",
                           HandleValueArray::empty(), &rval)) {
    MOZ_CRASH("ClearPositionHandlers");
  }
}

void
ClearPausedState()
{
  AutoDisallowThreadEvents disallow;
  AutoSafeJSContext cx;
  JSAutoRealm ar(cx, *gDevtoolsSandbox);

  RootedValue rval(cx);
  if (!JS_CallFunctionName(cx, *gDevtoolsSandbox, "ClearPausedState",
                           HandleValueArray::empty(), &rval)) {
    MOZ_CRASH("ClearPausedState");
  }
}

Maybe<BreakpointPosition>
GetEntryPosition(const BreakpointPosition& aPosition)
{
  AutoDisallowThreadEvents disallow;
  AutoSafeJSContext cx;
  JSAutoRealm ar(cx, *gDevtoolsSandbox);

  RootedObject positionObject(cx, aPosition.Encode(cx));
  if (!positionObject) {
    MOZ_CRASH("GetEntryPosition");
  }

  RootedValue rval(cx);
  RootedValue positionValue(cx, ObjectValue(*positionObject));
  if (!JS_CallFunctionName(cx, *gDevtoolsSandbox, "GetEntryPosition",
                           HandleValueArray(positionValue), &rval)) {
    MOZ_CRASH("GetEntryPosition");
  }

  if (!rval.isObject()) {
    return Nothing();
  }

  RootedObject rvalObject(cx, &rval.toObject());
  BreakpointPosition entryPosition;
  if (!entryPosition.Decode(cx, rvalObject)) {
    MOZ_CRASH("GetEntryPosition");
  }

  return Some(entryPosition);
}

///////////////////////////////////////////////////////////////////////////////
// Replaying process content
///////////////////////////////////////////////////////////////////////////////

struct ContentInfo
{
  const void* mToken;
  char* mURL;
  char* mContentType;
  InfallibleVector<char> mContent8;
  InfallibleVector<char16_t> mContent16;

  ContentInfo(const void* aToken, const char* aURL, const char* aContentType)
    : mToken(aToken),
      mURL(strdup(aURL)),
      mContentType(strdup(aContentType))
  {}

  ContentInfo(ContentInfo&& aOther)
    : mToken(aOther.mToken),
      mURL(aOther.mURL),
      mContentType(aOther.mContentType),
      mContent8(std::move(aOther.mContent8)),
      mContent16(std::move(aOther.mContent16))
  {
    aOther.mURL = nullptr;
    aOther.mContentType = nullptr;
  }

  ~ContentInfo()
  {
    free(mURL);
    free(mContentType);
  }
};

// All content that has been parsed so far. Protected by child::gMonitor.
static StaticInfallibleVector<ContentInfo> gContent;

extern "C" {

MOZ_EXPORT void
RecordReplayInterface_BeginContentParse(const void* aToken,
                                        const char* aURL, const char* aContentType)
{
  MOZ_RELEASE_ASSERT(IsRecordingOrReplaying());
  MOZ_RELEASE_ASSERT(aToken);

  RecordReplayAssert("BeginContentParse %s", aURL);

  MonitorAutoLock lock(*child::gMonitor);
  for (ContentInfo& info : gContent) {
    MOZ_RELEASE_ASSERT(info.mToken != aToken);
  }
  gContent.emplaceBack(aToken, aURL, aContentType);
}

MOZ_EXPORT void
RecordReplayInterface_AddContentParseData8(const void* aToken,
                                           const Utf8Unit* aUtf8Buffer, size_t aLength)
{
  MOZ_RELEASE_ASSERT(IsRecordingOrReplaying());
  MOZ_RELEASE_ASSERT(aToken);

  RecordReplayAssert("AddContentParseData8ForRecordReplay %d", (int) aLength);

  MonitorAutoLock lock(*child::gMonitor);
  for (ContentInfo& info : gContent) {
    if (info.mToken == aToken) {
      info.mContent8.append(reinterpret_cast<const char*>(aUtf8Buffer), aLength);
      return;
    }
  }
  MOZ_CRASH("Unknown content parse token");
}

MOZ_EXPORT void
RecordReplayInterface_AddContentParseData16(const void* aToken,
                                            const char16_t* aBuffer, size_t aLength)
{
  MOZ_RELEASE_ASSERT(IsRecordingOrReplaying());
  MOZ_RELEASE_ASSERT(aToken);

  RecordReplayAssert("AddContentParseData16ForRecordReplay %d", (int) aLength);

  MonitorAutoLock lock(*child::gMonitor);
  for (ContentInfo& info : gContent) {
    if (info.mToken == aToken) {
      info.mContent16.append(aBuffer, aLength);
      return;
    }
  }
  MOZ_CRASH("Unknown content parse token");
}

MOZ_EXPORT void
RecordReplayInterface_EndContentParse(const void* aToken)
{
  MOZ_RELEASE_ASSERT(IsRecordingOrReplaying());
  MOZ_RELEASE_ASSERT(aToken);

  MonitorAutoLock lock(*child::gMonitor);
  for (ContentInfo& info : gContent) {
    if (info.mToken == aToken) {
      info.mToken = nullptr;
      return;
    }
  }
  MOZ_CRASH("Unknown content parse token");
}

} // extern "C"

static bool
FetchContent(JSContext* aCx, HandleString aURL,
             MutableHandleString aContentType, MutableHandleString aContent)
{
  MonitorAutoLock lock(*child::gMonitor);
  for (ContentInfo& info : gContent) {
    if (JS_FlatStringEqualsAscii(JS_ASSERT_STRING_IS_FLAT(aURL), info.mURL)) {
      aContentType.set(JS_NewStringCopyZ(aCx, info.mContentType));
      if (!aContentType) {
        return false;
      }

      MOZ_ASSERT(info.mContent8.length() == 0 ||
                 info.mContent16.length() == 0,
                 "should have content data of only one type");

      aContent.set(info.mContent8.length() > 0
                   ? JS_NewStringCopyUTF8N(aCx, JS::UTF8Chars(info.mContent8.begin(),
                                                              info.mContent8.length()))
                   : JS_NewUCStringCopyN(aCx, info.mContent16.begin(), info.mContent16.length()));
      return aContent != nullptr;
    }
  }
  aContentType.set(JS_NewStringCopyZ(aCx, "text/plain"));
  aContent.set(JS_NewStringCopyZ(aCx, "Could not find record/replay content"));
  return aContentType && aContent;
}

///////////////////////////////////////////////////////////////////////////////
// Recording/Replaying Methods
///////////////////////////////////////////////////////////////////////////////

static bool
RecordReplay_AreThreadEventsDisallowed(JSContext* aCx, unsigned aArgc, Value* aVp)
{
  CallArgs args = CallArgsFromVp(aArgc, aVp);
  args.rval().setBoolean(AreThreadEventsDisallowed());
  return true;
}

static bool
RecordReplay_MaybeDivergeFromRecording(JSContext* aCx, unsigned aArgc, Value* aVp)
{
  CallArgs args = CallArgsFromVp(aArgc, aVp);
  args.rval().setBoolean(navigation::MaybeDivergeFromRecording());
  return true;
}

static bool
RecordReplay_AdvanceProgressCounter(JSContext* aCx, unsigned aArgc, Value* aVp)
{
  CallArgs args = CallArgsFromVp(aArgc, aVp);
  AdvanceExecutionProgressCounter();
  args.rval().setUndefined();
  return true;
}

static bool
RecordReplay_PositionHit(JSContext* aCx, unsigned aArgc, Value* aVp)
{
  CallArgs args = CallArgsFromVp(aArgc, aVp);
  RootedObject obj(aCx, NonNullObject(aCx, args.get(0)));
  if (!obj) {
    return false;
  }

  BreakpointPosition position;
  if (!position.Decode(aCx, obj)) {
    return false;
  }

  navigation::PositionHit(position);

  args.rval().setUndefined();
  return true;
}

static bool
RecordReplay_GetContent(JSContext* aCx, unsigned aArgc, Value* aVp)
{
  CallArgs args = CallArgsFromVp(aArgc, aVp);
  RootedString url(aCx, ToString(aCx, args.get(0)));

  RootedString contentType(aCx), content(aCx);
  if (!FetchContent(aCx, url, &contentType, &content)) {
    return false;
  }

  RootedObject obj(aCx, JS_NewObject(aCx, nullptr));
  if (!obj ||
      !JS_DefineProperty(aCx, obj, "contentType", contentType, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(aCx, obj, "content", content, JSPROP_ENUMERATE))
  {
    return false;
  }

  args.rval().setObject(*obj);
  return true;
}

static bool
RecordReplay_CurrentExecutionPoint(JSContext* aCx, unsigned aArgc, Value* aVp)
{
  CallArgs args = CallArgsFromVp(aArgc, aVp);

  Maybe<BreakpointPosition> position;
  if (!args.get(0).isUndefined()) {
    RootedObject obj(aCx, NonNullObject(aCx, args.get(0)));
    if (!obj) {
      return false;
    }

    position.emplace();
    if (!position.ref().Decode(aCx, obj)) {
      return false;
    }
  }

  ExecutionPoint point = navigation::CurrentExecutionPoint(position);
  RootedObject result(aCx, point.Encode(aCx));
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

static bool
RecordReplay_TimeWarpTargetExecutionPoint(JSContext* aCx, unsigned aArgc, Value* aVp)
{
  CallArgs args = CallArgsFromVp(aArgc, aVp);
  double timeWarpTarget;
  if (!ToNumber(aCx, args.get(0), &timeWarpTarget)) {
    return false;
  }

  ExecutionPoint point = navigation::TimeWarpTargetExecutionPoint((ProgressCounter) timeWarpTarget);
  RootedObject result(aCx, point.Encode(aCx));
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

static bool
RecordReplay_RecordingEndpoint(JSContext* aCx, unsigned aArgc, Value* aVp)
{
  CallArgs args = CallArgsFromVp(aArgc, aVp);

  ExecutionPoint point = navigation::GetRecordingEndpoint();
  RootedObject result(aCx, point.Encode(aCx));
  if (!result) {
    return false;
  }

  args.rval().setObject(*result);
  return true;
}

static bool
RecordReplay_Repaint(JSContext* aCx, unsigned aArgc, Value* aVp)
{
  CallArgs args = CallArgsFromVp(aArgc, aVp);

  size_t width, height;
  child::Repaint(&width, &height);

  RootedObject obj(aCx, JS_NewObject(aCx, nullptr));
  if (!obj ||
      !JS_DefineProperty(aCx, obj, "width", (double) width, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(aCx, obj, "height", (double) height, JSPROP_ENUMERATE))
  {
    return false;
  }

  args.rval().setObject(*obj);
  return true;
}

static bool
RecordReplay_Dump(JSContext* aCx, unsigned aArgc, Value* aVp)
{
  // This method is an alternative to dump() that can be used in places where
  // thread events are disallowed.
  CallArgs args = CallArgsFromVp(aArgc, aVp);
  for (size_t i = 0; i < args.length(); i++) {
    RootedString str(aCx, ToString(aCx, args[i]));
    if (!str) {
      return false;
    }
    JS::UniqueChars cstr = JS_EncodeStringToLatin1(aCx, str);
    if (!cstr) {
      return false;
    }
    Print("%s", cstr.get());
  }

  args.rval().setUndefined();
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// Plumbing
///////////////////////////////////////////////////////////////////////////////

static const JSFunctionSpec gMiddlemanMethods[] = {
  JS_FN("registerReplayDebugger", Middleman_RegisterReplayDebugger, 1, 0),
  JS_FN("canRewind", Middleman_CanRewind, 0, 0),
  JS_FN("resume", Middleman_Resume, 1, 0),
  JS_FN("timeWarp", Middleman_TimeWarp, 1, 0),
  JS_FN("pause", Middleman_Pause, 0, 0),
  JS_FN("sendRequest", Middleman_SendRequest, 1, 0),
  JS_FN("setBreakpoint", Middleman_SetBreakpoint, 2, 0),
  JS_FN("clearBreakpoint", Middleman_ClearBreakpoint, 1, 0),
  JS_FN("maybeSwitchToReplayingChild", Middleman_MaybeSwitchToReplayingChild, 0, 0),
  JS_FN("hadRepaint", Middleman_HadRepaint, 2, 0),
  JS_FN("hadRepaintFailure", Middleman_HadRepaintFailure, 0, 0),
  JS_FN("childIsRecording", Middleman_ChildIsRecording, 0, 0),
  JS_FS_END
};

static const JSFunctionSpec gRecordReplayMethods[] = {
  JS_FN("areThreadEventsDisallowed", RecordReplay_AreThreadEventsDisallowed, 0, 0),
  JS_FN("maybeDivergeFromRecording", RecordReplay_MaybeDivergeFromRecording, 0, 0),
  JS_FN("advanceProgressCounter", RecordReplay_AdvanceProgressCounter, 0, 0),
  JS_FN("positionHit", RecordReplay_PositionHit, 1, 0),
  JS_FN("getContent", RecordReplay_GetContent, 1, 0),
  JS_FN("currentExecutionPoint", RecordReplay_CurrentExecutionPoint, 1, 0),
  JS_FN("timeWarpTargetExecutionPoint", RecordReplay_TimeWarpTargetExecutionPoint, 1, 0),
  JS_FN("recordingEndpoint", RecordReplay_RecordingEndpoint, 0, 0),
  JS_FN("repaint", RecordReplay_Repaint, 0, 0),
  JS_FN("dump", RecordReplay_Dump, 1, 0),
  JS_FS_END
};

extern "C" {

MOZ_EXPORT bool
RecordReplayInterface_DefineRecordReplayControlObject(JSContext* aCx, JSObject* aObjectArg)
{
  RootedObject object(aCx, aObjectArg);

  RootedObject staticObject(aCx, JS_NewObject(aCx, nullptr));
  if (!staticObject || !JS_DefineProperty(aCx, object, "RecordReplayControl", staticObject, 0)) {
    return false;
  }

  // FIXME Bug 1475901 Define this interface via WebIDL instead of raw JSAPI.
  if (IsMiddleman()) {
    if (!JS_DefineFunctions(aCx, staticObject, gMiddlemanMethods)) {
      return false;
    }
  } else if (IsRecordingOrReplaying()) {
    if (!JS_DefineFunctions(aCx, staticObject, gRecordReplayMethods)) {
      return false;
    }
  } else {
    // Leave RecordReplayControl as an empty object. We still define the object
    // to avoid reference errors in scripts that run in normal processes.
  }

  return true;
}

} // extern "C"

} // namespace js
} // namespace recordreplay
} // namespace mozilla
