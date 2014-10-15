.. Marionette Python Client documentation master file, created by
   sphinx-quickstart on Tue Aug  6 13:54:46 2013.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

Marionette Python Client
========================

The Marionette python client library allows you to remotely control a
Gecko-based browser or device which is running a Marionette_
server. This includes desktop Firefox and FirefoxOS (support for
Firefox for Android is planned, but not yet fully implemented).

.. _Marionette: https://developer.mozilla.org/en-US/docs/Marionette

Getting Started
---------------

Getting the Client
^^^^^^^^^^^^^^^^^^

We officially support a python client. The latest supported version of
the client is available on pypi_, so you can download it via pip.
This client should be used within a virtual environment to ensure that
your environment is pristine:

.. parsed-literal::

   virtualenv venv
   source venv/bin/activate
   pip install marionette_client

.. _pypi: https://pypi.python.org/pypi/marionette_client/

Using the Client Interactively
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Once you installed the client and have Marionette running, you can fire
up your favourite interactive python environment and start playing with
Marionette. Let's use a typical python shell:

.. parsed-literal::

   python

First, import Marionette:

.. parsed-literal::
   from marionette import Marionette

Now create the client for this session. Assuming you're using the default
port on a Marionette instance running locally:

.. parsed-literal::

   client = Marionette(host='localhost', port=2828)
   client.start_session()

This will return some id representing your session id. Now that you've
established a connection, let's start doing interesting things:

.. parsed-literal::

   client.execute_script("alert('o hai there!');")

You should now see this alert pop up! How exciting! Okay, let's do
something practical. Close the dialog and try this:

.. parsed-literal::

   client.navigate("http://www.mozilla.org")

Now you're at mozilla.org! You can even verify it using the following:

.. parsed-literal::
   client.get_url()

You can even find an element and click on it. Let's say you want to get
the first link:

.. parsed-literal::
   first_link = client.find_element("tag name", "a")

first_link now holds a reference to the first link on the page. You can click it:

.. parsed-literal::
   first_link.click()

Using the Client for Testing
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Please visit, our `Marionette Tests`_ section for information regarding testing.

.. _Marionette Tests: https://developer.mozilla.org/en/Marionette/Tests

.. automodule:: marionette

Marionette Objects
------------------
.. autoclass:: Marionette

Session Management
^^^^^^^^^^^^^^^^^^
.. automethod:: Marionette.start_session
.. automethod:: Marionette.delete_session
.. autoattribute:: Marionette.session_capabilities
.. automethod:: Marionette.get_cookie
.. automethod:: Marionette.get_cookies
.. automethod:: Marionette.add_cookie
.. automethod:: Marionette.delete_all_cookies

Context Management
^^^^^^^^^^^^^^^^^^
.. autoattribute:: Marionette.current_window_handle
.. autoattribute:: Marionette.window_handles
.. automethod:: Marionette.set_context
.. automethod:: Marionette.switch_to_frame
.. automethod:: Marionette.switch_to_window
.. automethod:: Marionette.get_active_frame
.. automethod:: Marionette.close

Navigation Methods
^^^^^^^^^^^^^^^^^^
.. autoattribute:: Marionette.title
.. automethod:: Marionette.navigate
.. automethod:: Marionette.get_url
.. automethod:: Marionette.go_back
.. automethod:: Marionette.go_forward
.. automethod:: Marionette.refresh
.. automethod:: Marionette.absolute_url
.. automethod:: Marionette.get_window_type

DOM Element Methods
^^^^^^^^^^^^^^^^^^^
.. automethod:: Marionette.set_search_timeout
.. automethod:: Marionette.find_element
.. automethod:: Marionette.find_elements

Script Execution
^^^^^^^^^^^^^^^^
.. automethod:: Marionette.execute_script
.. automethod:: Marionette.execute_async_script
.. automethod:: Marionette.set_script_timeout

Debugging
^^^^^^^^^
.. autoattribute:: Marionette.page_source
.. automethod:: Marionette.log
.. automethod:: Marionette.get_logs
.. automethod:: Marionette.screenshot

Querying and Modifying Document Content
---------------------------------------
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

.. autoclass:: DateTimeValue
.. autoattribute:: DateTimeValue.date
.. autoattribute:: DateTimeValue.time

Action Objects
--------------

Action Sequences
^^^^^^^^^^^^^^^^
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
^^^^^^^^^^^^^^^^^^^^^^
.. autoclass:: MultiActions
.. automethod:: MultiActions.add
.. automethod:: MultiActions.perform

Explicit Waiting and Expected Conditions
----------------------------------------

Waits are used to pause program execution
until a given condition is true.
This is a useful technique to employ
when documents load new content or change
after ``Document.readyState``'s value changes to "complete".

Because Marionette returns control to the user
when the document is completely loaded,
any subsequent interaction with elements
are subject to manual synchronisation.
The reason for this is that Marionette
does not keep a direct representation of the DOM,
but instead exposes a way for the user to
query the browser's DOM state.

The `Wait` helper class provided by Marionette
avoids some of the caveats of ``time.sleep(n)``,
which sets the condition to an exact time period to wait.
It will return immediately
once the provided condition evaluates to true.

In addition to writing your own custom conditions
you can combine `Wait`
with a number of ready-made expected conditions
that are listed below.

Waits
^^^^^
.. autoclass:: marionette.wait.Wait
   :members:
   :special-members:
.. autoattribute marionette.wait.DEFAULT_TIMEOUT
.. autoattribute marionette.wait.DEFAULT_INTERVAL

Expected Conditions
^^^^^^^^^^^^^^^^^^^
.. automodule:: marionette.expected
   :members:

Indices and tables
==================

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`
