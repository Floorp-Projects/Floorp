# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


class SelectionManager(object):
    '''Interface for manipulating the selection and carets of the element.

    We call the blinking cursor (nsCaret) as cursor, and call AccessibleCaret as
    caret for short.

    Simple usage example:

    ::

        element = marionette.find_element(By.ID, 'input')
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
            return '''var sel = arguments[0].editor.selection;'''
        else:
            return '''var sel = window.getSelection();'''

    def move_cursor_by_offset(self, offset, backward=False):
        '''Move cursor in the element by character offset.

        :param offset: Move the cursor to the direction by offset characters.
        :param backward: Optional, True to move backward; Default to False to
         move forward.

        '''
        cmd = self.js_selection_cmd() + '''
              for (let i = 0; i < {0}; ++i) {{
                  sel.modify("move", "{1}", "character");
              }}
              '''.format(offset, 'backward' if backward else 'forward')

        self.element.marionette.execute_script(
            cmd, script_args=[self.element], sandbox='system')

    def move_cursor_to_front(self):
        '''Move cursor in the element to the front of the content.'''
        if self._input_or_textarea():
            cmd = '''arguments[0].setSelectionRange(0, 0);'''
        else:
            cmd = '''var sel = window.getSelection();
                  sel.collapse(arguments[0].firstChild, 0);'''

        self.element.marionette.execute_script(cmd, script_args=[self.element])

    def move_cursor_to_end(self):
        '''Move cursor in the element to the end of the content.'''
        if self._input_or_textarea():
            cmd = '''var len = arguments[0].value.length;
                  arguments[0].setSelectionRange(len, len);'''
        else:
            cmd = '''var sel = window.getSelection();
                  sel.collapse(arguments[0].lastChild, arguments[0].lastChild.length);'''

        self.element.marionette.execute_script(cmd, script_args=[self.element])

    def selection_rect_list(self, idx):
        '''Return the selection's DOMRectList object for the range at given idx.

        If the element is either <input> or <textarea>, return the DOMRectList of
        the range at given idx of the selection within the element. Otherwise,
        return the DOMRectList of the of the range at given idx of current selection.

        '''
        cmd = self.js_selection_cmd() +\
            '''return sel.getRangeAt({}).getClientRects();'''.format(idx)
        return self.element.marionette.execute_script(cmd,
                                                      script_args=[self.element],
                                                      sandbox='system')

    def range_count(self):
        '''Get selection's range count'''
        cmd = self.js_selection_cmd() +\
            '''return sel.rangeCount;'''
        return self.element.marionette.execute_script(cmd,
                                                      script_args=[self.element],
                                                      sandbox='system')

    def _selection_location_helper(self, location_type):
        '''Return the start and end location of the selection in the element.

        Return a tuple containing two pairs of (x, y) coordinates of the start
        and end locations in the element. The coordinates are relative to the
        top left-hand corner of the element. Both ltr and rtl directions are
        considered.

        '''
        range_count = self.range_count()
        first_rect_list = self.selection_rect_list(0)
        last_rect_list = self.selection_rect_list(range_count - 1)
        last_list_length = last_rect_list['length']
        first_rect, last_rect = first_rect_list['0'], last_rect_list[str(last_list_length - 1)]
        origin_x, origin_y = self.element.rect['x'], self.element.rect['y']

        if self.element.get_property('dir') == 'rtl':  # such as Arabic
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

    def carets_location(self):
        '''Return a pair of the two carets' location.

        Return a tuple containing two pairs of (x, y) coordinates of the two
        carets' tip. The coordinates are relative to the top left-hand corner of
        the element. Both ltr and rtl direction are considered.

        '''
        return self._selection_location_helper('caret')

    def cursor_location(self):
        '''Return the blanking cursor's center location within the element.

        Return (x, y) coordinates of the cursor's center relative to the top
        left-hand corner of the element.

        '''
        return self._selection_location_helper('center')[0]

    def first_caret_location(self):
        '''Return the first caret's location.

        Return (x, y) coordinates of the first caret's tip relative to the top
        left-hand corner of the element.

        '''
        return self.carets_location()[0]

    def second_caret_location(self):
        '''Return the second caret's location.

        Return (x, y) coordinates of the second caret's tip relative to the top
        left-hand corner of the element.

        '''
        return self.carets_location()[1]

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
            return self.element.get_property('value')
        else:
            return self.element.text

    @property
    def selected_content(self):
        '''Return the selected portion of the content in the element.'''
        cmd = self.js_selection_cmd() +\
            '''return sel.toString();'''
        return self.element.marionette.execute_script(cmd,
                                                      script_args=[self.element],
                                                      sandbox='system')
