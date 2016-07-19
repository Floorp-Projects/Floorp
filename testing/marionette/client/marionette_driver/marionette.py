# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import base64
import ConfigParser
import json
import os
import socket
import traceback
import warnings

from contextlib import contextmanager

from decorators import do_crash_check
from keys import Keys

import geckoinstance
import errors
import transport

WEBELEMENT_KEY = "ELEMENT"
W3C_WEBELEMENT_KEY = "element-6066-11e4-a52e-4f735466cecf"


class HTMLElement(object):
    """
    Represents a DOM Element.
    """

    def __init__(self, marionette, id):
        self.marionette = marionette
        assert(id is not None)
        self.id = id

    def __str__(self):
        return self.id

    def __eq__(self, other_element):
        return self.id == other_element.id

    def find_element(self, method, target):
        """Returns an ``HTMLElement`` instance that matches the specified
        method and target, relative to the current element.

        For more details on this function, see the `find_element` method
        in the Marionette class.
        """
        return self.marionette.find_element(method, target, self.id)

    def find_elements(self, method, target):
        """Returns a list of all ``HTMLElement`` instances that match the
        specified method and target in the current context.

        For more details on this function, see the find_elements method
        in the Marionette class.
        """
        return self.marionette.find_elements(method, target, self.id)

    def get_attribute(self, name):
        """Returns the requested attribute, or None if no attribute
        is set.
        """
        body = {"id": self.id, "name": name}
        return self.marionette._send_message("getElementAttribute", body, key="value")

    def get_property(self, name):
        """Returns the requested property, or None if the property is
        not set.
        """
        body = {"id": self.id, "name": name}
        return self.marionette._send_message("getElementProperty", body, key="value")

    def click(self):
        self.marionette._send_message("clickElement", {"id": self.id})

    def tap(self, x=None, y=None):
        """Simulates a set of tap events on the element.

        :param x: X coordinate of tap event.  If not given, default to
            the centre of the element.
        :param y: Y coordinate of tap event. If not given, default to
            the centre of the element.
        """
        body = {"id": self.id, "x": x, "y": y}
        self.marionette._send_message("singleTap", body)

    @property
    def text(self):
        """Returns the visible text of the element, and its child elements."""
        body = {"id": self.id}
        return self.marionette._send_message("getElementText", body, key="value")

    def send_keys(self, *string):
        """Sends the string via synthesized keypresses to the element."""
        keys = Marionette.convert_keys(*string)
        body = {"id": self.id, "value": keys}
        self.marionette._send_message("sendKeysToElement", body)

    def clear(self):
        """Clears the input of the element."""
        self.marionette._send_message("clearElement", {"id": self.id})

    def is_selected(self):
        """Returns True if the element is selected."""
        body = {"id": self.id}
        return self.marionette._send_message("isElementSelected", body, key="value")

    def is_enabled(self):
        """This command will return False if all the following criteria
        are met otherwise return True:

        * A form control is disabled.
        * A ``HTMLElement`` has a disabled boolean attribute.
        """
        body = {"id": self.id}
        return self.marionette._send_message("isElementEnabled", body, key="value")

    def is_displayed(self):
        """Returns True if the element is displayed, False otherwise."""
        body = {"id": self.id}
        return self.marionette._send_message("isElementDisplayed", body, key="value")

    @property
    def size(self):
        """A dictionary with the size of the element."""
        warnings.warn("The size property has been deprecated and will be removed in a future version. \
            Please use HTMLElement#rect", DeprecationWarning)
        rect = self.rect
        return {"width": rect["width"], "height": rect["height"]}

    @property
    def tag_name(self):
        """The tag name of the element."""
        body = {"id": self.id}
        return self.marionette._send_message("getElementTagName", body, key="value")

    @property
    def location(self):
        """Get an element's location on the page.

        The returned point will contain the x and y coordinates of the
        top left-hand corner of the given element.  The point (0,0)
        refers to the upper-left corner of the document.

        :returns: a dictionary containing x and y as entries
        """
        warnings.warn("The location property has been deprecated and will be removed in a future version. \
            Please use HTMLElement#rect", DeprecationWarning)
        rect = self.rect
        return {"x": rect["x"], "y": rect["y"]}

    @property
    def rect(self):
        """Gets the element's bounding rectangle.

        This will return a dictionary with the following:

          * x and y represent the top left coordinates of the ``HTMLElement``
            relative to top left corner of the document.
          * height and the width will contain the height and the width
            of the DOMRect of the ``HTMLElement``.
        """
        body = {"id": self.id}
        return self.marionette._send_message(
            "getElementRect", body, key="value" if self.marionette.protocol == 1 else None)

    def value_of_css_property(self, property_name):
        """Gets the value of the specified CSS property name.

        :param property_name: Property name to get the value of.
        """
        body = {"id": self.id, "propertyName": property_name}
        return self.marionette._send_message(
            "getElementValueOfCssProperty", body, key="value")


