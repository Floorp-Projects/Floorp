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
import android.os.AsyncTask;
import android.util.Log;

/**
 * An async loader to load user config in background thread based on internal generated UUID.
 * 
 * Call <code>AsyncConfigLoader.execute()</code> to load SwitchBoard.loadConfig() with own ID. 
 * To use your custom UUID call <code>AsyncConfigLoader.execute(uuid)</code> with uuid being your unique user id
 * as a String 
 *
 * @author Philipp Berner
 *
 */
public class AsyncConfigLoader extends AsyncTask<Void, Void, Void> {

	private String TAG = "AsyncConfigLoader";
	
	public static final int UPDATE_SERVER = 1;
	public static final int CONFIG_SERVER = 2;
	
	private Context context;
	private int configToLoad;
	private String uuid;
	
	/**
	 * Sets the params for async loading either SwitchBoard.updateConfigServerUrl()
	 * or SwitchBoard.loadConfig.
	 * @param c Application context
	 * @param configType Either UPDATE_SERVER or CONFIG_SERVER
	 */
	public AsyncConfigLoader(Context c, int configType) {
		this(c, configType, null);
	}
	
	/**
	 * Sets the params for async loading either SwitchBoard.updateConfigServerUrl()
	 * or SwitchBoard.loadConfig.
	 * Loads config with a custom UUID
	 * @param c Application context
	 * @param configType Either UPDATE_SERVER or CONFIG_SERVER
	 * @param uuid Custom UUID
	 */
	public AsyncConfigLoader(Context c, int configType, String uuid) {
		this.context = c;
		this.configToLoad = configType;
		this.uuid = uuid;
	}
	
	@Override
	protected Void doInBackground(Void... params) {
		
		if(configToLoad == UPDATE_SERVER) {
			SwitchBoard.updateConfigServerUrl(context);
		}
		else {
			if(uuid == null)
				SwitchBoard.loadConfig(context);
			else
				SwitchBoard.loadConfig(context, uuid);
		}
			
		return null;
	}
	
}