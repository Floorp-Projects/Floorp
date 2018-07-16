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

import android.os.Build;

import com.google.android.gms.iid.InstanceIDListenerService;
import com.leanplum.internal.Log;

/**
 * GCM InstanceID listener service to handle creation, rotation, and updating of registration
 * tokens.
 *
 * @author Aleksandar Gyorev
 */
public class LeanplumPushInstanceIDService extends InstanceIDListenerService {
  /**
   * Called if InstanceID token is updated. This may occur if the security of the previous token had
   * been compromised. This call is initiated by the InstanceID provider.
   */
  @Override
  public void onTokenRefresh() {
    try {
      if (Build.VERSION.SDK_INT < 26) {
        LeanplumNotificationHelper.startPushRegistrationService(this, "GCM");
      } else {
        LeanplumNotificationHelper.scheduleJobService(this,
                LeanplumGcmRegistrationJobService.class, LeanplumGcmRegistrationJobService.JOB_ID);
      }
    } catch (Throwable t) {
      Log.e("Failed to update GCM token.", t);
    }
  }
}
