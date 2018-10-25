/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;

public class Checkers {
    public static void leak() {
        try {
            BufferedReader br = new BufferedReader(
                new FileReader(new File("some.txt"))
            );
        } catch (Exception e) {

        }
    }

    public static void error1() {
        String str = null;
        try {
            int x = str.length(); // Error: even if exception is caught
        } catch (NullPointerException e) {

        }
    }

    public static void error2() {
        String str = null;
        int x = str.length(); // Error: not checking for null
    }

}
