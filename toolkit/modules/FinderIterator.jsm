/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["FinderIterator"];

const { interfaces: Ci, classes: Cc, utils: Cu } = Components;

Cu.import("resource://gre/modules/Task.jsm");

const kIterationSizeMax = 100;

/**
 * FinderIterator singleton. See the documentation for the `start()` method to
 * learn more.
 */
this.FinderIterator = {
  _currentParams: null,
  _listeners: new Map(),
  _catchingUp: new Set(),
  _previousParams: null,
  _previousRanges: [],
  _spawnId: 0,
  ranges: [],
  running: false,

  // Expose `kIterationSizeMax` to the outside world for unit tests to use.
  get kIterationSizeMax() { return kIterationSizeMax },

  get params() {
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
   * Results are also passed to the `onRange` callback method, along with a flag
   * that specifies if the result comes from the cache or is fresh. The callback
   * also adheres to the `limit` flag.
   * The returned promise is resolved when 1) the limit is reached, 2) when all
   * the ranges have been found or 3) when `stop()` is called whilst iterating.
   *
   * @param {Finder}   options.finder      Currently active Finder instance
   * @param {Number}   [options.limit]     Limit the amount of results to be
   *                                       passed back. Optional, defaults to no
   *                                       limit.
   * @param {Boolean}  [options.linksOnly] Only yield ranges that are inside a
   *                                       hyperlink (used by QuickFind).
   *                                       Optional, defaults to `false`.
   * @param {Function} options.onRange     Callback invoked when a range is found
   * @param {Boolean}  [options.useCache]  Whether to allow results already
   *                                       present in the cache or demand fresh.
   *                                       Optional, defaults to `false`.
   * @param {String}   options.word        Word to search for
   * @return {Promise}
   */
  start({ finder, limit, linksOnly, onRange, useCache, word }) {
    // Take care of default values for non-required options.
    if (typeof limit != "number")
      limit = -1;
    if (typeof linksOnly != "boolean")
      linksOnly = false;
    if (typeof useCache != "boolean")
      useCache = false;

    // Validate the options.
    if (!finder)
      throw new Error("Missing required option 'finder'");
    if (!word)
      throw new Error("Missing required option 'word'");
    if (typeof onRange != "function")
      throw new TypeError("Missing valid, required option 'onRange'");

    // Don't add the same listener twice.
    if (this._listeners.has(onRange))
      throw new Error("Already listening to iterator results");

    let window = finder._getWindow();
    let resolver;
    let promise = new Promise(resolve => resolver = resolve);
    let iterParams = { linksOnly, useCache, word };

    this._listeners.set(onRange, { limit, onEnd: resolver });

    // If we're not running anymore and we're requesting the previous result, use it.
    if (!this.running && this._previousResultAvailable(iterParams)) {
      this._yieldPreviousResult(onRange, window);
      return promise;
    }

    if (this.running) {
      // Double-check if we're not running the iterator with a different set of
      // parameters, otherwise throw an error with the most common reason.
      if (!this._areParamsEqual(this._currentParams, iterParams))
        throw new Error(`We're currently iterating over '${this._currentParams.word}', not '${word}'`);

      // if we're still running, yield the set we have built up this far.
      this._yieldIntermediateResult(onRange, window);

      return promise;
    }

    // Start!
    this.running = true;
    this._currentParams = iterParams;
    this._findAllRanges(finder, window, ++this._spawnId);

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
    if (!this.running)
      return;

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

    for (let [, { onEnd }] of this._listeners)
      onEnd();
    this._listeners.clear();
  },

  /**
   * Reset the internal state of the iterator. Typically this would be called
   * when the docShell is not active anymore, which makes the current and cached
   * previous result invalid.
   * If the iterator is running, it will be stopped as soon as possible.
   */
  reset() {
    this._catchingUp.clear();
    this._currentParams = this._previousParams = null;
    this._previousRanges = [];
    this.ranges = [];
    this.running = false;

    for (let [, { onEnd }] of this._listeners)
      onEnd();
    this._listeners.clear();
  },

  /**
   * Check if the currently running iterator parameters are the same as the ones
   * passed through the arguments. When `true`, we can keep it running as-is and
   * the consumer should stop the iterator when `false`.
   *
   * @param  {Boolean} options.linksOnly Whether to search for the word to be
   *                                     present in links only
   * @param  {String}  options.word      The word being searched for
   * @return {Boolean}
   */
  continueRunning({ linksOnly, word }) {
    return (this.running &&
      this._currentParams.linksOnly === linksOnly &&
      this._currentParams.word == word);
  },

  /**
   * Internal; check if an iteration request is available in the previous result
   * that we cached.
   *
   * @param  {Boolean} options.linksOnly Whether to search for the word to be
   *                                     present in links only
   * @param  {Boolean} options.useCache  Whether the consumer wants to use the
   *                                     cached previous result at all
   * @param  {String}  options.word      The word being searched for
   * @return {Boolean}
   */
  _previousResultAvailable({ linksOnly, useCache, word }) {
    return !!(useCache &&
      this._areParamsEqual(this._previousParams, { word, linksOnly }) &&
      this._previousRanges.length);
  },

  /**
   * Internal; compare if two sets of iterator parameters are equivalent.
   *
   * @param  {Object} paramSet1 First set of params (left hand side)
   * @param  {Object} paramSet2 Second set of params (right hand side)
   * @return {Boolean}
   */
  _areParamsEqual(paramSet1, paramSet2) {
    return (!!paramSet1 && !!paramSet2 &&
      paramSet1.linksOnly === paramSet2.linksOnly &&
      paramSet1.word == paramSet2.word);
  },

  /**
   * Internal; iterate over a predefined set of ranges that have been collected
   * before.
   * Also here, we make sure to pause every `kIterationSizeMax` iterations to
   * make sure we don't block the host process too long. In the case of a break
   * like this, we yield `undefined`, instead of a range.
   *
   * @param {Function}     onRange     Callback invoked when a range is found
   * @param {Array}        rangeSource Set of ranges to iterate over
   * @param {nsIDOMWindow} window      The window object is only really used
   *                                   for access to `setTimeout`
   * @yield {nsIDOMRange}
   */
  _yieldResult: function* (onRange, rangeSource, window) {
    // We keep track of the number of iterations to allow a short pause between
    // every `kIterationSizeMax` number of iterations.
    let iterCount = 0;
    let { limit, onEnd } = this._listeners.get(onRange);
    let ranges = rangeSource.slice(0, limit > -1 ? limit : undefined);
    for (let range of ranges) {
      try {
        range.startContainer;
      } catch (ex) {
        // Don't yield dead objects, so use the escape hatch.
        if (ex.message.includes("dead object"))
          return;
      }

      // Pass a flag that is `true` when we're returning the result from a
      // cached previous iteration.
      onRange(range, !this.running);
      yield range;

      if (++iterCount >= kIterationSizeMax) {
        iterCount = 0;
        // Make sure to save the current limit for later.
        this._listeners.set(onRange, { limit, onEnd });
        // Sleep for the rest of this cycle.
        yield new Promise(resolve => window.setTimeout(resolve, 0));
        // After a sleep, the set of ranges may have updated.
        ranges = rangeSource.slice(0, limit > -1 ? limit : undefined);
      }

      if (limit !== -1 && --limit === 0) {
        // We've reached our limit; no need to do more work.
        this._listeners.delete(onRange);
        onEnd();
        return;
      }
    }

    // Save the updated limit globally.
    this._listeners.set(onRange, { limit, onEnd });
  },

  /**
   * Internal; iterate over the set of previously found ranges. Meanwhile it'll
   * mark the listener as 'catching up', meaning it will not receive fresh
   * results from a running iterator.
   *
   * @param {Function}     onRange Callback invoked when a range is found
   * @param {nsIDOMWindow} window  The window object is only really used
   *                               for access to `setTimeout`
   * @yield {nsIDOMRange}
   */
  _yieldPreviousResult: Task.async(function* (onRange, window) {
    this._catchingUp.add(onRange);
    yield* this._yieldResult(onRange, this._previousRanges, window);
    this._catchingUp.delete(onRange);
    let { onEnd } = this._listeners.get(onRange);
    if (onEnd) {
      onEnd();
      this._listeners.delete(onRange);
    }
  }),

  /**
   * Internal; iterate over the set of already found ranges. Meanwhile it'll
   * mark the listener as 'catching up', meaning it will not receive fresh
   * results from the running iterator.
   *
   * @param {Function}     onRange Callback invoked when a range is found
   * @param {nsIDOMWindow} window  The window object is only really used
   *                               for access to `setTimeout`
   * @yield {nsIDOMRange}
   */
  _yieldIntermediateResult: Task.async(function* (onRange, window) {
    this._catchingUp.add(onRange);
    yield* this._yieldResult(onRange, this.ranges, window);
    this._catchingUp.delete(onRange);
  }),

  /**
   * Internal; see the documentation of the start() method above.
   *
   * @param {Finder}       finder  Currently active Finder instance
   * @param {nsIDOMWindow} window  The window to search in
   * @param {Number}       spawnId Since `stop()` is synchronous and this method
   *                               is not, this identifier is used to learn if
   *                               it's supposed to still continue after a pause.
   * @yield {nsIDOMRange}
   */
  _findAllRanges: Task.async(function* (finder, window, spawnId) {
    // First we collect all frames we need to search through, whilst making sure
    // that the parent window gets dibs.
    let frames = [window].concat(this._collectFrames(window, finder));
    let { linksOnly, word } = this._currentParams;
    let iterCount = 0;
    for (let frame of frames) {
      for (let range of this._iterateDocument(word, frame, finder)) {
        // Between iterations, for example after a sleep of one cycle, we could
        // have gotten the signal to stop iterating. Make sure we do here.
        if (!this.running || spawnId !== this._spawnId)
          return;

        // Deal with links-only mode here.
        if (linksOnly && this._rangeStartsInLink(range))
          continue;

        this.ranges.push(range);

        // Call each listener with the range we just found.
        for (let [onRange, { limit, onEnd }] of this._listeners) {
          if (this._catchingUp.has(onRange))
            continue;

          onRange(range);

          if (limit !== -1 && --limit === 0) {
            // We've reached our limit; no need to do more work for this listener.
            this._listeners.delete(onRange);
            onEnd();
            continue;
          }

          // Save the updated limit globally.
          this._listeners.set(onRange, { limit, onEnd });
        }

        yield range;

        if (++iterCount >= kIterationSizeMax) {
          iterCount = 0;
          // Sleep for the rest of this cycle.
          yield new Promise(resolve => window.setTimeout(resolve, 0));
        }
      }
    }

    // When the iterating has finished, make sure we reset and save the state
    // properly.
    this.stop(true);
  }),

  /**
   * Internal; basic wrapper around nsIFind that provides a generator yielding
   * a range each time an occurence of `word` string is found.
   *
   * @param {String}       word   The word to search for
   * @param {nsIDOMWindow} window The window to search in
   * @param {Finder}       finder The Finder instance
   * @yield {nsIDOMRange}
   */
  _iterateDocument: function* (word, window, finder) {
    let doc = window.document;
    let body = (doc instanceof Ci.nsIDOMHTMLDocument && doc.body) ?
               doc.body : doc.documentElement;

    if (!body)
      return;

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
    nsIFind.caseSensitive = finder._fastFind.caseSensitive;
    nsIFind.entireWord = finder._fastFind.entireWord;

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
    if (!window.frames || !window.frames.length)
      return frames;

    // Casting `window.frames` to an Iterator doesn't work, so we're stuck with
    // a plain, old for-loop.
    for (let i = 0, l = window.frames.length; i < l; ++i) {
      let frame = window.frames[i];
      // Don't count matches in hidden frames.
      let frameEl = frame && frame.frameElement;
      if (!frameEl)
        continue;
      // Construct a range around the frame element to check its visiblity.
      let range = window.document.createRange();
      range.setStart(frameEl, 0);
      range.setEnd(frameEl, 0);
      if (!finder._fastFind.isRangeVisible(range, this._getDocShell(range), true))
        continue;
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
   * @param  {nsIDOMRange} windowOrRange Window object to query. May also be a
   *                                     Range, from which the owner window will
   *                                     be queried.
   * @return {nsIDocShell}
   */
  _getDocShell(windowOrRange) {
    let window = windowOrRange;
    // Ranges may also be passed in, so fetch its window.
    if (windowOrRange instanceof Ci.nsIDOMRange)
      window = windowOrRange.startContainer.ownerDocument.defaultView;
    return window.QueryInterface(Ci.nsIInterfaceRequestor)
                 .getInterface(Ci.nsIWebNavigation)
                 .QueryInterface(Ci.nsIDocShell);
  },

  /**
   * Internal; determines whether a range is inside a link.
   *
   * @param  {nsIDOMRange} range the range to check
   * @return {Boolean}     True if the range starts in a link
   */
  _rangeStartsInLink(range) {
    let isInsideLink = false;
    let node = range.startContainer;

    if (node.nodeType == node.ELEMENT_NODE) {
      if (node.hasChildNodes) {
        let childNode = node.item(range.startOffset);
        if (childNode)
          node = childNode;
      }
    }

    const XLink_NS = "http://www.w3.org/1999/xlink";
    const HTMLAnchorElement = (node.ownerDocument || node).defaultView.HTMLAnchorElement;
    do {
      if (node instanceof HTMLAnchorElement) {
        isInsideLink = node.hasAttribute("href");
        break;
      } else if (typeof node.hasAttributeNS == "function" &&
                 node.hasAttributeNS(XLink_NS, "href")) {
        isInsideLink = (node.getAttributeNS(XLink_NS, "type") == "simple");
        break;
      }

      node = node.parentNode;
    } while (node);

    return isInsideLink;
  }
};
