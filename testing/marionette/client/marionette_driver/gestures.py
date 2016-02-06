# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from marionette import MultiActions, Actions

#axis is y or x
#direction is 0 for positive, and -1 for negative
#length is the total length we want to scroll
#increments is how much we want to move per scrolling
#wait_period is the seconds we wait between scrolling
#scroll_back is whether we want to scroll back to original view
def smooth_scroll(marionette_session, start_element, axis, direction, length, increments=None, wait_period=None, scroll_back=None):
    if axis not in ["x", "y"]:
        raise Exception("Axis must be either 'x' or 'y'")
    if direction not in [-1, 0]:
        raise Exception("Direction must either be -1 negative or 0 positive")
    increments = increments or 100
    wait_period = wait_period or 0.05
    scroll_back = scroll_back or False
    current = 0
    if axis is "x":
        if direction is -1:
            offset = [-increments, 0]
        else:
            offset = [increments, 0]
    else:
        if direction is -1:
            offset = [0, -increments]
        else:
            offset = [0, increments]
    action = Actions(marionette_session)
    action.press(start_element)
    while (current < length):
        current += increments
        action.move_by_offset(*offset).wait(wait_period)
    if scroll_back:
        offset = [-value for value in offset]
        while (current > 0):
            current -= increments
            action.move_by_offset(*offset).wait(wait_period)
    action.release()
    action.perform()

#element is the target
#x1,x2 are 1st finger starting position relative to the target
#x3,y3 are 1st finger ending position relative to the target
#x2,y2 are 2nd finger starting position relative to the target
#x4,y4 are 2nd finger ending position relative to the target
#duration is the amount of time in milliseconds we want to complete the pinch.
def pinch(marionette_session, element, x1, y1, x2, y2, x3, y3, x4, y4, duration=200):
    time = 0
    time_increment = 10
    if time_increment >= duration:
        time_increment = duration
    move_x1 = time_increment*1.0/duration * (x3 - x1)
    move_y1 = time_increment*1.0/duration * (y3 - y1)
    move_x2 = time_increment*1.0/duration * (x4 - x2)
    move_y2 = time_increment*1.0/duration * (y4 - y2)
    multiAction = MultiActions(marionette_session)
    action1 = Actions(marionette_session)
    action2 = Actions(marionette_session)
    action1.press(element, x1, y1)
    action2.press(element, x2, y2)
    while (time < duration):
        time += time_increment
        action1.move_by_offset(move_x1, move_y1).wait(time_increment/1000)
        action2.move_by_offset(move_x2, move_y2).wait(time_increment/1000)
    action1.release()
    action2.release()
    multiAction.add(action1).add(action2).perform()

#element: The element to press.
#time_in_seconds: Time in seconds to wait before releasing the press.
#x: Optional, x-coordinate to tap, relative to the top-left corner of the element.
#y: Optional, y-coordinate to tap, relative to the top-leftcorner of the element.
def long_press_without_contextmenu(marionette_session, element, time_in_seconds, x=None, y=None):
    action = Actions(marionette_session)
    action.press(element, x, y)
    action.move_by_offset(0, 0)
    action.wait(time_in_seconds)
    action.release()
    action.perform()
