/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["PlacesTransactions"];

/**
 * Overview
 * --------
 * This modules serves as the transactions manager for Places (hereinafter PTM).
 * It implements all the elementary transactions for its UI commands: creating
 * items, editing their various properties, and so forth.
 *
 * Note that since the effect of invoking a Places command is not limited to the
 * window in which it was performed (e.g. a folder created in the Library may be
 * the parent of a bookmark created in some browser window), PTM is a singleton.
 * It's therefore unnecessary to initialize PTM in any way apart importing this
 * module.
 *
 * PTM shares most of its semantics with common command pattern implementations.
 * However, the asynchronous design of contemporary and future APIs, combined
 * with the commitment to serialize all UI operations, does make things a little
 * bit different.  For example, when |undo| is called in order to undo the top
 * undo entry, the caller cannot tell for sure what entry would it be, because
 * the execution of some transactions is either in process, or enqueued to be.
 *
 * Also note that unlike the nsITransactionManager, for example, this API is by
 * no means generic.  That is, it cannot be used to execute anything but the
 * elementary transactions implemented here (Please file a bug if you find
 * anything uncovered).  More-complex transactions (e.g. creating a folder and
 * moving a bookmark into it) may be implemented as a batch (see below).
 *
 * A note about GUIDs and item-ids
 * -------------------------------
 * There's an ongoing effort (see bug 1071511) to deprecate item-ids in Places
 * in favor of GUIDs.  Both because new APIs (e.g. Bookmark.jsm) expose them to
 * the minimum necessary, and because GUIDs play much better with implementing
 * |redo|, this API doesn't support item-ids at all, and only accepts bookmark
 * GUIDs, both for input (e.g. for setting the parent folder for a new bookmark)
 * and for output (when the GUID for such a bookmark is propagated).
 *
 * When working in conjugation with older Places API which only expose item ids,
 * use PlacesUtils.promiseItemGuid for converting those to GUIDs (note that
 * for result nodes, the guid is available through their bookmarkGuid getter).
 * Should you need to convert GUIDs to item-ids, use PlacesUtils.promiseItemId.
 *
 * Constructing transactions
 * -------------------------
 * At the bottom of this module you will find transactions for all Places UI
 * commands.  They are exposed as constructors set on the PlacesTransactions
 * object (e.g. PlacesTransactions.NewFolder).  The input for this constructors
 * is taken in the form of a single argument, a plain object consisting of the
 * properties for the transaction.  Input properties may be either required or
 * optional (for example, |keyword| is required for the EditKeyword transaction,
 * but optional for the NewBookmark transaction).
 *
 * To make things simple, a given input property has the same basic meaning and
 * valid values across all transactions which accept it in the input object.
 * Here is a list of all supported input properties along with their expected
 * values:
 *  - url: a URL object, an nsIURI object, or a href.
 *  - urls: an array of urls, as above.
 *  - feedUrl: an url (as above), holding the url for a live bookmark.
 *  - siteUrl an url (as above), holding the url for the site with which
 *            a live bookmark is associated.
 *  - tag - a string.
 *  - tags: an array of strings.
 *  - guid, parentGuid, newParentGuid: a valid Places GUID string.
 *  - guids: an array of valid Places GUID strings.
 *  - title: a string
 *  - index, newIndex: the position of an item in its containing folder,
 *    starting from 0.
 *    integer and PlacesUtils.bookmarks.DEFAULT_INDEX
 *  - annotation: see PlacesUtils.setAnnotationsForItem
 *  - annotations: an array of annotation objects as above.
 *  - excludingAnnotation: a string (annotation name).
 *  - excludingAnnotations: an array of string (annotation names).
 *
 * If a required property is missing in the input object (e.g. not specifying
 * parentGuid for NewBookmark), or if the value for any of the input properties
 * is invalid "on the surface" (e.g. a numeric value for GUID, or a string that
 * isn't 12-characters long), the transaction constructor throws right way.
 * More complex errors (e.g. passing a non-existent GUID for parentGuid) only
 * reveal once the transaction is executed.
 *
 * Executing Transactions (the |transact| method of transactions)
 * --------------------------------------------------------------
 * Once a transaction is created, you must call its |transact| method for it to
 * be executed and take effect.  |transact| is an asynchronous method that takes
 * no arguments, and returns a promise that resolves once the transaction is
 * executed.  Executing one of the transactions for creating items (NewBookmark,
 * NewFolder, NewSeparator or NewLivemark) resolve to the new item's GUID.
 * There's no resolution value for other transactions.
 * If a transaction fails to execute, |transact| rejects and the transactions
 * history is not affected.
 *
 * |transact| throws if it's called more than once (successfully or not) on the
 * same transaction object.
 *
 * Batches
 * -------
 * Sometimes it is useful to "batch" or "merge" transactions.  For example,
 * something like "Bookmark All Tabs" may be implemented as one NewFolder
 * transaction followed by numerous NewBookmark transactions - all to be undone
 * or redone in a single undo or redo command.  Use |PlacesTransactions.batch|
 * in such cases.  It can take either an array of transactions which will be
 * executed in the given order and later be treated a a single entry in the
 * transactions history, or a generator function that is passed to Task.spawn,
 * that is to "contain" the batch: once the generator function is called a batch
 * starts, and it lasts until the asynchronous generator iteration is complete
 * All transactions executed by |transact| during this time are to be treated as
 * a single entry in the transactions history.
 *
 * In both modes, |PlacesTransactions.batch| returns a promise that is to be
 * resolved when the batch ends.  In the array-input mode, there's no resolution
 * value.  In the generator mode, the resolution value is whatever the generator
 * function returned (the semantics are the same as in Task.spawn, basically).
 *
 * The array-input mode of |PlacesTransactions.batch| is useful for implementing
 * a batch of mostly-independent transaction (for example, |paste| into a folder
 * can be implemented as a batch of multiple NewBookmark transactions).
 * The generator mode is useful when the resolution value of executing one
 * transaction is the input of one more subsequent transaction.
 *
 * In the array-input mode, if any transactions fails to execute, the batch
 * continues (exceptions are logged).  Only transactions that were executed
 * successfully are added to the transactions history.
 *
 * WARNING: "nested" batches are not supported, if you call batch while another
 * batch is still running, the new batch is enqueued with all other PTM work
 * and thus not run until the running batch ends. The same goes for undo, redo
 * and clearTransactionsHistory (note batches cannot be done partially, meaning
 * undo and redo calls that during a batch are just enqueued).
 *
 * *****************************************************************************
 * IT"S PARTICULARLY IMPORTANT NOT TO YIELD ANY PROMISE RETURNED BY ANY OF
 * THESE METHODS (undo, redo, clearTransactionsHistory) FROM A BATCH FUNCTION.
 * UNTIL WE FIND A WAY TO THROW IN THAT CASE (SEE BUG 1091446) DOING SO WILL
 * COMPLETELY BREAK PTM UNTIL SHUTDOWN, NOT ALLOWING THE EXECUTION OF ANY
 * TRANSACTION!
 * *****************************************************************************
 *
 * Serialization
 * -------------
 * All |PlacesTransaction| operations are serialized.  That is, even though the
 * implementation is asynchronous, the order in which PlacesTransactions methods
 * is called does guarantee the order in which they are to be invoked.
 *
 * The only exception to this rule is |transact| calls done during a batch (see
 * above).  |transact| calls are serialized with each other (and with undo, redo
 * and clearTransactionsHistory), but they  are, of course, not serialized with
 * batches.
 *
 * The transactions-history structure
 * ----------------------------------
 * The transactions-history is a two-dimensional stack of transactions: the
 * transactions are ordered in reverse to the order they were committed.
 * It's two-dimensional because PTM allows batching transactions together for
 * the purpose of undo or redo (see Batches above).
 *
 * The undoPosition property is set to the index of the top entry. If there is
 * no entry at that index, there is nothing to undo.
 * Entries prior to undoPosition, if any, are redo entries, the first one being
 * the top redo entry.
 *
 * [ [2nd redo txn, 1st redo txn],  <= 2nd redo entry
 *   [2nd redo txn, 1st redo txn],  <= 1st redo entry
 *   [1st undo txn, 2nd undo txn],  <= 1st undo entry
 *   [1st undo txn, 2nd undo txn]   <= 2nd undo entry ]
 * undoPostion: 2.
 *
 * Note that when a new entry is created, all redo entries are removed.
 */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Promise",
                                  "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "console",
                                  "resource://gre/modules/Console.jsm");

