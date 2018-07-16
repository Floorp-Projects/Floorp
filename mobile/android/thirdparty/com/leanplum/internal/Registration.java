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

package com.leanplum.internal;

import com.leanplum.callbacks.StartCallback;

import org.json.JSONObject;

import java.util.HashMap;
import java.util.Map;

public class Registration {
  public static void registerDevice(String email, final StartCallback callback) {
    Map<String, Object> params = new HashMap<>();
    params.put(Constants.Params.EMAIL, email);
    Request request = Request.post(Constants.Methods.REGISTER_FOR_DEVELOPMENT, params);
    request.onResponse(new Request.ResponseCallback() {
      @Override
      public void response(final JSONObject response) {
        OsHandler.getInstance().post(new Runnable() {
          @Override
          public void run() {
            try {
              boolean isSuccess = Request.isResponseSuccess(response);
              if (isSuccess) {
                if (callback != null) {
                  callback.onResponse(true);
                }
              } else {
                Log.e(Request.getResponseError(response));
                if (callback != null) {
                  callback.onResponse(false);
                }
              }
            } catch (Throwable t) {
              Util.handleException(t);
            }
          }
        });
      }
    });
    request.onError(new Request.ErrorCallback() {
      @Override
      public void error(final Exception e) {
        OsHandler.getInstance().post(new Runnable() {
          @Override
          public void run() {
            if (callback != null) {
              callback.onResponse(false);
            }
          }
        });
      }
    });
    request.sendIfConnected();
  }
}
