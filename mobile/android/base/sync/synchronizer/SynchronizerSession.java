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


import java.util.concurrent.ExecutorService;

import org.mozilla.gecko.sync.repositories.InactiveSessionException;
import org.mozilla.gecko.sync.repositories.InvalidSessionTransitionException;
import org.mozilla.gecko.sync.repositories.RepositorySession;
import org.mozilla.gecko.sync.repositories.RepositorySessionBundle;
import org.mozilla.gecko.sync.repositories.delegates.DeferrableRepositorySessionCreationDelegate;
import org.mozilla.gecko.sync.repositories.delegates.DeferredRepositorySessionFinishDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFinishDelegate;

import android.content.Context;
import android.util.Log;

public class SynchronizerSession
extends DeferrableRepositorySessionCreationDelegate
implements RecordsChannelDelegate,
           RepositorySessionFinishDelegate {

  protected static final String LOG_TAG = "SynchronizerSession";
  private Synchronizer synchronizer;
  private SynchronizerSessionDelegate delegate;
  private Context context;

  /*
   * Computed during init.
   */
  private RepositorySession sessionA;
  private RepositorySession sessionB;
  private RepositorySessionBundle bundleA;
  private RepositorySessionBundle bundleB;

  // Bug 726054: just like desktop, we track our last interaction with the server,
  // not the last record timestamp that we fetched. This ensures that we don't re-
  // download the records we just uploaded, at the cost of skipping any records
  // that a concurrently syncing client has uploaded.
  private long pendingATimestamp = -1;
  private long pendingBTimestamp = -1;
  private long storeEndATimestamp = -1;
  private long storeEndBTimestamp = -1;
  private boolean flowAToBCompleted = false;
  private boolean flowBToACompleted = false;

  private static void warn(String msg, Exception e) {
    System.out.println("WARN: " + msg);
    e.printStackTrace(System.err);
    Log.w(LOG_TAG, msg, e);
  }
  private static void warn(String msg) {
    System.out.println("WARN: " + msg);
    Log.w(LOG_TAG, msg);
  }
  private static void info(String msg) {
    System.out.println("INFO: " + msg);
    Log.i(LOG_TAG, msg);
  }

  /*
   * Public API: constructor, init, synchronize.
   */
  public SynchronizerSession(Synchronizer synchronizer, SynchronizerSessionDelegate delegate) {
    this.setSynchronizer(synchronizer);
    this.delegate = delegate;
  }

  public Synchronizer getSynchronizer() {
    return synchronizer;
  }

  public void setSynchronizer(Synchronizer synchronizer) {
    this.synchronizer = synchronizer;
  }

  public void init(Context context, RepositorySessionBundle bundleA, RepositorySessionBundle bundleB) {
    this.context = context;
    this.bundleA = bundleA;
    this.bundleB = bundleB;
    // Begin sessionA and sessionB, call onInitialized in callbacks.
    this.getSynchronizer().repositoryA.createSession(this, context);
  }

  // TODO: implement aborting.
  public void abort() {
    this.delegate.onSynchronizeAborted(this);
  }

  /**
   * Please don't call this until you've been notified with onInitialized.
   */
  public void synchronize() {
    // First thing: decide whether we should.
    if (!sessionA.dataAvailable() &&
        !sessionB.dataAvailable()) {
      info("Neither session reports data available. Short-circuiting sync.");
      sessionA.abort();
      sessionB.abort();
      this.delegate.onSynchronizeSkipped(this);
      return;
    }

    final SynchronizerSession session = this;

    // TODO: failed record handling.
    final RecordsChannel channelBToA = new RecordsChannel(this.sessionB, this.sessionA, this);
    RecordsChannelDelegate channelDelegate = new RecordsChannelDelegate() {
      public void onFlowCompleted(RecordsChannel recordsChannel, long fetchEnd, long storeEnd) {
        info("First RecordsChannel flow completed. Fetch end is " + fetchEnd +
             ". Store end is " + storeEnd + ". Starting next.");
        pendingATimestamp = fetchEnd;
        storeEndBTimestamp = storeEnd;
        flowAToBCompleted = true;
        channelBToA.flow();
      }

      @Override
      public void onFlowBeginFailed(RecordsChannel recordsChannel, Exception ex) {
        warn("First RecordsChannel flow failed to begin.");
        session.delegate.onSessionError(ex);
      }

      @Override
      public void onFlowStoreFailed(RecordsChannel recordsChannel, Exception ex) {
        // TODO: clean up, tear down, abort.
        warn("First RecordsChannel flow failed.");
        session.delegate.onStoreError(ex);
      }

      @Override
      public void onFlowFinishFailed(RecordsChannel recordsChannel, Exception ex) {
        warn("onFlowFinishedFailed. Reporting store error.", ex);
        session.delegate.onStoreError(ex);
      }
    };
    final RecordsChannel channelAToB = new RecordsChannel(this.sessionA, this.sessionB, channelDelegate);
    info("Starting A to B flow. Channel is " + channelAToB);
    try {
      channelAToB.beginAndFlow();
    } catch (InvalidSessionTransitionException e) {
      onFlowBeginFailed(channelAToB, e);
    }
  }

  @Override
  public void onFlowCompleted(RecordsChannel channel, long fetchEnd, long storeEnd) {
    info("Second RecordsChannel flow completed. Fetch end is " + fetchEnd +
         ". Store end is " + storeEnd + ". Finishing.");

    pendingBTimestamp = fetchEnd;
    storeEndATimestamp = storeEnd;
    flowBToACompleted = true;

    // Finish the two sessions.
    try {
      this.sessionA.finish(this);
    } catch (InactiveSessionException e) {
      this.onFinishFailed(e);
    }
  }

  @Override
  public void onFlowBeginFailed(RecordsChannel recordsChannel, Exception ex) {
    warn("Second RecordsChannel flow failed to begin.", ex);
  }

  @Override
  public void onFlowStoreFailed(RecordsChannel recordsChannel, Exception ex) {
    // TODO Auto-generated method stub
    warn("Second RecordsChannel flow failed.");
  }

  @Override
  public void onFlowFinishFailed(RecordsChannel recordsChannel, Exception ex) {
    // TODO Auto-generated method stub
    warn("First RecordsChannel flow failed to finish.");
  }


  /*
   * RepositorySessionCreationDelegate methods.
   */
  @Override
  public void onSessionCreateFailed(Exception ex) {
    // Attempt to finish the first session, if the second is the one that failed.
    if (this.sessionA != null) {
      try {
        // We no longer need a reference to our context.
        this.context = null;
        this.sessionA.finish(this);
      } catch (Exception e) {
        // Never mind; best-effort finish.
      }
    }
    // We no longer need a reference to our context.
    this.context = null;
    this.delegate.onSessionError(ex);
  }

  // TODO: some of this "finish and clean up" code can be refactored out.
  @Override
  public void onSessionCreated(RepositorySession session) {
    if (session == null ||
        this.sessionA == session) {
      // TODO: clean up sessionA.
      this.delegate.onSessionError(new UnexpectedSessionException(session));
      return;
    }
    if (this.sessionA == null) {
      this.sessionA = session;

      // Unbundle.
      try {
        this.sessionA.unbundle(this.bundleA);
      } catch (Exception e) {
        this.delegate.onSessionError(new UnbundleError(e, sessionA));
        // TODO: abort
        return;
      }
      this.getSynchronizer().repositoryB.createSession(this, this.context);
      return;
    }
    if (this.sessionB == null) {
      this.sessionB = session;
      // We no longer need a reference to our context.
      this.context = null;

      // Unbundle. We unbundled sessionA when that session was created.
      try {
        this.sessionB.unbundle(this.bundleB);
      } catch (Exception e) {
        // TODO: abort
        this.delegate.onSessionError(new UnbundleError(e, sessionB));
        return;
      }

      this.delegate.onInitialized(this);
      return;
    }
    // TODO: need a way to make sure we don't call any more delegate methods.
    this.delegate.onSessionError(new UnexpectedSessionException(session));
  }

  /*
   * RepositorySessionFinishDelegate methods.
   */
  @Override
  public void onFinishFailed(Exception ex) {
    if (this.sessionB == null) {
      // Ah, it was a problem cleaning up. Never mind.
      warn("Got exception cleaning up first after second session creation failed.", ex);
      return;
    }
    String session = (this.sessionA == null) ? "B" : "A";
    this.delegate.onSynchronizeFailed(this, ex, "Finish of session " + session + " failed.");
  }

  @Override
  public void onFinishSucceeded(RepositorySession session,
                                RepositorySessionBundle bundle) {
    info("onFinishSucceeded. Flows? " +
         flowAToBCompleted + ", " + flowBToACompleted);

    if (session == sessionA) {
      if (flowAToBCompleted) {
        info("onFinishSucceeded: bumping session A's timestamp to " + pendingATimestamp + " or " + storeEndATimestamp);
        bundle.bumpTimestamp(Math.max(pendingATimestamp, storeEndATimestamp));
        this.synchronizer.bundleA = bundle;
      }
      if (this.sessionB != null) {
        info("Finishing session B.");
        // On to the next.
        try {
          this.sessionB.finish(this);
        } catch (InactiveSessionException e) {
          this.onFinishFailed(e);
        }
      }
    } else if (session == sessionB) {
      if (flowBToACompleted) {
        info("onFinishSucceeded: bumping session B's timestamp to " + pendingBTimestamp + " or " + storeEndBTimestamp);
        bundle.bumpTimestamp(Math.max(pendingBTimestamp, storeEndBTimestamp));
        this.synchronizer.bundleB = bundle;
        info("Notifying delegate.onSynchronized.");
        this.delegate.onSynchronized(this);
      }
    } else {
      // TODO: hurrrrrr...
    }

    if (this.sessionB == null) {
      this.sessionA = null; // We're done.
    }
  }

  @Override
  public RepositorySessionFinishDelegate deferredFinishDelegate(final ExecutorService executor) {
    return new DeferredRepositorySessionFinishDelegate(this, executor);
  }
}
