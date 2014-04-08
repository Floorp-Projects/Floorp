/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["PlacesTransactions"];

/**
 * Overview
 * --------
 * This modules serves as the transactions manager for Places, and implements
 * all the standard transactions for its UI commands (creating items, editing
 * various properties, etc.). It shares most of its semantics with common
 * command pattern implementations, the HTML5 Undo Manager in particular.
 * However, the asynchronous design of [future] Places APIs, combined with the
 * commitment to serialize all UI operations, makes things a little bit
 * different.  For example, when |undo| is called in order to undo the top undo
 * entry, the caller cannot tell for sure what entry would it be because the
 * execution of some transaction is either in process, or queued.
 *
 * GUIDs and item-ids
 * -------------------
 * The Bookmarks API still relies heavily on item-ids, but since those do not
 * play nicely with the concept of undo and redo (especially not in an
 * asynchronous environment), this API only accepts bookmark GUIDs, both for
 * input (e.g. for specifying the parent folder for a new bookmark) and for
 * output (when the GUID for such a bookmark is propagated).
 *
 * GUIDs are readily available when dealing with the "output" of this API and
 * when result nodes are used (see nsINavHistoryResultNode::bookmarkGUID).
 * If you only have item-ids in hand, use PlacesUtils.promiseItemGUID for
 * converting them.  Should you need to convert them back into itemIds, use
 * PlacesUtils.promiseItemId.
 *
 * The Standard Transactions
 * -------------------------
 * At the bottom of this module you will find implementations for all Places UI
 * commands (One should almost never fallback to raw Places APIs.  Please file
 * a bug if you find anything uncovered). The transactions' constructors are
 * set on the PlacesTransactions object (e.g. PlacesTransactions.NewFolder).
 * The input for this constructors is taken in the form of a single argument
 * plain object.  Input properties may be either required (e.g. the |keyword|
 * property for the EditKeyword transaction) or optional (e.g. the |keyword|
 * property for NewBookmark).  Once a transaction is created, you may pass it
 * to |transact| or use it in the for batching (see next section).
 *
 * The constructors throw right away when any required input is missing or when
 * some input is invalid "on the surface" (e.g. GUID values are validated to be
 * 12-characters strings, but are not validated to point to existing item.  Such
 * an error will reveal when the transaction is executed).
 *
 * To make things simple, a given input property has the same basic meaning and
 * valid values across all transactions which accept it in the input object.
 * Here is a list of all supported input properties along with their expected
 * values:
 *  - uri: an nsIURI object.
 *  - feedURI: an nsIURI object, holding the url for a live bookmark.
 *  - siteURI: an nsIURI object, holding the url for the site with which
 *             a live bookmark is associated.
 *  - GUID, parentGUID, newParentGUID: a valid places GUID string.
 *  - title: a string
 *  - index, newIndex: the position of an item in its containing folder,
 *    starting from 0.
 *    integer and PlacesUtils.bookmarks.DEFAULT_INDEX
 *  - annotationObject: see PlacesUtils.setAnnotationsForItem
 *  - annotations: an array of annotation objects as above.
 *  - tags: an array of strings.
 *
 * Batching transactions
 * ---------------------
 * Sometimes it is useful to "batch" or "merge" transactions. For example,
 * "Bookmark All Tabs" may be implemented as one NewFolder transaction followed
 * by numerous NewBookmark transactions - all to be undone or redone in a single
 * command.  The |transact| method makes this possible using a generator
 * function as an input.  These generators have the same semantics as in
 * Task.jsm except that when you yield a transaction, it's executed, and the
 * resolution (e.g. the new bookmark GUID) is sent to the generator so you can
 * use it as the input for another transaction.  See |transact| for the details.
 *
 * "Custom" transactions
 * ---------------------
 * In the legacy transactions API it was possible to pass-in transactions
 * implemented "externally".  For various reason this isn't allowed anymore:
 * transact throws right away if one attempts to pass a transaction that was not
 * created by this module.  However, it's almost always possible to achieve the
 * same functionality with the batching technique described above.
 *
 * The transactions-history structure
 * ----------------------------------
 * The transactions-history is a two-dimensional stack of transactions: the
 * transactions are ordered in reverse to the order they were committed.
 * It's two-dimensional because the undo manager allows batching transactions
 * together for the purpose of undo or redo (batched transactions can never be
 * undone or redone partially).
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
                                  "resource://gre/modules/devtools/Console.jsm");

// Updates commands in the undo group of the active window commands.
// Inactive windows commands will be updated on focus.
function updateCommandsOnActiveWindow() {
  // Updating "undo" will cause a group update including "redo".
  try {
    let win = Services.focus.activeWindow;
    if (win)
      win.updateCommands("undo");
  }
  catch(ex) { console.error(ex, "Couldn't update undo commands"); }
}

// The internal object for managing the transactions history.
// The public API is included in PlacesTransactions.
// TODO bug 982099: extending the array "properly" makes it painful to implement
// getters.  If/when ES6 gets proper array subclassing we can revise this.
let TransactionsHistory = [];
TransactionsHistory.__proto__ = {
  __proto__: Array.prototype,

  // The index of the first undo entry (if any) - See the documentation
  // at the top of this file.
  _undoPosition: 0,
  get undoPosition() this._undoPosition,

  // Handy shortcuts
  get topUndoEntry() this.undoPosition < this.length ?
                     this[this.undoPosition] : null,
  get topRedoEntry() this.undoPosition > 0 ?
                     this[this.undoPosition - 1] : null,

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
    let proxy = Object.freeze({});
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
  isProxifiedTransactionObject:
  function (aValue) this.proxifiedToRaw.has(aValue),

  /**
   * Get the raw transaction for the given proxy.
   * @param aProxy
   *        the proxy object
   * @return the transaction proxified by aProxy; |undefined| is returned if
   * aProxy is not a proxified transaction.
   */
  getRawTransaction: function (aProxy) this.proxifiedToRaw.get(aProxy),

  /**
   * Undo the top undo entry, if any, and update the undo position accordingly.
   */
  undo: function* () {
    let entry = this.topUndoEntry;
    if (!entry)
      return;

    for (let transaction of entry) {
      try {
        yield TransactionsHistory.getRawTransaction(transaction).undo();
      }
      catch(ex) {
        // If one transaction is broken, it's not safe to work with any other
        // undo entry.  Report the error and clear the undo history.
        console.error(ex,
                      "Couldn't undo a transaction, clearing all undo entries.");
        this.clearUndoEntries();
        return;
      }
    }
    this._undoPosition++;
    updateCommandsOnActiveWindow();
  },

  /**
   * Redo the top redo entry, if any, and update the undo position accordingly.
   */
  redo: function* () {
    let entry = this.topRedoEntry;
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
      catch(ex) {
        // If one transaction is broken, it's not safe to work with any other
        // redo entry. Report the error and clear the undo history.
        console.error(ex,
                      "Couldn't redo a transaction, clearing all redo entries.");
        this.clearRedoEntries();
        return;
      }
    }
    this._undoPosition--;
    updateCommandsOnActiveWindow();
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
  add: function (aProxifiedTransaction, aForceNewEntry = false) {
    if (!this.isProxifiedTransactionObject(aProxifiedTransaction))
      throw new Error("aProxifiedTransaction is not a proxified transaction");

    if (this.length == 0 || aForceNewEntry) {
      this.clearRedoEntries();
      this.unshift([aProxifiedTransaction]);
    }
    else {
      this[this.undoPosition].unshift(aProxifiedTransaction);
    }
    updateCommandsOnActiveWindow();
  },

  /**
   * Clear all undo entries.
   */
  clearUndoEntries: function () {
    if (this.undoPosition < this.length)
      this.splice(this.undoPosition);
  },

  /**
   * Clear all redo entries.
   */
  clearRedoEntries: function () {
    if (this.undoPosition > 0) {
      this.splice(0, this.undoPosition);
      this._undoPosition = 0;
    }
  },

  /**
   * Clear all entries.
   */
  clearAllEntries: function () {
    if (this.length > 0) {
      this.splice(0);
      this._undoPosition = 0;
    }
  }
};


