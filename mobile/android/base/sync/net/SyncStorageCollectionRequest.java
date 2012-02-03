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
            SyncResourceDelegate.consumeEntity(entity);
            return;
          }
        }
      } catch (IOException ex) {
        delegate.handleRequestError(ex);
        SyncResourceDelegate.consumeEntity(entity);
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
      SyncResourceDelegate.consumeEntity(entity);
      delegate.handleRequestSuccess(new SyncStorageResponse(response));
    }
  }
}