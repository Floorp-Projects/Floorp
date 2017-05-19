/*
 * Copyright 2015, Leanplum, Inc. All rights reserved.
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

/**
 * LeanplumDeviceIdMode enum used for Leanplum.setDeviceMode.
 *
 * @author Paul Beusterien
 */
public enum LeanplumDeviceIdMode {
  /**
   * Takes the md5 hash of the MAC address, or the ANDROID_ID on Marshmallow or later, or if the
   * permission to access the MAC address is not set (Default).
   */
  MD5_MAC_ADDRESS,

  /**
   * Uses the ANDROID_ID.
   */
  ANDROID_ID,

  /**
   * Uses the Android Advertising ID. Requires Google Play Services v4.0 or higher. If there is an
   * error retrieving the Advertising ID, MD5_MAC_ADDRESS will be used instead.
   * <p>
   * <p>You also need the following line of code in your Android manifest within your
   * &lt;application&gt; tag:
   * <p>
   * <pre>&lt;meta-data android:name="com.google.android.gms.version"
   * android:value="@integer/google_play_services_version" /&gt;</pre>
   */
  ADVERTISING_ID,
}