// Our transaction manager is asynchronous in the sense that all of its methods
// don't execute synchronously. However, all actions must be serialized.
let currentTask = Promise.resolve();
function Serialize(aTask) {
  // Ignore failures.
  return currentTask = currentTask.then( () => Task.spawn(aTask) )
                                  .then(null, Components.utils.reportError);
}

// Transactions object should never be recycled (that is, |execute| should
// only be called once, or not at all, after they're constructed.
// This keeps track of all transactions which were executed.
let executedTransactions = new WeakMap(); // TODO: use WeakSet (bug 792439)
executedTransactions.add = k => executedTransactions.set(k, null);

let PlacesTransactions = {
  /**
   * Asynchronously transact either a single transaction, or a sequence of
   * transactions that would be treated as a single entry in the transactions
   * history.
   *
   * @param aToTransact
   *        Either a transaction object or a generator function (ES6-style only)
   *        that yields transaction objects.
   *
   *        Generator mode how-to: when a transaction is yielded, it's executed.
   *        Then, if it was executed successfully, the resolution of |execute|
   *        is sent to the generator.  If |execute| threw or rejected, the
   *        exception is propagated to the generator.
   *        Any other value yielded by a generator function is handled the
   *        same way as in a Task (see Task.jsm).
   *
   * @return {Promise}
   * @resolves either to the resolution of |execute|, in single-transaction mode,
   * or to the return value of the generator, in generator-mode.
   * @rejects either if |execute| threw, in single-transaction mode, or if
   * the generator function threw (or didn't handle) an exception, in generator
   * mode.
   * @throws if aTransactionOrGeneratorFunction is neither a transaction object
   * created by this module or a generator function.
   * @note If no transaction was executed successfully, the transactions history
   * is not affected.
   *
   * @note All PlacesTransactions operations are serialized. This means that the
   * transactions history state may change by the time the transaction/generator
   * is processed.  It's guaranteed, however, that a generator function "blocks"
   * the queue: that is, it is assured that no other operations are performed
   * by or on PlacesTransactions until the generator returns.  Keep in mind you
   * are not protected from consumers who use the raw places APIs directly.
   */
  transact: function (aToTransact) {
    let generatorMode = typeof(aToTransact) == "function";
    if (generatorMode) {
      if (!aToTransact.isGenerator())
        throw new Error("aToTransact is not a generator function");
    }
    else {
      if (!TransactionsHistory.isProxifiedTransactionObject(aToTransact))
        throw new Error("aToTransact is not a valid transaction object");
      if (executedTransactions.has(aToTransact))
        throw new Error("Transactions objects may not be recycled.");
    }

    return Serialize(function* () {
      // The entry in the transactions history is created once the first
      // transaction is committed. This means that if |transact| is called
      // in its "generator mode" and no transactions are committed by the
      // generator, the transactions history is left unchanged.
      // Bug 982115: Depending on how this API is actually used we may revise
      // this decision and make it so |transact| always forces a new entry.
      let forceNewEntry = true;
      function* transactOneTransaction(aTransaction) {
        let retval =
          yield TransactionsHistory.getRawTransaction(aTransaction).execute();
        executedTransactions.add(aTransaction);
        TransactionsHistory.add(aTransaction, forceNewEntry);
        forceNewEntry = false;
        return retval;
      }

      function* transactBatch(aGenerator) {
        let error = false;
        let sendValue = undefined;
        while (true) {
          let next = error ?
                     aGenerator.throw(sendValue) : aGenerator.next(sendValue);
          sendValue = next.value;
          if (Object.prototype.toString.call(sendValue) == "[object Generator]") {
            sendValue = yield transactBatch(sendValue);
          }
          else if (typeof(sendValue) == "object" && sendValue) {
            if (TransactionsHistory.isProxifiedTransactionObject(sendValue)) {
              if (executedTransactions.has(sendValue)) {
                sendValue = new Error("Transactions may not be recycled.");
                error = true;
              }
              else {
                sendValue = yield transactOneTransaction(sendValue);
              }
            }
            else if ("then" in sendValue) {
              sendValue = yield sendValue;
            }
          }
          if (next.done)
            break;
        }
        return sendValue;
      }

      if (generatorMode)
        return yield transactBatch(aToTransact());
      else
        return yield transactOneTransaction(aToTransact);
    }.bind(this));
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
  undo: function () Serialize(() => TransactionsHistory.undo()),

  /**
   * Asynchronously redo the transaction immediately before the current undo
   * position in the transactions history, if any, and adjusts the undo
   * position.
   *
   * @return {Promises).  The promise always resolves.
   * @note All undo manager operations are queued. This means that transactions
   * history may change by the time your request is fulfilled.
   */
  redo: function () Serialize(() => TransactionsHistory.redo()),

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
  clearTransactionsHistory:
  function (aUndoEntries = true, aRedoEntries = true) {
    return Serialize(function* () {
      if (aUndoEntries && aRedoEntries)
        TransactionsHistory.clearAllEntries();
      else if (aUndoEntries)
        TransactionsHistory.clearUndoEntries();
      else if (aRedoEntries)
        TransactionsHistory.clearRedoEntries();
      else
        throw new Error("either aUndoEntries or aRedoEntries should be true");
    });
  },

  /**
   * The numbers of entries in the transactions history.
   */
  get length() TransactionsHistory.length,

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
  entry: function (aIndex) {
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
  get undoPosition() TransactionsHistory.undoPosition,

  /**
   * Shortcut for accessing the top undo entry in the transaction history.
   */
  get topUndoEntry() TransactionsHistory.topUndoEntry,

  /**
   * Shortcut for accessing the top redo entry in the transaction history.
   */
  get topRedoEntry() TransactionsHistory.topRedoEntry
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
                         ...[input[prop] for (prop of aRequiredProps)],
                         ...[input[prop] for (prop of aOptionalProps)]];
      this.execute = Function.bind.apply(this.execute, executeArgs);
    }
    return TransactionsHistory.proxifyTransaction(this);
  };
  return ctor;
}

