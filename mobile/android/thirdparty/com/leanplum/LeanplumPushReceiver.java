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

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

import com.leanplum.internal.Log;
import com.leanplum.internal.Util;

/**
 * Handles push notification intents, for example, by tracking opens and performing the open
 * action.
 *
 * @author Aleksandar Gyorev
 */
public class LeanplumPushReceiver extends BroadcastReceiver {

  @Override
  public void onReceive(Context context, Intent intent) {
    try {
      if (intent == null) {
        Log.e("Received a null intent.");
        return;
      }
      LeanplumPushService.openNotification(context, intent.getExtras());
    } catch (Throwable t) {
      Util.handleException(t);
    }
  }
}
