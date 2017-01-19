/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.android.sync.net.test;

import android.os.SystemClock;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.sync.InfoCollections;
import org.mozilla.gecko.sync.InfoConfiguration;
import org.mozilla.gecko.sync.repositories.NonPersistentRepositoryStateProvider;
import org.mozilla.gecko.sync.repositories.RepositoryStateProvider;
import org.mozilla.gecko.sync.repositories.Server15Repository;

import java.net.URI;
import java.net.URISyntaxException;
import java.util.concurrent.TimeUnit;

@RunWith(TestRunner.class)
public class TestServer15Repository {

  private static final String COLLECTION = "bookmarks";
  private static final String COLLECTION_URL = "http://foo.com/1.5/n6ec3u5bee3tixzp2asys7bs6fve4jfw/storage";
  private static final long SYNC_DEADLINE = SystemClock.elapsedRealtime() + TimeUnit.MINUTES.toMillis(30);

  protected final InfoCollections infoCollections = new InfoCollections();
  protected final InfoConfiguration infoConfiguration = new InfoConfiguration();
  protected final RepositoryStateProvider stateProvider = new NonPersistentRepositoryStateProvider();

  public static void assertQueryEquals(String expected, URI u) {
    Assert.assertEquals(expected, u.getRawQuery());
  }

  @Test
  public void testCollectionURI() throws URISyntaxException {
    Server15Repository noTrailingSlash = new Server15Repository(COLLECTION, SYNC_DEADLINE, COLLECTION_URL, null, infoCollections, infoConfiguration, stateProvider);
    Server15Repository trailingSlash = new Server15Repository(COLLECTION, SYNC_DEADLINE, COLLECTION_URL + "/", null, infoCollections, infoConfiguration, stateProvider);
    Assert.assertEquals("http://foo.com/1.5/n6ec3u5bee3tixzp2asys7bs6fve4jfw/storage/bookmarks", noTrailingSlash.collectionURI().toASCIIString());
    Assert.assertEquals("http://foo.com/1.5/n6ec3u5bee3tixzp2asys7bs6fve4jfw/storage/bookmarks", trailingSlash.collectionURI().toASCIIString());
  }
}
