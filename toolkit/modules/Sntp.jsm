/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "Sntp",
];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

// Set to true to see debug messages.
var DEBUG = false;

/**
 * Constructor of Sntp.
 *
 * @param dataAvailableCb
 *        Callback function gets called when SNTP offset available. Signature
 *        is function dataAvailableCb(offsetInMS).
 * @param maxRetryCount
 *        Maximum retry count when SNTP failed to connect to server; set to
 *        zero to disable the retry.
 * @param refreshPeriodInSecs
 *        Refresh period; set to zero to disable refresh.
 * @param timeoutInSecs
 *        Timeout value used for connection.
 * @param pools
 *        SNTP server lists separated by ';'.
 * @param port
 *        SNTP port.
 */
this.Sntp = function Sntp(dataAvailableCb, maxRetryCount, refreshPeriodInSecs,
                          timeoutInSecs, pools, port) {
  if (dataAvailableCb != null) {
    this._dataAvailableCb = dataAvailableCb;
  }
  if (maxRetryCount != null) {
    this._maxRetryCount = maxRetryCount;
  }
  if (refreshPeriodInSecs != null) {
    this._refreshPeriodInMS = refreshPeriodInSecs * 1000;
  }
  if (timeoutInSecs != null) {
    this._timeoutInMS = timeoutInSecs * 1000;
  }
  if (pools != null && Array.isArray(pools) && pools.length > 0) {
    this._pools = pools;
  }
  if (port != null) {
    this._port = port;
  }
}

