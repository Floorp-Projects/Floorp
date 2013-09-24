/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.healthreport;

import java.util.concurrent.BrokenBarrierException;

import org.mozilla.gecko.background.common.GlobalConstants;
import org.mozilla.gecko.background.healthreport.prune.HealthReportPruneService;
import org.mozilla.gecko.background.healthreport.upload.HealthReportUploadService;
import org.mozilla.gecko.background.helpers.BackgroundServiceTestCase;

import android.content.Intent;
import android.content.SharedPreferences;

public class TestHealthReportBroadcastService
    extends BackgroundServiceTestCase<TestHealthReportBroadcastService.MockHealthReportBroadcastService> {
  public static class MockHealthReportBroadcastService extends HealthReportBroadcastService {
    @Override
    protected SharedPreferences getSharedPreferences() {
      return this.getSharedPreferences(sharedPrefsName, GlobalConstants.SHARED_PREFERENCES_MODE);
    }

    @Override
    protected void onHandleIntent(Intent intent) {
      super.onHandleIntent(intent);
      try {
        barrier.await();
      } catch (InterruptedException e) {
        fail("Awaiting Service thread should not be interrupted.");
      } catch (BrokenBarrierException e) {
        // This will happen on timeout - do nothing.
      }
    }
  }

  public TestHealthReportBroadcastService() {
    super(MockHealthReportBroadcastService.class);
  }

  @Override
  public void setUp() throws Exception {
    super.setUp();
    // We can't mock AlarmManager since it has a package-private constructor, so instead we reset
    // the alarm by hand.
    cancelAlarm(getUploadIntent());
  }

  @Override
  public void tearDown() throws Exception {
    cancelAlarm(getUploadIntent());
    super.tearDown();
  }

  protected Intent getUploadIntent() {
    final Intent intent = new Intent(getContext(), HealthReportUploadService.class);
    intent.setAction("upload");
    return intent;
  }

  protected Intent getPruneIntent() {
    final Intent intent = new Intent(getContext(), HealthReportPruneService.class);
    intent.setAction("prune");
    return intent;
  }

  public void testIgnoredUploadPrefIntents() throws Exception {
    // Intent without "upload" extra is ignored.
    intent.setAction(HealthReportConstants.ACTION_HEALTHREPORT_UPLOAD_PREF)
        .putExtra("profileName", "profileName")
        .putExtra("profilePath", "profilePath");
    startService(intent);
    await();

    assertFalse(isServiceAlarmSet(getUploadIntent()));
    barrier.reset();

    // No "profileName" extra.
    intent.putExtra("enabled", true)
        .removeExtra("profileName");
    startService(intent);
    await();

    assertFalse(isServiceAlarmSet(getUploadIntent()));
    barrier.reset();

    // No "profilePath" extra.
    intent.putExtra("profileName", "profileName")
        .removeExtra("profilePath");
    startService(intent);
    await();

    assertFalse(isServiceAlarmSet(getUploadIntent()));
  }

  public void testUploadPrefIntentDisabled() throws Exception {
    intent.setAction(HealthReportConstants.ACTION_HEALTHREPORT_UPLOAD_PREF)
        .putExtra("enabled", false)
        .putExtra("profileName", "profileName")
        .putExtra("profilePath", "profilePath");
    startService(intent);
    await();

    assertFalse(isServiceAlarmSet(getUploadIntent()));
  }

  public void testUploadPrefIntentEnabled() throws Exception {
    intent.setAction(HealthReportConstants.ACTION_HEALTHREPORT_UPLOAD_PREF)
        .putExtra("enabled", true)
        .putExtra("profileName", "profileName")
        .putExtra("profilePath", "profilePath");
    startService(intent);
    await();

    assertTrue(isServiceAlarmSet(getUploadIntent()));
  }

  public void testUploadServiceCancelled() throws Exception {
    intent.setAction(HealthReportConstants.ACTION_HEALTHREPORT_UPLOAD_PREF)
        .putExtra("enabled", true)
        .putExtra("profileName", "profileName")
        .putExtra("profilePath", "profilePath");
    startService(intent);
    await();

    assertTrue(isServiceAlarmSet(getUploadIntent()));
    barrier.reset();

    intent.putExtra("enabled", false);
    startService(intent);
    await();

    assertFalse(isServiceAlarmSet(getUploadIntent()));
  }

  public void testPruneService() throws Exception {
    intent.setAction(HealthReportConstants.ACTION_HEALTHREPORT_PRUNE)
        .putExtra("profileName", "profileName")
        .putExtra("profilePath", "profilePath");
    startService(intent);
    await();
    assertTrue(isServiceAlarmSet(getPruneIntent()));
    barrier.reset();
  }
}
