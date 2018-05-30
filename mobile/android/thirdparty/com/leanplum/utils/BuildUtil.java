package com.leanplum.utils;

/*
 * Copyright 2017, Leanplum, Inc. All rights reserved.
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

import android.content.Context;
import android.os.Build;

/**
 * Utilities related to Build Version and target SDK.
 *
 * @author Anna Orlova
 */
public class BuildUtil {
    private static int targetSdk = -1;

    /**
     * Whether notification channels are supported.
     *
     * @param context The application context.
     * @return True if notification channels are supported, false otherwise.
     */
    public static boolean isNotificationChannelSupported(Context context) {
        return Build.VERSION.SDK_INT >= 26 && getTargetSdkVersion(context) >= 26;
    }

    /**
     * Returns target SDK version parsed from manifest.
     *
     * @param context The application context.
     * @return Target SDK version.
     */
    private static int getTargetSdkVersion(Context context) {
        if (targetSdk == -1 && context != null) {
            targetSdk = context.getApplicationInfo().targetSdkVersion;
        }
        return targetSdk;
    }
}