/*
 * Copyright 2016, Leanplum, Inc. All rights reserved.
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

package com.leanplum;

import android.app.IntentService;
import android.content.Intent;

import com.leanplum.internal.Log;

/**
 * Registration service that handles registration with the GCM and FCM, using
 * InstanceID.
 *
 * @author Aleksandar Gyorev
 */
public class LeanplumPushRegistrationService extends IntentService {
  private static String existingRegistrationId;

  public LeanplumPushRegistrationService() {
    super("LeanplumPushRegistrationService");
  }

  @Override
  protected void onHandleIntent(Intent intent) {
    LeanplumCloudMessagingProvider provider = LeanplumPushService.getCloudMessagingProvider();
    if (provider == null) {
      Log.e("Failed to complete registration token refresh.");
      return;
    }
    String registrationId = provider.getRegistrationId();
    if (registrationId != null) {
      if (existingRegistrationId != null && !registrationId.equals(existingRegistrationId)) {
        Log.e("WARNING: It appears your app is registering " +
            "with GCM/FCM using multiple GCM/FCM sender ids. Please be sure to call " +
            "LeanplumPushService.setGcmSenderIds() with " +
            "all of the GCM sender ids that you use, not just the one that you use with " +
            "Leanplum. Otherwise, GCM/FCM push notifications may not work consistently.");
      }
      existingRegistrationId = registrationId;
      provider.onRegistrationIdReceived(getApplicationContext(), registrationId);
    }
  }
}
