/* -*-  indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var _WORKINGDIR_ = null;
var _OS_ = null;

var Components = {
  classes: { },
  interfaces: { },
  stack: {
    caller: null
  },
  utils: {
    import() { }
  }
};

function do_throw(message, stack) {
  info("error: " + message);
  info("stack: " + (stack ? stack : new Error().stack));
  throw message;
}

var Assert = {
  notEqual(left, right, stack) {
    if (left == right) {
      var text = "Assert.notEqual failed";
      try {
        text += ": " + left + " == " + right;
      } catch (e) {
      }
      do_throw(text, stack);
    }
  },

  equal(left, right, stack) {
    if (left != right) {
      var text = "Assert.equal failed";
      try {
        text += ": " + left + " != " + right;
      } catch (e) {
      }
      do_throw(text, stack);
    }
  },

  ok(condition, stack) {
    this.equal(condition, true, stack);
  }
};

function info(text) {
  dump("INFO: " + text + "\n");
}

function FileFaker(path) {
  this._path = path;
}
FileFaker.prototype = {
  get path() {
    return this._path;
  },
  get parent() {
    let lastSlash = this._path.lastIndexOf("/");
    if (lastSlash == -1) {
      return "";
    }
    this._path = this._path.substring(0, lastSlash);
    return this;
  },
  append(leaf) {
    this._path = this._path + "/" + leaf;
  }
};

function do_get_file(path, allowNonexistent) {
  if (!_WORKINGDIR_) {
    do_throw("No way to fake files if working directory is unknown!");
  }

  let lf = new FileFaker(_WORKINGDIR_);
  let bits = path.split("/");
  for (let i = 0; i < bits.length; i++) {
    if (bits[i]) {
      if (bits[i] == "..")
        lf = lf.parent;
      else
        lf.append(bits[i]);
    }
  }
  return lf;
}

function get_os() {
  return _OS_;
}
