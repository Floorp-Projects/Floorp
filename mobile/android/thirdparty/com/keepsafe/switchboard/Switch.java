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

import org.json.JSONObject;

import android.content.Context;

/**
 * Single instance of an existing experiment for easier and cleaner code.
 * 
 * @author Philipp Berner
 *
 */
public class Switch {

	private Context context;
	private String experimentName;
	
	/**
	 * Creates an instance of a single experiment to give more convenient access to its values.
	 * When the given experiment does not exist, it will give back default valued that can be found 
	 * in <code>Switchboard</code>. Developer has to know that experiment exists when using it.
	 * @param c Application context
	 * @param experimentName Name of the experiment as defined on the server
	 */
	public Switch(Context c, String experimentName) {
		this.context = c;
		this.experimentName = experimentName;
	}
	
	/**
	 * Returns true if the experiment is active for this particular user.
	 * @return Status of the experiment and false when experiment does not exist.
	 */
	public boolean isActive() {
		return SwitchBoard.isInExperiment(context, experimentName);
	}
	
	/** 
	 * Returns the status of the experiment or the given default value when experiment
	 * does not exist.
	 * @param defaultValue Value to return when experiment does not exist.
	 * @return Experiment status
	 */
	public boolean isActive(boolean defaultValue) {
		return SwitchBoard.isInExperiment(context, experimentName, defaultValue);
	}
	
	/**
	 * Returns true if the experiment has aditional values.
	 * @return true when values exist
	 */
	public boolean hasValues() {
		return SwitchBoard.hasExperimentValues(context, experimentName);
	}
	
	/**
	 * Gives back all the experiment values in a JSONObject. This function checks if
	 * values exists. If no values exist, it returns null.
	 * @return Values in JSONObject or null if non
	 */
	public JSONObject getValues() {
		if(hasValues())
			return SwitchBoard.getExperimentValueFromJson(context, experimentName);
		else
			return null;
	}
}
