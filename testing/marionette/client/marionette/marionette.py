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

import socket

from client import MarionetteClient
from errors import *
from emulator import Emulator

class HTMLElement(object):

    CLASS = "class name"
    SELECTOR = "css selector"
    ID = "id"
    NAME = "name"
    LINK_TEXT = "link text"
    PARTIAL_LINK_TEXT = "partial link text"
    TAG = "tag name"
    XPATH = "xpath"

    def __init__(self, marionette, id):
        self.marionette = marionette
        assert(id is not None)
        self.id = id

    def __str__(self):
        return self.id

    def equals(self, other_element):
        return self.marionette._send_message('elementsEqual', 'value', elements=[self.id, other_element.id])

    def find_element(self, method, target):
        return self.marionette.find_element(method, target, self.id)

    def find_elements(self, method, target):
        return self.marionette.find_elements(method, target, self.id)

    def get_attribute(self, attribute):
        return self.marionette._send_message('getElementAttribute', 'value', element=self.id, name=attribute)

    def click(self):
        return self.marionette._send_message('clickElement', 'ok', element=self.id)

    def text(self):
        return self.marionette._send_message('getElementText', 'value', element=self.id)

    def send_keys(self, string):
        return self.marionette._send_message('sendKeysToElement', 'ok', element=self.id, value=string)

    def value(self):
        return self.marionette._send_message('getElementValue', 'value', element=self.id)

    def clear(self):
        return self.marionette._send_message('clearElement', 'ok', element=self.id)

    def selected(self):
        return self.marionette._send_message('isElementSelected', 'value', element=self.id)

    def enabled(self):
        return self.marionette._send_message('isElementEnabled', 'value', element=self.id)

    def displayed(self):
        return self.marionette._send_message('isElementDisplayed', 'value', element=self.id)


