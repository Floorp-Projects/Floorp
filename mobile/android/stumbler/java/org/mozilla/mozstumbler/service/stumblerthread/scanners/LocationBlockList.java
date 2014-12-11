/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.mozstumbler.service.stumblerthread.scanners;

import android.location.Location;
import android.util.Log;
import org.mozilla.mozstumbler.service.AppGlobals;
import org.mozilla.mozstumbler.service.Prefs;

public final class LocationBlockList {
    private static final String LOG_TAG = AppGlobals.makeLogTag(LocationBlockList.class.getSimpleName());
    private static final double MAX_ALTITUDE = 8848;      // Mount Everest's altitude in meters
    private static final double MIN_ALTITUDE = -418;      // Dead Sea's altitude in meters
    private static final float MAX_SPEED = 340.29f;   // Mach 1 in meters/second
    private static final float MIN_ACCURACY = 500;       // meter radius
    private static final long MIN_TIMESTAMP = 946684801; // 2000-01-01 00:00:01
    private static final double GEOFENCE_RADIUS = 0.01;      // .01 degrees is approximately 1km
    private static final long MILLISECONDS_PER_DAY = 86400000;

    private Location mBlockedLocation;
    private boolean mGeofencingEnabled;
    private boolean mIsGeofenced = false;

    public LocationBlockList() {
        updateBlocks();
    }

    public void updateBlocks()    {
        Prefs prefs = Prefs.getInstanceWithoutContext();
        if (prefs == null) {
            return;
        }
        mBlockedLocation = prefs.getGeofenceLocation();
        mGeofencingEnabled = prefs.getGeofenceEnabled();
    }

    public boolean contains(Location location) {
        final float inaccuracy = location.getAccuracy();
        final double altitude = location.getAltitude();
        final float bearing = location.getBearing();
        final double latitude = location.getLatitude();
        final double longitude = location.getLongitude();
        final float speed = location.getSpeed();
        final long timestamp = location.getTime();
        final long tomorrow = System.currentTimeMillis() + MILLISECONDS_PER_DAY;

        boolean block = false;
        mIsGeofenced = false;

        if (latitude == 0 && longitude == 0) {
            block = true;
            Log.w(LOG_TAG, "Bogus latitude,longitude: 0,0");
        } else {
            if (latitude < -90 || latitude > 90) {
                block = true;
                Log.w(LOG_TAG, "Bogus latitude: " + latitude);
            }

            if (longitude < -180 || longitude > 180) {
                block = true;
                Log.w(LOG_TAG, "Bogus longitude: " + longitude);
            }
        }

        if (location.hasAccuracy() && (inaccuracy < 0 || inaccuracy > MIN_ACCURACY)) {
            block = true;
            Log.w(LOG_TAG, "Insufficient accuracy: " + inaccuracy + " meters");
        }

        if (location.hasAltitude() && (altitude < MIN_ALTITUDE || altitude > MAX_ALTITUDE)) {
            block = true;
            Log.w(LOG_TAG, "Bogus altitude: " + altitude + " meters");
        }

        if (location.hasBearing() && (bearing < 0 || bearing > 360)) {
            block = true;
            Log.w(LOG_TAG, "Bogus bearing: " + bearing + " degrees");
        }

        if (location.hasSpeed() && (speed < 0 || speed > MAX_SPEED)) {
            block = true;
            Log.w(LOG_TAG, "Bogus speed: " + speed + " meters/second");
        }

        if (timestamp < MIN_TIMESTAMP || timestamp > tomorrow) {
            block = true;
            Log.w(LOG_TAG, "Bogus timestamp: " + timestamp);
        }

        if (mGeofencingEnabled &&
            Math.abs(location.getLatitude() - mBlockedLocation.getLatitude()) < GEOFENCE_RADIUS &&
            Math.abs(location.getLongitude() - mBlockedLocation.getLongitude()) < GEOFENCE_RADIUS) {
            block = true;
            mIsGeofenced = true;
        }

        return block;
    }

    public boolean isGeofenced() {
        return mIsGeofenced;
    }
}
