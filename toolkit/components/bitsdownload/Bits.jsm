/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This module is used to interact with the Windows BITS component (Background
 * Intelligent Transfer Service). This functionality cannot be used unless on
 * Windows.
 *
 * The reason for this file's existence is that the interfaces in nsIBits.idl
 * are asynchronous, but are unable to use Promises because they are implemented
 * in Rust, which does not yet support Promises. This file functions as a layer
 * between the Rust and the JS that provides access to the functionality
 * provided by nsIBits via Promises rather than callbacks.
 */

"use strict";

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

// This conditional prevents errors if this file is imported from operating
// systems other than Windows. This is purely for convenient importing, because
// attempting to use anything in this file on platforms other than Windows will
// result in an error.
if (AppConstants.MOZ_BITS_DOWNLOAD) {
  XPCOMUtils.defineLazyServiceGetter(
    this,
    "gBits",
    "@mozilla.org/bits;1",
    "nsIBits"
  );
}

// This value exists to mitigate a very unlikely problem: If a BITS method
// catastrophically fails, it may never call its callback. This would result in
// methods in this file returning promises that never resolve. This could, in
// turn, lead to download code hanging altogether rather than being able to
// report errors and utilize fallback mechanisms.
// This problem is mitigated by giving these promises a timeout, the length of
// which will be determined by this value.
const kBitsMethodTimeoutMs = 10 * 60 * 1000; // 10 minutes

/**
 * This class will wrap the errors returned by the nsIBits interface to make
 * them more uniform and more easily consumable.
 *
 * The values of stored by this error type are entirely numeric. This should
 * make them easier to consume with JS and telemetry, but does make them fairly
 * unreadable. nsIBits.idl will need to be referenced to look up what errors
 * the values correspond to.
 *
 * The type of BitsError.code is dependent on the value of BitsError.codeType.
 * It may be null, a number (corresponding to an nsresult or hresult value),
 * a string, or an exception.
 */
class BitsError extends Error {
  // If codeType == "none", code may be unspecified.
  constructor(type, action, stage, codeType, code) {
    let message =
      `${BitsError.name} {type: ${type}, action: ${action}, ` +
      `stage: ${stage}`;
    switch (codeType) {
      case gBits.ERROR_CODE_TYPE_NONE:
        code = null;
        message += ", codeType: none}";
        break;
      case gBits.ERROR_CODE_TYPE_NSRESULT:
        message += `, codeType: nsresult, code: ${code}}`;
        break;
      case gBits.ERROR_CODE_TYPE_HRESULT:
        message += `, codeType: hresult, code: ${code}}`;
        break;
      case gBits.ERROR_CODE_TYPE_STRING:
        message += `, codeType: string, code: ${JSON.stringify(code)}}`;
        break;
      case gBits.ERROR_CODE_TYPE_EXCEPTION:
        message += `, codeType: exception, code: ${code}}`;
        break;
      default:
        message += ", codeType: invalid}";
        break;
    }
    super(message);

    this.type = type;
    this.action = action;
    this.stage = stage;
    this.codeType = codeType;
    this.code = code;
    this.name = this.constructor.name;
    this.succeeded = false;
  }
}

// These specializations exist to make them easier to construct since they may
// need to be constructed outside of this file.
class BitsVerificationError extends BitsError {
  constructor() {
    super(
      Ci.nsIBits.ERROR_TYPE_VERIFICATION_FAILURE,
      Ci.nsIBits.ERROR_ACTION_NONE,
      Ci.nsIBits.ERROR_STAGE_VERIFICATION,
      Ci.nsIBits.ERROR_CODE_TYPE_NONE
    );
  }
}
class BitsUnknownError extends BitsError {
  constructor() {
    super(
      Ci.nsIBits.ERROR_TYPE_UNKNOWN,
      Ci.nsIBits.ERROR_ACTION_UNKNOWN,
      Ci.nsIBits.ERROR_STAGE_UNKNOWN,
      Ci.nsIBits.ERROR_CODE_TYPE_NONE
    );
  }
}

/**
 * Returns a timer object. If the timer expires, reject will be called with
 * a BitsError error. The timer's cancel method should be called if the promise
 * resolves or rejects without the timeout expiring.
 */
