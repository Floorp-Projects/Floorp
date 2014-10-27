# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


class SelectionManager(object):
    '''Interface for manipulating the selection and carets of the element.

    Simple usage example:

    ::

        element = marionette.find_element('id', 'input')
        sel = SelectionManager(element)
        sel.move_caret_to_front()

    '''

    def __init__(self, element):
        self.element = element

    def _input_or_textarea(self):
        '''Return True if element is either <input> or <textarea>.'''
        return self.element.tag_name in ('input', 'textarea')

    def js_selection_cmd(self):
        '''Return a command snippet to get selection object.

        If the element is <input> or <textarea>, return the selection object
        associated with it. Otherwise, return the current selection object.

        Note: "element" must be provided as the first argument to
        execute_script().

        '''
        if self._input_or_textarea():
            # We must unwrap sel so that DOMRect could be returned to Python
            # side.
            return '''var sel = SpecialPowers.wrap(arguments[0]).editor.selection;
                   sel = SpecialPowers.unwrap(sel);'''
        else:
            return '''var sel = window.getSelection();'''

    def move_caret_by_offset(self, offset, backward=False):
        '''Move caret in the element by character offset.'''
        cmd = self.js_selection_cmd() +\
            '''sel.modify("move", arguments[1], "character");'''
        direction = 'backward' if backward else 'forward'

        for i in range(offset):
            self.element.marionette.execute_script(
                cmd, script_args=[self.element, direction])

    def move_caret_to_front(self):
        '''Move caret in the element to the front of the content.'''
        if self._input_or_textarea():
            cmd = '''arguments[0].setSelectionRange(0, 0);'''
        else:
            cmd = '''var sel = window.getSelection();
                  sel.collapse(arguments[0].firstChild, 0);'''

        self.element.marionette.execute_script(cmd, script_args=[self.element])

    def move_caret_to_end(self):
        '''Move caret in the element to the end of the content.'''
        if self._input_or_textarea():
            cmd = '''var len = arguments[0].value.length;
                  arguments[0].setSelectionRange(len, len);'''
        else:
            cmd = '''var sel = window.getSelection();
                  sel.collapse(arguments[0].lastChild, arguments[0].lastChild.length);'''

        self.element.marionette.execute_script(cmd, script_args=[self.element])

    def selection_rect_list(self):
        '''Return the selection's DOMRectList object.

        If the element is either <input> or <textarea>, return the selection's
        DOMRectList within the element. Otherwise, return the DOMRectList of the
        current selection.

        '''
        cmd = self.js_selection_cmd() +\
            '''return sel.getRangeAt(0).getClientRects();'''
        return self.element.marionette.execute_script(cmd, script_args=[self.element])

    def _selection_location_helper(self, location_type):
        '''Return the start and end location of the selection in the element.

        Return a tuple containing two pairs of (x, y) coordinates of the start
        and end locations in the element. The coordinates are relative to the
        top left-hand corner of the element. Both ltr and rtl directions are
        considered.

        '''
        rect_list = self.selection_rect_list()
        list_length = rect_list['length']
        first_rect, last_rect = rect_list['0'], rect_list[str(list_length - 1)]
        origin_x, origin_y = self.element.location['x'], self.element.location['y']

        if self.element.get_attribute('dir') == 'rtl':  # such as Arabic
            start_pos, end_pos = 'right', 'left'
        else:
            start_pos, end_pos = 'left', 'right'

        # Calculate y offset according to different needs.
        if location_type == 'center':
            start_y_offset = first_rect['height'] / 2.0
            end_y_offset = last_rect['height'] / 2.0
        elif location_type == 'caret':
            # Selection carets' tip are below the bottom of the two ends of the
            # selection. Add 5px to y should be sufficient to locate them.
            caret_tip_y_offset = 5
            start_y_offset = first_rect['height'] + caret_tip_y_offset
            end_y_offset = last_rect['height'] + caret_tip_y_offset
        else:
            start_y_offset = end_y_offset = 0

        caret1_x = first_rect[start_pos] - origin_x
        caret1_y = first_rect['top'] + start_y_offset - origin_y
        caret2_x = last_rect[end_pos] - origin_x
        caret2_y = last_rect['top'] + end_y_offset - origin_y

        return ((caret1_x, caret1_y), (caret2_x, caret2_y))

    def selection_location(self):
        '''Return the start and end location of the selection in the element.

        Return a tuple containing two pairs of (x, y) coordinates of the start
        and end of the selection. The coordinates are relative to the top
        left-hand corner of the element. Both ltr and rtl direction are
        considered.

        '''
        return self._selection_location_helper('center')

    def selection_carets_location(self):
        '''Return a pair of the two selection carets' location.

        Return a tuple containing two pairs of (x, y) coordinates of the two
        selection carets' tip. The coordinates are relative to the top left-hand
        corner of the element. Both ltr and rtl direction are considered.

        '''
        return self._selection_location_helper('caret')

    def caret_location(self):
        '''Return caret's center location within the element.

        Return (x, y) coordinates of the caret's center relative to the top
        left-hand corner of the element.

        '''
        return self._selection_location_helper('center')[0]

    def touch_caret_location(self):
        '''Return touch caret's location (based on current caret location).

        Return (x, y) coordinates of the touch caret's tip relative to the top
        left-hand corner of the element.

        '''
        return self._selection_location_helper('caret')[0]

    def select_all(self):
        '''Select all the content in the element.'''
        if self._input_or_textarea():
            cmd = '''var len = arguments[0].value.length;
                  arguments[0].focus();
                  arguments[0].setSelectionRange(0, len);'''
        else:
            cmd = '''var range = document.createRange();
                  range.setStart(arguments[0].firstChild, 0);
                  range.setEnd(arguments[0].lastChild, arguments[0].lastChild.length);
                  var sel = window.getSelection();
                  sel.removeAllRanges();
                  sel.addRange(range);'''

        self.element.marionette.execute_script(cmd, script_args=[self.element])

    @property
    def content(self):
        '''Return all the content of the element.'''
        if self._input_or_textarea():
            return self.element.get_attribute('value')
        else:
            return self.element.text

    @property
    def selected_content(self):
        '''Return the selected portion of the content in the element.'''
        cmd = self.js_selection_cmd() +\
            '''return sel.toString();'''
        return self.element.marionette.execute_script(cmd, script_args=[self.element])
