/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.stage;

import android.os.SystemClock;

import java.net.URISyntaxException;

import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.HTTPFailureException;
import org.mozilla.gecko.sync.InfoCollections;
import org.mozilla.gecko.sync.delegates.JSONRecordFetchDelegate;
import org.mozilla.gecko.sync.net.SyncStorageResponse;
import org.mozilla.gecko.sync.telemetry.TelemetryCollector;

public class FetchInfoCollectionsStage extends AbstractNonRepositorySyncStage {
  public class StageInfoCollectionsDelegate implements JSONRecordFetchDelegate {

    @Override
    public void handleSuccess(ExtendedJSONObject global) {
      session.config.infoCollections = new InfoCollections(global);
      telemetryStageCollector.finished = SystemClock.elapsedRealtime();
      session.advance();
    }

    @Override
    public void handleFailure(SyncStorageResponse response) {
      telemetryStageCollector.finished = SystemClock.elapsedRealtime();
      telemetryStageCollector.error = new TelemetryCollector.StageErrorBuilder()
              .setLastException(new HTTPFailureException(response))
              .build();
      session.handleHTTPError(response, "Failure fetching info/collections.");
    }

    @Override
    public void handleError(Exception e) {
      telemetryStageCollector.finished = SystemClock.elapsedRealtime();
      telemetryStageCollector.error = new TelemetryCollector.StageErrorBuilder()
              .setLastException(e)
              .build();
      session.abort(e, "Failure fetching info/collections.");
    }

  }

  @Override
  public void execute() throws NoSuchStageException {
    try {
      session.fetchInfoCollections(new StageInfoCollectionsDelegate());
    } catch (URISyntaxException e) {
      telemetryStageCollector.finished = SystemClock.elapsedRealtime();
      telemetryStageCollector.error = new TelemetryCollector.StageErrorBuilder()
              .setLastException(e)
              .build();
      session.abort(e, "Invalid URI.");
    }
  }

}
