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
 * This code is based off of like.test from the sqlite code
 *
 * Contributor(s):
 *   Paul O'Shannessy <poshannessy@mozilla.com>
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

// copy from test_storage_statement_wrapper.js
var wrapper = new Components.Constructor("@mozilla.org/storage/statement-wrapper;1",
                                         Ci.mozIStorageStatementWrapper,
                                         "initialize");
// we want to override the default function for this file
createStatement = function(aSQL) {
    return new wrapper(getOpenedDatabase().createStatement(aSQL));
}

function setup() {
    // Create the table
    getOpenedDatabase().createTable("test_bug444233",
                                    "id INTEGER PRIMARY KEY, value TEXT");

    // Insert dummy data, using wrapper methods
    var stmt = createStatement("INSERT INTO test_bug444233 (value) VALUES (:value)");
    stmt.params.value = "value1"
    stmt.execute();
    stmt.statement.finalize();
    
    stmt = createStatement("INSERT INTO test_bug444233 (value) VALUES (:value)");
    stmt.params.value = "value2"
    stmt.execute();
    stmt.statement.finalize();
}

function test_bug444233() {
    print("*** test_bug444233: started");
    
    // Check that there are 2 results
    var stmt = createStatement("SELECT COUNT(*) AS number FROM test_bug444233");
    do_check_true(stmt.step());
    do_check_eq(2, stmt.row.number);
    stmt.reset();
    stmt.statement.finalize();

    print("*** test_bug444233: doing delete");
    
    // Now try to delete using IN
    // Cheating since we know ids are 1,2
    try {
        var ids = [1, 2];
        stmt = createStatement("DELETE FROM test_bug444233 WHERE id IN (:ids)");
        stmt.params.ids = ids;
    } catch (e) {
        print("*** test_bug444233: successfully caught exception");
    }
    stmt.statement.finalize();
}

function run_test() {
    setup();
    test_bug444233();
    cleanup();
}