Components.utils.importGlobalProperties(["URL"]);

var TransactionsHistory = [];
TransactionsHistory.__proto__ = {
  __proto__: Array.prototype,

  // The index of the first undo entry (if any) - See the documentation
  // at the top of this file.
  _undoPosition: 0,
  get undoPosition() {
    return this._undoPosition;
  },

  // Handy shortcuts
  get topUndoEntry() {
    return this.undoPosition < this.length ? this[this.undoPosition] : null;
  },
  get topRedoEntry() {
    return this.undoPosition > 0 ? this[this.undoPosition - 1] : null;
  },

  // Outside of this module, the API of transactions is inaccessible, and so
  // are any internal properties.  To achieve that, transactions are proxified
  // in their constructors.  This maps the proxies to their respective raw
  // objects.
  proxifiedToRaw: new WeakMap(),

  /**
   * Proxify a transaction object for consumers.
   * @param aRawTransaction
   *        the raw transaction object.
   * @return the proxified transaction object.
   * @see getRawTransaction for retrieving the raw transaction.
   */
  proxifyTransaction: function (aRawTransaction) {
    let proxy = Object.freeze({
      transact() {
        return TransactionsManager.transact(this);
      }
    });
    this.proxifiedToRaw.set(proxy, aRawTransaction);
    return proxy;
  },

  /**
   * Check if the given object is a the proxy object for some transaction.
   * @param aValue
   *        any JS value.
   * @return true if aValue is the proxy object for some transaction, false
   * otherwise.
   */
  isProxifiedTransactionObject(aValue) {
    return this.proxifiedToRaw.has(aValue);
  },

  /**
   * Get the raw transaction for the given proxy.
   * @param aProxy
   *        the proxy object
   * @return the transaction proxified by aProxy; |undefined| is returned if
   * aProxy is not a proxified transaction.
   */
  getRawTransaction(aProxy) {
    return this.proxifiedToRaw.get(aProxy);
  },

  /**
   * Add a transaction either as a new entry, if forced or if there are no undo
   * entries, or to the top undo entry.
   *
   * @param aProxifiedTransaction
   *        the proxified transaction object to be added to the transaction
   *        history.
   * @param [optional] aForceNewEntry
   *        Force a new entry for the transaction. Default: false.
   *        If false, an entry will we created only if there's no undo entry
   *        to extend.
   */
  add(aProxifiedTransaction, aForceNewEntry = false) {
    if (!this.isProxifiedTransactionObject(aProxifiedTransaction))
      throw new Error("aProxifiedTransaction is not a proxified transaction");

    if (this.length == 0 || aForceNewEntry) {
      this.clearRedoEntries();
      this.unshift([aProxifiedTransaction]);
    }
    else {
      this[this.undoPosition].unshift(aProxifiedTransaction);
    }
  },

  /**
   * Clear all undo entries.
   */
  clearUndoEntries() {
    if (this.undoPosition < this.length)
      this.splice(this.undoPosition);
  },

  /**
   * Clear all redo entries.
   */
  clearRedoEntries() {
    if (this.undoPosition > 0) {
      this.splice(0, this.undoPosition);
      this._undoPosition = 0;
    }
  },

  /**
   * Clear all entries.
   */
  clearAllEntries() {
    if (this.length > 0) {
      this.splice(0);
      this._undoPosition = 0;
    }
  }
};


var PlacesTransactions = {
  /**
   * @see Batches in the module documentation.
   */
  batch(aToBatch) {
    let batchFunc;
    if (Array.isArray(aToBatch)) {
      if (aToBatch.length == 0)
        throw new Error("aToBatch must not be an empty array");

      if (aToBatch.some(
           o => !TransactionsHistory.isProxifiedTransactionObject(o))) {
        throw new Error("aToBatch contains non-transaction element");
      }
      return TransactionsManager.batch(function* () {
        for (let txn of aToBatch) {
          try {
            yield txn.transact();
          }
          catch (ex) {
            console.error(ex);
          }
        }
      });
    }
    if (typeof(aToBatch) == "function") {
      return TransactionsManager.batch(aToBatch);
    }

    throw new Error("aToBatch must be either a function or a transactions array");
  },

  /**
   * Asynchronously undo the transaction immediately after the current undo
   * position in the transactions history in the reverse order, if any, and
   * adjusts the undo position.
   *
   * @return {Promises).  The promise always resolves.
   * @note All undo manager operations are queued. This means that transactions
   * history may change by the time your request is fulfilled.
   */
  undo() {
    return TransactionsManager.undo();
  },

  /**
   * Asynchronously redo the transaction immediately before the current undo
   * position in the transactions history, if any, and adjusts the undo
   * position.
   *
   * @return {Promises).  The promise always resolves.
   * @note All undo manager operations are queued. This means that transactions
   * history may change by the time your request is fulfilled.
   */
  redo() {
    return TransactionsManager.redo();
  },

  /**
   * Asynchronously clear the undo, redo, or all entries from the transactions
   * history.
   *
   * @param [optional] aUndoEntries
   *        Whether or not to clear undo entries.  Default: true.
   * @param [optional] aRedoEntries
   *        Whether or not to clear undo entries.  Default: true.
   *
   * @return {Promises).  The promise always resolves.
   * @throws if both aUndoEntries and aRedoEntries are false.
   * @note All undo manager operations are queued. This means that transactions
   * history may change by the time your request is fulfilled.
   */
  clearTransactionsHistory(aUndoEntries = true, aRedoEntries = true) {
    return TransactionsManager.clearTransactionsHistory(aUndoEntries, aRedoEntries);
  },

  /**
   * The numbers of entries in the transactions history.
   */
  get length() {
    return TransactionsHistory.length;
  },

  /**
   * Get the transaction history entry at a given index.  Each entry consists
   * of one or more transaction objects.
   *
   * @param aIndex
   *        the index of the entry to retrieve.
   * @return an array of transaction objects in their undo order (that is,
   * reversely to the order they were executed).
   * @throw if aIndex is invalid (< 0 or >= length).
   * @note the returned array is a clone of the history entry and is not
   * kept in sync with the original entry if it changes.
   */
  entry(aIndex) {
    if (!Number.isInteger(aIndex) || aIndex < 0 || aIndex >= this.length)
      throw new Error("Invalid index");

    return TransactionsHistory[aIndex];
  },

  /**
   * The index of the top undo entry in the transactions history.
   * If there are no undo entries, it equals to |length|.
   * Entries past this point
   * Entries at and past this point are redo entries.
   */
  get undoPosition() {
    return TransactionsHistory.undoPosition;
  },

  /**
   * Shortcut for accessing the top undo entry in the transaction history.
   */
  get topUndoEntry() {
    return TransactionsHistory.topUndoEntry;
  },

  /**
   * Shortcut for accessing the top redo entry in the transaction history.
   */
  get topRedoEntry() {
    return TransactionsHistory.topRedoEntry;
  }
};

