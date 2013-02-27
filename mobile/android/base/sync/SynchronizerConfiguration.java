/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync;

import java.io.IOException;

import org.json.simple.parser.ParseException;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.SyncConfiguration.ConfigurationBranch;
import org.mozilla.gecko.sync.repositories.RepositorySessionBundle;

import android.content.SharedPreferences.Editor;

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
    Logger.debug(LOG_TAG, "Loaded SynchronizerConfiguration. syncID: " + syncID + ", remoteBundle: " + remoteBundle + ", localBundle: " + localBundle);
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
    Logger.debug(LOG_TAG, "Persisted SynchronizerConfiguration. syncID: " + syncID + ", remoteBundle: " + remoteBundle + ", localBundle: " + localBundle);
  }
}
