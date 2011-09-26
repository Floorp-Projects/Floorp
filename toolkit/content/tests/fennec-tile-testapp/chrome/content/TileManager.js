// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/*
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Mobile Browser.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Roy Frostig <rfrostig@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const kXHTMLNamespaceURI  = "http://www.w3.org/1999/xhtml";

// base-2 exponent for width, height of a single tile.
const kTileExponentWidth  = 7;
const kTileExponentHeight = 7;
const kTileWidth  = Math.pow(2, kTileExponentWidth);   // 2^7 = 128
const kTileHeight = Math.pow(2, kTileExponentHeight);  // 2^7 = 128
const kLazyRoundTimeCap = 500;    // millis


function bind(f, thisObj) {
  return function() {
    return f.apply(thisObj, arguments);
  };
}

function bindSome(instance, methodNames) {
  for each (let methodName in methodNames)
    if (methodName in instance)
      instance[methodName] = bind(instance[methodName], instance);
}

function bindAll(instance) {
  for (let key in instance)
    if (instance[key] instanceof Function)
      instance[key] = bind(instance[key], instance);
}


/**
 * The Tile Manager!
 *
 * @param appendTile The function the tile manager should call in order to
 * "display" a tile (e.g. append it to the DOM).  The argument to this
 * function is a TileManager.Tile object.
 * @param removeTile The function the tile manager should call in order to
 * "undisplay" a tile (e.g. remove it from the DOM).  The argument to this
 * function is a TileManager.Tile object.
 * @param fakeWidth The width of the widest possible visible rectangle, e.g.
 * the width of the screen.  This is used in setting the zoomLevel.
 */
function TileManager(appendTile, removeTile, browserView) {
  // backref to the BrowserView object that owns us
  this._browserView = browserView;

  // callbacks to append / remove a tile to / from the parent
  this._appendTile = appendTile;
  this._removeTile = removeTile;

  // tile cache holds tile objects and pools them under a given capacity
  let self = this;
  this._tileCache = new TileManager.TileCache(function(tile) { self._removeTileSafe(tile); },
                                              -1, -1, 110);

  // Rectangle within the viewport that is visible to the user.  It is "critical"
  // in the sense that it must be rendered as soon as it becomes dirty
  this._criticalRect = null;

  // Current <browser> DOM element, holding the content we wish to render.
  // This is null when no browser is attached
  this._browser = null;

  // if we have an outstanding paint timeout, its value is stored here
  // for cancelling when we end page loads
  //this._drawTimeout = 0;
  this._pageLoadResizerTimeout = 0;

  // timeout of the non-visible-tiles-crawler to cache renders from the browser
  this._idleTileCrawlerTimeout = 0;

  // object that keeps state on our current lazyload crawl
  this._crawler = null;

  // the max right coordinate we've seen from paint events
  // while we were loading a page.  If we see something that's bigger than
  // our width, we'll trigger a page zoom.
  this._pageLoadMaxRight = 0;
  this._pageLoadMaxBottom = 0;

  // Tells us to pan to top before first draw
  this._needToPanToTop = false;
}