/**
 * Helper for serializing the calls to TransactionsManager methods. It allows
 * us to guarantee that the order in which TransactionsManager asynchronous
 * methods are called also enforces the order in which they're executed, and
 * that they are never executed in parallel.
 *
 * In other words: Enqueuer.enqueue(aFunc1); Enqueuer.enqueue(aFunc2) is roughly
 * the same as Task.spawn(aFunc1).then(Task.spawn(aFunc2)).
 */
function Enqueuer() {
  this._promise = Promise.resolve();
}
Enqueuer.prototype = {
  /**
   * Spawn a functions once all previous functions enqueued are done running,
   * and all promises passed to alsoWaitFor are no longer pending.
   *
   * @param   aFunc
   *          @see Task.spawn.
   * @return  a promise that resolves once aFunc is done running. The promise
   *          "mirrors" the promise returned by aFunc.
   */
  enqueue(aFunc) {
    let promise = this._promise.then(Task.async(aFunc));

    // Propagate exceptions to the caller, but dismiss them internally.
    this._promise = promise.catch(console.error);
    return promise;
  },

  /**
   * Same as above, but for a promise returned by a function that already run.
   * This is useful, for example, for serializing transact calls with undo calls,
   * even though transact has its own Enqueuer.
   *
   * @param aPromise
   *        any promise.
   */
  alsoWaitFor(aPromise) {
    // We don't care if aPromise resolves or rejects, but just that is not
    // pending anymore.
    let promise = aPromise.catch(console.error);
    this._promise = Promise.all([this._promise, promise]);
  },

  /**
   * The promise for this queue.
   */
  get promise() {
    return this._promise;
  }
};

var TransactionsManager = {
  // See the documentation at the top of this file. |transact| calls are not
  // serialized with |batch| calls.
  _mainEnqueuer: new Enqueuer(),
  _transactEnqueuer: new Enqueuer(),

  // Is a batch in progress? set when we enter a batch function and unset when
  // it's execution is done.
  _batching: false,

  // If a batch started, this indicates if we've already created an entry in the
  // transactions history for the batch (i.e. if at least one transaction was
  // executed successfully).
  _createdBatchEntry: false,

  // Transactions object should never be recycled (that is, |execute| should
  // only be called once (or not at all) after they're constructed.
  // This keeps track of all transactions which were executed.
  _executedTransactions: new WeakSet(),

  transact(aTxnProxy) {
    let rawTxn = TransactionsHistory.getRawTransaction(aTxnProxy);
    if (!rawTxn)
      throw new Error("|transact| was called with an unexpected object");

    if (this._executedTransactions.has(rawTxn))
      throw new Error("Transactions objects may not be recycled.");

    // Add it in advance so one doesn't accidentally do
    // sameTxn.transact(); sameTxn.transact();
    this._executedTransactions.add(rawTxn);

    let promise = this._transactEnqueuer.enqueue(function* () {
      // Don't try to catch exceptions. If execute fails, we better not add the
      // transaction to the undo stack.
      let retval = yield rawTxn.execute();

      let forceNewEntry = !this._batching || !this._createdBatchEntry;
      TransactionsHistory.add(aTxnProxy, forceNewEntry);
      if (this._batching)
        this._createdBatchEntry = true;

      this._updateCommandsOnActiveWindow();
      return retval;
    }.bind(this));
    this._mainEnqueuer.alsoWaitFor(promise);
    return promise;
  },

  batch(aTask) {
    return this._mainEnqueuer.enqueue(function* () {
      this._batching = true;
      this._createdBatchEntry = false;
      let rv;
      try {
        // We should return here, but bug 958949 makes that impossible.
        rv = (yield Task.spawn(aTask));
      }
      finally {
        this._batching = false;
        this._createdBatchEntry = false;
      }
      return rv;
    }.bind(this));
  },

  /**
   * Undo the top undo entry, if any, and update the undo position accordingly.
   */
  undo() {
    let promise = this._mainEnqueuer.enqueue(function* () {
      let entry = TransactionsHistory.topUndoEntry;
      if (!entry)
        return;

      for (let txnProxy of entry) {
        try {
          yield TransactionsHistory.getRawTransaction(txnProxy).undo();
        }
        catch (ex) {
          // If one transaction is broken, it's not safe to work with any other
          // undo entry.  Report the error and clear the undo history.
          console.error(ex,
                        "Couldn't undo a transaction, clearing all undo entries.");
          TransactionsHistory.clearUndoEntries();
          return;
        }
      }
      TransactionsHistory._undoPosition++;
      this._updateCommandsOnActiveWindow();
    }.bind(this));
    this._transactEnqueuer.alsoWaitFor(promise);
    return promise;
  },

  /**
   * Redo the top redo entry, if any, and update the undo position accordingly.
   */
  redo() {
    let promise = this._mainEnqueuer.enqueue(function* () {
      let entry = TransactionsHistory.topRedoEntry;
      if (!entry)
        return;

      for (let i = entry.length - 1; i >= 0; i--) {
        let transaction = TransactionsHistory.getRawTransaction(entry[i]);
        try {
          if (transaction.redo)
            yield transaction.redo();
          else
            yield transaction.execute();
        }
        catch (ex) {
          // If one transaction is broken, it's not safe to work with any other
          // redo entry. Report the error and clear the undo history.
          console.error(ex,
                        "Couldn't redo a transaction, clearing all redo entries.");
          TransactionsHistory.clearRedoEntries();
          return;
        }
      }
      TransactionsHistory._undoPosition--;
      this._updateCommandsOnActiveWindow();
    }.bind(this));

    this._transactEnqueuer.alsoWaitFor(promise);
    return promise;
  },

  clearTransactionsHistory(aUndoEntries, aRedoEntries) {
    let promise = this._mainEnqueuer.enqueue(function* () {
      if (aUndoEntries && aRedoEntries)
        TransactionsHistory.clearAllEntries();
      else if (aUndoEntries)
        TransactionsHistory.clearUndoEntries();
      else if (aRedoEntries)
        TransactionsHistory.clearRedoEntries();
      else
        throw new Error("either aUndoEntries or aRedoEntries should be true");
    }.bind(this));

    this._transactEnqueuer.alsoWaitFor(promise);
    return promise;
  },

  // Updates commands in the undo group of the active window commands.
  // Inactive windows commands will be updated on focus.
  _updateCommandsOnActiveWindow() {
    // Updating "undo" will cause a group update including "redo".
    try {
      let win = Services.focus.activeWindow;
      if (win)
        win.updateCommands("undo");
    }
    catch (ex) { console.error(ex, "Couldn't update undo commands"); }
  }
};

