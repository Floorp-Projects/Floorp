/* import-globals-from head_cache.js */
/* import-globals-from head_channels.js */

"use strict";

var callbacks = [];

// Expect an existing entry
const NORMAL = 0;
// Expect a new entry
const NEW = 1 << 0;
// Return early from onCacheEntryCheck and set the callback to state it expects onCacheEntryCheck to happen
const NOTVALID = 1 << 1;
// Throw from onCacheEntryAvailable
const THROWAVAIL = 1 << 2;
// Open entry for reading-only
const READONLY = 1 << 3;
// Expect the entry to not be found
const NOTFOUND = 1 << 4;
// Return ENTRY_NEEDS_REVALIDATION from onCacheEntryCheck
const REVAL = 1 << 5;
// Return ENTRY_PARTIAL from onCacheEntryCheck, in combo with NEW or RECREATE bypasses check for emptiness of the entry
const PARTIAL = 1 << 6;
// Expect the entry is doomed, i.e. the output stream should not be possible to open
const DOOMED = 1 << 7;
// Don't trigger the go-on callback until the entry is written
const WAITFORWRITE = 1 << 8;
// Don't write data (i.e. don't open output stream)
const METAONLY = 1 << 9;
// Do recreation of an existing cache entry
const RECREATE = 1 << 10;
// Do not give me the entry
const NOTWANTED = 1 << 11;
// Tell the cache to wait for the entry to be completely written first
const COMPLETE = 1 << 12;
// Don't write meta/data and don't set valid in the callback, consumer will do it manually
const DONTFILL = 1 << 13;
// Used in combination with METAONLY, don't call setValid() on the entry after metadata has been set
const DONTSETVALID = 1 << 14;
// Notify before checking the data, useful for proper callback ordering checks
const NOTIFYBEFOREREAD = 1 << 15;
// It's allowed to not get an existing entry (result of opening is undetermined)
const MAYBE_NEW = 1 << 16;

var log_c2 = true;
function LOG_C2(o, m) {
  if (!log_c2) {
    return;
  }
  if (!m) {
    dump("TEST-INFO | CACHE2: " + o + "\n");
  } else {
    dump(
      "TEST-INFO | CACHE2: callback #" +
        o.order +
        "(" +
        (o.workingData ? o.workingData.substr(0, 10) : "---") +
        ") " +
        m +
        "\n"
    );
  }
}

function pumpReadStream(inputStream, goon) {
  if (inputStream.isNonBlocking()) {
    // non-blocking stream, must read via pump
    var pump = Cc["@mozilla.org/network/input-stream-pump;1"].createInstance(
      Ci.nsIInputStreamPump
    );
    pump.init(inputStream, 0, 0, true);
    var data = "";
    pump.asyncRead(
      {
        onStartRequest(aRequest) {},
        onDataAvailable(aRequest, aInputStream, aOffset, aCount) {
          var wrapper = Cc[
            "@mozilla.org/scriptableinputstream;1"
          ].createInstance(Ci.nsIScriptableInputStream);
          wrapper.init(aInputStream);
          var str = wrapper.read(wrapper.available());
          LOG_C2("reading data '" + str.substring(0, 5) + "'");
          data += str;
        },
        onStopRequest(aRequest, aStatusCode) {
          LOG_C2("done reading data: " + aStatusCode);
          Assert.equal(aStatusCode, Cr.NS_OK);
          goon(data);
        },
      },
      null
    );
  } else {
    // blocking stream
    var data = read_stream(inputStream, inputStream.available());
    goon(data);
  }
}

