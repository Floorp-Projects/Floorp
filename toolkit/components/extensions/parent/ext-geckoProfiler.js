/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(this, "OS", "resource://gre/modules/osfile.jsm");
ChromeUtils.defineModuleGetter(
  this,
  "ProfilerGetSymbols",
  "resource://gre/modules/ProfilerGetSymbols.jsm"
);

const PREF_ASYNC_STACK = "javascript.options.asyncstack";

const ASYNC_STACKS_ENABLED = Services.prefs.getBoolPref(
  PREF_ASYNC_STACK,
  false
);

var { ExtensionError } = ExtensionUtils;

const symbolCache = new Map();

const primeSymbolStore = libs => {
  for (const { path, debugName, debugPath, breakpadId } of libs) {
    symbolCache.set(`${debugName}/${breakpadId}`, { path, debugPath });
  }
};

const isRunningObserver = {
  _observers: new Set(),

  observe(subject, topic, data) {
    switch (topic) {
      case "profiler-started":
      case "profiler-stopped":
        // Call observer(false) or observer(true), but do it through a promise
        // so that it's asynchronous.
        // We don't want it to be synchronous because of the observer call in
        // addObserver, which is asynchronous, and we want to get the ordering
        // right.
        const isRunningPromise = Promise.resolve(topic === "profiler-started");
        for (let observer of this._observers) {
          isRunningPromise.then(observer);
        }
        break;
    }
  },

  _startListening() {
    Services.obs.addObserver(this, "profiler-started");
    Services.obs.addObserver(this, "profiler-stopped");
  },

  _stopListening() {
    Services.obs.removeObserver(this, "profiler-started");
    Services.obs.removeObserver(this, "profiler-stopped");
  },

  addObserver(observer) {
    if (this._observers.size === 0) {
      this._startListening();
    }

    this._observers.add(observer);
    observer(Services.profiler.IsActive());
  },

  removeObserver(observer) {
    if (this._observers.delete(observer) && this._observers.size === 0) {
      this._stopListening();
    }
  },
};

this.geckoProfiler = class extends ExtensionAPI {
  getAPI(context) {
    return {
      geckoProfiler: {
        async start(options) {
          const {
            bufferSize,
            windowLength,
            interval,
            features,
            threads,
          } = options;

          Services.prefs.setBoolPref(PREF_ASYNC_STACK, false);
          if (threads) {
            Services.profiler.StartProfiler(
              bufferSize,
              interval,
              features,
              threads,
              0,
              windowLength
            );
          } else {
            Services.profiler.StartProfiler(
              bufferSize,
              interval,
              features,
              [],
              0,
              windowLength
            );
          }
        },

        async stop() {
          if (ASYNC_STACKS_ENABLED !== null) {
            Services.prefs.setBoolPref(PREF_ASYNC_STACK, ASYNC_STACKS_ENABLED);
          }

          Services.profiler.StopProfiler();
        },

        async pause() {
          Services.profiler.Pause();
        },

        async resume() {
          Services.profiler.Resume();
        },

        async dumpProfileToFile(fileName) {
          if (!Services.profiler.IsActive()) {
            throw new ExtensionError(
              "The profiler is stopped. " +
                "You need to start the profiler before you can capture a profile."
            );
          }

          if (fileName.includes("\\") || fileName.includes("/")) {
            throw new ExtensionError("Path cannot contain a subdirectory.");
          }

          let fragments = [OS.Constants.Path.profileDir, "profiler", fileName];
          let filePath = OS.Path.join(...fragments);

          try {
            await Services.profiler.dumpProfileToFileAsync(filePath);
          } catch (e) {
            Cu.reportError(e);
            throw new ExtensionError(`Dumping profile to ${filePath} failed.`);
          }
        },

        async getProfile() {
          if (!Services.profiler.IsActive()) {
            throw new ExtensionError(
              "The profiler is stopped. " +
                "You need to start the profiler before you can capture a profile."
            );
          }

          return Services.profiler.getProfileDataAsync();
        },

        async getProfileAsArrayBuffer() {
          if (!Services.profiler.IsActive()) {
            throw new ExtensionError(
              "The profiler is stopped. " +
                "You need to start the profiler before you can capture a profile."
            );
          }

          return Services.profiler.getProfileDataAsArrayBuffer();
        },

        async getProfileAsGzippedArrayBuffer() {
          if (!Services.profiler.IsActive()) {
            throw new ExtensionError(
              "The profiler is stopped. " +
                "You need to start the profiler before you can capture a profile."
            );
          }

          return Services.profiler.getProfileDataAsGzippedArrayBuffer();
        },

        async getSymbols(debugName, breakpadId) {
          if (symbolCache.size === 0) {
            primeSymbolStore(Services.profiler.sharedLibraries);
          }

          const cachedLibInfo = symbolCache.get(`${debugName}/${breakpadId}`);
          if (!cachedLibInfo) {
            throw new Error(
              `The library ${debugName} ${breakpadId} is not in the Services.profiler.sharedLibraries list, ` +
                "so the local path for it is not known and symbols for it can not be obtained. " +
                "This usually happens if a content process uses a library that's not used in the parent " +
                "process - Services.profiler.sharedLibraries only knows about libraries in the parent process."
            );
          }

          const { path, debugPath } = cachedLibInfo;
          if (!OS.Path.split(path).absolute) {
            throw new Error(
              `Services.profiler.sharedLibraries did not contain an absolute path for the library ${debugName} ${breakpadId}, ` +
                "so symbols for this library can not be obtained."
            );
          }

          return ProfilerGetSymbols.getSymbolTable(path, debugPath, breakpadId);
        },

        onRunning: new EventManager({
          context,
          name: "geckoProfiler.onRunning",
          register: fire => {
            isRunningObserver.addObserver(fire.async);
            return () => {
              isRunningObserver.removeObserver(fire.async);
            };
          },
        }).api(),
      },
    };
  }
};