TileManager.prototype = {

  setBrowser: function setBrowser(b) { this._browser = b; },

  // This is the callback fired by our client whenever the viewport
  // changed somehow (or didn't change but someone asked it to update).
  viewportChangeHandler: function viewportChangeHandler(viewportRect,
                                                        criticalRect,
                                                        boundsSizeChanged,
                                                        dirtyAll) {
    // !!! --- DEBUG BEGIN -----
    dump("***vphandler***\n");
    dump(viewportRect.toString() + "\n");
    dump(criticalRect.toString() + "\n");
    dump(boundsSizeChanged + "\n");
    dump(dirtyAll + "\n***************\n");
    // !!! --- DEBUG END -------

    let tc = this._tileCache;

    tc.iBound = Math.ceil(viewportRect.right / kTileWidth);
    tc.jBound = Math.ceil(viewportRect.bottom / kTileHeight);

    if (!criticalRect || !criticalRect.equals(this._criticalRect)) {
      this.beginCriticalMove(criticalRect);
      this.endCriticalMove(criticalRect, !boundsSizeChanged);
    }

    if (boundsSizeChanged) {
      // TODO fastpath if !dirtyAll
      this.dirtyRects([viewportRect.clone()], true);
    }
  },

  dirtyRects: function dirtyRects(rects, doCriticalRender) {
    let criticalIsDirty = false;
    let criticalRect = this._criticalRect;

    for each (let rect in rects) {
      this._tileCache.forEachIntersectingRect(rect, false, this._dirtyTile, this);

      if (criticalRect && rect.intersects(criticalRect))
        criticalIsDirty = true;
    }

    if (criticalIsDirty && doCriticalRender)
      this.criticalRectPaint();
  },

  criticalRectPaint: function criticalRectPaint() {
    let cr = this._criticalRect;

    if (cr) {
      let [ctrx, ctry] = cr.centerRounded();
      this.recenterEvictionQueue(ctrx, ctry);
      this._renderAppendHoldRect(cr);
    }
  },

  beginCriticalMove2: function beginCriticalMove(destCriticalRect) {
    let start = Date.now();
    function appendNonDirtyTile(tile) {
      if (!tile.isDirty())
        this._appendTileSafe(tile);
    }

    if (destCriticalRect)
      this._tileCache.forEachIntersectingRect(destCriticalRect, false, appendNonDirtyTile, this);
    let end = Date.now();
    dump("start: " + (end-start) + "\n")
  },

  beginCriticalMove: function beginCriticalMove(destCriticalRect) {
  /*
  function appendNonDirtyTile(tile) {
    if (!tile.isDirty())
      this._appendTileSafe(tile);
  }
  */

    let start = Date.now();

    if (destCriticalRect) {

      let rect = destCriticalRect;

      let create = false;

      // this._tileCache.forEachIntersectingRect(destCriticalRect, false, appendNonDirtyTile, this);
      let visited = {};
      let evictGuard = null;
      if (create) {
	evictGuard = function evictGuard(tile) {
	  return !visited[tile.toString()];
	};
      }

      let starti = rect.left  >> kTileExponentWidth;
      let endi   = rect.right >> kTileExponentWidth;

      let startj = rect.top    >> kTileExponentHeight;
      let endj   = rect.bottom >> kTileExponentHeight;

      let tile = null;
      let tc = this._tileCache;

      for (var j = startj; j <= endj; ++j) {
	for (var i = starti; i <= endi; ++i) {

	  // 'this' for getTile needs to be tc

	  //tile = this.getTile(i, j, create, evictGuard);
	  //if (!tc.inBounds(i, j)) {
	  if (0 <= i && 0 <= j && i <= tc.iBound && j <= tc.jBound) {
	    //return null;
	    break;
	  }

	  tile = null;

	  //if (tc._isOccupied(i, j)) {
	  if (!!(tc._tiles[i] && tc._tiles[i][j])) {
	    tile = tc._tiles[i][j];
	  } else if (create) {
	    // NOTE: create is false here
	    tile = tc._createTile(i, j, evictionGuard);
	    if (tile) tile.markDirty();
	  }

	  if (tile) {
	    visited[tile.toString()] = true;
	    //fn.call(thisObj, tile);
	    //function appendNonDirtyTile(tile) {
	    //if (!tile.isDirty())
	    if (!tile._dirtyTileCanvas) {
	      //this._appendTileSafe(tile);
	      if (!tile._appended) {
		let astart = Date.now();
		this._appendTile(tile);
		tile._appended = true;
		let aend = Date.now();
		dump("append: " + (aend - astart) + "\n");
	      }
	    }
	    //}
	  }
	}
      }
    }

    let end = Date.now();
    dump("start: " + (end-start) + "\n")
  },

  endCriticalMove: function endCriticalMove(destCriticalRect, doCriticalPaint) {
    let start = Date.now();

    let tc = this._tileCache;
    let cr = this._criticalRect;

    let dcr = destCriticalRect.clone();

    let f = function releaseOldTile(tile) {
      // release old tile
      if (!tile.boundRect.intersects(dcr))
        tc.releaseTile(tile);
    }

    if (cr)
      tc.forEachIntersectingRect(cr, false, f, this);

    this._holdRect(destCriticalRect);

    if (cr)
      cr.copyFrom(destCriticalRect);
    else
      this._criticalRect = cr = destCriticalRect;

    let crpstart = Date.now();
    if (doCriticalPaint)
      this.criticalRectPaint();
    dump(" crp: " + (Date.now() - crpstart) + "\n");

    let end = Date.now();
    dump("end: " + (end - start) + "\n");
  },

  restartLazyCrawl: function restartLazyCrawl(startRectOrQueue) {
    if (!startRectOrQueue || startRectOrQueue instanceof Array) {
      this._crawler = new TileManager.CrawlIterator(this._tileCache);

      if (startRectOrQueue) {
        let len = startRectOrQueue.length;
        for (let k = 0; k < len; ++k)
          this._crawler.enqueue(startRectOrQueue[k].i, startRectOrQueue[k].j);
      }
    } else {
      this._crawler = new TileManager.CrawlIterator(this._tileCache, startRectOrQueue);
    }

    if (!this._idleTileCrawlerTimeout)
      this._idleTileCrawlerTimeout = setTimeout(this._idleTileCrawler, 2000, this);
  },

  stopLazyCrawl: function stopLazyCrawl() {
    this._idleTileCrawlerTimeout = 0;
    this._crawler = null;

    let cr = this._criticalRect;
    if (cr) {
      let [ctrx, ctry] = cr.centerRounded();
      this.recenterEvictionQueue(ctrx, ctry);
    }
  },

  recenterEvictionQueue: function recenterEvictionQueue(ctrx, ctry) {
    let ctri = ctrx >> kTileExponentWidth;
    let ctrj = ctry >> kTileExponentHeight;

    function evictFarTiles(a, b) {
      let dista = Math.max(Math.abs(a.i - ctri), Math.abs(a.j - ctrj));
      let distb = Math.max(Math.abs(b.i - ctri), Math.abs(b.j - ctrj));
      return dista - distb;
    }

    this._tileCache.sortEvictionQueue(evictFarTiles);
  },

  _renderTile: function _renderTile(tile) {
    if (tile.isDirty())
      tile.render(this._browser, this._browserView);
  },

  _appendTileSafe: function _appendTileSafe(tile) {
    if (!tile._appended) {
      this._appendTile(tile);
      tile._appended = true;
    }
  },

  _removeTileSafe: function _removeTileSafe(tile) {
    if (tile._appended) {
      this._removeTile(tile);
      tile._appended = false;
    }
  },

  _dirtyTile: function _dirtyTile(tile) {
    if (!this._criticalRect || !tile.boundRect.intersects(this._criticalRect))
      this._removeTileSafe(tile);

    tile.markDirty();

    if (this._crawler)
      this._crawler.enqueue(tile.i, tile.j);
  },

  _holdRect: function _holdRect(rect) {
    this._tileCache.holdTilesIntersectingRect(rect);
  },

  _releaseRect: function _releaseRect(rect) {
    this._tileCache.releaseTilesIntersectingRect(rect);
  },

  _renderAppendHoldRect: function _renderAppendHoldRect(rect) {
    function renderAppendHoldTile(tile) {
      if (tile.isDirty())
        this._renderTile(tile);

      this._appendTileSafe(tile);
      this._tileCache.holdTile(tile);
    }

    this._tileCache.forEachIntersectingRect(rect, true, renderAppendHoldTile, this);
  },

  _idleTileCrawler: function _idleTileCrawler(self) {
    if (!self) self = this;
    dump('crawl pass.\n');
    let itered = 0, rendered = 0;

    let start = Date.now();
    let comeAgain = true;

    while ((Date.now() - start) <= kLazyRoundTimeCap) {
      let tile = self._crawler.next();

      if (!tile) {
        comeAgain = false;
        break;
      }

      if (tile.isDirty()) {
        self._renderTile(tile);
        ++rendered;
      }
      ++itered;
    }

    dump('crawl itered:' + itered + ' rendered:' + rendered + '\n');

    if (comeAgain) {
      self._idleTileCrawlerTimeout = setTimeout(self._idleTileCrawler, 2000, self);
    } else {
      self.stopLazyCrawl();
      dump('crawl end\n');
    }
  }

};


