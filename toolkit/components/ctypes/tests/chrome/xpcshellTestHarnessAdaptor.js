/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
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
    import: function() { }
  }
};

function do_throw(message, stack) {
  throw message;
}

function do_check_neq(left, right, stack) {
  if (left == right)
    do_throw(text, stack);
}

function do_check_eq(left, right, stack) {
  if (left != right)
    do_throw(text, stack);
}

function do_check_true(condition, stack) {
  do_check_eq(condition, true, stack);
}

function do_check_false(condition, stack) {
  do_check_eq(condition, false, stack);
}

function do_print(text) {
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
  append: function(leaf) {
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
