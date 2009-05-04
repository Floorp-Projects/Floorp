/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jim Mathies <jmathies@mozilla.com>.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef WinGesture_cpp__
#define WinGesture_cpp__

#include "nscore.h"
#include "nsWinGesture.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsIServiceManager.h"
#include "nsIDOMSimpleGestureEvent.h"
#include "nsGUIEvent.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

const PRUnichar nsWinGesture::kGestureLibraryName[] =  L"user32.dll";
HMODULE nsWinGesture::sLibraryHandle = nsnull;
nsWinGesture::GetGestureInfoPtr nsWinGesture::getGestureInfo = nsnull;
nsWinGesture::CloseGestureInfoHandlePtr nsWinGesture::closeGestureInfoHandle = nsnull;
nsWinGesture::GetGestureExtraArgsPtr nsWinGesture::getGestureExtraArgs = nsnull;
nsWinGesture::SetGestureConfigPtr nsWinGesture::setGestureConfig = nsnull;
nsWinGesture::GetGestureConfigPtr nsWinGesture::getGestureConfig = nsnull;
static PRBool gEnableSingleFingerPanEvents = PR_FALSE;

nsWinGesture::nsWinGesture() :
  mAvailable(PR_FALSE)
{
  (void)InitLibrary();
}

nsWinGesture::~nsWinGesture()
{
  ShutdownLibrary();
}

/* Load and shutdown */

PRBool nsWinGesture::InitLibrary()
{
#ifdef WINCE
  return PR_FALSE;
#else
  if (getGestureInfo) {
    mAvailable = PR_TRUE;
    return PR_TRUE;
  } else if (sLibraryHandle) {
    return PR_FALSE;
  }

  sLibraryHandle = ::LoadLibraryW(kGestureLibraryName);
  if (sLibraryHandle) {
    getGestureInfo = (GetGestureInfoPtr)GetProcAddress(sLibraryHandle, "GetGestureInfo");
    closeGestureInfoHandle = (CloseGestureInfoHandlePtr)GetProcAddress(sLibraryHandle, "CloseGestureInfoHandle");
    getGestureExtraArgs = (GetGestureExtraArgsPtr)GetProcAddress(sLibraryHandle, "GetGestureExtraArgs");
    setGestureConfig = (SetGestureConfigPtr)GetProcAddress(sLibraryHandle, "SetGestureConfig");
    getGestureConfig = (GetGestureConfigPtr)GetProcAddress(sLibraryHandle, "GetGestureConfig");
  }

  if (!getGestureInfo || !closeGestureInfoHandle || !getGestureExtraArgs ||
      !setGestureConfig || !getGestureConfig) {
    getGestureInfo         = nsnull;
    closeGestureInfoHandle = nsnull;
    getGestureExtraArgs    = nsnull;
    setGestureConfig       = nsnull;
    getGestureConfig       = nsnull;
    
    return PR_FALSE;
  }

  mAvailable = PR_TRUE;

  // Check to see if we want single finger gesture input. Only do this once
  // for the app so we don't have to look it up on every window create.
  nsCOMPtr<nsIPrefService> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefs) {
    nsCOMPtr<nsIPrefBranch> prefBranch;
    prefs->GetBranch(0, getter_AddRefs(prefBranch));
    if (prefBranch) {
      PRBool flag;
      if (NS_SUCCEEDED(prefBranch->GetBoolPref("gestures.enable_single_finger_input", &flag))
          && flag)
        gEnableSingleFingerPanEvents = PR_TRUE;
    }
  }

  return PR_TRUE;
#endif
}

void nsWinGesture::ShutdownLibrary()
{
  mAvailable = PR_FALSE;
}

#define GCOUNT 5

PRBool nsWinGesture::InitWinGestureSupport(HWND hWnd)
{
  if (!mAvailable)
    return PR_FALSE;

  GESTURECONFIG config[GCOUNT];

  memset(&config, 0, sizeof(config));

  config[0].dwID = GID_ZOOM;
  config[0].dwWant = GC_ZOOM;
  config[0].dwBlock = 0;

  config[1].dwID = GID_ROTATE;
  config[1].dwWant = GC_ROTATE;
  config[1].dwBlock = 0;

  config[2].dwID = GID_PAN;
  if (gEnableSingleFingerPanEvents) {
    config[2].dwWant = GC_PAN|GC_PAN_WITH_INERTIA|
                       GC_PAN_WITH_GUTTER|
                       GC_PAN_WITH_SINGLE_FINGER_VERTICALLY|
                       GC_PAN_WITH_SINGLE_FINGER_HORIZONTALLY;
    config[2].dwBlock = 0;
  }
  else {
    config[2].dwWant = GC_PAN|GC_PAN_WITH_INERTIA|
                       GC_PAN_WITH_GUTTER;
    config[2].dwBlock = GC_PAN_WITH_SINGLE_FINGER_VERTICALLY|
                        GC_PAN_WITH_SINGLE_FINGER_HORIZONTALLY;
  }

  config[3].dwWant = GC_TWOFINGERTAP;
  config[3].dwID = GID_TWOFINGERTAP;
  config[3].dwBlock = 0;

  config[4].dwWant = GC_ROLLOVER;
  config[4].dwID = GID_ROLLOVER;
  config[4].dwBlock = 0;

  return SetGestureConfig(hWnd, GCOUNT, (PGESTURECONFIG)&config);
}

