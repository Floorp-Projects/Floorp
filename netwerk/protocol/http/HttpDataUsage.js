/* -*- indent-tabs-mode: nil -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* HTTPDATA_* telemetry producer
   every 3 minutes of idle time we update a data file and report it
   once a day. this avoids adding io to the shutdown path
*/

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;
const CC = Components.Constructor;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "idleService",
                                   "@mozilla.org/widget/idleservice;1",
                                   "nsIIdleService");
XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
                                  "resource://gre/modules/FileUtils.jsm");

const MB = 1000000;

var gDataUsage;
function HttpDataUsage() {}
HttpDataUsage.prototype = {
    classID: Components.ID("{6d72bfca-2747-4859-887f-6f06d4ce6787}"),
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),
    contractID: "@mozilla.org/network/HttpDataUsage;1",

    _isIdleObserver: false,
    _locked : false,
    _idle_timeout : 15,
    _quanta : 86400000, // per day

    _logtime : new Date(),
    _ethernetRead : 0,
    _ethernetWritten : 0,
    _cellRead : 0,
    _cellWritten : 0,

    _dataFile : FileUtils.getFile("ProfD", ["httpDataUsage.dat"], true),
    _dataUsage : Cc["@mozilla.org/network/protocol;1?name=http"]
        .getService(Ci.nsIHttpProtocolHandler)
        .QueryInterface(Ci.nsIHttpDataUsage),
    _pipe : CC("@mozilla.org/pipe;1", "nsIPipe", "init"),
    _outputStream : CC("@mozilla.org/network/file-output-stream;1",
                       "nsIFileOutputStream", "init"),

    setup: function setup() {
        gDataUsage = this;

        var enabled = false;
        try {
            if (Services.prefs.getBoolPref("toolkit.telemetry.enabled"))
                enabled = true;
        } catch (e) { }
        try {
            if (Services.prefs.getBoolPref("toolkit.telemetry.enabledPreRelease"))
                enabled = true;
        } catch (e) { }

        // this isn't important enough to worry about getting a
        // runtime telemetry config change for
        if (!enabled)
            return;

        if (this._dataUsage == null)
            return;

        Services.obs.addObserver(this, "quit-application", false);
        idleService.addIdleObserver(this, this._idle_timeout);
        this._isIdleObserver = true;
    },
    
    shutdown: function shutdown() {
        if (this._isIdleObserver) {
            idleService.removeIdleObserver(this, this._idle_timeout);
            Services.obs.removeObserver(this, "quit-application");
        }
        this._isIdleObserver = false;
    },

    sUpdateStats2: function sUpdateStats2(stream, result) {
        gDataUsage.updateStats2(stream, result);
    },

    readCounters: function readCounters(stream, result) {
        if (Components.isSuccessCode(result)) {
            let count = stream.available();
            let data = NetUtil.readInputStreamToString(stream, count);
            var list = data.split(",");
            if (list.length == 5) {
                this._logtime = new Date(Number(list[0]));
                this._ethernetRead = Number(list[1]);
                this._ethernetWritten = Number(list[2]);
                this._cellRead = Number(list[3]);
                this._cellWritten = Number(list[4]);
            }
        }

        this._ethernetRead += this._dataUsage.ethernetBytesRead;
        this._ethernetWritten += this._dataUsage.ethernetBytesWritten;
        this._cellRead += this._dataUsage.cellBytesRead;
        this._cellWritten += this._dataUsage.cellBytesWritten;
        this._dataUsage.resetHttpDataUsage();
    },

    writeCounters: function writeCounters() {
        var dataout = this._logtime.getTime().toString() + "," +
            this._ethernetRead.toString() + "," +
            this._ethernetWritten.toString() + "," +
            this._cellRead.toString() + "," +
            this._cellWritten.toString() + "\n";

        var buffer = new this._pipe(true, false, 4096, 1, null);
        buffer.outputStream.write(dataout, dataout.length);
        buffer.outputStream.close();
        var fileOut = new this._outputStream(this._dataFile, -1, -1, 0);

        NetUtil.asyncCopy(buffer.inputStream, fileOut,
                          function (result) { gDataUsage.finishedWriting(); });
    },

    updateStats2: function updateStats2(stream, result) {
        this.readCounters(stream, result);
        this.submitTelemetry();
    },

    submitTelemetry: function submitTelemetry() {
        var now = new Date();
        var elapsed = now.getTime() - this._logtime.getTime();
        // make sure we have at least 1 day of data.. if not just write new data out
        if (elapsed < this._quanta) {
            this.writeCounters();
            return;
        }

        var days = elapsed / this._quanta;
        var eInPerQuanta = Math.floor(this._ethernetRead / days);
        var eOutPerQuanta = Math.floor(this._ethernetWritten / days);
        var cInPerQuanta = Math.floor(this._cellRead / days);
        var cOutPerQuanta = Math.floor(this._cellWritten / days);
        
        var histogram;

        while (elapsed >= this._quanta) {
            histogram = Services.telemetry.getHistogramById("HTTPDATA_DAILY_ETHERNET_IN");
            histogram.add(Math.round(eInPerQuanta / MB));
            histogram = Services.telemetry.getHistogramById("HTTPDATA_DAILY_ETHERNET_OUT");
            histogram.add(Math.round(eOutPerQuanta / MB));
            histogram = Services.telemetry.getHistogramById("HTTPDATA_DAILY_CELL_IN");
            histogram.add(Math.round(cInPerQuanta / MB));
            histogram = Services.telemetry.getHistogramById("HTTPDATA_DAILY_CELL_OUT");
            histogram.add(Math.round(cOutPerQuanta / MB));

            elapsed -= this._quanta;
            this._ethernetRead -= eInPerQuanta;
            this._ethernetWritten -= eOutPerQuanta;
            this._cellRead -= cInPerQuanta;
            this._cellWritten -= cOutPerQuanta;
        }
        this._logtime = new Date(now.getTime() - elapsed);

        // need to write back the decremented counters
        this.writeCounters();
    },

    finishedWriting : function finishedWriting() {
        this._locked = false; // all done
    },

    updateStats: function updateStats() {
        if (this._locked)
            return;
        this._locked = true;
        
        NetUtil.asyncFetch(this._dataFile, this.sUpdateStats2);
    },

    observe: function (aSubject, aTopic, aData) {
        switch (aTopic) {
        case "profile-after-change":
            this.setup();
            break;
        case "idle":
            this.updateStats();
            break;
        case "quit-application":
            this.updateStats();
            this.shutdown();
            break;
        }
    },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([HttpDataUsage]);
