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
import org.mozilla.gecko.sync.repositories.Server11Repository;

import java.net.URI;
import java.net.URISyntaxException;
import java.util.concurrent.TimeUnit;

@RunWith(TestRunner.class)
public class TestServer11Repository {

  private static final String COLLECTION = "bookmarks";
  private static final String COLLECTION_URL = "http://foo.com/1.5/n6ec3u5bee3tixzp2asys7bs6fve4jfw/storage";
  private static final long SYNC_DEADLINE = SystemClock.elapsedRealtime() + TimeUnit.MINUTES.toMillis(30);

  protected final InfoCollections infoCollections = new InfoCollections();
  protected final InfoConfiguration infoConfiguration = new InfoConfiguration();

  public static void assertQueryEquals(String expected, URI u) {
    Assert.assertEquals(expected, u.getRawQuery());
  }

  @Test
  public void testCollectionURI() throws URISyntaxException {
    Server11Repository noTrailingSlash = new Server11Repository(COLLECTION, SYNC_DEADLINE, COLLECTION_URL, null, infoCollections, infoConfiguration);
    Server11Repository trailingSlash = new Server11Repository(COLLECTION, SYNC_DEADLINE, COLLECTION_URL + "/", null, infoCollections, infoConfiguration);
    Assert.assertEquals("http://foo.com/1.5/n6ec3u5bee3tixzp2asys7bs6fve4jfw/storage/bookmarks", noTrailingSlash.collectionURI().toASCIIString());
    Assert.assertEquals("http://foo.com/1.5/n6ec3u5bee3tixzp2asys7bs6fve4jfw/storage/bookmarks", trailingSlash.collectionURI().toASCIIString());
  }
}
