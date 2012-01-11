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

package org.mozilla.gecko.sync.synchronizer;

import org.mozilla.gecko.sync.repositories.Repository;
import org.mozilla.gecko.sync.repositories.RepositorySessionBundle;

import android.content.Context;
import android.util.Log;

/**
 * This, sunshine, is where the magic happens.
 *
 * I hope for all our sakes that it's bug-free.
 *
 * @author rnewman
 *
 */
public class Synchronizer {

  /**
   * Wrap a SynchronizerDelegate in a SynchronizerSessionDelegate.
   * Also handle communication of bundled data.
   *
   * @author rnewman
   */
  public class SynchronizerDelegateSessionDelegate implements
      SynchronizerSessionDelegate {

    private static final String LOG_TAG = "SynchronizerDelegateSessionDelegate";
    private SynchronizerDelegate synchronizerDelegate;
    private SynchronizerSession  session;

    public SynchronizerDelegateSessionDelegate(SynchronizerDelegate delegate) {
      this.synchronizerDelegate = delegate;
    }

    @Override
    public void onInitialized(SynchronizerSession session) {
      this.session = session;
      session.synchronize();
    }

    @Override
    public void onSynchronized(SynchronizerSession session) {
      Log.d(LOG_TAG, "Got onSynchronized.");
      Log.d(LOG_TAG, "Notifying SynchronizerDelegate.");
      this.synchronizerDelegate.onSynchronized(session.getSynchronizer());
    }

    @Override
    public void onSynchronizeFailed(SynchronizerSession session,
                                    Exception lastException, String reason) {
      this.synchronizerDelegate.onSynchronizeFailed(session.getSynchronizer(), lastException, reason);
    }

    @Override
    public void onSynchronizeAborted(SynchronizerSession synchronizerSession) {
      this.synchronizerDelegate.onSynchronizeAborted(session.getSynchronizer());
    }

    @Override
    public void onFetchError(Exception e) {
      session.abort();
      synchronizerDelegate.onSynchronizeFailed(session.getSynchronizer(), e, "Got fetch error.");
    }

    @Override
    public void onStoreError(Exception e) {
      session.abort();
      synchronizerDelegate.onSynchronizeFailed(session.getSynchronizer(), e, "Got store error.");
    }

    @Override
    public void onSessionError(Exception e) {
      session.abort();
      synchronizerDelegate.onSynchronizeFailed(session.getSynchronizer(), e, "Got session error.");
    }
  }

  public Repository repositoryA;
  public Repository repositoryB;
  public RepositorySessionBundle bundleA;
  public RepositorySessionBundle bundleB;

  public void synchronize(Context context, SynchronizerDelegate delegate) {
    SynchronizerDelegateSessionDelegate sessionDelegate = new SynchronizerDelegateSessionDelegate(delegate);
    SynchronizerSession session = new SynchronizerSession(this, sessionDelegate);
    session.init(context, bundleA, bundleB);
  }
}
