/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.net;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.net.URI;

import ch.boye.httpclientandroidlib.Header;
import ch.boye.httpclientandroidlib.HttpEntity;
import ch.boye.httpclientandroidlib.HttpResponse;
import ch.boye.httpclientandroidlib.client.methods.HttpRequestBase;
import ch.boye.httpclientandroidlib.impl.client.DefaultHttpClient;

/**
 * A request class that handles line-by-line responses. Eventually this will
 * handle real stream processing; for now, just parse the returned body
 * line-by-line.
 *
 * @author rnewman
 *
 */
public class SyncStorageCollectionRequest extends SyncStorageRequest {
  public SyncStorageCollectionRequest(URI uri) {
    super(uri);
  }

  @Override
  protected SyncResourceDelegate makeResourceDelegate(SyncStorageRequest request) {
    return new SyncCollectionResourceDelegate((SyncStorageCollectionRequest) request);
  }

  // TODO: this is awful.
  public class SyncCollectionResourceDelegate extends
      SyncStorageResourceDelegate {

    SyncCollectionResourceDelegate(SyncStorageCollectionRequest request) {
      super(request);
    }

    @Override
    public void addHeaders(HttpRequestBase request, DefaultHttpClient client) {
      super.addHeaders(request, client);
      request.setHeader("Accept", "application/newlines");
      // Caller is responsible for setting full=1.
    }

    @Override
    public void handleHttpResponse(HttpResponse response) {
      if (response.getStatusLine().getStatusCode() != 200) {
        super.handleHttpResponse(response);
        return;
      }

      HttpEntity entity = response.getEntity();
      Header contentType = entity.getContentType();
      System.out.println("content type is " + contentType.getValue());
      if (!contentType.getValue().startsWith("application/newlines")) {
        // Not incremental!
        super.handleHttpResponse(response);
        return;
      }

      // TODO: at this point we can access X-Weave-Timestamp, compare
      // that to our local timestamp, and compute an estimate of clock
      // skew. We can provide this to the incremental delegate, which
      // will allow it to seamlessly correct timestamps on the records
      // it processes. Bug 721887.

      // Line-by-line processing, then invoke success.
      SyncStorageCollectionRequestDelegate delegate = (SyncStorageCollectionRequestDelegate) this.request.delegate;
      InputStream content = null;
      BufferedReader br = null;
      try {
        content = entity.getContent();
        int bufSize = 1024 * 1024;         // 1MB. TODO: lift and consider.
        br = new BufferedReader(new InputStreamReader(content), bufSize);
        String line;

        // This relies on connection timeouts at the HTTP layer.
        while (null != (line = br.readLine())) {
          try {
            delegate.handleRequestProgress(line);
          } catch (Exception ex) {
            delegate.handleRequestError(new HandleProgressException(ex));
            BaseResource.consumeEntity(entity);
            return;
          }
        }
      } catch (IOException ex) {
        delegate.handleRequestError(ex);
        BaseResource.consumeEntity(entity);
        return;
      } finally {
        // Attempt to close the stream and reader.
        if (br != null) {
          try {
            br.close();
          } catch (IOException e) {
            // We don't care if this fails.
          }
        }
      }
      // We're done processing the entity. Don't let fetching the body succeed!
      BaseResource.consumeEntity(entity);
      delegate.handleRequestSuccess(new SyncStorageResponse(response));
    }
  }
}