function makeTimeout(reject, errorAction) {
  let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  timer.initWithCallback(
    () => {
      let error = new BitsError(
        gBits.ERROR_TYPE_METHOD_TIMEOUT,
        errorAction,
        gBits.ERROR_STAGE_UNKNOWN,
        gBits.ERROR_CODE_TYPE_NONE
      );
      reject(error);
    },
    kBitsMethodTimeoutMs,
    Ci.nsITimer.TYPE_ONE_SHOT
  );
  return timer;
}

/**
 * This function does all of the wrapping and error handling for an async
 * BitsRequest method. This allows the implementations for those methods to
 * simply call this function with a closure that executes appropriate
 * nsIBitsRequest method.
 *
 * Specifically, this function takes an nsBitsErrorAction and a function.
 * The nsBitsErrorAction will be used when constructing a BitsError, if the
 * wrapper encounters an error.
 * The function will be passed the callback function that should be passed to
 * the nsIBitsRequest method.
 */
async function requestPromise(errorAction, actionFn) {
  return new Promise((resolve, reject) => {
    let timer = makeTimeout(reject, errorAction);

    let callback = {
      QueryInterface: ChromeUtils.generateQI([Ci.nsIBitsCallback]),
      success() {
        timer.cancel();
        resolve();
      },
      failure(type, action, stage) {
        timer.cancel();
        let error = new BitsError(
          type,
          action,
          stage,
          gBits.ERROR_CODE_TYPE_NONE
        );
        reject(error);
      },
      failureNsresult(type, action, stage, code) {
        timer.cancel();
        let error = new BitsError(
          type,
          action,
          stage,
          gBits.ERROR_CODE_TYPE_NSRESULT,
          code
        );
        reject(error);
      },
      failureHresult(type, action, stage, code) {
        timer.cancel();
        let error = new BitsError(
          type,
          action,
          stage,
          gBits.ERROR_CODE_TYPE_HRESULT,
          code
        );
        reject(error);
      },
      failureString(type, action, stage, message) {
        timer.cancel();
        let error = new BitsError(
          type,
          action,
          stage,
          gBits.ERROR_CODE_TYPE_STRING,
          message
        );
        reject(error);
      },
    };

    try {
      actionFn(callback);
    } catch (e) {
      let error = new BitsError(
        gBits.ERROR_TYPE_METHOD_THREW,
        errorAction,
        gBits.ERROR_STAGE_PRETASK,
        gBits.ERROR_CODE_TYPE_EXCEPTION,
        e
      );
      reject(error);
    }
  });
}

/**
 * This class is a wrapper around nsIBitsRequest that converts functions taking
 * callbacks to asynchronous functions. This class implements nsIRequest.
 *
 * Note that once the request has been shutdown, calling methods on it will
 * cause an exception to be thrown. The request will be shutdown automatically
 * when the BITS job is successfully completed or cancelled. If the request is
 * no longer needed, but the transfer is still in progress, the shutdown method
 * should be called manually to prevent memory leaks.
 * Getter methods (except loadGroup and loadFlags) should continue to be
 * accessible, even after shutdown.
 */
class BitsRequest {
  constructor(request) {
    this._request = request;
    this._request.QueryInterface(Ci.nsIBitsRequest);
  }

  /**
   * This function releases the Rust request that backs this wrapper. Calling
   * any method on this request after calling release will result in a BitsError
   * being thrown.
   *
   * This step is important, because otherwise a cycle exists that the cycle
   * collector doesn't know about. To break this cycle, either the Rust request
   * needs to let go of the observer, or the JS request wrapper needs to let go
   * of the Rust request (which is what we do here).
   *
   * Once there is support for cycle collection of cycles that extend through
   * Rust, this function may no longer be necessary.
   */
  shutdown() {
    if (this.hasShutdown) {
      return;
    }
    // Cache some values before we shut down so they are still available
    this._name = this._request.name;
    this._status = this._request.status;
    this._bitsId = this._request.bitsId;
    this._transferError = this._request.transferError;

    this._request = null;
  }

  /**
   * Allows consumers to determine if this request has been shutdown.
   */
  get hasShutdown() {
    return !this._request;
  }

