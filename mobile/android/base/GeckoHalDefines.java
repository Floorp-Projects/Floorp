/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

public class GeckoHalDefines
{
    /*
     * Keep these values consistent with |SensorType| in Hal.h
    */
    public static final int SENSOR_ORIENTATION = 0;
    public static final int SENSOR_ACCELERATION = 1;
    public static final int SENSOR_PROXIMITY = 2;
    public static final int SENSOR_LINEAR_ACCELERATION = 3;
    public static final int SENSOR_GYROSCOPE = 4;
    public static final int SENSOR_LIGHT = 5;

    public static final int SENSOR_ACCURACY_UNKNOWN = -1;
    public static final int SENSOR_ACCURACY_UNRELIABLE = 0;
    public static final int SENSOR_ACCURACY_LOW = 1;
    public static final int SENSOR_ACCURACY_MED = 2;
    public static final int SENSOR_ACCURACY_HIGH = 3;
};
