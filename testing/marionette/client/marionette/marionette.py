# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import socket
import sys
import traceback

from client import MarionetteClient
from application_cache import ApplicationCache
from keys import Keys
from errors import *
from emulator import Emulator
from geckoinstance import GeckoInstance


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

    def __eq__(self, other_element):
        return self.id == other_element.id

    def find_element(self, method, target):
        return self.marionette.find_element(method, target, self.id)

    def find_elements(self, method, target):
        return self.marionette.find_elements(method, target, self.id)

    def get_attribute(self, attribute):
        return self.marionette._send_message('getElementAttribute', 'value', element=self.id, name=attribute)

    def click(self):
        return self.marionette._send_message('clickElement', 'ok', element=self.id)

    @property
    def text(self):
        return self.marionette._send_message('getElementText', 'value', element=self.id)

    def send_keys(self, *string):
        typing = []
        for val in string:
            if isinstance(val, Keys):
                typing.append(val)
            elif isinstance(val, int):
                val = str(val)
                for i in range(len(val)):
                    typing.append(val[i])
            else:
                for i in range(len(val)):
                    typing.append(val[i])
        return self.marionette._send_message('sendKeysToElement', 'ok', element=self.id, value=typing)

    def clear(self):
        return self.marionette._send_message('clearElement', 'ok', element=self.id)

    def is_selected(self):
        return self.marionette._send_message('isElementSelected', 'value', element=self.id)

    def is_enabled(self):
        return self.marionette._send_message('isElementEnabled', 'value', element=self.id)

    def is_displayed(self):
        return self.marionette._send_message('isElementDisplayed', 'value', element=self.id)

    @property
    def size(self):
        return self.marionette._send_message('getElementSize', 'value', element=self.id)

    @property
    def tag_name(self):
        return self.marionette._send_message('getElementTagName', 'value', element=self.id)

    @property
    def location(self):
        return self.marionette._send_message('getElementPosition', 'value', element=self.id)


