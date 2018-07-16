package com.leanplum;

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

import android.annotation.TargetApi;
import android.app.Notification;
import android.app.PendingIntent;
import android.app.job.JobInfo;
import android.app.job.JobScheduler;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.AdaptiveIconDrawable;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.os.Bundle;
import android.support.v4.app.NotificationCompat;
import android.text.TextUtils;
import android.util.TypedValue;
import android.widget.RemoteViews;

import com.leanplum.internal.Constants;
import com.leanplum.internal.JsonConverter;
import com.leanplum.internal.Log;
import com.leanplum.utils.BuildUtil;

import java.util.List;
import java.util.Map;
import java.util.Random;
import java.util.TreeSet;

/**
 * LeanplumNotificationHelper helper class for push notifications.
 *
 * @author Anna Orlova
 */
class LeanplumNotificationHelper {

    private static final int BIGPICTURE_TEXT_TOP_PADDING = -14;
    private static final int BIGPICTURE_TEXT_SIZE = 14;
    private static final String LEANPLUM_DEFAULT_PUSH_ICON = "leanplum_default_push_icon";

    /**
     * If notification channels are supported this method will try to create
     * NotificationCompat.Builder with default notification channel if default channel id is provided.
     * If notification channels not supported this method will return NotificationCompat.Builder for
     * context.
     *
     * @param context                        The application context.
     * @param isNotificationChannelSupported True if notification channels are supported.
     * @return NotificationCompat.Builder for provided context or null.
     */
    // NotificationCompat.Builder(Context context) constructor was deprecated in API level 26.
    @SuppressWarnings("deprecation")
    static NotificationCompat.Builder getDefaultCompatNotificationBuilder(Context context,
                                                                          boolean isNotificationChannelSupported) {
        if (!isNotificationChannelSupported) {
            return new NotificationCompat.Builder(context);
        }
        String channelId = LeanplumNotificationChannel.getDefaultNotificationChannelId(context);
        if (!TextUtils.isEmpty(channelId)) {
            return new NotificationCompat.Builder(context, channelId);
        } else {
            Log.w("Failed to post notification, there are no notification channels configured.");
            return null;
        }
    }

    /**
     * Starts push registration service to update GCM/FCM InstanceId token.
     *
     * @param context Current application context.
     * @param providerName Name of push notification provider.
     */
    static void startPushRegistrationService(Context context, String providerName) {
        try {
            if (context == null) {
                return;
            }
            Log.i("Updating " + providerName + " InstanceId token.");
            // Fetch updated Instance ID token and notify our app's server of any changes (if applicable).
            Intent intent = new Intent(context, LeanplumPushRegistrationService.class);
            context.startService(intent);
        } catch (Throwable t) {
            Log.e("Couldn't update " + providerName + " InstanceId token.", t);
        }
    }

    /**
     * Schedule JobService to JobScheduler.
     *
     * @param context Current application context.
     * @param clazz JobService class.
     * @param jobId JobService id.
     */
    @TargetApi(21)
    static void scheduleJobService(Context context, Class clazz, int jobId) {
        if (context == null) {
            return;
        }
        ComponentName serviceName = new ComponentName(context, clazz);
        JobScheduler jobScheduler =
                (JobScheduler) context.getSystemService(Context.JOB_SCHEDULER_SERVICE);
        if (jobScheduler != null) {
            jobId = verifyJobId(jobScheduler.getAllPendingJobs(), jobId);
            JobInfo startMyServiceJobInfo = new JobInfo.Builder(jobId, serviceName)
                    .setMinimumLatency(10).build();
            jobScheduler.schedule(startMyServiceJobInfo);
        }
    }

    /**
     * Verifies that jobId don't present on JobScheduler pending jobs. If jobId present on
     * JobScheduler pending jobs generates a new one.
     *
     * @param allPendingJobs List of current pending jobs.
     * @param jobId JobService id.
     * @return jobId if jobId don't present on JobScheduler pending jobs
     */
    @TargetApi(21)
    private static int verifyJobId(List<JobInfo> allPendingJobs, int jobId) {
        if (allPendingJobs != null && !allPendingJobs.isEmpty()) {
            TreeSet<Integer> idsSet = new TreeSet<>();
            for (JobInfo jobInfo : allPendingJobs) {
                idsSet.add(jobInfo.getId());
            }
            if (idsSet.contains(jobId)) {
                if (idsSet.first() > Integer.MIN_VALUE) {
                    jobId = idsSet.first() - 1;
                } else if (idsSet.last() < Integer.MIN_VALUE) {
                    jobId = idsSet.last() + 1;
                } else {
                    while (idsSet.contains(jobId)) {
                        jobId = new Random().nextInt();
                    }
                }
            }
        }
        return jobId;
    }

