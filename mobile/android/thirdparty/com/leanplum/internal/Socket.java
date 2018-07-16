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

import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;

import com.leanplum.ActionContext;
import com.leanplum.Leanplum;
import com.leanplum.LeanplumActivityHelper;
import com.leanplum.LeanplumEditorMode;
import com.leanplum.callbacks.VariablesChangedCallback;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.net.URI;
import java.net.URISyntaxException;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.Timer;
import java.util.TimerTask;

/**
 * Leanplum socket class, that handles connections to the Leanplum remote socket.
 *
 * @author Andrew First, Ben Marten
 */
// Suppressing deprecated apache dependency.
@SuppressWarnings("deprecation")
public class Socket {
  private static final String TAG = "Leanplum";
  private static final String EVENT_CONTENT_RESPONSE = "getContentResponse";
  private static final String EVENT_UPDATE_VARS = "updateVars";
  private static final String EVENT_GET_VIEW_HIERARCHY = "getViewHierarchy";
  private static final String EVENT_PREVIEW_UPDATE_RULES = "previewUpdateRules";
  private static final String EVENT_TRIGGER = "trigger";
  private static final String EVENT_GET_VARIABLES = "getVariables";
  private static final String EVENT_GET_ACTIONS = "getActions";
  private static final String EVENT_REGISTER_DEVICE = "registerDevice";
  private static final String EVENT_APPLY_VARS = "applyVars";

  private static Socket instance = new Socket();
  private SocketIOClient sio;
  private boolean authSent;
  private boolean connected = false;
  private boolean connecting = false;

  public Socket() {
    createSocketClient();
  }

  public static Socket getInstance() {
    return instance;
  }

  private void createSocketClient() {
    SocketIOClient.Handler socketIOClientHandler = new SocketIOClient.Handler() {
      @Override
      public void onError(Exception error) {
        Log.e("Development socket error", error);
      }

      @Override
      public void onDisconnect(int code, String reason) {
        Log.i("Disconnected from development server");
        connected = false;
        connecting = false;
        authSent = false;
      }

      @Override
      public void onConnect() {
        if (!authSent) {
          Log.i("Connected to development server");
          try {
            Map<String, String> args = Util.newMap(
                Constants.Params.APP_ID, Request.appId(),
                Constants.Params.DEVICE_ID, Request.deviceId());
            try {
              sio.emit("auth", new JSONArray(Collections.singletonList(new JSONObject(args))));
            } catch (JSONException e) {
              e.printStackTrace();
            }
          } catch (Throwable t) {
            Util.handleException(t);
          }
          authSent = true;
          connected = true;
          connecting = false;
        }
      }

      @Override
      public void on(String event, JSONArray arguments) {
        try {
          switch (event) {
            case EVENT_UPDATE_VARS:
              Leanplum.forceContentUpdate();
              break;
            case EVENT_TRIGGER:
              handleTriggerEvent(arguments);
              break;
            case EVENT_GET_VIEW_HIERARCHY:
              LeanplumUIEditorWrapper.getInstance().startUpdating();
              LeanplumUIEditorWrapper.getInstance().sendUpdate();
              break;
            case EVENT_PREVIEW_UPDATE_RULES:
              previewUpdateRules(arguments);
              break;
            case EVENT_GET_VARIABLES:
              handleGetVariablesEvent();
              break;
            case EVENT_GET_ACTIONS:
              handleGetActionsEvent();
              break;
            case EVENT_REGISTER_DEVICE:
              handleRegisterDeviceEvent(arguments);
              break;
            case EVENT_APPLY_VARS:
              handleApplyVarsEvent(arguments);
              break;
            default:
              break;
          }
        } catch (Throwable t) {
          Util.handleException(t);
        }
      }
    };

    try {
      sio = new SocketIOClient(new URI("http://" + Constants.SOCKET_HOST + ":" +
          Constants.SOCKET_PORT), socketIOClientHandler);
    } catch (URISyntaxException e) {
      Log.e(e.getMessage());
    }
    connect();
    Timer reconnectTimer = new Timer();
    reconnectTimer.schedule(new TimerTask() {
      @Override
      public void run() {
        try {
          reconnect();
        } catch (Throwable t) {
          Util.handleException(t);
        }
      }
    }, 0, 5000);
  }

  /**
   * Connect to the remote socket.
   */
  private void connect() {
    connecting = true;
    sio.connect();
  }

  /**
   * Disconnect from the remote socket.
   */
  private void reconnect() {
    if (!connected && !connecting) {
      connect();
    }
  }

  /**
   * Send a given event and data to the remote socket server.
   *
   * @param eventName The name of the event.
   * @param data The data to be sent to the remote server.
   */
  public <T> void sendEvent(String eventName, Map<String, T> data) {
    try {
      Log.p("Sending event: " + eventName + " & data over socket:\n" + data);
      sio.emit(eventName,
          new JSONArray(Collections.singletonList(JsonConverter.mapToJsonObject(data))));
    } catch (JSONException e) {
      Log.e("Failed to create JSON data object: " + e.getMessage());
    }
  }