class Marionette(object):

    CONTEXT_CHROME = 'chrome'
    CONTEXT_CONTENT = 'content'

    def __init__(self, host='localhost', port=2828, bin=None, profile=None,
                 emulator=None, sdcard=None, emulatorBinary=None,
                 emulatorImg=None, emulator_res='480x800', gecko_path=None,
                 connectToRunningEmulator=False, homedir=None, baseurl=None,
                 noWindow=False, logcat_dir=None, busybox=None, load_early=False):
        self.host = host
        self.port = self.local_port = port
        self.bin = bin
        self.instance = None
        self.profile = profile
        self.session = None
        self.window = None
        self.emulator = None
        self.extra_emulators = []
        self.homedir = homedir
        self.baseurl = baseurl
        self.noWindow = noWindow
        self.logcat_dir = logcat_dir
        self._test_name = None

        if bin:
            port = int(self.port)
            if not Marionette.is_port_available(port, host=self.host):
                ex_msg = "%s:%d is unavailable." % (self.host, port)
                raise MarionetteException(message=ex_msg)
            self.instance = GeckoInstance(host=self.host, port=self.port,
                                          bin=self.bin, profile=self.profile)
            self.instance.start()
            assert(self.instance.wait_for_port())

        if emulator:
            self.emulator = Emulator(homedir=homedir,
                                     noWindow=self.noWindow,
                                     logcat_dir=self.logcat_dir,
                                     arch=emulator,
                                     sdcard=sdcard,
                                     emulatorBinary=emulatorBinary,
                                     userdata=emulatorImg,
                                     res=emulator_res)
            self.emulator.start()
            self.port = self.emulator.setup_port_forwarding(self.port)
            assert(self.emulator.wait_for_port())

        if connectToRunningEmulator:
            self.emulator = Emulator(homedir=homedir,
                                     logcat_dir=self.logcat_dir)
            self.emulator.connect()
            self.port = self.emulator.setup_port_forwarding(self.port)
            assert(self.emulator.wait_for_port())

        self.client = MarionetteClient(self.host, self.port)

        if emulator:
            self.emulator.setup(self, gecko_path=gecko_path,
                                load_early=load_early)
            if busybox:
                self.emulator.install_busybox(busybox)

    def __del__(self):
        if self.emulator:
            self.emulator.close()
        if self.instance:
            self.instance.close()
        for qemu in self.extra_emulators:
            qemu.emulator.close()

    @staticmethod
    def is_port_available(port, host=''):
        port = int(port)
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            s.bind((host, port))
            return True
        except socket.error:
            return False
        finally:
            s.close()

    @classmethod
    def getMarionetteOrExit(cls, *args, **kwargs):
        try:
            m = cls(*args, **kwargs)
            return m
        except InstallGeckoError:
            # Bug 812395 - the process of installing gecko into the emulator
            # and then restarting B2G tickles some bug in the emulator/b2g
            # that intermittently causes B2G to fail to restart.  To work
            # around this in TBPL runs, we will fail gracefully from this
            # error so that the mozharness script can try the run again.

            # This string will get caught by mozharness and will cause it
            # to retry the tests.
            print "Error installing gecko!"

            # Exit without a normal exception to prevent mozharness from
            # flagging the error.
            sys.exit()

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
            raise TimeoutException(message='socket.timeout', status=ErrorCodes.TIMEOUT, stacktrace=None)

        # Process any emulator commands that are sent from a script
        # while it's executing.
        while response.get("emulator_cmd"):
            response = self._handle_emulator_cmd(response)

        if (response_key == 'ok' and response.get('ok') ==  True) or response_key in response:
            return response[response_key]
        else:
            self._handle_error(response)

    def _handle_emulator_cmd(self, response):
        cmd = response.get("emulator_cmd")
        if not cmd or not self.emulator:
            raise MarionetteException(message="No emulator in this test to run "
                                      "command against.")
        cmd = cmd.encode("ascii")
        result = self.emulator._run_telnet(cmd)
        return self.client.send({"type": "emulatorCmdResult",
                                 "id": response.get("id"),
                                 "result": result})

    def _handle_error(self, response):
        if 'error' in response and isinstance(response['error'], dict):
            status = response['error'].get('status', 500)
            message = response['error'].get('message')
            stacktrace = response['error'].get('stacktrace')
            # status numbers come from 
            # http://code.google.com/p/selenium/wiki/JsonWireProtocol#Response_Status_Codes
            if status == ErrorCodes.NO_SUCH_ELEMENT:
                raise NoSuchElementException(message=message, status=status, stacktrace=stacktrace)
            elif status == ErrorCodes.NO_SUCH_FRAME:
                raise NoSuchFrameException(message=message, status=status, stacktrace=stacktrace)
            elif status == ErrorCodes.STALE_ELEMENT_REFERENCE:
                raise StaleElementException(message=message, status=status, stacktrace=stacktrace)
            elif status == ErrorCodes.ELEMENT_NOT_VISIBLE:
                raise ElementNotVisibleException(message=message, status=status, stacktrace=stacktrace)
            elif status == ErrorCodes.INVALID_ELEMENT_STATE:
                raise InvalidElementStateException(message=message, status=status, stacktrace=stacktrace)
            elif status == ErrorCodes.UNKNOWN_ERROR:
                raise MarionetteException(message=message, status=status, stacktrace=stacktrace)
            elif status == ErrorCodes.ELEMENT_IS_NOT_SELECTABLE:
                raise ElementNotSelectableException(message=message, status=status, stacktrace=stacktrace)
            elif status == ErrorCodes.JAVASCRIPT_ERROR:
                raise JavascriptException(message=message, status=status, stacktrace=stacktrace)
            elif status == ErrorCodes.XPATH_LOOKUP_ERROR:
                raise XPathLookupException(message=message, status=status, stacktrace=stacktrace)
            elif status == ErrorCodes.TIMEOUT:
                raise TimeoutException(message=message, status=status, stacktrace=stacktrace)
            elif status == ErrorCodes.NO_SUCH_WINDOW:
                raise NoSuchWindowException(message=message, status=status, stacktrace=stacktrace)
            elif status == ErrorCodes.INVALID_COOKIE_DOMAIN:
                raise InvalidCookieDomainException(message=message, status=status, stacktrace=stacktrace)
            elif status == ErrorCodes.UNABLE_TO_SET_COOKIE:
                raise UnableToSetCookieException(message=message, status=status, stacktrace=stacktrace)
            elif status == ErrorCodes.NO_ALERT_OPEN:
                raise NoAlertPresentException(message=message, status=status, stacktrace=stacktrace)
            elif status == ErrorCodes.SCRIPT_TIMEOUT:
                raise ScriptTimeoutException(message=message, status=status, stacktrace=stacktrace)
            elif status == ErrorCodes.INVALID_SELECTOR \
                 or status == ErrorCodes.INVALID_XPATH_SELECTOR \
                 or status == ErrorCodes.INVALID_XPATH_SELECTOR_RETURN_TYPER:
                raise InvalidSelectorException(message=message, status=status, stacktrace=stacktrace)
            elif status == ErrorCodes.MOVE_TARGET_OUT_OF_BOUNDS:
                MoveTargetOutOfBoundsException(message=message, status=status, stacktrace=stacktrace)
            else:
                raise MarionetteException(message=message, status=status, stacktrace=stacktrace)
        raise MarionetteException(message=response, status=500)

    def check_for_crash(self):
        returncode = None
        name = None
        if self.emulator:
            if self.emulator.check_for_crash():
                returncode = self.emulator.proc.returncode
                name = 'emulator'
        elif self.instance:
            # In the future, a check for crashed Firefox processes
            # should be here.
            pass
        if returncode is not None:
            print ('TEST-UNEXPECTED-FAIL - PROCESS CRASH - %s has terminated with exit code %d' %
                (name, returncode))
        return returncode is not None

    def absolute_url(self, relative_url):
        return "%s%s" % (self.baseurl, relative_url)

    def status(self):
        return self._send_message('getStatus', 'value')

    def start_session(self, desired_capabilities=None):
        # We are ignoring desired_capabilities, at least for now.
        self.session = self._send_message('newSession', 'value')
        self.b2g = 'b2g' in self.session
        return self.session

    @property
    def test_name(self):
        return self._test_name

    @test_name.setter
    def test_name(self, test_name):
        if self._send_message('setTestName', 'ok', value=test_name):
            self._test_name = test_name

    def delete_session(self):
        response = self._send_message('deleteSession', 'ok')
        self.session = None
        self.window = None
        self.client.close()
        return response

    @property
    def session_capabilities(self):
        response = self._send_message('getSessionCapabilities', 'value')
        return response

    def set_script_timeout(self, timeout):
        response = self._send_message('setScriptTimeout', 'ok', value=timeout)
        return response

    def set_search_timeout(self, timeout):
        response = self._send_message('setSearchTimeout', 'ok', value=timeout)
        return response

    @property
    def current_window_handle(self):
        self.window = self._send_message('getWindow', 'value')
        return self.window

    @property
    def title(self):
        response = self._send_message('getTitle', 'value') 
        return response

    @property
    def window_handles(self):
        response = self._send_message('getWindows', 'value')
        return response

    @property
    def page_source(self):
        response = self._send_message('getPageSource', 'value')
        return response

    def close(self, window_id=None):
        if not window_id:
            window_id = self.current_window_handle
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
              isinstance(args, int) or isinstance(args, float) or args is None):
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

    def execute_js_script(self, script, script_args=None, async=True, new_sandbox=True, special_powers=False):
        if script_args is None:
            script_args = []
        args = self.wrapArguments(script_args)
        response = self._send_message('executeJSScript',
                                      'value',
                                      value=script,
                                      args=args,
                                      async=async,
                                      newSandbox=new_sandbox,
                                      specialPowers=special_powers)
        return self.unwrapValue(response)

    def execute_script(self, script, script_args=None, new_sandbox=True, special_powers=False):
        if script_args is None:
            script_args = []
        args = self.wrapArguments(script_args)
        response = self._send_message('executeScript',
                                     'value',
                                      value=script,
                                      args=args,
                                      newSandbox=new_sandbox,
                                      specialPowers=special_powers)
        return self.unwrapValue(response)

    def execute_async_script(self, script, script_args=None, new_sandbox=True, special_powers=False):
        if script_args is None:
            script_args = []
        args = self.wrapArguments(script_args)
        response = self._send_message('executeAsyncScript',
                                      'value',
                                      value=script,
                                      args=args,
                                      newSandbox=new_sandbox,
                                      specialPowers=special_powers)
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

    def add_perf_data(self, suite, name, value):
        return self._send_message('addPerfData', 'ok', suite=suite, name=name, value=value)

    def get_perf_data(self):
        return self._send_message('getPerfData', 'value')

    def import_script(self, js_file):
        js = ''
        with open(js_file, 'r') as f:
            js = f.read()
        return self._send_message('importScript', 'ok', script=js)

    def add_cookie(self, cookie):
        """
           Adds a cookie to your current session.

           :Args:
           - cookie_dict: A dictionary object, with required keys - "name" and "value";
           optional keys - "path", "domain", "secure", "expiry"

           Usage:
              driver.add_cookie({'name' : 'foo', 'value' : 'bar'})
              driver.add_cookie({'name' : 'foo', 'value' : 'bar', 'path' : '/'})
              driver.add_cookie({'name' : 'foo', 'value' : 'bar', 'path' : '/',
                                 'secure':True})
        """
        return self._send_message('addCookie', 'ok', cookie=cookie)

    def delete_all_cookies(self):
        """
            Delete all cookies in the scope of the session.
            :Usage:
                driver.delete_all_cookies()
        """
        return self._send_message('deleteAllCookies', 'ok')

    def delete_cookie(self, name):
        """
            Delete a cookie by its name
            :Usage:
                driver.delete_cookie('foo')

        """
        return self._send_message('deleteCookie', 'ok', name=name);

    def get_cookie(self, name):
        """
            Get a single cookie by name. Returns the cookie if found, None if not.

            :Usage:
                driver.get_cookie('my_cookie')
        """
        cookies = self.get_cookies()
        for cookie in cookies:
            if cookie['name'] == name:
                return cookie
        return None

    def get_cookies(self):
        return self._send_message("getAllCookies", "value")

    @property
    def application_cache(self):
        return ApplicationCache(self)

    def screenshot(self, element=None, highlights=None):
        if element is not None:
            element = element.id
        return self._send_message("screenShot", 'value', element=element, highlights=highlights)
