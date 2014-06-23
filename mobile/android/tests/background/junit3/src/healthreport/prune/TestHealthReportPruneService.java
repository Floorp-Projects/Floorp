/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.healthreport.prune;

import java.util.concurrent.BrokenBarrierException;

import org.mozilla.gecko.background.common.GlobalConstants;
import org.mozilla.gecko.background.helpers.BackgroundServiceTestCase;

import android.content.Intent;
import android.content.SharedPreferences;

public class TestHealthReportPruneService
    extends BackgroundServiceTestCase<TestHealthReportPruneService.MockHealthReportPruneService> {
  public static class MockHealthReportPruneService extends HealthReportPruneService {
    protected MockPrunePolicy prunePolicy;

    @Override
    protected SharedPreferences getSharedPreferences() {
      return this.getSharedPreferences(sharedPrefsName,
          GlobalConstants.SHARED_PREFERENCES_MODE);
    }

    @Override
    public void onHandleIntent(Intent intent) {
      super.onHandleIntent(intent);
      try {
        barrier.await();
      } catch (InterruptedException e) {
        fail("Awaiting thread should not be interrupted.");
      } catch (BrokenBarrierException e) {
        // This will happen on timeout - do nothing.
      }
    }

    @Override
    public PrunePolicy getPrunePolicy(final String profilePath) {
      // We don't actually need any storage, so just make it null. Actually
      // creating storage requires a valid context; here, we only have a
      // MockContext.
      final PrunePolicyStorage storage = null;
      prunePolicy = new MockPrunePolicy(storage, getSharedPreferences());
      return prunePolicy;
    }

    public boolean wasTickCalled() {
      if (prunePolicy == null) {
        return false;
      }
      return prunePolicy.wasTickCalled();
    }
  }

  // TODO: This is a spy - perhaps we should be using a framework for this.
  public static class MockPrunePolicy extends PrunePolicy {
    private boolean wasTickCalled;

    public MockPrunePolicy(final PrunePolicyStorage storage, final SharedPreferences sharedPreferences) {
      super(storage, sharedPreferences);
      wasTickCalled = false;
    }

    @Override
    public void tick(final long time) {
      wasTickCalled = true;
    }

    public boolean wasTickCalled() {
      return wasTickCalled;
    }
  }

  public TestHealthReportPruneService() {
    super(MockHealthReportPruneService.class);
  }

  @Override
  public void setUp() throws Exception {
    super.setUp();
  }

  public void testIsIntentValid() throws Exception {
    // No profilePath or profileName.
    startService(intent);
    await();
    assertFalse(getService().wasTickCalled());
    barrier.reset();

    // No profilePath.
    intent.putExtra("profileName", "profileName");
    startService(intent);
    await();
    assertFalse(getService().wasTickCalled());
    barrier.reset();

    // No profileName.
    intent.putExtra("profilePath", "profilePath")
          .removeExtra("profileName");
    startService(intent);
    await();
    assertFalse(getService().wasTickCalled());
    barrier.reset();

    intent.putExtra("profileName", "profileName");
    startService(intent);
    await();
    assertTrue(getService().wasTickCalled());
  }
}
