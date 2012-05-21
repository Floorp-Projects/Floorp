# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette import Marionette, HTMLElement

if __name__ == '__main__':

    # launch Fennec with Marionette before starting this test!
    m = Marionette(host='localhost', port=2828)
    assert(m.start_session())
    assert(10 == m.execute_script('return 10;'))

