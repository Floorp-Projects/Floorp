/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.healthreport.upload;

import java.util.concurrent.BrokenBarrierException;

import org.mozilla.gecko.background.common.GlobalConstants;
import org.mozilla.gecko.background.healthreport.HealthReportConstants;
import org.mozilla.gecko.background.helpers.BackgroundServiceTestCase;

import android.content.Intent;
import android.content.SharedPreferences;

public class TestHealthReportUploadService
    extends BackgroundServiceTestCase<TestHealthReportUploadService.MockHealthReportUploadService> {
  public static class MockHealthReportUploadService extends HealthReportUploadService {
    @Override
    protected SharedPreferences getSharedPreferences() {
      return this.getSharedPreferences(sharedPrefsName,
          GlobalConstants.SHARED_PREFERENCES_MODE);
    }

    @Override
    public boolean backgroundDataIsEnabled() {
      // When testing, we always want to say we can upload.
      return true;
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
  }

  public TestHealthReportUploadService() {
    super(MockHealthReportUploadService.class);
  }

  protected boolean isFirstRunSet() throws Exception {
    return getSharedPreferences().contains(HealthReportConstants.PREF_FIRST_RUN);
  }

  @Override
  public void setUp() throws Exception {
    super.setUp();
    // First run state is used for comparative testing.
    assertFalse(isFirstRunSet());
  }


  public void testFailedFirstRun() throws Exception {
    // Missing "uploadEnabled".
    intent.putExtra("profileName", "profileName")
        .putExtra("profilePath", "profilePath");
    startService(intent);
    await();
    assertFalse(isFirstRunSet());
    barrier.reset();

    // Missing "profilePath".
    intent.putExtra("uploadEnabled", true)
        .removeExtra("profilePath");
    startService(intent);
    await();
    assertFalse(isFirstRunSet());
    barrier.reset();

    // Missing "profileName".
    intent.putExtra("profilePath", "profilePath")
        .removeExtra("profileName");
    startService(intent);
    assertFalse(isFirstRunSet());
    await();
    assertFalse(isFirstRunSet());
  }

  public void testUploadDisabled() throws Exception {
    intent.putExtra("profileName", "profileName")
        .putExtra("profilePath", "profilePath")
        .putExtra("uploadEnabled", false);
    startService(intent);
    await();
    assertFalse(isFirstRunSet());
  }

  public void testSuccessfulFirstRun() throws Exception {
    intent.putExtra("profileName", "profileName")
        .putExtra("profilePath", "profilePath")
        .putExtra("uploadEnabled", true);
    startService(intent);
    await();
    assertTrue(isFirstRunSet());
  }
}
