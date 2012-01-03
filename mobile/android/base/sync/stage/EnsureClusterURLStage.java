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
 *  Richard Newman <rnewman@mozilla.com>
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

package org.mozilla.gecko.sync.stage;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.net.URI;
import java.net.URISyntaxException;
import java.security.GeneralSecurityException;

import org.mozilla.gecko.sync.GlobalSession;
import org.mozilla.gecko.sync.ThreadPool;
import org.mozilla.gecko.sync.net.BaseResource;
import org.mozilla.gecko.sync.net.SyncResourceDelegate;

import android.util.Log;
import ch.boye.httpclientandroidlib.HttpEntity;
import ch.boye.httpclientandroidlib.HttpResponse;
import ch.boye.httpclientandroidlib.client.ClientProtocolException;

public class EnsureClusterURLStage implements GlobalSyncStage {
  public interface ClusterURLFetchDelegate {
    public void handleSuccess(String url);
    public void handleFailure(HttpResponse response);
    public void handleError(Exception e);
  }

  protected static final String LOG_TAG = "EnsureClusterURLStage";

  // TODO: if cluster URL has changed since last time, we need to ensure that we do
  // a fresh start. This takes place at the GlobalSession level. Verify!
  public static void fetchClusterURL(final GlobalSession session,
                                     final ClusterURLFetchDelegate delegate) throws URISyntaxException {
    Log.i(LOG_TAG, "In fetchClusterURL. Server URL is " + session.config.serverURL);
    String nodeWeaveURL = session.config.nodeWeaveURL();
    Log.d(LOG_TAG, "node/weave is " + nodeWeaveURL);

    BaseResource resource = new BaseResource(nodeWeaveURL);
    resource.delegate = new SyncResourceDelegate(resource) {

      @Override
      public void handleHttpResponse(HttpResponse response) {
        int status = response.getStatusLine().getStatusCode();
        switch (status) {
        case 200:
          Log.i(LOG_TAG, "Got 200 for node/weave fetch.");
          // Great!
          HttpEntity entity = response.getEntity();
          if (entity == null) {
            delegate.handleSuccess(null);
            return;
          }
          String output = null;
          try {
            InputStream content = entity.getContent();
            BufferedReader reader = new BufferedReader(new InputStreamReader(content, "UTF-8"), 1024);
            output = reader.readLine();
            SyncResourceDelegate.consumeReader(reader);
            reader.close();
          } catch (IllegalStateException e) {
            delegate.handleError(e);
          } catch (IOException e) {
            delegate.handleError(e);
          }

          if (output == null || output.equals("null")) {
            delegate.handleSuccess(null);
          }
          delegate.handleSuccess(output);
          break;
        case 400:
        case 404:
          Log.i(LOG_TAG, "Got " + status + " for cluster URL request.");
          delegate.handleFailure(response);
          SyncResourceDelegate.consumeEntity(response.getEntity());
          break;
        default:
          Log.w(LOG_TAG, "Got " + status + " fetching node/weave. Returning failure.");
          delegate.handleFailure(response);
          SyncResourceDelegate.consumeEntity(response.getEntity());
        }
      }

      @Override
      public void handleHttpProtocolException(ClientProtocolException e) {
        delegate.handleError(e);
      }

      @Override
      public void handleHttpIOException(IOException e) {
        delegate.handleError(e);
      }

      @Override
      public void handleTransportException(GeneralSecurityException e) {
        delegate.handleError(e);
      }
    };

    resource.get();
  }

  public void execute(final GlobalSession session) throws NoSuchStageException {

    if (session.config.clusterURL != null) {
      Log.i(LOG_TAG, "Cluster URL already set. Continuing with sync.");
      session.advance();
      return;
    }

    Log.i(LOG_TAG, "Fetching cluster URL.");
    final ClusterURLFetchDelegate delegate = new ClusterURLFetchDelegate() {

      @Override
      public void handleSuccess(final String url) {
        Log.i(LOG_TAG, "Node assignment pointed us to " + url);

        try {
          session.config.setClusterURL(url);
          ThreadPool.run(new Runnable() {
            @Override
            public void run() {
              session.advance();
            }
          });
          return;
        } catch (URISyntaxException e) {
          final URISyntaxException uriException = e;
          ThreadPool.run(new Runnable() {
            @Override
            public void run() {
              session.abort(uriException, "Invalid cluster URL.");
            }
          });
        }
      }

      @Override
      public void handleFailure(HttpResponse response) {
        int statusCode = response.getStatusLine().getStatusCode();
        Log.w(LOG_TAG, "Got HTTP failure fetching node assignment: " + statusCode);
        if (statusCode == 404) {
          URI serverURL = session.config.serverURL;
          if (serverURL == null) {
            Log.w(LOG_TAG, "No serverURL set to use as fallback cluster URL. Aborting sync.");
            session.abort(new Exception("HTTP failure."), "Got failure fetching cluster URL.");
            return;
          }
          Log.i(LOG_TAG, "Using serverURL <" + serverURL.toASCIIString() + "> as clusterURL.");
          session.config.clusterURL = serverURL;
        }
      }

      @Override
      public void handleError(Exception e) {
        session.abort(e, "Got exception fetching cluster URL.");
      }
    };

    ThreadPool.run(new Runnable() {
      @Override
      public void run() {
        try {
          fetchClusterURL(session, delegate);
        } catch (URISyntaxException e) {
          session.abort(e, "Invalid URL for node/weave.");
        }
      }});
  }
}
