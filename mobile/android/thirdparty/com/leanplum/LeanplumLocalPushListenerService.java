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
import android.os.Bundle;

import com.leanplum.internal.Constants;
import com.leanplum.internal.Log;
import com.leanplum.internal.Util;

/**
 * Listener Service for local push notifications.
 *
 * @author Aleksandar Gyorev
 */
public class LeanplumLocalPushListenerService extends IntentService {
  public LeanplumLocalPushListenerService() {
    super("LeanplumLocalPushListenerService");
  }

  @Override
  protected void onHandleIntent(Intent intent) {
    try {
      if (intent == null) {
        Log.e("The intent cannot be null");
        return;
      }
      Bundle extras = intent.getExtras();
      if (extras != null && extras.containsKey(Constants.Keys.PUSH_MESSAGE_TEXT)) {
        LeanplumPushService.handleNotification(this, extras);
      }
    } catch (Throwable t) {
      Util.handleException(t);
    }
  }
}
