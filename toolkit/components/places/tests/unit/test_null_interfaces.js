/***************************** BEGIN LICENSE BLOCK *****************************
* Version: MPL 1.1/GPL 2.0/LGPL 2.1
*
* The contents of this file are subject to the Mozilla Public License Version
* 1.1 (the "License"); you may not use this file except in compliance with the
* License. You may obtain a copy of the License at http://www.mozilla.org/MPL/
*
* Software distributed under the License is distributed on an "AS IS" basis,
* WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License for
* the specific language governing rights and limitations under the License.
*
* The Original Code is Places Test Code.
*
* The Initial Developer of the Original Code is Mozilla Foundation.
* Portions created by the Initial Developer are Copyright (C) 2009 the Initial
* Developer. All Rights Reserved.
*
* Contributor(s):
*  Edward Lee <edilee@mozilla.com> (original author)
*
* Alternatively, the contents of this file may be used under the terms of either
* the GNU General Public License Version 2 or later (the "GPL"), or the GNU
* Lesser General Public License Version 2.1 or later (the "LGPL"), in which case
* the provisions of the GPL or the LGPL are applicable instead of those above.
* If you wish to allow use of your version of this file only under the terms of
* either the GPL or the LGPL, and not to allow others to use your version of
* this file under the terms of the MPL, indicate your decision by deleting the
* provisions above and replace them with the notice and other provisions
* required by the GPL or the LGPL. If you do not delete the provisions above, a
* recipient may use your version of this file under the terms of any one of the
* MPL, the GPL or the LGPL.
*
****************************** END LICENSE BLOCK ******************************/

/**
 * Test bug 489872 to make sure passing nulls to nsNavHistory doesn't crash.
 */

/**
 * Print some debug message to the console. All arguments will be printed,
 * separated by spaces.
 *
 * @param [arg0, arg1, arg2, ...]
 *        Any number of arguments to print out
 * @usage _("Hello World") -> prints "Hello World"
 * @usage _(1, 2, 3) -> prints "1 2 3"
 */
let _ = function(some, debug, text, to) print(Array.slice(arguments).join(" "));

_("Make an array of services to test, each specifying a class id, interface,",
  "and an array of function names that don't throw when passed nulls");
let testServices = [
  ["browser/nav-history-service;1", "nsINavHistoryService",
    ["queryStringToQueries", "removePagesByTimeframe", "removePagesFromHost",
     "removeVisitsByTimeframe"]],
  ["browser/nav-bookmarks-service;1","nsINavBookmarksService",
    ["createFolder"]],
  ["browser/livemark-service;2","nsILivemarkService", []],
  ["browser/livemark-service;2","mozIAsyncLivemarks", ["reloadLivemarks"]],
  ["browser/annotation-service;1","nsIAnnotationService", []],
  ["browser/favicon-service;1","nsIFaviconService", []],
  ["browser/tagging-service;1","nsITaggingService", []],
];
_(testServices.join("\n"));

function run_test()
{
  testServices.forEach(function([cid, iface, nothrow]) {
    _("Running test with", cid, iface, nothrow);
    let s = Cc["@mozilla.org/" + cid].getService(Ci[iface]);

    let okName = function(name) {
      _("Checking if function is okay to test:", name);
      let func = s[name];

      let mesg = "";
      if (typeof func != "function")
        mesg = "Not a function!";
      else if (func.length == 0)
        mesg = "No args needed!";
      else if (name == "QueryInterface")
        mesg = "Ignore QI!";

      if (mesg == "")
        return true;

      _(mesg, "Skipping:", name);
      return false;
    }

    _("Generating an array of functions to test service:", s);
    [i for (i in s) if (okName(i))].sort().forEach(function(n) {
      _();
      _("Testing " + iface + " function with null args:", n);

      let func = s[n];
      let num = func.length;
      _("Generating array of nulls for #args:", num);
      let args = [];
      for (let i = num; --i >= 0; )
        args.push(null);

      let tryAgain = true;
      while (tryAgain == true) {
        try {
          _("Calling with args:", JSON.stringify(args));
          func.apply(s, args);
  
          _("The function didn't throw! Is it one of the nothrow?", nothrow);
          do_check_neq(nothrow.indexOf(n), -1);

          _("Must have been an expected nothrow, so no need to try again");
          tryAgain = false;
        }
        catch(ex if ex.name.match(/NS_ERROR_ILLEGAL_VALUE/)) {
          _("Caught an expected exception:", ex.name);

          _("Moving on to the next test..");
          tryAgain = false;
        }
        catch(ex if ex.name.match(/NS_ERROR_XPC_NEED_OUT_OBJECT/)) {
          let pos = Number(ex.message.match(/object arg (\d+)/)[1]);
          _("Function call expects an out object at", pos);
          args[pos] = {};
        }
        catch(ex if ex.name.match(/NS_ERROR_NOT_IMPLEMENTED/)) {
          _("Method not implemented exception:", ex.name);

          _("Moving on to the next test..");
          tryAgain = false;
        }
        catch(ex) {
          _("Caught some unexpected exception.. dumping");
          _([[i, ex[i]] for (i in ex)].join("\n"));
          do_check_true(false);
        }
      }
    });
  });
}
