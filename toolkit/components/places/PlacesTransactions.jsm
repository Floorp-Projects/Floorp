/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["PlacesTransactions"];

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
 * IT'S PARTICULARLY IMPORTANT NOT TO await ANY PROMISE RETURNED BY ANY OF
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

const TRANSACTIONS_QUEUE_TIMEOUT_MS = 240000; // 4 Mins.

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.defineModuleGetter(this, "PlacesUtils",
                               "resource://gre/modules/PlacesUtils.jsm");

XPCOMUtils.defineLazyGlobalGetters(this, ["URL"]);

function setTimeout(callback, ms) {
  let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  timer.initWithCallback(callback, ms, timer.TYPE_ONE_SHOT);
}

class TransactionsHistoryArray extends Array {
  constructor() {
    super();

    // The index of the first undo entry (if any) - See the documentation
    // at the top of this file.
    this._undoPosition = 0;
    // Outside of this module, the API of transactions is inaccessible, and so
    // are any internal properties.  To achieve that, transactions are proxified
    // in their constructors.  This maps the proxies to their respective raw
    // objects.
    this.proxifiedToRaw = new WeakMap();
  }

  get undoPosition() {
    return this._undoPosition;
  }

  // Handy shortcuts
  get topUndoEntry() {
    return this.undoPosition < this.length ? this[this.undoPosition] : null;
  }
  get topRedoEntry() {
    return this.undoPosition > 0 ? this[this.undoPosition - 1] : null;
  }

  /**
   * Proxify a transaction object for consumers.
   * @param rawTransaction
   *        the raw transaction object.
   * @return the proxified transaction object.
   * @see getRawTransaction for retrieving the raw transaction.
   */
  proxifyTransaction(rawTransaction) {
    let proxy = Object.freeze({
      transact() {
        return TransactionsManager.transact(this);
      }
    });
    this.proxifiedToRaw.set(proxy, rawTransaction);
    return proxy;
  }

  /**
   * Check if the given object is a the proxy object for some transaction.
   * @param aValue
   *        any JS value.
   * @return true if aValue is the proxy object for some transaction, false
   * otherwise.
   */
  isProxifiedTransactionObject(value) {
    return this.proxifiedToRaw.has(value);
  }

  /**
   * Get the raw transaction for the given proxy.
   * @param aProxy
   *        the proxy object
   * @return the transaction proxified by aProxy; |undefined| is returned if
   * aProxy is not a proxified transaction.
   */
  getRawTransaction(proxy) {
    return this.proxifiedToRaw.get(proxy);
  }

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
  add(proxifiedTransaction, forceNewEntry = false) {
    if (!this.isProxifiedTransactionObject(proxifiedTransaction))
      throw new Error("aProxifiedTransaction is not a proxified transaction");

    if (this.length == 0 || forceNewEntry) {
      this.clearRedoEntries();
      this.unshift([proxifiedTransaction]);
    } else {
      this[this.undoPosition].unshift(proxifiedTransaction);
    }
  }

  /**
   * Clear all undo entries.
   */
  clearUndoEntries() {
    if (this.undoPosition < this.length)
      this.splice(this.undoPosition);
  }

  /**
   * Clear all redo entries.
   */
  clearRedoEntries() {
    if (this.undoPosition > 0) {
      this.splice(0, this.undoPosition);
      this._undoPosition = 0;
    }
  }

  /**
   * Clear all entries.
   */
  clearAllEntries() {
    if (this.length > 0) {
      this.splice(0);
      this._undoPosition = 0;
    }
  }
}

XPCOMUtils.defineLazyGetter(this, "TransactionsHistory",
                            () => new TransactionsHistoryArray());