DefineTransaction.isStr = v => typeof(v) == "string";
DefineTransaction.isURI = v => v instanceof Components.interfaces.nsIURI;
DefineTransaction.isIndex = v => Number.isInteger(v) &&
                                 v >= PlacesUtils.bookmarks.DEFAULT_INDEX;
DefineTransaction.isGUID = v => /^[a-zA-Z0-9\-_]{12}$/.test(v);
DefineTransaction.isPrimitive = v => v === null || (typeof(v) != "object" &&
                                                    typeof(v) != "function");
DefineTransaction.isAnnotationObject = function (obj) {
  let checkProperty = (aPropName, aRequired, aCheckFunc) => {
    if (aPropName in obj)
      return aCheckFunc(obj[aPropName]);

    return !aRequired;
  };

  if (obj &&
      checkProperty("name",    true,  DefineTransaction.isStr)      &&
      checkProperty("expires", false, Number.isInteger) &&
      checkProperty("flags",   false, Number.isInteger) &&
      checkProperty("value",   false, DefineTransaction.isPrimitive) ) {
    // Nothing else should be set
    let validKeys = ["name", "value", "flags", "expires"];
    if (Object.keys(obj).every( (k) => validKeys.indexOf(k) != -1 ))
      return true;
  }
  return false;
};

