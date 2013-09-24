/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.announcements;

import java.util.concurrent.BrokenBarrierException;

import org.mozilla.gecko.background.common.GlobalConstants;
import org.mozilla.gecko.background.helpers.BackgroundServiceTestCase;

import android.content.Intent;
import android.content.SharedPreferences;

public class TestAnnouncementsBroadcastService
    extends BackgroundServiceTestCase<TestAnnouncementsBroadcastService.MockAnnouncementsBroadcastService> {
  public static class MockAnnouncementsBroadcastService extends AnnouncementsBroadcastService {
    @Override
    protected SharedPreferences getSharedPreferences() {
      return this.getSharedPreferences(sharedPrefsName,
          GlobalConstants.SHARED_PREFERENCES_MODE);
    }

    @Override
    protected void onHandleIntent(Intent intent) {
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

  public TestAnnouncementsBroadcastService() {
    super(MockAnnouncementsBroadcastService.class);
  }

  @Override
  public void setUp() throws Exception {
    super.setUp();
    // We can't mock AlarmManager since it has a package-private constructor, so instead we reset
    // the alarm by hand.
    cancelAlarm(getServiceIntent());
  }

  @Override
  public void tearDown() throws Exception {
    cancelAlarm(getServiceIntent());
    AnnouncementsConstants.DISABLED = false;
    super.tearDown();
  }

  protected Intent getServiceIntent() {
    final Intent intent = new Intent(getContext(), AnnouncementsService.class);
    return intent;
  }

  public void testIgnoredServicePrefIntents() throws Exception {
    // Intent without "enabled" extra is ignored.
    intent.setAction(AnnouncementsConstants.ACTION_ANNOUNCEMENTS_PREF);
    startService(intent);
    await();

    assertFalse(isServiceAlarmSet(getServiceIntent()));
  }

  public void testServicePrefIntentDisabled() throws Exception {
    intent.setAction(AnnouncementsConstants.ACTION_ANNOUNCEMENTS_PREF)
        .putExtra("enabled", false);
    startService(intent);
    await();
    assertFalse(isServiceAlarmSet(getServiceIntent()));
  }

  public void testServicePrefIntentEnabled() throws Exception {
    intent.setAction(AnnouncementsConstants.ACTION_ANNOUNCEMENTS_PREF)
        .putExtra("enabled", true);
    startService(intent);
    await();
    assertTrue(isServiceAlarmSet(getServiceIntent()));
  }

  public void testServicePrefCancelled() throws Exception {
    intent.setAction(AnnouncementsConstants.ACTION_ANNOUNCEMENTS_PREF)
        .putExtra("enabled", true);
    startService(intent);
    await();

    assertTrue(isServiceAlarmSet(getServiceIntent()));
    barrier.reset();

    intent.putExtra("enabled", false);
    startService(intent);
    await();
    assertFalse(isServiceAlarmSet(getServiceIntent()));
  }
}
