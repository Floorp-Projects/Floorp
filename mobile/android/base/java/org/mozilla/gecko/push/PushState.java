/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.push;

import android.content.Context;
import android.support.annotation.NonNull;
import android.support.annotation.WorkerThread;
import android.util.AtomicFile;
import android.util.Log;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;

/**
 * Firefox for Android maintains an App-wide mapping associating
 * profile names to push registrations.  Each push registration in turn associates channels to
 * push subscriptions.
 * <p/>
 * We use a simple storage model of JSON backed by an atomic file.  It is assumed that instances
 * of this class will reference distinct files on disk; and that all accesses will be happen on a
 * single (worker thread).
 */
public class PushState {
    private static final String LOG_TAG = "GeckoPushState";

    private static final long VERSION = 1L;

    protected final @NonNull AtomicFile file;

    protected final @NonNull Map<String, PushRegistration> registrations;

    public PushState(Context context, @NonNull String fileName) {
        this.registrations = new HashMap<>();

        file = new AtomicFile(new File(context.getApplicationInfo().dataDir, fileName));
        synchronized (file) {
            try {
                final String s = new String(file.readFully(), "UTF-8");
                final JSONObject temp = new JSONObject(s);
                if (temp.optLong("version", 0L) != VERSION) {
                    throw new JSONException("Unknown version!");
                }

                final JSONObject registrationsObject = temp.getJSONObject("registrations");
                final Iterator<String> it = registrationsObject.keys();
                while (it.hasNext()) {
                    final String profileName = it.next();
                    final PushRegistration registration = PushRegistration.fromJSONObject(registrationsObject.getJSONObject(profileName));
                    this.registrations.put(profileName, registration);
                }
            } catch (FileNotFoundException e) {
                Log.i(LOG_TAG, "No storage found; starting fresh.");
                this.registrations.clear();
            } catch (IOException | JSONException e) {
                Log.w(LOG_TAG, "Got exception reading storage; dropping storage and starting fresh.", e);
                this.registrations.clear();
            }
        }
    }

    public JSONObject toJSONObject() throws JSONException {
        final JSONObject registrations = new JSONObject();
        for (Map.Entry<String, PushRegistration> entry : this.registrations.entrySet()) {
            registrations.put(entry.getKey(), entry.getValue().toJSONObject());
        }

        final JSONObject jsonObject = new JSONObject();
        jsonObject.put("version", 1L);
        jsonObject.put("registrations", registrations);
        return jsonObject;
    }

    /**
     * Synchronously persist the cache to disk.
     * @return whether the cache was persisted successfully.
     */
    @WorkerThread
    public boolean checkpoint() {
        synchronized (file) {
            FileOutputStream fileOutputStream = null;
            try {
                fileOutputStream = file.startWrite();
                fileOutputStream.write(toJSONObject().toString().getBytes("UTF-8"));
                file.finishWrite(fileOutputStream);
                return true;
            } catch (JSONException | IOException e) {
                Log.e(LOG_TAG, "Got exception writing JSON storage; ignoring.", e);
                if (fileOutputStream != null) {
                    file.failWrite(fileOutputStream);
                }
                return false;
            }
        }
    }

    public PushRegistration putRegistration(@NonNull String profileName, @NonNull PushRegistration registration) {
        return registrations.put(profileName, registration);
    }

    /**
     * Return the existing push registration for the given profile name.
     * @return the push registration, if one is registered; null otherwise.
     */
    public PushRegistration getRegistration(@NonNull String profileName) {
        return registrations.get(profileName);
    }

    /**
     * Return all push registrations, keyed by profile names.
     * @return a map of all push registrations.  <b>The map is intentionally mutable - be careful!</b>
     */
    public @NonNull Map<String, PushRegistration> getRegistrations() {
        return registrations;
    }

    /**
     * Remove any existing push registration for the given profile name.
     * </p>
     * Most registration removals are during iteration, which should use an iterator that is
     * aware of removals.
     * @return the removed push registration, if one was removed; null otherwise.
     */
    public PushRegistration removeRegistration(@NonNull String profileName) {
        return registrations.remove(profileName);
    }
}
