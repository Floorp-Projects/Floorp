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
import java.net.ProtocolException;
import java.net.URL;
import java.util.Locale;
import java.util.MissingResourceException;
import java.util.UUID;
import java.util.zip.CRC32;

import org.json.JSONException;
import org.json.JSONObject;

import android.content.Context;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.Build;
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
	
	/** Set if the application is run in debug mode. DynamicConfig runs against staging server when in debug and production when not */
	public static boolean DEBUG = true;
	
	/** Production server to update the remote server URLs. http://staging.domain/path_to/SwitchboardURLs.php */
	private static String DYNAMIC_CONFIG_SERVER_URL_UPDATE;
	
	/** Production server for getting the actual config file. http://staging.domain/path_to/SwitchboardDriver.php */
	private static String DYNAMIC_CONFIG_SERVER_DEFAULT_URL;
	
	
	private static final String kUpdateServerUrl = "updateServerUrl";
	private static final String kConfigServerUrl = "configServerUrl";
	
	private static final String IS_EXPERIMENT_ACTIVE = "isActive";
	private static final String EXPERIMENT_VALUES = "values";
	
	
	/**
	 * Basic initialization with one server. 
	 * @param configServerUpdateUrl Url to: http://staging.domain/path_to/SwitchboardURLs.php 
	 * @param configServerUrl Url to: http://staging.domain/path_to/SwitchboardDriver.php - the acutall config
	 * @param isDebug Is the application running in debug mode. This will add log messages.
	 */
	public static void initDefaultServerUrls(String configServerUpdateUrl, String configServerUrl,
			boolean isDebug) {
		
		DYNAMIC_CONFIG_SERVER_URL_UPDATE = configServerUpdateUrl;
		DYNAMIC_CONFIG_SERVER_DEFAULT_URL = configServerUrl;
		DEBUG = isDebug;
	}
	
	/**
	 * Advanced initialization that supports a production and staging environment without changing the server URLs manually.
	 * SwitchBoard will connect to the staging environment in debug mode. This makes it very simple to test new experiements
	 * during development.
	 * @param configServerUpdateUrlStaging Url to http://staging.domain/path_to/SwitchboardURLs.php in staging environment
	 * @param configServerUrlStaging Url to: http://staging.domain/path_to/SwitchboardDriver.php in production - the acutall config
	 * @param configServerUpdateUrl Url to http://staging.domain/path_to/SwitchboardURLs.php in production environment
	 * @param configServerUrl Url to: http://staging.domain/path_to/SwitchboardDriver.php in production - the acutall config
	 * @param isDebug Defines if the app runs in debug.
	 */
	public static void initDefaultServerUrls(String configServerUpdateUrlStaging, String configServerUrlStaging, 
			String configServerUpdateUrl, String configServerUrl,
			boolean isDebug) {
		
		if(isDebug) {
			DYNAMIC_CONFIG_SERVER_URL_UPDATE = configServerUpdateUrlStaging;
			DYNAMIC_CONFIG_SERVER_DEFAULT_URL = configServerUrlStaging;
		} else {
			DYNAMIC_CONFIG_SERVER_URL_UPDATE = configServerUpdateUrl;
			DYNAMIC_CONFIG_SERVER_DEFAULT_URL = configServerUrl;	
		}
		
		DEBUG = isDebug;
	}
	
	/**
	 * Updates the server URLs from remote and stores it locally in the app. This allows to move the server side
	 * whith users already using Switchboard. 
	 * When there is no internet connection it will continue to use the URLs from the last time or 
	 * default URLS that have been set with <code>initDefaultServerUrls</code>.
	 * 
	 * This methode should always be executed in a background thread to not block the UI.
	 * 
	 * @param c Application context
	 */
	public static void updateConfigServerUrl(Context c) {
		if(DEBUG) Log.d(TAG, "start initConfigServerUrl");
		
		if(DEBUG) {
			//set default value that is set in code for debug mode.
			Preferences.setDynamicConfigServerUrl(c, DYNAMIC_CONFIG_SERVER_URL_UPDATE, DYNAMIC_CONFIG_SERVER_DEFAULT_URL);
			return;
		}
		
		//lookup new config server url from the one that is in shared prefs
		String updateServerUrl = Preferences.getDynamicUpdateServerUrl(c);
		
		//set to default when not set in preferences
		if(updateServerUrl == null) 
			updateServerUrl = DYNAMIC_CONFIG_SERVER_URL_UPDATE;
		
		try {
			String result = readFromUrlGET(updateServerUrl, "");
			if(DEBUG) Log.d(TAG, "Result String: " + result);
			
			if(result != null){
				JSONObject a = new JSONObject(result);
				
				Preferences.setDynamicConfigServerUrl(c, (String)a.get(kUpdateServerUrl), (String)a.get(kConfigServerUrl));
				
				if(DEBUG) Log.d(TAG, "Update Server Url: " + (String)a.get(kUpdateServerUrl));
				if(DEBUG) Log.d(TAG, "Config Server Url: " + (String)a.get(kConfigServerUrl));
			} else {
				storeDefaultUrlsInPreferences(c);
			}
			
		} catch (JSONException e) {
			e.printStackTrace();
		}
		
		if(DEBUG) Log.d(TAG, "end initConfigServerUrl");
	}
	
	/**
	 * Loads a new config file for the specific user from current config server. Uses internal unique user ID.
	 * Use this method only in background thread as network connections are involved that block UI thread.
	 * Use AsyncConfigLoader() for easy background threading.
	 * @param c ApplicationContext
	 */
	public static void loadConfig(Context c) {
		loadConfig(c, null);
	}

	/**
	 * Loads a new config for a user. This method allows you to pass your own unique user ID instead of using 
	 * the SwitchBoard internal user ID.
	 * Don't call method direct for background threading reasons. 
	 * @param c ApplicationContext
	 * @param uuid Custom unique user ID
	 */
	public static void loadConfig(Context c, String uuid) {
		
		try {
			
			//get uuid
			if(uuid == null) {
				DeviceUuidFactory df = new DeviceUuidFactory(c);
				uuid = df.getDeviceUuid().toString();
			}
			
			String device = Build.DEVICE;
			String manufacturer = Build.MANUFACTURER;
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
			String packageName = c.getPackageName();
			String versionName = "none";
			try {
				versionName = c.getPackageManager().getPackageInfo(c.getPackageName(), 0).versionName;
			} catch (NameNotFoundException e) {
				e.printStackTrace();
			}
			
			//load config, includes all experiments
			String serverUrl = Preferences.getDynamicConfigServerUrl(c);
			
			if(serverUrl != null) {
				String params = "uuid="+uuid+"&device="+device+"&lang="+lang+"&country="+country
						+"&manufacturer="+manufacturer+"&appId="+packageName+"&version="+versionName;
				if(DEBUG) Log.d(TAG, "Read from server URL: " + serverUrl + params);
				String serverConfig = readFromUrlGET(serverUrl, params);
				
				if(DEBUG) Log.d(TAG, serverConfig);
				
				//store experiments in shared prefs (one variable)
				if(serverConfig != null)
					Preferences.setDynamicConfigJson(c, serverConfig);
			}
			
		} catch (NullPointerException e) {
			e.printStackTrace();
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
		return isInExperiment(c, experimentName, false);
	}
	
	/**
	 * Looks up in config if user is in certain experiment.
	 * Experiment names are defined server side as Key in array for return values.
	 * @param experimentName Name of the experiment to lookup
	 * @param defaultReturnVal The return value that should be return when experiment does not exist
	 * @return returns value for experiment or defaultReturnVal if experiment does not exist.
	 */
	public static boolean isInExperiment(Context c, String experimentName, boolean defaultReturnVal) {
		//lookup experiment in config
		String config = Preferences.getDynamicConfigJson(c);
		
		//if it does not exist
		if(config == null)
			return false;
		else {
			
			try {
				JSONObject experiment = (JSONObject) new JSONObject(config).get(experimentName);
				if(DEBUG) Log.d(TAG, "experiment " + experimentName + " JSON object: " + experiment.toString());
				if(experiment == null)
					return defaultReturnVal;
				
				boolean returnValue = defaultReturnVal;
				returnValue = experiment.getBoolean(IS_EXPERIMENT_ACTIVE);
				
				return returnValue;
			} catch (JSONException e) {
				Log.e(TAG, "Config: " + config);
				e.printStackTrace();
				
			}
		
			//return false when JSON fails
			return defaultReturnVal;
		}
		
	}
	
	/**
	 * Checks if a certain experiment exists. 
	 * @param c ApplicationContext
	 * @param experimentName Name of the experiment
	 * @return true when experiment exists
	 */
	public static boolean hasExperimentValues(Context c, String experimentName) {
		if(getExperimentValueFromJson(c, experimentName) == null)
			return false;
		else
			return true;
	}
	
	/**
	 * Returns the experiment value as a JSONObject. Depending on what experiment is has to be converted to the right type.
	 * Typcasting is by convention. You have to know what it's in there. Use <code>hasExperiment()</code>
	 * before this to avoid NullPointerExceptions. 
	 * @param experimentName Name of the experiment to lookup
	 * @return Experiment value as String, null if experiment does not exist.
	 */
	public static JSONObject getExperimentValueFromJson(Context c, String experimentName) {
		String config = Preferences.getDynamicConfigJson(c);
		
		if(config == null)
			return null;
		
		try {
			JSONObject experiment = (JSONObject) new JSONObject(config).get(experimentName);
			JSONObject values = experiment.getJSONObject(EXPERIMENT_VALUES);
			
			return values;
			
		} catch (JSONException e) {
			Log.e(TAG, "Config: " + config);
			e.printStackTrace();
			Log.e(TAG, "Could not create JSON object from config string", e);
		}
		
		return null;
	}
	
	/**
	 * Sets config server URLs in shared prefs to defaul when not set already. It keeps
	 * URLs when already set in shared preferences.
	 * @param c
	 */
	private static void storeDefaultUrlsInPreferences(Context c) {
		String configUrl = Preferences.getDynamicConfigServerUrl(c);
		String updateUrl = Preferences.getDynamicUpdateServerUrl(c);
			
		if(configUrl == null)
			configUrl = DYNAMIC_CONFIG_SERVER_DEFAULT_URL;
		
		if(updateUrl == null)
			updateUrl = DYNAMIC_CONFIG_SERVER_URL_UPDATE;
		
		Preferences.setDynamicConfigServerUrl(c, updateUrl, configUrl);
	}
	
	/**
	 * Returns a String containing the server response from a GET request
	 * @param address Valid http addess.
	 * @param params String of params. Multiple params seperated with &. No leading ? in string
	 * @return Returns String from server or null when failed.
	 */
	private static String readFromUrlGET(String address, String params) {
		if(address == null || params == null)
			return null;
		
		String completeUrl = address + "?" + params;
		if(DEBUG) Log.d(TAG, "readFromUrl(): " + completeUrl);
		
		try {
			URL url = new URL(completeUrl);
			HttpURLConnection connection = (HttpURLConnection) url.openConnection();
			connection.setRequestMethod("GET");
			connection.setUseCaches(false);
			connection.setDoOutput(true);

			// get response
			InputStream is = connection.getInputStream();
			InputStreamReader inputStreamReader = new InputStreamReader(is);
			BufferedReader bufferReader = new BufferedReader(inputStreamReader, 8192);
			String line = "";
			StringBuffer resultContent = new StringBuffer();
			while ((line = bufferReader.readLine()) != null) {
				if(DEBUG) Log.d(TAG, line);
				resultContent.append(line);
			}
			bufferReader.close();
			
			if(DEBUG) Log.d(TAG, "readFromUrl() result: " + resultContent.toString());
			
			return resultContent.toString();
		} catch (ProtocolException e) {
			e.printStackTrace();
		} catch (IOException e) {
			e.printStackTrace();
		}

		return null;
	}

	/**
	 * Return the bucket number of the user. There are 100 possible buckets.
	 */
	private static int getUserBucket(Context c) {
		//get uuid
		DeviceUuidFactory df = new DeviceUuidFactory(c);
		String uuid = df.getDeviceUuid().toString();

		CRC32 crc = new CRC32();
		crc.update(uuid.getBytes());
		long checksum = crc.getValue();
		return (int)(checksum % 100L);
	}
}