DefineTransaction.inputProps = new Map();
DefineTransaction.defineInputProps =
function (aNames, aValidationFunction, aDefaultValue) {
  for (let name of aNames) {
    this.inputProps.set(name, {
      validate:     aValidationFunction,
      defaultValue: aDefaultValue,
      isGUIDProp:   false
    });
  }
};

DefineTransaction.defineArrayInputProp =
function (aName, aValidationFunction, aDefaultValue) {
  this.inputProps.set(aName, {
    validate:     (v) => Array.isArray(v) && v.every(aValidationFunction),
    defaultValue: aDefaultValue,
    isGUIDProp:   false
  });
};

DefineTransaction.verifyPropertyValue =
function (aProp, aValue, aRequired) {
  if (aValue === undefined) {
    if (aRequired)
      throw new Error("Required property is missing: " + aProp);
    return this.inputProps.get(aProp).defaultValue;
  }

  if (!this.inputProps.get(aProp).validate(aValue))
    throw new Error("Invalid value for property: " + aProp);

  if (Array.isArray(aValue)) {
    // The original array cannot be referenced by this module because it would
    // then implicitly reference its global as well.
    return Components.utils.cloneInto(aValue, {});
  }

  return aValue;
};

DefineTransaction.verifyInput =
function (aInput, aRequired = [], aOptional = []) {
  if (aRequired.length == 0 && aOptional.length == 0)
    return {};

  // If there's just a single required/optional property, we allow passing it
  // as is, so, for example, one could do PlacesTransactions.RemoveItem(myGUID)
  // rather than PlacesTransactions.RemoveItem({ GUID: myGUID}).
  // This shortcut isn't supported for "complex" properties - e.g. one cannot
  // pass an annotation object this way (note there is no use case for this at
  // the moment anyway).
  let isSinglePropertyInput =
    this.isPrimitive(aInput) ||
    (aInput instanceof Components.interfaces.nsISupports);
  let fixedInput = { };
  if (aRequired.length > 0) {
    if (isSinglePropertyInput) {
      if (aRequired.length == 1) {
        let prop = aRequired[0], value = aInput;
        value = this.verifyPropertyValue(prop, value, true);
        fixedInput[prop] = value;
      }
      else {
        throw new Error("Transaction input isn't an object");
      }
    }
    else {
      for (let prop of aRequired) {
        let value = this.verifyPropertyValue(prop, aInput[prop], true);
        fixedInput[prop] = value;
      }
    }
  }

  if (aOptional.length > 0) {
    if (isSinglePropertyInput && !aRequired.length > 0) {
      if (aOptional.length == 1) {
        let prop = aOptional[0], value = aInput;
        value = this.verifyPropertyValue(prop, value, true);
        fixedInput[prop] = value;
      }
      else if (aInput !== null && aInput !== undefined) {
        throw new Error("Transaction input isn't an object");
      }
    }
    else {
      for (let prop of aOptional) {
        let value = this.verifyPropertyValue(prop, aInput[prop], false);
        if (value !== undefined)
          fixedInput[prop] = value;
        else
          fixedInput[prop] = this.defaultValues[prop];
      }
    }
  }

  return fixedInput;
};

