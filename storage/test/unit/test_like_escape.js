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
 * The Original Code is Storage Test Code.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Seth Spitzer <sspitzer@mozilla.org> (Original Author)
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

const LATIN1_AE = "\xc6";
const LATIN1_ae = "\xe6"; 

function setup()
{
  getOpenedDatabase().createTable("t1", "x TEXT");

  var stmt = createStatement("INSERT INTO t1 (x) VALUES ('foo/bar_baz%20cheese')");
  stmt.execute();
  stmt.finalize();

  stmt = createStatement("INSERT INTO t1 (x) VALUES (?1)");
  // insert LATIN_ae, but search on LATIN_AE
  stmt.bindStringParameter(0, "foo%20" + LATIN1_ae + "/_bar");
  stmt.execute();
  stmt.finalize();
}
    
function test_escape_for_like_ascii()
{
  var stmt = createStatement("SELECT x FROM t1 WHERE x LIKE ?1 ESCAPE '/'");
  var paramForLike = stmt.escapeStringForLIKE("oo/bar_baz%20chees", '/');
  // verify that we escaped / _ and %
  do_check_eq(paramForLike, "oo//bar/_baz/%20chees");
  // prepend and append with % for "contains"
  stmt.bindStringParameter(0, "%" + paramForLike + "%"); 
  stmt.executeStep();
  do_check_eq("foo/bar_baz%20cheese", stmt.getString(0));
  stmt.finalize();
}

function test_escape_for_like_non_ascii()
{
  var stmt = createStatement("SELECT x FROM t1 WHERE x LIKE ?1 ESCAPE '/'");
  var paramForLike = stmt.escapeStringForLIKE("oo%20" + LATIN1_AE + "/_ba", '/');
  // verify that we escaped / _ and %
  do_check_eq(paramForLike, "oo/%20" + LATIN1_AE + "///_ba");
  // prepend and append with % for "contains"
  stmt.bindStringParameter(0, "%" + paramForLike + "%");
  stmt.executeStep();
  do_check_eq("foo%20" + LATIN1_ae + "/_bar", stmt.getString(0));
  stmt.finalize();
}

var tests = [test_escape_for_like_ascii, test_escape_for_like_non_ascii];

function run_test()
{
  setup();

  for (var i = 0; i < tests.length; i++)
    tests[i]();
    
  cleanup();
}
