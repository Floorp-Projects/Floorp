# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import threading
from testserver import TestServer
from marionette import Marionette, HTMLElement

if __name__ == '__main__':

    # start the test server
    server = TestServer(2626)
    thread = threading.Thread(target=server.run)
    thread.daemon = True
    thread.start()

    # run some trivial unit tests which just verify the protocol
    m = Marionette(host='localhost', port=2626)
    assert(m.status()['os']['arch'] == 'x86')
    assert(m.start_session())
    assert(m.get_session_capabilities()['javascriptEnabled'] == True)
    assert(m.get_window() == server.TEST_CURRENT_WINDOW)
    assert(m.window == server.TEST_CURRENT_WINDOW)
    assert(m.get_windows() == server.TEST_WINDOW_LIST)
    assert(m.switch_to_window('window2'))
    assert(m.window == 'window2')
    assert(m.close_window('window2'))
    assert(m.set_script_timeout(1000))
    assert(m.set_search_timeout(500))
    assert(m.get_url() == server.TEST_URL)
    assert(m.navigate(server.TEST_URL))
    assert(m.go_back())
    assert(m.go_forward())
    assert(m.refresh())
    assert(m.execute_script(server.TEST_EXECUTE_SCRIPT))
    assert(m.execute_js_script(server.TEST_EXECUTE_SCRIPT))
    assert(m.execute_js_script(server.TEST_EXECUTE_SCRIPT, server.TEST_EXECUTE_SCRIPT_ARGS))
    assert(m.execute_script(server.TEST_EXECUTE_SCRIPT, server.TEST_EXECUTE_SCRIPT_ARGS))
    assert(m.execute_async_script(server.TEST_EXECUTE_SCRIPT))
    assert(m.execute_async_script(server.TEST_EXECUTE_SCRIPT, server.TEST_EXECUTE_SCRIPT_ARGS))
    assert(str(m.find_element(HTMLElement.CLASS, 'heading')) == server.TEST_FIND_ELEMENT)
    assert([str(x) for x in m.find_elements(HTMLElement.TAG, 'p')] == server.TEST_FIND_ELEMENTS)
    assert(str(m.find_element(HTMLElement.CLASS, 'heading').find_element(HTMLElement.TAG, 'h1')) == server.TEST_FIND_ELEMENT)
    assert([str(x) for x in m.find_element(HTMLElement.ID, 'div1').find_elements(HTMLElement.SELECTOR, '.main')] == \
        server.TEST_FIND_ELEMENTS)
    assert(m.find_element(HTMLElement.ID, 'id1').click())
    assert(m.find_element(HTMLElement.ID, 'id2').text() == server.TEST_GET_TEXT)
    assert(m.find_element(HTMLElement.ID, 'id3').send_keys('Mozilla Firefox'))
    assert(m.find_element(HTMLElement.ID, 'id3').value() == server.TEST_GET_VALUE)
    assert(m.find_element(HTMLElement.ID, 'id3').clear())
    assert(m.find_element(HTMLElement.ID, 'id3').selected())
    assert(m.find_element(HTMLElement.ID, 'id1').equals(m.find_element(HTMLElement.TAG, 'p')))
    assert(m.find_element(HTMLElement.ID, 'id3').enabled())
    assert(m.find_element(HTMLElement.ID, 'id3').displayed())
    assert(m.find_element(HTMLElement.ID, 'id3').get_attribute('value') == server.TEST_GET_VALUE)
    assert(m.delete_session())

    # verify a session is started automatically for us if needed
    assert(m.switch_to_frame('frame1'))
    assert(m.switch_to_frame(1))
    assert(m.switch_to_frame(m.find_element(HTMLElement.ID, 'frameid')))
    assert(m.switch_to_frame())
    assert(m.get_window() == server.TEST_CURRENT_WINDOW)
    assert(m.set_context(m.CONTEXT_CHROME))
    assert(m.delete_session())
