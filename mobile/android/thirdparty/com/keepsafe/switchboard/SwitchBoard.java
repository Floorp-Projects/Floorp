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

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URL;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.Locale;
import java.util.MissingResourceException;
import java.util.zip.CRC32;

import org.json.JSONException;
import org.json.JSONObject;

import android.content.Context;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.Build;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.util.Log;


/**
 * SwitchBoard is the core class of the KeepSafe Switchboard mobile A/B testing framework.
 * This class provides a bunch of static methods that can be used in your app to run A/B tests. 
 * 
 * The SwitchBoard supports production and staging environment. 
 * 
 * For usage <code>initDefaultServerUrls</code> for first time usage. Server URLs can be updates from
 * a remote location with <code>initConfigServerUrl</code>.
 * 
 * To run a experiment use <code>isInExperiment()</code>. The experiment name has to match the one you 
 * setup on the server.
 * All functions are design to be safe for programming mistakes and network connection issues. If the 
 * experiment does not exists it will return false and pretend the user is not part of it.
 * 
 * @author Philipp Berner
 *
 */
public class SwitchBoard {

    private static final String TAG = "SwitchBoard";

    /** Set if the application is run in debug mode. */
    public static boolean DEBUG = true;

    private static final String IS_EXPERIMENT_ACTIVE = "isActive";
    private static final String EXPERIMENT_VALUES = "values";

    private static final String KEY_SERVER_URL = "mainServerUrl";
    private static final String KEY_CONFIG_RESULTS = "results";

    /**
     * Loads a new config for a user. This method allows you to pass your own unique user ID instead of using
     * the SwitchBoard internal user ID.
     * Don't call method direct for background threading reasons.
     * @param c ApplicationContext
     * @param defaultServerUrl Default server URL endpoint.
     */
    static void loadConfig(Context c, @NonNull String defaultServerUrl) {

        // Eventually, we want to check `Preferences.getDynamicConfigServerUrl(c);` before
        // falling back to the default server URL. However, this will require figuring
        // out a new solution for dynamically specifying a new server from the intent.
        String serverUrl = defaultServerUrl;

        final URL requestUrl = buildConfigRequestUrl(c, serverUrl);
        if (DEBUG) Log.d(TAG, "Request URL: " + requestUrl);
        if (requestUrl == null) {
            return;
        }


        final String result = readFromUrlGET(requestUrl);
        if (DEBUG) Log.d(TAG, "Result: " + result);
        if (result == null) {
            return;
        }

        try {
            final JSONObject json = new JSONObject(result);

            // Update the server URL if necessary.
            final String newServerUrl = json.getString(KEY_SERVER_URL);
            if (!defaultServerUrl.equals(newServerUrl)) {
                Preferences.setDynamicConfigServerUrl(c, newServerUrl);
            }

            // Store the config in shared prefs.
            final String config = json.getString(KEY_CONFIG_RESULTS);
            Preferences.setDynamicConfigJson(c, config);
        } catch (JSONException e) {
            Log.e(TAG, "Exception parsing server result", e);
        }
    }

    @Nullable private static URL buildConfigRequestUrl(Context c, String serverUrl) {
        final DeviceUuidFactory df = new DeviceUuidFactory(c);
        final String uuid = df.getDeviceUuid().toString();

        final String device = Build.DEVICE;
        final String manufacturer = Build.MANUFACTURER;
        String lang = "unknown";
        try {
            lang = Locale.getDefault().getISO3Language();
        } catch (MissingResourceException e) {
            e.printStackTrace();
        }
        String country = "unknown";
        try {
            country = Locale.getDefault().getISO3Country();
        } catch (MissingResourceException e) {
            e.printStackTrace();
        }

        final String packageName = c.getPackageName();
        String versionName = "none";
        try {
            versionName = c.getPackageManager().getPackageInfo(c.getPackageName(), 0).versionName;
        } catch (NameNotFoundException e) {
            e.printStackTrace();
        }

        final String params = "uuid="+uuid+"&device="+device+"&lang="+lang+"&country="+country
                +"&manufacturer="+manufacturer+"&appId="+packageName+"&version="+versionName;

        try {
            return new URL(serverUrl + "?" + params);
        } catch (MalformedURLException e) {
            e.printStackTrace();
            return null;
        }
    }