    /**
     * If notification channels are supported this method will try to create
     * Notification.Builder with default notification channel if default channel id is provided.
     * If notification channels not supported this method will return Notification.Builder for
     * context.
     *
     * @param context                        The application context.
     * @param isNotificationChannelSupported True if notification channels are supported.
     * @return Notification.Builder for provided context or null.
     */
    // Notification.Builder(Context context) constructor was deprecated in API level 26.
    @TargetApi(Build.VERSION_CODES.O)
    @SuppressWarnings("deprecation")
    private static Notification.Builder getDefaultNotificationBuilder(Context context,
                                                                      boolean isNotificationChannelSupported) {
        if (!isNotificationChannelSupported) {
            return new Notification.Builder(context);
        }
        String channelId = LeanplumNotificationChannel.getDefaultNotificationChannelId(context);
        if (!TextUtils.isEmpty(channelId)) {
            return new Notification.Builder(context, channelId);
        } else {
            Log.w("Failed to post notification, there are no notification channels configured.");
            return null;
        }
    }

    /**
     * If notification channels are supported this method will try to create a channel with
     * information from the message if it doesn't exist and return NotificationCompat.Builder for this
     * channel. In the case where no channel information inside the message, we will try to get a
     * channel with default channel id. If notification channels not supported this method will return
     * NotificationCompat.Builder for context.
     *
     * @param context The application context.
     * @param message Push notification Bundle.
     * @return NotificationCompat.Builder or null.
     */
    // NotificationCompat.Builder(Context context) constructor was deprecated in API level 26.
    @SuppressWarnings("deprecation")
    static NotificationCompat.Builder getNotificationCompatBuilder(Context context, Bundle message) {
        NotificationCompat.Builder builder = null;
        // If we are targeting API 26, try to find supplied channel to post notification.
        if (BuildUtil.isNotificationChannelSupported(context)) {
            try {
                String channel = message.getString("lp_channel");
                if (!TextUtils.isEmpty(channel)) {
                    // Create channel if it doesn't exist and post notification to that channel.
                    Map<String, Object> channelDetails = JsonConverter.fromJson(channel);
                    String channelId = LeanplumNotificationChannel.createNotificationChannel(context,
                            channelDetails);
                    if (!TextUtils.isEmpty(channelId)) {
                        builder = new NotificationCompat.Builder(context, channelId);
                    } else {
                        Log.w("Failed to post notification to specified channel.");
                    }
                } else {
                    // If channel isn't supplied, try to look up for default channel.
                    builder = LeanplumNotificationHelper.getDefaultCompatNotificationBuilder(context, true);
                }
            } catch (Exception e) {
                Log.e("Failed to post notification to specified channel.");
            }
        } else {
            builder = new NotificationCompat.Builder(context);
        }
        return builder;
    }

    /**
     * If notification channels are supported this method will try to create a channel with
     * information from the message if it doesn't exist and return Notification.Builder for this
     * channel. In the case where no channel information inside the message, we will try to get a
     * channel with default channel id. If notification channels not supported this method will return
     * Notification.Builder for context.
     *
     * @param context The application context.
     * @param message Push notification Bundle.
     * @return Notification.Builder or null.
     */
    static Notification.Builder getNotificationBuilder(Context context, Bundle message) {
        Notification.Builder builder = null;
        // If we are targeting API 26, try to find supplied channel to post notification.
        if (BuildUtil.isNotificationChannelSupported(context)) {
            try {
                String channel = message.getString("lp_channel");
                if (!TextUtils.isEmpty(channel)) {
                    // Create channel if it doesn't exist and post notification to that channel.
                    Map<String, Object> channelDetails = JsonConverter.fromJson(channel);
                    String channelId = LeanplumNotificationChannel.createNotificationChannel(context,
                            channelDetails);
                    if (!TextUtils.isEmpty(channelId)) {
                        builder = new Notification.Builder(context, channelId);
                    } else {
                        Log.w("Failed to post notification to specified channel.");
                    }
                } else {
                    // If channel isn't supplied, try to look up for default channel.
                    builder = LeanplumNotificationHelper.getDefaultNotificationBuilder(context, true);
                }
            } catch (Exception e) {
                Log.e("Failed to post notification to specified channel.");
            }
        } else {
            builder = new Notification.Builder(context);
        }
        return builder;
    }

