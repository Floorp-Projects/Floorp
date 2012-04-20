/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Hal.h"
#include "nsIScreen.h"
#include "nsIScreenManager.h"
#include "OrientationObserver.h"

using namespace mozilla;
using namespace dom;

namespace {

struct OrientationMapping {
  PRUint32 mScreenRotation;
  ScreenOrientation mDomOrientation;
};

static OrientationMapping sOrientationMappings[] = {
  {nsIScreen::ROTATION_0_DEG,   eScreenOrientation_PortraitPrimary},
  {nsIScreen::ROTATION_180_DEG, eScreenOrientation_PortraitSecondary},
  {nsIScreen::ROTATION_0_DEG,   eScreenOrientation_Portrait},
  {nsIScreen::ROTATION_90_DEG,  eScreenOrientation_LandscapePrimary},
  {nsIScreen::ROTATION_270_DEG, eScreenOrientation_LandscapeSecondary},
  {nsIScreen::ROTATION_90_DEG,  eScreenOrientation_Landscape}
};

const static int sDefaultLandscape = 3;
const static int sDefaultPortrait = 0;

static PRUint32 sOrientationOffset = 0;

static already_AddRefed<nsIScreen>
GetPrimaryScreen()
{
  nsCOMPtr<nsIScreenManager> screenMgr =
    do_GetService("@mozilla.org/gfx/screenmanager;1");
  NS_ENSURE_TRUE(screenMgr, nsnull);

  nsCOMPtr<nsIScreen> screen;
  screenMgr->GetPrimaryScreen(getter_AddRefs(screen));
  return screen.forget();
}

static void
DetectDefaultOrientation()
{
  nsCOMPtr<nsIScreen> screen = GetPrimaryScreen();
  if (!screen) {
    return;
  }

  PRInt32 left, top, width, height;
  if (NS_FAILED(screen->GetRect(&left, &top, &width, &height))) {
    return;
  }

  PRUint32 rotation;
  if (NS_FAILED(screen->GetRotation(&rotation))) {
    return;
  }

  if (width < height) {
    if (rotation == nsIScreen::ROTATION_0_DEG ||
        rotation == nsIScreen::ROTATION_180_DEG) {
      sOrientationOffset = sDefaultPortrait;
    } else {
      sOrientationOffset = sDefaultLandscape;
    }
  } else {
    if (rotation == nsIScreen::ROTATION_0_DEG ||
        rotation == nsIScreen::ROTATION_180_DEG) {
      sOrientationOffset = sDefaultLandscape;
    } else {
      sOrientationOffset = sDefaultPortrait;
    }
  }
}

/**
 * Converts DOM orientation to nsIScreen rotation. Portrait and Landscape are
 * treated as PortraitPrimary and LandscapePrimary, respectively, during
 * conversion.
 *
 * @param aOrientation DOM orientation e.g.
 *        dom::eScreenOrientation_PortraitPrimary.
 * @param aResult output nsIScreen rotation e.g. nsIScreen::ROTATION_0_DEG.
 * @return NS_OK on success. NS_ILLEGAL_VALUE on failure.
 */
static nsresult
ConvertToScreenRotation(ScreenOrientation aOrientation, PRUint32 *aResult)
{
  for (int i = 0; i < ArrayLength(sOrientationMappings); i++) {
    if (aOrientation == sOrientationMappings[i].mDomOrientation) {
      // Shift the mappings in sOrientationMappings so devices with default
      // landscape orientation map landscape-primary to 0 degree and so forth.
      int adjusted = (i + sOrientationOffset) %
                     ArrayLength(sOrientationMappings);
      *aResult = sOrientationMappings[adjusted].mScreenRotation;
      return NS_OK;
    }
  }

  *aResult = nsIScreen::ROTATION_0_DEG;
  return NS_ERROR_ILLEGAL_VALUE;
}

/**
 * Converts nsIScreen rotation to DOM orientation.
 *
 * @param aRotation nsIScreen rotation e.g. nsIScreen::ROTATION_0_DEG.
 * @param aResult output DOM orientation e.g.
 *        dom::eScreenOrientation_PortraitPrimary.
 * @return NS_OK on success. NS_ILLEGAL_VALUE on failure.
 */
nsresult
ConvertToDomOrientation(PRUint32 aRotation, ScreenOrientation *aResult)
{
  for (int i = 0; i < ArrayLength(sOrientationMappings); i++) {
    if (aRotation == sOrientationMappings[i].mScreenRotation) {
      // Shift the mappings in sOrientationMappings so devices with default
      // landscape orientation map 0 degree to landscape-primary and so forth.
      int adjusted = (i + sOrientationOffset) %
                     ArrayLength(sOrientationMappings);
      *aResult = sOrientationMappings[adjusted].mDomOrientation;
      return NS_OK;
    }
  }

  *aResult = eScreenOrientation_None;
  return NS_ERROR_ILLEGAL_VALUE;
}

// Note that all operations with sOrientationSensorObserver
// should be on the main thread.
static nsAutoPtr<OrientationObserver> sOrientationSensorObserver;

} // Anonymous namespace

