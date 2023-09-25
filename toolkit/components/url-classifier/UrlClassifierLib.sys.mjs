/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// We wastefully reload the same JS files across components.  This puts all
// the common JS files used by safebrowsing and url-classifier into a
// single component.

const PREF_DISABLE_TEST_BACKOFF =
  "browser.safebrowsing.provider.test.disableBackoff";

/**
 * Partially applies a function to a particular "this object" and zero or
 * more arguments. The result is a new function with some arguments of the first
 * function pre-filled and the value of |this| "pre-specified".
 *
 * Remaining arguments specified at call-time are appended to the pre-
 * specified ones.
 *
 * Usage:
 * var barMethBound = BindToObject(myFunction, myObj, "arg1", "arg2");
 * barMethBound("arg3", "arg4");
 *
 * @param fn {string} Reference to the function to be bound
 *
 * @param self {object} Specifies the object which |this| should point to
 * when the function is run. If the value is null or undefined, it will default
 * to the global object.
 *
 * @returns {function} A partially-applied form of the speficied function.
 */
export function BindToObject(fn, self, opt_args) {
  var boundargs = fn.boundArgs_ || [];
  boundargs = boundargs.concat(
    Array.prototype.slice.call(arguments, 2, arguments.length)
  );

  if (fn.boundSelf_) {
    self = fn.boundSelf_;
  }
  if (fn.boundFn_) {
    fn = fn.boundFn_;
  }

  var newfn = function () {
    // Combine the static args and the new args into one big array
    var args = boundargs.concat(Array.prototype.slice.call(arguments));
    return fn.apply(self, args);
  };

  newfn.boundArgs_ = boundargs;
  newfn.boundSelf_ = self;
  newfn.boundFn_ = fn;

  return newfn;
}

// This implements logic for stopping requests if the server starts to return
// too many errors.  If we get MAX_ERRORS errors in ERROR_PERIOD minutes, we
// back off for TIMEOUT_INCREMENT minutes.  If we get another error
// immediately after we restart, we double the timeout and add
// TIMEOUT_INCREMENT minutes, etc.
//
// This is similar to the logic used by the search suggestion service.

// HTTP responses that count as an error.  We also include any 5xx response
// as an error.
const HTTP_FOUND = 302;
const HTTP_SEE_OTHER = 303;
const HTTP_TEMPORARY_REDIRECT = 307;

/**
 * @param maxErrors Number of times to request before backing off.
 * @param retryIncrement Time (ms) for each retry before backing off.
 * @param maxRequests Number the number of requests needed to trigger backoff
 * @param requestPeriod Number time (ms) in which maxRequests have to occur to
 *     trigger the backoff behavior (0 to disable maxRequests)
 * @param timeoutIncrement Number time (ms) the starting timeout period
 *     we double this time for consecutive errors
 * @param maxTimeout Number time (ms) maximum timeout period
 * @param tolerance Checking next request tolerance.
 */
function RequestBackoff(
  maxErrors,
  retryIncrement,
  maxRequests,
  requestPeriod,
  timeoutIncrement,
  maxTimeout,
  tolerance,
  provider = null
) {
  this.MAX_ERRORS_ = maxErrors;
  this.RETRY_INCREMENT_ = retryIncrement;
  this.MAX_REQUESTS_ = maxRequests;
  this.REQUEST_PERIOD_ = requestPeriod;
  this.TIMEOUT_INCREMENT_ = timeoutIncrement;
  this.MAX_TIMEOUT_ = maxTimeout;
  this.TOLERANCE_ = tolerance;

  // Queue of ints keeping the time of all requests
  this.requestTimes_ = [];

  this.numErrors_ = 0;
  this.errorTimeout_ = 0;
  this.nextRequestTime_ = 0;

  // For test provider, we will disable backoff if preference is set to false.
  if (provider === "test") {
    this.canMakeRequestDefault = this.canMakeRequest;
    this.canMakeRequest = function () {
      if (Services.prefs.getBoolPref(PREF_DISABLE_TEST_BACKOFF, true)) {
        return true;
      }
      return this.canMakeRequestDefault();
    };
  }
}

