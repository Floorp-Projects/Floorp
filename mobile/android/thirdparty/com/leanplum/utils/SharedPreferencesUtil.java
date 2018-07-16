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

package com.leanplum.utils;

import android.content.Context;
import android.content.SharedPreferences;

/**
 * Shared preferences manipulation utilities.
 *
 * @author Anna Orlova
 */
public class SharedPreferencesUtil {
  public static final String DEFAULT_STRING_VALUE = "";

  /**
   * Gets string value for key from shared preferences.
   *
   * @param context Application context.
   * @param sharedPreferenceName Shared preference name.
   * @param key Key of preference.
   * @return String Value for key, if here no value - return DEFAULT_STRING_VALUE.
   */
  public static String getString(Context context, String sharedPreferenceName, String key) {
    final SharedPreferences sharedPreferences = getPreferences(context, sharedPreferenceName);
    return sharedPreferences.getString(key, DEFAULT_STRING_VALUE);
  }

  /**
   * Get application shared preferences with sharedPreferenceName name.
   *
   * @param context Application context.
   * @param sharedPreferenceName Shared preference name.
   * @return Application's {@code SharedPreferences}.
   */
  private static SharedPreferences getPreferences(Context context, String sharedPreferenceName) {
    return context.getSharedPreferences(sharedPreferenceName, Context.MODE_PRIVATE);
  }

  /**
   * Sets string value for provided key to shared preference with sharedPreferenceName name.
   *
   * @param context application context.
   * @param sharedPreferenceName shared preference name.
   * @param key key of preference.
   * @param value value of preference.
   */
  public static void setString(Context context, String sharedPreferenceName, String key,
      String value) {
    final SharedPreferences sharedPreferences = getPreferences(context, sharedPreferenceName);
    SharedPreferences.Editor editor = sharedPreferences.edit();
    editor.putString(key, value);
    commitChanges(editor);
  }

  public static void commitChanges(SharedPreferences.Editor editor){
    try {
      editor.apply();
    } catch (NoSuchMethodError e) {
      editor.commit();
    }
  }
}