var PlacesTransactions = {
  /**
   * @see Batches in the module documentation.
   */
  batch(transactionsToBatch) {
    if (Array.isArray(transactionsToBatch)) {
      if (transactionsToBatch.length == 0)
        throw new Error("Must pass a non-empty array");

      if (transactionsToBatch.some(
           o => !TransactionsHistory.isProxifiedTransactionObject(o))) {
        throw new Error("Must pass only transaction entries");
      }
      return TransactionsManager.batch(async function() {
        for (let txn of transactionsToBatch) {
          try {
            await txn.transact();
          } catch (ex) {
            console.error(ex);
          }
        }
      });
    }
    if (typeof(transactionsToBatch) == "function") {
      return TransactionsManager.batch(transactionsToBatch);
    }

    throw new Error("Must pass either a function or a transactions array");
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
   * @param [optional] undoEntries
   *        Whether or not to clear undo entries.  Default: true.
   * @param [optional] redoEntries
   *        Whether or not to clear undo entries.  Default: true.
   *
   * @return {Promises).  The promise always resolves.
   * @throws if both aUndoEntries and aRedoEntries are false.
   * @note All undo manager operations are queued. This means that transactions
   * history may change by the time your request is fulfilled.
   */
  clearTransactionsHistory(undoEntries = true, redoEntries = true) {
    return TransactionsManager.clearTransactionsHistory(undoEntries, redoEntries);
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
   * @param index
   *        the index of the entry to retrieve.
   * @return an array of transaction objects in their undo order (that is,
   * reversely to the order they were executed).
   * @throw if aIndex is invalid (< 0 or >= length).
   * @note the returned array is a clone of the history entry and is not
   * kept in sync with the original entry if it changes.
   */
  entry(index) {
    if (!Number.isInteger(index) || index < 0 || index >= this.length)
      throw new Error("Invalid index");

    return TransactionsHistory[index];
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
   * @param   func
   *          a function returning a promise.
   * @return  a promise that resolves once aFunc is done running. The promise
   *          "mirrors" the promise returned by aFunc.
   */
  enqueue(func) {
    // If a transaction awaits on a never resolved promise, or is mistakenly
    // nested, it could hang the transactions queue forever.  Thus we timeout
    // the execution after a meaningful amount of time, to ensure in any case
    // we'll proceed after a while.
    let timeoutPromise = new Promise((resolve, reject) => {
      setTimeout(() => reject(new Error("PlacesTransaction timeout, most likely caused by unresolved pending work.")),
                 TRANSACTIONS_QUEUE_TIMEOUT_MS);
    });
    let promise = this._promise.then(() => Promise.race([func(), timeoutPromise]));

    // Propagate exceptions to the caller, but dismiss them internally.
    this._promise = promise.catch(console.error);
    return promise;
  },

  /**
   * Same as above, but for a promise returned by a function that already run.
   * This is useful, for example, for serializing transact calls with undo calls,
   * even though transact has its own Enqueuer.
   *
   * @param otherPromise
   *        any promise.
   */
  alsoWaitFor(otherPromise) {
    // We don't care if aPromise resolves or rejects, but just that is not
    // pending anymore.
    // If a transaction awaits on a never resolved promise, or is mistakenly
    // nested, it could hang the transactions queue forever.  Thus we timeout
    // the execution after a meaningful amount of time, to ensure in any case
    // we'll proceed after a while.
    let timeoutPromise = new Promise((resolve, reject) => {
      setTimeout(() => reject(new Error("PlacesTransaction timeout, most likely caused by unresolved pending work.")),
                 TRANSACTIONS_QUEUE_TIMEOUT_MS);
    });
    let promise = Promise.race([otherPromise, timeoutPromise])
                         .catch(console.error);
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

  transact(txnProxy) {
    let rawTxn = TransactionsHistory.getRawTransaction(txnProxy);
    if (!rawTxn)
      throw new Error("|transact| was called with an unexpected object");

    if (this._executedTransactions.has(rawTxn))
      throw new Error("Transactions objects may not be recycled.");

    // Add it in advance so one doesn't accidentally do
    // sameTxn.transact(); sameTxn.transact();
    this._executedTransactions.add(rawTxn);

    let promise = this._transactEnqueuer.enqueue(async () => {
      // Don't try to catch exceptions. If execute fails, we better not add the
      // transaction to the undo stack.
      let retval = await rawTxn.execute();

      let forceNewEntry = !this._batching || !this._createdBatchEntry;
      TransactionsHistory.add(txnProxy, forceNewEntry);
      if (this._batching)
        this._createdBatchEntry = true;

      this._updateCommandsOnActiveWindow();
      return retval;
    });
    this._mainEnqueuer.alsoWaitFor(promise);
    return promise;
  },

  batch(task) {
    return this._mainEnqueuer.enqueue(async () => {
      this._batching = true;
      this._createdBatchEntry = false;
      let rv;
      try {
        rv = await task();
      } finally {
        // We must enqueue clearing batching mode to ensure that any existing
        // transactions have completed before we clear the batching mode.
        this._mainEnqueuer.enqueue(() => {
          this._batching = false;
          this._createdBatchEntry = false;
        });
      }
      return rv;
    });
  },

  /**
   * Undo the top undo entry, if any, and update the undo position accordingly.
   */
  undo() {
    let promise = this._mainEnqueuer.enqueue(async () => {
      let entry = TransactionsHistory.topUndoEntry;
      if (!entry)
        return;

      for (let txnProxy of entry) {
        try {
          await TransactionsHistory.getRawTransaction(txnProxy).undo();
        } catch (ex) {
          // If one transaction is broken, it's not safe to work with any other
          // undo entry.  Report the error and clear the undo history.
          console.error(ex, "Can't undo a transaction, clearing undo entries.");
          TransactionsHistory.clearUndoEntries();
          return;
        }
      }
      TransactionsHistory._undoPosition++;
      this._updateCommandsOnActiveWindow();
    });
    this._transactEnqueuer.alsoWaitFor(promise);
    return promise;
  },

  /**
   * Redo the top redo entry, if any, and update the undo position accordingly.
   */
  redo() {
    let promise = this._mainEnqueuer.enqueue(async () => {
      let entry = TransactionsHistory.topRedoEntry;
      if (!entry)
        return;

      for (let i = entry.length - 1; i >= 0; i--) {
        let transaction = TransactionsHistory.getRawTransaction(entry[i]);
        try {
          if (transaction.redo) {
            await transaction.redo();
          } else {
            await transaction.execute();
          }
        } catch (ex) {
          // If one transaction is broken, it's not safe to work with any other
          // redo entry. Report the error and clear the undo history.
          console.error(ex, "Can't redo a transaction, clearing redo entries.");
          TransactionsHistory.clearRedoEntries();
          return;
        }
      }
      TransactionsHistory._undoPosition--;
      this._updateCommandsOnActiveWindow();
    });

    this._transactEnqueuer.alsoWaitFor(promise);
    return promise;
  },

  clearTransactionsHistory(undoEntries, redoEntries) {
    let promise = this._mainEnqueuer.enqueue(function() {
      if (undoEntries && redoEntries)
        TransactionsHistory.clearAllEntries();
      else if (undoEntries)
        TransactionsHistory.clearUndoEntries();
      else if (redoEntries)
        TransactionsHistory.clearRedoEntries();
      else
        throw new Error("either aUndoEntries or aRedoEntries should be true");
    });

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
    } catch (ex) {
      console.error(ex, "Couldn't update undo commands.");
    }
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
function DefineTransaction(requiredProps = [], optionalProps = []) {
  for (let prop of [...requiredProps, ...optionalProps]) {
    if (!DefineTransaction.inputProps.has(prop))
      throw new Error("Property '" + prop + "' is not defined");
  }

  let ctor = function(input) {
    // We want to support both syntaxes:
    // let t = new PlacesTransactions.NewBookmark(),
    // let t = PlacesTransactions.NewBookmark()
    if (this == PlacesTransactions)
      return new ctor(input);

    if (requiredProps.length > 0 || optionalProps.length > 0) {
      // Bind the input properties to the arguments of execute.
      input = DefineTransaction.verifyInput(input, requiredProps, optionalProps);
      this.execute = this.execute.bind(this, input);
    }
    return TransactionsHistory.proxifyTransaction(this);
  };
  return ctor;
}

function simpleValidateFunc(checkFn) {
  return v => {
    if (!checkFn(v))
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

function checkProperty(obj, prop, required, checkFn) {
  if (prop in obj)
    return checkFn(obj[prop]);

  return !required;
}

DefineTransaction.annotationObjectValidate = function(obj) {
  if (obj &&
      checkProperty(obj, "name", true, v => typeof(v) == "string" && v.length > 0) &&
      checkProperty(obj, "expires", false, Number.isInteger) &&
      checkProperty(obj, "flags", false, Number.isInteger) &&
      checkProperty(obj, "value", false, isPrimitive) ) {
    // Nothing else should be set
    let validKeys = ["name", "value", "flags", "expires"];
    if (Object.keys(obj).every(k => validKeys.includes(k))) {
      // Annotations objects are passed through to the backend, to avoid memory
      // leaks, we must clone the object.
      return {...obj};
    }
  }
  throw new Error("Invalid annotation object");
};

DefineTransaction.childObjectValidate = function(obj) {
  if (obj &&
      checkProperty(obj, "title", false, v => typeof(v) == "string") &&
      !("type" in obj && obj.type != PlacesUtils.bookmarks.TYPE_BOOKMARK)) {
    obj.url = DefineTransaction.urlValidate(obj.url);
    let validKeys = ["title", "url"];
    if (Object.keys(obj).every(k => validKeys.includes(k))) {
      return obj;
    }
  }
  throw new Error("Invalid child object");
};

DefineTransaction.urlValidate = function(url) {
  if (url instanceof Ci.nsIURI)
    return new URL(url.spec);
  return new URL(url);
};

DefineTransaction.inputProps = new Map();
DefineTransaction.defineInputProps = function(names, validateFn, defaultValue) {
  for (let name of names) {
    this.inputProps.set(name, {
      validateValue(value) {
        if (value === undefined)
          return defaultValue;
        try {
          return validateFn(value);
        } catch (ex) {
          throw new Error(`Invalid value for input property ${name}: ${ex}`);
        }
      },

      validateInput(input, required) {
        if (required && !(name in input))
          throw new Error(`Required input property is missing: ${name}`);
        return this.validateValue(input[name]);
      },

      isArrayProperty: false
    });
  }
};

DefineTransaction.defineArrayInputProp = function(name, basePropertyName) {
  let baseProp = this.inputProps.get(basePropertyName);
  if (!baseProp)
    throw new Error(`Unknown input property: ${basePropertyName}`);

  this.inputProps.set(name, {
    validateValue(aValue) {
      if (aValue == undefined)
        return [];

      if (!Array.isArray(aValue))
        throw new Error(`${name} input property value must be an array`);

      // We must create a new array in the local scope to avoid a memory leak due
      // to the array global object. We can't use Cu.cloneInto as that doesn't
      // handle the URIs. Slice & map also aren't good enough, so we start off
      // with a clean array and insert what we need into it.
      let newArray = [];
      for (let item of aValue) {
        newArray.push(baseProp.validateValue(item));
      }
      return newArray;
    },

    // We allow setting either the array property itself (e.g. urls), or a
    // single element of it (url, in that example), that is then transformed
    // into a single-element array.
    validateInput(input, required) {
      if (name in input) {
        // It's not allowed to set both though.
        if (basePropertyName in input) {
          throw new Error(`It is not allowed to set both ${name} and
                          ${basePropertyName} as  input properties`);
        }
        let array = this.validateValue(input[name]);
        if (required && array.length == 0) {
          throw new Error(`Empty array passed for required input property:
                           ${name}`);
        }
        return array;
      }
      // If the property is required and it's not set as is, check if the base
      // property is set.
      if (required && !(basePropertyName in input))
        throw new Error(`Required input property is missing: ${name}`);

      if (basePropertyName in input)
        return [baseProp.validateValue(input[basePropertyName])];

      return [];
    },

    isArrayProperty: true
  });
};

DefineTransaction.validatePropertyValue = function(prop, input, required) {
  return this.inputProps.get(prop).validateInput(input, required);
};

DefineTransaction.getInputObjectForSingleValue = function(input,
                                                          requiredProps,
                                                          optionalProps) {
  // The following input forms may be deduced from a single value:
  // * a single required property with or without optional properties (the given
  //   value is set to the required property).
  // * a single optional property with no required properties.
  if (requiredProps.length > 1 ||
      (requiredProps.length == 0 && optionalProps.length > 1)) {
    throw new Error("Transaction input isn't an object");
  }

  let propName = requiredProps.length == 1 ? requiredProps[0]
                                           : optionalProps[0];
  let propValue =
    this.inputProps.get(propName).isArrayProperty && !Array.isArray(input) ?
    [input] : input;
  return { [propName]: propValue };
};

DefineTransaction.verifyInput = function(input,
                                         requiredProps = [],
                                         optionalProps = []) {
  if (requiredProps.length == 0 && optionalProps.length == 0)
    return {};

  // If there's just a single required/optional property, we allow passing it
  // as is, so, for example, one could do PlacesTransactions.Remove(myGuid)
  // rather than PlacesTransactions.Remove({ guid: myGuid}).
  // This shortcut isn't supported for "complex" properties - e.g. one cannot
  // pass an annotation object this way (note there is no use case for this at
  // the moment anyway).
  let isSinglePropertyInput = isPrimitive(input) ||
                              Array.isArray(input) ||
                              (input instanceof Ci.nsISupports);
  if (isSinglePropertyInput) {
    input = this.getInputObjectForSingleValue(input, requiredProps, optionalProps);
  }

  let fixedInput = {};
  for (let prop of requiredProps) {
    fixedInput[prop] = this.validatePropertyValue(prop, input, true);
  }
  for (let prop of optionalProps) {
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
DefineTransaction.defineInputProps(["title", "postData"],
                                   DefineTransaction.strOrNullValidate, null);
DefineTransaction.defineInputProps(["keyword", "oldKeyword", "oldTag", "tag",
                                    "excludingAnnotation"],
                                   DefineTransaction.strValidate, "");
DefineTransaction.defineInputProps(["index", "newIndex"],
                                   DefineTransaction.indexValidate,
                                   PlacesUtils.bookmarks.DEFAULT_INDEX);
DefineTransaction.defineInputProps(["annotation"],
                                   DefineTransaction.annotationObjectValidate);
DefineTransaction.defineInputProps(["child"],
                                   DefineTransaction.childObjectValidate);
DefineTransaction.defineArrayInputProp("guids", "guid");
DefineTransaction.defineArrayInputProp("urls", "url");
DefineTransaction.defineArrayInputProp("tags", "tag");
DefineTransaction.defineArrayInputProp("annotations", "annotation");
DefineTransaction.defineArrayInputProp("children", "child");
DefineTransaction.defineArrayInputProp("excludingAnnotations",
                                       "excludingAnnotation");

/**
 * Creates items (all types) from a bookmarks tree representation, as defined
 * in PlacesUtils.promiseBookmarksTree.
 *
 * @param tree
 *        the bookmarks tree object.  You may pass either a bookmarks tree
 *        returned by promiseBookmarksTree, or a manually defined one.
 * @param [optional] restoring (default: false)
 *        Whether or not the items are restored.  Only in restore mode, are
 *        the guid, dateAdded and lastModified properties honored.
 * @param [optional] excludingAnnotations
 *        Array of annotations names to ignore in aBookmarksTree. This argument
 *        is ignored if aRestoring is set.
 * @note the id, root and charset properties of items in aBookmarksTree are
 *       always ignored.  The index property is ignored for all items but the
 *       root one.
 * @return {Promise}
 * @resolves to the guid of the new item.
 */
// TODO: Replace most of this with insertTree.
function createItemsFromBookmarksTree(tree, restoring = false,
                                      excludingAnnotations = []) {
  function extractLivemarkDetails(annos) {
    let feedURI = null, siteURI = null;
    annos = annos.filter(anno => {
      switch (anno.name) {
        case PlacesUtils.LMANNO_FEEDURI:
          feedURI = Services.io.newURI(anno.value);
          return false;
        case PlacesUtils.LMANNO_SITEURI:
          siteURI = Services.io.newURI(anno.value);
          return false;
        default:
          return true;
      }
    });
    return [feedURI, siteURI, annos];
  }

  async function createItem(item,
                            parentGuid,
                            index = PlacesUtils.bookmarks.DEFAULT_INDEX) {
    let guid;
    let info = { parentGuid, index };
    if (restoring) {
      info.guid = item.guid;
      info.dateAdded = PlacesUtils.toDate(item.dateAdded);
      info.lastModified = PlacesUtils.toDate(item.lastModified);
    }
    let annos = item.annos ? [...item.annos] : [];
    let shouldResetLastModified = false;
    switch (item.type) {
      case PlacesUtils.TYPE_X_MOZ_PLACE: {
        info.url = item.uri;
        if (typeof(item.title) == "string")
          info.title = item.title;

        guid = (await PlacesUtils.bookmarks.insert(info)).guid;

        if ("keyword" in item) {
          let { uri: url, keyword, postData } = item;
          await PlacesUtils.keywords.insert({ url, keyword, postData });
        }
        if ("tags" in item) {
          PlacesUtils.tagging.tagURI(Services.io.newURI(item.uri),
                                     item.tags.split(","));
        }
        break;
      }
      case PlacesUtils.TYPE_X_MOZ_PLACE_CONTAINER: {
        // Either a folder or a livemark
        let feedURI, siteURI;
        [feedURI, siteURI, annos] = extractLivemarkDetails(annos);
        if (!feedURI) {
          info.type = PlacesUtils.bookmarks.TYPE_FOLDER;
          if (typeof(item.title) == "string")
            info.title = item.title;
          guid = (await PlacesUtils.bookmarks.insert(info)).guid;
          if ("children" in item) {
            for (let child of item.children) {
              await createItem(child, guid);
            }
          }
          if (restoring)
            shouldResetLastModified = true;
        } else {
          info.parentId = await PlacesUtils.promiseItemId(parentGuid);
          info.feedURI = feedURI;
          info.siteURI = siteURI;
          if (info.dateAdded)
            info.dateAdded = PlacesUtils.toPRTime(info.dateAdded);
          if (info.lastModified)
            info.lastModified = PlacesUtils.toPRTime(info.lastModified);
          if (typeof(item.title) == "string")
            info.title = item.title;
          guid = (await PlacesUtils.livemarks.addLivemark(info)).guid;
        }
        break;
      }
      case PlacesUtils.TYPE_X_MOZ_PLACE_SEPARATOR: {
        info.type = PlacesUtils.bookmarks.TYPE_SEPARATOR;
        guid = (await PlacesUtils.bookmarks.insert(info)).guid;
        break;
      }
    }
    if (annos.length > 0) {
      if (!restoring && excludingAnnotations.length > 0) {
        annos = annos.filter(a => !excludingAnnotations.includes(a.name));
      }

      if (annos.length > 0) {
        let itemId = await PlacesUtils.promiseItemId(guid);
        PlacesUtils.setAnnotationsForItem(itemId, annos,
          Ci.nsINavBookmarksService.SOURCE_DEFAULT, true);
      }
    }

    if (shouldResetLastModified) {
      let lastModified = PlacesUtils.toDate(item.lastModified);
      await PlacesUtils.bookmarks.update({ guid, lastModified });
    }

    return guid;
  }
  return createItem(tree,
                    tree.parentGuid,
                    tree.index);
}

/** ***************************************************************************
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
                                   ["index", "title", "annotations", "tags"]);
PT.NewBookmark.prototype = Object.seal({
  async execute({ parentGuid, url, index, title, annotations, tags }) {
    let info = { parentGuid, index, url, title };
    // Filter tags to exclude already existing ones.
    if (tags.length > 0) {
      let currentTags = PlacesUtils.tagging
                                   .getTagsForURI(Services.io.newURI(url.href));
      tags = tags.filter(t => !currentTags.includes(t));
    }

    async function createItem() {
      info = await PlacesUtils.bookmarks.insert(info);
      if (annotations.length > 0) {
        let itemId = await PlacesUtils.promiseItemId(info.guid);
        PlacesUtils.setAnnotationsForItem(itemId, annotations,
          Ci.nsINavBookmarksService.SOURCE_DEFAULT, true);
      }
      if (tags.length > 0) {
        PlacesUtils.tagging.tagURI(Services.io.newURI(url.href), tags);
      }
    }

    await createItem();

    this.undo = async function() {
      // Pick up the removed info so we have the accurate last-modified value,
      // which could be affected by any annotation we set in createItem.
      await PlacesUtils.bookmarks.remove(info);
      if (tags.length > 0) {
        PlacesUtils.tagging.untagURI(Services.io.newURI(url.href), tags);
      }
    };
    this.redo = async function() {
      await createItem();
    };
    return info.guid;
  }
});

/**
 * Transaction for creating a folder.
 *
 * Required Input Properties: title, parentGuid.
 * Optional Input Properties: index, annotations, children
 *
 * When this transaction is executed, it's resolved to the new folder's GUID.
 */
PT.NewFolder = DefineTransaction(["parentGuid", "title"],
                                 ["index", "annotations", "children"]);
PT.NewFolder.prototype = Object.seal({
  async execute({ parentGuid, title, index, annotations, children }) {
    let folderGuid;
    let info = {
      children: [{
        // Ensure to specify a guid to be restored on redo.
        guid: PlacesUtils.history.makeGuid(),
        title,
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
      }],
      // insertTree uses guid as the parent for where it is being inserted
      // into.
      guid: parentGuid,
    };

    if (children && children.length > 0) {
      // Ensure to specify a guid for each child to be restored on redo.
      info.children[0].children = children.map(c => {
        c.guid = PlacesUtils.history.makeGuid();
        return c;
      });
    }

    async function createItem() {
      // Note, insertTree returns an array, rather than the folder/child structure.
      // For simplicity, we only get the new folder id here. This means that
      // an undo then redo won't retain exactly the same information for all
      // the child bookmarks, but we believe that isn't important at the moment.
      let bmInfo = await PlacesUtils.bookmarks.insertTree(info);
      // insertTree returns an array, but we only need to deal with the folder guid.
      folderGuid = bmInfo[0].guid;

      // Bug 1388097: insertTree doesn't handle inserting at a specific index for the folder,
      // therefore we update the bookmark manually afterwards.
      if (index != PlacesUtils.bookmarks.DEFAULT_INDEX) {
        bmInfo[0].index = index;
        bmInfo = await PlacesUtils.bookmarks.update(bmInfo[0]);
      }

      if (annotations.length > 0) {
        let itemId = await PlacesUtils.promiseItemId(folderGuid);
        PlacesUtils.setAnnotationsForItem(itemId, annotations,
          Ci.nsINavBookmarksService.SOURCE_DEFAULT, true);
      }
    }
    await createItem();

    this.undo = async function() {
      await PlacesUtils.bookmarks.remove(folderGuid);
    };
    this.redo = async function() {
      await createItem();
    };
    return folderGuid;
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
  async execute(info) {
    info.type = PlacesUtils.bookmarks.TYPE_SEPARATOR;
    info = await PlacesUtils.bookmarks.insert(info);
    this.undo = PlacesUtils.bookmarks.remove.bind(PlacesUtils.bookmarks, info);
    this.redo = PlacesUtils.bookmarks.insert.bind(PlacesUtils.bookmarks, info);
    return info.guid;
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
  async execute({ feedUrl, title, parentGuid, siteUrl, index, annotations }) {
    let livemarkInfo = { title,
                         feedURI: Services.io.newURI(feedUrl.href),
                         siteURI: siteUrl ? Services.io.newURI(siteUrl.href) : null,
                         index,
                         parentGuid};
    let createItem = async function() {
      let livemark = await PlacesUtils.livemarks.addLivemark(livemarkInfo);
      if (annotations.length > 0) {
        PlacesUtils.setAnnotationsForItem(livemark.id, annotations,
          Ci.nsINavBookmarksService.SOURCE_DEFAULT, true);
      }
      return livemark;
    };

    let livemark = await createItem();
    livemarkInfo.guid = livemark.guid;
    livemarkInfo.dateAdded = livemark.dateAdded;
    livemarkInfo.lastModified = livemark.lastModified;

    this.undo = async function() {
      await PlacesUtils.livemarks.removeLivemark(livemark);
    };
    this.redo = async function() {
      livemark = await createItem();
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
PT.Move = DefineTransaction(["guids", "newParentGuid"], ["newIndex"]);
PT.Move.prototype = Object.seal({
  async execute({ guids, newParentGuid, newIndex }) {
    let originalInfos = [];
    let index = newIndex;

    for (let guid of guids) {
      // We need to save the original data for undo.
      let originalInfo = await PlacesUtils.bookmarks.fetch(guid);
      if (!originalInfo)
        throw new Error("Cannot move a non-existent item");

      originalInfos.push(originalInfo);
    }

    await PlacesUtils.bookmarks.moveToFolder(guids, newParentGuid, index);

    this.undo = async function() {
      // Undo has the potential for moving multiple bookmarks to multiple different
      // folders and positions, which is very complicated to manage. Therefore we do
      // individual moves one at a time and hopefully everything is put back approximately
      // where it should be.
      for (let info of originalInfos) {
        await PlacesUtils.bookmarks.update(info);
      }
    };
    this.redo = PlacesUtils.bookmarks.moveToFolder.bind(PlacesUtils.bookmarks, guids, newParentGuid, index);
    return guids;
  }
});

/**
 * Transaction for setting the title for an item.
 *
 * Required Input Properties: guid, title.
 */
PT.EditTitle = DefineTransaction(["guid", "title"]);
PT.EditTitle.prototype = Object.seal({
  async execute({ guid, title }) {
    let originalInfo = await PlacesUtils.bookmarks.fetch(guid);
    if (!originalInfo)
      throw new Error("cannot update a non-existent item");

    let updateInfo = { guid, title };
    updateInfo = await PlacesUtils.bookmarks.update(updateInfo);

    this.undo = PlacesUtils.bookmarks.update.bind(PlacesUtils.bookmarks, originalInfo);
    this.redo = PlacesUtils.bookmarks.update.bind(PlacesUtils.bookmarks, updateInfo);
  }
});

/**
 * Transaction for setting the URI for an item.
 *
 * Required Input Properties: guid, url.
 */
PT.EditUrl = DefineTransaction(["guid", "url"]);
PT.EditUrl.prototype = Object.seal({
  async execute({ guid, url }) {
    let originalInfo = await PlacesUtils.bookmarks.fetch(guid);
    if (!originalInfo)
      throw new Error("cannot update a non-existent item");
    if (originalInfo.type != PlacesUtils.bookmarks.TYPE_BOOKMARK)
      throw new Error("Cannot edit url for non-bookmark items");

    let uri = Services.io.newURI(url.href);
    let originalURI = Services.io.newURI(originalInfo.url.href);
    let originalTags = PlacesUtils.tagging.getTagsForURI(originalURI);
    let updatedInfo = { guid, url };
    let newURIAdditionalTags = null;

    async function updateItem() {
      updatedInfo = await PlacesUtils.bookmarks.update(updatedInfo);
      // Move tags from the original URI to the new URI.
      if (originalTags.length > 0) {
        // Untag the original URI only if this was the only bookmark.
        if (!(await PlacesUtils.bookmarks.fetch({ url: originalInfo.url })))
          PlacesUtils.tagging.untagURI(originalURI, originalTags);
        let currentNewURITags = PlacesUtils.tagging.getTagsForURI(uri);
        newURIAdditionalTags = originalTags.filter(t => !currentNewURITags.includes(t));
        if (newURIAdditionalTags && newURIAdditionalTags.length > 0)
          PlacesUtils.tagging.tagURI(uri, newURIAdditionalTags);
      }
    }
    await updateItem();

    this.undo = async function() {
      await PlacesUtils.bookmarks.update(originalInfo);
      // Move tags from new URI to original URI.
      if (originalTags.length > 0) {
         // Only untag the new URI if this is the only bookmark.
         if (newURIAdditionalTags && newURIAdditionalTags.length > 0 &&
            !(await PlacesUtils.bookmarks.fetch({ url }))) {
          PlacesUtils.tagging.untagURI(uri, newURIAdditionalTags);
         }
        PlacesUtils.tagging.tagURI(originalURI, originalTags);
      }
    };

    this.redo = async function() {
      updatedInfo = await updateItem();
    };
  }
});

/**
 * Transaction for setting the keyword for a bookmark.
 *
 * Required Input Properties: guid, keyword.
 * Optional Input Properties: postData, oldKeyword.
 */
PT.EditKeyword = DefineTransaction(["guid", "keyword"],
                                   ["postData", "oldKeyword"]);
PT.EditKeyword.prototype = Object.seal({
  async execute({ guid, keyword, postData, oldKeyword }) {
    let url;
    let oldKeywordEntry;
    if (oldKeyword) {
      oldKeywordEntry = await PlacesUtils.keywords.fetch(oldKeyword);
      url = oldKeywordEntry.url;
      await PlacesUtils.keywords.remove(oldKeyword);
    }

    if (keyword) {
      if (!url) {
        url = (await PlacesUtils.bookmarks.fetch(guid)).url;
      }
      await PlacesUtils.keywords.insert({
        url,
        keyword,
        postData: postData || (oldKeywordEntry ? oldKeywordEntry.postData : "")
      });
    }

    this.undo = async function() {
      if (keyword) {
        await PlacesUtils.keywords.remove(keyword);
      }
      if (oldKeywordEntry) {
        await PlacesUtils.keywords.insert(oldKeywordEntry);
      }
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
  async execute({ guid }) {
    let sortingMethod = (node_a, node_b) => {
      if (PlacesUtils.nodeIsContainer(node_a) && !PlacesUtils.nodeIsContainer(node_b))
        return -1;
      if (!PlacesUtils.nodeIsContainer(node_a) && PlacesUtils.nodeIsContainer(node_b))
        return 1;
      return node_a.title.localeCompare(node_b.title);
    };
    let oldOrderGuids = [];
    let newOrderGuids = [];
    let preSepNodes = [];

    // This is not great, since it does main-thread IO.
    // PromiseBookmarksTree can't be used, since it' won't stop at the first level'.
    let root = PlacesUtils.getFolderContents(guid, false, false).root;
    for (let i = 0; i < root.childCount; ++i) {
      let node = root.getChild(i);
      oldOrderGuids.push(node.bookmarkGuid);
      if (PlacesUtils.nodeIsSeparator(node)) {
        if (preSepNodes.length > 0) {
          preSepNodes.sort(sortingMethod);
          newOrderGuids.push(...preSepNodes.map(n => n.bookmarkGuid));
          preSepNodes = [];
        }
        newOrderGuids.push(node.bookmarkGuid);
      } else {
        preSepNodes.push(node);
      }
    }
    root.containerOpen = false;
    if (preSepNodes.length > 0) {
      preSepNodes.sort(sortingMethod);
      newOrderGuids.push(...preSepNodes.map(n => n.bookmarkGuid));
    }
    await PlacesUtils.bookmarks.reorder(guid, newOrderGuids);

    this.undo = async function() {
      await PlacesUtils.bookmarks.reorder(guid, oldOrderGuids);
    };
    this.redo = async function() {
      await PlacesUtils.bookmarks.reorder(guid, newOrderGuids);
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
  async execute({ guids }) {
    let removedItems = [];

    for (let guid of guids) {
      try {
        // Although we don't strictly need to get this information for the remove,
        // we do need it for the possibility of undo().
        removedItems.push(await PlacesUtils.promiseBookmarksTree(guid));
      } catch (ex) {
        throw new Error("Failed to get info for the specified item (guid: " +
                          guid + "): " + ex);
      }
    }

    let removeThem = async function() {
      let bmsToRemove = [];
      for (let info of removedItems) {
        if (info.annos &&
            info.annos.some(anno => anno.name == PlacesUtils.LMANNO_FEEDURI)) {
          await PlacesUtils.livemarks.removeLivemark({ guid: info.guid });
        } else {
          bmsToRemove.push({guid: info.guid});
        }
      }

      if (bmsToRemove.length) {
        await PlacesUtils.bookmarks.remove(bmsToRemove);
      }
    };
    await removeThem();

    this.undo = async function() {
      for (let info of removedItems) {
        await createItemsFromBookmarksTree(info, true);
      }
    };
    this.redo = removeThem;
  }
};

/**
 * Transaction for tagging urls.
 *
 * Required Input Properties: urls, tags.
 */
PT.Tag = DefineTransaction(["urls", "tags"]);
PT.Tag.prototype = {
  async execute({ urls, tags }) {
    let onUndo = [], onRedo = [];
    for (let url of urls) {
      if (!(await PlacesUtils.bookmarks.fetch({ url }))) {
        // Tagging is only allowed for bookmarked URIs (but see 424160).
        let createTxn = TransactionsHistory.getRawTransaction(
          PT.NewBookmark({ url,
                           tags,
                           parentGuid: PlacesUtils.bookmarks.unfiledGuid })
        );
        await createTxn.execute();
        onUndo.unshift(createTxn.undo.bind(createTxn));
        onRedo.push(createTxn.redo.bind(createTxn));
      } else {
        let uri = Services.io.newURI(url.href);
        let currentTags = PlacesUtils.tagging.getTagsForURI(uri);
        let newTags = tags.filter(t => !currentTags.includes(t));
        if (newTags.length) {
          PlacesUtils.tagging.tagURI(uri, newTags);
          onUndo.unshift(() => {
            PlacesUtils.tagging.untagURI(uri, newTags);
          });
          onRedo.push(() => {
            PlacesUtils.tagging.tagURI(uri, newTags);
          });
        }
      }
    }
    this.undo = async function() {
      for (let f of onUndo) {
        await f();
      }
    };
    this.redo = async function() {
      for (let f of onRedo) {
        await f();
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
  execute({ urls, tags }) {
    let onUndo = [], onRedo = [];
    for (let url of urls) {
      let uri = Services.io.newURI(url.href);
      let tagsToRemove;
      let tagsSet = PlacesUtils.tagging.getTagsForURI(uri);
      if (tags.length > 0) {
        tagsToRemove = tags.filter(t => tagsSet.includes(t));
      } else {
        tagsToRemove = tagsSet;
      }
      if (tagsToRemove.length > 0) {
        PlacesUtils.tagging.untagURI(uri, tagsToRemove);
      }
      onUndo.unshift(() => {
        if (tagsToRemove.length > 0) {
          PlacesUtils.tagging.tagURI(uri, tagsToRemove);
        }
      });
      onRedo.push(() => {
        if (tagsToRemove.length > 0) {
          PlacesUtils.tagging.untagURI(uri, tagsToRemove);
        }
      });
    }
    this.undo = async function() {
      for (let f of onUndo) {
        await f();
      }
    };
    this.redo = async function() {
      for (let f of onRedo) {
        await f();
      }
    };
  }
};

/**
 * Transaction for renaming a tag.
 *
 * Required Input Properties: oldTag, tag.
 */
PT.RenameTag = DefineTransaction(["oldTag", "tag"]);
PT.RenameTag.prototype = {
  async execute({ oldTag, tag }) {
    // For now this is implemented by untagging and tagging all the bookmarks.
    // We should create a specialized bookmarking API to just rename the tag.
    let onUndo = [], onRedo = [];
    let urls = PlacesUtils.tagging.getURIsForTag(oldTag);
    if (urls.length > 0) {
      let tagTxn = TransactionsHistory.getRawTransaction(
        PT.Tag({ urls, tags: [tag] })
      );
      await tagTxn.execute();
      onUndo.unshift(tagTxn.undo.bind(tagTxn));
      onRedo.push(tagTxn.redo.bind(tagTxn));
      let untagTxn = TransactionsHistory.getRawTransaction(
        PT.Untag({ urls, tags: [oldTag] })
      );
      await untagTxn.execute();
      onUndo.unshift(untagTxn.undo.bind(untagTxn));
      onRedo.push(untagTxn.redo.bind(untagTxn));

      // Update all the place: queries that refer to this tag.
      let db = await PlacesUtils.promiseDBConnection();
      let rows = await db.executeCached(`
        SELECT h.url, b.guid, b.title
        FROM moz_places h
        JOIN moz_bookmarks b ON b.fk = h.id
        WHERE url_hash BETWEEN hash("place", "prefix_lo")
                           AND hash("place", "prefix_hi")
          AND url LIKE :tagQuery
      `, { tagQuery: "%tag=%" });
      for (let row of rows) {
        let url = row.getResultByName("url");
        try {
          url = new URL(url);
          let urlParams = new URLSearchParams(url.pathname);
          let tags = urlParams.getAll("tag");
          if (!tags.includes(oldTag))
            continue;
          if (tags.length > 1) {
            // URLSearchParams cannot set more than 1 same-named param.
            urlParams.delete("tag");
            urlParams.set("tag", tag);
            url = new URL(url.protocol + urlParams +
                          "&tag=" + tags.filter(t => t != oldTag).join("&tag="));
          } else {
            urlParams.set("tag", tag);
            url = new URL(url.protocol + urlParams);
          }
        } catch (ex) {
          Cu.reportError("Invalid bookmark url: " + row.getResultByName("url") + ": " + ex);
          continue;
        }
        let guid = row.getResultByName("guid");
        let title = row.getResultByName("title");

        let editUrlTxn = TransactionsHistory.getRawTransaction(
          PT.EditUrl({ guid, url })
        );
        await editUrlTxn.execute();
        onUndo.unshift(editUrlTxn.undo.bind(editUrlTxn));
        onRedo.push(editUrlTxn.redo.bind(editUrlTxn));
        if (title == oldTag) {
          let editTitleTxn = TransactionsHistory.getRawTransaction(
            PT.EditTitle({ guid, title: tag })
          );
          await editTitleTxn.execute();
          onUndo.unshift(editTitleTxn.undo.bind(editTitleTxn));
          onRedo.push(editTitleTxn.redo.bind(editTitleTxn));
        }
      }
    }
    this.undo = async function() {
      for (let f of onUndo) {
        await f();
      }
    };
    this.redo = async function() {
      for (let f of onRedo) {
        await f();
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
  async execute({ guid, newParentGuid, newIndex, excludingAnnotations }) {
    let creationInfo = null;
    try {
      creationInfo = await PlacesUtils.promiseBookmarksTree(guid);
    } catch (ex) {
      throw new Error("Failed to get info for the specified item (guid: " +
                      guid + "). Ex: " + ex);
    }
    creationInfo.parentGuid = newParentGuid;
    creationInfo.index = newIndex;

    let newItemGuid = await createItemsFromBookmarksTree(creationInfo, false,
                                                         excludingAnnotations);
    let newItemInfo = null;
    this.undo = async function() {
      if (!newItemInfo) {
        newItemInfo = await PlacesUtils.promiseBookmarksTree(newItemGuid);
      }
      await PlacesUtils.bookmarks.remove(newItemGuid);
    };
    this.redo = async function() {
      await createItemsFromBookmarksTree(newItemInfo, true);
    };

    return newItemGuid;
  }
};