/* Helpers */

PRBool nsWinGesture::IsAvailable()
{
  return mAvailable;
}

PRBool nsWinGesture::GetGestureInfo(HGESTUREINFO hGestureInfo, PGESTUREINFO pGestureInfo)
{
  if (!mAvailable || !hGestureInfo || !pGestureInfo)
    return PR_FALSE;

  ZeroMemory(pGestureInfo, sizeof(GESTUREINFO));
  pGestureInfo->cbSize = sizeof(GESTUREINFO);

  return getGestureInfo(hGestureInfo, pGestureInfo);
}

PRBool nsWinGesture::CloseGestureInfoHandle(HGESTUREINFO hGestureInfo)
{
  if (!mAvailable || !hGestureInfo)
    return PR_FALSE;

  return closeGestureInfoHandle(hGestureInfo);
}

PRBool nsWinGesture::GetGestureExtraArgs(HGESTUREINFO hGestureInfo, UINT cbExtraArgs, PBYTE pExtraArgs)
{
  if (!mAvailable || !hGestureInfo || !pExtraArgs)
    return PR_FALSE;

  return getGestureExtraArgs(hGestureInfo, cbExtraArgs, pExtraArgs);
}

PRBool nsWinGesture::SetGestureConfig(HWND hWnd, UINT cIDs, PGESTURECONFIG pGestureConfig)
{
  if (!mAvailable || !pGestureConfig)
    return PR_FALSE;

  return setGestureConfig(hWnd, 0, cIDs, pGestureConfig, sizeof(GESTURECONFIG));
}

PRBool nsWinGesture::GetGestureConfig(HWND hWnd, DWORD dwFlags, PUINT pcIDs, PGESTURECONFIG pGestureConfig)
{
  if (!mAvailable || !pGestureConfig)
    return PR_FALSE;

  return getGestureConfig(hWnd, 0, dwFlags, pcIDs, pGestureConfig, sizeof(GESTURECONFIG));
}

PRBool nsWinGesture::IsPanEvent(LPARAM lParam)
{
  GESTUREINFO gi;

  ZeroMemory(&gi,sizeof(GESTUREINFO));
  gi.cbSize = sizeof(GESTUREINFO);

  BOOL result = GetGestureInfo((HGESTUREINFO)lParam, &gi);
  if (!result)
    return PR_FALSE;

  if (gi.dwID == GID_PAN)
    return PR_TRUE;

  return PR_FALSE;
}

/* Gesture event processing */

