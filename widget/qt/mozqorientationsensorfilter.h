/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZQORIENTATIONMODULE_H
#define MOZQORIENTATIONMODULE_H

#include <QtSensors/QOrientationReading>
#include <QtSensors/QOrientationFilter>
#include <QObject>
#include <QTransform>


class MozQOrientationSensorFilter : public QObject
                                  , public QtMobility::QOrientationFilter
{
    Q_OBJECT

public:
    MozQOrientationSensorFilter()
    {
        mWindowRotationAngle = 0;
    }

    virtual ~MozQOrientationSensorFilter(){}

    virtual bool filter(QtMobility::QOrientationReading* reading);

    static int GetWindowRotationAngle();
    static QTransform& GetRotationTransform();

Q_SIGNALS:
    void orientationChanged();

private:
    bool filter(QtMobility::QSensorReading *reading)
    {
        return filter(static_cast<QtMobility::QOrientationReading*>(reading));
    }

    static int mWindowRotationAngle;
    static QTransform mWindowRotationTransform;
};

#endif