OrientationObserver*
OrientationObserver::GetInstance()
{
  if (!sOrientationSensorObserver) {
    sOrientationSensorObserver = new OrientationObserver();
    ClearOnShutdown(&sOrientationSensorObserver);
  }

  return sOrientationSensorObserver;
}

OrientationObserver::OrientationObserver()
  : mAutoOrientationEnabled(false)
  , mLastUpdate(0)
  , mAllowedOrientations(sDefaultOrientations)
{
  DetectDefaultOrientation();

  EnableAutoOrientation();
}

OrientationObserver::~OrientationObserver()
{
  if (mAutoOrientationEnabled) {
    DisableAutoOrientation();
  }
}

void
OrientationObserver::Notify(const hal::SensorData& aSensorData)
{
  // Sensor will call us on the main thread.
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aSensorData.sensor() == SensorType::SENSOR_ORIENTATION);

  InfallibleTArray<float> values = aSensorData.values();
  // Azimuth (values[0]): the device's horizontal orientation
  // (0 degree is north). It's unused for screen rotation.
  float pitch = values[1];
  float roll = values[2];

  PRUint32 rotation;
  if (roll > 45) {
    rotation = nsIScreen::ROTATION_90_DEG;
  } else if (roll < -45) {
    rotation = nsIScreen::ROTATION_270_DEG;
  } else if (pitch < -45) {
    rotation = nsIScreen::ROTATION_0_DEG;
  } else if (pitch > 45) {
    rotation = nsIScreen::ROTATION_180_DEG;
  } else {
    // Don't rotate if neither pitch nor roll exceeds the 45 degree threshold.
    return;
  }

  nsCOMPtr<nsIScreen> screen = GetPrimaryScreen();
  if (!screen) {
    return;
  }

  PRUint32 currRotation;
  if (NS_FAILED(screen->GetRotation(&currRotation)) ||
      rotation == currRotation) {
    return;
  }

  ScreenOrientation orientation;
  if (NS_FAILED(ConvertToDomOrientation(rotation, &orientation))) {
    return;
  }

  if ((mAllowedOrientations & orientation) == eScreenOrientation_None) {
    // The orientation from sensor is not allowed.
    return;
  }

  PRTime now = PR_Now();
  MOZ_ASSERT(now > mLastUpdate);
  if (now - mLastUpdate < sMinUpdateInterval) {
    return;
  }
  mLastUpdate = now;

  if (NS_FAILED(screen->SetRotation(rotation))) {
    // Don't notify dom on rotation failure.
    return;
  }
}

/**
 * Register the observer. Note that the observer shouldn't be registered.
 */
void
OrientationObserver::EnableAutoOrientation()
{
  MOZ_ASSERT(NS_IsMainThread() && !AutoOrientationEnabled());

  hal::RegisterSensorObserver(hal::SENSOR_ORIENTATION, this);
  mAutoOrientationEnabled = true;
}

/**
 * Unregister the observer. Note that the observer should already be registered.
 */
void
OrientationObserver::DisableAutoOrientation()
{
  MOZ_ASSERT(NS_IsMainThread() && mAutoOrientationEnabled);

  hal::UnregisterSensorObserver(hal::SENSOR_ORIENTATION, this);
  mAutoOrientationEnabled = false;
}

bool
OrientationObserver::LockScreenOrientation(ScreenOrientation aOrientation)
{
  MOZ_ASSERT(eScreenOrientation_None < aOrientations &&
             aOrientations < eScreenOrientation_EndGuard);

  // Enable/disable the observer depending on 1. multiple orientations
  // allowed, and 2. observer enabled.
  if (aOrientation == eScreenOrientation_Landscape ||
      aOrientation == eScreenOrientation_Portrait) {
    if (!mAutoOrientationEnabled) {
      EnableAutoOrientation();
    }
  } else if (mAutoOrientationEnabled) {
    DisableAutoOrientation();
  }

  mAllowedOrientations = aOrientation;

  nsCOMPtr<nsIScreen> screen = GetPrimaryScreen();
  NS_ENSURE_TRUE(screen, false);

  PRUint32 currRotation;
  nsresult rv = screen->GetRotation(&currRotation);
  NS_ENSURE_SUCCESS(rv, false);

  ScreenOrientation currOrientation = eScreenOrientation_None;
  rv = ConvertToDomOrientation(currRotation, &currOrientation);
  NS_ENSURE_SUCCESS(rv, false);

  // Don't rotate if the current orientation matches one of the
  // requested orientations.
  if (currOrientation & aOrientation) {
    return true;
  }

  // Return false on invalid orientation value.
  PRUint32 rotation;
  rv = ConvertToScreenRotation(aOrientation, &rotation);
  NS_ENSURE_SUCCESS(rv, false);

  rv = screen->SetRotation(rotation);
  NS_ENSURE_SUCCESS(rv, false);

  // This conversion will disambiguate aOrientation.
  ScreenOrientation orientation;
  rv = ConvertToDomOrientation(rotation, &orientation);
  NS_ENSURE_SUCCESS(rv, false);

  return true;
}

void
OrientationObserver::UnlockScreenOrientation()
{
  if (!mAutoOrientationEnabled) {
    EnableAutoOrientation();
  }

  mAllowedOrientations = sDefaultOrientations;
}
