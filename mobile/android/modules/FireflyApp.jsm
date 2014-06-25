/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["FireflyApp"];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");

// meta constants
const HEADER_META = ".meta";
const PROTO_CMD_KEY = "cmd";
const PROTO_CMD_VALUE_CREATE_SESSION = "create-session";
const PROTO_CMD_VALUE_END_SESSION = "end-session";
const PROTO_CMD_VALUE_ATTACH_APP = "attach-app";
const PROTO_CMD_VALUE_SET_WIFI = "set-wifi";
const PROTO_CMD_RESPONSE_PREFIX = "~";
const PROTO_CMD_VALUE_START_APP = "start-app";
const PROTO_CMD_VALUE_CANCEL_START_APP = "cancel-start-app";
const PROTO_CMD_VALUE_DOWNLODING_APP = "downloading-app";
const PROTO_CMD_VALUE_ATTACH_APP_MANAGER = "attach-app-manager";
const PROTO_CMD_VALUE_EXCEPTION = "exception";
const PROTO_EXCEPTION_KEY = "exception";
const PROTO_EXCEPTION_NO_KEY = "errno";
const PROTO_STATUS_VALUE_APP_CLOSED = "app-closed";
const PROTO_STATUS_VALUE_TRY_LATER = "try-later";
const PROTO_STATUS_VALUE_CANCELLED = "cancelled";
const PROTO_STATUS_VALUE_REFUSED = "refused";
const PROTO_STATUS_VALUE_TIMEOUT = "time-out";
const PROTO_STATUS_VALUE_INVALID_APP = "app_not_exist";
const PROTO_STATUS_VALUE_APP_CHECK_FAIL = "app_check_fail";
const PROTO_STATUS_VALUE_APP_INSTALL_FAIL = "app_install_fail";
const PROTO_STATUS_VALUE_APP_DOWNLOAD_FAIL = "app_download_fail";
const PROTO_STATUS_VALUE_APP_LAUNCH_FAIL = "app_launch_fail";
const PROTO_STATUS_KEY = "status";
const PROTO_STATUS_VALUE_OK = "OK";
const PROTO_STATUS_VALUE_FAILED = "FAILED_REASON";
const PROTO_EXCEPTION_NO_APP_NAME_NULL = 10;
const PROTO_EXCEPTION_NO_MESSAGE_NULL = 11;
const PROTO_EXCEPTION_NO_NEW_GROUP_START = 12;
const PROTO_EXCEPTION_NO_UNKNOW_CMD = 13;
const PROTO_EXCEPTION_NO_JSON_DATA_ERROR = 14;
const PARAM_APP_NAME = "app-name";
const PROTO_HOME_APP = "home";
const PARAM_DOWNLOADING_PERCENT = "download-percent";
const PARAM_KEEP_SESSION = "keep-session";
const PROTO_SENDER_HOST_NAME_KEY = "sender-host-name";
const PROTO_SENDER_PORT_KEY = "sender-port";
const PROTO_APP_META_KEY = "app-meta";
const PROTO_APP_META_NAME = "name";
const PROTO_APP_META_TITLE = "title";
const PROTO_APP_META_IMG_URI = "img-uri";
const PROTO_MIME_DATA_KEY = "mime-data";
const PROTO_APP_NAME_KEY = "app-name";
const PARAM_WIFI_NAME = "wifi-name";
const PARAM_WIFI_PASSWORD = "wifi-password";
const PARAM_WIFI_TYPE = "wifi-type";
const PARAM_WIFI_KEY = "key";
const PROTO_MIME_DATA_KEY_TYPE = "type";
const PROTO_MIME_DATA_KEY_DATA = "data";

