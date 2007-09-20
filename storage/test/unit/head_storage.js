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
 *   Shawn Wilsher <me@shawnwilsher.com> (Original Author)
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

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cr = Components.results;

var dirSvc = Cc["@mozilla.org/file/directory_service;1"].
             getService(Ci.nsIProperties);

function getTestDB()
{
  var db = dirSvc.get("CurProcD", Ci.nsIFile);
  db.append("test_storage.sqlite");
  return db;
}

function cleanup()
{
  // close the connection
  print("*** Storage Tests: Trying to close!");
  getOpenedDatabase().close();

  // we need to null out the database variable to get a new connection the next
  // time getOpenedDatabase is called
  gDBConn = null;

  // removing test db
  print("*** Storage Tests: Trying to remove file!");
  var dbFile = getTestDB();
  if (dbFile.exists())
    try { dbFile.remove(false); } catch(e) { /* stupid windows box */ }
}

function getService()
{
  return Cc["@mozilla.org/storage/service;1"].getService(Ci.mozIStorageService);
}

var gDBConn = null;
function getOpenedDatabase()
{
  if (!gDBConn) {
    gDBConn = getService().openDatabase(getTestDB());
  }
  return gDBConn;
}

function createStatement(aSQL)
{
  return getOpenedDatabase().createStatement(aSQL);
}

cleanup();

