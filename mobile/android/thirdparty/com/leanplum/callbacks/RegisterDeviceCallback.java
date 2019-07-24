/*
 * Copyright 2013, Leanplum, Inc. All rights reserved.
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

package com.leanplum.callbacks;

import com.leanplum.Leanplum;

/**
 * Callback that gets run when the device needs to be registered.
 *
 * @author Andrew First
 */
public abstract class RegisterDeviceCallback implements Runnable {
  public static abstract class EmailCallback implements Runnable {
    private String email;

    public void setResponseHandler(String email) {
      this.email = email;
    }

    public void run() {
      this.onResponse(email);
    }

    public abstract void onResponse(String email);
  }

  private EmailCallback callback;

  public void setResponseHandler(EmailCallback callback) {
    this.callback = callback;
    Leanplum.countAggregator().incrementCount("init_with_callback");
  }

  public void run() { this.onResponse(callback);
  }

  public abstract void onResponse(EmailCallback callback);
}
