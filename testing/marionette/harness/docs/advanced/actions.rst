Actions
=======

.. py:currentmodule:: marionette

Action Sequences
----------------

:class:`Actions` are designed as a way to simulate user input as closely as possible
on a touch device like a smart phone. A common operation is to tap the screen
and drag your finger to another part of the screen and lift it off.

This can be simulated using an Action::

    from marionette import Actions

    start_element = marionette.find_element('id', 'start')
    end_element = marionette.find_element('id', 'end')

    action = Actions(marionette)
    action.press(start_element).wait(1).move(end_element).release()
    action.perform()

This will simulate pressing an element, waiting for one second, moving the
finger over to another element and then lifting the finger off the screen. The
wait is optional in this case, but can be useful for simulating delays typical
to a users behaviour.

Multi-Action Sequences
----------------------

Sometimes it may be necessary to simulate multiple actions at the same time.
For example a user may be dragging one finger while tapping another. This is
where :class:`MultiActions` come in. MultiActions are simply a way of combining
two or more actions together and performing them all at the same time::

    action1 = Actions(marionette)
    action1.press(start_element).move(end_element).release()

    action2 = Actions(marionette)
    action2.press(another_element).wait(1).release()

    multi = MultiActions(marionette)
    multi.add(action1)
    multi.add(action2)
    multi.perform()