class MouseButton(object):
    """Enum-like class for mouse button constants."""
    LEFT = 0
    MIDDLE = 1
    RIGHT = 2


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
        element1 = marionette.find_element(By.ID, "element1")
        element2 = marionette.find_element(By.ID, "element2")
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
        element = element.id
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
        element = element.id
        self.action_chain.append(['move', element])
        return self

    def move_by_offset(self, x, y):
        '''
        Sends 'touchmove' event to the given x, y coordinates relative to the
        top-left of the currently touched element.

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

        :param time: Time in seconds to wait. If time is None then this has no effect
                     for a single action chain. If used inside a multi-action chain,
                     then time being None indicates that we should wait for all other
                     currently executing actions that are part of the chain to complete.
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
        element = element.id
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
        element = element.id
        self.action_chain.append(['press', element, x, y])
        self.action_chain.append(['release'])
        self.action_chain.append(['press', element, x, y])
        self.action_chain.append(['release'])
        return self

    def click(self, element, button=MouseButton.LEFT, count=1):
        '''
        Performs a click with additional parameters to allow for double clicking,
        right click, middle click, etc.

        :param element: The element to click.
        :param button: The mouse button to click (indexed from 0, left to right).
        :param count: Optional, the count of clicks to synthesize (for double
                      click events).
        '''
        el = element.id
        self.action_chain.append(['click', el, button, count])
        return self

    def context_click(self, element):
        '''
        Performs a context click on the specified element.

        :param element: The element to context click.
        '''
        return self.click(element, button=MouseButton.RIGHT)

    def middle_click(self, element):
        '''
        Performs a middle click on the specified element.

        :param element: The element to middle click.
        '''
        return self.click(element, button=MouseButton.MIDDLE)

    def double_click(self, element):
        '''
        Performs a double click on the specified element.

        :param element: The element to double click.
        '''
        return self.click(element, count=2)

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

    def long_press(self, element, time_in_seconds, x=None, y=None):
        '''
        Performs a long press gesture on the target element.

        :param element: The element to press.
        :param time_in_seconds: Time in seconds to wait before releasing the press.
        :param x: Optional, x-coordinate to tap, relative to the top-left
         corner of the element.
        :param y: Optional, y-coordinate to tap, relative to the top-left
         corner of the element.

        This is equivalent to calling:

        ::

          action.press(element, x, y).wait(time_in_seconds).release()

        '''
        element = element.id
        self.action_chain.append(['press', element, x, y])
        self.action_chain.append(['wait', time_in_seconds])
        self.action_chain.append(['release'])
        return self

    def key_down(self, key_code):
        """
        Perform a "keyDown" action for the given key code. Modifier keys are
        respected by the server for the course of an action chain.

        :param key_code: The key to press as a result of this action.
        """
        self.action_chain.append(['keyDown', key_code])
        return self

    def key_up(self, key_code):
        """
        Perform a "keyUp" action for the given key code. Modifier keys are
        respected by the server for the course of an action chain.
        :param key_up: The key to release as a result of this action.
        """
        self.action_chain.append(['keyUp', key_code])
        return self

    def perform(self):
        """Sends the action chain built so far to the server side for
        execution and clears the current chain of actions."""
        body = {"chain": self.action_chain, "nextId": self.current_id}
        self.current_id = self.marionette._send_message("actionChain", body, key="value")
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
        """Perform all the actions added to this object."""
        body = {"value": self.multi_actions, "max_length": self.max_length}
        self.marionette._send_message("multiAction", body)


class Alert(object):
    """A class for interacting with alerts.

    ::

        Alert(marionette).accept()
        Alert(merionette).dismiss()
    """

    def __init__(self, marionette):
        self.marionette = marionette

    def accept(self):
        """Accept a currently displayed modal dialog."""
        self.marionette._send_message("acceptDialog")

    def dismiss(self):
        """Dismiss a currently displayed modal dialog."""
        self.marionette._send_message("dismissDialog")

    @property
    def text(self):
        """Return the currently displayed text in a tab modal."""
        return self.marionette._send_message("getTextFromDialog", key="value")

    def send_keys(self, *string):
        """Send keys to the currently displayed text input area in an open
        tab modal dialog."""
        body = {"value": Marionette.convert_keys(*string)}
        self.marionette._send_message("sendKeysToDialog", body)


class Marionette(object):
    """Represents a Marionette connection to a browser or device."""

    CONTEXT_CHROME = 'chrome'  # non-browser content: windows, dialogs, etc.
    CONTEXT_CONTENT = 'content'  # browser content: iframes, divs, etc.
    TIMEOUT_SEARCH = 'implicit'
    TIMEOUT_SCRIPT = 'script'
    TIMEOUT_PAGE = 'page load'
    DEFAULT_SOCKET_TIMEOUT = 360
    DEFAULT_STARTUP_TIMEOUT = 120

    def __init__(self, host='localhost', port=2828, app=None, bin=None,
                 baseurl=None, timeout=None, socket_timeout=DEFAULT_SOCKET_TIMEOUT,
                 startup_timeout=None, **instance_args):
        """
        :param host: address for Marionette connection
        :param port: integer port for Marionette connection
        :param baseurl: where to look for files served from Marionette's www directory
        :param startup_timeout: seconds to wait for a connection with binary
        :param timeout: time to wait for page load, scripts, search
        :param socket_timeout: timeout for Marionette socket operations
        :param bin: path to app binary; if any truthy value is given this will
            attempt to start a gecko instance with the specified `app`
        :param app: type of instance_class to use for managing app instance.
            See marionette_driver.geckoinstance
        :param instance_args: args to pass to instance_class
        """
        self.host = host
        self.port = self.local_port = int(port)
        self.bin = bin
        self.instance = None
        self.session = None
        self.session_id = None
        self.window = None
        self.chrome_window = None
        self.baseurl = baseurl
        self._test_name = None
        self.timeout = timeout
        self.socket_timeout = socket_timeout

        startup_timeout = startup_timeout or self.DEFAULT_STARTUP_TIMEOUT
        if self.bin:
            self.instance = self._create_instance(app, instance_args)
            self.instance.start()
            self.raise_for_port(self.wait_for_port(timeout=startup_timeout))

    def _create_instance(self, app, instance_args):
        if not Marionette.is_port_available(self.port, host=self.host):
            ex_msg = "%s:%d is unavailable." % (self.host, self.port)
            raise errors.MarionetteException(message=ex_msg)
        if app:
            # select instance class for the given app
            try:
                instance_class = geckoinstance.apps[app]
            except KeyError:
                msg = 'Application "%s" unknown (should be one of %s)'
                raise NotImplementedError(
                    msg % (app, geckoinstance.apps.keys()))
        else:
            try:
                if not isinstance(self.bin, basestring):
                    raise TypeError("bin must be a string if app is not specified")
                config = ConfigParser.RawConfigParser()
                config.read(os.path.join(os.path.dirname(self.bin),
                                         'application.ini'))
                app = config.get('App', 'Name')
                instance_class = geckoinstance.apps[app.lower()]
            except (ConfigParser.NoOptionError,
                    ConfigParser.NoSectionError,
                    KeyError):
                instance_class = geckoinstance.GeckoInstance
        return instance_class(host=self.host, port=self.port, bin=self.bin,
                              **instance_args)

    @property
    def profile_path(self):
        if self.instance and self.instance.profile:
            return self.instance.profile.profile

    def cleanup(self):
        if self.session:
            try:
                self.delete_session()
            except (errors.MarionetteException, socket.error, IOError):
                # These exceptions get thrown if the Marionette server
                # hit an exception/died or the connection died. We can
                # do no further server-side cleanup in this case.
                pass
            self.session = None
        if self.instance:
            self.instance.close()

    def __del__(self):
        self.cleanup()

    @staticmethod
    def is_port_available(port, host=''):
        port = int(port)
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        try:
            s.bind((host, port))
            return True
        except socket.error:
            return False
        finally:
            s.close()

    def wait_for_port(self, timeout=None):
        timeout = timeout or self.DEFAULT_STARTUP_TIMEOUT
        return transport.wait_for_port(self.host, self.port, timeout=timeout)

    @do_crash_check
    def raise_for_port(self, port_obtained):
        if not port_obtained:
            raise IOError("Timed out waiting for port!")

    @do_crash_check
    def _send_message(self, name, params=None, key=None):
        """Send a blocking message to the server.

        Marionette provides an asynchronous, non-blocking interface and
        this attempts to paper over this by providing a synchronous API
        to the user.

        :param name: Requested command key.
        :param params: Optional dictionary of key/value arguments.
        :param key: Optional key to extract from response.

        :returns: Full response from the server, or if `key` is given,
            the value of said key in the response.
        """

        if not self.session_id and name != "newSession":
            raise errors.MarionetteException("Please start a session")

        try:
            if self.protocol < 3:
                data = {"name": name}
                if params:
                    data["parameters"] = params
                self.client.send(data)
                msg = self.client.receive()

            else:
                msg = self.client.request(name, params)

        except IOError:
            if self.instance:
                # If we've launched the binary we've connected to, wait
                # for it to shut down.
                returncode = self.instance.runner.wait(timeout=self.DEFAULT_STARTUP_TIMEOUT)
                raise IOError("process died with returncode %s" % returncode)
            raise

        except socket.timeout:
            self.session = None
            self.window = None
            self.client.close()
            raise errors.TimeoutException("Connection timed out")

        res, err = msg.result, msg.error
        if err:
            self._handle_error(err)

        if key is not None:
            return self._unwrap_response(res.get(key))
        else:
            return self._unwrap_response(res)

    def _unwrap_response(self, value):
        if isinstance(value, dict) and (WEBELEMENT_KEY in value or
                                        W3C_WEBELEMENT_KEY in value):
            if value.get(WEBELEMENT_KEY):
                return HTMLElement(self, value.get(WEBELEMENT_KEY))
            else:
                return HTMLElement(self, value.get(W3C_WEBELEMENT_KEY))
        elif isinstance(value, list):
            return list(self._unwrap_response(item) for item in value)
        else:
            return value

    def _handle_error(self, obj):
        if self.protocol == 1:
            if "error" not in obj or not isinstance(obj["error"], dict):
                raise errors.MarionetteException(
                    "Malformed packet, expected key 'error' to be a dict: %s" % obj)
            error = obj["error"].get("status")
            message = obj["error"].get("message")
            stacktrace = obj["error"].get("stacktrace")

        else:
            error = obj["error"]
            message = obj["message"]
            stacktrace = obj["stacktrace"]

        raise errors.lookup(error)(message, stacktrace=stacktrace)

    def reset_timeouts(self):
        if self.timeout is not None:
            self.timeouts(self.TIMEOUT_SEARCH, self.timeout)
            self.timeouts(self.TIMEOUT_SCRIPT, self.timeout)
            self.timeouts(self.TIMEOUT_PAGE, self.timeout)
        else:
            self.timeouts(self.TIMEOUT_PAGE, 30000)

    def check_for_crash(self):
        returncode = None
        name = None
        crashed = False
        if self.instance:
            if self.instance.runner.check_for_crashes(
                    test_name=self.test_name):
                crashed = True
        if returncode is not None:
            print ('PROCESS-CRASH | %s | abnormal termination with exit code %d' %
                   (name, returncode))
        return crashed

    @staticmethod
    def convert_keys(*string):
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
        return typing

    def get_permission(self, perm):
        script = """
        let value = {
          'url': document.nodePrincipal.URI.spec,
          'appId': document.nodePrincipal.appId,
          'isInIsolatedMozBrowserElement': document.nodePrincipal.isInIsolatedMozBrowserElement,
          'type': arguments[0]
        };
        return value;"""
        with self.using_context('content'):
            value = self.execute_script(script, script_args=[perm], sandbox='system')

        with self.using_context('chrome'):
            permission = self.execute_script("""
                Components.utils.import("resource://gre/modules/Services.jsm");
                let perm = arguments[0];
                let secMan = Services.scriptSecurityManager;
                let attrs = {appId: perm.appId,
                            inIsolatedMozBrowser: perm.isInIsolatedMozBrowserElement};
                let principal = secMan.createCodebasePrincipal(
                                Services.io.newURI(perm.url, null, null),
                                attrs);
                let testPerm = Services.perms.testPermissionFromPrincipal(
                               principal, perm.type);
                return testPerm;
                """, script_args=[value])
        return permission

    def push_permission(self, perm, allow):
        script = """
        let allow = arguments[0];
        if (typeof(allow) == "boolean") {
            if (allow) {
              allow = Components.interfaces.nsIPermissionManager.ALLOW_ACTION;
            }
            else {
              allow = Components.interfaces.nsIPermissionManager.DENY_ACTION;
            }
        }
        let perm_type = arguments[1];

        Components.utils.import("resource://gre/modules/Services.jsm");
        window.wrappedJSObject.permChanged = false;
        window.wrappedJSObject.permObserver = function(subject, topic, data) {
          if (topic == "perm-changed") {
            let permission = subject.QueryInterface(Components.interfaces.nsIPermission);
            if (perm_type == permission.type) {
              Services.obs.removeObserver(window.wrappedJSObject.permObserver,
                                          "perm-changed");
              window.wrappedJSObject.permChanged = true;
            }
          }
        };
        Services.obs.addObserver(window.wrappedJSObject.permObserver,
                                 "perm-changed", false);

        let value = {
          'url': document.nodePrincipal.URI.spec,
          'appId': document.nodePrincipal.appId,
          'isInIsolatedMozBrowserElement': document.nodePrincipal.isInIsolatedMozBrowserElement,
          'type': perm_type,
          'action': allow
        };
        return value;
        """
        with self.using_context('content'):
            perm = self.execute_script(script, script_args=[allow, perm], sandbox='system')

        current_perm = self.get_permission(perm['type'])
        if current_perm == perm['action']:
            with self.using_context('content'):
                self.execute_script("""
                    Components.utils.import("resource://gre/modules/Services.jsm");
                    Services.obs.removeObserver(window.wrappedJSObject.permObserver,
                                                "perm-changed");
                    """, sandbox='system')
            return

        with self.using_context('chrome'):
            self.execute_script("""
                Components.utils.import("resource://gre/modules/Services.jsm");
                let perm = arguments[0];
                let secMan = Services.scriptSecurityManager;
                let attrs = {appId: perm.appId,
                             inIsolatedMozBrowser: perm.isInIsolatedMozBrowserElement};
                let principal = secMan.createCodebasePrincipal(Services.io.newURI(perm.url,
                                                                                  null, null),
                                                                                  attrs);
                Services.perms.addFromPrincipal(principal, perm.type, perm.action);
                return true;
                """, script_args=[perm])

        with self.using_context("content"):
            self.execute_async_script("""
                let start = new Date();
                let end = new Date(start.valueOf() + 5000);
                let wait = function() {
                  let now = new Date();
                  if (window.wrappedJSObject.permChanged || end >= now) {
                    marionetteScriptFinished();
                  }
                };
                window.setTimeout(wait, 100);
                """, sandbox="system")

    @contextmanager
    def using_permissions(self, perms):
        '''
        Sets permissions for code being executed in a `with` block,
        and restores them on exit.

        :param perms: A dict containing one or more perms and their
        values to be set.

        Usage example::

          with marionette.using_permissions({'systemXHR': True}):
              ... do stuff ...
        '''
        original_perms = {}
        for perm in perms:
            original_perms[perm] = self.get_permission(perm)
            self.push_permission(perm, perms[perm])

        try:
            yield
        finally:
            for perm in original_perms:
                self.push_permission(perm, original_perms[perm])

    def get_pref(self, pref):
        """Gets the preference value.

        :param pref: Name of the preference.

        Usage example::

            marionette.get_pref("browser.tabs.warnOnClose")
        """
        with self.using_context(self.CONTEXT_CONTENT):
            pref_value = self.execute_script("""
                Components.utils.import("resource://gre/modules/Preferences.jsm");
                return Preferences.get(arguments[0], null);
                """, script_args=[pref], sandbox="system")
            return pref_value

    def clear_pref(self, pref):
        with self.using_context(self.CONTEXT_CHROME):
            self.execute_script("""
               Components.utils.import("resource://gre/modules/Preferences.jsm");
               Preferences.reset(arguments[0]);
               """, script_args=[pref])

    def set_pref(self, pref, value):
        with self.using_context(self.CONTEXT_CHROME):
            if value is None:
                self.clear_pref(pref)
                return

            self.execute_script("""
                Components.utils.import("resource://gre/modules/Preferences.jsm");
                Preferences.set(arguments[0], arguments[1]);
                """, script_args=[pref, value])

    def set_prefs(self, prefs):
        '''Sets preferences.

        If the value of the preference to be set is None, reset the preference
        to its default value. If no default value exists, the preference will
        cease to exist.

        :param prefs: A dict containing one or more preferences and their values
        to be set.

        Usage example::

          marionette.set_prefs({'browser.tabs.warnOnClose': True})

        '''
        for pref, value in prefs.items():
            self.set_pref(pref, value)

    @contextmanager
    def using_prefs(self, prefs):
        '''Sets preferences for code being executed in a `with` block,
        and restores them on exit.

        :param prefs: A dict containing one or more preferences and their values
        to be set.

        Usage example::

          with marionette.using_prefs({'browser.tabs.warnOnClose': True}):
              # ... do stuff ...

        '''
        original_prefs = {p: self.get_pref(p) for p in prefs}
        self.set_prefs(prefs)

        try:
            yield
        finally:
            self.set_prefs(original_prefs)

    def enforce_gecko_prefs(self, prefs):
        """
        Checks if the running instance has the given prefs. If not, it will kill the
        currently running instance, and spawn a new instance with the requested preferences.

        : param prefs: A dictionary whose keys are preference names.
        """
        if not self.instance:
            raise errors.MarionetteException("enforce_gecko_prefs can only be called "
                                             "on gecko instances launched by Marionette")
        pref_exists = True
        self.set_context(self.CONTEXT_CHROME)
        for pref, value in prefs.iteritems():
            if type(value) is not str:
                value = json.dumps(value)
            pref_exists = self.execute_script("""
            let prefInterface = Components.classes["@mozilla.org/preferences-service;1"]
                                          .getService(Components.interfaces.nsIPrefBranch);
            let pref = '%s';
            let value = '%s';
            let type = prefInterface.getPrefType(pref);
            switch(type) {
                case prefInterface.PREF_STRING:
                    return value == prefInterface.getCharPref(pref).toString();
                case prefInterface.PREF_BOOL:
                    return value == prefInterface.getBoolPref(pref).toString();
                case prefInterface.PREF_INT:
                    return value == prefInterface.getIntPref(pref).toString();
                case prefInterface.PREF_INVALID:
                    return false;
            }
            """ % (pref, value))
            if not pref_exists:
                break
        self.set_context(self.CONTEXT_CONTENT)
        if not pref_exists:
            self.delete_session()
            self.instance.restart(prefs)
            self.raise_for_port(self.wait_for_port())
            self.start_session()
            self.reset_timeouts()

    def quit_in_app(self):
        """
        This will terminate the currently running instance.
        """
        if not self.instance:
            raise errors.MarionetteException("quit_in_app can only be called "
                                             "on gecko instances launched by Marionette")
        # Values here correspond to constants in nsIAppStartup.
        # See http://mzl.la/1X0JZsC
        restart_flags = [
            "eForceQuit",
            "eRestart",
        ]
        self._send_message("quitApplication", {"flags": restart_flags})
        self.client.close()
        self.raise_for_port(self.wait_for_port())

    def restart(self, clean=False, in_app=False):
        """
        This will terminate the currently running instance, and spawn a new instance
        with the same profile and then reuse the session id when creating a session again.

        : param clean: If False the same profile will be used after the restart. Note
                       that the in app initiated restart always maintains the same
                       profile.
        : param in_app: If True, marionette will cause a restart from within the
                        browser. Otherwise the browser will be restarted immediately
                        by killing the process.
        """
        if not self.instance:
            raise errors.MarionetteException("restart can only be called "
                                             "on gecko instances launched by Marionette")
        if in_app:
            if clean:
                raise ValueError
            self.quit_in_app()
        else:
            self.delete_session()
            self.instance.restart(clean=clean)
            self.raise_for_port(self.wait_for_port())

        self.start_session(session_id=self.session_id)
        self.reset_timeouts()

        if in_app:
            # In some cases Firefox restarts itself by spawning into a new process group.
            # As long as mozprocess cannot track that behavior (bug 1284864) we assist by
            # informing about the new process id.
            self.instance.runner.process_handler.check_for_detached(self.session['processId'])

    def absolute_url(self, relative_url):
        '''
        Returns an absolute url for files served from Marionette's www directory.

        :param relative_url: The url of a static file, relative to Marionette's www directory.
        '''
        return "%s%s" % (self.baseurl, relative_url)

    def start_session(self, desired_capabilities=None, session_id=None, timeout=60):
        """Create a new Marionette session.

        This method must be called before performing any other action.

        :param desired_capabilities: An optional dict of desired
            capabilities.  This is currently ignored.
        :param timeout: Timeout in seconds for the server to be ready.
        :param session_id: unique identifier for the session. If no session id is
            passed in then one will be generated by the marionette server.

        :returns: A dict of the capabilities offered."""
        if self.instance:
            returncode = self.instance.runner.process_handler.proc.returncode
            if returncode is not None:
                # We're managing a binary which has terminated, so restart it.
                self.instance.restart()

        self.client = transport.TcpTransport(
            self.host,
            self.port,
            self.socket_timeout)

        # Call wait_for_port() before attempting to connect in
        # the event gecko hasn't started yet.
        self.wait_for_port(timeout=timeout)
        self.protocol, _ = self.client.connect()

        body = {"capabilities": desired_capabilities, "sessionId": session_id}
        resp = self._send_message("newSession", body)

        self.session_id = resp["sessionId"]
        self.session = resp["value"] if self.protocol == 1 else resp["capabilities"]

        return self.session

    @property
    def test_name(self):
        return self._test_name

    @test_name.setter
    def test_name(self, test_name):
        if self._send_message("setTestName", {"value": test_name}):
            self._test_name = test_name

    def delete_session(self):
        """Close the current session and disconnect from the server."""
        self._send_message("deleteSession")
        self.session_id = None
        self.session = None
        self.window = None
        self.client.close()

    @property
    def session_capabilities(self):
        '''
        A JSON dictionary representing the capabilities of the current session.
        '''
        return self.session

    def set_script_timeout(self, timeout):
        """Sets the maximum number of ms that an asynchronous script is
        allowed to run.

        If a script does not return in the specified amount of time,
        a ScriptTimeoutException is raised.

        :param timeout: The maximum number of milliseconds an asynchronous
            script can run without causing an ScriptTimeoutException to
            be raised
        """
        self._send_message("setScriptTimeout", {"ms": timeout})

    def set_search_timeout(self, timeout):
        """Sets a timeout for the find methods.

        When searching for an element using
        either :class:`Marionette.find_element` or
        :class:`Marionette.find_elements`, the method will continue
        trying to locate the element for up to timeout ms. This can be
        useful if, for example, the element you're looking for might
        not exist immediately, because it belongs to a page which is
        currently being loaded.

        :param timeout: Timeout in milliseconds.
        """
        self._send_message("setSearchTimeout", {"ms": timeout})

    @property
    def current_window_handle(self):
        """Get the current window's handle.

        Returns an opaque server-assigned identifier to this window
        that uniquely identifies it within this Marionette instance.
        This can be used to switch to this window at a later point.

        :returns: unique window handle
        :rtype: string
        """
        self.window = self._send_message("getWindowHandle", key="value")
        return self.window

    @property
    def current_chrome_window_handle(self):
        """Get the current chrome window's handle. Corresponds to
        a chrome window that may itself contain tabs identified by
        window_handles.

        Returns an opaque server-assigned identifier to this window
        that uniquely identifies it within this Marionette instance.
        This can be used to switch to this window at a later point.

        :returns: unique window handle
        :rtype: string
        """
        self.chrome_window = self._send_message(
            "getCurrentChromeWindowHandle", key="value")
        return self.chrome_window

    def get_window_position(self):
        """Get the current window's position.

        :returns: a dictionary with x and y
        """
        return self._send_message(
            "getWindowPosition", key="value" if self.protocol == 1 else None)

    def set_window_position(self, x, y):
        """Set the position of the current window

        :param x: x coordinate for the top left of the window
        :param y: y coordinate for the top left of the window
        """
        self._send_message("setWindowPosition", {"x": x, "y": y})

    @property
    def title(self):
        """Current title of the active window."""
        return self._send_message("getTitle", key="value")

    @property
    def window_handles(self):
        """Get list of windows in the current context.

        If called in the content context it will return a list of
        references to all available browser windows.  Called in the
        chrome context, it will list all available windows, not just
        browser windows (e.g. not just navigator.browser).

        Each window handle is assigned by the server, and the list of
        strings returned does not have a guaranteed ordering.

        :returns: unordered list of unique window handles as strings
        """
        return self._send_message(
            "getWindowHandles", key="value" if self.protocol == 1 else None)

    @property
    def chrome_window_handles(self):
        """Get a list of currently open chrome windows.

        Each window handle is assigned by the server, and the list of
        strings returned does not have a guaranteed ordering.

        :returns: unordered list of unique window handles as strings
        """
        return self._send_message(
            "getChromeWindowHandles", key="value" if self.protocol == 1 else None)

    @property
    def page_source(self):
        """A string representation of the DOM."""
        return self._send_message("getPageSource", key="value")

    def close(self):
        """Close the current window, ending the session if it's the last
        window currently open.

        On B2G this method is a noop and will return immediately.
        """
        self._send_message("close")

    def close_chrome_window(self):
        """Close the currently selected chrome window, ending the session
        if it's the last window open.

        On B2G this method is a noop and will return immediately.
        """
        self._send_message("closeChromeWindow")

    def set_context(self, context):
        """Sets the context that Marionette commands are running in.

        :param context: Context, may be one of the class properties
            `CONTEXT_CHROME` or `CONTEXT_CONTENT`.

        Usage example::

            marionette.set_context(marionette.CONTEXT_CHROME)
        """
        if context not in [self.CONTEXT_CHROME, self.CONTEXT_CONTENT]:
            raise ValueError("Unknown context: %s" % context)
        self._send_message("setContext", {"value": context})

    @contextmanager
    def using_context(self, context):
        """Sets the context that Marionette commands are running in using
        a `with` statement. The state of the context on the server is
        saved before entering the block, and restored upon exiting it.

        :param context: Context, may be one of the class properties
            `CONTEXT_CHROME` or `CONTEXT_CONTENT`.

        Usage example::

            with marionette.using_context(marionette.CONTEXT_CHROME):
                # chrome scope
                ... do stuff ...
        """
        scope = self._send_message("getContext", key="value")
        self.set_context(context)
        try:
            yield
        finally:
            self.set_context(scope)

    def switch_to_alert(self):
        """Returns an Alert object for interacting with a currently
        displayed alert.

        ::

            alert = self.marionette.switch_to_alert()
            text = alert.text
            alert.accept()
        """
        return Alert(self)

    def switch_to_window(self, window_id):
        """Switch to the specified window; subsequent commands will be
        directed at the new window.

        :param window_id: The id or name of the window to switch to.
        """
        self._send_message("switchToWindow", {"name": window_id})
        self.window = window_id

    def get_active_frame(self):
        """Returns an HTMLElement representing the frame Marionette is
        currently acting on."""
        return self._send_message("getActiveFrame", key="value")

    def switch_to_default_content(self):
        """Switch the current context to page's default content."""
        return self.switch_to_frame()

    def switch_to_parent_frame(self):
        """
           Switch to the Parent Frame
        """
        self._send_message("switchToParentFrame")

    def switch_to_frame(self, frame=None, focus=True):
        """Switch the current context to the specified frame. Subsequent
        commands will operate in the context of the specified frame,
        if applicable.

        :param frame: A reference to the frame to switch to.  This can
            be an ``HTMLElement``, an integer index, string name, or an
            ID attribute.  If you call ``switch_to_frame`` without an
            argument, it will switch to the top-level frame.

        :param focus: A boolean value which determins whether to focus
            the frame that we just switched to.
        """
        body = {"focus": focus}
        if isinstance(frame, HTMLElement):
            body["element"] = frame.id
        elif frame is not None:
            body["id"] = frame
        self._send_message("switchToFrame", body)

    def switch_to_shadow_root(self, host=None):
        """Switch the current context to the specified host's Shadow DOM.
        Subsequent commands will operate in the context of the specified Shadow
        DOM, if applicable.

        :param host: A reference to the host element containing Shadow DOM.
            This can be an ``HTMLElement``. If you call
            ``switch_to_shadow_root`` without an argument, it will switch to the
            parent Shadow DOM or the top-level frame.
        """
        body = {}
        if isinstance(host, HTMLElement):
            body["id"] = host.id
        return self._send_message("switchToShadowRoot", body)

    def get_url(self):
        """Get a string representing the current URL.

        On Desktop this returns a string representation of the URL of
        the current top level browsing context.  This is equivalent to
        document.location.href.

        When in the context of the chrome, this returns the canonical
        URL of the current resource.

        :returns: string representation of URL
        """
        return self._send_message("getCurrentUrl", key="value")

    def get_window_type(self):
        """Gets the windowtype attribute of the window Marionette is
        currently acting on.

        This command only makes sense in a chrome context. You might use this
        method to distinguish a browser window from an editor window.
        """
        return self._send_message("getWindowType", key="value")

    def navigate(self, url):
        """Navigate to given `url`.

        Navigates the current top-level browsing context's content
        frame to the given URL and waits for the document to load or
        the session's page timeout duration to elapse before returning.

        The command will return with a failure if there is an error
        loading the document or the URL is blocked.  This can occur if
        it fails to reach the host, the URL is malformed, the page is
        restricted (about:* pages), or if there is a certificate issue
        to name some examples.

        The document is considered successfully loaded when the
        `DOMContentLoaded` event on the frame element associated with the
        `window` triggers and `document.readState` is "complete".

        In chrome context it will change the current `window`'s location
        to the supplied URL and wait until `document.readState` equals
        "complete" or the page timeout duration has elapsed.

        :param url: The URL to navigate to.
        """
        self._send_message("get", {"url": url})

    def timeouts(self, timeout_type, ms):
        """An interface for managing timeout behaviour of a Marionette
        instance.

        Setting timeouts specifies the type and amount of time the
        Marionette instance should wait during requests.

        There are three types of timeouts that can be set: implicit,
        script and page load.

        * An implicit  timeout specifies the amount of time a Marionette
        instance should wait when searching for elements. Here, marionette
        polls a page until an element is found or the timeout expires,
        whichever occurs first. When searching for multiple elements,
        the driver should poll the page until at least one element is
        found or the timeout expires, at which point it should return
        an empty list.

        * A script timeout specifies the amount of time the Marionette
        instance should wait after calling executeAsyncScript for the
        callback to have executed before returning a timeout response.

        * A page load timeout specifies the amount of time the Marionette
        instance should wait for a page load operation to complete. If
        this limit is exceeded, the Marionette instance will return a
        "timeout" response status.

        :param timeout_type: A string value specifying the timeout
            type. This must be one of three types: 'implicit', 'script'
            or 'page load'
        :param ms: A number value specifying the timeout length in
            milliseconds (ms)
        """
        if timeout_type not in [self.TIMEOUT_SEARCH, self.TIMEOUT_SCRIPT, self.TIMEOUT_PAGE]:
            raise ValueError("Unknown timeout type: %s" % timeout_type)
        body = {"type": timeout_type, "ms": ms}
        self._send_message("timeouts", body)

    def go_back(self):
        """Causes the browser to perform a back navigation."""
        self._send_message("goBack")

    def go_forward(self):
        """Causes the browser to perform a forward navigation."""
        self._send_message("goForward")

    def refresh(self):
        """Causes the browser to perform to refresh the current page."""
        self._send_message("refresh")

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
            wrapped = {W3C_WEBELEMENT_KEY: args.id,
                       WEBELEMENT_KEY: args.id}
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
                if key == W3C_WEBELEMENT_KEY:
                    unwrapped = HTMLElement(self, value[key])
                    break
                elif key == WEBELEMENT_KEY:
                    unwrapped = HTMLElement(self, value[key])
                    break
                else:
                    unwrapped[key] = self.unwrapValue(value[key])
        else:
            unwrapped = value
        return unwrapped

    def execute_js_script(self, script, script_args=None, async=True,
                          new_sandbox=True, script_timeout=None,
                          inactivity_timeout=None, filename=None,
                          sandbox='default'):
        if script_args is None:
            script_args = []
        args = self.wrapArguments(script_args)
        body = {"script": script,
                "args": args,
                "async": async,
                "newSandbox": new_sandbox,
                "scriptTimeout": script_timeout,
                "inactivityTimeout": inactivity_timeout,
                "filename": filename,
                "line": None}
        rv = self._send_message("executeJSScript", body, key="value")
        return self.unwrapValue(rv)

    def execute_script(self, script, script_args=None, new_sandbox=True,
                       sandbox="default", script_timeout=None):
        """Executes a synchronous JavaScript script, and returns the
        result (or None if the script does return a value).

        The script is executed in the context set by the most recent
        set_context() call, or to the CONTEXT_CONTENT context if set_context()
        has not been called.

        :param script: A string containing the JavaScript to execute.
        :param script_args: A list of arguments to pass to the script.
        :param sandbox: A tag referring to the sandbox you wish to use;
            if you specify a new tag, a new sandbox will be created.
            If you use the special tag `system`, the sandbox will
            be created using the system principal which has elevated
            privileges.
        :param new_sandbox: If False, preserve global variables from
            the last execute_*script call. This is True by default, in which
            case no globals are preserved.

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
            some_element = marionette.find_element(By.ID, "someElement")
            sid = marionette.execute_script("return arguments[0].id;", script_args=[some_element])
            assert some_element.get_attribute("id") == sid

        Scripts wishing to access non-standard properties of the window
        object must use window.wrappedJSObject:

        ::

            result = marionette.execute_script('''
              window.wrappedJSObject.test1 = "foo";
              window.wrappedJSObject.test2 = "bar";
              return window.wrappedJSObject.test1 + window.wrappedJSObject.test2;
              ''')
            assert result == "foobar"

        Global variables set by individual scripts do not persist between
        script calls by default.  If you wish to persist data between
        script calls, you can set new_sandbox to False on your next call,
        and add any new variables to a new 'global' object like this:

        ::

            marionette.execute_script("global.test1 = 'foo';")
            result = self.marionette.execute_script("return global.test1;", new_sandbox=False)
            assert result == "foo"

        """
        if script_args is None:
            script_args = []
        args = self.wrapArguments(script_args)
        stack = traceback.extract_stack()
        frame = stack[-2:-1][0]  # grab the second-to-last frame
        body = {"script": script,
                "args": args,
                "newSandbox": new_sandbox,
                "sandbox": sandbox,
                "scriptTimeout": script_timeout,
                "line": int(frame[1]),
                "filename": os.path.basename(frame[0])}
        rv = self._send_message("executeScript", body, key="value")
        return self.unwrapValue(rv)

    def execute_async_script(self, script, script_args=None, new_sandbox=True,
                             sandbox="default", script_timeout=None,
                             debug_script=False):
        """Executes an asynchronous JavaScript script, and returns the
        result (or None if the script does return a value).

        The script is executed in the context set by the most recent
        set_context() call, or to the CONTEXT_CONTENT context if
        set_context() has not been called.

        :param script: A string containing the JavaScript to execute.
        :param script_args: A list of arguments to pass to the script.
        :param sandbox: A tag referring to the sandbox you wish to use; if
            you specify a new tag, a new sandbox will be created.  If you
            use the special tag `system`, the sandbox will be created
            using the system principal which has elevated privileges.
        :param new_sandbox: If False, preserve global variables from
            the last execute_*script call. This is True by default,
            in which case no globals are preserved.
        :param debug_script: Capture javascript exceptions when in
            `CONTEXT_CHROME` context.

        Usage example:

        ::

            marionette.set_script_timeout(10000) # set timeout period of 10 seconds
            result = self.marionette.execute_async_script('''
              // this script waits 5 seconds, and then returns the number 1
              setTimeout(function() {
                marionetteScriptFinished(1);
              }, 5000);
            ''')
            assert result == 1
        """
        if script_args is None:
            script_args = []
        args = self.wrapArguments(script_args)
        stack = traceback.extract_stack()
        frame = stack[-2:-1][0]  # grab the second-to-last frame
        body = {"script": script,
                "args": args,
                "newSandbox": new_sandbox,
                "sandbox": sandbox,
                "scriptTimeout": script_timeout,
                "line": int(frame[1]),
                "filename": os.path.basename(frame[0]),
                "debug_script": debug_script}
        rv = self._send_message("executeAsyncScript", body, key="value")
        return self.unwrapValue(rv)

    def find_element(self, method, target, id=None):
        """Returns an HTMLElement instances that matches the specified
        method and target in the current context.

        An HTMLElement instance may be used to call other methods on the
        element, such as click().  If no element is immediately found, the
        attempt to locate an element will be repeated for up to the amount of
        time set by set_search_timeout(). If multiple elements match the given
        criteria, only the first is returned. If no element matches, a
        NoSuchElementException will be raised.

        :param method: The method to use to locate the element; one of:
            "id", "name", "class name", "tag name", "css selector",
            "link text", "partial link text", "xpath", "anon" and "anon
            attribute". Note that the "name", "link text" and "partial
            link test" methods are not supported in the chrome DOM.
        :param target: The target of the search.  For example, if method =
            "tag", target might equal "div".  If method = "id", target would
            be an element id.
        :param id: If specified, search for elements only inside the element
            with the specified id.
        """
        body = {"value": target, "using": method}
        if id:
            body["element"] = id
        return self._send_message("findElement", body, key="value")

    def find_elements(self, method, target, id=None):
        """Returns a list of all HTMLElement instances that match the
        specified method and target in the current context.

        An HTMLElement instance may be used to call other methods on the
        element, such as click().  If no element is immediately found,
        the attempt to locate an element will be repeated for up to the
        amount of time set by set_search_timeout().

        :param method: The method to use to locate the elements; one
            of: "id", "name", "class name", "tag name", "css selector",
            "link text", "partial link text", "xpath", "anon" and "anon
            attribute". Note that the "name", "link text" and "partial link
            test" methods are not supported in the chrome DOM.
        :param target: The target of the search.  For example, if method =
            "tag", target might equal "div".  If method = "id", target would be
            an element id.
        :param id: If specified, search for elements only inside the element
            with the specified id.
        """
        body = {"value": target, "using": method}
        if id:
            body["element"] = id
        return self._send_message(
            "findElements", body, key="value" if self.protocol == 1 else None)

    def get_active_element(self):
        el_or_ref = self._send_message("getActiveElement", key="value")
        if self.protocol < 3:
            return HTMLElement(self, el_or_ref)
        return el_or_ref

    def log(self, msg, level="INFO"):
        """Stores a timestamped log message in the Marionette server
        for later retrieval.

        :param msg: String with message to log.
        :param level: String with log level (e.g. "INFO" or "DEBUG").
            Defaults to "INFO".
        """
        body = {"value": msg, "level": level}
        self._send_message("log", body)

    def get_logs(self):
        """Returns the list of logged messages.

        Each log message is an array with three string elements: the level,
        the message, and a date.

        Usage example::

            marionette.log("I AM INFO")
            marionette.log("I AM ERROR", "ERROR")
            logs = marionette.get_logs()
            assert logs[0][1] == "I AM INFO"
            assert logs[1][1] == "I AM ERROR"
        """
        return self._send_message("getLogs",
                                  key="value" if self.protocol == 1 else None)

    def import_script(self, js_file):
        """Imports a script into the scope of the execute_script and
        execute_async_script calls.

        This is particularly useful if you wish to import your own
        libraries.

        :param js_file: Filename of JavaScript file to import.

        For example, Say you have a script, importfunc.js, that contains:

        ::

            let testFunc = function() { return "i'm a test function!";};

        Assuming this file is in the same directory as the test, you
        could do something like:

        ::

            js = os.path.abspath(os.path.join(__file__, os.path.pardir, "importfunc.js"))
            marionette.import_script(js)
            assert "i'm a test function!" == self.marionette.execute_script("return testFunc();")
        """
        js = ""
        with open(js_file, "r") as f:
            js = f.read()
        body = {"script": js}
        self._send_message("importScript", body)

    def clear_imported_scripts(self):
        """Clears all imported scripts in this context, ie: calling
        clear_imported_scripts in chrome context will clear only scripts
        you imported in chrome, and will leave the scripts you imported
        in content context.
        """
        self._send_message("clearImportedScripts")

    def add_cookie(self, cookie):
        """Adds a cookie to your current session.

        :param cookie: A dictionary object, with required keys - "name"
            and "value"; optional keys - "path", "domain", "secure",
            "expiry".

        Usage example:

        ::

            driver.add_cookie({"name": "foo", "value": "bar"})
            driver.add_cookie({"name": "foo", "value": "bar", "path": "/"})
            driver.add_cookie({"name": "foo", "value": "bar", "path": "/",
                               "secure": True})
        """
        body = {"cookie": cookie}
        self._send_message("addCookie", body)

    def delete_all_cookies(self):
        """Delete all cookies in the scope of the current session.

        Usage example:

        ::

            driver.delete_all_cookies()
        """
        self._send_message("deleteAllCookies")

    def delete_cookie(self, name):
        """Delete a cookie by its name.

        :param name: Name of cookie to delete.

        Usage example:

        ::

            driver.delete_cookie("foo")
        """
        self._send_message("deleteCookie", {"name": name})

    def get_cookie(self, name):
        """Get a single cookie by name. Returns the cookie if found,
        None if not.

        :param name: Name of cookie to get.
        """
        cookies = self.get_cookies()
        for cookie in cookies:
            if cookie["name"] == name:
                return cookie
        return None

    def get_cookies(self):
        """Get all the cookies for the current domain.

        This is the equivalent of calling `document.cookie` and
        parsing the result.

        :returns: A list of cookies for the current domain.
        """
        return self._send_message("getCookies", key="value" if self.protocol == 1 else None)

    def screenshot(self, element=None, highlights=None, format="base64",
                   full=True):
        """Takes a screenshot of a web element or the current frame.

        The screen capture is returned as a lossless PNG image encoded
        as a base 64 string by default. If the `element` argument is defined the
        capture area will be limited to the bounding box of that
        element.  Otherwise, the capture area will be the bounding box
        of the current frame.

        :param element: The element to take a screenshot of.  If None, will
            take a screenshot of the current frame.

        :param highlights: A list of HTMLElement objects to draw a red
            box around in the returned screenshot.

        :param format: if "base64" (the default), returns the screenshot
            as a base64-string. If "binary", the data is decoded and
            returned as raw binary. If "hash", the data is hashed using
            the SHA-256 algorithm and the result is returned as a hex digest.

        :param full: If True (the default), the capture area will be the
            complete frame. Else only the viewport is captured. Only applies
            when `element` is None.
        """

        if element:
            element = element.id
        lights = None
        if highlights:
            lights = [highlight.id for highlight in highlights]

        body = {"id": element,
                "highlights": lights,
                "full": full,
                "hash": False}
        if format == "hash":
            body["hash"] = True
        data = self._send_message("takeScreenshot", body, key="value")

        if format == "base64" or format == "hash":
            return data
        elif format == "binary":
            return base64.b64decode(data.encode("ascii"))
        else:
            raise ValueError("format parameter must be either 'base64'"
                             " or 'binary', not {0}".format(repr(format)))

    @property
    def orientation(self):
        """Get the current browser orientation.

        Will return one of the valid primary orientation values
        portrait-primary, landscape-primary, portrait-secondary, or
        landscape-secondary.
        """
        return self._send_message("getScreenOrientation", key="value")

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
        body = {"orientation": orientation}
        self._send_message("setScreenOrientation", body)

    @property
    def window_size(self):
        """Get the current browser window size.

        Will return the current browser window size in pixels. Refers to
        window outerWidth and outerHeight values, which include scroll bars,
        title bars, etc.

        :returns: dictionary representation of current window width and height
        """
        return self._send_message("getWindowSize",
                                  key="value" if self.protocol == 1 else None)

    def set_window_size(self, width, height):
        """Resize the browser window currently in focus.

        The supplied width and height values refer to the window outerWidth
        and outerHeight values, which include scroll bars, title bars, etc.

        An error will be returned if the requested window size would result
        in the window being in the maximised state.

        :param width: The width to resize the window to.
        :param height: The height to resize the window to.

        """
        body = {"width": width, "height": height}
        return self._send_message("setWindowSize", body)

    def maximize_window(self):
        """ Resize the browser window currently receiving commands. The action
        should be equivalent to the user pressing the the maximize button
        """
        return self._send_message("maximizeWindow")