  /**
   * This is the nsIRequest implementation. Since this._request is an
   * nsIRequest, these functions just call the corresponding method on it.
   *
   * Note that nsIBitsRequest does not yet properly implement load groups or
   * load flags. This class will still forward those calls, but they will have
   * not succeed.
   */
  get name() {
    if (!this._request) {
      return this._name;
    }
    return this._request.name;
  }
  isPending() {
    if (!this._request) {
      return false;
    }
    return this._request.isPending();
  }
  get status() {
    if (!this._request) {
      return this._status;
    }
    return this._request.status;
  }
  cancel(status) {
    return this.cancelAsync(status);
  }
  suspend() {
    if (!this._request) {
      throw new BitsError(
        Ci.nsIBits.ERROR_TYPE_USE_AFTER_REQUEST_SHUTDOWN,
        Ci.nsIBits.ERROR_ACTION_SUSPEND,
        Ci.nsIBits.ERROR_STAGE_PRETASK,
        Ci.nsIBits.ERROR_CODE_TYPE_NONE
      );
    }
    return this._request.suspend();
  }
  resume() {
    if (!this._request) {
      throw new BitsError(
        Ci.nsIBits.ERROR_TYPE_USE_AFTER_REQUEST_SHUTDOWN,
        Ci.nsIBits.ERROR_ACTION_RESUME,
        Ci.nsIBits.ERROR_STAGE_PRETASK,
        Ci.nsIBits.ERROR_CODE_TYPE_NONE
      );
    }
    return this._request.resume();
  }
  get loadGroup() {
    if (!this._request) {
      throw new BitsError(
        Ci.nsIBits.ERROR_TYPE_USE_AFTER_REQUEST_SHUTDOWN,
        Ci.nsIBits.ERROR_ACTION_NONE,
        Ci.nsIBits.ERROR_STAGE_PRETASK,
        Ci.nsIBits.ERROR_CODE_TYPE_NONE
      );
    }
    return this._request.loadGroup;
  }
  set loadGroup(group) {
    if (!this._request) {
      throw new BitsError(
        Ci.nsIBits.ERROR_TYPE_USE_AFTER_REQUEST_SHUTDOWN,
        Ci.nsIBits.ERROR_ACTION_NONE,
        Ci.nsIBits.ERROR_STAGE_PRETASK,
        Ci.nsIBits.ERROR_CODE_TYPE_NONE
      );
    }
    this._request.loadGroup = group;
  }
  get loadFlags() {
    if (!this._request) {
      throw new BitsError(
        Ci.nsIBits.ERROR_TYPE_USE_AFTER_REQUEST_SHUTDOWN,
        Ci.nsIBits.ERROR_ACTION_NONE,
        Ci.nsIBits.ERROR_STAGE_PRETASK,
        Ci.nsIBits.ERROR_CODE_TYPE_NONE
      );
    }
    return this._request.loadFlags;
  }
  set loadFlags(flags) {
    if (!this._request) {
      throw new BitsError(
        Ci.nsIBits.ERROR_TYPE_USE_AFTER_REQUEST_SHUTDOWN,
        Ci.nsIBits.ERROR_ACTION_NONE,
        Ci.nsIBits.ERROR_STAGE_PRETASK,
        Ci.nsIBits.ERROR_CODE_TYPE_NONE
      );
    }
    this._request.loadFlags = flags;
  }

  /**
   * This function wraps nsIBitsRequest::bitsId.
   */
  get bitsId() {
    if (!this._request) {
      return this._bitsId;
    }
    return this._request.bitsId;
  }

  /**
   * This function wraps nsIBitsRequest::transferError.
   *
   * Instead of simply returning the nsBitsErrorType value, however, it returns
   * a BitsError object, or null.
   */
  get transferError() {
    let result;
    if (this._request) {
      result = this._request.transferError;
    } else {
      result = this._transferError;
    }
    if (result == Ci.nsIBits.ERROR_TYPE_SUCCESS) {
      return null;
    }
    return new BitsError(
      result,
      Ci.nsIBits.ERROR_ACTION_NONE,
      Ci.nsIBits.ERROR_STAGE_MONITOR,
      Ci.nsIBits.ERROR_CODE_TYPE_NONE
    );
  }

  /**
   * This function wraps nsIBitsRequest::changeMonitorInterval.
   *
   * Instead of taking a callback, the function is asynchronous.
   * This method either resolves with no data, or rejects with a BitsError.
   */
  async changeMonitorInterval(monitorIntervalMs) {
    if (!this._request) {
      throw new BitsError(
        Ci.nsIBits.ERROR_TYPE_USE_AFTER_REQUEST_SHUTDOWN,
        Ci.nsIBits.ERROR_ACTION_CHANGE_MONITOR_INTERVAL,
        Ci.nsIBits.ERROR_STAGE_PRETASK,
        Ci.nsIBits.ERROR_CODE_TYPE_NONE
      );
    }
    let action = gBits.ERROR_ACTION_CHANGE_MONITOR_INTERVAL;
    return requestPromise(action, callback => {
      this._request.changeMonitorInterval(monitorIntervalMs, callback);
    });
  }

