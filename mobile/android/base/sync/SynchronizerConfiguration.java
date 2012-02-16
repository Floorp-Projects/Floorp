/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Android Sync Client.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Richard Newman <rnewman@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

package org.mozilla.gecko.sync;

import java.io.IOException;

import org.json.simple.parser.ParseException;
import org.mozilla.gecko.sync.SyncConfiguration.ConfigurationBranch;
import org.mozilla.gecko.sync.repositories.RepositorySessionBundle;

import android.content.SharedPreferences.Editor;
import android.util.Log;

public class SynchronizerConfiguration {
  private static final String LOG_TAG = "SynczrConfiguration";

  public String syncID;
  public RepositorySessionBundle remoteBundle;
  public RepositorySessionBundle localBundle;

  public SynchronizerConfiguration(ConfigurationBranch config) throws NonObjectJSONException, IOException, ParseException {
    this.load(config);
  }

  public SynchronizerConfiguration(String syncID, RepositorySessionBundle remoteBundle, RepositorySessionBundle localBundle) {
    this.syncID       = syncID;
    this.remoteBundle = remoteBundle;
    this.localBundle  = localBundle;
  }

  public String[] toStringValues() {
    String[] out = new String[3];
    out[0] = syncID;
    out[1] = remoteBundle.toJSONString();
    out[2] = localBundle.toJSONString();
    return out;
  }

  // This should get partly shuffled back into SyncConfiguration, I think.
  public void load(ConfigurationBranch config) throws NonObjectJSONException, IOException, ParseException {
    if (config == null) {
      throw new IllegalArgumentException("config cannot be null.");
    }
    String remoteJSON = config.getString("remote", null);
    String localJSON  = config.getString("local",  null);
    RepositorySessionBundle rB = new RepositorySessionBundle(remoteJSON);
    RepositorySessionBundle lB = new RepositorySessionBundle(localJSON);
    if (remoteJSON == null) {
      rB.setTimestamp(0);
    }
    if (localJSON == null) {
      lB.setTimestamp(0);
    }
    syncID = config.getString("syncID", null);
    remoteBundle = rB;
    localBundle  = lB;
    Log.i(LOG_TAG, "Initialized SynchronizerConfiguration. syncID: " + syncID + ", remoteBundle: " + remoteBundle + ", localBundle: " + localBundle);
  }

  public void persist(ConfigurationBranch config) {
    if (config == null) {
      throw new IllegalArgumentException("config cannot be null.");
    }
    String jsonRemote = remoteBundle.toJSONString();
    String jsonLocal  = localBundle.toJSONString();
    Editor editor = config.edit();
    editor.putString("remote", jsonRemote);
    editor.putString("local",  jsonLocal);
    editor.putString("syncID", syncID);

    // Synchronous.
    editor.commit();
  }
}
