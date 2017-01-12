/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Services",
  "resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS",
  "resource://gre/modules/osfile.jsm");

const FRAME_SCRIPT = "chrome://talos-powers/content/talos-powers-content.js";

function TalosPowersService() {
  this.wrappedJSObject = this;
};

TalosPowersService.prototype = {
  classDescription: "Talos Powers",
  classID: Components.ID("{f5d53443-d58d-4a2f-8df0-98525d4f91ad}"),
  contractID: "@mozilla.org/talos/talos-powers-service;1",
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),

  observe(subject, topic, data) {
    switch(topic) {
      case "profile-after-change":
        // Note that this observation is registered in the chrome.manifest
        // for this add-on.
        this.init();
        break;
      case "xpcom-shutdown":
        this.uninit();
        break;
    }
  },

  init() {
    Services.mm.loadFrameScript(FRAME_SCRIPT, true);
    Services.mm.addMessageListener("Talos:ForceQuit", this);
    Services.mm.addMessageListener("TalosContentProfiler:Command", this);
    Services.mm.addMessageListener("TalosPowersContent:ForceCCAndGC", this);
    Services.mm.addMessageListener("TalosPowersContent:GetStartupInfo", this);
    Services.mm.addMessageListener("TalosPowers:ParentExec:QueryMsg", this);
    Services.obs.addObserver(this, "xpcom-shutdown", false);
  },

  uninit() {
    Services.obs.removeObserver(this, "xpcom-shutdown");
  },

  receiveMessage(message) {
    switch(message.name) {
      case "Talos:ForceQuit": {
        this.forceQuit(message.data);
        break;
      }
      case "TalosContentProfiler:Command": {
        this.receiveProfileCommand(message);
        break;
      }
      case "TalosPowersContent:ForceCCAndGC": {
        Cu.forceGC();
        Cu.forceCC();
        Cu.forceShrinkingGC();
        break;
      }
      case "TalosPowersContent:GetStartupInfo": {
        this.receiveGetStartupInfo(message);
        break;
      }
      case "TalosPowers:ParentExec:QueryMsg": {
        this.RecieveParentExecCommand(message);
        break;
      }
    }
  },

  /**
   * Enable the SPS profiler with some settings and then pause
   * immediately.
   *
   * @param data (object)
   *        A JavaScript object with the following properties:
   *
   *        entries (int):
   *          The sampling buffer size in bytes.
   *
   *        interval (int):
   *          The sampling interval in milliseconds.
   *
   *        threadsArray (array of strings):
   *          The thread names to sample.
   */
  profilerBegin(data) {
    Services.profiler
            .StartProfiler(data.entries, data.interval,
                           ["js", "leaf", "stackwalk", "threads"], 4,
                           data.threadsArray, data.threadsArray.length);

    Services.profiler.PauseSampling();
  },

  /**
   * Assuming the Profiler is running, dumps the Profile from all sampled
   * processes and threads to the disk. The Profiler will be stopped once
   * the profiles have been dumped. This method returns a Promise that
   * will resolve once this has occurred.
   *
   * @param profileFile (string)
   *        The name of the file to write to.
   *
   * @returns Promise
   */
  profilerFinish(profileFile) {
    return new Promise((resolve, reject) => {
      Services.profiler.getProfileDataAsync().then((profile) => {
        let encoder = new TextEncoder();
        let array = encoder.encode(JSON.stringify(profile));

        OS.File.writeAtomic(profileFile, array, {
          tmpPath: profileFile + ".tmp",
        }).then(() => {
          Services.profiler.StopProfiler();
          resolve();
        });
      }, (error) => {
        Cu.reportError("Failed to gather profile: " + error);
        // FIXME: We should probably send a message down to the
        // child which causes it to reject the waiting Promise.
        reject();
      });
    });
  },

  /**
   * Pauses the Profiler, optionally setting a parent process marker before
   * doing so.
   *
   * @param marker (string, optional)
   *        A marker to set before pausing.
   */
  profilerPause(marker=null) {
    if (marker) {
      Services.profiler.AddMarker(marker);
    }

    Services.profiler.PauseSampling();
  },

  /**
   * Resumes a pausedProfiler, optionally setting a parent process marker
   * after doing so.
   *
   * @param marker (string, optional)
   *        A marker to set after resuming.
   */
  profilerResume(marker=null) {
    Services.profiler.ResumeSampling();

    if (marker) {
      Services.profiler.AddMarker(marker);
    }
  },

  /**
   * Adds a marker to the Profile in the parent process.
   */
  profilerMarker(marker) {
    Services.profiler.AddMarker(marker);
  },

  receiveProfileCommand(message) {
    const ACK_NAME = "TalosContentProfiler:Response";
    let mm = message.target.messageManager;
    let name = message.data.name;
    let data = message.data.data;

    switch(name) {
      case "Profiler:Begin": {
        this.profilerBegin(data);
        // profilerBegin will cause the parent to send an async message to any
        // child processes to start profiling. Because messages are serviced
        // in order, we know that by the time that the child services the
        // ACK message, that the profiler has started in its process.
        mm.sendAsyncMessage(ACK_NAME, { name });
        break;
      }

      case "Profiler:Finish": {
        // The test is done. Dump the profile.
        let profileFile = data.profileFile;
        this.profilerFinish(data.profileFile).then(() => {
          mm.sendAsyncMessage(ACK_NAME, { name });
        });
        break;
      }

      case "Profiler:Pause": {
        this.profilerPause(data.marker);
        mm.sendAsyncMessage(ACK_NAME, { name });
        break;
      }

      case "Profiler:Resume": {
        this.profilerResume(data.marker);
        mm.sendAsyncMessage(ACK_NAME, { name });
        break;
      }

      case "Profiler:Marker": {
        this.profilerMarker(data.marker);
        mm.sendAsyncMessage(ACK_NAME, { name });
      }
    }
  },

  forceQuit(messageData) {
    if (messageData && messageData.waitForSafeBrowsing) {
      let SafeBrowsing = Cu.import("resource://gre/modules/SafeBrowsing.jsm", {}).SafeBrowsing;

      let whenDone = () => {
        this.forceQuit();
      };
      SafeBrowsing.addMozEntriesFinishedPromise.then(whenDone, whenDone);
      // Speed things up in case nobody else called this:
      SafeBrowsing.init();
      return;
    }

    let enumerator = Services.wm.getEnumerator(null);
    while (enumerator.hasMoreElements()) {
      let domWindow = enumerator.getNext();
      domWindow.close();
    }

    try {
      Services.startup.quit(Services.startup.eForceQuit);
    } catch(e) {
      dump('Force Quit failed: ' + e);
    }
  },

  receiveGetStartupInfo(message) {
    let mm = message.target.messageManager;
    let startupInfo = Services.startup.getStartupInfo();

    if (!startupInfo["firstPaint"]) {
      // It's possible that we were called early enough that
      // the firstPaint measurement hasn't been set yet. In
      // that case, we set up an observer for the next time
      // a window is painted and re-retrieve the startup info.
      let obs = function(subject, topic) {
        Services.obs.removeObserver(this, topic);
        startupInfo = Services.startup.getStartupInfo();
        mm.sendAsyncMessage("TalosPowersContent:GetStartupInfo:Result",
                            startupInfo);
      };
      Services.obs.addObserver(obs, "widget-first-paint", false);
    } else {
      mm.sendAsyncMessage("TalosPowersContent:GetStartupInfo:Result",
                          startupInfo);
    }
  },

  // These services are exposed to local unprivileged content.
  // Each service is a function which accepts an argument, a callback for sending
  // the reply (possibly async), and the parent window as a utility.
  // arg/reply semantice are service-specific.
  // To add a service: add a method at ParentExecServices here, then at the content:
  // <script src="chrome://talos-powers-content/content/TalosPowersContent.js"></script>
  // and then e.g. TalosPowersParent.exec("sampleParentService", myArg, myCallback)
  // Sample service:
  /*
    // arg: anything. return: sample reply
    sampleParentService: function(arg, callback, win) {
      win.setTimeout(function() {
        callback("sample reply for: " + arg);
      }, 500);
    },
  */
  ParentExecServices: {

    // arg: ignored. return: handle (number) for use with stopFrameTimeRecording
    startFrameTimeRecording: function(arg, callback, win) {
      var rv = win.QueryInterface(Ci.nsIInterfaceRequestor)
                  .getInterface(Ci.nsIDOMWindowUtils)
                  .startFrameTimeRecording();
      callback(rv);
    },

    // arg: handle from startFrameTimeRecording. return: array with composition intervals
    stopFrameTimeRecording: function(arg, callback, win) {
      var rv = win.QueryInterface(Ci.nsIInterfaceRequestor)
                  .getInterface(Ci.nsIDOMWindowUtils)
                  .stopFrameTimeRecording(arg);
      callback(rv);
    },
  },

  RecieveParentExecCommand(msg) {
    function sendResult(result) {
      let mm = msg.target.messageManager;
      mm.sendAsyncMessage("TalosPowers:ParentExec:ReplyMsg", {
        id: msg.data.id,
        result: result
      });
    }

    let command = msg.data.command;
    if (!this.ParentExecServices.hasOwnProperty(command.name))
      throw new Error("TalosPowers:ParentExec: Invalid service '" + command.name + "'");

    this.ParentExecServices[command.name](command.data, sendResult, msg.target.ownerGlobal);
  },

};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([TalosPowersService]);