// RAMP constants
const RAMP_CMD_KEY_ID = "cmd_id";
const RAMP_CMD_KEY_TYPE = "type";
const RAMP_CMD_KEY_STATUS = "status";
const RAMP_CMD_KEY_URL = "url";
const RAMP_CMD_KEY_VALUE = "value";
const RAMP_CMD_KEY_VIDEO_NAME = "videoname";
const RAMP_CMD_KEY_EVENT_SEQ = "event_sequence";
const NAMESPACE_RAMP = "ramp";
const RAMP_CMD_ID_START = 0;
const RAMP_CMD_ID_INFO = 1;
const RAMP_CMD_ID_PLAY = 2;
const RAMP_CMD_ID_PAUSE = 3;
const RAMP_CMD_ID_STOP = 4;
const RAMP_CMD_ID_SEEKTO = 5;
const RAMP_CMD_ID_SETVOLUME = 6;
const RAMP_CMD_ID_MUTE = 7;
const RAMP_CMD_ID_DECONSTE = 8;
const RAMP_CMD_START = "START";
const RAMP_CMD_INFO = "INFO";
const RAMP_CMD_PLAY = "PLAY";
const RAMP_CMD_PAUSE = "PAUSE";
const RAMP_CMD_STOP = "STOP";
const RAMP_CMD_SEEKTO = "SEEKTO";
const RAMP_CMD_SETVOLUME = "SETVOLUME";
const RAMP_CMD_MUTE = "MUTE";
const RAMP_CMD_DECONSTE = "DECONSTE";
const RAMP_CAST_STATUS_EVENT_SEQUENCE = "event_sequence";
const RAMP_CAST_STATUS_STATE = "state";
const RAMP_CAST_STATUS_CONTENT_ID = "content_id";
const RAMP_CAST_STATUS_CURRENT_TIME = "current_time";
const RAMP_CAST_STATUS_DURATION = "duration";
const RAMP_CAST_STATUS_MUTED = "muted";
const RAMP_CAST_STATUS_TIME_PROGRESS = "time_progress";
const RAMP_CAST_STATUS_TITLE = "title";
const RAMP_CAST_STATUS_VOLUME = "volume";
const PLAYER_STATUS_PREPARING = 1;
const PLAYER_STATUS_PLAYING = 2;
const PLAYER_STATUS_PAUSE = 3;
const PLAYER_STATUS_STOP = 4;
const PLAYER_STATUS_IDLE = 5;
const PLAYER_STATUS_BUFFERING = 6;

function FireflyApp(service) {
  let uri = Services.io.newURI(service.location, null, null);
  this._ip = uri.host;
};