/**
 * The tile cache used by the tile manager to hold and index all
 * tiles.  Also responsible for pooling tiles and maintaining the
 * number of tiles under given capacity.
 *
 * @param onBeforeTileDetach callback set by the TileManager to call before
 * we must "detach" a tile from a tileholder due to needing it elsewhere or
 * having to discard it on capacity decrease
 * @param capacity the initial capacity of the tile cache, i.e. the max number
 * of tiles the cache can have allocated
 */
TileManager.TileCache = function TileCache(onBeforeTileDetach, iBound, jBound, capacity) {
  if (arguments.length <= 3 || capacity < 0)
    capacity = Infinity;

  // We track all pooled tiles in a 2D array (row, column) ordered as
  // they "appear on screen".  The array is a grid that functions for
  // storage of the tiles and as a lookup map.  Each array entry is
  // a reference to the tile occupying that space ("tileholder").  Entries
  // are not unique, so a tile could be referenced by multiple array entries,
  // i.e. a tile could "span" many tile placeholders (e.g. if we merge
  // neighbouring tiles).
  this._tiles = [];

  // holds the same tiles that _tiles holds, but as contiguous array
  // elements, for pooling tiles for reuse under finite capacity
  this._tilePool = (capacity == Infinity) ? new Array() : new Array(capacity);

  this._capacity = capacity;
  this._nTiles = 0;
  this._numFree = 0;

  this._onBeforeTileDetach = onBeforeTileDetach;

  this.iBound = iBound;
  this.jBound = jBound;
};