    /**
     * Gets Notification.Builder with 2 lines at BigPictureStyle notification text.
     *
     * @param context                           The application context.
     * @param message                           Push notification Bundle.
     * @param contentIntent                     PendingIntent.
     * @param title                             String with title for push notification.
     * @param messageText                       String with text for push notification.
     * @param bigPicture                        Bitmap for BigPictureStyle notification.
     * @param defaultNotificationIconResourceId int Resource id for default push notification icon.
     * @return Notification.Builder or null.
     */
    static Notification.Builder getNotificationBuilder(Context context, Bundle message,
                                                       PendingIntent contentIntent, String title, final String messageText, Bitmap bigPicture,
                                                       int defaultNotificationIconResourceId) {
        if (Build.VERSION.SDK_INT < 16) {
            return null;
        }
        Notification.Builder notificationBuilder =
                getNotificationBuilder(context, message);
        if (defaultNotificationIconResourceId == 0) {
            notificationBuilder.setSmallIcon(context.getApplicationInfo().icon);
        } else {
            notificationBuilder.setSmallIcon(defaultNotificationIconResourceId);
        }
        notificationBuilder.setContentTitle(title)
                .setContentText(messageText);
        Notification.BigPictureStyle bigPictureStyle = new Notification.BigPictureStyle() {
            @Override
            protected RemoteViews getStandardView(int layoutId) {
                RemoteViews remoteViews = super.getStandardView(layoutId);
                // Modifications of stanxdard push RemoteView.
                try {
                    int id = Resources.getSystem().getIdentifier("text", "id", "android");
                    remoteViews.setBoolean(id, "setSingleLine", false);
                    remoteViews.setInt(id, "setLines", 2);
                    if (Build.VERSION.SDK_INT < 23) {
                        // Make text smaller.
                        remoteViews.setViewPadding(id, 0, BIGPICTURE_TEXT_TOP_PADDING, 0, 0);
                        remoteViews.setTextViewTextSize(id, TypedValue.COMPLEX_UNIT_SP, BIGPICTURE_TEXT_SIZE);
                    }
                } catch (Throwable throwable) {
                    Log.e("Cannot modify push notification layout.");
                }
                return remoteViews;
            }
        };

        bigPictureStyle.bigPicture(bigPicture)
                .setBigContentTitle(title)
                .setSummaryText(message.getString(Constants.Keys.PUSH_MESSAGE_TEXT));
        notificationBuilder.setStyle(bigPictureStyle);

        if (Build.VERSION.SDK_INT >= 24) {
            // By default we cannot reach getStandardView method on API>=24. I we call
            // createBigContentView, Android will call getStandardView method and we can get
            // modified RemoteView.
            try {
                RemoteViews remoteView = notificationBuilder.createBigContentView();
                if (remoteView != null) {
                    // We need to set received RemoteView as a custom big content view.
                    notificationBuilder.setCustomBigContentView(remoteView);
                }
            } catch (Throwable t) {
                Log.e("Cannot modify push notification layout.", t);
            }
        }

        notificationBuilder.setAutoCancel(true);
        notificationBuilder.setContentIntent(contentIntent);
        return notificationBuilder;
    }

    /**
     * Checks a possibility to create icon drawable from current app icon.
     *
     * @param context Current application context.
     * @return boolean True if it is possible to create a drawable from current app icon.
     */
    private static boolean canCreateIconDrawable(Context context) {
        try {
            // Try to create icon drawable.
            Drawable drawable = AdaptiveIconDrawable.createFromStream(
                    context.getResources().openRawResource(context.getApplicationInfo().icon),
                    "applicationInfo.icon");
            // If there was no crash, we still need to check for null.
            if (drawable != null) {
                return true;
            }
        } catch (Throwable ignored) {
        }
        return false;
    }

    /**
     * Validation of Application icon for small icon on push notification.
     *
     * @param context Current application context.
     * @return boolean True if application icon can be used for small icon on push notification.
     */
    static boolean isApplicationIconValid(Context context) {
        if (context == null) {
            return false;
        }

        // TODO: Potentially there should be checked for Build.VERSION.SDK_INT != 26, but we need to
        // TODO: confirm that adaptive icon works well on 27, before to change it.
        if (Build.VERSION.SDK_INT < 26) {
            return true;
        }

        return canCreateIconDrawable(context);
    }

    /**
     * Gets default push notification resource id for LEANPLUM_DEFAULT_PUSH_ICON in drawable.
     *
     * @param context Current application context.
     * @return int Resource id.
     */
    static int getDefaultPushNotificationIconResourceId(Context context) {
        try {
            Resources resources = context.getResources();
            return resources.getIdentifier(LEANPLUM_DEFAULT_PUSH_ICON, "drawable",
                    context.getPackageName());
        } catch (Throwable ignored) {
            return 0;
        }
    }
}