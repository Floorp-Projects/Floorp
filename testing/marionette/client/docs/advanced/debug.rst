Debugging
=========

.. py:currentmodule:: marionette_driver.marionette

Sometimes when working with Marionette you'll run into unexpected behaviour and
need to do some debugging. This page outlines some of the Marionette methods
that can be useful to you.

Please note that the best tools for debugging are the `ones that ship with
Gecko`_. This page doesn't describe how to use those with Marionette. Also see
a related topic about `using the debugger with Marionette`_ on MDN.

.. _ones that ship with Gecko: https://developer.mozilla.org/en-US/docs/Tools
.. _using the debugger with Marionette: https://developer.mozilla.org/en-US/docs/Marionette/Debugging

Seeing What's on the Page
~~~~~~~~~~~~~~~~~~~~~~~~~

Sometimes it's difficult to tell what is actually on the page that is being
manipulated. Either because it happens too fast, the window isn't big enough or
you are manipulating a remote server! There are two methods that can help you
out. The first is :func:`~Marionette.screenshot`::

    marionette.screenshot() # takes screenshot of entire frame
    elem = marionette.find_element(By.ID, 'some-div')
    marionette.screenshot(elem) # takes a screenshot of only the given element

Sometimes you just want to see the DOM layout. You can do this with the
:attr:`~Marionette.page_source` property. Note that the page source depends on
the context you are in::

    print(marionette.page_source)
    marionette.set_context('chrome')
    print(marionette.page_source)
