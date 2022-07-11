/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["FinderIterator"];

const { clearTimeout, setTimeout } = ChromeUtils.import(
  "resource://gre/modules/Timer.jsm"
);

const lazy = {};

ChromeUtils.defineModuleGetter(lazy, "NLP", "resource://gre/modules/NLP.jsm");
ChromeUtils.defineModuleGetter(
  lazy,
  "Rect",
  "resource://gre/modules/Geometry.jsm"
);

const kDebug = false;
const kIterationSizeMax = 100;
const kTimeoutPref = "findbar.iteratorTimeout";

/**
 * FinderIterator. See the documentation for the `start()` method to
 * learn more.
 */
function FinderIterator() {
  this._listeners = new Map();
  this._currentParams = null;
  this._catchingUp = new Set();
  this._previousParams = null;
  this._previousRanges = [];
  this._spawnId = 0;
  this._timer = null;
  this.ranges = [];
  this.running = false;
  this.useSubFrames = false;
}

FinderIterator.prototype = {
  _timeout: Services.prefs.getIntPref(kTimeoutPref),

  // Expose `kIterationSizeMax` to the outside world for unit tests to use.
  get kIterationSizeMax() {
    return kIterationSizeMax;
  },

  get params() {
    if (!this._currentParams && !this._previousParams) {
      return null;
    }
    return Object.assign({}, this._currentParams || this._previousParams);
  },

  /**
   * Start iterating the active Finder docShell, using the options below. When
   * it already started at the request of another consumer, we first yield the
   * results we already collected before continuing onward to yield fresh results.
   * We make sure to pause every `kIterationSizeMax` iterations to make sure we
   * don't block the host process too long. In the case of a break like this, we
   * yield `undefined`, instead of a range.
   * Upon re-entrance after a break, we check if `stop()` was called during the
   * break and if so, we stop iterating.
   * Results are also passed to the `listener.onIteratorRangeFound` callback
   * method, along with a flag that specifies if the result comes from the cache
   * or is fresh. The callback also adheres to the `limit` flag.
   * The returned promise is resolved when 1) the limit is reached, 2) when all
   * the ranges have been found or 3) when `stop()` is called whilst iterating.
   *
   * @param {Number}  [options.allowDistance] Allowed edit distance between the
   *                                          current word and `options.word`
   *                                          when the iterator is already running
   * @param {Boolean} options.caseSensitive   Whether to search in case sensitive
   *                                          mode
   * @param {Boolean} options.entireWord      Whether to search in entire-word mode
   * @param {Finder}  options.finder          Currently active Finder instance
   * @param {Number}  [options.limit]         Limit the amount of results to be
   *                                          passed back. Optional, defaults to no
   *                                          limit.
   * @param {Boolean} [options.linksOnly]     Only yield ranges that are inside a
   *                                          hyperlink (used by QuickFind).
   *                                          Optional, defaults to `false`.
   * @param {Object}  options.listener        Listener object that implements the
   *                                          following callback functions:
   *                                           - onIteratorRangeFound({Range} range);
   *                                           - onIteratorReset();
   *                                           - onIteratorRestart({Object} iterParams);
   *                                           - onIteratorStart({Object} iterParams);
   * @param {Boolean} options.matchDiacritics Whether to search in
   *                                          diacritic-matching mode
   * @param {Boolean} [options.useCache]        Whether to allow results already
   *                                            present in the cache or demand fresh.
   *                                            Optional, defaults to `false`.
   * @param {Boolean} [options.useSubFrames]    Whether to iterate over subframes.
   *                                            Optional, defaults to `false`.
   * @param {String}  options.word              Word to search for
   * @return {Promise}
   */
  start({
    allowDistance,
    caseSensitive,
    entireWord,
    finder,
    limit,
    linksOnly,
    listener,
    matchDiacritics,
    useCache,
    word,
    useSubFrames,
  }) {
    // Take care of default values for non-required options.
    if (typeof allowDistance != "number") {
      allowDistance = 0;
    }
    if (typeof limit != "number") {
      limit = -1;
    }
    if (typeof linksOnly != "boolean") {
      linksOnly = false;
    }
    if (typeof useCache != "boolean") {
      useCache = false;
    }
    if (typeof useSubFrames != "boolean") {
      useSubFrames = false;
    }

    // Validate the options.
    if (typeof caseSensitive != "boolean") {
      throw new Error("Missing required option 'caseSensitive'");
    }
    if (typeof entireWord != "boolean") {
      throw new Error("Missing required option 'entireWord'");
    }
    if (typeof matchDiacritics != "boolean") {
      throw new Error("Missing required option 'matchDiacritics'");
    }
    if (!finder) {
      throw new Error("Missing required option 'finder'");
    }
    if (!word) {
      throw new Error("Missing required option 'word'");
    }
    if (typeof listener != "object" || !listener.onIteratorRangeFound) {
      throw new TypeError("Missing valid, required option 'listener'");
    }

    // If the listener was added before, make sure the promise is resolved before
    // we replace it with another.
    if (this._listeners.has(listener)) {
      let { onEnd } = this._listeners.get(listener);
      if (onEnd) {
        onEnd();
      }
    }

    let window = finder._getWindow();
    let resolver;
    let promise = new Promise(resolve => (resolver = resolve));
    let iterParams = {
      caseSensitive,
      entireWord,
      linksOnly,
      matchDiacritics,
      useCache,
      window,
      word,
      useSubFrames,
    };

    this._listeners.set(listener, { limit, onEnd: resolver });

    // If we're not running anymore and we're requesting the previous result, use it.
    if (!this.running && this._previousResultAvailable(iterParams)) {
      this._yieldPreviousResult(listener, window);
      return promise;
    }

    if (this.running) {
      // Double-check if we're not running the iterator with a different set of
      // parameters, otherwise report an error with the most common reason.
      if (
        !this._areParamsEqual(this._currentParams, iterParams, allowDistance)
      ) {
        if (kDebug) {
          Cu.reportError(
            `We're currently iterating over '${this._currentParams.word}', not '${word}'\n` +
              new Error().stack
          );
        }
        this._listeners.delete(listener);
        resolver();
        return promise;
      }

      // if we're still running, yield the set we have built up this far.
      this._yieldIntermediateResult(listener, window);

      return promise;
    }

    // Start!
    this.running = true;
    this._currentParams = iterParams;
    this._findAllRanges(finder, ++this._spawnId);

    return promise;
  },

  /**
   * Stop the currently running iterator as soon as possible and optionally cache
   * the result for later.
   *
   * @param {Boolean} [cachePrevious] Whether to save the result for later.
   *                                  Optional.
   */
  stop(cachePrevious = false) {
    if (!this.running) {
      return;
    }

    if (this._timer) {
      clearTimeout(this._timer);
      this._timer = null;
    }
    if (this._runningFindResolver) {
      this._runningFindResolver();
      this._runningFindResolver = null;
    }

    if (cachePrevious) {
      this._previousRanges = [].concat(this.ranges);
      this._previousParams = Object.assign({}, this._currentParams);
    } else {
      this._previousRanges = [];
      this._previousParams = null;
    }

    this._catchingUp.clear();
    this._currentParams = null;
    this.ranges = [];
    this.running = false;

    for (let [, { onEnd }] of this._listeners) {
      onEnd();
    }
  },

  /**
   * Stops the iteration that currently running, if it is, and start a new one
   * with the exact same params as before.
   *
   * @param {Finder} finder Currently active Finder instance
   */
  restart(finder) {
    // Capture current iterator params before we stop the show.
    let iterParams = this.params;
    if (!iterParams) {
      return;
    }
    this.stop();

    // Restart manually.
    this.running = true;
    this._currentParams = iterParams;

    this._findAllRanges(finder, ++this._spawnId);
    this._notifyListeners("restart", iterParams);
  },

  /**
   * Reset the internal state of the iterator. Typically this would be called
   * when the docShell is not active anymore, which makes the current and cached
   * previous result invalid.
   * If the iterator is running, it will be stopped as soon as possible.
   */
  reset() {
    if (this._timer) {
      clearTimeout(this._timer);
      this._timer = null;
    }
    if (this._runningFindResolver) {
      this._runningFindResolver();
      this._runningFindResolver = null;
    }

    this._catchingUp.clear();
    this._currentParams = this._previousParams = null;
    this._previousRanges = [];
    this.ranges = [];
    this.running = false;

    this._notifyListeners("reset");
    for (let [, { onEnd }] of this._listeners) {
      onEnd();
    }
    this._listeners.clear();
  },

  /**
   * Check if the currently running iterator parameters are the same as the ones
   * passed through the arguments. When `true`, we can keep it running as-is and
   * the consumer should stop the iterator when `false`.
   *
   * @param {Boolean}  options.caseSensitive Whether to search in case sensitive
   *                                         mode
   * @param {Boolean}  options.entireWord    Whether to search in entire-word mode
   * @param  {Boolean} options.linksOnly     Whether to search for the word to be
   *                                         present in links only
   * @param {Boolean}  options.matchDiacritics Whether to search in
   *                                           diacritic-matching mode
   * @param  {String}  options.word          The word being searched for
   * @param  (Boolean) options.useSubFrames  Whether to search subframes
   * @return {Boolean}
   */
  continueRunning({
    caseSensitive,
    entireWord,
    linksOnly,
    matchDiacritics,
    word,
    useSubFrames,
  }) {
    return (
      this.running &&
      this._currentParams.caseSensitive === caseSensitive &&
      this._currentParams.entireWord === entireWord &&
      this._currentParams.linksOnly === linksOnly &&
      this._currentParams.matchDiacritics === matchDiacritics &&
      this._currentParams.word == word &&
      this._currentParams.useSubFrames == useSubFrames
    );
  },

  /**
   * The default mode of operation of the iterator is to not accept duplicate
   * listeners, resolve the promise of the older listeners and replace it with
   * the new listener.
   * Consumers may opt-out of this behavior by using this check and not call
   * start().
   *
   * @param  {Object} paramSet Property bag with the same signature as you would
   *                           pass into `start()`
   * @return {Boolean}
   */
  isAlreadyRunning(paramSet) {
    return (
      this.running &&
      this._areParamsEqual(this._currentParams, paramSet) &&
      this._listeners.has(paramSet.listener)
    );
  },

  /**
   * Safely notify all registered listeners that an event has occurred.
   *
   * @param {String}   callback    Name of the callback to invoke
   * @param {mixed}    [params]    Optional argument that will be passed to the
   *                               callback
   * @param {Iterable} [listeners] Set of listeners to notify. Optional, defaults
   *                               to `this._listeners.keys()`.
   */
  _notifyListeners(callback, params, listeners = this._listeners.keys()) {
    callback =
      "onIterator" + callback.charAt(0).toUpperCase() + callback.substr(1);
    for (let listener of listeners) {
      try {
        listener[callback](params);
      } catch (ex) {
        Cu.reportError("FinderIterator Error: " + ex);
      }
    }
  },

  /**
   * Internal; check if an iteration request is available in the previous result
   * that we cached.
   *
   * @param  {Boolean} options.caseSensitive Whether to search in case sensitive
   *                                         mode
   * @param  {Boolean} options.entireWord    Whether to search in entire-word mode
   * @param  {Boolean} options.linksOnly     Whether to search for the word to be
   *                                         present in links only
   * @param  {Boolean} options.matchDiacritics Whether to search in
   *                                           diacritic-matching mode
   * @param  {Boolean} options.useCache      Whether the consumer wants to use the
   *                                         cached previous result at all
   * @param  {String}  options.word          The word being searched for
   * @return {Boolean}
   */
  _previousResultAvailable({
    caseSensitive,
    entireWord,
    linksOnly,
    matchDiacritics,
    useCache,
    word,
  }) {
    return !!(
      useCache &&
      this._areParamsEqual(this._previousParams, {
        caseSensitive,
        entireWord,
        linksOnly,
        matchDiacritics,
        word,
      }) &&
      this._previousRanges.length
    );
  },

  /**
   * Internal; compare if two sets of iterator parameters are equivalent.
   *
   * @param  {Object} paramSet1       First set of params (left hand side)
   * @param  {Object} paramSet2       Second set of params (right hand side)
   * @param  {Number} [allowDistance] Allowed edit distance between the two words.
   *                                  Optional, defaults to '0', which means 'no
   *                                  distance'.
   * @return {Boolean}
   */
  _areParamsEqual(paramSet1, paramSet2, allowDistance = 0) {
    return (
      !!paramSet1 &&
      !!paramSet2 &&
      paramSet1.caseSensitive === paramSet2.caseSensitive &&
      paramSet1.entireWord === paramSet2.entireWord &&
      paramSet1.linksOnly === paramSet2.linksOnly &&
      paramSet1.matchDiacritics === paramSet2.matchDiacritics &&
      paramSet1.window === paramSet2.window &&
      paramSet1.useSubFrames === paramSet2.useSubFrames &&
      lazy.NLP.levenshtein(paramSet1.word, paramSet2.word) <= allowDistance
    );
  },

  /**
   * Internal; iterate over a predefined set of ranges that have been collected
   * before.
   * Also here, we make sure to pause every `kIterationSizeMax` iterations to
   * make sure we don't block the host process too long. In the case of a break
   * like this, we yield `undefined`, instead of a range.
   *
   * @param {Object}       listener    Listener object
   * @param {Array}        rangeSource Set of ranges to iterate over
   * @param {nsIDOMWindow} window      The window object is only really used
   *                                   for access to `setTimeout`
   * @param {Boolean}      [withPause] Whether to pause after each `kIterationSizeMax`
   *                                   number of ranges yielded. Optional, defaults
   *                                   to `true`.
   * @yield {Range}
   */
  async _yieldResult(listener, rangeSource, window, withPause = true) {
    // We keep track of the number of iterations to allow a short pause between
    // every `kIterationSizeMax` number of iterations.
    let iterCount = 0;
    let { limit, onEnd } = this._listeners.get(listener);
    let ranges = rangeSource.slice(0, limit > -1 ? limit : undefined);
    for (let range of ranges) {
      try {
        range.startContainer;
      } catch (ex) {
        // Don't yield dead objects, so use the escape hatch.
        if (ex.message.includes("dead object")) {
          return;
        }
      }

      // Pass a flag that is `true` when we're returning the result from a
      // cached previous iteration.
      listener.onIteratorRangeFound(range, !this.running);
      await range;

      if (withPause && ++iterCount >= kIterationSizeMax) {
        iterCount = 0;
        // Make sure to save the current limit for later.
        this._listeners.set(listener, { limit, onEnd });
        // Sleep for the rest of this cycle.
        await new Promise(resolve => window.setTimeout(resolve, 0));
        // After a sleep, the set of ranges may have updated.
        ranges = rangeSource.slice(0, limit > -1 ? limit : undefined);
      }

      if (limit !== -1 && --limit === 0) {
        // We've reached our limit; no need to do more work.
        this._listeners.delete(listener);
        onEnd();
        return;
      }
    }

    // Save the updated limit globally.
    this._listeners.set(listener, { limit, onEnd });
  },

  /**
   * Internal; iterate over the set of previously found ranges. Meanwhile it'll
   * mark the listener as 'catching up', meaning it will not receive fresh
   * results from a running iterator.
   *
   * @param {Object}       listener Listener object
   * @param {nsIDOMWindow} window   The window object is only really used
   *                                for access to `setTimeout`
   * @yield {Range}
   */
  async _yieldPreviousResult(listener, window) {
    this._notifyListeners("start", this.params, [listener]);
    this._catchingUp.add(listener);
    await this._yieldResult(listener, this._previousRanges, window);
    this._catchingUp.delete(listener);
    let { onEnd } = this._listeners.get(listener);
    if (onEnd) {
      onEnd();
    }
  },

  /**
   * Internal; iterate over the set of already found ranges. Meanwhile it'll
   * mark the listener as 'catching up', meaning it will not receive fresh
   * results from the running iterator.
   *
   * @param {Object}       listener Listener object
   * @param {nsIDOMWindow} window   The window object is only really used
   *                                for access to `setTimeout`
   * @yield {Range}
   */
  async _yieldIntermediateResult(listener, window) {
    this._notifyListeners("start", this.params, [listener]);
    this._catchingUp.add(listener);
    await this._yieldResult(listener, this.ranges, window, false);
    this._catchingUp.delete(listener);
  },

  /**
   * Internal; see the documentation of the start() method above.
   *
   * @param {Finder}       finder  Currently active Finder instance
   * @param {Number}       spawnId Since `stop()` is synchronous and this method
   *                               is not, this identifier is used to learn if
   *                               it's supposed to still continue after a pause.
   * @yield {Range}
   */
  async _findAllRanges(finder, spawnId) {
    if (this._timeout) {
      if (this._timer) {
        clearTimeout(this._timer);
      }
      if (this._runningFindResolver) {
        this._runningFindResolver();
      }

      let timeout = this._timeout;
      let searchTerm = this._currentParams.word;
      // Wait a little longer when the first or second character is typed into
      // the findbar.
      if (searchTerm.length == 1) {
        timeout *= 4;
      } else if (searchTerm.length == 2) {
        timeout *= 2;
      }
      await new Promise(resolve => {
        this._runningFindResolver = resolve;
        this._timer = setTimeout(resolve, timeout);
      });
      this._timer = this._runningFindResolver = null;
      // During the timeout, we could have gotten the signal to stop iterating.
      // Make sure we do here.
      if (!this.running || spawnId !== this._spawnId) {
        return;
      }
    }

    this._notifyListeners("start", this.params);

    let { linksOnly, useSubFrames, window } = this._currentParams;
    // First we collect all frames we need to search through, whilst making sure
    // that the parent window gets dibs.
    let frames = [window];
    if (useSubFrames) {
      frames.push(...this._collectFrames(window, finder));
    }
    let iterCount = 0;
    for (let frame of frames) {
      for (let range of this._iterateDocument(this._currentParams, frame)) {
        // Between iterations, for example after a sleep of one cycle, we could
        // have gotten the signal to stop iterating. Make sure we do here.
        if (!this.running || spawnId !== this._spawnId) {
          return;
        }

        // Deal with links-only mode here.
        if (linksOnly && !this._rangeStartsInLink(range)) {
          continue;
        }

        this.ranges.push(range);

        // Call each listener with the range we just found.
        for (let [listener, { limit, onEnd }] of this._listeners) {
          if (this._catchingUp.has(listener)) {
            continue;
          }

          listener.onIteratorRangeFound(range);

          if (limit !== -1 && --limit === 0) {
            // We've reached our limit; no need to do more work for this listener.
            this._listeners.delete(listener);
            onEnd();
            continue;
          }

          // Save the updated limit globally.
          this._listeners.set(listener, { limit, onEnd });
        }

        await range;

        if (++iterCount >= kIterationSizeMax) {
          iterCount = 0;
          // Sleep for the rest of this cycle.
          await new Promise(resolve => window.setTimeout(resolve, 0));
        }
      }
    }

    // When the iterating has finished, make sure we reset and save the state
    // properly.
    this.stop(true);
  },

  /**
   * Internal; basic wrapper around nsIFind that provides a generator yielding
   * a range each time an occurence of `word` string is found.
   *
   * @param {Boolean}      options.caseSensitive Whether to search in case
   *                                             sensitive mode
   * @param {Boolean}      options.entireWord    Whether to search in entire-word
   *                                             mode
   * @param {Boolean}      options.matchDiacritics Whether to search in
   *                                               diacritic-matching mode
   * @param {String}       options.word          The word to search for
   * @param {nsIDOMWindow} window                The window to search in
   * @yield {Range}
   */
  *_iterateDocument(
    { caseSensitive, entireWord, matchDiacritics, word },
    window
  ) {
    let doc = window.document;
    let body = doc.body || doc.documentElement;

    if (!body) {
      return;
    }

    let searchRange = doc.createRange();
    searchRange.selectNodeContents(body);

    let startPt = searchRange.cloneRange();
    startPt.collapse(true);

    let endPt = searchRange.cloneRange();
    endPt.collapse(false);

    let retRange = null;

    let nsIFind = Cc["@mozilla.org/embedcomp/rangefind;1"]
      .createInstance()
      .QueryInterface(Ci.nsIFind);
    nsIFind.caseSensitive = caseSensitive;
    nsIFind.entireWord = entireWord;
    nsIFind.matchDiacritics = matchDiacritics;

    while ((retRange = nsIFind.Find(word, searchRange, startPt, endPt))) {
      yield retRange;
      startPt = retRange.cloneRange();
      startPt.collapse(false);
    }
  },

  /**
   * Internal; helper method for the iterator that recursively collects all
   * visible (i)frames inside a window.
   *
   * @param  {nsIDOMWindow} window The window to extract the (i)frames from
   * @param  {Finder}       finder The Finder instance
   * @return {Array}        Stack of frames to iterate over
   */
  _collectFrames(window, finder) {
    let frames = [];
    if (!("frames" in window) || !window.frames.length) {
      return frames;
    }

    // Casting `window.frames` to an Iterator doesn't work, so we're stuck with
    // a plain, old for-loop.
    let dwu = window.windowUtils;
    for (let i = 0, l = window.frames.length; i < l; ++i) {
      let frame = window.frames[i];
      // Don't count matches in hidden frames; get the frame element rect and
      // check if it's empty. We shan't flush!
      let frameEl = frame && frame.frameElement;
      if (
        !frameEl ||
        lazy.Rect.fromRect(dwu.getBoundsWithoutFlushing(frameEl)).isEmpty()
      ) {
        continue;
      }
      // All conditions pass, so push the current frame and its children on the
      // stack.
      frames.push(frame, ...this._collectFrames(frame, finder));
    }

    return frames;
  },

  /**
   * Internal; helper method to extract the docShell reference from a Window or
   * Range object.
   *
   * @param  {Range} windowOrRange Window object to query. May also be a
   *                               Range, from which the owner window will
   *                               be queried.
   * @return {nsIDocShell}
   */
  _getDocShell(windowOrRange) {
    let window = windowOrRange;
    // Ranges may also be passed in, so fetch its window.
    if (ChromeUtils.getClassName(windowOrRange) === "Range") {
      window = windowOrRange.startContainer.ownerGlobal;
    }
    return window.docShell;
  },

  /**
   * Internal; determines whether a range is inside a link.
   *
   * @param  {Range} range the range to check
   * @return {Boolean}     True if the range starts in a link
   */
  _rangeStartsInLink(range) {
    let isInsideLink = false;
    let node = range.startContainer;

    if (node.nodeType == node.ELEMENT_NODE) {
      if (node.hasChildNodes) {
        let childNode = node.item(range.startOffset);
        if (childNode) {
          node = childNode;
        }
      }
    }

    const XLink_NS = "http://www.w3.org/1999/xlink";
    const HTMLAnchorElement = (node.ownerDocument || node).defaultView
      .HTMLAnchorElement;
    do {
      if (HTMLAnchorElement.isInstance(node)) {
        isInsideLink = node.hasAttribute("href");
        break;
      } else if (
        typeof node.hasAttributeNS == "function" &&
        node.hasAttributeNS(XLink_NS, "href")
      ) {
        isInsideLink = node.getAttributeNS(XLink_NS, "type") == "simple";
        break;
      }

      node = node.parentNode;
    } while (node);

    return isInsideLink;
  },
};
