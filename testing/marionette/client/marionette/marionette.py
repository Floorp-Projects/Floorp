# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import ConfigParser
import datetime
import os
import socket
import sys
import time
import traceback

from application_cache import ApplicationCache
from client import MarionetteClient
from emulator import Emulator
from emulator_screen import EmulatorScreen
from errors import *
from keys import Keys

import geckoinstance

class HTMLElement(object):
    """
    Represents a DOM Element.
    """

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
        '''
        Returns an HTMLElement instance that matches the specified method and target, relative to the current element.

        For more details on this function, see the find_element method in the
        Marionette class.
        '''
        return self.marionette.find_element(method, target, self.id)

    def find_elements(self, method, target):
        '''
        Returns a list of all HTMLElement instances that match the specified method and target in the current context.

        For more details on this function, see the find_elements method in the
        Marionette class.
        '''
        return self.marionette.find_elements(method, target, self.id)

    def get_attribute(self, attribute):
        '''
        Returns the requested attribute (or None, if no attribute is set).

        :param attribute: The name of the attribute.
        '''
        return self.marionette._send_message('getElementAttribute', 'value', id=self.id, name=attribute)

    def click(self):
        return self.marionette._send_message('clickElement', 'ok', id=self.id)

    def tap(self, x=None, y=None):
        '''
        Simulates a set of tap events on the element.

        :param x: X-coordinate of tap event. If not given, default to the
         center of the element.
        :param x: X-coordinate of tap event. If not given, default to the
         center of the element.
        '''
        return self.marionette._send_message('singleTap', 'ok', id=self.id, x=x, y=y)

    @property
    def text(self):
        '''
        Returns the visible text of the element, and its child elements.
        '''
        return self.marionette._send_message('getElementText', 'value', id=self.id)

    def send_keys(self, *string):
        '''
        Sends the string via synthesized keypresses to the element.
        '''
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
        return self.marionette._send_message('sendKeysToElement', 'ok', id=self.id, value=typing)

    def clear(self):
        '''
        Clears the input of the element.
        '''
        return self.marionette._send_message('clearElement', 'ok', id=self.id)

    def is_selected(self):
        '''
        Returns True if the element is selected.
        '''
        return self.marionette._send_message('isElementSelected', 'value', id=self.id)

    def is_enabled(self):
        '''
        Returns True if the element is enabled.
        '''
        return self.marionette._send_message('isElementEnabled', 'value', id=self.id)

    def is_displayed(self):
        '''
        Returns True if the element is displayed.
        '''
        return self.marionette._send_message('isElementDisplayed', 'value', id=self.id)

    @property
    def size(self):
        '''
        A dictionary with the size of the element.
        '''
        return self.marionette._send_message('getElementSize', 'value', id=self.id)

    @property
    def tag_name(self):
        '''
        The tag name of the element.
        '''
        return self.marionette._send_message('getElementTagName', 'value', id=self.id)

    @property
    def location(self):
        """Get an element's location on the page.

        The returned point will contain the x and y coordinates of the
        top left-hand corner of the given element.  The point (0,0)
        refers to the upper-left corner of the document.

        :returns: a dictionary containing x and y as entries

        """

        return self.marionette._send_message("getElementLocation", "value", id=self.id)

    def value_of_css_property(self, property_name):
        '''
        Gets the value of the specified CSS property name.

        :param property_name: Property name to get the value of.
        '''
        return self.marionette._send_message('getElementValueOfCssProperty', 'value',
                                             id=self.id,
                                             propertyName=property_name)
    def submit(self):
        '''
        Submits if the element is a form or is within a form
        '''
        return self.marionette._send_message('submitElement', 'ok', id=self.id)

