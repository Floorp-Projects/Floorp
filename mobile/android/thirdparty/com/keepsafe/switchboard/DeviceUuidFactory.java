/*
   Copyright 2012 KeepSafe Software Inc.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/
package com.keepsafe.switchboard;

import java.util.UUID;

import android.content.Context;
import android.content.SharedPreferences;

/**
 * Generates a UUID and stores is persistent as in the apps shared preferences.
 * 
 * @author Philipp Berner
 */
public class DeviceUuidFactory {
    protected static final String PREFS_FILE = "com.keepsafe.switchboard.uuid";
    protected static final String PREFS_DEVICE_ID = "device_id";

    private static UUID uuid = null;

    public DeviceUuidFactory(Context context) {
        if (uuid == null) {
            synchronized (DeviceUuidFactory.class) {
                if (uuid == null) {
                    final SharedPreferences prefs = context
                            .getSharedPreferences(PREFS_FILE, Context.MODE_PRIVATE);
                    final String id = prefs.getString(PREFS_DEVICE_ID, null);

                    if (id != null) {
                        // Use the ids previously computed and stored in the prefs file
                        uuid = UUID.fromString(id);
                    } else {
                        uuid = UUID.randomUUID();

                        // Write the value out to the prefs file
                        prefs.edit().putString(PREFS_DEVICE_ID, uuid.toString()).apply();
                    }
                }
            }
        }
    }

    /**
     * Returns a unique UUID for the current android device. As with all UUIDs,
     * this unique ID is "very highly likely" to be unique across all Android
     * devices. Much more so than ANDROID_ID is.
     *
     * The UUID is generated with <code>UUID.randomUUID()</code>.
     *
     * @return a UUID that may be used to uniquely identify your device for most
     *         purposes.
     */
    public UUID getDeviceUuid() {
        return uuid;
    }

}