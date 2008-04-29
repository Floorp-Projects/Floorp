/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Google Safe Browsing.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Tony Chang <tc@google.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
 *     trigger the backoff behavior
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
 * Reset the object for reuse.
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

