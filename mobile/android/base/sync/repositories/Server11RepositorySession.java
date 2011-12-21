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

package org.mozilla.gecko.sync.repositories;

import java.net.URI;
import java.net.URISyntaxException;
import java.util.Date;

import org.mozilla.gecko.sync.crypto.KeyBundle;
import org.mozilla.gecko.sync.CryptoRecord;
import org.mozilla.gecko.sync.DelayedWorkTracker;
import org.mozilla.gecko.sync.HTTPFailureException;
import org.mozilla.gecko.sync.net.SyncStorageCollectionRequest;
import org.mozilla.gecko.sync.net.SyncStorageRequestDelegate;
import org.mozilla.gecko.sync.net.SyncStorageResponse;
import org.mozilla.gecko.sync.net.WBOCollectionRequestDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFetchRecordsDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionGuidsSinceDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionStoreDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionWipeDelegate;
import org.mozilla.gecko.sync.repositories.domain.Record;

import android.util.Log;

public class Server11RepositorySession extends RepositorySession {

  public static final String LOG_TAG = "Server11RepositorySession";

  /**
   * Convert HTTP request delegate callbacks into fetch callbacks within the
   * context of this RepositorySession.
   *
   * @author rnewman
   *
   */
  public class RequestFetchDelegateAdapter extends WBOCollectionRequestDelegate {
    RepositorySessionFetchRecordsDelegate delegate;
    private DelayedWorkTracker workTracker = new DelayedWorkTracker();

    public RequestFetchDelegateAdapter(RepositorySessionFetchRecordsDelegate delegate) {
      this.delegate = delegate;
    }

    @Override
    public String credentials() {
      return serverRepository.credentialsSource.credentials();
    }

    @Override
    public String ifUnmodifiedSince() {
      return null;
    }

    @Override
    public void handleRequestSuccess(SyncStorageResponse response) {
      Log.i(LOG_TAG, "Fetch done.");

      long normalizedTimestamp = -1;
      try {
        normalizedTimestamp = response.normalizedWeaveTimestamp();
      } catch (NumberFormatException e) {
        Log.w(LOG_TAG, "Malformed X-Weave-Timestamp header received.", e);
      }
      if (-1 == normalizedTimestamp) {
        Log.w(LOG_TAG, "Computing stand-in timestamp from local clock. Clock drift could cause records to be skipped.");
        normalizedTimestamp = new Date().getTime();
      }

      Log.d(LOG_TAG, "Fetch completed. Timestamp is " + normalizedTimestamp);
      final long ts = normalizedTimestamp;

      // When we're done processing other events, finish.
      workTracker.delayWorkItem(new Runnable() {
        @Override
        public void run() {
          Log.d(LOG_TAG, "Delayed onFetchCompleted running.");
          // TODO: verify number of returned records.
          delegate.onFetchCompleted(ts);
        }
      });
    }

    @Override
    public void handleRequestFailure(SyncStorageResponse response) {
      // TODO: ensure that delegate methods don't get called more than once.
      this.handleRequestError(new HTTPFailureException(response));
    }

    @Override
    public void handleRequestError(final Exception ex) {
      Log.i(LOG_TAG, "Got request error.", ex);
      // When we're done processing other events, finish.
      workTracker.delayWorkItem(new Runnable() {
        @Override
        public void run() {
          Log.i(LOG_TAG, "Running onFetchFailed.");
          delegate.onFetchFailed(ex, null);
        }
      });
    }

    @Override
    public void handleWBO(CryptoRecord record) {
      workTracker.incrementOutstanding();
      try {
        delegate.onFetchedRecord(record);
      } catch (Exception ex) {
        Log.i(LOG_TAG, "Got exception calling onFetchedRecord with WBO.", ex);
        // TODO: handle this better.
        throw new RuntimeException(ex);
      } finally {
        workTracker.decrementOutstanding();
      }
    }

    // TODO: this implies that we've screwed up our inheritance chain somehow.
    @Override
    public KeyBundle keyBundle() {
      return null;
    }
  }

  Server11Repository serverRepository;
  public Server11RepositorySession(Repository repository) {
    super(repository);
    serverRepository = (Server11Repository) repository;
  }

  private String flattenIDs(String[] guids) {
    if (guids.length == 0) {
      return "";
    }
    if (guids.length == 1) {
      return guids[0];
    }
    StringBuilder b = new StringBuilder();
    for (String guid : guids) {
      b.append(guid);
      b.append(",");
    }
    return b.substring(0, b.length() - 1);
  }

  @Override
  public void guidsSince(long timestamp,
                         RepositorySessionGuidsSinceDelegate delegate) {
    // TODO Auto-generated method stub

  }

  private void fetchWithParameters(long newer,
                                   boolean full,
                                   String ids,
                                   SyncStorageRequestDelegate delegate) throws URISyntaxException {

    URI collectionURI = serverRepository.collectionURI(full, newer, ids);
    SyncStorageCollectionRequest request = new SyncStorageCollectionRequest(collectionURI);
    request.delegate = delegate;
    request.get();
  }

  @Override
  public void fetchSince(long timestamp,
                         RepositorySessionFetchRecordsDelegate delegate) {
    try {
      this.fetchWithParameters(timestamp, true, null, new RequestFetchDelegateAdapter(delegate));
    } catch (URISyntaxException e) {
      delegate.onFetchFailed(e, null);
    }
  }

  @Override
  public void fetchAll(RepositorySessionFetchRecordsDelegate delegate) {
    this.fetchSince(-1, delegate);
  }

  @Override
  public void fetch(String[] guids,
                    RepositorySessionFetchRecordsDelegate delegate) {
    // TODO: watch out for URL length limits!
    try {
      String ids = flattenIDs(guids);
      this.fetchWithParameters(-1, true, ids, new RequestFetchDelegateAdapter(delegate));
    } catch (URISyntaxException e) {
      delegate.onFetchFailed(e, null);
    }
  }

  @Override
  public void store(Record record, RepositorySessionStoreDelegate delegate) {
    // TODO: implement store.
  }

  @Override
  public void wipe(RepositorySessionWipeDelegate delegate) {
    // TODO: implement wipe.
  }
}
