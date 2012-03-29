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

import json
import select
import socket

class TestServer(object):
    """ A test Marionette server which can be used to test the Marionette
        protocol.  Each request will trigger a canned response; see
        process_command().
    """

    TEST_URL = 'http://www.mozilla.org'
    TEST_CURRENT_WINDOW = 'window1'
    TEST_WINDOW_LIST = ['window1', 'window2', 'window3']
    TEST_EXECUTE_RETURN_VALUE = 10
    TEST_EXECUTE_SCRIPT = 'return 2 * 5;'
    TEST_EXECUTE_SCRIPT_ARGS = 'testing'
    TEST_FIND_ELEMENT = 'element1'
    TEST_FIND_ELEMENTS = ['element1', 'element2', 'element3']
    TEST_GET_TEXT = 'first name'
    TEST_GET_VALUE = 'Mozilla Firefox'

    # canned responses for test messages
    test_responses = {
        'newSession': { 'value': 'a65bef90b145' },
        'getMarionetteID': { 'id': 'conn0.marionette' },
        'deleteSession': { 'ok': True },
        'setScriptTimeout': { 'ok': True },
        'setSearchTimeout': { 'ok': True },
        'getWindow': { 'value': TEST_CURRENT_WINDOW },
        'getWindows': { 'values': TEST_WINDOW_LIST },
        'closeWindow': { 'ok': True },
        'switchToWindow': { 'ok': True },
        'switchToFrame': { 'ok': True },
        'setContext': { 'ok': True },
        'getUrl' : { 'value': TEST_URL },
        'goUrl': { 'ok': True },
        'goBack': { 'ok': True },
        'goForward': { 'ok': True },
        'refresh': { 'ok': True },
        'executeScript': { 'value': TEST_EXECUTE_RETURN_VALUE },
        'executeAsyncScript': { 'value': TEST_EXECUTE_RETURN_VALUE },
        'executeJSScript': { 'value': TEST_EXECUTE_RETURN_VALUE },
        'findElement': { 'value': TEST_FIND_ELEMENT },
        'findElements': { 'values': TEST_FIND_ELEMENTS },
        'clickElement': { 'ok': True },
        'getElementText': { 'value': TEST_GET_TEXT },
        'sendKeysToElement': { 'ok': True },
        'getElementValue': { 'value': TEST_GET_VALUE },
        'clearElement': { 'ok': True },
        'isElementSelected': { 'value': True },
        'elementsEqual': { 'value': True },
        'isElementEnabled': { 'value': True },
        'isElementDisplayed': { 'value': True },
        'getElementAttribute': { 'value': TEST_GET_VALUE },
        'getSessionCapabilities': { 'value': {
            "cssSelectorsEnabled": True,
            "browserName": "firefox",
            "handlesAlerts": True,
            "javascriptEnabled": True,
            "nativeEvents": True,
            "platform": 'linux',
            "takeScreenshot": False,
            "version": "10.1"
            }
        },
        'getStatus': { 'value': {
            "os": {
                "arch": "x86",
                "name": "linux",
                "version": "unknown"
                },
            "build": {
                "revision": "unknown",
                "time": "unknown",
                "version": "unknown"
                }
            }
        }
    }

    # canned error responses for test messages
    error_responses = {
        'executeScript': { 'error': { 'message': 'JavaScript error', 'status': 17 } },
        'executeAsyncScript': { 'error': { 'message': 'Script timed out', 'status': 28 } },
        'findElement': { 'error': { 'message': 'Element not found', 'status': 7 } },
        'findElements': { 'error': { 'message': 'XPath is invalid', 'status': 19 } },
        'closeWindow': { 'error': { 'message': 'No such window', 'status': 23 } },
        'getWindow': { 'error': { 'message': 'No such window', 'status': 23 } },
        'clickElement': { 'error': { 'message': 'Element no longer exists', 'status': 10 } },
        'sendKeysToElement': { 'error': { 'message': 'Element is not visible on the page', 'status': 11 } },
        'switchToFrame': { 'error': { 'message': 'No such frame', 'status': 8 } }
    }

    def __init__(self, port):
        self.port = port

        self.srvsock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.srvsock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.srvsock.bind(("", port))
        self.srvsock.listen(5)
        self.descriptors = [self.srvsock]
        self.responses = self.test_responses
        print 'TestServer started on port %s' % port

    def _recv_n_bytes(self, sock, n):
        """ Convenience method for receiving exactly n bytes from
            self.sock (assuming it's open and connected).
        """
        data = ''
        while len(data) < n:
            chunk = sock.recv(n - len(data))
            if chunk == '':
                break
            data += chunk
        return data

    def receive(self, sock):
        """ Receive the next complete response from the server, and return
            it as a dict.  Each response from the server is prepended by
            len(message) + ':'.
        """
        assert(sock)
        response = sock.recv(10)
        sep = response.find(':')
        if sep == -1:
            return None
        length = response[0:sep]
        response = response[sep + 1:]
        response += self._recv_n_bytes(sock, int(length) + 1 + len(length) - 10)
        print 'received', response
        return json.loads(response)

    def send(self, sock, msg):
        print 'msg', msg
        data = json.dumps(msg)
        print 'sending %s' % data
        sock.send('%s:%s' % (len(data), data))

    def accept_new_connection(self):
        newsock, (remhost, remport) = self.srvsock.accept()
        self.descriptors.append( newsock )
        str = 'Client connected %s:%s\r\n' % (remhost, remport)
        print str
        self.send(newsock, {'from': 'root',
                            'applicationType': 'gecko',
                            'traits': []})

    def process_command(self, data):
        command = data['type']

        if command == 'use_test_responses':
            self.responses = self.test_responses
            return { 'ok': True }
        elif command == 'use_error_responses':
            self.responses = self.error_responses
            return { 'ok': True }

        if command in self.responses:
            response = self.responses[command]
        else:
            response = { 'error': { 'message': 'unknown command: %s' % command, 'status': 500} }

        if command not in ('newSession', 'getStatus', 'getMarionetteID') and 'session' not in data:
            response = { 'error': { 'message': 'no session specified', 'status': 500 } }

        return response

    def run(self):
        while 1:
            # Await an event on a readable socket descriptor
            (sread, swrite, sexc) = select.select( self.descriptors, [], [] )
            # Iterate through the tagged read descriptors
            for sock in sread:
                # Received a connect to the server (listening) socket
                if sock == self.srvsock:
                    self.accept_new_connection()
                else:
                    # Received something on a client socket
                    try:
                        data = self.receive(sock)
                    except:
                        data = None
                    # Check to see if the peer socket closed
                    if data is None:
                        host,port = sock.getpeername()
                        str = 'Client disconnected %s:%s\r\n' % (host, port)
                        print str
                        sock.close
                        self.descriptors.remove(sock)
                    else:
                        if 'type' in data:
                            msg = self.process_command(data)
                        else:
                            msg = 'command: %s' % json.dumps(data)
                        self.send(sock, msg)


if __name__ == "__main__":
    server = TestServer(2626)
    server.run()
