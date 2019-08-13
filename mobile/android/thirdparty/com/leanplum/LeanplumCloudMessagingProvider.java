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

import android.content.Context;

import com.leanplum.internal.Constants;
import com.leanplum.internal.Log;
import com.leanplum.utils.SharedPreferencesUtil;

/**
 * Leanplum Cloud Messaging provider.
 *
 * @author Anna Orlova
 */
abstract class LeanplumCloudMessagingProvider {
  private static String registrationId;

  /**
   * Gets the registration Id associated with current messaging provider.
   *
   * @return Registration Id.
   */
  static String getCurrentRegistrationId() {
    return registrationId;
  }

  /**
   * Sends the registration ID to the server over HTTP.
   */
  private static void sendRegistrationIdToBackend(String registrationId) {
    Leanplum.setRegistrationId(registrationId);
  }

  /**
   * Registration app for Cloud Messaging.
   *
   * @return String - registration id for app.
   */
  public abstract String getRegistrationId();

  /**
   * Whether Messaging Provider is initialized correctly.
   *
   * @return True if provider is initialized, false otherwise.
   */
  public abstract boolean isInitialized();

  /**
   * Whether app manifest is setup correctly.
   *
   * @return True if manifest is setup, false otherwise.
   */
  public abstract boolean isManifestSetup();

  /**
   * Unregister from cloud messaging.
   */
  public abstract void unregister();

  /**
   * Callback should be invoked when Registration ID is received from provider.
   *
   * @param context The application context.
   * @param registrationId Registration Id.
   */
  void onRegistrationIdReceived(Context context, String registrationId) {
    if (registrationId == null) {
      Log.w("Registration ID is undefined.");
      return;
    }
    LeanplumCloudMessagingProvider.registrationId = registrationId;
    storePreferences(context.getApplicationContext());
    sendRegistrationIdToBackend(registrationId);
  }

  /**
   * Stores the registration ID in the application's {@code SharedPreferences}.
   *
   * @param context The application context.
   */
  public void storePreferences(Context context) {
    Log.v("Saving the registration ID in the shared preferences.");
    SharedPreferencesUtil.setString(context, Constants.Defaults.LEANPLUM_PUSH,
        Constants.Defaults.PROPERTY_REGISTRATION_ID, registrationId);
  }
}