class Marionette(object):

    CONTEXT_CHROME = 'chrome'
    CONTEXT_CONTENT = 'content'

    def __init__(self, host='localhost', port=2828, emulator=False,
                 connectToRunningEmulator=False, homedir=None,
                 baseurl=None, noWindow=False):
        self.host = host
        self.port = self.local_port = port
        self.session = None
        self.window = None
        self.emulator = None
        self.homedir = homedir
        self.baseurl = baseurl
        self.noWindow = noWindow

        if emulator:
            self.emulator = Emulator(homedir=homedir, noWindow=self.noWindow)
            self.emulator.start()
            self.port = self.emulator.setup_port_forwarding(self.port)
            assert(self.emulator.wait_for_port())

        if connectToRunningEmulator:
            self.emulator = Emulator(homedir=homedir)
            self.emulator.connect()
            self.port = self.emulator.setup_port_forwarding(self.port)
            assert(self.emulator.wait_for_port())

        self.client = MarionetteClient(self.host, self.port)

    def __del__(self):
        if self.emulator:
            self.emulator.close()

    def _send_message(self, command, response_key, **kwargs):
        if not self.session and command not in ('newSession', 'getStatus'):
            raise MarionetteException(message="Please start a session")

        message = { 'type': command }
        if self.session:
            message['session'] = self.session
        if kwargs:
            message.update(kwargs)

        try:
            response = self.client.send(message)
        except socket.timeout:
            self.session = None
            self.window = None
            self.client.close()
            if self.emulator:
                port = self.emulator.restart(self.local_port)
                if port is not None:
                    self.port = self.client.port = port
            raise TimeoutException(message='socket.timeout', status=21, stacktrace=None)

        if (response_key == 'ok' and response.get('ok') ==  True) or response_key in response:
            return response[response_key]
        else:
            self._handle_error(response)

    def _handle_error(self, response):
        if 'error' in response and isinstance(response['error'], dict):
            status = response['error'].get('status', 500)
            message = response['error'].get('message')
            stacktrace = response['error'].get('stacktrace')
            # status numbers come from 
            # http://code.google.com/p/selenium/wiki/JsonWireProtocol#Response_Status_Codes
            if status == 7:
                raise NoSuchElementException(message=message, status=status, stacktrace=stacktrace)
            elif status == 8:
                raise NoSuchFrameException(message=message, status=status, stacktrace=stacktrace)
            elif status == 10:
                raise StaleElementException(message=message, status=status, stacktrace=stacktrace)
            elif status == 11:
                raise ElementNotVisibleException(message=message, status=status, stacktrace=stacktrace)
            elif status == 17:
                raise JavascriptException(message=message, status=status, stacktrace=stacktrace)
            elif status == 19:
                raise XPathLookupException(message=message, status=status, stacktrace=stacktrace)
            elif status == 21:
                raise TimeoutException(message=message, status=status, stacktrace=stacktrace)
            elif status == 23:
                raise NoSuchWindowException(message=message, status=status, stacktrace=stacktrace)
            elif status == 28:
                raise ScriptTimeoutException(message=message, status=status, stacktrace=stacktrace)
            else:
                raise MarionetteException(message=message, status=status, stacktrace=stacktrace)
        raise MarionetteException(message=response, status=500)

    def absolute_url(self, relative_url):
        return "%s%s" % (self.baseurl, relative_url)

    def status(self):
        return self._send_message('getStatus', 'value')

    def start_session(self, desired_capabilities=None):
        # We are ignoring desired_capabilities, at least for now.
        self.session = self._send_message('newSession', 'value')
        self.b2g = 'b2g' in self.session
        return self.session

    def delete_session(self):
        response = self._send_message('deleteSession', 'ok')
        self.session = None
        self.window = None
        self.client.close()
        return response

    def get_session_capabilities(self):
        response = self._send_message('getSessionCapabilities', 'value')
        return response

    def set_script_timeout(self, timeout):
        response = self._send_message('setScriptTimeout', 'ok', value=timeout)
        return response

    def set_search_timeout(self, timeout):
        response = self._send_message('setSearchTimeout', 'ok', value=timeout)
        return response

    def get_window(self):
        self.window = self._send_message('getWindow', 'value')
        return self.window

    def get_windows(self):
        response = self._send_message('getWindows', 'value')
        return response

    def close_window(self, window_id=None):
        if not window_id:
            window_id = self.get_window()
        response = self._send_message('closeWindow', 'ok', value=window_id)
        return response

    def set_context(self, context):
        assert(context == self.CONTEXT_CHROME or context == self.CONTEXT_CONTENT)
        return self._send_message('setContext', 'ok', value=context)

    def switch_to_window(self, window_id):
        response = self._send_message('switchToWindow', 'ok', value=window_id)
        self.window = window_id
        return response

    def switch_to_frame(self, frame=None):
        if isinstance(frame, HTMLElement):
            response = self._send_message('switchToFrame', 'ok', element=frame.id)
        else:
            response = self._send_message('switchToFrame', 'ok', value=frame)
        return response

    def get_url(self):
        response = self._send_message('getUrl', 'value')
        return response

    def navigate(self, url):
        response = self._send_message('goUrl', 'ok', value=url)
        return response

    def go_back(self):
        response = self._send_message('goBack', 'ok')
        return response

    def go_forward(self):
        response = self._send_message('goForward', 'ok')
        return response

    def refresh(self):
        response = self._send_message('refresh', 'ok')
        return response

    def wrapArguments(self, args):
        if isinstance(args, list):
            wrapped = []
            for arg in args:
                wrapped.append(self.wrapArguments(arg))
        elif isinstance(args, dict):
            wrapped = {}
            for arg in args:
                wrapped[arg] = self.wrapArguments(args[arg])
        elif type(args) == HTMLElement:
            wrapped = {'ELEMENT': args.id }
        elif (isinstance(args, bool) or isinstance(args, basestring) or
              isinstance(args, int) or args is None):
            wrapped = args

        return wrapped

    def unwrapValue(self, value):
        if isinstance(value, list):
            unwrapped = []
            for item in value:
                unwrapped.append(self.unwrapValue(item))
        elif isinstance(value, dict):
            unwrapped = {}
            for key in value:
                if key == 'ELEMENT':
                    unwrapped = HTMLElement(self, value[key])
                else:
                    unwrapped[key] = self.unwrapValue(value[key])
        else:
            unwrapped = value

        return unwrapped

    def execute_js_script(self, script, script_args=None, timeout=True):
        if script_args is None:
            script_args = []
        args = self.wrapArguments(script_args)
        response = self._send_message('executeJSScript',
                                      'value',
                                      value=script,
                                      args=args,
                                      timeout=timeout)
        return self.unwrapValue(response)

    def execute_script(self, script, script_args=None):
        if script_args is None:
            script_args = []
        args = self.wrapArguments(script_args)
        response = self._send_message('executeScript', 'value', value=script, args=args)
        return self.unwrapValue(response)

    def execute_async_script(self, script, script_args=None):
        if script_args is None:
            script_args = []
        args = self.wrapArguments(script_args)
        response = self._send_message('executeAsyncScript', 'value', value=script, args=args)
        return self.unwrapValue(response)

    def find_element(self, method, target, id=None):
        kwargs = { 'value': target, 'using': method }
        if id:
            kwargs['element'] = id
        response = self._send_message('findElement', 'value', **kwargs)
        element = HTMLElement(self, response)
        return element

    def find_elements(self, method, target, id=None):
        kwargs = { 'value': target, 'using': method }
        if id:
            kwargs['element'] = id
        response = self._send_message('findElements', 'value', **kwargs)
        assert(isinstance(response, list))
        elements = []
        for x in response:
            elements.append(HTMLElement(self, x))
        return elements

    def log(self, msg, level=None):
        return self._send_message('log', 'ok', value=msg, level=level)

    def get_logs(self):
        return self._send_message('getLogs', 'value')