/**
 * Reset the object for reuse. This deliberately doesn't clear requestTimes_.
 */
RequestBackoff.prototype.reset = function () {
  this.numErrors_ = 0;
  this.errorTimeout_ = 0;
  this.nextRequestTime_ = 0;
};

/**
 * Check to see if we can make a request.
 */
RequestBackoff.prototype.canMakeRequest = function () {
  var now = Date.now();
  // Note that nsITimer delay is approximate: the timer can be fired before the
  // requested time has elapsed. So, give it a tolerance
  if (now + this.TOLERANCE_ < this.nextRequestTime_) {
    return false;
  }

  return (
    this.requestTimes_.length < this.MAX_REQUESTS_ ||
    now - this.requestTimes_[0] > this.REQUEST_PERIOD_
  );
};

RequestBackoff.prototype.noteRequest = function () {
  var now = Date.now();
  this.requestTimes_.push(now);

  // We only care about keeping track of MAX_REQUESTS
  if (this.requestTimes_.length > this.MAX_REQUESTS_) {
    this.requestTimes_.shift();
  }
};

RequestBackoff.prototype.nextRequestDelay = function () {
  return Math.max(0, this.nextRequestTime_ - Date.now());
};

/**
 * Notify this object of the last server response.  If it's an error,
 */
RequestBackoff.prototype.noteServerResponse = function (status) {
  if (this.isErrorStatus(status)) {
    this.numErrors_++;

    if (this.numErrors_ < this.MAX_ERRORS_) {
      this.errorTimeout_ = this.RETRY_INCREMENT_;
    } else if (this.numErrors_ == this.MAX_ERRORS_) {
      this.errorTimeout_ = this.TIMEOUT_INCREMENT_;
    } else {
      this.errorTimeout_ *= 2;
    }

    this.errorTimeout_ = Math.min(this.errorTimeout_, this.MAX_TIMEOUT_);
    this.nextRequestTime_ = Date.now() + this.errorTimeout_;
  } else {
    // Reset error timeout, allow requests to go through.
    this.reset();
  }
};

/**
 * We consider 302, 303, 307, 4xx, and 5xx http responses to be errors.
 * @param status Number http status
 * @return Boolean true if we consider this http status an error
 */
RequestBackoff.prototype.isErrorStatus = function (status) {
  return (
    (400 <= status && status <= 599) ||
    HTTP_FOUND == status ||
    HTTP_SEE_OTHER == status ||
    HTTP_TEMPORARY_REDIRECT == status
  );
};

// Wrap a general-purpose |RequestBackoff| to a v4-specific one
// since both listmanager and hashcompleter would use it.
// Note that |maxRequests| and |requestPeriod| is still configurable
// to throttle pending requests.
function RequestBackoffV4(maxRequests, requestPeriod, provider = null) {
  let rand = Math.random();
  let retryInterval = Math.floor(15 * 60 * 1000 * (rand + 1)); // 15 ~ 30 min.
  let backoffInterval = Math.floor(30 * 60 * 1000 * (rand + 1)); // 30 ~ 60 min.

  return new RequestBackoff(
    2 /* max errors */,
    retryInterval /* retry interval, 15~30 min */,
    maxRequests /* num requests */,
    requestPeriod /* request time, 60 min */,
    backoffInterval /* backoff interval, 60 min */,
    24 * 60 * 60 * 1000 /* max backoff, 24hr */,
    1000 /* tolerance of 1 sec */,
    provider /* provider name */
  );
}

export function UrlClassifierLib() {
  this.wrappedJSObject = {
    RequestBackoff,
    RequestBackoffV4,
    BindToObject,
  };
}

UrlClassifierLib.prototype.QueryInterface = ChromeUtils.generateQI([]);
