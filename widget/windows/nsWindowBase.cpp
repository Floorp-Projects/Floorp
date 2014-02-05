/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWindowBase.h"

#include "mozilla/MiscEvents.h"
#include "nsGkAtoms.h"
#include "WinUtils.h"
#include "npapi.h"

using namespace mozilla;
using namespace mozilla::widget;

static const wchar_t kUser32LibName[] =  L"user32.dll";
bool nsWindowBase::sTouchInjectInitialized = false;
InjectTouchInputPtr nsWindowBase::sInjectTouchFuncPtr;

bool
nsWindowBase::DispatchPluginEvent(const MSG& aMsg)
{
  if (!PluginHasFocus()) {
    return false;
  }
  WidgetPluginEvent pluginEvent(true, NS_PLUGIN_INPUT_EVENT, this);
  nsIntPoint point(0, 0);
  InitEvent(pluginEvent, &point);
  NPEvent npEvent;
  npEvent.event = aMsg.message;
  npEvent.wParam = aMsg.wParam;
  npEvent.lParam = aMsg.lParam;
  pluginEvent.pluginEvent = &npEvent;
  pluginEvent.retargetToFocusedDocument = true;
  return DispatchWindowEvent(&pluginEvent);
}

// static
bool
nsWindowBase::InitTouchInjection()
{
  if (!sTouchInjectInitialized) {
    // Initialize touch injection on the first call
    HMODULE hMod = LoadLibraryW(kUser32LibName);
    if (!hMod) {
      return false;
    }

    InitializeTouchInjectionPtr func =
      (InitializeTouchInjectionPtr)GetProcAddress(hMod, "InitializeTouchInjection");
    if (!func) {
      WinUtils::Log("InitializeTouchInjection not available.");
      return false;
    }

    if (!func(TOUCH_INJECT_MAX_POINTS, TOUCH_FEEDBACK_DEFAULT)) {
      WinUtils::Log("InitializeTouchInjection failure. GetLastError=%d", GetLastError());
      return false;
    }

    sInjectTouchFuncPtr =
      (InjectTouchInputPtr)GetProcAddress(hMod, "InjectTouchInput");
    if (!sInjectTouchFuncPtr) {
      WinUtils::Log("InjectTouchInput not available.");
      return false;
    }
    sTouchInjectInitialized = true;
  }
  return true;
}

bool
nsWindowBase::InjectTouchPoint(uint32_t aId, nsIntPoint& aPointerScreenPoint,
                               POINTER_FLAGS aFlags, uint32_t aPressure,
                               uint32_t aOrientation)
{
  if (aId > TOUCH_INJECT_MAX_POINTS) {
    WinUtils::Log("Pointer ID exceeds maximum. See TOUCH_INJECT_MAX_POINTS.");
    return false;
  }

  POINTER_TOUCH_INFO info;
  memset(&info, 0, sizeof(POINTER_TOUCH_INFO));

  info.touchFlags = TOUCH_FLAG_NONE;
  info.touchMask = TOUCH_MASK_CONTACTAREA|TOUCH_MASK_ORIENTATION|TOUCH_MASK_PRESSURE;
  info.pressure = aPressure;
  info.orientation = aOrientation;
  
  info.pointerInfo.pointerFlags = aFlags;
  info.pointerInfo.pointerType =  PT_TOUCH;
  info.pointerInfo.pointerId = aId;
  info.pointerInfo.ptPixelLocation.x = WinUtils::LogToPhys(aPointerScreenPoint.x);
  info.pointerInfo.ptPixelLocation.y = WinUtils::LogToPhys(aPointerScreenPoint.y);

  info.rcContact.top = info.pointerInfo.ptPixelLocation.y - 2;
  info.rcContact.bottom = info.pointerInfo.ptPixelLocation.y + 2;
  info.rcContact.left = info.pointerInfo.ptPixelLocation.x - 2;
  info.rcContact.right = info.pointerInfo.ptPixelLocation.x + 2;
  
  if (!sInjectTouchFuncPtr(1, &info)) {
    WinUtils::Log("InjectTouchInput failure. GetLastError=%d", GetLastError());
    return false;
  }
  return true;
}

