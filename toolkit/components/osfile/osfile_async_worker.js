if (this.Components) {
  throw new Error("This worker can only be loaded from a worker thread");
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */


// Worker thread for osfile asynchronous front-end

(function(exports) {
  "use strict";

   try {
     importScripts("resource://gre/modules/osfile.jsm");

     let LOG = exports.OS.Shared.LOG.bind(exports.OS.Shared.LOG, "Agent");

     /**
      * Communications with the controller.
      *
      * Accepts messages:
      * {fun:function_name, args:array_of_arguments_or_null, id:id}
      *
      * Sends messages:
      * {ok: result, id:id} / {fail: serialized_form_of_OS.File.Error, id:id}
      */
     self.onmessage = function onmessage(msg) {
       let data = msg.data;
       LOG("Received message", data);
       let id = data.id;
       let result;
       let exn;
       let durationMs;
       try {
         let method = data.fun;
         LOG("Calling method", method);
         let start;
         let options;
         if (data.args) {
           options = data.args[data.args.length - 1];
         }
         // If |outExecutionDuration| option was supplied, start measuring the
         // duration of the operation.
         if (typeof options === "object" && "outExecutionDuration" in options) {
           start = Date.now();
         }
         result = Agent[method].apply(Agent, data.args);
         if (start) {
           // Only record duration if the method succeeds.
           durationMs = Date.now() - start;
           LOG("Method took", durationMs, "MS");
         }
         LOG("Method", method, "succeeded");
       } catch (ex) {
         exn = ex;
         LOG("Error while calling agent method", exn, exn.stack);
       }
       // Now, post a reply, possibly as an uncaught error.
       // We post this message from outside the |try ... catch| block
       // to avoid capturing errors that take place during |postMessage| and
       // built-in serialization.
       if (!exn) {
         LOG("Sending positive reply", result, "id is", id);
         if (result instanceof Transfer) {
           // Take advantage of zero-copy transfers
           self.postMessage({ok: result.data, id: id, durationMs: durationMs},
             result.transfers);
         } else {
           self.postMessage({ok: result, id:id, durationMs: durationMs});
         }
       } else if (exn == StopIteration) {
         // StopIteration cannot be serialized automatically
         LOG("Sending back StopIteration");
         self.postMessage({StopIteration: true, id: id});
       } else if (exn instanceof exports.OS.File.Error) {
         LOG("Sending back OS.File error", exn, "id is", id);
         // Instances of OS.File.Error know how to serialize themselves
         // (deserialization ensures that we end up with OS-specific
         // instances of |OS.File.Error|)
         self.postMessage({fail: exports.OS.File.Error.toMsg(exn), id:id});
       } else {
         LOG("Sending back regular error", exn, exn.stack, "id is", id);
         // Other exceptions do not, and should be propagated through DOM's
         // built-in mechanism for uncaught errors, although this mechanism
         // may lose interesting information.
         throw exn;
       }
     };

     /**
      * A data structure used to track opened resources
      */
     let ResourceTracker = function ResourceTracker() {
       // A number used to generate ids
       this._idgen = 0;
       // A map from id to resource
       this._map = new Map();
     };
     ResourceTracker.prototype = {
       /**
        * Get a resource from its unique identifier.
        */
       get: function(id) {
         let result = this._map.get(id);
         if (result == null) {
           return result;
         }
         return result.resource;
       },
       /**
        * Remove a resource from its unique identifier.
        */
       remove: function(id) {
         if (!this._map.has(id)) {
           throw new Error("Cannot find resource id " + id);
         }
         this._map.delete(id);
       },
       /**
        * Add a resource, return a new unique identifier
        *
        * @param {*} resource A resource.
        * @param {*=} info Optional information. For debugging purposes.
        *
        * @return {*} A unique identifier. For the moment, this is a number,
        * but this might not remain the case forever.
        */
       add: function(resource, info) {
         let id = this._idgen++;
         this._map.set(id, {resource:resource, info:info});
         return id;
       },
       /**
        * Return a list of all open resources i.e. the ones still present in
        * ResourceTracker's _map.
        */
       listOpenedResources: function listOpenedResources() {
         return [resource.info.path for ([id, resource] of this._map)];
       }
     };

     /**
      * A map of unique identifiers to opened files.
      */
     let OpenedFiles = new ResourceTracker();

     /**
      * Execute a function in the context of a given file.
      *
      * @param {*} id A unique identifier, as used by |OpenFiles|.
      * @param {Function} f A function to call.
      * @param {boolean} ignoreAbsent If |true|, the error is ignored. Otherwise, the error causes an exception.
      * @return The return value of |f()|
      *
      * This function attempts to get the file matching |id|. If
      * the file exists, it executes |f| within the |this| set
      * to the corresponding file. Otherwise, it throws an error.
      */
     let withFile = function withFile(id, f) {
       let file = OpenedFiles.get(id);
       if (file == null) {
         throw new Error("Could not find File");
       }
       return f.call(file);
     };

     let OpenedDirectoryIterators = new ResourceTracker();
     let withDir = function withDir(fd, f, ignoreAbsent) {
       let file = OpenedDirectoryIterators.get(fd);
       if (file == null) {
         if (!ignoreAbsent) {
           throw new Error("Could not find Directory");
         }
         return;
       }
       if (!(file instanceof File.DirectoryIterator)) {
         throw new Error("file is not a directory iterator " + file.__proto__.toSource());
       }
       return f.call(file);
     };

     let Type = exports.OS.Shared.Type;

     let File = exports.OS.File;

     /**
      * A constructor used to transfer data to the caller
      * without copy.
      *
      * @param {*} data The data to return to the caller.
      * @param {Array} transfers An array of Transferable
      * values that should be moved instead of being copied.
      *
      * @constructor
      */
     let Transfer = function Transfer(data, transfers) {
       this.data = data;
       this.transfers = transfers;
     };

     /**
      * The agent.
      *
      * It is in charge of performing method-specific deserialization
      * of messages, calling the function/method of OS.File and serializing
      * back the results.
      */
     let Agent = {
       // Update worker's OS.Shared.DEBUG flag message from controller.
       SET_DEBUG: function SET_DEBUG (aDEBUG) {
         exports.OS.Shared.DEBUG = aDEBUG;
       },
       // Return worker's current OS.Shared.DEBUG value to controller.
       // Note: This is used for testing purposes.
       GET_DEBUG: function GET_DEBUG () {
         return exports.OS.Shared.DEBUG;
       },
       // Report file descriptors leaks.
       System_shutdown: function System_shutdown () {
         // Return information about both opened files and opened
         // directory iterators.
         return {
           openedFiles: OpenedFiles.listOpenedResources(),
           openedDirectoryIterators:
             OpenedDirectoryIterators.listOpenedResources()
         };
       },
       // Functions of OS.File
       stat: function stat(path) {
         return exports.OS.File.Info.toMsg(
           exports.OS.File.stat(Type.path.fromMsg(path)));
       },
       getCurrentDirectory: function getCurrentDirectory() {
         return exports.OS.Shared.Type.path.toMsg(File.getCurrentDirectory());
       },
       setCurrentDirectory: function setCurrentDirectory(path) {
         File.setCurrentDirectory(exports.OS.Shared.Type.path.fromMsg(path));
       },
       copy: function copy(sourcePath, destPath, options) {
         return File.copy(Type.path.fromMsg(sourcePath),
           Type.path.fromMsg(destPath), options);
       },
       move: function move(sourcePath, destPath, options) {
         return File.move(Type.path.fromMsg(sourcePath),
           Type.path.fromMsg(destPath), options);
       },
       makeDir: function makeDir(path, options) {
         return File.makeDir(Type.path.fromMsg(path), options);
       },
       removeEmptyDir: function removeEmptyDir(path, options) {
         return File.removeEmptyDir(Type.path.fromMsg(path), options);
       },
       remove: function remove(path) {
         return File.remove(Type.path.fromMsg(path));
       },
       open: function open(path, mode, options) {
         let filePath = Type.path.fromMsg(path);
         let file = File.open(filePath, mode, options);
         return OpenedFiles.add(file, {
           // Adding path information to keep track of opened files
           // to report leaks when debugging.
           path: filePath
         });
       },
       read: function read(path, bytes) {
         let data = File.read(Type.path.fromMsg(path), bytes);
         return new Transfer({buffer: data.buffer, byteOffset: data.byteOffset, byteLength: data.byteLength}, [data.buffer]);
       },
       exists: function exists(path) {
         return File.exists(Type.path.fromMsg(path));
       },
       writeAtomic: function writeAtomic(path, buffer, options) {
         if (options.tmpPath) {
           options.tmpPath = Type.path.fromMsg(options.tmpPath);
         }
         return File.writeAtomic(Type.path.fromMsg(path),
                                 Type.voidptr_t.fromMsg(buffer),
                                 options
                                );
       },
       new_DirectoryIterator: function new_DirectoryIterator(path, options) {
         let directoryPath = Type.path.fromMsg(path);
         let iterator = new File.DirectoryIterator(directoryPath, options);
         return OpenedDirectoryIterators.add(iterator, {
           // Adding path information to keep track of opened directory
           // iterators to report leaks when debugging.
           path: directoryPath
         });
       },
       // Methods of OS.File
       File_prototype_close: function close(fd) {
         return withFile(fd,
           function do_close() {
             try {
               return this.close();
             } finally {
               OpenedFiles.remove(fd);
             }
         });
       },
       File_prototype_stat: function stat(fd) {
         return withFile(fd,
           function do_stat() {
             return exports.OS.File.Info.toMsg(this.stat());
           });
       },
       File_prototype_read: function read(fd, nbytes, options) {
         return withFile(fd,
           function do_read() {
             let data = this.read(nbytes, options);
             return new Transfer({buffer: data.buffer, byteOffset: data.byteOffset, byteLength: data.byteLength}, [data.buffer]);
           }
         );
       },
       File_prototype_readTo: function readTo(fd, buffer, options) {
         return withFile(fd,
           function do_readTo() {
             return this.readTo(exports.OS.Shared.Type.voidptr_t.fromMsg(buffer),
             options);
           });
       },
       File_prototype_write: function write(fd, buffer, options) {
         return withFile(fd,
           function do_write() {
             return this.write(exports.OS.Shared.Type.voidptr_t.fromMsg(buffer),
             options);
           });
       },
       File_prototype_setPosition: function setPosition(fd, pos, whence) {
         return withFile(fd,
           function do_setPosition() {
             return this.setPosition(pos, whence);
           });
       },
       File_prototype_getPosition: function getPosition(fd) {
         return withFile(fd,
           function do_getPosition() {
             return this.getPosition();
           });
       },
       // Methods of OS.File.DirectoryIterator
       DirectoryIterator_prototype_next: function next(dir) {
         return withDir(dir,
           function do_next() {
             try {
               return File.DirectoryIterator.Entry.toMsg(this.next());
             } catch (x) {
               if (x == StopIteration) {
                 OpenedDirectoryIterators.remove(dir);
               }
               throw x;
             }
           }, false);
       },
       DirectoryIterator_prototype_nextBatch: function nextBatch(dir, size) {
         return withDir(dir,
           function do_nextBatch() {
             let result;
             try {
               result = this.nextBatch(size);
             } catch (x) {
               OpenedDirectoryIterators.remove(dir);
               throw x;
             }
             return result.map(File.DirectoryIterator.Entry.toMsg);
           }, false);
       },
       DirectoryIterator_prototype_close: function close(dir) {
         return withDir(dir,
           function do_close() {
             this.close();
             OpenedDirectoryIterators.remove(dir);
           }, true);// ignore error to support double-closing |DirectoryIterator|
       },
       DirectoryIterator_prototype_exists: function exists(dir) {
         return withDir(dir,
           function do_exists() {
             return this.exists();
           });
       }
     };
  } catch(ex) {
    dump("WORKER ERROR DURING SETUP " + ex + "\n");
    dump("WORKER ERROR DETAIL " + ex.stack + "\n");
  }
})(this);
