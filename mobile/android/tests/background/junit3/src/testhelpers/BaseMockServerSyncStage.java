/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.testhelpers;

import java.io.IOException;
import java.net.URISyntaxException;

import org.json.simple.parser.ParseException;
import org.mozilla.gecko.sync.NoCollectionKeysSetException;
import org.mozilla.gecko.sync.NonObjectJSONException;
import org.mozilla.gecko.sync.SynchronizerConfiguration;
import org.mozilla.gecko.sync.repositories.RecordFactory;
import org.mozilla.gecko.sync.repositories.Repository;
import org.mozilla.gecko.sync.stage.ServerSyncStage;

/**
 * A stage that joins two Repositories with no wrapping.
 */
public abstract class BaseMockServerSyncStage extends ServerSyncStage {

  public Repository local;
  public Repository remote;
  public String name;
  public String collection;
  public int version = 1;

  @Override
  public boolean isEnabled() {
    return true;
  }

  @Override
  protected String getCollection() {
    return collection;
  }

  @Override
  protected Repository getLocalRepository() {
    return local;
  }

  @Override
  protected Repository getRemoteRepository() throws URISyntaxException {
    return remote;
  }

  @Override
  protected String getEngineName() {
    return name;
  }

  @Override
  public Integer getStorageVersion() {
    return version;
  }

  @Override
  protected RecordFactory getRecordFactory() {
    return null;
  }

  @Override
  protected Repository wrappedServerRepo()
  throws NoCollectionKeysSetException, URISyntaxException {
    return getRemoteRepository();
  }

  public SynchronizerConfiguration leakConfig()
  throws NonObjectJSONException, IOException, ParseException {
    return this.getConfig();
  }
}
