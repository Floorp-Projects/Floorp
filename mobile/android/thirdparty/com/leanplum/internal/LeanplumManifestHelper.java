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

package com.leanplum.internal;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.ComponentInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.os.Bundle;

import com.leanplum.Leanplum;
import com.leanplum.LeanplumPushService;

import java.util.List;

/**
 * LeanplumManifestHelper class to work with AndroidManifest components.
 *
 * @author Anna Orlova
 */
public class LeanplumManifestHelper {
  // Google Cloud Messaging
  public static final String GCM_SEND_PERMISSION = "com.google.android.c2dm.permission.SEND";
  public static final String GCM_RECEIVE_PERMISSION = "com.google.android.c2dm.permission.RECEIVE";
  public static final String GCM_RECEIVE_ACTION = "com.google.android.c2dm.intent.RECEIVE";
  public static final String GCM_REGISTRATION_ACTION = "com.google.android.c2dm.intent.REGISTRATION";
  public static final String GCM_INSTANCE_ID_ACTION = "com.google.android.gms.iid.InstanceID";
  public static final String GCM_RECEIVER = "com.google.android.gms.gcm.GcmReceiver";

  // Firebase
  public static final String FCM_INSTANCE_ID_EVENT = "com.google.firebase.INSTANCE_ID_EVENT";
  public static final String FCM_MESSAGING_EVENT = "com.google.firebase.MESSAGING_EVENT";

  // Leanplum
  public static final String LP_PUSH_INSTANCE_ID_SERVICE = "com.leanplum.LeanplumPushInstanceIDService";
  public static final String LP_PUSH_LISTENER_SERVICE = "com.leanplum.LeanplumPushListenerService";
  public static final String LP_PUSH_FCM_LISTENER_SERVICE = "com.leanplum.LeanplumPushFcmListenerService";
  public static final String LP_PUSH_FCM_MESSAGING_SERVICE = "com.leanplum.LeanplumPushFirebaseMessagingService";
  public static final String LP_PUSH_REGISTRATION_SERVICE = "com.leanplum.LeanplumPushRegistrationService";
  public static final String LP_PUSH_RECEIVER = "com.leanplum.LeanplumPushReceiver";

  /**
   * Gets Class for name.
   *
   * @param className - class name.
   * @return Class for provided class name.
   */
  public static Class getClassForName(String className) {
    try {
      return Class.forName(className);
    } catch (Throwable t) {
      return null;
    }
  }

  /**
   * Enables and starts service for provided class name.
   *
   * @param context Current Context.
   * @param packageManager Current PackageManager.
   * @param clazz Class of service that needs to be enabled and started.
   * @return True if service was enabled and started.
   */
  public static boolean enableServiceAndStart(Context context, PackageManager packageManager,
      Class clazz) {
    if (!enableComponent(context, packageManager, clazz)) {
      return false;
    }
    try {
      context.startService(new Intent(context, clazz));
    } catch (Throwable t) {
      Log.w("Could not start service " + clazz.getName());
      return false;
    }
    return true;
  }

  /**
   * Enables component for provided class.
   *
   * @param context Current Context.
   * @param packageManager Current PackageManager.
   * @param clazz Class for enable.
   * @return True if component was enabled.
   */
  public static boolean enableComponent(Context context, PackageManager packageManager,
      Class clazz) {
    if (clazz == null || context == null || packageManager == null) {
      return false;
    }

    try {
      packageManager.setComponentEnabledSetting(new ComponentName(context, clazz),
          PackageManager.COMPONENT_ENABLED_STATE_ENABLED, PackageManager.DONT_KILL_APP);
    } catch (Throwable t) {
      Log.w("Could not enable component " + clazz.getName());
      return false;
    }
    return true;
  }

  /**
   * Disables component for provided class name.
   *
   * @param context The application context.
   * @param packageManager Application Package manager.
   * @param className Class name to disable.
   * @return True if component was disabled successfully, false otherwise.
   */
  public static boolean disableComponent(Context context, PackageManager packageManager, String className) {
    if (context == null || packageManager == null || className == null) {
      return false;
    }
    try {
      packageManager.setComponentEnabledSetting(new ComponentName(context, className),
          PackageManager.COMPONENT_ENABLED_STATE_DISABLED, PackageManager.DONT_KILL_APP);
    } catch (Throwable t) {
      return false;
    }
    return true;
  }

  /**
   * Checks if component for provided class enabled before.
   *
   * @param context Current Context.
   * @param packageManager Current PackageManager.
   * @param clazz Class for check.
   * @return True if component was enabled before.
   */
  public static boolean wasComponentEnabled(Context context, PackageManager packageManager,
      Class clazz) {
    if (clazz == null || context == null || packageManager == null) {
      return false;
    }
    int componentStatus = packageManager.getComponentEnabledSetting(new ComponentName(context,
        clazz));
    if (PackageManager.COMPONENT_ENABLED_STATE_DEFAULT == componentStatus ||
        PackageManager.COMPONENT_ENABLED_STATE_DISABLED == componentStatus) {
      return false;
    }
    return true;
  }

