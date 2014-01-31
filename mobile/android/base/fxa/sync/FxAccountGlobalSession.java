/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.sync;

import java.io.IOException;
import java.net.URI;
import java.net.URISyntaxException;
import java.util.Collections;
import java.util.HashMap;

import org.json.simple.parser.ParseException;
import org.mozilla.gecko.fxa.FxAccountConstants;
import org.mozilla.gecko.sync.GlobalSession;
import org.mozilla.gecko.sync.NonObjectJSONException;
import org.mozilla.gecko.sync.SyncConfiguration;
import org.mozilla.gecko.sync.SyncConfigurationException;
import org.mozilla.gecko.sync.delegates.BaseGlobalSessionCallback;
import org.mozilla.gecko.sync.delegates.ClientsDataDelegate;
import org.mozilla.gecko.sync.stage.CheckPreconditionsStage;
import org.mozilla.gecko.sync.stage.GlobalSyncStage;
import org.mozilla.gecko.sync.stage.GlobalSyncStage.Stage;

import android.content.Context;
import android.os.Bundle;

public class FxAccountGlobalSession extends GlobalSession {
  private static final String LOG_TAG = FxAccountGlobalSession.class.getSimpleName();

  public FxAccountGlobalSession(String storageEndpoint, SyncConfiguration config, BaseGlobalSessionCallback callback,
      Context context, Bundle extras, ClientsDataDelegate clientsDelegate)
      throws SyncConfigurationException, IllegalArgumentException, IOException,
      ParseException, NonObjectJSONException, URISyntaxException {
    super(config, callback, context, extras, clientsDelegate, null);
    URI storageURI = new URI(storageEndpoint);
    this.config.setClusterURL(storageURI);
    FxAccountConstants.pii(LOG_TAG, "clusterURL is " + config.getClusterURLString());
  }

  @Override
  public void prepareStages() {
    super.prepareStages();
    HashMap<Stage, GlobalSyncStage> stages = new HashMap<Stage, GlobalSyncStage>();
    stages.putAll(this.stages);
    stages.put(Stage.ensureClusterURL, new CheckPreconditionsStage());
    this.stages = Collections.unmodifiableMap(stages);
  }
}