/**
 * Internal helper for defining the standard transactions and their input.
 * It takes the required and optional properties, and generates the public
 * constructor (which takes the input in the form of a plain object) which,
 * when called, creates the argument-less "public" |execute| method by binding
 * the input properties to the function arguments (required properties first,
 * then the optional properties).
 *
 * If this seems confusing, look at the consumers.
 *
 * This magic serves two purposes:
 * (1) It completely hides the transactions' internals from the module
 *     consumers.
 * (2) It keeps each transaction implementation to what is about, bypassing
 *     all this bureaucracy while still validating input appropriately.
 */
function DefineTransaction(aRequiredProps = [], aOptionalProps = []) {
  for (let prop of [...aRequiredProps, ...aOptionalProps]) {
    if (!DefineTransaction.inputProps.has(prop))
      throw new Error("Property '" + prop + "' is not defined");
  }

  let ctor = function (aInput) {
    // We want to support both syntaxes:
    // let t = new PlacesTransactions.NewBookmark(),
    // let t = PlacesTransactions.NewBookmark()
    if (this == PlacesTransactions)
      return new ctor(aInput);

    if (aRequiredProps.length > 0 || aOptionalProps.length > 0) {
      // Bind the input properties to the arguments of execute.
      let input = DefineTransaction.verifyInput(aInput, aRequiredProps,
                                                aOptionalProps);
      let executeArgs = [this,
                         ...aRequiredProps.map(prop => input[prop]),
                         ...aOptionalProps.map(prop => input[prop])];
      this.execute = Function.bind.apply(this.execute, executeArgs);
    }
    return TransactionsHistory.proxifyTransaction(this);
  };
  return ctor;
}

function simpleValidateFunc(aCheck) {
  return v => {
    if (!aCheck(v))
      throw new Error("Invalid value");
    return v;
  };
}

DefineTransaction.strValidate = simpleValidateFunc(v => typeof(v) == "string");
DefineTransaction.strOrNullValidate =
  simpleValidateFunc(v => typeof(v) == "string" || v === null);
DefineTransaction.indexValidate =
  simpleValidateFunc(v => Number.isInteger(v) &&
                          v >= PlacesUtils.bookmarks.DEFAULT_INDEX);
DefineTransaction.guidValidate =
  simpleValidateFunc(v => /^[a-zA-Z0-9\-_]{12}$/.test(v));

function isPrimitive(v) {
  return v === null || (typeof(v) != "object" && typeof(v) != "function");
}

DefineTransaction.annotationObjectValidate = function (obj) {
  let checkProperty = (aPropName, aRequired, aCheckFunc) => {
    if (aPropName in obj)
      return aCheckFunc(obj[aPropName]);

    return !aRequired;
  };

  if (obj &&
      checkProperty("name", true, v => typeof(v) == "string" && v.length > 0) &&
      checkProperty("expires", false, Number.isInteger) &&
      checkProperty("flags", false, Number.isInteger) &&
      checkProperty("value", false, isPrimitive) ) {
    // Nothing else should be set
    let validKeys = ["name", "value", "flags", "expires"];
    if (Object.keys(obj).every( (k) => validKeys.includes(k)))
      return obj;
  }
  throw new Error("Invalid annotation object");
};

DefineTransaction.urlValidate = function(url) {
  // When this module is updated to use Bookmarks.jsm, we should actually
  // convert nsIURIs/spec to URL objects.
  if (url instanceof Components.interfaces.nsIURI)
    return url;
  let spec = url instanceof URL ? url.href : url;
  return NetUtil.newURI(spec);
};

DefineTransaction.inputProps = new Map();
DefineTransaction.defineInputProps =
function (aNames, aValidationFunction, aDefaultValue) {
  for (let name of aNames) {
    // Workaround bug 449811.
    let propName = name;
    this.inputProps.set(propName, {
      validateValue: function (aValue) {
        if (aValue === undefined)
          return aDefaultValue;
        try {
          return aValidationFunction(aValue);
        }
        catch (ex) {
          throw new Error(`Invalid value for input property ${propName}`);
        }
      },

      validateInput: function (aInput, aRequired) {
        if (aRequired && !(propName in aInput))
          throw new Error(`Required input property is missing: ${propName}`);
        return this.validateValue(aInput[propName]);
      },

      isArrayProperty: false
    });
  }
};

DefineTransaction.defineArrayInputProp =
function (aName, aBasePropertyName) {
  let baseProp = this.inputProps.get(aBasePropertyName);
  if (!baseProp)
    throw new Error(`Unknown input property: ${aBasePropertyName}`);

  this.inputProps.set(aName, {
    validateValue: function (aValue) {
      if (aValue == undefined)
        return [];

      if (!Array.isArray(aValue))
        throw new Error(`${aName} input property value must be an array`);

      // This also takes care of abandoning the global scope of the input
      // array (through Array.prototype).
      return aValue.map(baseProp.validateValue);
    },

    // We allow setting either the array property itself (e.g. urls), or a
    // single element of it (url, in that example), that is then transformed
    // into a single-element array.
    validateInput: function (aInput, aRequired) {
      if (aName in aInput) {
        // It's not allowed to set both though.
        if (aBasePropertyName in aInput) {
          throw new Error(`It is not allowed to set both ${aName} and
                          ${aBasePropertyName} as  input properties`);
        }
        let array = this.validateValue(aInput[aName]);
        if (aRequired && array.length == 0) {
          throw new Error(`Empty array passed for required input property:
                           ${aName}`);
        }
        return array;
      }
      // If the property is required and it's not set as is, check if the base
      // property is set.
      if (aRequired && !(aBasePropertyName in aInput))
        throw new Error(`Required input property is missing: ${aName}`);

      if (aBasePropertyName in aInput)
        return [baseProp.validateValue(aInput[aBasePropertyName])];

      return [];
    },

    isArrayProperty: true
  });
};

DefineTransaction.validatePropertyValue =
function (aProp, aInput, aRequired) {
  return this.inputProps.get(aProp).validateInput(aInput, aRequired);
};

DefineTransaction.getInputObjectForSingleValue =
function (aInput, aRequiredProps, aOptionalProps) {
  // The following input forms may be deduced from a single value:
  // * a single required property with or without optional properties (the given
  //   value is set to the required property).
  // * a single optional property with no required properties.
  if (aRequiredProps.length > 1 ||
      (aRequiredProps.length == 0 && aOptionalProps.length > 1)) {
    throw new Error("Transaction input isn't an object");
  }

  let propName = aRequiredProps.length == 1 ?
                 aRequiredProps[0] : aOptionalProps[0];
  let propValue =
    this.inputProps.get(propName).isArrayProperty && !Array.isArray(aInput) ?
    [aInput] : aInput;
  return { [propName]: propValue };
};

