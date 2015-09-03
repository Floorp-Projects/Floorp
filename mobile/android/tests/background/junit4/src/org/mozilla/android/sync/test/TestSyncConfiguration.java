/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.android.sync.test;

import java.net.URI;

import org.junit.Assert;
import org.junit.Test;
import org.mozilla.gecko.background.testhelpers.MockSharedPreferences;
import org.mozilla.gecko.sync.Sync11Configuration;
import org.mozilla.gecko.sync.SyncConfiguration;

public class TestSyncConfiguration {
  @Test
  public void testURLs() throws Exception {
    final MockSharedPreferences prefs = new MockSharedPreferences();

    // N.B., the username isn't used in the cluster path.
    SyncConfiguration fxaConfig = new SyncConfiguration("username", null, prefs);
    fxaConfig.clusterURL = new URI("http://db1.oldsync.dev.lcip.org/1.1/174");
    Assert.assertEquals("http://db1.oldsync.dev.lcip.org/1.1/174/info/collections", fxaConfig.infoCollectionsURL());
    Assert.assertEquals("http://db1.oldsync.dev.lcip.org/1.1/174/info/collection_counts", fxaConfig.infoCollectionCountsURL());
    Assert.assertEquals("http://db1.oldsync.dev.lcip.org/1.1/174/storage/meta/global", fxaConfig.metaURL());
    Assert.assertEquals("http://db1.oldsync.dev.lcip.org/1.1/174/storage", fxaConfig.storageURL());
    Assert.assertEquals("http://db1.oldsync.dev.lcip.org/1.1/174/storage/collection", fxaConfig.collectionURI("collection").toASCIIString());

    SyncConfiguration oldConfig = new Sync11Configuration("username", null, prefs);
    oldConfig.clusterURL = new URI("https://db.com/internal/");
    Assert.assertEquals("https://db.com/internal/1.1/username/info/collections", oldConfig.infoCollectionsURL());
    Assert.assertEquals("https://db.com/internal/1.1/username/info/collection_counts", oldConfig.infoCollectionCountsURL());
    Assert.assertEquals("https://db.com/internal/1.1/username/storage/meta/global", oldConfig.metaURL());
    Assert.assertEquals("https://db.com/internal/1.1/username/storage", oldConfig.storageURL());
    Assert.assertEquals("https://db.com/internal/1.1/username/storage/collection", oldConfig.collectionURI("collection").toASCIIString());
  }
}