OpenCallback.prototype = {
  QueryInterface: ChromeUtils.generateQI(["nsICacheEntryOpenCallback"]),
  onCacheEntryCheck(entry, appCache) {
    LOG_C2(this, "onCacheEntryCheck");
    Assert.ok(!this.onCheckPassed);
    this.onCheckPassed = true;

    if (this.behavior & NOTVALID) {
      LOG_C2(this, "onCacheEntryCheck DONE, return ENTRY_WANTED");
      return Ci.nsICacheEntryOpenCallback.ENTRY_WANTED;
    }

    if (this.behavior & NOTWANTED) {
      LOG_C2(this, "onCacheEntryCheck DONE, return ENTRY_NOT_WANTED");
      return Ci.nsICacheEntryOpenCallback.ENTRY_NOT_WANTED;
    }

    Assert.equal(entry.getMetaDataElement("meto"), this.workingMetadata);

    // check for sane flag combination
    Assert.notEqual(this.behavior & (REVAL | PARTIAL), REVAL | PARTIAL);

    if (this.behavior & (REVAL | PARTIAL)) {
      LOG_C2(this, "onCacheEntryCheck DONE, return ENTRY_NEEDS_REVALIDATION");
      return Ci.nsICacheEntryOpenCallback.ENTRY_NEEDS_REVALIDATION;
    }

    if (this.behavior & COMPLETE) {
      LOG_C2(
        this,
        "onCacheEntryCheck DONE, return RECHECK_AFTER_WRITE_FINISHED"
      );
      // Specific to the new backend because of concurrent read/write:
      // when a consumer returns RECHECK_AFTER_WRITE_FINISHED from onCacheEntryCheck
      // the cache calls this callback again after the entry write has finished.
      // This gives the consumer a chance to recheck completeness of the entry
      // again.
      // Thus, we reset state as onCheck would have never been called.
      this.onCheckPassed = false;
      // Don't return RECHECK_AFTER_WRITE_FINISHED on second call of onCacheEntryCheck.
      this.behavior &= ~COMPLETE;
      return Ci.nsICacheEntryOpenCallback.RECHECK_AFTER_WRITE_FINISHED;
    }

    LOG_C2(this, "onCacheEntryCheck DONE, return ENTRY_WANTED");
    return Ci.nsICacheEntryOpenCallback.ENTRY_WANTED;
  },
  onCacheEntryAvailable(entry, isnew, appCache, status) {
    if (this.behavior & MAYBE_NEW && isnew) {
      this.behavior |= NEW;
    }

    LOG_C2(this, "onCacheEntryAvailable, " + this.behavior);
    Assert.ok(!this.onAvailPassed);
    this.onAvailPassed = true;

    Assert.equal(isnew, !!(this.behavior & NEW));

    if (this.behavior & (NOTFOUND | NOTWANTED)) {
      Assert.equal(status, Cr.NS_ERROR_CACHE_KEY_NOT_FOUND);
      Assert.ok(!entry);
      if (this.behavior & THROWAVAIL) {
        this.throwAndNotify(entry);
      }
      this.goon(entry);
    } else if (this.behavior & (NEW | RECREATE)) {
      Assert.ok(!!entry);

      if (this.behavior & RECREATE) {
        entry = entry.recreate();
        Assert.ok(!!entry);
      }

      if (this.behavior & THROWAVAIL) {
        this.throwAndNotify(entry);
      }

      if (!(this.behavior & WAITFORWRITE)) {
        this.goon(entry);
      }

      if (!(this.behavior & PARTIAL)) {
        try {
          entry.getMetaDataElement("meto");
          Assert.ok(false);
        } catch (ex) {}
      }

      if (this.behavior & DONTFILL) {
        Assert.equal(false, this.behavior & WAITFORWRITE);
        return;
      }

      var self = this;
      executeSoon(function() {
        // emulate network latency
        entry.setMetaDataElement("meto", self.workingMetadata);
        entry.metaDataReady();
        if (self.behavior & METAONLY) {
          // Since forcing GC/CC doesn't trigger OnWriterClosed, we have to set the entry valid manually :(
          if (!(self.behavior & DONTSETVALID)) {
            entry.setValid();
          }

          entry.close();
          if (self.behavior & WAITFORWRITE) {
            self.goon(entry);
          }

          return;
        }
        executeSoon(function() {
          // emulate more network latency
          if (self.behavior & DOOMED) {
            LOG_C2(self, "checking doom state");
            try {
              var os = entry.openOutputStream(0, -1);
              // Unfortunately, in the undetermined state we cannot even check whether the entry
              // is actually doomed or not.
              os.close();
              Assert.ok(!!(self.behavior & MAYBE_NEW));
            } catch (ex) {
              Assert.ok(true);
            }
            if (self.behavior & WAITFORWRITE) {
              self.goon(entry);
            }
            return;
          }

          var offset = self.behavior & PARTIAL ? entry.dataSize : 0;
          LOG_C2(self, "openOutputStream @ " + offset);
          var os = entry.openOutputStream(offset, -1);
          LOG_C2(self, "writing data");
          var wrt = os.write(self.workingData, self.workingData.length);
          Assert.equal(wrt, self.workingData.length);
          os.close();
          if (self.behavior & WAITFORWRITE) {
            self.goon(entry);
          }

          entry.close();
        });
      });
    } else {
      // NORMAL
      Assert.ok(!!entry);
      Assert.equal(entry.getMetaDataElement("meto"), this.workingMetadata);
      if (this.behavior & THROWAVAIL) {
        this.throwAndNotify(entry);
      }
      if (this.behavior & NOTIFYBEFOREREAD) {
        this.goon(entry, true);
      }

      var wrapper = Cc["@mozilla.org/scriptableinputstream;1"].createInstance(
        Ci.nsIScriptableInputStream
      );
      var self = this;
      pumpReadStream(entry.openInputStream(0), function(data) {
        Assert.equal(data, self.workingData);
        self.onDataCheckPassed = true;
        LOG_C2(self, "entry read done");
        self.goon(entry);
        entry.close();
      });
    }
  },
  selfCheck() {
    LOG_C2(this, "selfCheck");

    Assert.ok(this.onCheckPassed || this.behavior & MAYBE_NEW);
    Assert.ok(this.onAvailPassed);
    Assert.ok(this.onDataCheckPassed || this.behavior & MAYBE_NEW);
  },
  throwAndNotify(entry) {
    LOG_C2(this, "Throwing");
    var self = this;
    executeSoon(function() {
      LOG_C2(self, "Notifying");
      self.goon(entry);
    });
    throw Components.Exception("", Cr.NS_ERROR_FAILURE);
  },
};

