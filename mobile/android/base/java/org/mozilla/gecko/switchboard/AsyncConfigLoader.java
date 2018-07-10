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


import android.content.Context;
import android.os.AsyncTask;

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

    private Context context;
    private String defaultServerUrl;
    private SwitchBoard.ConfigStatusListener listener;

    /**
     * Sets the params for async loading either SwitchBoard.updateConfigServerUrl()
     * or SwitchBoard.loadConfig.
     * Loads config with a custom UUID
     * @param c Application context
     * @param defaultServerUrl Default URL endpoint for Switchboard config.
     */
    public AsyncConfigLoader(Context c, String defaultServerUrl,
                             SwitchBoard.ConfigStatusListener listener) {
        this.context = c;
        this.defaultServerUrl = defaultServerUrl;
        this.listener = listener;
    }

    @Override
    protected Void doInBackground(Void... params) {
        SwitchBoard.loadConfig(context, defaultServerUrl, listener);
        return null;
    }
}
