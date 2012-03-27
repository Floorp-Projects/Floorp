# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1 
# 
# The contents of this file are subject to the Mozilla Public License Version 
# 1.1 (the "License"); you may not use this file except in compliance with 
# the License. You may obtain a copy of the License at 
# http://www.mozilla.org/MPL/ # 
# Software distributed under the License is distributed on an "AS IS" basis, 
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License 
# for the specific language governing rights and limitations under the 
# License. 
# 
# The Original Code is Marionette Client. 
# 
# The Initial Developer of the Original Code is 
#   Mozilla Foundation. 
# Portions created by the Initial Developer are Copyright (C) 2011
# the Initial Developer. All Rights Reserved. 
# 
# Contributor(s): 
#  Jonathan Griffin <jgriffin@mozilla.com>
# 
# Alternatively, the contents of this file may be used under the terms of 
# either the GNU General Public License Version 2 or later (the "GPL"), or 
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"), 
# in which case the provisions of the GPL or the LGPL are applicable instead 
# of those above. If you wish to allow use of your version of this file only 
# under the terms of either the GPL or the LGPL, and not to allow others to 
# use your version of this file under the terms of the MPL, indicate your 
# decision by deleting the provisions above and replace them with the notice 
# and other provisions required by the GPL or the LGPL. If you do not delete 
# the provisions above, a recipient may use your version of this file under 
# the terms of any one of the MPL, the GPL or the LGPL. 
# 
# ***** END LICENSE BLOCK ***** 

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