TileManager.TileCache.prototype = {

  get size() { return this._nTiles; },
  get numFree() { return this._numFree; },

  // A comparison function that will compare all free tiles as greater
  // than all non-free tiles.  Useful because, for instance, to shrink
  // the tile pool when capacity is lowered, we want to remove all tiles
  // at the new cap and beyond, favoring removal of free tiles first.
  evictionCmp: function freeTilesLast(a, b) {
    if (a.free == b.free) return (a.j == b.j) ? b.i - a.i : b.j - a.j;
    return (a.free) ? 1 : -1;
  },

  getCapacity: function getCapacity() { return this._capacity; },

  setCapacity: function setCapacity(newCap, skipEvictionQueueSort) {
    if (newCap < 0)
      throw "Cannot set a negative tile cache capacity";

    if (newCap == Infinity) {
      this._capacity = Infinity;
      return;
    } else if (this._capacity == Infinity) {
      // pretend we had a finite capacity all along and proceed normally
      this._capacity = this._tilePool.length;
    }

    let rem = null;

    if (newCap < this._capacity) {
      // This case is obnoxious.  We're decreasing our capacity which means
      // we may have to get rid of tiles.  Depending on our eviction comparator,
      // we probably try to get rid free tiles first, but we might have to throw
      // out some nonfree ones too.  Note that "throwing out" means the cache
      // won't keep them, and they'll get GC'ed as soon as all other refholders
      // let go of their refs to the tile.
      if (!skipEvictionQueueSort)
        this.sortEvictionQueue();

      rem = this._tilePool.splice(newCap, this._tilePool.length);

    } else {
      // This case is win.  Extend our tile pool array with new empty space.
      this._tilePool.push.apply(this._tilePool, new Array(newCap - this._capacity));
    }

    // update state in the case that we threw things out.
    let nTilesDeleted = this._nTiles - newCap;
    if (nTilesDeleted > 0) {
      let nFreeDeleted = 0;
      for (let k = 0; k < nTilesDeleted; ++k) {
        if (rem[k].free)
          nFreeDeleted++;

        this._detachTile(rem[k].i, rem[k].j);
      }

      this._nTiles -= nTilesDeleted;
      this._numFree -= nFreeDeleted;
    }

    this._capacity = newCap;
  },

  _isOccupied: function _isOccupied(i, j) {
    return !!(this._tiles[i] && this._tiles[i][j]);
  },

  _detachTile: function _detachTile(i, j) {
    let tile = null;
    if (this._isOccupied(i, j)) {
      tile = this._tiles[i][j];

      if (this._onBeforeTileDetach)
        this._onBeforeTileDetach(tile);

      this.releaseTile(tile);
      delete this._tiles[i][j];
    }
    return tile;
  },

  _reassignTile: function _reassignTile(tile, i, j) {
    this._detachTile(tile.i, tile.j);    // detach
    tile.init(i, j);                     // re-init
    this._tiles[i][j] = tile;            // attach
    return tile;
  },

  _evictTile: function _evictTile(evictionGuard) {
    let k = this._nTiles - 1;
    let pool = this._tilePool;
    let victim = null;

    for (; k >= 0; --k) {
      if (pool[k].free &&
          (!evictionGuard || evictionGuard(pool[k])))
      {
        victim = pool[k];
        break;
      }
    }

    return victim;
  },

  _createTile: function _createTile(i, j, evictionGuard) {
    if (!this._tiles[i])
      this._tiles[i] = [];

    let tile = null;

    if (this._nTiles < this._capacity) {
      // either capacity is infinite, or we still have room to allocate more
      tile = new TileManager.Tile(i, j);
      this._tiles[i][j] = tile;
      this._tilePool[this._nTiles++] = tile;
      this._numFree++;

    } else {
      // assert: nTiles == capacity
      dump("\nevicting\n");
      tile = this._evictTile(evictionGuard);
      if (tile)
        this._reassignTile(tile, i, j);
    }

    return tile;
  },

  inBounds: function inBounds(i, j) {
    return 0 <= i && 0 <= j && i <= this.iBound && j <= this.jBound;
  },

  sortEvictionQueue: function sortEvictionQueue(cmp) {
    if (!cmp) cmp = this.evictionCmp;
    this._tilePool.sort(cmp);
  },

  /**
   * Get a tile by its indices
   *
   * @param i Column
   * @param j Row
   * @param create Flag true if the tile should be created in case there is no
   * tile at (i, j)
   * @param reuseCondition Boolean-valued function to restrict conditions under
   * which an old tile may be reused for creating this one.  This can happen if
   * the cache has reached its capacity and must reuse existing tiles in order to
   * create this one.  The function is given a Tile object as its argument and
   * returns true if the tile is OK for reuse. This argument has no effect if the
   * create argument is false.
   */
  getTile: function getTile(i, j, create, evictionGuard) {
    if (!this.inBounds(i, j))
      return null;

    let tile = null;

    if (this._isOccupied(i, j)) {
      tile = this._tiles[i][j];
    } else if (create) {
      tile = this._createTile(i, j, evictionGuard);
      if (tile) tile.markDirty();
    }

    return tile;
  },

  /**
   * Look up (possibly creating) a tile from its viewport coordinates.
   *
   * @param x
   * @param y
   * @param create Flag true if the tile should be created in case it doesn't
   * already exist at the tileholder corresponding to (x, y)
   */
  tileFromPoint: function tileFromPoint(x, y, create) {
    let i = x >> kTileExponentWidth;
    let j = y >> kTileExponentHeight;

    return this.getTile(i, j, create);
  },

  /**
   * Hold a tile (i.e. mark it non-free).  Returns true if the operation
   * actually did something, false elsewise.
   */
  holdTile: function holdTile(tile) {
    if (tile && tile.free) {
      tile._hold();
      this._numFree--;
      return true;
    }
    return false;
  },

  /**
   * Release a tile (i.e. mark it free).  Returns true if the operation
   * actually did something, false elsewise.
   */
  releaseTile: function releaseTile(tile) {
    if (tile && !tile.free) {
      tile._release();
      this._numFree++;
      return true;
    }
    return false;
  },

  // XXX the following two functions will iterate through duplicate tiles
  // once we begin to merge tiles.
  /**
   * Fetch all tiles that share at least one point with this rect.  If `create'
   * is true then any tileless tileholders will have tiles created for them.
   */
  tilesIntersectingRect: function tilesIntersectingRect(rect, create) {
    let dx = (rect.right % kTileWidth) - (rect.left % kTileWidth);
    let dy = (rect.bottom % kTileHeight) - (rect.top % kTileHeight);
    let tiles = [];

    for (let y = rect.top; y <= rect.bottom - dy; y += kTileHeight) {
      for (let x = rect.left; x <= rect.right - dx; x += kTileWidth) {
        let tile = this.tileFromPoint(x, y, create);
        if (tile)
          tiles.push(tile);
      }
    }

    return tiles;
  },

  forEachIntersectingRect: function forEachIntersectingRect(rect, create, fn, thisObj) {
    let visited = {};
    let evictGuard = null;
    if (create) {
      evictGuard = function evictGuard(tile) {
        return !visited[tile.toString()];
      };
    }

    let starti = rect.left  >> kTileExponentWidth;
    let endi   = rect.right >> kTileExponentWidth;

    let startj = rect.top    >> kTileExponentHeight;
    let endj   = rect.bottom >> kTileExponentHeight;

    let tile = null;
    for (var j = startj; j <= endj; ++j) {
      for (var i = starti; i <= endi; ++i) {
        tile = this.getTile(i, j, create, evictGuard);
        if (tile) {
          visited[tile.toString()] = true;
          fn.call(thisObj, tile);
        }
      }
    }
  },

  holdTilesIntersectingRect: function holdTilesIntersectingRect(rect) {
    this.forEachIntersectingRect(rect, false, this.holdTile, this);
  },

  releaseTilesIntersectingRect: function releaseTilesIntersectingRect(rect) {
    this.forEachIntersectingRect(rect, false, this.releaseTile, this);
  }

};