  /**
   * Handles the "trigger" event received from server.
   *
   * @param arguments The arguments received from server.
   */
  void handleTriggerEvent(JSONArray arguments) {
    // Trigger a custom action.
    try {
      JSONObject payload = arguments.getJSONObject(0);
      JSONObject actionJson = payload.getJSONObject(Constants.Params.ACTION);
      if (actionJson != null) {
        String messageId = payload.getString(Constants.Params.MESSAGE_ID);
        boolean isRooted = payload.getBoolean("isRooted");
        String actionType = actionJson.getString(Constants.Values.ACTION_ARG);
        Map<String, Object> defaultDefinition = CollectionUtil.uncheckedCast(
            VarCache.actionDefinitions().get(actionType));
        Map<String, Object> defaultArgs = null;
        if (defaultDefinition != null) {
          defaultArgs = CollectionUtil.uncheckedCast(defaultDefinition.get("values"));
        }
        Map<String, Object> action = JsonConverter.mapFromJson(actionJson);
        action = CollectionUtil.uncheckedCast(VarCache.mergeHelper(defaultArgs, action));
        ActionContext context = new ActionContext(actionType, action, messageId);
        context.preventRealtimeUpdating();
        ((BaseActionContext) context).setIsRooted(isRooted);
        ((BaseActionContext) context).setIsPreview(true);
        context.update();
        LeanplumInternal.triggerAction(context);
        ActionManager.getInstance().recordMessageImpression(messageId);
      }
    } catch (JSONException e) {
      Log.e("Error getting action info", e);
    }
  }

  /**
   * Handles the "getVariables" event received from server.
   */
  public void handleGetVariablesEvent() {
    boolean sentValues = VarCache.sendVariablesIfChanged();
    VarCache.maybeUploadNewFiles();
    sendEvent(EVENT_CONTENT_RESPONSE, Util.newMap("updated", sentValues));
  }

  /**
   * Handles the "getActions" event received from server.
   */
  void handleGetActionsEvent() {
    boolean sentValues = VarCache.sendActionsIfChanged();
    VarCache.maybeUploadNewFiles();
    sendEvent(EVENT_CONTENT_RESPONSE, Util.newMap("updated", sentValues));
  }

  /**
   * Handles the "registerDevice" event received from server.
   *
   * @param arguments The arguments received from server.
   */
  void handleRegisterDeviceEvent(JSONArray arguments) {
    LeanplumInternal.onHasStartedAndRegisteredAsDeveloper();
    String emailArg = null;
    try {
      emailArg = arguments.getJSONObject(0).getString("email");
    } catch (JSONException e) {
      Log.v("Socket - No developer e-mail provided.");
    }
    final String email = (emailArg == null) ? "a Leanplum account" : emailArg;
    OsHandler.getInstance().post(new Runnable() {
      @Override
      public void run() {
        LeanplumActivityHelper.queueActionUponActive(new VariablesChangedCallback() {
          @Override
          public void variablesChanged() {
            Activity activity = LeanplumActivityHelper.getCurrentActivity();
            AlertDialog.Builder alert = new AlertDialog.Builder(activity);
            alert.setTitle(TAG);
            alert.setMessage("Your device is registered to " + email + ".");
            alert.setPositiveButton("OK", new DialogInterface.OnClickListener() {
              @Override
              public void onClick(DialogInterface dialog, int which) {
              }
            });
            alert.show();
          }
        });
      }
    });
  }

  /**
   * Apply variables passed in from applyVars endpoint.
   */
  static void handleApplyVarsEvent(JSONArray args) {
    if (args == null) {
      return;
    }

    try {
      JSONObject object = args.getJSONObject(0);
      if (object == null) {
        return;
      }
      VarCache.applyVariableDiffs(
          JsonConverter.mapFromJson(object), null, null, null, null, null);
    } catch (JSONException e) {
      Log.e("Couldn't applyVars for preview.", e);
    } catch (Throwable e) {
      Util.handleException(e);
    }
  }

  void previewUpdateRules(JSONArray arguments) {
    JSONObject packetData;
    try {
      packetData = arguments.getJSONObject(0);
    } catch (Exception e) {
      Log.e("Error parsing data");
      return;
    }

    if (!packetData.optBoolean("closed")) {
      LeanplumUIEditorWrapper.getInstance().startUpdating();
    } else {
      LeanplumUIEditorWrapper.getInstance().stopUpdating();
    }

    LeanplumEditorMode mode;
    int intMode = packetData.optInt("mode");
    if (intMode >= LeanplumEditorMode.values().length) {
      Log.p("Invalid editor mode in packet");
      mode = LeanplumEditorMode.LP_EDITOR_MODE_INTERFACE;
    } else {
      mode = LeanplumEditorMode.values()[intMode];
    }
    LeanplumUIEditorWrapper.getInstance().setMode(mode);

    JSONArray rules = packetData.optJSONArray("rules");
    if (rules != null) {
      List<Map<String, Object>> ruleDiffs = JsonConverter.listFromJson(rules);
      VarCache.applyUpdateRuleDiffs(ruleDiffs);
    }

    LeanplumUIEditorWrapper.getInstance().sendUpdateDelayedDefault();
  }

  /**
   * Returns whether the socket connection is established
   *
   * @return true if connected
   */
  public boolean isConnected() {
    return connected;
  }
}