  /**
   * This function wraps nsIBitsRequest::cancelAsync.
   *
   * Instead of taking a callback, the function is asynchronous.
   * This method either resolves with no data, or rejects with a BitsError.
   *
   * Adds a default status of NS_ERROR_ABORT if one is not provided.
   */
  async cancelAsync(status) {
    if (!this._request) {
      throw new BitsError(
        Ci.nsIBits.ERROR_TYPE_USE_AFTER_REQUEST_SHUTDOWN,
        Ci.nsIBits.ERROR_ACTION_CANCEL,
        Ci.nsIBits.ERROR_STAGE_PRETASK,
        Ci.nsIBits.ERROR_CODE_TYPE_NONE
      );
    }
    if (status === undefined) {
      status = Cr.NS_ERROR_ABORT;
    }
    let action = gBits.ERROR_ACTION_CANCEL;
    return requestPromise(action, callback => {
      this._request.cancelAsync(status, callback);
    }).then(() => this.shutdown());
  }

  /**
   * This function wraps nsIBitsRequest::setPriorityHigh.
   *
   * Instead of taking a callback, the function is asynchronous.
   * This method either resolves with no data, or rejects with a BitsError.
   */
  async setPriorityHigh() {
    if (!this._request) {
      throw new BitsError(
        Ci.nsIBits.ERROR_TYPE_USE_AFTER_REQUEST_SHUTDOWN,
        Ci.nsIBits.ERROR_ACTION_SET_PRIORITY,
        Ci.nsIBits.ERROR_STAGE_PRETASK,
        Ci.nsIBits.ERROR_CODE_TYPE_NONE
      );
    }
    let action = gBits.ERROR_ACTION_SET_PRIORITY;
    return requestPromise(action, callback => {
      this._request.setPriorityHigh(callback);
    });
  }

  /**
   * This function wraps nsIBitsRequest::setPriorityLow.
   *
   * Instead of taking a callback, the function is asynchronous.
   * This method either resolves with no data, or rejects with a BitsError.
   */
  async setPriorityLow() {
    if (!this._request) {
      throw new BitsError(
        Ci.nsIBits.ERROR_TYPE_USE_AFTER_REQUEST_SHUTDOWN,
        Ci.nsIBits.ERROR_ACTION_SET_PRIORITY,
        Ci.nsIBits.ERROR_STAGE_PRETASK,
        Ci.nsIBits.ERROR_CODE_TYPE_NONE
      );
    }
    let action = gBits.ERROR_ACTION_SET_PRIORITY;
    return requestPromise(action, callback => {
      this._request.setPriorityLow(callback);
    });
  }

  /**
   * This function wraps nsIBitsRequest::setNoProgressTimeout.
   *
   * Instead of taking a callback, the function is asynchronous.
   * This method either resolves with no data, or rejects with a BitsError.
   */
  async setNoProgressTimeout(timeoutSecs) {
    if (!this._request) {
      throw new BitsError(
        Ci.nsIBits.ERROR_TYPE_USE_AFTER_REQUEST_SHUTDOWN,
        Ci.nsIBits.ERROR_ACTION_SET_NO_PROGRESS_TIMEOUT,
        Ci.nsIBits.ERROR_STAGE_PRETASK,
        Ci.nsIBits.ERROR_CODE_TYPE_NONE
      );
    }
    let action = gBits.ERROR_ACTION_SET_NO_PROGRESS_TIMEOUT;
    return requestPromise(action, callback => {
      this._request.setNoProgressTimeout(timeoutSecs, callback);
    });
  }

  /**
   * This function wraps nsIBitsRequest::complete.
   *
   * Instead of taking a callback, the function is asynchronous.
   * This method either resolves with no data, or rejects with a BitsError.
   */
  async complete() {
    if (!this._request) {
      throw new BitsError(
        Ci.nsIBits.ERROR_TYPE_USE_AFTER_REQUEST_SHUTDOWN,
        Ci.nsIBits.ERROR_ACTION_COMPLETE,
        Ci.nsIBits.ERROR_STAGE_PRETASK,
        Ci.nsIBits.ERROR_CODE_TYPE_NONE
      );
    }
    let action = gBits.ERROR_ACTION_COMPLETE;
    return requestPromise(action, callback => {
      this._request.complete(callback);
    }).then(() => this.shutdown());
  }

