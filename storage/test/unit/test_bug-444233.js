/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function setup() {
    // Create the table
    getOpenedDatabase().createTable("test_bug444233",
                                    "id INTEGER PRIMARY KEY, value TEXT");

    // Insert dummy data, using wrapper methods
    var stmt = createStatement("INSERT INTO test_bug444233 (value) VALUES (:value)");
    stmt.params.value = "value1"
    stmt.execute();
    stmt.finalize();
    
    stmt = createStatement("INSERT INTO test_bug444233 (value) VALUES (:value)");
    stmt.params.value = "value2"
    stmt.execute();
    stmt.finalize();
}

function test_bug444233() {
    print("*** test_bug444233: started");
    
    // Check that there are 2 results
    var stmt = createStatement("SELECT COUNT(*) AS number FROM test_bug444233");
    do_check_true(stmt.executeStep());
    do_check_eq(2, stmt.row.number);
    stmt.reset();
    stmt.finalize();

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
    stmt.finalize();
}

function run_test() {
    setup();
    test_bug444233();
    cleanup();
}