DefineTransaction.verifyInput =
function (aInput, aRequiredProps = [], aOptionalProps = []) {
  if (aRequiredProps.length == 0 && aOptionalProps.length == 0)
    return {};

  // If there's just a single required/optional property, we allow passing it
  // as is, so, for example, one could do PlacesTransactions.RemoveItem(myGuid)
  // rather than PlacesTransactions.RemoveItem({ guid: myGuid}).
  // This shortcut isn't supported for "complex" properties - e.g. one cannot
  // pass an annotation object this way (note there is no use case for this at
  // the moment anyway).
  let input = aInput;
  let isSinglePropertyInput =
    isPrimitive(aInput) ||
    Array.isArray(aInput) ||
    (aInput instanceof Components.interfaces.nsISupports);
  if (isSinglePropertyInput) {
    input =  this.getInputObjectForSingleValue(aInput,
                                               aRequiredProps,
                                               aOptionalProps);
  }

  let fixedInput = { };
  for (let prop of aRequiredProps) {
    fixedInput[prop] = this.validatePropertyValue(prop, input, true);
  }
  for (let prop of aOptionalProps) {
    fixedInput[prop] = this.validatePropertyValue(prop, input, false);
  }

  return fixedInput;
};

// Update the documentation at the top of this module if you add or
// remove properties.
DefineTransaction.defineInputProps(["url", "feedUrl", "siteUrl"],
                                   DefineTransaction.urlValidate, null);
DefineTransaction.defineInputProps(["guid", "parentGuid", "newParentGuid"],
                                   DefineTransaction.guidValidate);
DefineTransaction.defineInputProps(["title"],
                                   DefineTransaction.strOrNullValidate, null);
DefineTransaction.defineInputProps(["keyword", "postData", "tag",
                                    "excludingAnnotation"],
                                   DefineTransaction.strValidate, "");
DefineTransaction.defineInputProps(["index", "newIndex"],
                                   DefineTransaction.indexValidate,
                                   PlacesUtils.bookmarks.DEFAULT_INDEX);
DefineTransaction.defineInputProps(["annotation"],
                                   DefineTransaction.annotationObjectValidate);
DefineTransaction.defineArrayInputProp("guids", "guid");
DefineTransaction.defineArrayInputProp("urls", "url");
DefineTransaction.defineArrayInputProp("tags", "tag");
DefineTransaction.defineArrayInputProp("annotations", "annotation");
DefineTransaction.defineArrayInputProp("excludingAnnotations",
                                       "excludingAnnotation");

/**
 * Internal helper for implementing the execute method of NewBookmark, NewFolder
 * and NewSeparator.
 *
 * @param aTransaction
 *        The transaction object
 * @param aParentGuid
 *        The GUID of the parent folder
 * @param aCreateItemFunction(aParentId, aGuidToRestore)
 *        The function to be called for creating the item on execute and redo.
 *        It should return the itemId for the new item
 *        - aGuidToRestore - the GUID to set for the item (used for redo).
 * @param [optional] aOnUndo
 *        an additional function to call after undo
 * @param [optional] aOnRedo
 *        an additional function to call after redo
 */
function* ExecuteCreateItem(aTransaction, aParentGuid, aCreateItemFunction,
                            aOnUndo = null, aOnRedo = null) {
  let parentId = yield PlacesUtils.promiseItemId(aParentGuid),
      itemId = yield aCreateItemFunction(parentId, ""),
      guid = yield PlacesUtils.promiseItemGuid(itemId);

  // On redo, we'll restore the date-added and last-modified properties.
  let dateAdded = 0, lastModified = 0;
  aTransaction.undo = function* () {
    if (dateAdded == 0) {
      dateAdded = PlacesUtils.bookmarks.getItemDateAdded(itemId);
      lastModified = PlacesUtils.bookmarks.getItemLastModified(itemId);
    }
    PlacesUtils.bookmarks.removeItem(itemId);
    if (aOnUndo) {
      yield aOnUndo();
    }
  };
  aTransaction.redo = function* () {
    parentId = yield PlacesUtils.promiseItemId(aParentGuid);
    itemId = yield aCreateItemFunction(parentId, guid);
    if (aOnRedo)
      yield aOnRedo();

    // aOnRedo is called first to make sure it doesn't override
    // lastModified.
    PlacesUtils.bookmarks.setItemDateAdded(itemId, dateAdded);
    PlacesUtils.bookmarks.setItemLastModified(itemId, lastModified);
    PlacesUtils.bookmarks.setItemLastModified(parentId, dateAdded);
  };
  return guid;
}

/**
 * Creates items (all types) from a bookmarks tree representation, as defined
 * in PlacesUtils.promiseBookmarksTree.
 *
 * @param aBookmarksTree
 *        the bookmarks tree object.  You may pass either a bookmarks tree
 *        returned by promiseBookmarksTree, or a manually defined one.
 * @param [optional] aRestoring (default: false)
 *        Whether or not the items are restored.  Only in restore mode, are
 *        the guid, dateAdded and lastModified properties honored.
 * @param [optional] aExcludingAnnotations
 *        Array of annotations names to ignore in aBookmarksTree. This argument
 *        is ignored if aRestoring is set.
 * @note the id, root and charset properties of items in aBookmarksTree are
 *       always ignored.  The index property is ignored for all items but the
 *       root one.
 * @return {Promise}
 */
