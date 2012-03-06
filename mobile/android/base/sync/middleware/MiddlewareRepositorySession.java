/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.middleware;

import java.util.concurrent.ExecutorService;

import org.mozilla.gecko.sync.Logger;
import org.mozilla.gecko.sync.repositories.InactiveSessionException;
import org.mozilla.gecko.sync.repositories.InvalidSessionTransitionException;
import org.mozilla.gecko.sync.repositories.RepositorySession;
import org.mozilla.gecko.sync.repositories.RepositorySessionBundle;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionBeginDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFinishDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionGuidsSinceDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionWipeDelegate;

public abstract class MiddlewareRepositorySession extends RepositorySession {
  private static final String LOG_TAG = "MiddlewareSession";
  protected final RepositorySession inner;

  public MiddlewareRepositorySession(RepositorySession innerSession, MiddlewareRepository repository) {
    super(repository);
    this.inner = innerSession;
  }

  @Override
  public void wipe(RepositorySessionWipeDelegate delegate) {
    inner.wipe(delegate);
  }

  public class MiddlewareRepositorySessionBeginDelegate implements RepositorySessionBeginDelegate {

    private MiddlewareRepositorySession outerSession;
    private RepositorySessionBeginDelegate next;

    public MiddlewareRepositorySessionBeginDelegate(MiddlewareRepositorySession outerSession, RepositorySessionBeginDelegate next) {
      this.outerSession = outerSession;
      this.next = next;
    }

    @Override
    public void onBeginFailed(Exception ex) {
      next.onBeginFailed(ex);
    }

    @Override
    public void onBeginSucceeded(RepositorySession session) {
      next.onBeginSucceeded(outerSession);
    }

    @Override
    public RepositorySessionBeginDelegate deferredBeginDelegate(ExecutorService executor) {
      final RepositorySessionBeginDelegate deferred = next.deferredBeginDelegate(executor);
      return new RepositorySessionBeginDelegate() {
        @Override
        public void onBeginSucceeded(RepositorySession session) {
          if (inner != session) {
            Logger.warn(LOG_TAG, "Got onBeginSucceeded for session " + session + ", not our inner session!");
          }
          deferred.onBeginSucceeded(outerSession);
        }

        @Override
        public void onBeginFailed(Exception ex) {
          deferred.onBeginFailed(ex);
        }

        @Override
        public RepositorySessionBeginDelegate deferredBeginDelegate(ExecutorService executor) {
          return this;
        }
      };
    }
  }

  public void begin(RepositorySessionBeginDelegate delegate) throws InvalidSessionTransitionException {
    inner.begin(new MiddlewareRepositorySessionBeginDelegate(this, delegate));
  }

  public class MiddlewareRepositorySessionFinishDelegate implements RepositorySessionFinishDelegate {
    private final MiddlewareRepositorySession outerSession;
    private final RepositorySessionFinishDelegate next;

    public MiddlewareRepositorySessionFinishDelegate(MiddlewareRepositorySession outerSession, RepositorySessionFinishDelegate next) {
      this.outerSession = outerSession;
      this.next = next;
    }

    @Override
    public void onFinishFailed(Exception ex) {
      next.onFinishFailed(ex);
    }

    @Override
    public void onFinishSucceeded(RepositorySession session, RepositorySessionBundle bundle) {
      next.onFinishSucceeded(outerSession, bundle);
    }

    @Override
    public RepositorySessionFinishDelegate deferredFinishDelegate(ExecutorService executor) {
      return this;
    }
  }

  @Override
  public void finish(RepositorySessionFinishDelegate delegate) throws InactiveSessionException {
    inner.finish(new MiddlewareRepositorySessionFinishDelegate(this, delegate));
  }


  @Override
  public synchronized void ensureActive() throws InactiveSessionException {
    inner.ensureActive();
  }

  @Override
  public synchronized boolean isActive() {
    return inner.isActive();
  }

  @Override
  public synchronized SessionStatus getStatus() {
    return inner.getStatus();
  }

  @Override
  public synchronized void setStatus(SessionStatus status) {
    inner.setStatus(status);
  }

  @Override
  public synchronized void transitionFrom(SessionStatus from, SessionStatus to)
      throws InvalidSessionTransitionException {
    inner.transitionFrom(from, to);
  }

  @Override
  public void abort() {
    inner.abort();
  }

  @Override
  public void abort(RepositorySessionFinishDelegate delegate) {
    inner.abort(new MiddlewareRepositorySessionFinishDelegate(this, delegate));
  }

  @Override
  public void guidsSince(long timestamp, RepositorySessionGuidsSinceDelegate delegate) {
    // TODO: need to do anything here?
    inner.guidsSince(timestamp, delegate);
  }

  @Override
  public void storeDone() {
    inner.storeDone();
  }

  @Override
  public void storeDone(long storeEnd) {
    inner.storeDone(storeEnd);
  }
}