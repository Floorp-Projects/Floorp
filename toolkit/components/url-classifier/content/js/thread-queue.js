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
 *   Ojan Vafai <ojan@google.com> (original author)
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


/**
 * Simulates a thread in JavaScript. However, since there is no preemption in
 * it's not a proper thread. Specifically, any worker function that takes
 * longer than the allotted runtime will simply take up that extra time. It is
 * the responsibility of the user of this class to ensure that each iteration
 * of a worker function runs in less time than the necessary runtime.
 *
 * The delay and runtime (ms) settings control how much the thread affects the
 * user experience. Larger runtime will mean the computation happens faster,
 * but the user will notice more jumpiness. A larger delay can mitigate
 * jumpiness significantly and causes the computation to take longer.
 *
 * The interleave setting means that the thread interleaves the running of
 * the workers that have been added to it. If this value is false, each worker
 * runs to completion before the next worker starts running (a worker
 * that does not complete will cause all subsequent workers to never run).
 *
 * Dependencies:
 * - alarm.js (only when used inside a Firefox service)
 *
 * @param opt_config Configuration object with optional settings for runtime,
 *                   delay and noInterleave.
 */
function TH_ThreadQueue(opt_config) {
  // default values
  this.runtime_ = 100;
  this.delay_ = 25;
  this.interleave_ = true;

  if (opt_config) {
    this.interleave_ = (opt_config.interleave === true);
    if (opt_config.runtime) this.runtime_ = opt_config.runtime;
    if (opt_config.delay) this.delay_ = opt_config.delay;
  }

  this.workers_ = [];
  this.guid_ = 0;

  // Bind this here so that it doesn't need to be bound on every iteration.
  // We are using this clugy way of calling the method becuase of a Firefox
  // XUL bug <https://bugzilla.mozilla.org/show_bug.cgi?id=300079>. This used
  // to use BindToObject but that has been depreciated.
  this.iterate_ = Function.prototype.bind.call(this.runIteration_, this);
}

/**
 * Add a worker function to the thread. Adding more workers means that each
 * worker runs slower and the user will notice more jumpiness.
 *
 * @param worker Function that iteratively computes the task. Must not
 *               already be a worker in the thread.
 * @param opt_isComplete Function that identifies if this worker is complete.
 *                       If it is complete, then this worker is removed from
 *                       the thread. If omitted, this worker will never
 *                       complete.
 */
TH_ThreadQueue.prototype.addWorker = function(worker, opt_isComplete) {
  if (worker.__workerId) {
    throw new Error("Cannot add a worker to a thread while it is currently " +
                    "running in the thread.");
  }

  worker.__isComplete = opt_isComplete || function() { return false; };
  worker.__workerId = this.guid_++;
  this.workers_.push(worker);
  if (this.workers_.length == 1) {
    this.setCurrentWorkerByIndex_(0);
    // If thread is paused iterate_ is a noop.
    this.iterate_();
  }
}

/**
 * Remove a worker from this thread.
 *
 * @param worker The worker function to remove from the thread. This must be
 *               the same function object that was passed into add worker,
 *               not just the same function.
 */
TH_ThreadQueue.prototype.removeWorker = function(worker) {
  for (var i = 0; i < this.workers_.length; i++) {
    if (worker.__workerId == this.workers_[i].__workerId) {
      this.removeWorkerByIndex_(i);
    }
  }
}

/**
 * Remove a worker based on its index in the workers array.
 */
TH_ThreadQueue.prototype.removeWorkerByIndex_ = function(workerIndex) {
  var worker = this.workers_[workerIndex];
  delete(worker.__isComplete);
  delete(worker.__workerId);

  this.workers_.splice(workerIndex, 1);

  var newWorkerIndex = this.currentWorkerIndex_ % this.workers_.length;
  this.setCurrentWorkerByIndex_(newWorkerIndex);
}

/**
* Set the delay between iterations of the thread.
*
* @param delay Delay in milliseconds
*/
TH_ThreadQueue.prototype.setDelay = function(delay) {
  this.delay_ = delay;
}

/**
 * Set the runtime of each iteration of the thread. Note that this runtime
 * cannot be 100% assured and will definitely not be kept if any worker takes
 * longer than the runtime to run an iteration.
 *
 * @param runtime Runtime in milliseconds
 */
TH_ThreadQueue.prototype.setRuntime = function(runtime) {
  this.runtime_ = runtime;
}

/**
 * Set the thread running.
 */
TH_ThreadQueue.prototype.run = function() {
  this.running_ = true;
  this.iterate_();
}

/**
 * Pause the thread.
 */
TH_ThreadQueue.prototype.pause = function() {
  this.running_ = false;
}

/**
 * Run an iteration of the thread.
 * Does nothing if there are no workers, or the thread is paused.
 */
TH_ThreadQueue.prototype.runIteration_ = function() {
  if (!this.running_ || this.workers_.length == 0) {
    return;
  }

  var startTime = (new Date()).getTime();

  while ((new Date()).getTime() - startTime < this.runtime_) {
    if (this.currentWorker_.__isComplete()) {
      this.removeWorkerByIndex_(this.currentWorkerIndex_);
      break;
    }

    this.currentWorker_();
    if (this.interleave_) this.nextWorker_();
  }

  if (typeof top != "undefined" && top.setTimeout) {
    top.setTimeout(this.iterate_, this.delay_);
  } else if (G_Alarm) {
    new G_Alarm(this.iterate_, this.delay_);
  } else {
    throw new Error("Could not find a mechanism to start a timeout. Need " +
                    "window.setTimeout or G_Alarm.");
  }
}

/**
 * Set the current worker to be the worker the currentWorkerIndex_ points to.
 *
 * @param workerIndex Index of the worker into the workers_ array.
 */
TH_ThreadQueue.prototype.setCurrentWorkerByIndex_ = function(workerIndex) {
  this.currentWorkerIndex_ = workerIndex;
  this.currentWorker_ = this.workers_[workerIndex];
}

/**
* Iterate to the next worker function.
*/
TH_ThreadQueue.prototype.nextWorker_ = function() {
  var nextWorkerIndex = (this.currentWorkerIndex_ + 1) % this.workers_.length;
  this.setCurrentWorkerByIndex_(nextWorkerIndex);
}