class Actions(object):
    '''
    An Action object represents a set of actions that are executed in a particular order.

    All action methods (press, etc.) return the Actions object itself, to make
    it easy to create a chain of events.

    Example usage:

    ::

        # get html file
        testAction = marionette.absolute_url("testFool.html")
        # navigate to the file
        marionette.navigate(testAction)
        # find element1 and element2
        element1 = marionette.find_element("id", "element1")
        element2 = marionette.find_element("id", "element2")
        # create action object
        action = Actions(marionette)
        # add actions (press, wait, move, release) into the object
        action.press(element1).wait(5). move(element2).release()
        # fire all the added events
        action.perform()
    '''

    def __init__(self, marionette):
        self.action_chain = []
        self.marionette = marionette
        self.current_id = None

    def press(self, element, x=None, y=None):
        '''
        Sends a 'touchstart' event to this element.

        If no coordinates are given, it will be targeted at the center of the
        element. If given, it will be targeted at the (x,y) coordinates
        relative to the top-left corner of the element.

        :param element: The element to press on.
        :param x: Optional, x-coordinate to tap, relative to the top-left
         corner of the element.
        :param y: Optional, y-coordinate to tap, relative to the top-left
         corner of the element.
        '''
        element=element.id
        self.action_chain.append(['press', element, x, y])
        return self

    def release(self):
        '''
        Sends a 'touchend' event to this element.

        May only be called if press() has already be called on this element.

        If press and release are chained without a move action between them,
        then it will be processed as a 'tap' event, and will dispatch the
        expected mouse events ('mousemove' (if necessary), 'mousedown',
        'mouseup', 'mouseclick') after the touch events. If there is a wait
        period between press and release that will trigger a contextmenu,
        then the 'contextmenu' menu event will be fired instead of the
        touch/mouse events.
        '''
        self.action_chain.append(['release'])
        return self

    def move(self, element):
        '''
        Sends a 'touchmove' event at the center of the target element.

        :param element: Element to move towards.

        May only be called if press() has already be called.
        '''
        element=element.id
        self.action_chain.append(['move', element])
        return self

    def move_by_offset(self, x, y):
        '''
        Sends 'touchmove' event to the given x, y coordinates relative to the top-left of the currently touched element.

        May only be called if press() has already be called.

        :param x: Specifies x-coordinate of move event, relative to the
         top-left corner of the element.
        :param y: Specifies y-coordinate of move event, relative to the
         top-left corner of the element.
        '''
        self.action_chain.append(['moveByOffset', x, y])
        return self

    def wait(self, time=None):
        '''
        Waits for specified time period.

        :param time: Time in seconds to wait. If time is None then this has no effect for a single action chain. If used inside a multi-action chain, then time being None indicates that we should wait for all other currently executing actions that are part of the chain to complete.
        '''
        self.action_chain.append(['wait', time])
        return self

    def cancel(self):
        '''
        Sends 'touchcancel' event to the target of the original 'touchstart' event.

        May only be called if press() has already be called.
        '''
        self.action_chain.append(['cancel'])
        return self

    def tap(self, element, x=None, y=None):
        '''
        Performs a quick tap on the target element.

        :param element: The element to tap.
        :param x: Optional, x-coordinate of tap, relative to the top-left
         corner of the element. If not specified, default to center of
         element.
        :param y: Optional, y-coordinate of tap, relative to the top-left
         corner of the element. If not specified, default to center of
         element.

        This is equivalent to calling:

        ::

          action.press(element, x, y).release()
        '''
        element=element.id
        self.action_chain.append(['press', element, x, y])
        self.action_chain.append(['release'])
        return self

    def double_tap(self, element, x=None, y=None):
        '''
        Performs a double tap on the target element.

        :param element: The element to double tap.
        :param x: Optional, x-coordinate of double tap, relative to the
         top-left corner of the element.
        :param y: Optional, y-coordinate of double tap, relative to the
         top-left corner of the element.
        '''
        element=element.id
        self.action_chain.append(['press', element, x, y])
        self.action_chain.append(['release'])
        self.action_chain.append(['press', element, x, y])
        self.action_chain.append(['release'])
        return self

    def flick(self, element, x1, y1, x2, y2, duration=200):
        '''
        Performs a flick gesture on the target element.

        :param element: The element to perform the flick gesture on.
        :param x1: Starting x-coordinate of flick, relative to the top left
         corner of the element.
        :param y1: Starting y-coordinate of flick, relative to the top left
         corner of the element.
        :param x2: Ending x-coordinate of flick, relative to the top left
         corner of the element.
        :param y2: Ending y-coordinate of flick, relative to the top left
         corner of the element.
        :param duration: Time needed for the flick gesture for complete (in
         milliseconds).
        '''
        element = element.id
        elapsed = 0
        time_increment = 10
        if time_increment >= duration:
            time_increment = duration
        move_x = time_increment*1.0/duration * (x2 - x1)
        move_y = time_increment*1.0/duration * (y2 - y1)
        self.action_chain.append(['press', element, x1, y1])
        while elapsed < duration:
            elapsed += time_increment
            self.action_chain.append(['moveByOffset', move_x, move_y])
            self.action_chain.append(['wait', time_increment/1000])
        self.action_chain.append(['release'])
        return self

    def long_press(self, element, time_in_seconds):
        '''
        Performs a long press gesture on the target element.

        :param element: The element to press.
        :param time_in_seconds: Time in seconds to wait before releasing the press.
        '''
        element = element.id
        self.action_chain.append(['press', element])
        self.action_chain.append(['wait', time_in_seconds])
        self.action_chain.append(['release'])
        return self

    def perform(self):
        '''
        Sends the action chain built so far to the server side for execution and clears the current chain of actions.
        '''
        self.current_id = self.marionette._send_message('actionChain', 'value', chain=self.action_chain, nextId=self.current_id)
        self.action_chain = []
        return self

