# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from . import errors
from .marionette import MouseButton


class Actions(object):
    """Represent a set of actions that are executed in a particular order.

    All action methods (press, etc.) return the Actions object itself, to make
    it easy to create a chain of events.

    Example usage::

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
    """

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

        May only be called if :func:`press` has already be called on this element.

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

        May only be called if :func:`press` has already be called.
        '''
        element = element.id
        self.action_chain.append(['move', element])
        return self

    def move_by_offset(self, x, y):
        '''
        Sends 'touchmove' event to the given x, y coordinates relative to the
        top-left of the currently touched element.

        May only be called if :func:`press` has already be called.

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

        May only be called if :func:`press` has already be called.
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
        try:
            self.current_id = self.marionette._send_message("Marionette:ActionChain",
                                                            body, key="value")
        except errors.UnknownCommandException:
            self.current_id = self.marionette._send_message("actionChain",
                                                            body, key="value")
        self.action_chain = []
        return self


class MultiActions(object):
    """Represent a sequence of actions that may be performed at the same time.

    Its intent is to allow the simulation of multi-touch gestures.

    Usage example::

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
    """

    def __init__(self, marionette):
        self.multi_actions = []
        self.max_length = 0
        self.marionette = marionette

    def add(self, action):
        """Add a set of actions to perform.

        :param action: An Actions object.
        """
        self.multi_actions.append(action.action_chain)
        if len(action.action_chain) > self.max_length:
            self.max_length = len(action.action_chain)
        return self

    def perform(self):
        """Perform all the actions added to this object."""
        body = {"value": self.multi_actions, "max_length": self.max_length}
        try:
            self.marionette._send_message("Marionette:MultiAction", body)
        except errors.UnknownCommandException:
            self.marionette._send_message("multiAction", body)
