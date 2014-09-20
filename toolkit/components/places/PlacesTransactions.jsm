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
 * when result nodes are used (see nsINavHistoryResultNode::bookmarkGuid).
 * If you only have item-ids in hand, use PlacesUtils.promiseItemGuid for
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
 *  - tag - a string.
 *  - tags: an array of strings.
 *  - guid, parentGuid, newParentGuid: a valid places GUID string.
 *  - title: a string
 *  - index, newIndex: the position of an item in its containing folder,
 *    starting from 0.
 *    integer and PlacesUtils.bookmarks.DEFAULT_INDEX
 *  - annotation: see PlacesUtils.setAnnotationsForItem
 *  - annotations: an array of annotation objects as above.
 *  - excludingAnnotation: a string (annotation name).
 *  - excludingAnnotations: an array of string (annotation names).
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
   *        Either a transaction object or an array of transaction objects, or a
   *        generator function (ES6-style only) that yields transaction objects.
   *
   *        Generator mode how-to: when a transaction is yielded, it's executed.
   *        Then, if it was executed successfully, the resolution of |execute|
   *        is sent to the generator.  If |execute| threw or rejected, the
   *        exception is propagated to the generator.
   *        Any other value yielded by a generator function is handled the
   *        same way as in a Task (see Task.jsm).
   *
   * @return {Promise}
   * @resolves either to the resolution of |execute|, in single-transaction
   * mode, or to the return value of the generator, in generator-mode. For an
   * array of transactions, there's no resolution value.
   *
   * @rejects either if |execute| threw, in single-transaction mode, or if
   * the generator function threw (or didn't handle) an exception, in generator
   * mode.
   * @throws if aTransactionOrGeneratorFunction is neither a transaction object
   * created by this module, nor an array of such object, nor a generator
   * function.
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
    if (Array.isArray(aToTransact)) {
      if (aToTransact.some(
           o => !TransactionsHistory.isProxifiedTransactionObject(o))) {
        throw new Error("aToTransact contains non-transaction element");
      }
      // Proceed as if a generator yielding the transactions was passed in.
      return this.transact(function* () {
        for (let t of aToTransact) {
          yield t;
        }
      });
    }

    let isGeneratorObj =
      o => Object.prototype.toString.call(o) ==  "[object Generator]";

    let generator = null;
    if (typeof(aToTransact) == "function") {
      generator = aToTransact();
      if (!isGeneratorObj(generator))
        throw new Error("aToTransact is not a generator function");
    }
    else if (!TransactionsHistory.isProxifiedTransactionObject(aToTransact)) {
      throw new Error("aToTransact is not a valid transaction object");
    }
    else if (executedTransactions.has(aToTransact)) {
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
          if (isGeneratorObj(sendValue)) {
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

      if (generator)
        return yield transactBatch(generator);
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
DefineTransaction.isStrOrNull = v => typeof(v) == "string" || v === null;
DefineTransaction.isURI = v => v instanceof Components.interfaces.nsIURI;
DefineTransaction.isIndex = v => Number.isInteger(v) &&
                                 v >= PlacesUtils.bookmarks.DEFAULT_INDEX;
DefineTransaction.isGuid = v => /^[a-zA-Z0-9\-_]{12}$/.test(v);
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
function (aNames, aValidationFunction,
          aDefaultValue, aTransformFunction = null) {
  for (let name of aNames) {
    // Workaround bug 449811.
    let propName = name;
    this.inputProps.set(propName, {
      validateValue: function (aValue) {
        if (aValue === undefined)
          return aDefaultValue;
        if (!aValidationFunction(aValue))
          throw new Error(`Invalid value for input property ${propName}`);
        return aTransformFunction ? aTransformFunction(aValue) : aValue;
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
      return [for (e of aValue) baseProp.validateValue(e)];
    },

    // We allow setting either the array property itself (e.g. uris), or a
    // single element of it (uri, in that example), that is then transformed
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
    this.isPrimitive(aInput) ||
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
DefineTransaction.defineInputProps(["uri", "feedURI", "siteURI"],
                                   DefineTransaction.isURI, null);
DefineTransaction.defineInputProps(["guid", "parentGuid", "newParentGuid"],
                                   DefineTransaction.isGuid);
DefineTransaction.defineInputProps(["title"],
                                   DefineTransaction.isStrOrNull, null);
DefineTransaction.defineInputProps(["keyword", "postData", "tag",
                                    "excludingAnnotation"],
                                   DefineTransaction.isStr, "");
DefineTransaction.defineInputProps(["index", "newIndex"],
                                   DefineTransaction.isIndex,
                                   PlacesUtils.bookmarks.DEFAULT_INDEX);
DefineTransaction.defineInputProps(["annotation"],
                                   DefineTransaction.isAnnotationObject);
DefineTransaction.defineArrayInputProp("uris", "uri");
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
        annos = [for(a of annos)
                 if (aExcludingAnnotations.indexOf(a.name) == -1) a];
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

let PT = PlacesTransactions;

/**
 * Transaction for creating a bookmark.
 *
 * Required Input Properties: uri, parentGuid.
 * Optional Input Properties: index, title, keyword, annotations, tags.
 *
 * When this transaction is executed, it's resolved to the new bookmark's GUID.
 */
PT.NewBookmark = DefineTransaction(["parentGuid", "uri"],
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
          aTags = [t for (t of aTags) if (currentTags.indexOf(t) == -1)];
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
 * Required Input Properties: feedURI, title, parentGuid.
 * Optional Input Properties: siteURI, index, annotations.
 *
 * When this transaction is executed, it's resolved to the new livemark's
 * GUID.
 */
PT.NewLivemark = DefineTransaction(["feedURI", "title", "parentGuid"],
                                   ["siteURI", "index", "annotations"]);
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
 * Required Input Properties: guid, uri.
 */
PT.EditURI = DefineTransaction(["guid", "uri"]);
PT.EditURI.prototype = Object.seal({
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
 * Transaction for setting annotations for an item.
 *
 * Required Input Properties: guid, annotationObject
 */
PT.Annotate = DefineTransaction(["guid", "annotations"]);
PT.Annotate.prototype = {
  execute: function* (aGuid, aNewAnnos) {
    let itemId = yield PlacesUtils.promiseItemId(aGuid);
    let currentAnnos = PlacesUtils.getAnnotationsForItem(itemId);
    let undoAnnos = [];
    for (let newAnno of aNewAnnos) {
      let currentAnno = currentAnnos.find( a => a.name == newAnno.name );
      if (!!currentAnno) {
        undoAnnos.push(currentAnno);
      }
      else {
        // An unset value removes the annotation.
        undoAnnos.push({ name: newAnno.name });
      }
    }

    PlacesUtils.setAnnotationsForItem(itemId, aNewAnnos);
    this.undo = () => {
      PlacesUtils.setAnnotationsForItem(itemId, undoAnnos);
    };
    this.redo = () => {
      PlacesUtils.setAnnotationsForItem(itemId, aNewAnnos);
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
 * Required Input Properties: guid.
 */
PT.Remove = DefineTransaction(["guid"]);
PT.Remove.prototype = {
  execute: function* (aGuid) {
    const bms = PlacesUtils.bookmarks;

    let itemInfo = null;
    try {
      itemInfo = yield PlacesUtils.promiseBookmarksTree(aGuid);
    }
    catch(ex) {
      throw new Error("Failed to get info for the specified item (guid: " +
                      aGuid + "). Ex: " + ex);
    }
    PlacesUtils.bookmarks.removeItem(yield PlacesUtils.promiseItemId(aGuid));
    this.undo = createItemsFromBookmarksTree.bind(null, itemInfo, true);
  }
};

/**
 * Transaction for tagging a URI.
 *
 * Required Input Properties: uris, tags.
 */
PT.Tag = DefineTransaction(["uris", "tags"]);
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
        let unfiledGuid =
          yield PlacesUtils.promiseItemGuid(PlacesUtils.unfiledBookmarksFolderId);
        let createTxn = TransactionsHistory.getRawTransaction(
          PT.NewBookmark({ uri: currentURI
                         , tags: aTags, parentGuid: unfiledGuid }));
        yield createTxn.execute();
        onUndo.unshift(createTxn.undo.bind(createTxn));
        onRedo.push(createTxn.redo.bind(createTxn));
      }
      else {
        let currentTags = PlacesUtils.tagging.getTagsForURI(currentURI);
        let newTags = [t for (t of aTags) if (currentTags.indexOf(t) == -1)];
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
 * Required Input Properties: uris.
 * Optional Input Properties: tags.
 *
 * If |tags| is not set, all tags set for |uri| are removed.
 */
PT.Untag = DefineTransaction(["uris"], ["tags"]);
PT.Untag.prototype = {
  execute: function* (aURIs, aTags) {
    let onUndo = [], onRedo = [];
    for (let uri of aURIs) {
      // Workaround bug 449811.
      let currentURI = uri;
      let tagsToRemove;
      let tagsSet = PlacesUtils.tagging.getTagsForURI(currentURI);
      if (aTags.length > 0)
        tagsToRemove = [t for (t of aTags) if (tagsSet.indexOf(t) != -1)];
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
    catch(ex) {
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
