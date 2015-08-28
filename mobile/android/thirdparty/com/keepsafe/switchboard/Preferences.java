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


import android.content.Context;
import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;

/**
 * Application preferences for SwitchBoard.
 * @author Philipp Berner
 *
 */
public class Preferences {
	private static final String TAG = "Preferences";
	
	private static final String switchBoardSettings = "com.keepsafe.switchboard.settings";
	
	//dynamic config
	private static final String kDynamicConfigServerUrl = "dynamic-config-server-url";
	private static final String kDynamicConfigServerUpdateUrl = "dynamic-config-server-update-url";
	private static final String kDynamicConfig = "dynamic-config";
	
	

	//dynamic config
	/** TODO check this!!!
	 * Returns a JSON string array with <br />
	 * position 0 = updateserverUrl <br />
	 * Fields a null if not existent.
	 * @param c
	 * @return
	 */
	public static String getDynamicUpdateServerUrl(Context c) {
		SharedPreferences settings = (SharedPreferences) Preferences.getPreferenceObject(c, false);
		return settings.getString(kDynamicConfigServerUpdateUrl, null);
	}
	
	/**
	 * Returns a JSON string array with <br />
	 * postiion 1 = configServerUrl <br />
	 * Fields a null if not existent.
	 * @param c
	 * @return
	 */
	public static String getDynamicConfigServerUrl(Context c) {
		SharedPreferences settings = (SharedPreferences) Preferences.getPreferenceObject(c, false);
		return settings.getString(kDynamicConfigServerUrl, null);
	}

	/**
	 * Stores the config servers URL. 
	 * @param c
	 * @param updateServerUrl Url end point to get the current config server location
	 * @param configServerUrl UR: end point to get the current endpoint for the apps config file
	 * @return true if saved successful
	 */
	public static boolean setDynamicConfigServerUrl(Context c, String updateServerUrl, String configServerUrl) {
	
		SharedPreferences.Editor settings = (Editor) Preferences.getPreferenceObject(c, true);
		settings.putString(kDynamicConfigServerUpdateUrl, updateServerUrl);
		settings.putString(kDynamicConfigServerUrl, configServerUrl);
		return settings.commit();
	}
	
	/**
	 * Gets the user config as a JSON string.
	 * @param c
	 * @return
	 */
	public static String getDynamicConfigJson(Context c) {
		SharedPreferences settings = (SharedPreferences) Preferences.getPreferenceObject(c, false);
		return settings.getString(kDynamicConfig, null);
	}

	/**
	 * Saves the user config as a JSON sting.
	 * @param c
	 * @param configJson
	 * @return
	 */
	public static boolean setDynamicConfigJson(Context c, String configJson) {
		SharedPreferences.Editor settings = (Editor) Preferences.getPreferenceObject(c, true);
		settings.putString(kDynamicConfig, configJson);
		return settings.commit();
	}

	static private Object getPreferenceObject(Context ctx, boolean writeable) {
		
		Object returnValue = null;
		
		Context sharedDelegate = ctx.getApplicationContext();
		
		if(!writeable) {
			returnValue = sharedDelegate.getSharedPreferences(switchBoardSettings, Context.MODE_PRIVATE);
		} else {
			returnValue = sharedDelegate.getSharedPreferences(switchBoardSettings, Context.MODE_PRIVATE).edit();
		}
		
		return returnValue;
	}
}
