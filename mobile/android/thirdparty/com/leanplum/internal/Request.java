/*
 * Copyright 2018, Leanplum, Inc. All rights reserved.
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

import com.leanplum.Leanplum;

import java.util.Map;

public class Request implements Requesting {

  private final String httpMethod;
  private final String apiMethod;
  private final Map<String, Object> params;
  private ResponseCallback response = null;
  private ErrorCallback error = null;
  private final CountAggregator countAggregator;

  public Request(String httpMethod, String apiMethod, Map<String, Object> params) {
    this.httpMethod = httpMethod;
    this.apiMethod = apiMethod;
    this.params = params;
    this.countAggregator = Leanplum.countAggregator();
  }

  public static Request get(String apiMethod, Map<String, Object> params) {
    Log.LeanplumLogType level = Constants.Methods.LOG.equals(apiMethod) ?
        Log.LeanplumLogType.DEBUG : Log.LeanplumLogType.VERBOSE;
    Log.log(level, "Will call API method " + apiMethod + " with arguments " + params);

    Leanplum.countAggregator().incrementCount("get_lprequest");

    return new Request("GET", apiMethod, params);
  }

  public static Request post(String apiMethod, Map<String, Object> params) {
    Log.LeanplumLogType level = Constants.Methods.LOG.equals(apiMethod) ?
        Log.LeanplumLogType.DEBUG : Log.LeanplumLogType.VERBOSE;
    Log.log(level, "Will call API method " + apiMethod + " with arguments " + params);

    Leanplum.countAggregator().incrementCount("post_lprequest");

    return new Request("POST", apiMethod, params);
  }

  public void onResponse(ResponseCallback response) {
    this.response = response;
    this.countAggregator.incrementCount("on_response_lprequest");
  }

  public void onError(ErrorCallback error) {
    this.error = error;
    this.countAggregator.incrementCount("on_error_lprequest");
  }
}
