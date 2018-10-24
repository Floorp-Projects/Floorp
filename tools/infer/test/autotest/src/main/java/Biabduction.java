/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import javax.annotation.Nullable;
import java.util.List;

public class Biabduction {
    private String get() { return null; }

    public void f1() {
        get().length(); // error
    }

    public void f2() {
        try {
            get().length(); // error
        } catch (NullPointerException e) {

        }
    }
}
