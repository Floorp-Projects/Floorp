/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozqorientationsensorfilter.h"
#ifdef MOZ_ENABLE_MEEGOTOUCH
#include <MApplication>
#include <MWindow>
#include <MSceneManager>
#endif
#include "nsXULAppAPI.h"

int MozQOrientationSensorFilter::mWindowRotationAngle = 0;
QTransform MozQOrientationSensorFilter::mWindowRotationTransform;

bool
MozQOrientationSensorFilter::filter(QOrientationReading* reading)
{
    switch (reading->orientation()) {
    //The Top edge of the device is pointing up.
    case QOrientationReading::TopDown:
        mWindowRotationAngle = 90;
        break;
    //The Top edge of the device is pointing down.
    case QOrientationReading::TopUp:
        mWindowRotationAngle = 270;
        break;
    //The Left edge of the device is pointing up.
    case QOrientationReading::LeftUp:
        mWindowRotationAngle = 180;
        break;
    //The Right edge of the device is pointing up.
    case QOrientationReading::RightUp:
        mWindowRotationAngle = 0;
        break;
    //The Face of the device is pointing up.
    case QOrientationReading::FaceUp:
    //The Face of the device is pointing down.
    case QOrientationReading::FaceDown:
    //The orientation is unknown.
    case QOrientationReading::Undefined:
    default:
        return true;
    }

    mWindowRotationTransform = QTransform();
    mWindowRotationTransform.rotate(mWindowRotationAngle);

#ifdef MOZ_ENABLE_MEEGOTOUCH
    if (XRE_GetProcessType() == GeckoProcessType_Default) {
        MWindow* window = MApplication::activeWindow();
        if (window && window->sceneManager()) {
            window->sceneManager()->
                setOrientationAngle((M::OrientationAngle)mWindowRotationAngle,
                                    MSceneManager::ImmediateTransition);
        }
    }
#else
    emit orientationChanged();
#endif

    return true; // don't store the reading in the sensor
}

int
MozQOrientationSensorFilter::GetWindowRotationAngle()
{
#ifdef MOZ_ENABLE_MEEGOTOUCH
    if (XRE_GetProcessType() == GeckoProcessType_Default) {
        MWindow* window = MApplication::activeWindow();
        if (window) {
            M::OrientationAngle angle = window->orientationAngle();
            if (mWindowRotationAngle != angle) {
                mWindowRotationAngle = angle;
                mWindowRotationTransform = QTransform();
                mWindowRotationTransform.rotate(mWindowRotationAngle);
            }
        }
    }
#endif
    return mWindowRotationAngle;
}

QTransform&
MozQOrientationSensorFilter::GetRotationTransform()
{
    GetWindowRotationAngle();
    return mWindowRotationTransform;
}