nsresult
nsWindowBase::SynthesizeNativeTouchPoint(uint32_t aPointerId,
                                         nsIWidget::TouchPointerState aPointerState,
                                         nsIntPoint aPointerScreenPoint,
                                         double aPointerPressure,
                                         uint32_t aPointerOrientation)
{
  if (!InitTouchInjection()) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  bool hover = aPointerState & TOUCH_HOVER;
  bool contact = aPointerState & TOUCH_CONTACT;
  bool remove = aPointerState & TOUCH_REMOVE;
  bool cancel = aPointerState & TOUCH_CANCEL;

  // win api expects a value from 0 to 1024. aPointerPressure is a value
  // from 0.0 to 1.0.
  uint32_t pressure = (uint32_t)ceil(aPointerPressure * 1024);

  // If we already know about this pointer id get it's record
  PointerInfo* info = mActivePointers.Get(aPointerId);

  // We know about this pointer, send an update
  if (info) {
    POINTER_FLAGS flags = POINTER_FLAG_UPDATE;
    if (hover) {
      flags |= POINTER_FLAG_INRANGE;
    } else if (contact) {
      flags |= POINTER_FLAG_INCONTACT|POINTER_FLAG_INRANGE;
    } else if (remove) {
      flags = POINTER_FLAG_UP;
      // Remove the pointer from our tracking list. This is nsAutPtr wrapped,
      // so shouldn't leak.
      mActivePointers.Remove(aPointerId);
    }

    if (cancel) {
      flags |= POINTER_FLAG_CANCELED;
    }

    return !InjectTouchPoint(aPointerId, aPointerScreenPoint, flags,
                             pressure, aPointerOrientation) ?
      NS_ERROR_UNEXPECTED : NS_OK;
  }

  // Missing init state, error out
  if (remove || cancel) {
    return NS_ERROR_INVALID_ARG;
  }

  // Create a new pointer
  info = new PointerInfo(aPointerId, aPointerScreenPoint);

  POINTER_FLAGS flags = POINTER_FLAG_INRANGE;
  if (contact) {
    flags |= POINTER_FLAG_INCONTACT|POINTER_FLAG_DOWN;
  }

  mActivePointers.Put(aPointerId, info);
  return !InjectTouchPoint(aPointerId, aPointerScreenPoint, flags,
                           pressure, aPointerOrientation) ?
    NS_ERROR_UNEXPECTED : NS_OK;
}

// static
PLDHashOperator
nsWindowBase::CancelTouchPoints(const unsigned int& aPointerId, nsAutoPtr<PointerInfo>& aInfo, void* aUserArg)
{
  nsWindowBase* self = static_cast<nsWindowBase*>(aUserArg);
  self->InjectTouchPoint(aInfo.get()->mPointerId, aInfo.get()->mPosition, POINTER_FLAG_CANCELED);
  return (PLDHashOperator)(PL_DHASH_NEXT|PL_DHASH_REMOVE);
}

nsresult
nsWindowBase::ClearNativeTouchSequence()
{
  if (!sTouchInjectInitialized) {
    return NS_OK;
  }

  // cancel all input points
  mActivePointers.Enumerate(CancelTouchPoints, (void*)this);

  nsBaseWidget::ClearNativeTouchSequence();

  return NS_OK;
}