// Update the documentation at the top of this module if you add or
// remove properties.
DefineTransaction.defineInputProps(["uri", "feedURI", "siteURI"],
                                   DefineTransaction.isURI, null);
DefineTransaction.defineInputProps(["GUID", "parentGUID", "newParentGUID"],
                                   DefineTransaction.isGUID);
DefineTransaction.defineInputProps(["title", "keyword", "postData"],
                                   DefineTransaction.isStr, "");
DefineTransaction.defineInputProps(["index", "newIndex"],
                                   DefineTransaction.isIndex,
                                   PlacesUtils.bookmarks.DEFAULT_INDEX);
DefineTransaction.defineInputProps(["annotationObject"],
                                   DefineTransaction.isAnnotationObject);
DefineTransaction.defineArrayInputProp("tags",
                                       DefineTransaction.isStr, null);
DefineTransaction.defineArrayInputProp("annotations",
                                       DefineTransaction.isAnnotationObject,
                                       null);

/**
 * Internal helper for implementing the execute method of NewBookmark, NewFolder
 * and NewSeparator.
 *
 * @param aTransaction
 *        The transaction object
 * @param aParentGUID
 *        The guid of the parent folder
 * @param aCreateItemFunction(aParentId, aGUIDToRestore)
 *        The function to be called for creating the item on execute and redo.
 *        It should return the itemId for the new item
 *        - aGUIDToRestore - the GUID to set for the item (used for redo).
 * @param [optional] aOnUndo
 *        an additional function to call after undo
 * @param [optional] aOnRedo
 *        an additional function to call after redo
 */
function* ExecuteCreateItem(aTransaction, aParentGUID, aCreateItemFunction,
                            aOnUndo = null, aOnRedo = null) {
  let parentId = yield PlacesUtils.promiseItemId(aParentGUID),
      itemId = yield aCreateItemFunction(parentId, ""),
      guid = yield PlacesUtils.promiseItemGUID(itemId);

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
    parentId = yield PlacesUtils.promiseItemId(aParentGUID);
    itemId = yield aCreateItemFunction(parentId, guid);
    if (aOnRedo)
      yield aOnRedo();

    // aOnRedo is called first to make sure it doesn't override
    // lastModified.
    PlacesUtils.bookmarks.setItemDateAdded(itemId, dateAdded);
    PlacesUtils.bookmarks.setItemLastModified(itemId, lastModified);
  };
  return guid;
}

/*****************************************************************************
 * The Standard Places Transactions.
 *
 * See the documentation at the top of this file. The valid values for input
 * are also documented there.
 *****************************************************************************/

let PT = PlacesTransactions;

/**
 * Transaction for creating a bookmark.
 *
 * Required Input Properties: uri, parentGUID.
 * Optional Input Properties: index, title, keyword, annotations, tags.
 *
 * When this transaction is executed, it's resolved to the new bookmark's GUID.
 */
PT.NewBookmark = DefineTransaction(["parentGUID", "uri"],
                                   ["index", "title", "keyword", "postData",
                                    "annotations", "tags"]);
PT.NewBookmark.prototype = Object.seal({
  execute: function (aParentGUID, aURI, aIndex, aTitle,
                     aKeyword, aPostData, aAnnos, aTags) {
    return ExecuteCreateItem(this, aParentGUID,
      function (parentId, guidToRestore = "") {
        let itemId = PlacesUtils.bookmarks.insertBookmark(
          parentId, aURI, aIndex, aTitle, guidToRestore);
        if (aKeyword)
          PlacesUtils.bookmarks.setKeywordForBookmark(itemId, aKeyword);
        if (aPostData)
          PlacesUtils.setPostDataForBookmark(itemId, aPostData);
        if (aAnnos)
          PlacesUtils.setAnnotationsForItem(itemId, aAnnos);
        if (aTags && aTags.length > 0) {
          let currentTags = PlacesUtils.tagging.getTagsForURI(aURI);
          aTags = [t for (t of aTags) if (currentTags.indexOf(t) == -1)];
          PlacesUtils.tagging.tagURI(aURI, aTags);
        }

        return itemId;
      },
      function _additionalOnUndo() {
        if (aTags && aTags.length > 0)
          PlacesUtils.tagging.untagURI(aURI, aTags);
      });
  }
});

