/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cu } = require("chrome");
const protocol = require("devtools/server/protocol");
const { custom, method, RetVal, Arg, Option, types, preEvent } = protocol;
const { actorBridge } = require("devtools/server/actors/common");

loader.lazyRequireGetter(this, "events", "sdk/event/core");
loader.lazyRequireGetter(this, "merge", "sdk/util/object", true);
loader.lazyRequireGetter(this, "PerformanceIO",
  "devtools/toolkit/performance/io");
loader.lazyRequireGetter(this, "RecordingUtils",
  "devtools/toolkit/performance/utils");

/**
 * A set of functions used by both the front and actor to access
 * internal properties.
 */
const PerformanceRecordingCommon = {
  // Private fields, only needed when a recording is started or stopped.
  _console: false,
  _imported: false,
  _recording: false,
  _completed: false,
  _configuration: {},
  _startingBufferStatus: null,
  _localStartTime: null,

  // Serializable fields, necessary and sufficient for import and export.
  _label: "",
  _duration: 0,
  _markers: null,
  _frames: null,
  _memory: null,
  _ticks: null,
  _allocations: null,
  _profile: null,

  /**
   * Helper methods for returning the status of the recording.
   * These methods should be consistent on both the front and actor.
   */
  isRecording: function () { return this._recording; },
  isCompleted: function () { return this._completed || this.isImported(); },
  isFinalizing: function () { return !this.isRecording() && !this.isCompleted(); },
  isConsole: function () { return this._console; },
  isImported: function () { return this._imported; },

  /**
   * Helper methods for returning configuration for the recording.
   * These methods should be consistent on both the front and actor.
   */
  getConfiguration: function () { return this._configuration; },
  getLabel: function () { return this._label; },

  /**
   * Helper methods for returning recording data.
   * These methods should be consistent on both the front and actor.
   */
  getMarkers: function() { return this._markers; },
  getFrames: function() { return this._frames; },
  getMemory: function() { return this._memory; },
  getTicks: function() { return this._ticks; },
  getAllocations: function() { return this._allocations; },
  getProfile: function() { return this._profile; },

  getAllData: function () {
    let label = this.getLabel();
    let duration = this.getDuration();
    let markers = this.getMarkers();
    let frames = this.getFrames();
    let memory = this.getMemory();
    let ticks = this.getTicks();
    let allocations = this.getAllocations();
    let profile = this.getProfile();
    let configuration = this.getConfiguration();
    return { label, duration, markers, frames, memory, ticks, allocations, profile, configuration };
  },
};

/**
 * This actor wraps the Performance module at toolkit/devtools/shared/performance.js
 * and provides RDP definitions.
 *
 * @see toolkit/devtools/shared/performance.js for documentation.
 */
let PerformanceRecordingActor = exports.PerformanceRecordingActor = protocol.ActorClass(merge({
  typeName: "performance-recording",

  form: function(detail) {
    if (detail === "actorid") {
      return this.actorID;
    }

    let form = {
      actor: this.actorID,  // actorID is set when this is added to a pool
      configuration: this._configuration,
      startingBufferStatus: this._startingBufferStatus,
      console: this._console,
      label: this._label,
      startTime: this._startTime,
      localStartTime: this._localStartTime,
      recording: this._recording,
      completed: this._completed,
      duration: this._duration,
    };

    // Only send profiler data once it exists and it has
    // not yet been sent
    if (this._profile && !this._sentProfilerData) {
      form.profile = this._profile;
      this._sentProfilerData = true;
    }

    return form;
  },

  /**
   * @param {object} conn
   * @param {object} options
   *        A hash of features that this recording is utilizing.
   * @param {object} meta
   *        A hash of temporary metadata for a recording that is recording
   *        (as opposed to an imported recording).
   */
  initialize: function (conn, options, meta) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this._configuration = {
      withMarkers: options.withMarkers || false,
      withTicks: options.withTicks || false,
      withMemory: options.withMemory || false,
      withAllocations: options.withAllocations || false,
      withJITOptimizations: options.withJITOptimizations || false,
      allocationsSampleProbability: options.allocationsSampleProbability || 0,
      allocationsMaxLogLength: options.allocationsMaxLogLength || 0,
      bufferSize: options.bufferSize || 0,
      sampleFrequency: options.sampleFrequency || 1
    };

    this._console = !!options.console;
    this._label = options.label || "";

    if (meta) {
      // Store the start time roughly with Date.now() so when we
      // are checking the duration during a recording, we can get close
      // to the approximate duration to render elements without
      // making a real request
      this._localStartTime = Date.now();

      this._startTime = meta.startTime;
      this._startingBufferStatus = {
        position: meta.position,
        totalSize: meta.totalSize,
        generation: meta.generation
      };

      this._recording = true;
      this._markers = [];
      this._frames = [];
      this._memory = [];
      this._ticks = [];
      this._allocations = { sites: [], timestamps: [], frames: [], counts: [] };
    }
  },

  destroy: function() {
    protocol.Actor.prototype.destroy.call(this);
  },

  /**
   * Internal utility called by the PerformanceActor and PerformanceFront on state changes
   * to update the internal state of the PerformanceRecording.
   *
   * @param {string} state
   * @param {object} extraData
   */
  _setState: function (state, extraData) {
    switch (state) {
      case "recording-started": {
        this._recording = true;
        break;
      }
      case "recording-stopping": {
        this._recording = false;
        break;
      }
      case "recording-stopped": {
        this._profile = extraData.profile;
        this._duration = extraData.duration;

        // We filter out all samples that fall out of current profile's range
        // since the profiler is continuously running. Because of this, sample
        // times are not guaranteed to have a zero epoch, so offset the
        // timestamps.
        RecordingUtils.offsetSampleTimes(this._profile, this._startTime);

        // Markers need to be sorted ascending by time, to be properly displayed
        // in a waterfall view.
        this._markers = this._markers.sort((a, b) => (a.start > b.start));

        this._completed = true;
        break;
      }
    };
  },

}, PerformanceRecordingCommon));

