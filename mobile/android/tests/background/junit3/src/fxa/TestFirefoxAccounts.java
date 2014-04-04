/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.fxa;

import java.util.EnumSet;

import junit.framework.Assert;

import org.mozilla.gecko.background.helpers.AndroidSyncTestCase;
import org.mozilla.gecko.fxa.FirefoxAccounts;
import org.mozilla.gecko.fxa.sync.FxAccountSyncAdapter;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.setup.Constants;

import android.content.ContentResolver;
import android.os.Bundle;

public class TestFirefoxAccounts extends AndroidSyncTestCase {
  private static class InnerFirefoxAccounts extends FirefoxAccounts {
    // For testing, since putHintsToSync is not public.
    public static Bundle makeTestBundle(EnumSet<SyncHint> syncHints, String[] stagesToSync, String[] stagesToSkip) {
      final Bundle bundle = new Bundle();
      FirefoxAccounts.putHintsToSync(bundle, syncHints);
      Utils.putStageNamesToSync(bundle, stagesToSync, stagesToSkip);
      return bundle;
    }
  }

  protected void assertBundle(Bundle bundle, boolean manual, boolean respectLocal, boolean respectRemote, String stagesToSync, String stagesToSkip) {
    Assert.assertEquals(manual, bundle.getBoolean(ContentResolver.SYNC_EXTRAS_MANUAL));
    Assert.assertEquals(respectLocal, bundle.getBoolean(FxAccountSyncAdapter.SYNC_EXTRAS_RESPECT_LOCAL_RATE_LIMIT));
    Assert.assertEquals(respectRemote, bundle.getBoolean(FxAccountSyncAdapter.SYNC_EXTRAS_RESPECT_REMOTE_SERVER_BACKOFF));
    Assert.assertEquals(stagesToSync, bundle.getString(Constants.EXTRAS_KEY_STAGES_TO_SYNC));
    Assert.assertEquals(stagesToSkip, bundle.getString(Constants.EXTRAS_KEY_STAGES_TO_SKIP));
  }

  public void testMakeTestBundle() {
    Bundle bundle;
    bundle = InnerFirefoxAccounts.makeTestBundle(FirefoxAccounts.FORCE, new String[] { "clients" }, null);
    assertBundle(bundle, true, false, false, "{\"clients\":0}", null);
    assertEquals(FirefoxAccounts.FORCE, FirefoxAccounts.getHintsToSyncFromBundle(bundle));

    bundle = InnerFirefoxAccounts.makeTestBundle(FirefoxAccounts.NOW, null, new String[] { "bookmarks" });
    assertBundle(bundle, true, true, true, null, "{\"bookmarks\":0}");
    assertEquals(FirefoxAccounts.NOW, FirefoxAccounts.getHintsToSyncFromBundle(bundle));

    bundle = InnerFirefoxAccounts.makeTestBundle(FirefoxAccounts.SOON, null, null);
    assertBundle(bundle, false, true, true, null, null);
    assertEquals(FirefoxAccounts.SOON, FirefoxAccounts.getHintsToSyncFromBundle(bundle));
  }
}