/**
 * Transaction for creating a folder.
 *
 * Required Input Properties: title, parentGUID.
 * Optional Input Properties: index, annotations.
 *
 * When this transaction is executed, it's resolved to the new folder's GUID.
 */
PT.NewFolder = DefineTransaction(["parentGUID", "title"],
                                 ["index", "annotations"]);
PT.NewFolder.prototype = Object.seal({
  execute: function (aParentGUID, aTitle, aIndex, aAnnos) {
    return ExecuteCreateItem(this,  aParentGUID,
      function(parentId, guidToRestore = "") {
        let itemId = PlacesUtils.bookmarks.createFolder(
          parentId, aTitle, aIndex, guidToRestore);
        if (aAnnos)
          PlacesUtils.setAnnotationsForItem(itemId, aAnnos);
        return itemId;
      });
  }
});

/**
 * Transaction for creating a separator.
 *
 * Required Input Properties: parentGUID.
 * Optional Input Properties: index.
 *
 * When this transaction is executed, it's resolved to the new separator's
 * GUID.
 */
PT.NewSeparator = DefineTransaction(["parentGUID"], ["index"]);
PT.NewSeparator.prototype = Object.seal({
  execute: function (aParentGUID, aIndex) {
    return ExecuteCreateItem(this, aParentGUID,
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
 * Required Input Properties: feedURI, title, parentGUID.
 * Optional Input Properties: siteURI, index, annotations.
 *
 * When this transaction is executed, it's resolved to the new separators's
 * GUID.
 */
PT.NewLivemark = DefineTransaction(["feedURI", "title", "parentGUID"],
                                   ["siteURI", "index", "annotations"]);
PT.NewLivemark.prototype = Object.seal({
  execute: function* (aFeedURI, aTitle, aParentGUID, aSiteURI, aIndex, aAnnos) {
    let createItem = function* (aGUID = "") {
      let parentId = yield PlacesUtils.promiseItemId(aParentGUID);
      let livemarkInfo = {
        title: aTitle
      , feedURI: aFeedURI
      , parentId: parentId
      , index: aIndex
      , siteURI: aSiteURI };
      if (aGUID)
        livemarkInfo.guid = aGUID;

      let livemark = yield PlacesUtils.livemarks.addLivemark(livemarkInfo);
      if (aAnnos)
        PlacesUtils.setAnnotationsForItem(livemark.id, aAnnos);

      return livemark;
    };

    let guid = (yield createItem()).guid;
    this.undo = function* () {
      yield PlacesUtils.livemarks.removeLivemark({ guid: guid });
    };
    this.redo = function* () {
      yield createItem(guid);
    };
    return guid;
  }
});

/**
 * Transaction for moving an item.
 *
 * Required Input Properties: GUID, newParentGUID, newIndex.
 */
PT.MoveItem = DefineTransaction(["GUID", "newParentGUID", "newIndex"]);
PT.MoveItem.prototype = Object.seal({
  execute: function* (aGUID, aNewParentGUID, aNewIndex) {
    let itemId = yield PlacesUtils.promiseItemId(aGUID),
        oldParentId = PlacesUtils.bookmarks.getFolderIdForItem(itemId),
        oldIndex = PlacesUtils.bookmarks.getItemIndex(itemId),
        newParentId = yield PlacesUtils.promiseItemId(aNewParentGUID);

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
 * Required Input Properties: GUID, title.
 */
PT.EditTitle = DefineTransaction(["GUID", "title"]);
PT.EditTitle.prototype = Object.seal({
  execute: function* (aGUID, aTitle) {
    let itemId = yield PlacesUtils.promiseItemId(aGUID),
        oldTitle = PlacesUtils.bookmarks.getItemTitle(itemId);
    PlacesUtils.bookmarks.setItemTitle(itemId, aTitle);
    this.undo = () => { PlacesUtils.bookmarks.setItemTitle(itemId, oldTitle); };
  }
});

/**
 * Transaction for setting the URI for an item.
 *
 * Required Input Properties: GUID, uri.
 */
PT.EditURI = DefineTransaction(["GUID", "uri"]);
PT.EditURI.prototype = Object.seal({
  execute: function* (aGUID, aURI) {
    let itemId = yield PlacesUtils.promiseItemId(aGUID),
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
      newURIAdditionalTags = [t for (t of oldURITags)
                              if (currentNewURITags.indexOf(t) == -1)];
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
 * Transaction for setting an annotation for an item.
 *
 * Required Input Properties: GUID, annotationObject
 */
PT.SetItemAnnotation = DefineTransaction(["GUID", "annotationObject"]);
PT.SetItemAnnotation.prototype = {
  execute: function* (aGUID, aAnno) {
    let itemId = yield PlacesUtils.promiseItemId(aGUID), oldAnno;
    if (PlacesUtils.annotations.itemHasAnnotation(itemId, aAnno.name)) {
      // Fill the old anno if it is set.
      let flags = {}, expires = {};
      PlacesUtils.annotations.getItemAnnotationInfo(itemId, aAnno.name, flags,
                                                    expires, { });
      let value = PlacesUtils.annotations.getItemAnnotation(itemId, aAnno.name);
      oldAnno = { name: aAnno.name, flags: flags.value,
                  value: value, expires: expires.value };
    }
    else {
      // An unset value removes the annoation.
      oldAnno = { name: aAnno.name };
    }

    PlacesUtils.setAnnotationsForItem(itemId, [aAnno]);
    this.undo = () => { PlacesUtils.setAnnotationsForItem(itemId, [oldAnno]); };
  }
};

/**
 * Transaction for setting the keyword for a bookmark.
 *
 * Required Input Properties: GUID, keyword.
 */
PT.EditKeyword = DefineTransaction(["GUID", "keyword"]);
PT.EditKeyword.prototype = Object.seal({
  execute: function* (aGUID, aKeyword) {
    let itemId = yield PlacesUtils.promiseItemId(aGUID),
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
 * Required Input Properties: GUID.
 */
PT.SortByName = DefineTransaction(["GUID"]);
PT.SortByName.prototype = {
  execute: function* (aGUID) {
    let itemId = yield PlacesUtils.promiseItemId(aGUID),
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
 * Required Input Properties: GUID.
 */
PT.RemoveItem = DefineTransaction(["GUID"]);
PT.RemoveItem.prototype = {
  execute: function* (aGUID) {
    const bms = PlacesUtils.bookmarks;

    let itemsToRestoreOnUndo = [];
    function* saveItemRestoreData(aItem, aNode = null) {
      if (!aItem || !aItem.GUID)
        throw new Error("invalid item object");

      let itemId = aNode ?
                   aNode.itemId : yield PlacesUtils.promiseItemId(aItem.GUID);
      if (itemId == -1)
        throw new Error("Unexpected non-bookmarks node");

      aItem.itemType = function() {
        if (aNode) {
          switch (aNode.type) {
            case aNode.RESULT_TYPE_SEPARATOR:
              return bms.TYPE_SEPARATOR;
            case aNode.RESULT_TYPE_URI:   // regular bookmarks
            case aNode.RESULT_TYPE_FOLDER_SHORTCUT:  // place:folder= bookmarks
            case aNode.RESULT_TYPE_QUERY: // smart bookmarks
              return bms.TYPE_BOOKMARK;
            case aNode.RESULT_TYPE_FOLDER:
              return bms.TYPE_FOLDER;
            default:
              throw new Error("Unexpected node type");
          }
        }
        return bms.getItemType(itemId);
      }();

      let node = aNode;
      if (!node && aItem.itemType == bms.TYPE_FOLDER)
        node = PlacesUtils.getFolderContents(itemId).root;

      // dateAdded, lastModified and annotations apply to all types.
      aItem.dateAdded = node ? node.dateAdded : bms.getItemDateAdded(itemId);
      aItem.lastModified = node ?
                           node.lastModified : bms.getItemLastModified(itemId);
      aItem.annotations = PlacesUtils.getAnnotationsForItem(itemId);

      // For the first-level item, we don't have the parent.
      if (!aItem.parentGUID) {
        let parentId     = PlacesUtils.bookmarks.getFolderIdForItem(itemId);
        aItem.parentGUID = yield PlacesUtils.promiseItemGUID(parentId);
        // For the first-level item, we also need the index.
        // Note: node.bookmarkIndex doesn't work for root nodes.
        aItem.index      = bms.getItemIndex(itemId);
      }

      // Separators don't have titles.
      if (aItem.itemType != bms.TYPE_SEPARATOR) {
        aItem.title = node ? node.title : bms.getItemTitle(itemId);

        if (aItem.itemType == bms.TYPE_BOOKMARK) {
          aItem.uri =
            node ? NetUtil.newURI(node.uri) : bms.getBookmarkURI(itemId);
          aItem.keyword = PlacesUtils.bookmarks.getKeywordForBookmark(itemId);

          // This may be the last bookmark (excluding the tag-items themselves)
          // for the URI, so we need to preserve the tags.
          let tags = PlacesUtils.tagging.getTagsForURI(aItem.uri);;
          if (tags.length > 0)
            aItem.tags = tags;
        }
        else { // folder
          // We always have the node for folders
          aItem.readOnly = node.childrenReadOnly;
          for (let i = 0; i < node.childCount; i++) {
            let childNode = node.getChild(i);
            let childItem =
              { GUID: yield PlacesUtils.promiseItemGUID(childNode.itemId)
              , parentGUID: aItem.GUID };
            itemsToRestoreOnUndo.push(childItem);
            yield saveItemRestoreData(childItem, childNode);
          }
          node.containerOpen = false;
        }
      }
    }

    let item = { GUID: aGUID, parentGUID: null };
    itemsToRestoreOnUndo.push(item);
    yield saveItemRestoreData(item);

    let itemId = yield PlacesUtils.promiseItemId(aGUID);
    PlacesUtils.bookmarks.removeItem(itemId);
    this.undo = function() {
      for (let item of itemsToRestoreOnUndo) {
        let parentId = yield PlacesUtils.promiseItemId(item.parentGUID);
        let index = "index" in item ?
                    index : PlacesUtils.bookmarks.DEFAULT_INDEX;
        let itemId;
        if (item.itemType == bms.TYPE_SEPARATOR) {
          itemId = bms.insertSeparator(parentId, index, item.GUID);
        }
        else if (item.itemType == bms.TYPE_BOOKMARK) {
          itemId = bms.insertBookmark(parentId, item.uri, index, item.title,
                                       item.GUID);
        }
        else { // folder
          itemId = bms.createFolder(parentId, item.title, index, item.GUID);
        }

        if (item.itemType == bms.TYPE_BOOKMARK) {
          if (item.keyword)
            bms.setKeywordForBookmark(itemId, item.keyword);
          if ("tags" in item)
            PlacesUtils.tagging.tagURI(item.uri, item.tags);
        }
        else if (item.readOnly === true) {
          bms.setFolderReadonly(itemId, true);
        }

        PlacesUtils.setAnnotationsForItem(itemId, item.annotations);
        PlacesUtils.bookmarks.setItemDateAdded(itemId, item.dateAdded);
        PlacesUtils.bookmarks.setItemLastModified(itemId, item.lastModified);
      }
    };
  }
};

/**
 * Transaction for tagging a URI.
 *
 * Required Input Properties: uri, tags.
 */
PT.TagURI = DefineTransaction(["uri", "tags"]);
PT.TagURI.prototype = {
  execute: function* (aURI, aTags) {
    if (PlacesUtils.getMostRecentBookmarkForURI(aURI) == -1) {
      // Tagging is only allowed for bookmarked URIs.
      let unfileGUID =
        yield PlacesUtils.promiseItemGUID(PlacesUtils.unfiledBookmarksFolderId);
      let createTxn = TransactionsHistory.getRawTransaction(
        PT.NewBookmark({ uri: aURI, tags: aTags, parentGUID: unfileGUID }));
      yield createTxn.execute();
      this.undo = createTxn.undo.bind(createTxn);
      this.redo = createTxn.redo.bind(createTxn);
    }
    else {
      let currentTags = PlacesUtils.tagging.getTagsForURI(aURI);
      let newTags = [t for (t of aTags) if (currentTags.indexOf(t) == -1)];
      PlacesUtils.tagging.tagURI(aURI, newTags);
      this.undo = () => { PlacesUtils.tagging.untagURI(aURI, newTags); };
      this.redo = () => { PlacesUtils.tagging.tagURI(aURI, newTags); };
    }
  }
};

/**
 * Transaction for removing tags from a URI.
 *
 * Required Input Properties: uri.
 * Optional Input Properties: tags.
 *
 * If |tags| is not set, all tags set for |uri| are removed.
 */
PT.UntagURI = DefineTransaction(["uri"], ["tags"]);
PT.UntagURI.prototype = {
  execute: function* (aURI, aTags) {
    let tagsSet = PlacesUtils.tagging.getTagsForURI(aURI);

    if (aTags && aTags.length > 0)
      aTags = [t for (t of aTags) if (tagsSet.indexOf(t) != -1)];
    else
      aTags = tagsSet;

    PlacesUtils.tagging.untagURI(aURI, aTags);
    this.undo = () => { PlacesUtils.tagging.tagURI(aURI, aTags); };
    this.redo = () => { PlacesUtils.tagging.untagURI(aURI, aTags); };
  }
};
