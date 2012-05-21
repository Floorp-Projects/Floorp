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

using namespace QtMobility;

class MozQOrientationSensorFilter : public QObject, public QOrientationFilter
{
    Q_OBJECT

public:
    MozQOrientationSensorFilter()
    {
        mWindowRotationAngle = 0;
    }

    virtual ~MozQOrientationSensorFilter(){}

    virtual bool filter(QOrientationReading* reading);

    static int GetWindowRotationAngle();
    static QTransform& GetRotationTransform();

signals:
    void orientationChanged();

private:
    bool filter(QSensorReading *reading) { return filter(static_cast<QOrientationReading*>(reading)); }

    static int mWindowRotationAngle;
    static QTransform mWindowRotationTransform;
};

#endif
