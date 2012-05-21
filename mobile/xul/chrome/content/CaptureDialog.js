/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var CaptureDialog = {
  _video: null,
  _container: null,
  _lastOrientationEvent: null,
  
  init: function() {
    document.getElementsByAttribute('command', 'cmd_ok')[0].focus();
    this._video = document.getElementById("capturepicker-video");
    this._container = document.getElementById("capturepicker-container");
    window.addEventListener("resize", this, false);
    window.addEventListener("deviceorientation", this, false);
    this.handleEvent({ type: "resize" });
  },
  
  setPreviewOrientation: function(aEvent) {
    if (window.innerWidth < window.innerHeight) {
      if (aEvent.beta > 0)
        this._video.style.MozTransform = "rotate(90deg)";
      else
        this._video.style.MozTransform = "rotate(-90deg)";
      this._container.classList.add("vertical");
    }
    else {
      if (aEvent.gamma > 0)
        this._video.style.MozTransform = "rotate(180deg)";
      else
        this._video.style.MozTransform = "";
      this._container.classList.remove("vertical");
    }
  },
  
  handleEvent: function(aEvent) {
    if (aEvent.type == "deviceorientation") {
      if (!this._lastOrientationEvent)
        this.setPreviewOrientation(aEvent);
      this._lastOrientationEvent = aEvent;
    }
    else if (aEvent.type == "resize") {
      if (this._lastOrientationEvent)
        this.setPreviewOrientation(this._lastOrientationEvent);
    }
  },
  
  cancel: function() {
    this.doClose(false);
  },
  
  capture: function() {
    this.doClose(true);
  },
  
  doClose: function(aResult) {
    window.removeEventListener("resize", this, false);
    window.removeEventListener("deviceorientation", this, false);
    let dialog = document.getElementById("capturepicker-dialog");
    dialog.arguments.result = aResult;
    if (aResult)
      dialog.arguments.path = this.saveFrame();
    document.getElementById("capturepicker-video").setAttribute("src", "");
    dialog.close();
  },
  
  saveFrame: function() {
    let Cc = Components.classes;
    let Ci = Components.interfaces;
    Components.utils.import("resource://gre/modules/Services.jsm");

    let width = 320;
    let height = 240;
    let rotation = 0;

    if (window.innerWidth < window.innerHeight) {
      width = 240;
      height = 320;
      if (this._lastOrientationEvent.beta > 0)
        rotation = Math.PI / 2;
      else
        rotation = -Math.PI / 2;
    }
    else {
      if (this._lastOrientationEvent.gamma > 0)
        rotation = Math.PI;
    }

    let canvas = document.createElementNS("http://www.w3.org/1999/xhtml", "canvas");
    canvas.width = width;
    canvas.height = height;
    let ctx = canvas.getContext("2d");
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    //ctx.save();
    if (rotation != 0) {
      if (rotation == Math.PI / 2)
        ctx.translate(width, 0);
      else if (rotation == -Math.PI / 2)
        ctx.translate(-width, 0);
      else
        ctx.translate(width, height);
      ctx.rotate(rotation);
    }
    ctx.drawImage(document.getElementById("capturepicker-video"), 0, 0, 320, 240);
    //ctx.restore();
    let url = canvas.toDataURL("image/png");
    
    let tmpDir = Services.dirsvc.get("TmpD", Ci.nsIFile);
    let file = tmpDir.clone();
    file.append("capture.png");
    file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0600);
    let persist = Cc["@mozilla.org/embedding/browser/nsWebBrowserPersist;1"].createInstance(Ci.nsIWebBrowserPersist);
  
    persist.persistFlags = Ci.nsIWebBrowserPersist.PERSIST_FLAGS_REPLACE_EXISTING_FILES | Ci.nsIWebBrowserPersist.PERSIST_FLAGS_AUTODETECT_APPLY_CONVERSION;

    let mDone = false;
    let mFailed = false;

    let progressListener = {
      onProgressChange: function() {
          /* Ignore progress callback */
      },
      onStateChange: function(aProgress, aRequest, aStateFlag, aStatus) {
        if (aStateFlag & Ci.nsIWebProgressListener.STATE_STOP) {
          mDone = true;
        }
      }
    }
    persist.progressListener = progressListener;

    let source = Services.io.newURI(url, "UTF8", null);;
    persist.saveURI(source, null, null, null, null, file);
    
    // don't wait more than 3 seconds for a successful save
    window.setTimeout(function() {
      mDone = true;
      mFailed = true;
    }, 3000);
    
    while (!mDone)
      Services.tm.currentThread.processNextEvent(true);
    
    return (mFailed ? null : file.path);
  }
}