function OpenCallback(behavior, workingMetadata, workingData, goon) {
  this.behavior = behavior;
  this.workingMetadata = workingMetadata;
  this.workingData = workingData;
  this.goon = goon;
  this.onCheckPassed =
    (!!(behavior & (NEW | RECREATE)) || !workingMetadata) &&
    !(behavior & NOTVALID);
  this.onAvailPassed = false;
  this.onDataCheckPassed =
    !!(behavior & (NEW | RECREATE | NOTWANTED)) || !workingMetadata;
  callbacks.push(this);
  this.order = callbacks.length;
}

VisitCallback.prototype = {
  QueryInterface: ChromeUtils.generateQI(["nsICacheStorageVisitor"]),
  onCacheStorageInfo(num, consumption) {
    LOG_C2(this, "onCacheStorageInfo: num=" + num + ", size=" + consumption);
    Assert.equal(this.num, num);
    Assert.equal(this.consumption, consumption);
    if (!this.entries) {
      this.notify();
    }
  },
  onCacheEntryInfo(
    aURI,
    aIdEnhance,
    aDataSize,
    aFetchCount,
    aLastModifiedTime,
    aExpirationTime,
    aPinned,
    aInfo
  ) {
    var key = (aIdEnhance ? aIdEnhance + ":" : "") + aURI.asciiSpec;
    LOG_C2(this, "onCacheEntryInfo: key=" + key);

    function findCacheIndex(element) {
      if (typeof element === "string") {
        return element === key;
      } else if (typeof element === "object") {
        return (
          element.uri === key &&
          element.lci.isAnonymous === aInfo.isAnonymous &&
          ChromeUtils.isOriginAttributesEqual(
            element.lci.originAttributes,
            aInfo.originAttributes
          )
        );
      }

      return false;
    }

    Assert.ok(!!this.entries);

    var index = this.entries.findIndex(findCacheIndex);
    Assert.ok(index > -1);

    this.entries.splice(index, 1);
  },
  onCacheEntryVisitCompleted() {
    LOG_C2(this, "onCacheEntryVisitCompleted");
    if (this.entries) {
      Assert.equal(this.entries.length, 0);
    }
    this.notify();
  },
  notify() {
    Assert.ok(!!this.goon);
    var goon = this.goon;
    this.goon = null;
    executeSoon(goon);
  },
  selfCheck() {
    Assert.ok(!this.entries || !this.entries.length);
  },
};

function VisitCallback(num, consumption, entries, goon) {
  this.num = num;
  this.consumption = consumption;
  this.entries = entries;
  this.goon = goon;
  callbacks.push(this);
  this.order = callbacks.length;
}

EvictionCallback.prototype = {
  QueryInterface: ChromeUtils.generateQI(["nsICacheEntryDoomCallback"]),
  onCacheEntryDoomed(result) {
    Assert.equal(this.expectedSuccess, result == Cr.NS_OK);
    this.goon();
  },
  selfCheck() {},
};

function EvictionCallback(success, goon) {
  this.expectedSuccess = success;
  this.goon = goon;
  callbacks.push(this);
  this.order = callbacks.length;
}

MultipleCallbacks.prototype = {
  fired() {
    if (--this.pending == 0) {
      var self = this;
      if (this.delayed) {
        executeSoon(function() {
          self.goon();
        });
      } else {
        this.goon();
      }
    }
  },
  add() {
    ++this.pending;
  },
};

function MultipleCallbacks(number, goon, delayed) {
  this.pending = number;
  this.goon = goon;
  this.delayed = delayed;
}

function wait_for_cache_index(continue_func) {
  // This callback will not fire before the index is in the ready state.  nsICacheStorage.exists() will
  // no longer throw after this point.
  get_cache_service().asyncGetDiskConsumption({
    onNetworkCacheDiskConsumption() {
      continue_func();
    },
    // eslint-disable-next-line mozilla/use-chromeutils-generateqi
    QueryInterface() {
      return this;
    },
  });
}

function finish_cache2_test() {
  callbacks.forEach(function(callback, index) {
    callback.selfCheck();
  });
  do_test_finished();
}