  /**
   * Parses and returns client broadcast receiver class name.
   *
   * @return Client broadcast receiver class name.
   */
  public static String parseNotificationMetadata() {
    try {
      Context context = Leanplum.getContext();
      ApplicationInfo app = context.getPackageManager().getApplicationInfo(context.getPackageName(),
          PackageManager.GET_META_DATA);
      Bundle bundle = app.metaData;
      return bundle.getString(LeanplumPushService.LEANPLUM_NOTIFICATION);
    } catch (Throwable ignored) {
    }
    return null;
  }

  public static boolean checkPermission(String permission, boolean definesPermission,
      boolean logError) {
    Context context = Leanplum.getContext();
    if (context == null) {
      return false;
    }
    int result = context.checkCallingOrSelfPermission(permission);
    if (result != PackageManager.PERMISSION_GRANTED) {
      String definition;
      if (definesPermission) {
        definition = "<permission android:name=\"" + permission +
            "\" android:protectionLevel=\"signature\" />\n";
      } else {
        definition = "";
      }
      if (logError) {
        Log.e("In order to use push notifications, you need to enable " +
            "the " + permission + " permission in your AndroidManifest.xml file. " +
            "Add this within the <manifest> section:\n" +
            definition + "<uses-permission android:name=\"" + permission + "\" />");
      }
      return false;
    }
    return true;
  }

  /**
   * Verifies that a certain component (receiver or sender) is implemented in the
   * AndroidManifest.xml file or the application, in order to make sure that push notifications
   * work.
   *
   * @param componentType A receiver or a service.
   * @param name The name of the class.
   * @param exported What the exported option should be.
   * @param permission Whether we need any permission.
   * @param actions What actions we need to check for in the intent-filter.
   * @param packageName The package name for the category tag, if we require one.
   * @return true if the respective component is in the manifest file, and false otherwise.
   */
  public static boolean checkComponent(ApplicationComponent componentType, String name,
      boolean exported, String permission, List<String> actions, String packageName) {
    Context context = Leanplum.getContext();
    if (context == null) {
      return false;
    }
    if (actions != null) {
      for (String action : actions) {
        List<ResolveInfo> components = (componentType == ApplicationComponent.RECEIVER)
            ? context.getPackageManager().queryBroadcastReceivers(new Intent(action), 0)
            : context.getPackageManager().queryIntentServices(new Intent(action), 0);
        if (components == null) {
          return false;
        }
        boolean foundComponent = false;
        for (ResolveInfo component : components) {
          if (component == null) {
            continue;
          }
          ComponentInfo componentInfo = (componentType == ApplicationComponent.RECEIVER)
              ? component.activityInfo : component.serviceInfo;
          if (componentInfo != null && componentInfo.name.equals(name)) {
            // Only check components from our package.
            if (componentInfo.packageName != null && componentInfo.packageName.equals(packageName)) {
              foundComponent = true;
            }
          }
        }
        if (!foundComponent) {
          Log.e(getComponentError(componentType, name, exported,
              permission, actions, packageName));
          return false;
        }
      }
    } else {
      try {
        if (componentType == ApplicationComponent.RECEIVER) {
          context.getPackageManager().getReceiverInfo(
              new ComponentName(context.getPackageName(), name), 0);
        } else {
          context.getPackageManager().getServiceInfo(
              new ComponentName(context.getPackageName(), name), 0);
        }
      } catch (PackageManager.NameNotFoundException e) {
        Log.e(getComponentError(componentType, name, exported,
            permission, actions, packageName));
        return false;
      }
    }
    return true;
  }

  /**
   * Formats error if component isn't found in app's manifest.
   *
   * @param componentType The component type to format.
   * @param name Name of the component to format.
   * @param exported Whether component is exported.
   * @param permission Permission to format.
   * @param actions Actions to format.
   * @param packageName Package name to format
   * @return Formatted error message to be printed to console.
   */
  private static String getComponentError(ApplicationComponent componentType, String name,
      boolean exported, String permission, List<String> actions, String packageName) {
    StringBuilder errorMessage = new StringBuilder("Push notifications requires you to add the " +
        componentType.name().toLowerCase() + " " + name + " to your AndroidManifest.xml file." +
        "Add this code within the <application> section:\n");
    errorMessage.append("<").append(componentType.name().toLowerCase()).append("\n");
    errorMessage.append("    ").append("android:name=\"").append(name).append("\"\n");
    errorMessage.append("    android:exported=\"").append(Boolean.toString(exported)).append("\"");
    if (permission != null) {
      errorMessage.append("\n    android:permission=\"").append(permission).append("\"");
    }
    errorMessage.append(">\n");
    if (actions != null) {
      errorMessage.append("    <intent-filter>\n");
      for (String action : actions) {
        errorMessage.append("        <action android:name=\"").append(action).append("\" />\n");
      }
      if (packageName != null) {
        errorMessage.append("        <category android:name=\"").append(packageName).append("\" />\n");
      }
      errorMessage.append("    </intent-filter>\n");
    }
    errorMessage.append("</").append(componentType.name().toLowerCase()).append(">");
    return errorMessage.toString();
  }

  public enum ApplicationComponent {SERVICE, RECEIVER}
}