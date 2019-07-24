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

import android.app.Notification;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.v4.app.NotificationCompat;

/**
 * Implement LeanplumPushNotificationCustomizer to customize the appearance of notifications.
 */
public interface LeanplumPushNotificationCustomizer {
  /**
   * Implement this method to customize push notification. Please call {@link
   * LeanplumPushService#setCustomizer(LeanplumPushNotificationCustomizer)} to activate this method.
   * Leave this method empty if you want to support 2 lines of text
   * in BigPicture style push notification and implement {@link
   * LeanplumPushNotificationCustomizer#customize(Notification.Builder, Bundle, Notification.Style)}
   *
   * @param builder NotificationCompat.Builder for push notification.
   * @param notificationPayload Bundle notification payload.
   */
  void customize(NotificationCompat.Builder builder, Bundle notificationPayload);

  /**
   * Implement this method to support 2 lines of text in BigPicture style push notification,
   * otherwise implement {@link
   * LeanplumPushNotificationCustomizer#customize(NotificationCompat.Builder, Bundle)}  and leave
   * this method empty. Please call {@link
   * LeanplumPushService#setCustomizer(LeanplumPushNotificationCustomizer, boolean)}  with true
   * value to activate this method.
   *
   * @param builder Notification.Builder for push notification.
   * @param notificationPayload Bundle notification payload.
   * @param notificationStyle - Notification.BigPictureStyle or null - BigPicture style for current
   * push notification. Call ((Notification.BigPictureStyle) notificationStyle).bigLargeIcon(largeIcon)
   * if you want to set large icon on expanded push notification. If notificationStyle wasn't null
   * it will be set to push notification. Note: If you call notificationStyle = new
   * Notification.BigPictureStyle() or other Notification.Style - there will be no support 2 lines
   * of text on BigPicture push and you need to call builder.setStyle(notificationStyle) to set
   * yours expanded layout for push notification.
   */
  void customize(Notification.Builder builder, Bundle notificationPayload,
      @Nullable Notification.Style notificationStyle);
}