TileManager.Tile = function Tile(i, j) {
  // canvas element is where we keep paint data from browser for this tile
  this._canvas = document.createElementNS(kXHTMLNamespaceURI, "canvas");
  this._canvas.setAttribute("width", String(kTileWidth));
  this._canvas.setAttribute("height", String(kTileHeight));
  this._canvas.setAttribute("moz-opaque", "true");
  //this._canvas.style.border = "1px solid red";

  this.init(i, j);  // defines more properties, cf below
};

TileManager.Tile.prototype = {

  // essentially, this is part of constructor code, but since we reuse tiles
  // in the tile cache, this is here so that we can reinitialize tiles when we
  // reuse them
  init: function init(i, j) {
    if (!this.boundRect)
      this.boundRect = new wsRect(i * kTileWidth, j * kTileHeight, kTileWidth, kTileHeight);
    else
      this.boundRect.setRect(i * kTileWidth, j * kTileHeight, kTileWidth, kTileHeight);

    // indices!
    this.i = i;
    this.j = j;

    // flags true if we need to repaint our own local canvas
    this._dirtyTileCanvas = false;

    // keep a dirty rectangle (i.e. only part of the tile is dirty)
    this._dirtyTileCanvasRect = null;

    // flag used by TileManager to avoid re-appending tiles that have already
    // been appended
    this._appended = false;

    // We keep tile objects around after their use for later reuse, so this
    // flags true for an unused pooled tile.  We don't actually care about
    // this from within the Tile prototype, it is here for the cache to use.
    this.free = true;
  },

  // viewport coordinates
  get x() { return this.boundRect.left; },
  get y() { return this.boundRect.top; },

  // the actual canvas that holds the most recently rendered image of this
  // canvas
  getContentImage: function getContentImage() { return this._canvas; },

  isDirty: function isDirty() { return this._dirtyTileCanvas; },

  /**
   * Mark this entire tile as dirty (i.e. the whole tile needs to be rendered
   * on next render).
   */
  markDirty: function markDirty() { this.updateDirtyRegion(); },

  unmarkDirty: function unmarkDirty() {
    this._dirtyTileCanvasRect = null;
    this._dirtyTileCanvas = false;
  },

  /**
   * This will mark dirty at least everything in dirtyRect (which must be
   * specified in canvas coordinates).  If dirtyRect is not given then
   * the entire tile is marked dirty.
   */
  updateDirtyRegion: function updateDirtyRegion(dirtyRect) {
    if (!dirtyRect) {

      if (!this._dirtyTileCanvasRect)
        this._dirtyTileCanvasRect = this.boundRect.clone();
      else
        this._dirtyTileCanvasRect.copyFrom(this.boundRect);

    } else {

      if (!this._dirtyTileCanvasRect)
        this._dirtyTileCanvasRect = dirtyRect.intersect(this.boundRect);
      else if (dirtyRect.intersects(this.boundRect))
        this._dirtyTileCanvasRect.expandToContain(dirtyRect.intersect(this.boundRect));

    }

    // TODO if after the above, the dirty rectangle is large enough,
    // mark the whole tile dirty.

    if (this._dirtyTileCanvasRect)
      this._dirtyTileCanvas = true;
  },

  /**
   * Actually draw the browser content into the dirty region of this
   * tile.  This requires us to actually draw with the
   * nsIDOMCanvasRenderingContext2D object's drawWindow method, which
   * we expect to be a relatively heavy operation.
   *
   * You likely want to check if the tile isDirty() before asking it
   * to render, as this will cause the entire tile to re-render in the
   * case that it is not dirty.
   */
  render: function render(browser, browserView) {
    if (!this.isDirty())
      this.markDirty();

    let rect = this._dirtyTileCanvasRect;

    let x = rect.left - this.boundRect.left;
    let y = rect.top - this.boundRect.top;

    // content process is not being scaled, so don't scale our rect either
    //browserView.viewportToBrowserRect(rect);
    //rect.round(); // snap outward to get whole "pixel" (in browser coords)

    let ctx = this._canvas.getContext("2d");
    ctx.save();

    browserView.browserToViewportCanvasContext(ctx);

    ctx.translate(x, y);

    let cw = browserView._contentWindow;
    //let cw = browser.contentWindow;
    ctx.asyncDrawXULElement(browserView._browser,
                   rect.left, rect.top,
                   rect.right - rect.left, rect.bottom - rect.top,
                   "grey",
                   (ctx.DRAWWINDOW_DO_NOT_FLUSH | ctx.DRAWWINDOW_DRAW_CARET));

    ctx.restore();

    this.unmarkDirty();
  },

  toString: function toString(more) {
    if (more) {
      return 'Tile(' + [this.i,
                        this.j,
                        "dirty=" + this.isDirty(),
                        "boundRect=" + this.boundRect].join(', ')
               + ')';
    }

    return 'Tile(' + this.i + ', ' + this.j + ')';
  },

  _hold: function hold() { this.free = false; },
  _release: function release() { this.free = true; }

};