/**
 * This can be used on older Profiler implementations, but the methods cannot
 * be changed -- you must introduce a new method, and detect the server.
 */
let PerformanceRecordingFront = exports.PerformanceRecordingFront = protocol.FrontClass(PerformanceRecordingActor, merge({

  form: function(form, detail) {
    if (detail === "actorid") {
      this.actorID = form;
      return;
    }
    this.actorID = form.actor;
    this._form = form;
    this._configuration = form.configuration;
    this._startingBufferStatus = form.startingBufferStatus;
    this._console = form.console;
    this._label = form.label;
    this._startTime = form.startTime;
    this._localStartTime = form.localStartTime;
    this._recording = form.recording;
    this._completed = form.completed;
    this._duration = form.duration;

    if (form.profile) {
      this._profile = form.profile;
    }

    // Sort again on the client side if we're using realtime markers and the recording
    // just finished. This is because GC/Compositing markers can come into the array out of order with
    // the other markers, leading to strange collapsing in waterfall view.
    if (this._completed && !this._markersSorted) {
      this._markers = this._markers.sort((a, b) => (a.start > b.start));
      this._markersSorted = true;
    }
  },

  initialize: function (client, form, config) {
    protocol.Front.prototype.initialize.call(this, client, form);
    this._markers = [];
    this._frames = [];
    this._memory = [];
    this._ticks = [];
    this._allocations = { sites: [], timestamps: [], frames: [], counts: [] };
  },

  destroy: function () {
    protocol.Front.prototype.destroy.call(this);
  },

  /**
   * Saves the current recording to a file.
   *
   * @param nsILocalFile file
   *        The file to stream the data into.
   */
  exportRecording: function (file) {
    let recordingData = this.getAllData();
    return PerformanceIO.saveRecordingToFile(recordingData, file);
  },

  /**
   * Returns the position, generation, and totalSize of the profiler
   * when this recording was started.
   *
   * @return {object}
   */
  getStartingBufferStatus: function () {
    return this._form.startingBufferStatus;
  },

  /**
   * Gets duration of this recording, in milliseconds.
   * @return number
   */
  getDuration: function () {
    // Compute an approximate ending time for the current recording if it is
    // still in progress. This is needed to ensure that the view updates even
    // when new data is not being generated. If recording is completed, use
    // the duration from the profiler; if between recording and being finalized,
    // use the last estimated duration.
    if (this.isRecording()) {
      return this._estimatedDuration = Date.now() - this._localStartTime;
    } else {
      return this._duration || this._estimatedDuration || 0;
    }
  },

  /**
   * Fired whenever the PerformanceFront emits markers, memory or ticks.
   */
  _addTimelineData: function (eventName, data) {
    let config = this.getConfiguration();

    switch (eventName) {
      // Accumulate timeline markers into an array. Furthermore, the timestamps
      // do not have a zero epoch, so offset all of them by the start time.
      case "markers": {
        if (!config.withMarkers) { break; }
        let { markers } = data;
        RecordingUtils.offsetMarkerTimes(markers, this._startTime);
        RecordingUtils.pushAll(this._markers, markers);
        break;
      }
      // Accumulate stack frames into an array.
      case "frames": {
        if (!config.withMarkers) { break; }
        let { frames } = data;
        RecordingUtils.pushAll(this._frames, frames);
        break;
      }
      // Accumulate memory measurements into an array. Furthermore, the timestamp
      // does not have a zero epoch, so offset it by the actor's start time.
      case "memory": {
        if (!config.withMemory) { break; }
        let { delta, measurement } = data;
        this._memory.push({
          delta: delta - this._startTime,
          value: measurement.total / 1024 / 1024
        });
        break;
      }
      // Save the accumulated refresh driver ticks.
      case "ticks": {
        if (!config.withTicks) { break; }
        let { timestamps } = data;
        this._ticks = timestamps;
        break;
      }
      // Accumulate allocation sites into an array.
      case "allocations": {
        if (!config.withAllocations) { break; }
        let {
          allocations: sites,
          allocationsTimestamps: timestamps,
          frames,
        } = data;

        RecordingUtils.offsetAndScaleTimestamps(timestamps, this._startTime);
        RecordingUtils.pushAll(this._allocations.sites, sites);
        RecordingUtils.pushAll(this._allocations.timestamps, timestamps);
        RecordingUtils.pushAll(this._allocations.frames, frames);
        break;
      }
    }
  },

  toString: () => "[object PerformanceRecordingFront]"
}, PerformanceRecordingCommon));
