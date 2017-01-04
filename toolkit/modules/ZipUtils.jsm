/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = [ "ZipUtils" ];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
                                  "resource://gre/modules/FileUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Promise",
                                  "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");


// The maximum amount of file data to buffer at a time during file extraction
const EXTRACTION_BUFFER               = 1024 * 512;


/**
 * Asynchronously writes data from an nsIInputStream to an OS.File instance.
 * The source stream and OS.File are closed regardless of whether the operation
 * succeeds or fails.
 * Returns a promise that will be resolved when complete.
 *
 * @param  aPath
 *         The name of the file being extracted for logging purposes.
 * @param  aStream
 *         The source nsIInputStream.
 * @param  aFile
 *         The open OS.File instance to write to.
 */
function saveStreamAsync(aPath, aStream, aFile) {
  let deferred = Promise.defer();

  // Read the input stream on a background thread
  let sts = Cc["@mozilla.org/network/stream-transport-service;1"].
            getService(Ci.nsIStreamTransportService);
  let transport = sts.createInputTransport(aStream, -1, -1, true);
  let input = transport.openInputStream(0, 0, 0)
                       .QueryInterface(Ci.nsIAsyncInputStream);
  let source = Cc["@mozilla.org/binaryinputstream;1"].
               createInstance(Ci.nsIBinaryInputStream);
  source.setInputStream(input);


  function readFailed(error) {
    try {
      aStream.close();
    } catch (e) {
      logger.error("Failed to close JAR stream for " + aPath);
    }

    aFile.close().then(function() {
      deferred.reject(error);
    }, function(e) {
      logger.error("Failed to close file for " + aPath);
      deferred.reject(error);
    });
  }

  function readData() {
    try {
      let count = Math.min(source.available(), EXTRACTION_BUFFER);
      let data = new Uint8Array(count);
      source.readArrayBuffer(count, data.buffer);

      aFile.write(data, { bytes: count }).then(function() {
        input.asyncWait(readData, 0, 0, Services.tm.currentThread);
      }, readFailed);
    } catch (e) {
      if (e.result == Cr.NS_BASE_STREAM_CLOSED)
        deferred.resolve(aFile.close());
      else
        readFailed(e);
    }
  }

  input.asyncWait(readData, 0, 0, Services.tm.currentThread);

  return deferred.promise;
}


this.ZipUtils = {

  /**
   * Asynchronously extracts files from a ZIP file into a directory.
   * Returns a promise that will be resolved when the extraction is complete.
   *
   * @param  aZipFile
   *         The source ZIP file that contains the add-on.
   * @param  aDir
   *         The nsIFile to extract to.
   */
  extractFilesAsync: function ZipUtils_extractFilesAsync(aZipFile, aDir) {
    let zipReader = Cc["@mozilla.org/libjar/zip-reader;1"].
                    createInstance(Ci.nsIZipReader);

    try {
      zipReader.open(aZipFile);
    } catch (e) {
      return Promise.reject(e);
    }

    return Task.spawn(function* () {
      // Get all of the entries in the zip and sort them so we create directories
      // before files
      let entries = zipReader.findEntries(null);
      let names = [];
      while (entries.hasMore())
        names.push(entries.getNext());
      names.sort();

      for (let name of names) {
        let entryName = name;
        let zipentry = zipReader.getEntry(name);
        let path = OS.Path.join(aDir.path, ...name.split("/"));

        if (zipentry.isDirectory) {
          try {
            yield OS.File.makeDir(path);
          } catch (e) {
            dump("extractFilesAsync: failed to create directory " + path + "\n");
            throw e;
          }
        } else {
          let options = { unixMode: zipentry.permissions | FileUtils.PERMS_FILE };
          try {
            let file = yield OS.File.open(path, { truncate: true }, options);
            if (zipentry.realSize == 0)
              yield file.close();
            else
              yield saveStreamAsync(path, zipReader.getInputStream(entryName), file);
          } catch (e) {
            dump("extractFilesAsync: failed to extract file " + path + "\n");
            throw e;
          }
        }
      }

      zipReader.close();
    }).then(null, (e) => {
      zipReader.close();
      throw e;
    });
  },

  /**
   * Extracts files from a ZIP file into a directory.
   *
   * @param  aZipFile
   *         The source ZIP file that contains the add-on.
   * @param  aDir
   *         The nsIFile to extract to.
   */
  extractFiles: function ZipUtils_extractFiles(aZipFile, aDir) {
    function getTargetFile(aDir, entry) {
      let target = aDir.clone();
      entry.split("/").forEach(function(aPart) {
        target.append(aPart);
      });
      return target;
    }

    let zipReader = Cc["@mozilla.org/libjar/zip-reader;1"].
                    createInstance(Ci.nsIZipReader);
    zipReader.open(aZipFile);

    try {
      // create directories first
      let entries = zipReader.findEntries("*/");
      while (entries.hasMore()) {
        let entryName = entries.getNext();
        let target = getTargetFile(aDir, entryName);
        if (!target.exists()) {
          try {
            target.create(Ci.nsIFile.DIRECTORY_TYPE,
                          FileUtils.PERMS_DIRECTORY);
          } catch (e) {
            dump("extractFiles: failed to create target directory for extraction file = " + target.path + "\n");
          }
        }
      }

      entries = zipReader.findEntries(null);
      while (entries.hasMore()) {
        let entryName = entries.getNext();
        let target = getTargetFile(aDir, entryName);
        if (target.exists())
          continue;

        zipReader.extract(entryName, target);
        try {
          target.permissions |= FileUtils.PERMS_FILE;
        } catch (e) {
          dump("Failed to set permissions " + FileUtils.PERMS_FILE.toString(8) + " on " + target.path + " " + e + "\n");
        }
      }
    } finally {
      zipReader.close();
    }
  }

};