function* createItemsFromBookmarksTree(aBookmarksTree, aRestoring = false,
                                       aExcludingAnnotations = []) {
  function extractLivemarkDetails(aAnnos) {
    let feedURI = null, siteURI = null;
    aAnnos = aAnnos.filter(
      aAnno => {
        switch (aAnno.name) {
        case PlacesUtils.LMANNO_FEEDURI:
          feedURI = NetUtil.newURI(aAnno.value);
          return false;
        case PlacesUtils.LMANNO_SITEURI:
          siteURI = NetUtil.newURI(aAnno.value);
          return false;
        default:
          return true;
        }
      } );
    return [feedURI, siteURI];
  }

  function* createItem(aItem,
                       aParentGuid,
                       aIndex = PlacesUtils.bookmarks.DEFAULT_INDEX) {
    let itemId;
    let guid = aRestoring ? aItem.guid : undefined;
    let parentId = yield PlacesUtils.promiseItemId(aParentGuid);
    let annos = aItem.annos ? [...aItem.annos] : [];
    switch (aItem.type) {
      case PlacesUtils.TYPE_X_MOZ_PLACE: {
        let uri = NetUtil.newURI(aItem.uri);
        itemId = PlacesUtils.bookmarks.insertBookmark(
          parentId, uri, aIndex, aItem.title, guid);
        if ("keyword" in aItem)
          PlacesUtils.bookmarks.setKeywordForBookmark(itemId, aItem.keyword);
        if ("tags" in aItem) {
          PlacesUtils.tagging.tagURI(uri, aItem.tags.split(","));
        }
        break;
      }
      case PlacesUtils.TYPE_X_MOZ_PLACE_CONTAINER: {
        // Either a folder or a livemark
        let [feedURI, siteURI] = extractLivemarkDetails(annos);
        if (!feedURI) {
          itemId = PlacesUtils.bookmarks.createFolder(
              parentId, aItem.title, aIndex, guid);
          if (guid === undefined)
            guid = yield PlacesUtils.promiseItemGuid(itemId);
          if ("children" in aItem) {
            for (let child of aItem.children) {
              yield createItem(child, guid);
            }
          }
        }
        else {
          let livemark =
            yield PlacesUtils.livemarks.addLivemark({ title: aItem.title
                                                    , feedURI: feedURI
                                                    , siteURI: siteURI
                                                    , parentId: parentId
                                                    , index: aIndex
                                                    , guid: guid});
          itemId = livemark.id;
        }
        break;
      }
      case PlacesUtils.TYPE_X_MOZ_PLACE_SEPARATOR: {
        itemId = PlacesUtils.bookmarks.insertSeparator(parentId, aIndex, guid);
        break;
      }
    }
    if (annos.length > 0) {
      if (!aRestoring && aExcludingAnnotations.length > 0) {
        annos = annos.filter(a => !aExcludingAnnotations.includes(a.name));

      }

      PlacesUtils.setAnnotationsForItem(itemId, annos);
    }

    if (aRestoring) {
      if ("dateAdded" in aItem)
        PlacesUtils.bookmarks.setItemDateAdded(itemId, aItem.dateAdded);
      if ("lastModified" in aItem)
        PlacesUtils.bookmarks.setItemLastModified(itemId, aItem.lastModified);
    }
    return itemId;
  }
  return yield createItem(aBookmarksTree,
                          aBookmarksTree.parentGuid,
                          aBookmarksTree.index);
}

/*****************************************************************************
 * The Standard Places Transactions.
 *
 * See the documentation at the top of this file. The valid values for input
 * are also documented there.
 *****************************************************************************/

var PT = PlacesTransactions;

/**
 * Transaction for creating a bookmark.
 *
 * Required Input Properties: url, parentGuid.
 * Optional Input Properties: index, title, keyword, annotations, tags.
 *
 * When this transaction is executed, it's resolved to the new bookmark's GUID.
 */
PT.NewBookmark = DefineTransaction(["parentGuid", "url"],
                                   ["index", "title", "keyword", "postData",
                                    "annotations", "tags"]);
PT.NewBookmark.prototype = Object.seal({
  execute: function (aParentGuid, aURI, aIndex, aTitle,
                     aKeyword, aPostData, aAnnos, aTags) {
    return ExecuteCreateItem(this, aParentGuid,
      function (parentId, guidToRestore = "") {
        let itemId = PlacesUtils.bookmarks.insertBookmark(
          parentId, aURI, aIndex, aTitle, guidToRestore);
        if (aKeyword)
          PlacesUtils.bookmarks.setKeywordForBookmark(itemId, aKeyword);
        if (aPostData)
          PlacesUtils.setPostDataForBookmark(itemId, aPostData);
        if (aAnnos.length)
          PlacesUtils.setAnnotationsForItem(itemId, aAnnos);
        if (aTags.length > 0) {
          let currentTags = PlacesUtils.tagging.getTagsForURI(aURI);
          aTags = aTags.filter(t => !currentTags.includes(t));
          PlacesUtils.tagging.tagURI(aURI, aTags);
        }

        return itemId;
      },
      function _additionalOnUndo() {
        if (aTags.length > 0)
          PlacesUtils.tagging.untagURI(aURI, aTags);
      });
  }
});

/**
 * Transaction for creating a folder.
 *
 * Required Input Properties: title, parentGuid.
 * Optional Input Properties: index, annotations.
 *
 * When this transaction is executed, it's resolved to the new folder's GUID.
 */
PT.NewFolder = DefineTransaction(["parentGuid", "title"],
                                 ["index", "annotations"]);
PT.NewFolder.prototype = Object.seal({
  execute: function (aParentGuid, aTitle, aIndex, aAnnos) {
    return ExecuteCreateItem(this,  aParentGuid,
      function(parentId, guidToRestore = "") {
        let itemId = PlacesUtils.bookmarks.createFolder(
          parentId, aTitle, aIndex, guidToRestore);
        if (aAnnos.length > 0)
          PlacesUtils.setAnnotationsForItem(itemId, aAnnos);
        return itemId;
      });
  }
});

/**
 * Transaction for creating a separator.
 *
 * Required Input Properties: parentGuid.
 * Optional Input Properties: index.
 *
 * When this transaction is executed, it's resolved to the new separator's
 * GUID.
 */
PT.NewSeparator = DefineTransaction(["parentGuid"], ["index"]);
PT.NewSeparator.prototype = Object.seal({
  execute: function (aParentGuid, aIndex) {
    return ExecuteCreateItem(this, aParentGuid,
      function (parentId, guidToRestore = "") {
        let itemId = PlacesUtils.bookmarks.insertSeparator(
          parentId, aIndex, guidToRestore);
        return itemId;
      });
  }
});

/**
 * Transaction for creating a live bookmark (see mozIAsyncLivemarks for the
 * semantics).
 *
 * Required Input Properties: feedUrl, title, parentGuid.
 * Optional Input Properties: siteUrl, index, annotations.
 *
 * When this transaction is executed, it's resolved to the new livemark's
 * GUID.
 */
PT.NewLivemark = DefineTransaction(["feedUrl", "title", "parentGuid"],
                                   ["siteUrl", "index", "annotations"]);
PT.NewLivemark.prototype = Object.seal({
  execute: function* (aFeedURI, aTitle, aParentGuid, aSiteURI, aIndex, aAnnos) {
    let livemarkInfo = { title: aTitle
                       , feedURI: aFeedURI
                       , siteURI: aSiteURI
                       , index: aIndex };
    let createItem = function* () {
      livemarkInfo.parentId = yield PlacesUtils.promiseItemId(aParentGuid);
      let livemark = yield PlacesUtils.livemarks.addLivemark(livemarkInfo);
      if (aAnnos.length > 0)
        PlacesUtils.setAnnotationsForItem(livemark.id, aAnnos);

      if ("dateAdded" in livemarkInfo) {
        PlacesUtils.bookmarks.setItemDateAdded(livemark.id,
                                               livemarkInfo.dateAdded);
        PlacesUtils.bookmarks.setItemLastModified(livemark.id,
                                                  livemarkInfo.lastModified);
      }
      return livemark;
    };

    let livemark = yield createItem();
    this.undo = function* () {
      livemarkInfo.guid = livemark.guid;
      if (!("dateAdded" in livemarkInfo)) {
        livemarkInfo.dateAdded =
          PlacesUtils.bookmarks.getItemDateAdded(livemark.id);
        livemarkInfo.lastModified =
          PlacesUtils.bookmarks.getItemLastModified(livemark.id);
      }
      yield PlacesUtils.livemarks.removeLivemark(livemark);
    };
    this.redo = function* () {
      livemark = yield createItem();
    };
    return livemark.guid;
  }
});