  /**
   * This function wraps nsIBitsRequest::suspendAsync.
   *
   * Instead of taking a callback, the function is asynchronous.
   * This method either resolves with no data, or rejects with a BitsError.
   */
  async suspendAsync() {
    if (!this._request) {
      throw new BitsError(
        Ci.nsIBits.ERROR_TYPE_USE_AFTER_REQUEST_SHUTDOWN,
        Ci.nsIBits.ERROR_ACTION_SUSPEND,
        Ci.nsIBits.ERROR_STAGE_PRETASK,
        Ci.nsIBits.ERROR_CODE_TYPE_NONE
      );
    }
    let action = gBits.ERROR_ACTION_SUSPEND;
    return requestPromise(action, callback => {
      this._request.suspendAsync(callback);
    });
  }

  /**
   * This function wraps nsIBitsRequest::resumeAsync.
   *
   * Instead of taking a callback, the function is asynchronous.
   * This method either resolves with no data, or rejects with a BitsError.
   */
  async resumeAsync() {
    if (!this._request) {
      throw new BitsError(
        Ci.nsIBits.ERROR_TYPE_USE_AFTER_REQUEST_SHUTDOWN,
        Ci.nsIBits.ERROR_ACTION_RESUME,
        Ci.nsIBits.ERROR_STAGE_PRETASK,
        Ci.nsIBits.ERROR_CODE_TYPE_NONE
      );
    }
    let action = gBits.ERROR_ACTION_RESUME;
    return requestPromise(action, callback => {
      this._request.resumeAsync(callback);
    });
  }
}
BitsRequest.prototype.QueryInterface = ChromeUtils.generateQI([Ci.nsIRequest]);

/**
 * This function does all of the wrapping and error handling for an async
 * Bits Service method. This allows the implementations for those methods to
 * simply call this function with a closure that executes appropriate
 * nsIBits method.
 *
 * Specifically, this function takes an nsBitsErrorAction, an observer and a
 * function.
 * The nsBitsErrorAction will be used when constructing a BitsError, if the
 * wrapper encounters an error.
 * The observer should be the one that the caller passed to the Bits Interface
 * method. It will be wrapped so that its methods are passed a BitsRequest
 * rather than an nsIBitsRequest.
 * The function will be passed the callback function and the wrapped observer,
 * both of which should be passed to the nsIBitsRequest method.
 */