class MultiActions(object):
    '''
    A MultiActions object represents a sequence of actions that may be
    performed at the same time. Its intent is to allow the simulation
    of multi-touch gestures.
    Usage example:

    ::

      # create multiaction object
      multitouch = MultiActions(marionette)
      # create several action objects
      action_1 = Actions(marionette)
      action_2 = Actions(marionette)
      # add actions to each action object/finger
      action_1.press(element1).move_to(element2).release()
      action_2.press(element3).wait().release(element3)
      # fire all the added events
      multitouch.add(action_1).add(action_2).perform()
    '''

    def __init__(self, marionette):
        self.multi_actions = []
        self.max_length = 0
        self.marionette = marionette

    def add(self, action):
        '''
        Adds a set of actions to perform.

        :param action: An Actions object.
        '''
        self.multi_actions.append(action.action_chain)
        if len(action.action_chain) > self.max_length:
          self.max_length = len(action.action_chain)
        return self

    def perform(self):
        '''
        Perform all the actions added to this object.
        '''
        return self.marionette._send_message('multiAction', 'ok', value=self.multi_actions, max_length=self.max_length)

class Marionette(object):
    """
    Represents a Marionette connection to a browser or device.
    """

    CONTEXT_CHROME = 'chrome' # non-browser content: windows, dialogs, etc.
    CONTEXT_CONTENT = 'content' # browser content: iframes, divs, etc.
    TIMEOUT_SEARCH = 'implicit'
    TIMEOUT_SCRIPT = 'script'
    TIMEOUT_PAGE = 'page load'
    SCREEN_ORIENTATIONS = {"portrait": EmulatorScreen.SO_PORTRAIT_PRIMARY,
                           "landscape": EmulatorScreen.SO_LANDSCAPE_PRIMARY,
                           "portrait-primary": EmulatorScreen.SO_PORTRAIT_PRIMARY,
                           "landscape-primary": EmulatorScreen.SO_LANDSCAPE_PRIMARY,
                           "portrait-secondary": EmulatorScreen.SO_PORTRAIT_SECONDARY,
                           "landscape-secondary": EmulatorScreen.SO_LANDSCAPE_SECONDARY}

    def __init__(self, host='localhost', port=2828, app=None, app_args=None, bin=None,
                 profile=None, emulator=None, sdcard=None, emulatorBinary=None,
                 emulatorImg=None, emulator_res=None, gecko_path=None,
                 connectToRunningEmulator=False, homedir=None, baseurl=None,
                 noWindow=False, logcat_dir=None, busybox=None, symbols_path=None,
                 timeout=None, device_serial=None):
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
        self.timeout = timeout
        self.device_serial = device_serial

        if bin:
            port = int(self.port)
            if not Marionette.is_port_available(port, host=self.host):
                ex_msg = "%s:%d is unavailable." % (self.host, port)
                raise MarionetteException(message=ex_msg)
            if app:
                # select instance class for the given app
                try:
                    instance_class = geckoinstance.apps[app]
                except KeyError:
                    msg = 'Application "%s" unknown (should be one of %s)'
                    raise NotImplementedError(msg % (app, geckoinstance.apps.keys()))
            else:
                try:
                    config = ConfigParser.RawConfigParser()
                    config.read(os.path.join(os.path.dirname(bin), 'application.ini'))
                    app = config.get('App', 'Name')
                    instance_class = geckoinstance.apps[app.lower()]
                except (ConfigParser.NoOptionError,
                        ConfigParser.NoSectionError,
                        KeyError):
                    instance_class = geckoinstance.GeckoInstance
            self.instance = instance_class(host=self.host, port=self.port,
                                           bin=self.bin, profile=self.profile, app_args=app_args)
            self.instance.start()
            assert(self.wait_for_port()), "Timed out waiting for port!"

        if emulator:
            self.emulator = Emulator(homedir=homedir,
                                     noWindow=self.noWindow,
                                     logcat_dir=self.logcat_dir,
                                     arch=emulator,
                                     sdcard=sdcard,
                                     symbols_path=symbols_path,
                                     emulatorBinary=emulatorBinary,
                                     userdata=emulatorImg,
                                     res=emulator_res)
            self.emulator.start()
            self.port = self.emulator.setup_port_forwarding(self.port)
            assert(self.emulator.wait_for_port()), "Timed out waiting for port!"

        if connectToRunningEmulator:
            self.emulator = Emulator(homedir=homedir,
                                     logcat_dir=self.logcat_dir)
            self.emulator.connect()
            self.port = self.emulator.setup_port_forwarding(self.port)
            assert(self.emulator.wait_for_port()), "Timed out waiting for port!"

        self.client = MarionetteClient(self.host, self.port)

        if emulator:
            self.emulator.setup(self,
                                gecko_path=gecko_path,
                                busybox=busybox)

    def cleanup(self):
        if self.session:
            self.delete_session()
        if self.emulator:
            self.emulator.close()
        if self.instance:
            self.instance.close()
        for qemu in self.extra_emulators:
            qemu.emulator.close()

    def __del__(self):
        self.cleanup()

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

    def wait_for_port(self, timeout=60):
        starttime = datetime.datetime.now()
        while datetime.datetime.now() - starttime < datetime.timedelta(seconds=timeout):
            try:
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                sock.connect((self.host, self.port))
                data = sock.recv(16)
                sock.close()
                if '"from"' in data:
                    time.sleep(5)
                    return True
            except socket.error:
                pass
            time.sleep(1)
        return False

    def _send_message(self, command, response_key, **kwargs):
        if not self.session and command not in ('newSession', 'getStatus'):
            raise MarionetteException(message="Please start a session")

        message = { 'name': command }
        if self.session:
            message['sessionId'] = self.session
        if kwargs:
            message['parameters'] = kwargs

        try:
            response = self.client.send(message)
        except socket.timeout:
            self.session = None
            self.window = None
            self.client.close()
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
        return self.client.send({"name": "emulatorCmdResult",
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
                raise MoveTargetOutOfBoundsException(message=message, status=status, stacktrace=stacktrace)
            elif status == ErrorCodes.FRAME_SEND_NOT_INITIALIZED_ERROR:
                raise FrameSendNotInitializedError(message=message, status=status, stacktrace=stacktrace)
            elif status == ErrorCodes.FRAME_SEND_FAILURE_ERROR:
                raise FrameSendFailureError(message=message, status=status, stacktrace=stacktrace)
            else:
                raise MarionetteException(message=message, status=status, stacktrace=stacktrace)
        raise MarionetteException(message=response, status=500)

    def check_for_crash(self):
        returncode = None
        name = None
        crashed = False
        if self.emulator:
            if self.emulator.check_for_crash():
                returncode = self.emulator.proc.returncode
                name = 'emulator'
                crashed = True

            if self.emulator.check_for_minidumps():
                crashed = True
        elif self.instance:
            # In the future, a check for crashed Firefox processes
            # should be here.
            pass
        if returncode is not None:
            print ('PROCESS-CRASH | %s | abnormal termination with exit code %d' %
                (name, returncode))
        return crashed

    def absolute_url(self, relative_url):
        '''
        Returns an absolute url for files served from Marionette's www directory.

        :param relative_url: The url of a static file, relative to Marionette's www directory.
        '''
        return "%s%s" % (self.baseurl, relative_url)

    def status(self):
        return self._send_message('getStatus', 'value')

    def start_session(self, desired_capabilities=None):
        '''
        Creates a new Marionette session.

        You must call this method before performing any other action.
        '''
        try:
            # We are ignoring desired_capabilities, at least for now.
            self.session = self._send_message('newSession', 'value')
        except:
            exc, val, tb = sys.exc_info()
            self.check_for_crash()
            raise exc, val, tb

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
        '''
        A JSON dictionary representing the capabilities of the current session.
        '''
        response = self._send_message('getSessionCapabilities', 'value')
        return response

    def set_script_timeout(self, timeout):
        '''
        Sets the maximum number of ms that an asynchronous script is allowed to run.

        If a script does not return in the specified amount of time, a
        ScriptTimeoutException is raised.

        :param timeout: The maximum number of milliseconds an asynchronous
         script can run without causing an ScriptTimeoutException to be raised
        '''
        response = self._send_message('setScriptTimeout', 'ok', ms=timeout)
        return response

    def set_search_timeout(self, timeout):
        '''
        Sets a timeout for the find methods.

        When searching for an element using either
        :class:`Marionette.find_element` or :class:`Marionette.find_elements`,
        the method will continue trying to locate the element for up to timeout
        ms. This can be useful if, for example, the element you're looking for
        might not exist immediately, because it belongs to a page which is
        currently being loaded.

        :param timeout: Timeout in milliseconds.
        '''
        response = self._send_message('setSearchTimeout', 'ok', ms=timeout)
        return response

    @property
    def current_window_handle(self):
        """Get the current window's handle.

        Return an opaque server-assigned identifier to this window
        that uniquely identifies it within this Marionette instance.
        This can be used to switch to this window at a later point.

        :returns: unique window handle
        :rtype: string

        """

        self.window = self._send_message("getCurrentWindowHandle", "value")
        return self.window

    @property
    def title(self):
        '''
        Current title of the active window.
        '''
        response = self._send_message('getTitle', 'value')
        return response

    @property
    def window_handles(self):
        '''
        A list of references to all available browser windows if called in
        content context. If called while in the chrome context, it will list
        all available windows, not just browser windows (ie: not just
        'navigator:browser';).
        '''
        response = self._send_message('getWindows', 'value')
        return response

    @property
    def page_source(self):
        '''
        A string representation of the DOM.
        '''
        response = self._send_message('getPageSource', 'value')
        return response

    def close(self):
        """Close the current window, ending the session if it's the last
        window currently open.

        On B2G this method is a noop and will return immediately.

        """

        response = self._send_message("close", "ok")
        return response

    def set_context(self, context):
        '''
        Sets the context that marionette commands are running in.

        :param context: Context, may be one of the class properties
         `CONTEXT_CHROME` or `CONTEXT_CONTENT`.

        Usage example:

        ::

          marionette.set_context(marionette.CONTEXT_CHROME)
        '''
        assert(context == self.CONTEXT_CHROME or context == self.CONTEXT_CONTENT)
        return self._send_message('setContext', 'ok', value=context)

    def switch_to_window(self, window_id):
        '''
        Switch to the specified window; subsequent commands will be directed at the new window.

        :param window_id: The id or name of the window to switch to.
        '''
        response = self._send_message('switchToWindow', 'ok', name=window_id)
        self.window = window_id
        return response

    def get_active_frame(self):
        '''
        Returns an HTMLElement representing the frame Marionette is currently acting on.
        '''
        response = self._send_message('getActiveFrame', 'value')
        if response:
            return HTMLElement(self, response)
        return None

    def switch_to_default_content(self):
        '''
        Switch the current context to page's default content.
        '''
        return self.switch_to_frame()

    def switch_to_frame(self, frame=None, focus=True):
        '''
        Switch the current context to the specified frame. Subsequent commands will operate in the context of the specified frame, if applicable.

        :param frame: A reference to the frame to switch to: this can be an HTMLElement, an index, name or an id attribute. If you call switch_to_frame() without an argument, it will switch to the top-level frame.
        :param focus: A boolean value which determins whether to focus the frame that we just switched to.
        '''
        if isinstance(frame, HTMLElement):
            response = self._send_message('switchToFrame', 'ok', element=frame.id, focus=focus)
        else:
            response = self._send_message('switchToFrame', 'ok', id=frame, focus=focus)
        return response

    def get_url(self):
        """Get a string representing the current URL.

        On Desktop this returns a string representation of the URL of
        the current top level browsing context.  This is equivalent to
        document.location.href.

        When in the context of the chrome, this returns the canonical
        URL of the current resource.

        :returns: string representation of URL

        """

        response = self._send_message("getCurrentUrl", "value")
        return response

    def get_window_type(self):
        '''
        Gets the windowtype attribute of the window Marionette is currently acting on.

        This command only makes sense in a chrome context. You might use this
        method to distinguish a browser window from an editor window.
        '''
        response = self._send_message('getWindowType', 'value')
        return response

    def navigate(self, url):
        '''
        Causes the browser to navigate to the specified url.

        :param url: The url to navigate to.
        '''
        response = self._send_message('goUrl', 'ok', url=url)
        return response

    def timeouts(self, timeout_type, ms):
        assert(timeout_type == self.TIMEOUT_SEARCH or timeout_type == self.TIMEOUT_SCRIPT or timeout_type == self.TIMEOUT_PAGE)
        response = self._send_message('timeouts', 'ok', type=timeout_type, ms=ms)
        return response

    def go_back(self):
        '''
        Causes the browser to perform a back navigation.
        '''
        response = self._send_message('goBack', 'ok')
        return response

    def go_forward(self):
        '''
        Causes the browser to perform a forward navigation.
        '''
        response = self._send_message('goForward', 'ok')
        return response

    def refresh(self):
        '''
        Causes the browser to perform to refresh the current page.
        '''
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

    def execute_js_script(self, script, script_args=None, async=True,
                          new_sandbox=True, special_powers=False,
                          script_timeout=None, inactivity_timeout=None,
                          filename=None):
        if script_args is None:
            script_args = []
        args = self.wrapArguments(script_args)
        response = self._send_message('executeJSScript',
                                      'value',
                                      script=script,
                                      args=args,
                                      async=async,
                                      newSandbox=new_sandbox,
                                      specialPowers=special_powers,
                                      scriptTimeout=script_timeout,
                                      inactivityTimeout=inactivity_timeout,
                                      filename=filename,
                                      line=None)
        return self.unwrapValue(response)

    def execute_script(self, script, script_args=None, new_sandbox=True,
                       special_powers=False, script_timeout=None):
        '''
        Executes a synchronous JavaScript script, and returns the result (or None if the script does return a value).

        The script is executed in the context set by the most recent
        set_context() call, or to the CONTEXT_CONTENT context if set_context()
        has not been called.

        :param script: A string containing the JavaScript to execute.
        :param script_args: A list of arguments to pass to the script.
        :param special_powers: Whether or not you want access to SpecialPowers
         in your script. Set to False by default because it shouldn't really
         be used, since you already have access to chrome-level commands if you
         set context to chrome and do an execute_script. This method was added
         only to help us run existing Mochitests.
        :param new_sandbox: If False, preserve global variables from the last
         execute_*script call. This is True by default, in which case no
         globals are preserved.

        Simple usage example:

        ::

          result = marionette.execute_script("return 1;")
          assert result == 1

        You can use the `script_args` parameter to pass arguments to the
        script:

        ::

          result = marionette.execute_script("return arguments[0] + arguments[1];",
                                             script_args=[2, 3])
          assert result == 5
          some_element = marionette.find_element("id", "someElement")
          sid = marionette.execute_script("return arguments[0].id;", script_args=[some_element])
          assert some_element.get_attribute("id") == sid

        Scripts wishing to access non-standard properties of the window object must use
        window.wrappedJSObject:

        ::

          result = marionette.execute_script("""
            window.wrappedJSObject.test1 = 'foo';
            window.wrappedJSObject.test2 = 'bar';
            return window.wrappedJSObject.test1 + window.wrappedJSObject.test2;
            """)
          assert result == "foobar"

        Global variables set by individual scripts do not persist between
        script calls by default.  If you wish to persist data between script
        calls, you can set new_sandbox to False on your next call, and add any
        new variables to a new 'global' object like this:

        ::

          marionette.execute_script("global.test1 = 'foo';")
          result = self.marionette.execute_script("return global.test1;", new_sandbox=False)
          assert result == 'foo'

        '''
        if script_args is None:
            script_args = []
        args = self.wrapArguments(script_args)
        stack = traceback.extract_stack()
        frame = stack[-2:-1][0] # grab the second-to-last frame
        response = self._send_message('executeScript',
                                      'value',
                                      script=script,
                                      args=args,
                                      newSandbox=new_sandbox,
                                      specialPowers=special_powers,
                                      scriptTimeout=script_timeout,
                                      line=int(frame[1]),
                                      filename=os.path.basename(frame[0]))
        return self.unwrapValue(response)

    def execute_async_script(self, script, script_args=None, new_sandbox=True, special_powers=False, script_timeout=None):
        '''
        Executes an asynchronous JavaScript script, and returns the result (or None if the script does return a value).

        The script is executed in the context set by the most recent
        set_context() call, or to the CONTEXT_CONTENT context if set_context()
        has not been called.

        :param script: A string containing the JavaScript to execute.
        :param script_args: A list of arguments to pass to the script.
        :param special_powers: Whether or not you want access to SpecialPowers
         in your script. Set to False by default because it shouldn't really
         be used, since you already have access to chrome-level commands if you
         set context to chrome and do an execute_script. This method was added
         only to help us run existing Mochitests.
        :param new_sandbox: If False, preserve global variables from the last
         execute_*script call. This is True by default, in which case no
         globals are preserved.

        Usage example:

        ::

          marionette.set_script_timeout(10000) # set timeout period of 10 seconds
          result = self.marionette.execute_async_script("""
            // this script waits 5 seconds, and then returns the number 1
            setTimeout(function() {
              marionetteScriptFinished(1);
            }, 5000);
          """)
          assert result == 1
        '''
        if script_args is None:
            script_args = []
        args = self.wrapArguments(script_args)
        stack = traceback.extract_stack()
        frame = stack[-2:-1][0] # grab the second-to-last frame
        response = self._send_message('executeAsyncScript',
                                      'value',
                                      script=script,
                                      args=args,
                                      newSandbox=new_sandbox,
                                      specialPowers=special_powers,
                                      scriptTimeout=script_timeout,
                                      line=int(frame[1]),
                                      filename=os.path.basename(frame[0]))
        return self.unwrapValue(response)

    def find_element(self, method, target, id=None):
        '''
        Returns an HTMLElement instances that matches the specified method and target in the current context.

        An HTMLElement instance may be used to call other methods on the
        element, such as click().  If no element is immediately found, the
        attempt to locate an element will be repeated for up to the amount of
        time set by set_search_timeout(). If multiple elements match the given
        criteria, only the first is returned. If no element matches, a
        NoSuchElementException will be raised.

        :param method: The method to use to locate the element; one of: "id",
         "name", "class name", "tag name", "css selector", "link text",
         "partial link text" and "xpath". Note that the methods supported in
         the chrome dom are only "id", "class name", "tag name" and "xpath".
        :param target: The target of the search.  For example, if method =
         "tag", target might equal "div".  If method = "id", target would be
         an element id.
        :param id: If specified, search for elements only inside the element
         with the specified id.
        '''
        kwargs = { 'value': target, 'using': method }
        if id:
            kwargs['element'] = id
        response = self._send_message('findElement', 'value', **kwargs)
        element = HTMLElement(self, response['ELEMENT'])
        return element

    def find_elements(self, method, target, id=None):
        '''
        Returns a list of all HTMLElement instances that match the specified method and target in the current context.

        An HTMLElement instance may be used to call other methods on the
        element, such as click().  If no element is immediately found, the
        attempt to locate an element will be repeated for up to the amount of
        time set by set_search_timeout().

        :param method: The method to use to locate the elements; one of:
         "id", "name", "class name", "tag name", "css selector", "link text",
         "partial link text" and "xpath". Note that the methods supported in
         the chrome dom are only "id", "class name", "tag name" and "xpath".
        :param target: The target of the search.  For example, if method =
         "tag", target might equal "div".  If method = "id", target would be
         an element id.
        :param id: If specified, search for elements only inside the element
         with the specified id.
        '''
        kwargs = { 'value': target, 'using': method }
        if id:
            kwargs['element'] = id
        response = self._send_message('findElements', 'value', **kwargs)
        assert(isinstance(response, list))
        elements = []
        for x in response:
            elements.append(HTMLElement(self, x))
        return elements

    def get_active_element(self):
        response = self._send_message('getActiveElement', 'value')
        return HTMLElement(self, response)

    def log(self, msg, level=None):
        '''
        Stores a timestamped log message in the Marionette server for later retrieval.

        :param msg: String with message to log.
        :param level: String with log level (e.g. "INFO" or "DEBUG"). If None,
         defaults to "INFO".
        '''
        return self._send_message('log', 'ok', value=msg, level=level)

    def get_logs(self):
        '''
        Returns the list of logged messages.

        Each log message is an array with three string elements: the level,
        the message, and a date.

        Usage example:

        ::

          marionette.log("I AM INFO")
          marionette.log("I AM ERROR", "ERROR")
          logs = marionette.get_logs()
          assert logs[0][1] == "I AM INFO"
          assert logs[1][1] == "I AM ERROR"
        '''
        return self._send_message('getLogs', 'value')

    def import_script(self, js_file):
        '''
        Imports a script into the scope of the execute_script and execute_async_script calls.

        This is particularly useful if you wish to import your own libraries.

        :param js_file: Filename of JavaScript file to import.

        For example, Say you have a script, importfunc.js, that contains:

        ::

          let testFunc = function() { return "i'm a test function!";};

        Assuming this file is in the same directory as the test, you could do
        something like:

        ::

          js = os.path.abspath(os.path.join(__file__, os.path.pardir, "importfunc.js"))
          marionette.import_script(js)
          assert "i'm a test function!" == self.marionette.execute_script("return testFunc();")
        '''
        js = ''
        with open(js_file, 'r') as f:
            js = f.read()
        return self._send_message('importScript', 'ok', script=js)

    def clear_imported_scripts(self):
        '''
        Clears all imported scripts in this context, ie: calling clear_imported_scripts in chrome
        context will clear only scripts you imported in chrome, and will leave the scripts
        you imported in content context.
        '''
        return self._send_message('clearImportedScripts', 'ok')

    def add_cookie(self, cookie):
        """
        Adds a cookie to your current session.

        :param cookie: A dictionary object, with required keys - "name" and
         "value"; optional keys - "path", "domain", "secure", "expiry".

        Usage example:

        ::

          driver.add_cookie({'name': 'foo', 'value': 'bar'})
          driver.add_cookie({'name': 'foo', 'value': 'bar', 'path': '/'})
          driver.add_cookie({'name': 'foo', 'value': 'bar', 'path': '/',
                             'secure': True})
        """
        return self._send_message('addCookie', 'ok', cookie=cookie)

    def delete_all_cookies(self):
        """
        Delete all cookies in the scope of the current session.

        Usage example:

        ::

          driver.delete_all_cookies()
        """
        return self._send_message('deleteAllCookies', 'ok')

    def delete_cookie(self, name):
        """
        Delete a cookie by its name.

        :param name: Name of cookie to delete.

        Usage example:

        ::

          driver.delete_cookie('foo')
        """
        return self._send_message('deleteCookie', 'ok', name=name);

    def get_cookie(self, name):
        """
        Get a single cookie by name. Returns the cookie if found, None if not.

        :param name: Name of cookie to get.
        """
        cookies = self.get_cookies()
        for cookie in cookies:
            if cookie['name'] == name:
                return cookie
        return None

    def get_cookies(self):
        """Get all the cookies for the current domain.

        This is the equivalent of calling `document.cookie` and
        parsing the result.

        :returns: A set of cookies for the current domain.

        """

        return self._send_message("getCookies", "value")

    @property
    def application_cache(self):
        return ApplicationCache(self)

    def screenshot(self, element=None, highlights=None):
        '''
        Creates a base64-encoded screenshot of the element, or the current frame if no element is specified.

        :param element: The element to take a screenshot of. If None, will
         take a screenshot of the current frame.
        :param highlights: A list of HTMLElement objects to draw a red box around in the
         returned screenshot.
        '''
        if element is not None:
            element = element.id
        lights = None
        if highlights is not None:
            lights = [highlight.id for highlight in highlights if highlights]
        return self._send_message("screenShot", 'value', id=element, highlights=lights)

    @property
    def orientation(self):
        """Get the current browser orientation.

        Will return one of the valid primary orientation values
        portrait-primary, landscape-primary, portrait-secondary, or
        landscape-secondary.

        """
        return self._send_message("getScreenOrientation", "value")

    def set_orientation(self, orientation):
        """Set the current browser orientation.

        The supplied orientation should be given as one of the valid
        orientation values.  If the orientation is unknown, an error
        will be raised.

        Valid orientations are "portrait" and "landscape", which fall
        back to "portrait-primary" and "landscape-primary"
        respectively, and "portrait-secondary" as well as
        "landscape-secondary".

        :param orientation: The orientation to lock the screen in.

        """
        self._send_message("setScreenOrientation", "ok", orientation=orientation)
        if self.emulator:
            self.emulator.screen.orientation = self.SCREEN_ORIENTATIONS[orientation.lower()]
