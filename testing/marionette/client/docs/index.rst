.. Marionette Python Client documentation master file, created by
   sphinx-quickstart on Tue Aug  6 13:54:46 2013.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

Marionette Python Client
========================

The Marionette python client library allows you to remotely control a
Gecko-based browser or device which is running a Marionette_
server.

.. _Marionette: https://developer.mozilla.org/en-US/docs/Marionette

You can install this library from pypi. The package name is
marionette_client_.

.. _marionette_client: https://pypi.python.org/pypi/marionette_client

.. automodule:: marionette

Marionette Objects
------------------
.. autoclass:: Marionette

Session Management
``````````````````
.. automethod:: Marionette.start_session
.. autoattribute:: Marionette.session_capabilities
.. automethod:: Marionette.get_cookie
.. automethod:: Marionette.get_cookies
.. automethod:: Marionette.add_cookie
.. automethod:: Marionette.delete_all_cookies

Context Management
``````````````````
.. autoattribute:: Marionette.current_window_handle
.. autoattribute:: Marionette.window_handles
.. automethod:: Marionette.set_context
.. automethod:: Marionette.switch_to_frame
.. automethod:: Marionette.switch_to_window
.. automethod:: Marionette.get_active_frame
.. automethod:: Marionette.close

Navigation Methods
``````````````````
.. autoattribute:: Marionette.title
.. automethod:: Marionette.navigate
.. automethod:: Marionette.get_url
.. automethod:: Marionette.go_back
.. automethod:: Marionette.go_forward
.. automethod:: Marionette.refresh
.. automethod:: Marionette.absolute_url
.. automethod:: Marionette.get_window_type

DOM Element Methods
```````````````````
.. automethod:: Marionette.set_search_timeout
.. automethod:: Marionette.find_element
.. automethod:: Marionette.find_elements

Script Execution
````````````````
.. automethod:: Marionette.execute_script
.. automethod:: Marionette.execute_async_script
.. automethod:: Marionette.set_script_timeout

Debugging
`````````
.. autoattribute:: Marionette.page_source
.. automethod:: Marionette.log
.. automethod:: Marionette.get_logs
.. automethod:: Marionette.screenshot

HTMLElement Objects
-------------------
.. autoclass:: HTMLElement

.. autoattribute:: HTMLElement.text
.. autoattribute:: HTMLElement.location
.. autoattribute:: HTMLElement.size
.. autoattribute:: HTMLElement.tag_name
.. automethod:: HTMLElement.send_keys
.. automethod:: HTMLElement.clear
.. automethod:: HTMLElement.click
.. automethod:: HTMLElement.is_selected
.. automethod:: HTMLElement.is_enabled
.. automethod:: HTMLElement.is_displayed
.. automethod:: HTMLElement.value_of_css_property

Action Objects
--------------

Action Sequences
````````````````
.. autoclass:: Actions

.. automethod:: Actions.press
.. automethod:: Actions.release
.. automethod:: Actions.move
.. automethod:: Actions.move_by_offset
.. automethod:: Actions.wait
.. automethod:: Actions.cancel
.. automethod:: Actions.long_press
.. automethod:: Actions.flick
.. automethod:: Actions.tap
.. automethod:: Actions.double_tap
.. automethod:: Actions.perform

Multi-action Sequences
``````````````````````
.. autoclass:: MultiActions

.. automethod:: MultiActions.add
.. automethod:: MultiActions.perform


Indices and tables
==================

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`

