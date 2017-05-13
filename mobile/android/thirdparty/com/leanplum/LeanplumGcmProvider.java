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

import com.google.android.gms.gcm.GoogleCloudMessaging;
import com.google.android.gms.iid.InstanceID;
import com.leanplum.internal.Constants;
import com.leanplum.internal.LeanplumManifestHelper;
import com.leanplum.internal.Log;
import com.leanplum.internal.Util;
import com.leanplum.utils.SharedPreferencesUtil;

import java.io.IOException;
import java.util.Arrays;
import java.util.Collections;

/**
 * Leanplum provider for work with GCM.
 *
 * @author Anna Orlova
 */
class LeanplumGcmProvider extends LeanplumCloudMessagingProvider {
  private static final String ERROR_TIMEOUT = "TIMEOUT";
  private static final String ERROR_INVALID_SENDER = "INVALID_SENDER";
  private static final String ERROR_AUTHENTICATION_FAILED = "AUTHENTICATION_FAILED";
  private static final String ERROR_PHONE_REGISTRATION_ERROR = "PHONE_REGISTRATION_ERROR";
  private static final String ERROR_TOO_MANY_REGISTRATIONS = "TOO_MANY_REGISTRATIONS";

  private static final String SEND_PERMISSION = "com.google.android.c2dm.permission.SEND";
  private static final String RECEIVE_PERMISSION = "com.google.android.c2dm.permission.RECEIVE";
  private static final String RECEIVE_ACTION = "com.google.android.c2dm.intent.RECEIVE";
  private static final String REGISTRATION_ACTION = "com.google.android.c2dm.intent.REGISTRATION";
  private static final String INSTANCE_ID_ACTION = "com.google.android.gms.iid.InstanceID";
  private static final String PUSH_LISTENER_SERVICE = "com.leanplum.LeanplumPushListenerService";
  private static final String GCM_RECEIVER = "com.google.android.gms.gcm.GcmReceiver";
  private static final String PUSH_INSTANCE_ID_SERVICE =
      "com.leanplum.LeanplumPushInstanceIDService";

  private static String senderIds;

  static void setSenderId(String senderId) {
    senderIds = senderId;
  }

  /**
   * Stores the GCM sender ID in the application's {@code SharedPreferences}.
   *
   * @param context application's context.
   */
  @Override
  public void storePreferences(Context context) {
    super.storePreferences(context);
    Log.v("Saving GCM sender ID");
    SharedPreferencesUtil.setString(context, Constants.Defaults.LEANPLUM_PUSH,
        Constants.Defaults.PROPERTY_SENDER_IDS, senderIds);
  }

  public String getRegistrationId() {
    String registrationId = null;
    try {
      InstanceID instanceID = InstanceID.getInstance(Leanplum.getContext());
      if (senderIds == null || instanceID == null) {
        Log.w("There was a problem setting up GCM, please make sure you follow instructions " +
            "on how to set it up.");
        return null;
      }
      registrationId = instanceID.getToken(senderIds,
          GoogleCloudMessaging.INSTANCE_ID_SCOPE, null);
    } catch (IOException e) {
      if (GoogleCloudMessaging.ERROR_SERVICE_NOT_AVAILABLE.equals(e.getMessage())) {
        Log.w("GCM service is not available. Will try to " +
            "register again next time the app starts.");
      } else if (ERROR_TIMEOUT.equals(e.getMessage())) {
        Log.w("Retrieval of GCM registration token timed out. " +
            "Will try to register again next time the app starts.");
      } else if (ERROR_INVALID_SENDER.equals(e.getMessage())) {
        Log.e("The GCM sender account is not recognized. Please be " +
            "sure to call LeanplumPushService.setGsmSenderId() with a valid GCM sender id.");
      } else if (ERROR_AUTHENTICATION_FAILED.equals(e.getMessage())) {
        Log.w("Bad Google Account password.");
      } else if (ERROR_PHONE_REGISTRATION_ERROR.equals(e.getMessage())) {
        Log.w("This phone doesn't currently support GCM.");
      } else if (ERROR_TOO_MANY_REGISTRATIONS.equals(e.getMessage())) {
        Log.w("This phone has more than the allowed number of " +
            "apps that are registered with GCM.");
      } else {
        Log.e("Failed to complete registration token refresh.");
        Util.handleException(e);
      }
    } catch (Throwable t) {
      Log.w("There was a problem setting up GCM, please make sure you follow instructions " +
          "on how to set it up. Please verify that you are using correct version of " +
          "Google Play Services and Android Support Library v4.");
      Util.handleException(t);
    }
    return registrationId;
  }

  public boolean isInitialized() {
    return senderIds != null || getCurrentRegistrationId() != null;
  }

  public boolean isManifestSetUp() {
    Context context = Leanplum.getContext();
    if (context == null) {
      return false;
    }

    boolean hasPermissions = LeanplumManifestHelper.checkPermission(RECEIVE_PERMISSION, false, true)
        && (LeanplumManifestHelper.checkPermission(context.getPackageName() +
        ".gcm.permission.C2D_MESSAGE", true, false) || LeanplumManifestHelper.checkPermission(
        context.getPackageName() + ".permission.C2D_MESSAGE", true, true));

    boolean hasGcmReceiver = LeanplumManifestHelper.checkComponent(
        LeanplumManifestHelper.getReceivers(), GCM_RECEIVER, true, SEND_PERMISSION,
        Arrays.asList(RECEIVE_ACTION, REGISTRATION_ACTION), context.getPackageName());
    boolean hasPushReceiver = LeanplumManifestHelper.checkComponent(
        LeanplumManifestHelper.getReceivers(), PUSH_RECEIVER, false, null,
        Collections.singletonList(PUSH_LISTENER_SERVICE), null);

    boolean hasReceivers = hasGcmReceiver && hasPushReceiver;

    boolean hasPushListenerService = LeanplumManifestHelper.checkComponent(
        LeanplumManifestHelper.getServices(), PUSH_LISTENER_SERVICE, false, null,
        Collections.singletonList(RECEIVE_ACTION), null);
    boolean hasPushInstanceIDService = LeanplumManifestHelper.checkComponent(
        LeanplumManifestHelper.getServices(), PUSH_INSTANCE_ID_SERVICE, false, null,
        Collections.singletonList(INSTANCE_ID_ACTION), null);
    boolean hasPushRegistrationService = LeanplumManifestHelper.checkComponent(
        LeanplumManifestHelper.getServices(), PUSH_REGISTRATION_SERVICE, false, null, null, null);

    boolean hasServices = hasPushListenerService && hasPushInstanceIDService
        && hasPushRegistrationService;

    return hasPermissions && hasReceivers && hasServices;
  }

  /**
   * Unregister from GCM.
   */
  public void unregister() {
    try {
      InstanceID.getInstance(Leanplum.getContext()).deleteInstanceID();
      Log.i("Application was unregistred from GCM.");
    } catch (Exception e) {
      Log.e("Failed to unregister from GCM.");
    }
  }
}
