/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This implements logic for stopping requests if the server starts to return
// too many errors.  If we get MAX_ERRORS errors in ERROR_PERIOD minutes, we
// back off for TIMEOUT_INCREMENT minutes.  If we get another error
// immediately after we restart, we double the timeout and add
// TIMEOUT_INCREMENT minutes, etc.
// 
// This is similar to the logic used by the search suggestion service.

// HTTP responses that count as an error.  We also include any 5xx response
// as an error.
const HTTP_FOUND                 = 302;
const HTTP_SEE_OTHER             = 303;
const HTTP_TEMPORARY_REDIRECT    = 307;

/**
 * @param maxErrors Number of times to request before backing off.
 * @param retryIncrement Time (ms) for each retry before backing off.
 * @param maxRequests Number the number of requests needed to trigger backoff
 * @param requestPeriod Number time (ms) in which maxRequests have to occur to
 *     trigger the backoff behavior (0 to disable maxRequests)
 * @param timeoutIncrement Number time (ms) the starting timeout period
 *     we double this time for consecutive errors
 * @param maxTimeout Number time (ms) maximum timeout period
 */
function RequestBackoff(maxErrors, retryIncrement,
                        maxRequests, requestPeriod,
                        timeoutIncrement, maxTimeout) {
  this.MAX_ERRORS_ = maxErrors;
  this.RETRY_INCREMENT_ = retryIncrement;
  this.MAX_REQUESTS_ = maxRequests;
  this.REQUEST_PERIOD_ = requestPeriod;
  this.TIMEOUT_INCREMENT_ = timeoutIncrement;
  this.MAX_TIMEOUT_ = maxTimeout;

  // Queue of ints keeping the time of all requests
  this.requestTimes_ = [];

  this.numErrors_ = 0;
  this.errorTimeout_ = 0;
  this.nextRequestTime_ = 0;
}

/**
 * Reset the object for reuse. This deliberately doesn't clear requestTimes_.
 */
RequestBackoff.prototype.reset = function() {
  this.numErrors_ = 0;
  this.errorTimeout_ = 0;
  this.nextRequestTime_ = 0;
}

/**
 * Check to see if we can make a request.
 */
RequestBackoff.prototype.canMakeRequest = function() {
  var now = Date.now();
  if (now < this.nextRequestTime_) {
    return false;
  }

  return (this.requestTimes_.length < this.MAX_REQUESTS_ ||
          (now - this.requestTimes_[0]) > this.REQUEST_PERIOD_);
}

RequestBackoff.prototype.noteRequest = function() {
  var now = Date.now();
  this.requestTimes_.push(now);

  // We only care about keeping track of MAX_REQUESTS
  if (this.requestTimes_.length > this.MAX_REQUESTS_)
    this.requestTimes_.shift();
}

RequestBackoff.prototype.nextRequestDelay = function() {
  return Math.max(0, this.nextRequestTime_ - Date.now());
}

/**
 * Notify this object of the last server response.  If it's an error,
 */
RequestBackoff.prototype.noteServerResponse = function(status) {
  if (this.isErrorStatus(status)) {
    this.numErrors_++;

    if (this.numErrors_ < this.MAX_ERRORS_)
      this.errorTimeout_ = this.RETRY_INCREMENT_;
    else if (this.numErrors_ == this.MAX_ERRORS_)
      this.errorTimeout_ = this.TIMEOUT_INCREMENT_;
    else
      this.errorTimeout_ *= 2;

    this.errorTimeout_ = Math.min(this.errorTimeout_, this.MAX_TIMEOUT_);
    this.nextRequestTime_ = Date.now() + this.errorTimeout_;
  } else {
    // Reset error timeout, allow requests to go through.
    this.reset();
  }
}

/**
 * We consider 302, 303, 307, 4xx, and 5xx http responses to be errors.
 * @param status Number http status
 * @return Boolean true if we consider this http status an error
 */
RequestBackoff.prototype.isErrorStatus = function(status) {
  return ((400 <= status && status <= 599) ||
          HTTP_FOUND == status ||
          HTTP_SEE_OTHER == status ||
          HTTP_TEMPORARY_REDIRECT == status);
}

