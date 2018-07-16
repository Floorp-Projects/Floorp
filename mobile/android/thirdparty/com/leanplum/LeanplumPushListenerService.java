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

import android.os.Bundle;

import com.google.android.gms.gcm.GcmListenerService;
import com.leanplum.internal.Constants.Keys;
import com.leanplum.internal.Log;
import com.leanplum.internal.Util;

/**
 * GCM listener service, which enables handling messages on the app's behalf.
 *
 * @author Aleksandar Gyorev
 */
public class LeanplumPushListenerService extends GcmListenerService {
  /**
   * Called when a message is received.
   *
   * @param senderId Sender ID of the sender.
   * @param data Data bundle containing the message data as key-value pairs.
   */
  @Override
  public void onMessageReceived(String senderId, Bundle data) {
    try {
      if (data.containsKey(Keys.PUSH_MESSAGE_TEXT)) {
        LeanplumPushService.handleNotification(this, data);
      }
      Log.i("Received: " + data.toString());
    } catch (Throwable t) {
      Util.handleException(t);
    }
  }
}