bool
nsWindowBase::DispatchCommandEvent(uint32_t aEventCommand)
{
  nsCOMPtr<nsIAtom> command;
  switch (aEventCommand) {
    case APPCOMMAND_BROWSER_BACKWARD:
      command = nsGkAtoms::Back;
      break;
    case APPCOMMAND_BROWSER_FORWARD:
      command = nsGkAtoms::Forward;
      break;
    case APPCOMMAND_BROWSER_REFRESH:
      command = nsGkAtoms::Reload;
      break;
    case APPCOMMAND_BROWSER_STOP:
      command = nsGkAtoms::Stop;
      break;
    case APPCOMMAND_BROWSER_SEARCH:
      command = nsGkAtoms::Search;
      break;
    case APPCOMMAND_BROWSER_FAVORITES:
      command = nsGkAtoms::Bookmarks;
      break;
    case APPCOMMAND_BROWSER_HOME:
      command = nsGkAtoms::Home;
      break;
    case APPCOMMAND_CLOSE:
      command = nsGkAtoms::Close;
      break;
    case APPCOMMAND_FIND:
      command = nsGkAtoms::Find;
      break;
    case APPCOMMAND_HELP:
      command = nsGkAtoms::Help;
      break;
    case APPCOMMAND_NEW:
      command = nsGkAtoms::New;
      break;
    case APPCOMMAND_OPEN:
      command = nsGkAtoms::Open;
      break;
    case APPCOMMAND_PRINT:
      command = nsGkAtoms::Print;
      break;
    case APPCOMMAND_SAVE:
      command = nsGkAtoms::Save;
      break;
    case APPCOMMAND_FORWARD_MAIL:
      command = nsGkAtoms::ForwardMail;
      break;
    case APPCOMMAND_REPLY_TO_MAIL:
      command = nsGkAtoms::ReplyToMail;
      break;
    case APPCOMMAND_SEND_MAIL:
      command = nsGkAtoms::SendMail;
      break;
    default:
      return false;
  }
  WidgetCommandEvent event(true, nsGkAtoms::onAppCommand, command, this);

  InitEvent(event);
  return DispatchWindowEvent(&event);
}

bool
nsWindowBase::HandleAppCommandMsg(WPARAM aWParam,
                                  LPARAM aLParam,
                                  LRESULT *aRetValue)
{
  uint32_t appCommand = GET_APPCOMMAND_LPARAM(aLParam);
  uint32_t contentCommandMessage = NS_EVENT_NULL;
  // XXX After we implement KeyboardEvent.key, we should dispatch the
  //     key event if (GET_DEVICE_LPARAM(lParam) == FAPPCOMMAND_KEY) is.
  switch (appCommand)
  {
    case APPCOMMAND_BROWSER_BACKWARD:
    case APPCOMMAND_BROWSER_FORWARD:
    case APPCOMMAND_BROWSER_REFRESH:
    case APPCOMMAND_BROWSER_STOP:
    case APPCOMMAND_BROWSER_SEARCH:
    case APPCOMMAND_BROWSER_FAVORITES:
    case APPCOMMAND_BROWSER_HOME:
    case APPCOMMAND_CLOSE:
    case APPCOMMAND_FIND:
    case APPCOMMAND_HELP:
    case APPCOMMAND_NEW:
    case APPCOMMAND_OPEN:
    case APPCOMMAND_PRINT:
    case APPCOMMAND_SAVE:
    case APPCOMMAND_FORWARD_MAIL:
    case APPCOMMAND_REPLY_TO_MAIL:
    case APPCOMMAND_SEND_MAIL:
      // We shouldn't consume the message always because if we don't handle
      // the message, the sender (typically, utility of keyboard or mouse)
      // may send other key messages which indicate well known shortcut key.
      if (DispatchCommandEvent(appCommand)) {
        // tell the driver that we handled the event
        *aRetValue = 1;
        return true;
      }
      break;

    // Use content command for following commands:
    case APPCOMMAND_COPY:
      contentCommandMessage = NS_CONTENT_COMMAND_COPY;
      break;
    case APPCOMMAND_CUT:
      contentCommandMessage = NS_CONTENT_COMMAND_CUT;
      break;
    case APPCOMMAND_PASTE:
      contentCommandMessage = NS_CONTENT_COMMAND_PASTE;
      break;
    case APPCOMMAND_REDO:
      contentCommandMessage = NS_CONTENT_COMMAND_REDO;
      break;
    case APPCOMMAND_UNDO:
      contentCommandMessage = NS_CONTENT_COMMAND_UNDO;
      break;
  }

  if (contentCommandMessage) {
    WidgetContentCommandEvent contentCommand(true, contentCommandMessage,
                                              this);
    DispatchWindowEvent(&contentCommand);
    // tell the driver that we handled the event
    *aRetValue = 1;
    return true;
  }

  // default = false - tell the driver that the event was not handled
  return false;
}