/**
 * A CrawlIterator is in charge of creating and returning subsequent tiles "crawled"
 * over as we render tiles lazily.  It supports iterator semantics so you can use
 * CrawlIterator objects in for..in loops.
 *
 * Currently the CrawlIterator is built to expand a rectangle iteratively and return
 * subsequent tiles that intersect the boundary of the rectangle.  Each expansion of
 * the rectangle is one unit of tile dimensions in each direction.  This is repeated
 * until all tiles from elsewhere have been reused (assuming the cache has finite
 * capacity) in this crawl, so that we don't start reusing tiles from the beginning
 * of our crawl.  Afterward, the CrawlIterator enters a state where it operates as a
 * FIFO queue, and calls to next() simply dequeue elements, which must be added with
 * enqueue().
 *
 * @param tileCache The TileCache over whose tiles this CrawlIterator will crawl
 * @param startRect [optional] The rectangle that we grow in the first (rectangle
 * expansion) iteration state.
 */
TileManager.CrawlIterator = function CrawlIterator(tileCache, startRect) {
  this._tileCache = tileCache;
  this._stepRect = startRect;

  // used to remember tiles that we've reused during this crawl
  this._visited = {};

  // filters the tiles we've already reused once from being considered victims
  // for reuse when we ask the tile cache to create a new tile
  let visited = this._visited;
  this._notVisited = function(tile) { return !visited[tile]; };

  // a generator that generates tile indices corresponding to tiles intersecting
  // the boundary of an expanding rectangle
  this._crawlIndices = !startRect ? null : (function indicesGenerator(rect, tc) {
    let outOfBounds = false;
    while (!outOfBounds) {
      // expand rect
      rect.left   -= kTileWidth;
      rect.right  += kTileWidth;
      rect.top    -= kTileHeight;
      rect.bottom += kTileHeight;

      let dx = (rect.right % kTileWidth) - (rect.left % kTileWidth);
      let dy = (rect.bottom % kTileHeight) - (rect.top % kTileHeight);

      outOfBounds = true;

      // top, bottom borders
      for each (let y in [rect.top, rect.bottom]) {
        for (let x = rect.left; x <= rect.right - dx; x += kTileWidth) {
          let i = x >> kTileExponentWidth;
          let j = y >> kTileExponentHeight;
          if (tc.inBounds(i, j)) {
            outOfBounds = false;
            yield [i, j];
          }
        }
      }

      // left, right borders
      for each (let x in [rect.left, rect.right]) {
        for (let y = rect.top; y <= rect.bottom - dy; y += kTileHeight) {
          let i = x >> kTileExponentWidth;
          let j = y >> kTileExponentHeight;
          if (tc.inBounds(i, j)) {
            outOfBounds = false;
            yield [i, j];
          }
        }
      }
    }
  })(this._stepRect, this._tileCache),    // instantiate the generator

  // after we finish the rectangle iteration state, we enter the FIFO queue state
  this._queueState = !startRect;
  this._queue = [];

  // used to prevent tiles from being enqueued twice --- "patience, we'll get to
  // it in a moment"
  this._enqueued = {};
};