async function servicePromise(errorAction, observer, actionFn) {
  return new Promise((resolve, reject) => {
    if (!observer) {
      let error = new BitsError(
        gBits.ERROR_TYPE_NULL_ARGUMENT,
        errorAction,
        gBits.ERROR_STAGE_PRETASK,
        gBits.ERROR_CODE_TYPE_NONE
      );
      reject(error);
      return;
    }
    try {
      observer.QueryInterface(Ci.nsIRequestObserver);
    } catch (e) {
      let error = new BitsError(
        gBits.ERROR_TYPE_INVALID_ARGUMENT,
        errorAction,
        gBits.ERROR_STAGE_PRETASK,
        gBits.ERROR_CODE_TYPE_EXCEPTION,
        e
      );
      reject(error);
      return;
    }
    let isProgressEventSink = false;
    try {
      observer.QueryInterface(Ci.nsIProgressEventSink);
      isProgressEventSink = true;
    } catch (e) {}

    // This will be set to the BitsRequest (wrapping the nsIBitsRequest), once
    // it is available. This prevents a new wrapper from having to be made every
    // time an observer function is called.
    let wrappedRequest;

    let wrappedObserver = {
      onStartRequest: function wrappedObserver_onStartRequest(request) {
        if (!wrappedRequest) {
          wrappedRequest = new BitsRequest(request);
        }
        observer.onStartRequest(wrappedRequest);
      },
      onStopRequest: function wrappedObserver_onStopRequest(request, status) {
        if (!wrappedRequest) {
          wrappedRequest = new BitsRequest(request);
        }
        observer.onStopRequest(wrappedRequest, status);
      },
      onProgress: function wrappedObserver_onProgress(
        request,
        context,
        progress,
        progressMax
      ) {
        if (isProgressEventSink) {
          if (!wrappedRequest) {
            wrappedRequest = new BitsRequest(request);
          }
          observer.onProgress(wrappedRequest, context, progress, progressMax);
        }
      },
      onStatus: function wrappedObserver_onStatus(
        request,
        context,
        status,
        statusArg
      ) {
        if (isProgressEventSink) {
          if (!wrappedRequest) {
            wrappedRequest = new BitsRequest(request);
          }
          observer.onStatus(wrappedRequest, context, status, statusArg);
        }
      },
      QueryInterface: ChromeUtils.generateQI([
        Ci.nsIRequestObserver,
        Ci.nsIProgressEventSink,
      ]),
    };

    let timer = makeTimeout(reject, errorAction);
    let callback = {
      QueryInterface: ChromeUtils.generateQI([Ci.nsIBitsNewRequestCallback]),
      success(request) {
        timer.cancel();
        if (!wrappedRequest) {
          wrappedRequest = new BitsRequest(request);
        }
        resolve(wrappedRequest);
      },
      failure(type, action, stage) {
        timer.cancel();
        let error = new BitsError(
          type,
          action,
          stage,
          gBits.ERROR_CODE_TYPE_NONE
        );
        reject(error);
      },
      failureNsresult(type, action, stage, code) {
        timer.cancel();
        let error = new BitsError(
          type,
          action,
          stage,
          gBits.ERROR_CODE_TYPE_NSRESULT,
          code
        );
        reject(error);
      },
      failureHresult(type, action, stage, code) {
        timer.cancel();
        let error = new BitsError(
          type,
          action,
          stage,
          gBits.ERROR_CODE_TYPE_HRESULT,
          code
        );
        reject(error);
      },
      failureString(type, action, stage, message) {
        timer.cancel();
        let error = new BitsError(
          type,
          action,
          stage,
          gBits.ERROR_CODE_TYPE_STRING,
          message
        );
        reject(error);
      },
    };

    try {
      actionFn(wrappedObserver, callback);
    } catch (e) {
      let error = new BitsError(
        gBits.ERROR_TYPE_METHOD_THREW,
        errorAction,
        gBits.ERROR_STAGE_PRETASK,
        gBits.ERROR_CODE_TYPE_EXCEPTION,
        e
      );
      reject(error);
    }
  });
}

var Bits = {
  /**
   * This function wraps nsIBits::initialized.
   */
  get initialized() {
    return gBits.initialized;
  },

  /**
   * This function wraps nsIBits::init.
   */
  init(jobName, savePathPrefix, monitorTimeoutMs) {
    return gBits.init(jobName, savePathPrefix, monitorTimeoutMs);
  },

  /**
   * This function wraps nsIBits::startDownload.
   *
   * Instead of taking a callback, the function is asynchronous.
   * This method either resolves with a BitsRequest (which is also an
   * nsIRequest), or rejects with a BitsError.
   */
  async startDownload(
    downloadURL,
    saveRelPath,
    proxy,
    noProgressTimeoutSecs,
    monitorIntervalMs,
    observer,
    context
  ) {
    let action = gBits.ERROR_ACTION_START_DOWNLOAD;
    return servicePromise(action, observer, (wrappedObserver, callback) => {
      gBits.startDownload(
        downloadURL,
        saveRelPath,
        proxy,
        noProgressTimeoutSecs,
        monitorIntervalMs,
        wrappedObserver,
        context,
        callback
      );
    });
  },

  /**
   * This function wraps nsIBits::monitorDownload.
   *
   * Instead of taking a callback, the function is asynchronous.
   * This method either resolves with a BitsRequest (which is also an
   * nsIRequest), or rejects with a BitsError.
   */
  async monitorDownload(id, monitorIntervalMs, observer, context) {
    let action = gBits.ERROR_ACTION_MONITOR_DOWNLOAD;
    return servicePromise(action, observer, (wrappedObserver, callback) => {
      gBits.monitorDownload(
        id,
        monitorIntervalMs,
        wrappedObserver,
        context,
        callback
      );
    });
  },
};

const EXPORTED_SYMBOLS = [
  "Bits",
  "BitsError",
  "BitsRequest",
  "BitsSuccess",
  "BitsUnknownError",
  "BitsVerificationError",
];