    public static boolean isInBucket(Context c, int low, int high) {
        int userBucket = getUserBucket(c);
        if (userBucket >= low && userBucket < high)
            return true;
        else
            return false;
    }

    /**
     * Looks up in config if user is in certain experiment. Returns false as a default value when experiment
     * does not exist.
     * Experiment names are defined server side as Key in array for return values.
     * @param experimentName Name of the experiment to lookup
     * @return returns value for experiment or false if experiment does not exist.
     */
    public static boolean isInExperiment(Context c, String experimentName) {
        final Boolean override = Preferences.getOverrideValue(c, experimentName);
        if (override != null) {
            return override;
        }

        final String config = Preferences.getDynamicConfigJson(c);
        if (config == null) {
            return false;
        }

        try {
            final JSONObject experiment = new JSONObject(config).getJSONObject(experimentName);
            return experiment != null && experiment.getBoolean(IS_EXPERIMENT_ACTIVE);
        } catch (JSONException e) {
            // If the experiment name is not found in the JSON, just return false.
            // There is no need to log an error, since we don't really care if an
            // inactive experiment is missing from the config.
            return false;
        }
    }

    /**
     * @return a list of all active experiments.
     */
    public static List<String> getActiveExperiments(Context c) {
        final ArrayList<String> returnList = new ArrayList<>();

        final String config = Preferences.getDynamicConfigJson(c);
        if (config == null) {
            return returnList;
        }

        try {
            final JSONObject experiments = new JSONObject(config);
            Iterator<?> iter = experiments.keys();
            while (iter.hasNext()) {
                final String key = (String) iter.next();

                // Check override value before reading saved JSON.
                Boolean isActive = Preferences.getOverrideValue(c, key);
                if (isActive == null) {
                    final JSONObject experiment = experiments.getJSONObject(key);
                    isActive = experiment.getBoolean(IS_EXPERIMENT_ACTIVE);
                }
                if (isActive) {
                    returnList.add(key);
                }
            }
        } catch (JSONException e) {
            // Something went wrong!
        }

        return returnList;
    }

    /**
     * Checks if a certain experiment has additional values.
     * @param c ApplicationContext
     * @param experimentName Name of the experiment
     * @return true when experiment exists
     */
    public static boolean hasExperimentValues(Context c, String experimentName) {
        return getExperimentValuesFromJson(c, experimentName) != null;
    }

    /**
     * Returns the experiment value as a JSONObject.
     * @param experimentName Name of the experiment
     * @return Experiment value as String, null if experiment does not exist.
     */
    public static JSONObject getExperimentValuesFromJson(Context c, String experimentName) {
        final String config = Preferences.getDynamicConfigJson(c);

        if (config == null) {
            return null;
        }

        try {
            final JSONObject experiment = new JSONObject(config).getJSONObject(experimentName);
            return experiment.getJSONObject(EXPERIMENT_VALUES);
        } catch (JSONException e) {
            Log.e(TAG, "Could not create JSON object from config string", e);
        }

        return null;
    }

    /**
     * Returns a String containing the server response from a GET request
     * @param url URL for GET request.
     * @return Returns String from server or null when failed.
     */
    @Nullable private static String readFromUrlGET(URL url) {
        try {
            HttpURLConnection connection = (HttpURLConnection) url.openConnection();
            connection.setRequestMethod("GET");
            connection.setUseCaches(false);

            InputStream is = connection.getInputStream();
            InputStreamReader inputStreamReader = new InputStreamReader(is);
            BufferedReader bufferReader = new BufferedReader(inputStreamReader, 8192);
            String line;
            StringBuilder resultContent = new StringBuilder();
            while ((line = bufferReader.readLine()) != null) {
                resultContent.append(line);
            }
            bufferReader.close();

            return resultContent.toString();
        } catch (IOException e) {
            e.printStackTrace();
        }

        return null;
    }

    /**
     * Return the bucket number of the user. There are 100 possible buckets.
     */
    private static int getUserBucket(Context c) {
        final DeviceUuidFactory df = new DeviceUuidFactory(c);
        final String uuid = df.getDeviceUuid().toString();

        CRC32 crc = new CRC32();
        crc.update(uuid.getBytes());
        long checksum = crc.getValue();
        return (int)(checksum % 100L);
    }
}
