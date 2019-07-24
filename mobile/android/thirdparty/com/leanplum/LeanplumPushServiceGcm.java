package com.leanplum;

import com.leanplum.internal.Log;

/**
 * Created by anna on 12/15/17.
 */

public class LeanplumPushServiceGcm {
  /**
   * Call this when from {@link LeanplumPushService#onStart()} when Leanplum start for initialize
   * push provider and init push service. This method will call by reflection from AndroidSDKPush.
   */
  static void onStart() {
    try {
      LeanplumPushService.setCloudMessagingProvider(new LeanplumGcmProvider());
      LeanplumPushService.initPushService();
    } catch (LeanplumException e) {
      Log.e("There was an error registering for push notifications.\n" +
          Log.getStackTraceString(e));
    } catch (Throwable ignored) {
    }
  }
}
