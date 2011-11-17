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
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Fabrice Desré <fabrice@mozilla.com>, Original author
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