FireflyApp.prototype = {
  _ip: null,
  _port: 8888,
  _cmd_socket: null,
  _meta_callback: null,
  _ramp_callbacks: {},
  _mediaListener: null,
  _event_sequence: 0,
  status: "unloaded",
  _have_session: false,
  _info_timer: null,

  _send_meta_cmd: function(cmd, callback, extras) {
    this._meta_callback = callback;
    let msg = extras ? extras : {};
    msg.cmd = cmd;
    this._send_cmd(JSON.stringify([HEADER_META, msg]), 0);
  },

  _send_ramp_cmd: function(type, cmd_id, callback, extras) {
    let msg = extras ? extras : {};
    msg.cmd_id = cmd_id;
    msg.type = type;
    msg.event_sequence = this._event_sequence++;
    this._send_cmd(JSON.stringify([NAMESPACE_RAMP, msg]), 0);
  },

  _send_cmd: function(str, recursionDepth) {
    if (!this._cmd_socket) {
      let baseSocket = Cc["@mozilla.org/tcp-socket;1"].createInstance(Ci.nsIDOMTCPSocket);
      this._cmd_socket = baseSocket.open(this._ip, 8888, { useSecureTransport: false, binaryType: "string" });
      if (!(this._cmd_socket)) {
        dump("socket is null");
        return;
      }

      this._cmd_socket.ondata = function(response) {
        try {
          let data = JSON.parse(response.data);
          let res = data[1];
          switch (data[0]) {
            case ".meta":
              this._handle_meta_response(data[1]);
              return;
            case "ramp":
              this._handle_ramp_response(data[1]);
              return;
            default:
              dump("unknown response");
          }
        } catch(ex) {
          dump("error handling response: " + ex);
          if (!this._info_timer) {
            this._info_timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
            this._info_timer.init(this, 200, Ci.nsITimer.TYPE_ONE_SHOT);
          }
        }
      }.bind(this);

      this._cmd_socket.onerror = function(err) {
        this.shutdown();
        Cu.reportError("error: " + err.data.name);
        this._cmd_socket = null;
      }.bind(this);

      this._cmd_socket.onclose = function() {
        this.shutdown();
        this._cmd_socket = null;
        Cu.reportError("closed tcp socket")
      }.bind(this);

      this._cmd_socket.onopen = function() {
        if (recursionDepth <= 2) {
          this._send_cmd(str, ++recursionDepth);
        }
      }.bind(this);
    } else {
      try {
        this._cmd_socket.send(str, str.length);
        let data = JSON.parse(str);
        if (data[1][PARAM_APP_NAME] == PROTO_HOME_APP) {
          // assuming we got home OK
          if (this._meta_callback) {
            this._handle_meta_callback({ status: "OK" });
          }
        }
      } catch (ex) {
        this._cmd_socket = null;
        this._send_cmd(str);
      }
    }
  },

  observe: function(subject, data, topic) {
    if (data === "timer-callback") {
      this._info_timer = null;
      this._send_ramp_cmd(RAMP_CMD_INFO, RAMP_CMD_ID_INFO, null)
    }
  },

  start: function(func) {
    let cmd = this._have_session ? PROTO_CMD_VALUE_START_APP : PROTO_CMD_VALUE_CREATE_SESSION ;
    this._send_meta_cmd(cmd, func, { "app-name": "Remote Player" });
  },

  stop: function(func) {
    if (func) {
      func(true);
    }
  },

  remoteMedia: function(func, listener) {
    this._mediaListener = listener;
    func(this);
    if (listener) {
      listener.onRemoteMediaStart(this);
    }
  },

  _handle_meta_response: function(data) {
    switch(data.cmd) {
      case "create-session":
      case "~create-session":
        // if we get a response form start-app, assume we have a connection already
      case "start-app":
      case "~start-app":
        this._have_session = (data.status == "OK");
        break;
      case "end-session":
      case "~end-session":
        this._have_session = (data.status != "OK");
        break;
    }

    if (this._meta_callback) {
      let callback = this._meta_callback;
      this._meta_callback = null;
      callback(data.status == "OK");
    }
  },

  _handle_ramp_response: function(data) {
    switch (data.status.state) {
      case PLAYER_STATUS_PREPARING:
        this.status = "preparing";
        break;
      case PLAYER_STATUS_PLAYING:
        this.status = "started";
        break;
      case PLAYER_STATUS_PAUSE:
        this.status = "paused";
        break;
      case PLAYER_STATUS_STOP:
        this.status = "stopped";
        break;
      case PLAYER_STATUS_IDLE:
        this.status = "idle";
        break;
      case PLAYER_STATUS_BUFFERING:
        this.status = "buffering";
        break;
    }

    if (data.status.state == PLAYER_STATUS_STOP && data.status.current_time > 0 && data.status.current_time == data.status.duration) {
      this.status = "completed";
    } else if (!this._info_timer) {
      this._info_timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
      this._info_timer.init(this, 200, Ci.nsITimer.TYPE_ONE_SHOT);
    }

    if (this._mediaListener) {
      this._mediaListener.onRemoteMediaStatus(this);
    }
  },

  load: function(data) {
    let meta = {
      url: data.source,
      videoname: data.title
    };
    this._send_ramp_cmd(RAMP_CMD_START, RAMP_CMD_ID_START, null, meta);
  },

  play: function() {
    this._send_ramp_cmd(RAMP_CMD_PLAY, RAMP_CMD_ID_PLAY, null);
  },

  pause: function() {
    this._send_ramp_cmd(RAMP_CMD_PAUSE, RAMP_CMD_ID_PAUSE, null);
  },

  shutdown: function() {
    if (this._info_timer) {
      this._info_timer.clear();
      this._info_timer = null;
    }

    this.stop(function() {
      this._send_meta_cmd(PROTO_CMD_VALUE_END_SESSION);
      if (this._mediaListener) {
        this._mediaListener.onRemoteMediaStop(this);
      }
    }.bind(this));
  }
};