Sntp.prototype = {
  isAvailable: function isAvailable() {
    return this._cachedOffset != null;
  },

  isExpired: function isExpired() {
    let valid = this._cachedOffset != null && this._cachedTimeInMS != null;
    if (this._refreshPeriodInMS > 0) {
      valid = valid && Date.now() < this._cachedTimeInMS +
                                    this._refreshPeriodInMS;
    }
    return !valid;
  },

  request: function request() {
    this._request();
  },

  getOffset: function getOffset() {
    return this._cachedOffset;
  },

  /**
   * Indicates the system clock has been changed by [offset]ms so we need to
   * adjust the stored value.
   */
  updateOffset: function updateOffset(offset) {
    if (this._cachedOffset != null) {
      this._cachedOffset -= offset;
    }
  },

  /**
   * Used to schedule a retry or periodic updates.
   */
  _schedule: function _schedule(timeInMS) {
    if (this._updateTimer == null) {
      this._updateTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    }

    this._updateTimer.initWithCallback(this._request.bind(this),
                                       timeInMS,
                                       Ci.nsITimer.TYPE_ONE_SHOT);
    debug("Scheduled SNTP request in " + timeInMS + "ms");
  },

  /**
   * Handle the SNTP response.
   */
  _handleSntp: function _handleSntp(originateTimeInMS, receiveTimeInMS,
                                    transmitTimeInMS, respondTimeInMS) {
    let clockOffset = Math.floor(((receiveTimeInMS - originateTimeInMS) +
                                 (transmitTimeInMS - respondTimeInMS)) / 2);
    debug("Clock offset: " + clockOffset);

    // We've succeeded so clear the retry status.
    this._retryCount = 0;
    this._retryPeriodInMS = 0;

    // Cache the latest SNTP offset whenever receiving it.
    this._cachedOffset = clockOffset;
    this._cachedTimeInMS = respondTimeInMS;

    if (this._dataAvailableCb != null) {
      this._dataAvailableCb(clockOffset);
    }

    this._schedule(this._refreshPeriodInMS);
  },

  /**
   * Used for retry SNTP requests.
   */
  _retry: function _retry() {
    this._retryCount++;
    if (this._retryCount > this._maxRetryCount) {
      debug("stop retrying SNTP");
      // Clear so we can start with clean status next time we have network.
      this._retryCount = 0;
      this._retryPeriodInMS = 0;
      return;
    }
    this._retryPeriodInMS = Math.max(1000, this._retryPeriodInMS * 2);

    this._schedule(this._retryPeriodInMS);
  },

  /**
   * Request SNTP.
   */
  _request: function _request() {
    function GetRequest() {
      let NTP_PACKET_SIZE = 48;
      let NTP_MODE_CLIENT = 3;
      let NTP_VERSION = 3;
      let TRANSMIT_TIME_OFFSET = 40;

      // Send the NTP request.
      let requestTimeInMS = Date.now();
      let s = requestTimeInMS / 1000;
      let ms = requestTimeInMS % 1000;
      // NTP time is relative to 1900.
      s += OFFSET_1900_TO_1970;
      let f = ms * 0x100000000 / 1000;
      s = Math.floor(s);
      f = Math.floor(f);

      let buffer = new ArrayBuffer(NTP_PACKET_SIZE);
      let data = new DataView(buffer);
      data.setUint8(0, NTP_MODE_CLIENT | (NTP_VERSION << 3));
      data.setUint32(TRANSMIT_TIME_OFFSET, s, false);
      data.setUint32(TRANSMIT_TIME_OFFSET + 4, f, false);

      return String.fromCharCode.apply(null, new Uint8Array(buffer));
    }

    function SNTPListener() {}
    SNTPListener.prototype = {
      onStartRequest: function onStartRequest(request, context) {
      },

      onStopRequest: function onStopRequest(request, context, status) {
        if (!Components.isSuccessCode(status)) {
          debug("Connection failed");
          this._requesting = false;
          this._retry();
        }
      }.bind(this),

      onDataAvailable: function onDataAvailable(request, context, inputStream,
                                                offset, count) {
        function GetTimeStamp(binaryInputStream) {
          let s = binaryInputStream.read32();
          let f = binaryInputStream.read32();
          return Math.floor(
            ((s - OFFSET_1900_TO_1970) * 1000) + ((f * 1000) / 0x100000000)
          );
        }
        debug("Data available: " + count + " bytes");

        try {
          let binaryInputStream = Cc["@mozilla.org/binaryinputstream;1"]
                                    .createInstance(Ci.nsIBinaryInputStream);
          binaryInputStream.setInputStream(inputStream);
          // We don't need first 24 bytes.
          for (let i = 0; i < 6; i++) {
            binaryInputStream.read32();
          }
          // Offset 24: originate time.
          let originateTimeInMS = GetTimeStamp(binaryInputStream);
          // Offset 32: receive time.
          let receiveTimeInMS = GetTimeStamp(binaryInputStream);
          // Offset 40: transmit time.
          let transmitTimeInMS = GetTimeStamp(binaryInputStream);
          let respondTimeInMS = Date.now();

          this._handleSntp(originateTimeInMS, receiveTimeInMS,
                           transmitTimeInMS, respondTimeInMS);
          this._requesting = false;
        } catch (e) {
          debug("SNTPListener Error: " + e.message);
          this._requesting = false;
          this._retry();
        }
        inputStream.close();
      }.bind(this)
    };

    function SNTPRequester() {}
    SNTPRequester.prototype = {
      onOutputStreamReady: function(stream) {
        try {
          let data = GetRequest();
          let bytes_write = stream.write(data, data.length);
          debug("SNTP: sent " + bytes_write + " bytes");
          stream.close();
        } catch (e) {
          debug("SNTPRequester Error: " + e.message);
          this._requesting = false;
          this._retry();
        }
      }.bind(this)
    };

    // Number of seconds between Jan 1, 1900 and Jan 1, 1970.
    // 70 years plus 17 leap days.
    let OFFSET_1900_TO_1970 = ((365 * 70) + 17) * 24 * 60 * 60;

    if (this._requesting) {
      return;
    }
    if (this._pools.length < 1) {
      debug("No server defined");
      return;
    }
    if (this._updateTimer) {
      this._updateTimer.cancel();
    }

    debug("Making request");
    this._requesting = true;

    let currentThread = Cc["@mozilla.org/thread-manager;1"]
                          .getService().currentThread;
    let socketTransportService =
      Cc["@mozilla.org/network/socket-transport-service;1"]
        .getService(Ci.nsISocketTransportService);
    let pump = Cc["@mozilla.org/network/input-stream-pump;1"]
                 .createInstance(Ci.nsIInputStreamPump);
    let transport = socketTransportService
      .createTransport(["udp"],
                       1,
                       this._pools[Math.floor(this._pools.length * Math.random())],
                       this._port,
                       null);

    transport.setTimeout(Ci.nsISocketTransport.TIMEOUT_CONNECT, this._timeoutInMS);
    transport.setTimeout(Ci.nsISocketTransport.TIMEOUT_READ_WRITE, this._timeoutInMS);

    let outStream = transport.openOutputStream(0, 0, 0)
                             .QueryInterface(Ci.nsIAsyncOutputStream);
    let inStream = transport.openInputStream(0, 0, 0);

    pump.init(inStream, -1, -1, 0, 0, false);
    pump.asyncRead(new SNTPListener(), null);

    outStream.asyncWait(new SNTPRequester(), 0, 0, currentThread);
  },

  // Callback function.
  _dataAvailableCb: null,

  // Sntp servers.
  _pools: [
    "0.pool.ntp.org",
    "1.pool.ntp.org",
    "2.pool.ntp.org",
    "3.pool.ntp.org"
  ],

  // The SNTP port.
  _port: 123,

  // Maximum retry count allowed when request failed.
  _maxRetryCount: 0,

  // Refresh period.
  _refreshPeriodInMS: 0,

  // Timeout value used for connecting.
  _timeoutInMS: 30 * 1000,

  // Cached SNTP offset.
  _cachedOffset: null,

  // The time point when we cache the offset.
  _cachedTimeInMS: null,

  // Flag to avoid redundant requests.
  _requesting: false,

  // Retry counter.
  _retryCount: 0,

  // Retry time offset (in seconds).
  _retryPeriodInMS: 0,

  // Timer used for retries & daily updates.
  _updateTimer: null
};

var debug;
if (DEBUG) {
  debug = function(s) {
    dump("-*- Sntp: " + s + "\n");
  };
} else {
  debug = function(s) {};
}
