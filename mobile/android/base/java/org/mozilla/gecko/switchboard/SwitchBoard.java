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
package org.mozilla.gecko.switchboard;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URISyntaxException;
import java.net.URL;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.MissingResourceException;
import java.util.zip.CRC32;

import org.json.JSONException;
import org.json.JSONObject;
import org.json.JSONArray;
import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.search.SearchEngineManager;
import org.mozilla.gecko.util.HardwareUtils;
import org.mozilla.gecko.util.IOUtils;
import org.mozilla.gecko.util.ProxySelector;

import android.content.Context;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.Build;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.text.TextUtils;
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

    // Top-level experiment keys.
    private static final String KEY_DATA = "data";
    private static final String KEY_NAME = "name";
    private static final String KEY_MATCH = "match";
    private static final String KEY_BUCKETS = "buckets";
    private static final String KEY_VALUES = "values";

    // Match keys.
    private static final String KEY_APP_ID = "appId";
    private static final String KEY_COUNTRY = "country";
    private static final String KEY_REGION = "regions";
    private static final String KEY_DEVICE = "device";
    private static final String KEY_LANG = "lang";
    private static final String KEY_MANUFACTURER = "manufacturer";
    private static final String KEY_VERSION = "version";

    // Bucket keys.
    private static final String KEY_MIN = "min";
    private static final String KEY_MAX = "max";

    /**
     * Loads a new config for a user. This method does network I/O, so it
     * should not be called on the main thread.
     *
     * @param c ApplicationContext
     * @param serverUrl Server URL endpoint.
     */
    public static void loadConfig(Context c, @NonNull String serverUrl) {
        final URL url;
        try {
            url = new URL(serverUrl);
        } catch (MalformedURLException e) {
            Log.e(TAG, "Exception creating server URL", e);
            return;
        }

        final String result = readFromUrlGET(url);
        if (DEBUG) Log.d(TAG, "Result: " + result);
        if (result == null) {
            return;
        }

        // Cache result locally in shared preferences.
        Preferences.setDynamicConfigJson(c, result);
    }

    public static boolean isInBucket(Context c, int low, int high) {
        final int userBucket = getUserBucket(c);
        return (userBucket >= low) && (userBucket < high);
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
            // TODO: cache the array into a mapping so we don't do a loop everytime we are looking for a experiment key
            final JSONArray experiments = new JSONObject(config).getJSONArray(KEY_DATA);
            JSONObject experiment = null;

            for (int i = 0; i < experiments.length(); i++) {
                JSONObject entry = experiments.getJSONObject(i);
                final String name = entry.getString(KEY_NAME);
                if (name.equals(experimentName)) {
                    experiment = entry;
                    break;
                }
            }

            if (experiment == null) {
                return false;
            }

            if (!isMatch(c, experiment.optJSONObject(KEY_MATCH))) {
                return false;
            }

            final JSONObject buckets = experiment.getJSONObject(KEY_BUCKETS);
            final boolean inExperiment = isInBucket(c, buckets.getInt(KEY_MIN), buckets.getInt(KEY_MAX));

            if (DEBUG) {
                Log.d(TAG, experimentName + " = " + inExperiment);
            }
            return inExperiment;
        } catch (JSONException e) {
            // If the experiment name is not found in the JSON, just return false.
            // There is no need to log an error, since we don't really care if an
            // inactive experiment is missing from the config.
            return false;
        }
    }

    private static boolean isTargetRegion(JSONArray regions, String region) throws JSONException {
        if (regions == null || region == null) {
            return false;
        }
        for (int i = 0; i < regions.length(); i++) {

            final String checkingRegion = regions.getString(i);
            if (checkingRegion.equalsIgnoreCase(region)) {
                return true;
            }
        }
        return false;
    }

    private static List<String> getExperimentNames(Context c) throws JSONException {
        // TODO: cache the array into a mapping so we don't do a loop everytime we are looking for a experiment key
        final List<String> returnList = new ArrayList<>();
        final String config = Preferences.getDynamicConfigJson(c);
        final JSONArray experiments = new JSONObject(config).getJSONArray(KEY_DATA);

        for (int i = 0; i < experiments.length(); i++) {
            JSONObject entry = experiments.getJSONObject(i);
            returnList.add(entry.getString(KEY_NAME));
        }
        return returnList;
    }

    @Nullable
    private static JSONObject getExperiment(Context c, String experimentName) throws JSONException {
        // TODO: cache the array into a mapping so we don't do a loop everytime we are looking for a experiment key
        final String config = Preferences.getDynamicConfigJson(c);
        final JSONArray experiments = new JSONObject(config).getJSONArray(KEY_DATA);
        JSONObject experiment = null;

        for (int i = 0; i < experiments.length(); i++) {
            JSONObject entry = experiments.getJSONObject(i);
            if (entry.getString(KEY_NAME).equals(experimentName)) {
                experiment = entry;
                break;
            }
        }
        return experiment;
    }

    /**
     * Return false if the match object contains any non-matching patterns. Otherwise returns true.
     */
    private static boolean isMatch(Context context, @Nullable JSONObject matchKeys) {
        // If no match keys are specified, default to enabling the experiment.
        if (matchKeys == null) {
            return true;
        }

        if (matchKeys.has(KEY_APP_ID)) {
            try {
                final String packageName = context.getPackageName();
                final String expectedAppIdPattern = matchKeys.getString(KEY_APP_ID);

                if (!TextUtils.isEmpty(expectedAppIdPattern)
                        && !packageName.matches(expectedAppIdPattern)) {
                    return false;
                }
            } catch (JSONException e) {
                Log.e(TAG, "Exception matching appId", e);
            }
        }

        if (matchKeys.has(KEY_COUNTRY)) {
            try {
                final String country = Locale.getDefault().getISO3Country();
                final String expectedCountryPattern = matchKeys.getString(KEY_COUNTRY);

                if (!TextUtils.isEmpty(expectedCountryPattern)
                        && !country.matches(expectedCountryPattern)) {
                    return false;
                }
            } catch (MissingResourceException | JSONException e) {
                Log.e(TAG, "Exception matching country", e);
            }
        }

        if (matchKeys.has(KEY_DEVICE)) {
            try {
                final String device = Build.DEVICE;
                final String expectedDevicePattern = matchKeys.getString(KEY_DEVICE);

                if (!TextUtils.isEmpty(expectedDevicePattern)
                        && !device.matches(expectedDevicePattern)) {
                    return false;
                }
            } catch (JSONException e) {
                Log.e(TAG, "Exception matching device", e);
            }
        }

        if (matchKeys.has(KEY_LANG)) {
            try {
                final String lang = Locale.getDefault().getISO3Language();
                final String expectedLanguagePattern = matchKeys.getString(KEY_LANG);

                if (!TextUtils.isEmpty(expectedLanguagePattern)
                        && !lang.matches(expectedLanguagePattern)) {
                    return false;
                }
            } catch (MissingResourceException | JSONException e) {
                Log.e(TAG, "Exception matching lang", e);
            }
        }

        if (matchKeys.has(KEY_MANUFACTURER)) {
            try {
                final String manufacturer = Build.MANUFACTURER;
                final String expectedManufacturerPattern = matchKeys.getString(KEY_MANUFACTURER);

                if (!TextUtils.isEmpty(expectedManufacturerPattern)
                        && !manufacturer.matches(expectedManufacturerPattern)) {
                    return false;
                }
            } catch (JSONException e) {
                Log.e(TAG, "Exception matching manufacturer", e);
            }
        }

        if (matchKeys.has(KEY_VERSION)) {
            try {
                final String version = context.getPackageManager().getPackageInfo(context.getPackageName(), 0).versionName;
                final String expectedVersionPattern = matchKeys.getString(KEY_VERSION);

                if (!TextUtils.isEmpty(expectedVersionPattern)
                        && !version.matches(expectedVersionPattern)) {
                    return false;
                }
            } catch (NameNotFoundException | JSONException e) {
                Log.e(TAG, "Exception matching version", e);
            }
        }

        if (matchKeys.has(KEY_REGION)) {
            try {
                final JSONArray regions = matchKeys.getJSONArray(KEY_REGION);
                if (regions.length() <= 0) {
                    return true; // If the array is empty then I guess this means there are no region restrictions
                }
                final String region = GeckoSharedPrefs.forApp(context).getString(SearchEngineManager.PREF_REGION_KEY, null);

                if (!isTargetRegion(regions, region)) {
                    return false;
                }
            } catch (JSONException e) {
                // If the JSON is somehow broken (or this version doesn't understand a different format),
                // just log and continue
                Log.e(TAG, "Exception matching region", e);
            }
        }


        // Default to return true if no matches failed.
        return true;
    }

    /**
     * @return a list of all active experiments.
     */
    public static List<String> getActiveExperiments(Context c) {
        final List<String> returnList = new ArrayList<>();

        final String config = Preferences.getDynamicConfigJson(c);
        if (config == null) {
            return returnList;
        }

        try {
            final JSONObject data = new JSONObject(config);
            final List<String> experiments = getExperimentNames(c);

            for (int i = 0; i < experiments.size(); i++)  {
                final String name = experiments.get(i);

                // Check override value before reading saved JSON.
                Boolean isActive = Preferences.getOverrideValue(c, name);
                if (isActive == null) {
                    // TODO: This is inefficient because it will check all the match cases on all experiments.
                    isActive = isInExperiment(c, name);
                }
                if (isActive) {
                    returnList.add(name);
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
    @Nullable
    public static JSONObject getExperimentValuesFromJson(Context c, String experimentName) {
        final String config = Preferences.getDynamicConfigJson(c);

        if (config == null) {
            return null;
        }

        try {
            final JSONObject experiment = getExperiment(c, experimentName);
            if (experiment == null) {
                return null;
            }
            return experiment.getJSONObject(KEY_VALUES);
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
        HttpURLConnection connection = null;
        InputStreamReader inputStreamReader = null;
        BufferedReader bufferReader = null;
        try {
            connection = (HttpURLConnection) ProxySelector.openConnectionWithProxy(url.toURI());
            connection.setRequestProperty("User-Agent", HardwareUtils.isTablet() ?
                    AppConstants.USER_AGENT_FENNEC_TABLET :
                    AppConstants.USER_AGENT_FENNEC_MOBILE);
            connection.setRequestMethod("GET");
            connection.setUseCaches(false);

            // BufferedReader(Reader, int) can throw, hence we need to keep a separate reference
            // to the InputStreamReader in order to always be able to close it:
            inputStreamReader = new InputStreamReader(connection.getInputStream());
            bufferReader = new BufferedReader(inputStreamReader, 8192);
            String line;
            StringBuilder resultContent = new StringBuilder();
            while ((line = bufferReader.readLine()) != null) {
                resultContent.append(line);
            }

            return resultContent.toString();
        } catch (IOException | URISyntaxException e) {
            e.printStackTrace();
        } finally {
            IOUtils.safeStreamClose(bufferReader);
            IOUtils.safeStreamClose(inputStreamReader);
            if (connection != null) {
                connection.disconnect();
            }
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
