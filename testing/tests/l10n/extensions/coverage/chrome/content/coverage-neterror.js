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
 * The Original Code is mozilla.org l10n testing.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2066
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Axel Hecht <l10n@mozilla.com>
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

var SBS = Cc["@mozilla.org/intl/stringbundle;1"].getService(Ci.nsIStringBundleService);
var bundle = SBS.createBundle("chrome://global/locale/appstrings.properties");

/**
 * Item to wait on asynchronous loads
 */
var loadAction = {
  isLoaded: false,
  args: [],
  method: function() {
    if (!this.isLoaded) {
      Stack.push(this);
    }
  }
};
// onLoad handler for the iframe, sibling of loadAction
function onFrameLoad(aEvent) {
  aEvent.stopPropagation();
  loadAction.isLoaded = true;
  return false;
};

/**
 * Item to load the next error page
 *
 * Uses the loadAction object to wait for asynchronous loading.
 */
function ErrPageLoad(aFrame, aProperty) {
  this.args = [aFrame, aProperty.key, aProperty.value];
};
ErrPageLoad.prototype = {
  args: null,
  method: function(aFrame, aErr, aDesc) {
    loadAction.isLoaded = false;
    var q = 'e=' + encodeURIComponent(aErr);
    q += '&u=' + encodeURIComponent('http://foo.bar');
    // Simple replacement, replace anything that's %s with aURL,
    // see http://lxr.mozilla.org/mozilla1.8/source/docshell/base/nsDocShell.cpp#2907
    // on how to do this right. But that requires knowledge on each and
    // every error, too much work for now.
    aDesc = aDesc.replace('%S', 'http://foo.bar');
    q += '&d=' + encodeURIComponent(aDesc);
    Stack.push(loadAction);
    aFrame.setAttribute('src', 'about:neterror?' + q);
  }
};

/**
 * Item to start the neterror tests
 *
 * This item collects all the neterror pages we intend to test
 * and adds them to the Stack.
 */
function RootNeterror() {
  this.args = [];
};
RootNeterror.prototype = {
  args: null,
  method: function(aIFrame) {
    var frame = document.getElementById('neterror-pane');
    var nErrors =
      [err for (err in SimpleGenerator(bundle.getSimpleEnumeration(),
                                       Ci.nsIPropertyElement))];
    nErrors.sort(function(aPl, aPr) {
                   return aPl.key > aPr.key ? 1 :
                     aPl.key < aPr.key ? -1 : 0;
                 });
    for each (err in nErrors) {
      Stack.push(new ErrPageLoad(frame, err));
    }
  }
};

toRun.push(new TestDone('NETERROR'));
toRun.push(new RootNeterror());
toRun.push(new TestStart('NETERROR'));
