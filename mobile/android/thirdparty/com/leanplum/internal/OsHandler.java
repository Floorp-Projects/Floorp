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

package com.leanplum.internal;

import android.os.Handler;
import android.os.Looper;

/**
 * Wraps Handler while allowing overriding of methods that are needed for unit testing
 *
 * @author kkafadarov
 */
public class OsHandler {
  // Posts to UI thread. Visible for testing.
  public static OsHandler instance;

  final Handler handler = new Handler(Looper.getMainLooper());

  public Boolean post(Runnable runnable) {
    return handler.post(runnable);
  }

  public Boolean postDelayed(Runnable runnable, long lng) {
    return handler.postDelayed(runnable, lng);
  }

  public static OsHandler getInstance() {
    if (instance == null) {
      instance = new OsHandler();
    }
    return instance;
  }
}