PRBool
nsWinGesture::ProcessGestureMessage(HWND hWnd, WPARAM wParam, LPARAM lParam, nsSimpleGestureEvent& evt)
{
  GESTUREINFO gi;

  ZeroMemory(&gi,sizeof(GESTUREINFO));
  gi.cbSize = sizeof(GESTUREINFO);

  BOOL result = GetGestureInfo((HGESTUREINFO)lParam, &gi);
  if (!result)
    return PR_FALSE;

  // The coordinates of this event
  nsPointWin coord;
  coord = gi.ptsLocation;
  coord.ScreenToClient(hWnd);

  evt.refPoint.x = coord.x;
  evt.refPoint.y = coord.y;

  // Multiple gesture can occur at the same time so gesture state
  // info can't be shared.
  switch(gi.dwID)
  {
    case GID_BEGIN:
    case GID_END:
      // These should always fall through to DefWndProc
      return PR_FALSE;
      break;

    case GID_ZOOM:
    {
      if (gi.dwFlags & GF_BEGIN) {
        // Send a zoom start event

        // The low 32 bits are the distance in pixels.
        mZoomIntermediate = (float)gi.ullArguments;

        evt.message = NS_SIMPLE_GESTURE_MAGNIFY_START;
        evt.delta = 0.0;
      }
      else {
        // Send a zoom intermediate/end event, the delta is the change
        // in touch points.
        evt.message = NS_SIMPLE_GESTURE_MAGNIFY_UPDATE;
        // (positive for a "zoom in")
        evt.delta = -1.0 * (mZoomIntermediate - (float)gi.ullArguments);
        mZoomIntermediate = (float)gi.ullArguments;
      }
    }
    break;

    case GID_ROTATE:
    {
      // Send a rotate start event
      double radians = 0.0;

      // On GF_BEGIN, ullArguments contains the absolute rotation at the
      // start of the gesture. In later events it contains the offset from
      // the start angle.
      if (gi.ullArguments != 0)
        radians = GID_ROTATE_ANGLE_FROM_ARGUMENT(gi.ullArguments);

      double degrees = -1 * radians * (180/M_PI);

      if (gi.dwFlags & GF_BEGIN) {
          // At some point we should pass the initial angle in
          // along with delta. It's useful.
          degrees = mRotateIntermediate = 0.0;
      }

      evt.direction = 0;
      evt.delta = degrees - mRotateIntermediate;
      mRotateIntermediate = degrees;

      if (evt.delta > 0)
        evt.direction = nsIDOMSimpleGestureEvent::ROTATION_COUNTERCLOCKWISE;
      else if (evt.delta < 0)
        evt.direction = nsIDOMSimpleGestureEvent::ROTATION_CLOCKWISE;

      if (gi.dwFlags & GF_BEGIN)
        evt.message = NS_SIMPLE_GESTURE_ROTATE_START;
      else if (gi.dwFlags & GF_END)
        evt.message = NS_SIMPLE_GESTURE_ROTATE;
      else
        evt.message = NS_SIMPLE_GESTURE_ROTATE_UPDATE;
    }
    break;

    case GID_TWOFINGERTAP:
    {
      // Normally maps to "restore" from whatever you may have recently changed. A simple
      // double click.
      evt.message = NS_SIMPLE_GESTURE_TAP;
    }
    break;

    case GID_ROLLOVER:
    {
      // Two finger drum roll. Defaults to right click if it falls through.
      evt.message = NS_SIMPLE_GESTURE_PRESSTAP;
    }
    break;
  }

  return PR_TRUE;
}

PRBool
nsWinGesture::ProcessPanMessage(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
  GESTUREINFO gi;

  ZeroMemory(&gi,sizeof(GESTUREINFO));
  gi.cbSize = sizeof(GESTUREINFO);

  BOOL result = GetGestureInfo((HGESTUREINFO)lParam, &gi);
  if (!result)
    return PR_FALSE;

  // The coordinates of this event
  nsPointWin coord;
  coord = gi.ptsLocation;
  coord.ScreenToClient(hWnd);

  switch(gi.dwID)
  {
    case GID_BEGIN:
    case GID_END:
      // These should always fall through to DefWndProc
      return PR_FALSE;
      break;

    // Setup pixel scroll events for both axis
    case GID_PAN:
    {
      if (gi.dwFlags & GF_BEGIN) {
        mPanIntermediate = coord;
        mPixelScrollDelta = 0;
      } else {
        mPixelScrollDelta.x = mPanIntermediate.x - coord.x;
        mPixelScrollDelta.y = mPanIntermediate.y - coord.y;
        mPanIntermediate = coord;
      }
    }
    break;
  }
  return PR_TRUE;
}

PRBool
nsWinGesture::PanDeltaToPixelScrollX(nsMouseScrollEvent& evt)
{
  if (mPixelScrollDelta.x != 0)
  {
    evt.scrollFlags = nsMouseScrollEvent::kIsHorizontal|nsMouseScrollEvent::kHasPixels|nsMouseScrollEvent::kNoLines;
    evt.delta = mPixelScrollDelta.x;
    evt.refPoint.x = mPanIntermediate.x;
    evt.refPoint.y = mPanIntermediate.y;
    return PR_TRUE;
  }
  return PR_FALSE;
}

PRBool
nsWinGesture::PanDeltaToPixelScrollY(nsMouseScrollEvent& evt)
{
  if (mPixelScrollDelta.y != 0)
  {
    evt.scrollFlags = nsMouseScrollEvent::kIsVertical|nsMouseScrollEvent::kHasPixels|nsMouseScrollEvent::kNoLines;
    evt.delta = mPixelScrollDelta.y;
    evt.refPoint.x = mPanIntermediate.x;
    evt.refPoint.y = mPanIntermediate.y;
    return PR_TRUE;
  }
  return PR_FALSE;
}

#endif /* WinGesture_cpp__ */


