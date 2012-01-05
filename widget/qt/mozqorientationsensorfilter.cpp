/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=4 et sw=4 tw=80: */
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
 * The Original Code is Nokia.
 *
 * The Initial Developer of the Original Code is Nokia Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

