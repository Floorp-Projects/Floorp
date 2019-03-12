Actions
=======

.. py:currentmodule:: marionette_driver.marionette

Action Sequences
----------------

:class:`Actions` are designed as a way to simulate user input like a keyboard
or a pointer device as closely as possible. For multiple interactions an
action sequence can be used::

    element = marionette.find_element("id", "input")
    element.click()

    key_chain = self.marionette.actions.sequence("key", "keyboard1")
    key_chain.send_keys("fooba").pause(100).key_down("r").perform()

This will simulate entering "fooba" into the input field, waiting for 100ms,
and pressing the key "r". The pause is optional in this case, but can be useful
for simulating delays typical to a users behaviour.
