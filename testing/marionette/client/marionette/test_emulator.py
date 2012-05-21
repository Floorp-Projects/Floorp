# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import time
from marionette import Marionette, HTMLElement

#
# Test that Marionette can manage multiple emulators.
# Before running this code, you should have built B2G with the config-qemu
# configuration, see 
# https://wiki.mozilla.org/Auto-tools/Projects/Marionette/DevNotes#Running_B2G_on_an_emulator
#
# You should also set your B2G_HOME environment variable to point to the
# directory where the B2G code lives.
#

if __name__ == '__main__':
    # launch two instance of Marionette, each with their own emulator
    driver1 = Marionette(emulator=True, port=2828)
    assert(driver1.emulator.is_running)
    assert(driver1.emulator.port)
    print 'emulator1 is running on port', driver1.emulator.port
    assert(driver1.port != 2828)
    print 'emulator1 port forwarding configured from port', driver1.port
    print 'on localhost to port 2828 on the device'
    assert(driver1.start_session())

    driver2 = Marionette(emulator=True, port=2828)
    assert(driver2.emulator.is_running)
    assert(driver2.emulator.port)
    print 'emulator2 is running on port', driver2.emulator.port
    assert(driver2.port != 2828)
    print 'emulator1 port forwarding configured from port', driver2.port
    print 'on localhost to port 2828 on the device'
    assert(driver2.start_session())

    # shutdown both emulators
    assert(driver2.emulator.close() == 0)
    assert(not driver2.emulator.is_running)
    assert(driver1.emulator.close() == 0)
    assert(not driver1.emulator.is_running)


