/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import javax.annotation.Nullable;

// Examples taken from the infer website.
public class Eradicate {

    public String f; // Because it is not annoted with nullable -> can never be null!

    public void field(@Nullable Eradicate x) {
        x.f = "3"; // Error: Eradicate null field access
    }

    public void method(@Nullable Object x) {
        String s = x.toString(); // Error: Eradicate null method call
    }

    public void filedNotNull(@Nullable String s) {
        f = s; // Error: Eradicate field not nullable
    }

    public Eradicate() {} // Error: Eradicate field not initialized

    public void str(Eradicate x) {
        String s = x.toString();
    }

    public void callStr(@Nullable Eradicate x) {
        str(x); // Error:  Eradicate parameter not nullable
    }

    public String shouldNotReturnNullBecauseNotAnnotated() {
        return null; // Error: Eradicate return not nullable
    }

    public void redundant() {
        String s = new String("abc");
        if (s != null) { // Error: Eradicate condition redundant
            int n = s.length();
        }
    }

    @Nullable
    public static String someMethod() {
        return ""; // Error: Eradicate return overannotated
    }
}

class B extends Eradicate {
    @Nullable public String shouldNotReturnNullBecauseNotAnnotated() {
        return null; // Error: Eradicate inconsistent subclass return annotation
    }

    public void field(Eradicate x) {} // Error: Inconsistent subclass parameter annotation
}