TileManager.CrawlIterator.prototype = {
  __iterator__: function() {
    while (true) {
      let tile = this.next();
      if (!tile) break;
      yield tile;
    }
  },

  becomeQueue: function becomeQueue() {
    this._queueState = true;
  },

  unbecomeQueue: function unbecomeQueue() {
    this._queueState = false;
  },

  next: function next() {
    if (this._queueState)
      return this.dequeue();

    let tile = null;

    if (this._crawlIndices) {
      try {
        let [i, j] = this._crawlIndices.next();
        tile = this._tileCache.getTile(i, j, true, this._notVisited);
      } catch (e) {
        if (!(e instanceof StopIteration))
          throw e;
      }
    }

    if (tile) {
      this._visited[tile] = true;
    } else {
      this.becomeQueue();
      return this.next();
    }

    return tile;
  },

  dequeue: function dequeue() {
    let tile = null;
    do {
      let idx = this._queue.shift();
      if (!idx)
      return null;

      delete this._enqueued[idx];
      let [i, j]  = this._unstrIndices(idx);
      tile = this._tileCache.getTile(i, j, false);

    } while (!tile);

    return tile;
  },

  enqueue: function enqueue(i, j) {
    let idx = this._strIndices(i, j);
    if (!this._enqueued[idx]) {
      this._queue.push(idx);
      this._enqueued[idx] = true;
    }
  },

  _strIndices: function _strIndices(i, j) {
    return i + "," + j;
  },

  _unstrIndices: function _unstrIndices(str) {
    return str.split(',');
  }

};
