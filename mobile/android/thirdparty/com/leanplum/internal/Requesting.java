package com.leanplum.internal;

import org.json.JSONObject;

public interface Requesting {

  interface ResponseCallback {
    void response(JSONObject response);
  }

  interface ErrorCallback {
    void error(Exception e);
  }
}
