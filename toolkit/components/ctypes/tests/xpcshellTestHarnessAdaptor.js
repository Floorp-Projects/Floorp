/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is DOM Worker Tests.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Ben Turner <bent.mozilla@gmail.com>
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

var _WORKINGDIR_ = null;

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
