/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.middleware;

import java.io.UnsupportedEncodingException;
import java.util.concurrent.ExecutorService;

import org.mozilla.gecko.sync.CryptoRecord;
import org.mozilla.gecko.sync.crypto.CryptoException;
import org.mozilla.gecko.sync.crypto.KeyBundle;
import org.mozilla.gecko.sync.repositories.InactiveSessionException;
import org.mozilla.gecko.sync.repositories.NoStoreDelegateException;
import org.mozilla.gecko.sync.repositories.RecordFactory;
import org.mozilla.gecko.sync.repositories.RepositorySession;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFetchRecordsDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionStoreDelegate;
import org.mozilla.gecko.sync.repositories.domain.Record;

/**
 * It's a RepositorySession that accepts Records as input, producing CryptoRecords
 * for submission to a remote service.
 * Takes a RecordFactory as a parameter. This is in charge of taking decrypted CryptoRecords
 * as input and producing some expected kind of Record as output for local use.
 *
 *



                 +------------------------------------+
                 |    Server11RepositorySession       |
                 +-------------------------+----------+
                           ^               |
                           |               |
                        Encrypted CryptoRecords
                           |               |
                           |               v
                 +---------+--------------------------+
                 | Crypto5MiddlewareRepositorySession |
                 +------------------------------------+
                           ^               |
                           |               |  Decrypted CryptoRecords
                           |               |
                           |            +---------------+
                           |            | RecordFactory |
                           |            +--+------------+
                           |               |
                          Local Record instances
                           |               |
                           |               v
                 +---------+--------------------------+
                 |  Local RepositorySession instance  |
                 +------------------------------------+


 * @author rnewman
 *
 */
public class Crypto5MiddlewareRepositorySession extends MiddlewareRepositorySession {
  private final KeyBundle keyBundle;
  private final RecordFactory recordFactory;

  public Crypto5MiddlewareRepositorySession(RepositorySession session, Crypto5MiddlewareRepository repository, RecordFactory recordFactory) {
    super(session, repository);
    this.keyBundle = repository.keyBundle;
    this.recordFactory = recordFactory;
  }

  public class DecryptingTransformingFetchDelegate implements RepositorySessionFetchRecordsDelegate {
    private final RepositorySessionFetchRecordsDelegate next;
    private final KeyBundle keyBundle;
    private final RecordFactory recordFactory;

    DecryptingTransformingFetchDelegate(RepositorySessionFetchRecordsDelegate next, KeyBundle bundle, RecordFactory recordFactory) {
      this.next = next;
      this.keyBundle = bundle;
      this.recordFactory = recordFactory;
    }

    @Override
    public void onFetchFailed(Exception ex, Record record) {
      next.onFetchFailed(ex, record);
    }

    @Override
    public void onFetchedRecord(Record record) {
      CryptoRecord r;
      try {
        r = (CryptoRecord) record;
      } catch (ClassCastException e) {
        next.onFetchFailed(e, record);
        return;
      }
      r.keyBundle = keyBundle;
      try {
        r.decrypt();
      } catch (Exception e) {
        next.onFetchFailed(e, r);
        return;
      }
      Record transformed;
      try {
        transformed = this.recordFactory.createRecord(r);
      } catch (Exception e) {
        next.onFetchFailed(e, r);
        return;
      }
      next.onFetchedRecord(transformed);
    }

    @Override
    public void onFetchCompleted(final long fetchEnd) {
      next.onFetchCompleted(fetchEnd);
    }

    @Override
    public RepositorySessionFetchRecordsDelegate deferredFetchDelegate(ExecutorService executor) {
      // Synchronously perform *our* work, passing through appropriately.
      RepositorySessionFetchRecordsDelegate deferredNext = next.deferredFetchDelegate(executor);
      return new DecryptingTransformingFetchDelegate(deferredNext, keyBundle, recordFactory);
    }
  }

  private DecryptingTransformingFetchDelegate makeUnwrappingDelegate(RepositorySessionFetchRecordsDelegate inner) {
    if (inner == null) {
      throw new IllegalArgumentException("Inner delegate cannot be null!");
    }
    return new DecryptingTransformingFetchDelegate(inner, this.keyBundle, this.recordFactory);
  }

  @Override
  public void fetchSince(long timestamp,
                         RepositorySessionFetchRecordsDelegate delegate) {
    inner.fetchSince(timestamp, makeUnwrappingDelegate(delegate));
  }

  @Override
  public void fetch(String[] guids,
                    RepositorySessionFetchRecordsDelegate delegate) throws InactiveSessionException {
    inner.fetch(guids, makeUnwrappingDelegate(delegate));
  }

  @Override
  public void fetchAll(RepositorySessionFetchRecordsDelegate delegate) {
    inner.fetchAll(makeUnwrappingDelegate(delegate));
  }

  @Override
  public void setStoreDelegate(RepositorySessionStoreDelegate delegate) {
    // TODO: it remains to be seen how this will work.
    inner.setStoreDelegate(delegate);
    this.delegate = delegate;             // So we can handle errors without involving inner.
  }

  @Override
  public void store(Record record) throws NoStoreDelegateException {
    if (delegate == null) {
      throw new NoStoreDelegateException();
    }
    CryptoRecord rec = record.getEnvelope();
    rec.keyBundle = this.keyBundle;
    try {
      rec.encrypt();
    } catch (UnsupportedEncodingException | CryptoException e) {
      delegate.onRecordStoreFailed(e, record.guid);
      return;
    }
    // Allow the inner session to do delegate handling.
    inner.store(rec);
  }
}
