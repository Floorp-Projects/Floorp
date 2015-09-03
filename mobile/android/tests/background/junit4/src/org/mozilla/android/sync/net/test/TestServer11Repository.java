/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.android.sync.net.test;

import java.net.URI;
import java.net.URISyntaxException;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.sync.InfoCollections;
import org.mozilla.gecko.sync.repositories.Server11Repository;
import org.robolectric.RobolectricGradleTestRunner;

@RunWith(RobolectricGradleTestRunner.class)
public class TestServer11Repository {

  private static final String COLLECTION = "bookmarks";
  private static final String COLLECTION_URL = "http://foo.com/1.1/n6ec3u5bee3tixzp2asys7bs6fve4jfw/storage";

  protected final InfoCollections infoCollections = new InfoCollections();

  public static void assertQueryEquals(String expected, URI u) {
    Assert.assertEquals(expected, u.getRawQuery());
  }

  @SuppressWarnings("static-method")
  @Test
  public void testCollectionURIFull() throws URISyntaxException {
    Server11Repository r = new Server11Repository(COLLECTION, COLLECTION_URL, null, infoCollections);
    assertQueryEquals("full=1&newer=5000.000",              r.collectionURI(true,  5000000L, -1,    null, null));
    assertQueryEquals("newer=1230.000",                     r.collectionURI(false, 1230000L, -1,    null, null));
    assertQueryEquals("newer=5000.000&limit=10",            r.collectionURI(false, 5000000L, 10,    null, null));
    assertQueryEquals("full=1&newer=5000.000&sort=index",   r.collectionURI(true,  5000000L,  0, "index", null));
    assertQueryEquals("full=1&ids=123,abc",                 r.collectionURI(true,       -1L, -1,    null, "123,abc"));
  }

  @Test
  public void testCollectionURI() throws URISyntaxException {
    Server11Repository noTrailingSlash = new Server11Repository(COLLECTION, COLLECTION_URL, null, infoCollections);
    Server11Repository trailingSlash = new Server11Repository(COLLECTION, COLLECTION_URL + "/", null, infoCollections);
    Assert.assertEquals("http://foo.com/1.1/n6ec3u5bee3tixzp2asys7bs6fve4jfw/storage/bookmarks", noTrailingSlash.collectionURI().toASCIIString());
    Assert.assertEquals("http://foo.com/1.1/n6ec3u5bee3tixzp2asys7bs6fve4jfw/storage/bookmarks", trailingSlash.collectionURI().toASCIIString());
  }
}
