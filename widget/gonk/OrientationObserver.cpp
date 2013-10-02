/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* Copyright 2012 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "base/basictypes.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Hal.h"
#include "nsIScreen.h"
#include "nsIScreenManager.h"
#include "OrientationObserver.h"
#include "mozilla/HalSensor.h"
#include "ProcessOrientation.h"
#include "nsServiceManagerUtils.h"

using namespace mozilla;
using namespace dom;

namespace {

struct OrientationMapping {
  uint32_t mScreenRotation;
  ScreenOrientation mDomOrientation;
};

static OrientationMapping sOrientationMappings[] = {
  {nsIScreen::ROTATION_0_DEG,   eScreenOrientation_PortraitPrimary},
  {nsIScreen::ROTATION_180_DEG, eScreenOrientation_PortraitSecondary},
  {nsIScreen::ROTATION_90_DEG,  eScreenOrientation_LandscapePrimary},
  {nsIScreen::ROTATION_270_DEG, eScreenOrientation_LandscapeSecondary},
};

const static int sDefaultLandscape = 2;
const static int sDefaultPortrait = 0;

static uint32_t sOrientationOffset = 0;

static already_AddRefed<nsIScreen>
GetPrimaryScreen()
{
  nsCOMPtr<nsIScreenManager> screenMgr =
    do_GetService("@mozilla.org/gfx/screenmanager;1");
  NS_ENSURE_TRUE(screenMgr, nullptr);

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

  int32_t left, top, width, height;
  if (NS_FAILED(screen->GetRect(&left, &top, &width, &height))) {
    return;
  }

  uint32_t rotation;
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
ConvertToScreenRotation(ScreenOrientation aOrientation, uint32_t *aResult)
{
  for (int i = 0; i < ArrayLength(sOrientationMappings); i++) {
    if (aOrientation & sOrientationMappings[i].mDomOrientation) {
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
ConvertToDomOrientation(uint32_t aRotation, ScreenOrientation *aResult)
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
static StaticAutoPtr<OrientationObserver> sOrientationSensorObserver;

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
  , mAllowedOrientations(sDefaultOrientations)
  , mOrientation(new mozilla::ProcessOrientation())
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

/* static */ void
OrientationObserver::ShutDown()
{
  if (!sOrientationSensorObserver) {
    return;
  }

  if (sOrientationSensorObserver->mAutoOrientationEnabled) {
    sOrientationSensorObserver->DisableAutoOrientation();
  }
}

void
OrientationObserver::Notify(const hal::SensorData& aSensorData)
{
  // Sensor will call us on the main thread.
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aSensorData.sensor() == hal::SensorType::SENSOR_ACCELERATION);

  nsCOMPtr<nsIScreen> screen = GetPrimaryScreen();
  if (!screen) {
    return;
  }

  uint32_t currRotation;
  if(NS_FAILED(screen->GetRotation(&currRotation))) {
    return;
  }

  int rotation = mOrientation->OnSensorChanged(aSensorData, static_cast<int>(currRotation));
  if (rotation < 0 || rotation == currRotation) {
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

  if (NS_FAILED(screen->SetRotation(static_cast<uint32_t>(rotation)))) {
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
  MOZ_ASSERT(NS_IsMainThread() && !mAutoOrientationEnabled);

  mOrientation->Reset();
  hal::RegisterSensorObserver(hal::SENSOR_ACCELERATION, this);
  mAutoOrientationEnabled = true;
}

/**
 * Unregister the observer. Note that the observer should already be registered.
 */
void
OrientationObserver::DisableAutoOrientation()
{
  MOZ_ASSERT(NS_IsMainThread() && mAutoOrientationEnabled);

  hal::UnregisterSensorObserver(hal::SENSOR_ACCELERATION, this);
  mAutoOrientationEnabled = false;
}

bool
OrientationObserver::LockScreenOrientation(ScreenOrientation aOrientation)
{
  MOZ_ASSERT(aOrientation | (eScreenOrientation_PortraitPrimary |
                             eScreenOrientation_PortraitSecondary |
                             eScreenOrientation_LandscapePrimary |
                             eScreenOrientation_LandscapeSecondary |
                             eScreenOrientation_Default));

  if (aOrientation == eScreenOrientation_Default) {
    aOrientation = (sOrientationOffset == sDefaultPortrait) ?
                    eScreenOrientation_PortraitPrimary :
                    eScreenOrientation_LandscapePrimary;
  }

  // If there are multiple orientations allowed, we should enable the
  // auto-rotation.
  if (aOrientation != eScreenOrientation_LandscapePrimary &&
      aOrientation != eScreenOrientation_LandscapeSecondary &&
      aOrientation != eScreenOrientation_PortraitPrimary &&
      aOrientation != eScreenOrientation_PortraitSecondary) {
    if (!mAutoOrientationEnabled) {
      EnableAutoOrientation();
    }
  } else if (mAutoOrientationEnabled) {
    DisableAutoOrientation();
  }

  mAllowedOrientations = aOrientation;

  nsCOMPtr<nsIScreen> screen = GetPrimaryScreen();
  NS_ENSURE_TRUE(screen, false);

  uint32_t currRotation;
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
  uint32_t rotation;
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
