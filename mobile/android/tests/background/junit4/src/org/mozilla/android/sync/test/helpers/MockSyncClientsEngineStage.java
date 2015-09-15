/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.android.sync.test.helpers;

import static org.junit.Assert.assertTrue;

import org.mozilla.gecko.sync.net.BaseResource;
import org.mozilla.gecko.sync.net.SyncStorageResponse;
import org.mozilla.gecko.sync.stage.SyncClientsEngineStage;

public class MockSyncClientsEngineStage extends SyncClientsEngineStage {
  public class MockClientUploadDelegate extends ClientUploadDelegate {
    HTTPServerTestHelper data;

    public MockClientUploadDelegate(HTTPServerTestHelper data) {
      this.data = data;
    }

    @Override
    public void handleRequestSuccess(SyncStorageResponse response) {
      assertTrue(response.wasSuccessful());
      data.stopHTTPServer();
      super.handleRequestSuccess(response);
    }

    @Override
    public void handleRequestFailure(SyncStorageResponse response) {
      BaseResource.consumeEntity(response);
      data.stopHTTPServer();
      super.handleRequestFailure(response);
    }

    @Override
    public void handleRequestError(Exception ex) {
      ex.printStackTrace();
      data.stopHTTPServer();
      super.handleRequestError(ex);
    }
  }

  public class TestClientDownloadDelegate extends ClientDownloadDelegate {
    HTTPServerTestHelper data;

    public TestClientDownloadDelegate(HTTPServerTestHelper data) {
      this.data = data;
    }

    @Override
    public void handleRequestSuccess(SyncStorageResponse response) {
      assertTrue(response.wasSuccessful());
      BaseResource.consumeEntity(response);
      data.stopHTTPServer();
      super.handleRequestSuccess(response);
    }

    @Override
    public void handleRequestFailure(SyncStorageResponse response) {
      BaseResource.consumeEntity(response);
      super.handleRequestFailure(response);
      data.stopHTTPServer();
    }

    @Override
    public void handleRequestError(Exception ex) {
      ex.printStackTrace();
      super.handleRequestError(ex);
      data.stopHTTPServer();
    }
  }
}
