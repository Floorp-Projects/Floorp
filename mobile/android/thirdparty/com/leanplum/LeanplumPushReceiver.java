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

import com.leanplum.internal.Constants;
import com.leanplum.internal.LeanplumManifestHelper;
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
      // Parse manifest and pull metadata which contains client broadcast receiver class.
      String receiver = LeanplumManifestHelper.parseNotificationMetadata();
      // If receiver isn't found we will open up notification with default activity
      if (receiver == null) {
        Log.d("Custom broadcast receiver class not set, using default one.");
        LeanplumPushService.openNotification(context, intent);
      } else {
        Log.d("Custom broadcast receiver class found, using it to handle push notifications.");
        // Forward Intent to a client broadcast receiver.
        Intent forwardIntent = new Intent();
        // Add action to be able to differentiate between multiple intents.
        forwardIntent.setAction(LeanplumPushService.LEANPLUM_NOTIFICATION);
        forwardIntent.setClassName(context, receiver);
        forwardIntent.putExtras(intent.getExtras());
        context.sendBroadcast(forwardIntent);
      }
    } catch (Throwable t) {
      Util.handleException(t);
    }
  }
}
