/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import javax.annotation.concurrent.ThreadSafe;

// Examples taken from the infer website.
@ThreadSafe
public class Racerd {
    private int mTemperature;

    public void makeDinner() {
        boilWater();
    }

    private void boilWater() {
        mTemperature = 100; //Error: unprotected write.
    }
}

@ThreadSafe
class Account {

    int mBalance = 0;

    public void deposit(int amount) {
        if (amount > 0) {
            mBalance += amount; // Error: unsynchronized write
        }
    }

    public int withdraw(int amount){
        if (amount >= 0 && mBalance - amount >= 0) {
            mBalance -= amount; // Error: unsynchronized write
            return mBalance; // Error: unsynchronized read
        } else {
            return 0;
        }
    }
}