/**
 * Transaction for moving an item.
 *
 * Required Input Properties: guid, newParentGuid.
 * Optional Input Properties  newIndex.
 */
PT.Move = DefineTransaction(["guid", "newParentGuid"], ["newIndex"]);
PT.Move.prototype = Object.seal({
  execute: function* (aGuid, aNewParentGuid, aNewIndex) {
    let itemId = yield PlacesUtils.promiseItemId(aGuid),
        oldParentId = PlacesUtils.bookmarks.getFolderIdForItem(itemId),
        oldIndex = PlacesUtils.bookmarks.getItemIndex(itemId),
        newParentId = yield PlacesUtils.promiseItemId(aNewParentGuid);

    PlacesUtils.bookmarks.moveItem(itemId, newParentId, aNewIndex);

    let undoIndex = PlacesUtils.bookmarks.getItemIndex(itemId);
    this.undo = () => {
      // Moving down in the same parent takes in count removal of the item
      // so to revert positions we must move to oldIndex + 1
      if (newParentId == oldParentId && oldIndex > undoIndex)
        PlacesUtils.bookmarks.moveItem(itemId, oldParentId, oldIndex + 1);
      else
        PlacesUtils.bookmarks.moveItem(itemId, oldParentId, oldIndex);
    };
  }
});

/**
 * Transaction for setting the title for an item.
 *
 * Required Input Properties: guid, title.
 */
PT.EditTitle = DefineTransaction(["guid", "title"]);
PT.EditTitle.prototype = Object.seal({
  execute: function* (aGuid, aTitle) {
    let itemId = yield PlacesUtils.promiseItemId(aGuid),
        oldTitle = PlacesUtils.bookmarks.getItemTitle(itemId);
    PlacesUtils.bookmarks.setItemTitle(itemId, aTitle);
    this.undo = () => { PlacesUtils.bookmarks.setItemTitle(itemId, oldTitle); };
  }
});

/**
 * Transaction for setting the URI for an item.
 *
 * Required Input Properties: guid, url.
 */
PT.EditUrl = DefineTransaction(["guid", "url"]);
PT.EditUrl.prototype = Object.seal({
  execute: function* (aGuid, aURI) {
    let itemId = yield PlacesUtils.promiseItemId(aGuid),
        oldURI = PlacesUtils.bookmarks.getBookmarkURI(itemId),
        oldURITags = PlacesUtils.tagging.getTagsForURI(oldURI),
        newURIAdditionalTags = null;
    PlacesUtils.bookmarks.changeBookmarkURI(itemId, aURI);

    // Move tags from old URI to new URI.
    if (oldURITags.length > 0) {
      // Only untag the old URI if this is the only bookmark.
      if (PlacesUtils.getBookmarksForURI(oldURI, {}).length == 0)
        PlacesUtils.tagging.untagURI(oldURI, oldURITags);

      let currentNewURITags = PlacesUtils.tagging.getTagsForURI(aURI);
      newURIAdditionalTags = oldURITags.filter(t => !currentNewURITags.includes(t));
      if (newURIAdditionalTags)
        PlacesUtils.tagging.tagURI(aURI, newURIAdditionalTags);
    }

    this.undo = () => {
      PlacesUtils.bookmarks.changeBookmarkURI(itemId, oldURI);
      // Move tags from new URI to old URI.
      if (oldURITags.length > 0) {
        // Only untag the new URI if this is the only bookmark.
        if (newURIAdditionalTags && newURIAdditionalTags.length > 0 &&
            PlacesUtils.getBookmarksForURI(aURI, {}).length == 0) {
          PlacesUtils.tagging.untagURI(aURI, newURIAdditionalTags);
        }

        PlacesUtils.tagging.tagURI(oldURI, oldURITags);
      }
    };
  }
});

/**
 * Transaction for setting annotations for an item.
 *
 * Required Input Properties: guid, annotationObject
 */
PT.Annotate = DefineTransaction(["guids", "annotations"]);
PT.Annotate.prototype = {
  *execute(aGuids, aNewAnnos) {
    let undoAnnosForItem = new Map(); // itemId => undoAnnos;
    for (let guid of aGuids) {
      let itemId = yield PlacesUtils.promiseItemId(guid);
      let currentAnnos = PlacesUtils.getAnnotationsForItem(itemId);

      let undoAnnos = [];
      for (let newAnno of aNewAnnos) {
        let currentAnno = currentAnnos.find(a => a.name == newAnno.name);
        if (!!currentAnno) {
          undoAnnos.push(currentAnno);
        }
        else {
          // An unset value removes the annotation.
          undoAnnos.push({ name: newAnno.name });
        }
      }
      undoAnnosForItem.set(itemId, undoAnnos);

      PlacesUtils.setAnnotationsForItem(itemId, aNewAnnos);
    }

    this.undo = function() {
      for (let [itemId, undoAnnos] of undoAnnosForItem) {
        PlacesUtils.setAnnotationsForItem(itemId, undoAnnos);
      }
    };
    this.redo = function* () {
      for (let guid of aGuids) {
        let itemId = yield PlacesUtils.promiseItemId(guid);
        PlacesUtils.setAnnotationsForItem(itemId, aNewAnnos);
      }
    };
  }
};

/**
 * Transaction for setting the keyword for a bookmark.
 *
 * Required Input Properties: guid, keyword.
 */
PT.EditKeyword = DefineTransaction(["guid", "keyword"]);
PT.EditKeyword.prototype = Object.seal({
  execute: function* (aGuid, aKeyword) {
    let itemId = yield PlacesUtils.promiseItemId(aGuid),
        oldKeyword = PlacesUtils.bookmarks.getKeywordForBookmark(itemId);
    PlacesUtils.bookmarks.setKeywordForBookmark(itemId, aKeyword);
    this.undo = () => {
      PlacesUtils.bookmarks.setKeywordForBookmark(itemId, oldKeyword);
    };
  }
});

/**
 * Transaction for sorting a folder by name.
 *
 * Required Input Properties: guid.
 */
PT.SortByName = DefineTransaction(["guid"]);
PT.SortByName.prototype = {
  execute: function* (aGuid) {
    let itemId = yield PlacesUtils.promiseItemId(aGuid),
        oldOrder = [],  // [itemId] = old index
        contents = PlacesUtils.getFolderContents(itemId, false, false).root,
        count = contents.childCount;

    // Sort between separators.
    let newOrder = [], // nodes, in the new order.
        preSep   = []; // Temporary array for sorting each group of nodes.
    let sortingMethod = (a, b) => {
      if (PlacesUtils.nodeIsContainer(a) && !PlacesUtils.nodeIsContainer(b))
        return -1;
      if (!PlacesUtils.nodeIsContainer(a) && PlacesUtils.nodeIsContainer(b))
        return 1;
      return a.title.localeCompare(b.title);
    };

    for (let i = 0; i < count; ++i) {
      let node = contents.getChild(i);
      oldOrder[node.itemId] = i;
      if (PlacesUtils.nodeIsSeparator(node)) {
        if (preSep.length > 0) {
          preSep.sort(sortingMethod);
          newOrder = newOrder.concat(preSep);
          preSep.splice(0, preSep.length);
        }
        newOrder.push(node);
      }
      else
        preSep.push(node);
    }
    contents.containerOpen = false;

    if (preSep.length > 0) {
      preSep.sort(sortingMethod);
      newOrder = newOrder.concat(preSep);
    }

    // Set the nex indexes.
    let callback = {
      runBatched: function() {
        for (let i = 0; i < newOrder.length; ++i) {
          PlacesUtils.bookmarks.setItemIndex(newOrder[i].itemId, i);
        }
      }
    };
    PlacesUtils.bookmarks.runInBatchMode(callback, null);

    this.undo = () => {
      let callback = {
        runBatched: function() {
          for (let item in oldOrder) {
            PlacesUtils.bookmarks.setItemIndex(item, oldOrder[item]);
          }
        }
      };
      PlacesUtils.bookmarks.runInBatchMode(callback, null);
    };
  }
};

/**
 * Transaction for removing an item (any type).
 *
 * Required Input Properties: guids.
 */
PT.Remove = DefineTransaction(["guids"]);
PT.Remove.prototype = {
  *execute(aGuids) {
    function promiseBookmarksTree(guid) {
      try {
        return PlacesUtils.promiseBookmarksTree(guid);
      }
      catch (ex) {
        throw new Error("Failed to get info for the specified item (guid: " +
                        guid + "). Ex: " + ex);
      }
    }

    let toRestore = [];
    for (let guid of aGuids) {
      toRestore.push(yield promiseBookmarksTree(guid));
    }

    let removeThem = Task.async(function* () {
      for (let guid of aGuids) {
        PlacesUtils.bookmarks.removeItem(yield PlacesUtils.promiseItemId(guid));
      }
    });
    yield removeThem();

    this.undo = Task.async(function* () {
      for (let info of toRestore) {
        yield createItemsFromBookmarksTree(info, true);
      }
    });
    this.redo = removeThem;
  }
};

/**
 * Transactions for removing all bookmarks for one or more urls.
 *
 * Required Input Properties: urls.
 */
PT.RemoveBookmarksForUrls = DefineTransaction(["urls"]);
PT.RemoveBookmarksForUrls.prototype = {
  *execute(aUrls) {
    let guids = [];
    for (let url of aUrls) {
      yield PlacesUtils.bookmarks.fetch({ url }, info => {
        guids.push(info.guid);
      });
    }
    let removeTxn = TransactionsHistory.getRawTransaction(PT.Remove(guids));
    yield removeTxn.execute();
    this.undo = removeTxn.undo.bind(removeTxn);
    this.redo = removeTxn.redo.bind(removeTxn);
  }
};

/**
 * Transaction for tagging urls.
 *
 * Required Input Properties: urls, tags.
 */
PT.Tag = DefineTransaction(["urls", "tags"]);
PT.Tag.prototype = {
  execute: function* (aURIs, aTags) {
    let onUndo = [], onRedo = [];
    for (let uri of aURIs) {
      // Workaround bug 449811.
      let currentURI = uri;

      let promiseIsBookmarked = function* () {
        let deferred = Promise.defer();
        PlacesUtils.asyncGetBookmarkIds(
          currentURI, ids => { deferred.resolve(ids.length > 0); });
        return deferred.promise;
      };

      if (yield promiseIsBookmarked(currentURI)) {
        // Tagging is only allowed for bookmarked URIs (but see 424160).
        let createTxn = TransactionsHistory.getRawTransaction(
          PT.NewBookmark({ url: currentURI
                         , tags: aTags
                         , parentGuid: PlacesUtils.bookmarks.unfiledGuid }));
        yield createTxn.execute();
        onUndo.unshift(createTxn.undo.bind(createTxn));
        onRedo.push(createTxn.redo.bind(createTxn));
      }
      else {
        let currentTags = PlacesUtils.tagging.getTagsForURI(currentURI);
        let newTags = aTags.filter(t => !currentTags.includes(t));
        PlacesUtils.tagging.tagURI(currentURI, newTags);
        onUndo.unshift(() => {
          PlacesUtils.tagging.untagURI(currentURI, newTags);
        });
        onRedo.push(() => {
          PlacesUtils.tagging.tagURI(currentURI, newTags);
        });
      }
    }
    this.undo = function* () {
      for (let f of onUndo) {
        yield f();
      }
    };
    this.redo = function* () {
      for (let f of onRedo) {
        yield f();
      }
    };
  }
};

/**
 * Transaction for removing tags from a URI.
 *
 * Required Input Properties: urls.
 * Optional Input Properties: tags.
 *
 * If |tags| is not set, all tags set for |url| are removed.
 */
PT.Untag = DefineTransaction(["urls"], ["tags"]);
PT.Untag.prototype = {
  execute: function* (aURIs, aTags) {
    let onUndo = [], onRedo = [];
    for (let uri of aURIs) {
      // Workaround bug 449811.
      let currentURI = uri;
      let tagsToRemove;
      let tagsSet = PlacesUtils.tagging.getTagsForURI(currentURI);
      if (aTags.length > 0)
        tagsToRemove = aTags.filter(t => tagsSet.includes(t));
      else
        tagsToRemove = tagsSet;
      PlacesUtils.tagging.untagURI(currentURI, tagsToRemove);
      onUndo.unshift(() => {
        PlacesUtils.tagging.tagURI(currentURI, tagsToRemove);
      });
      onRedo.push(() => {
        PlacesUtils.tagging.untagURI(currentURI, tagsToRemove);
      });
    }
    this.undo = function* () {
      for (let f of onUndo) {
        yield f();
      }
    };
    this.redo = function* () {
      for (let f of onRedo) {
        yield f();
      }
    };
  }
};

/**
 * Transaction for copying an item.
 *
 * Required Input Properties: guid, newParentGuid
 * Optional Input Properties: newIndex, excludingAnnotations.
 */
PT.Copy = DefineTransaction(["guid", "newParentGuid"],
                            ["newIndex", "excludingAnnotations"]);
PT.Copy.prototype = {
  execute: function* (aGuid, aNewParentGuid, aNewIndex, aExcludingAnnotations) {
    let creationInfo = null;
    try {
      creationInfo = yield PlacesUtils.promiseBookmarksTree(aGuid);
    }
    catch (ex) {
      throw new Error("Failed to get info for the specified item (guid: " +
                      aGuid + "). Ex: " + ex);
    }
    creationInfo.parentGuid = aNewParentGuid;
    creationInfo.index = aNewIndex;

    let newItemId =
      yield createItemsFromBookmarksTree(creationInfo, false,
                                         aExcludingAnnotations);
    let newItemInfo = null;
    this.undo = function* () {
      if (!newItemInfo) {
        let newItemGuid = yield PlacesUtils.promiseItemGuid(newItemId);
        newItemInfo = yield PlacesUtils.promiseBookmarksTree(newItemGuid);
      }
      PlacesUtils.bookmarks.removeItem(newItemId);
    };
    this.redo = function* () {
      newItemId = yield createItemsFromBookmarksTree(newItemInfo, true);
    }

    return yield PlacesUtils.promiseItemGuid(newItemId);
  }
};
