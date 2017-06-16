# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import errors
import types

from marionette import HTMLElement

"""This file provides a set of expected conditions for common use
cases when writing Marionette tests.

The conditions rely on explicit waits that retries conditions a number
of times until they are either successfully met, or they time out.

"""


class element_present(object):
    """Checks that a web element is present in the DOM of the current
    context.  This does not necessarily mean that the element is
    visible.

    You can select which element to be checked for presence by
    supplying a locator::

        el = Wait(marionette).until(expected.element_present(By.ID, "foo"))

    Or by using a function/lambda returning an element::

        el = Wait(marionette).until(
            expected.element_present(lambda m: m.find_element(By.ID, "foo")))

    :param args: locator or function returning web element
    :returns: the web element once it is located, or False

    """

    def __init__(self, *args):
        if len(args) == 1 and isinstance(args[0], types.FunctionType):
            self.locator = args[0]
        else:
            self.locator = lambda m: m.find_element(*args)

    def __call__(self, marionette):
        return _find(marionette, self.locator)


class element_not_present(element_present):
    """Checks that a web element is not present in the DOM of the current
    context.

    You can select which element to be checked for lack of presence by
    supplying a locator::

        r = Wait(marionette).until(expected.element_not_present(By.ID, "foo"))

    Or by using a function/lambda returning an element::

        r = Wait(marionette).until(
            expected.element_present(lambda m: m.find_element(By.ID, "foo")))

    :param args: locator or function returning web element
    :returns: True if element is not present, or False if it is present

    """

    def __init__(self, *args):
        super(element_not_present, self).__init__(*args)

    def __call__(self, marionette):
        return not super(element_not_present, self).__call__(marionette)


class element_stale(object):
    """Check that the given element is no longer attached to DOM of the
    current context.

    This can be useful for waiting until an element is no longer
    present.

    Sample usage::

        el = marionette.find_element(By.ID, "foo")
        # ...
        Wait(marionette).until(expected.element_stale(el))

    :param element: the element to wait for
    :returns: False if the element is still attached to the DOM, True
        otherwise

    """

    def __init__(self, element):
        self.el = element

    def __call__(self, marionette):
        try:
            # Calling any method forces a staleness check
            self.el.is_enabled()
            return False
        except errors.StaleElementException:
            return True


class elements_present(object):
    """Checks that web elements are present in the DOM of the current
    context.  This does not necessarily mean that the elements are
    visible.

    You can select which elements to be checked for presence by
    supplying a locator::

        els = Wait(marionette).until(expected.elements_present(By.TAG_NAME, "a"))

    Or by using a function/lambda returning a list of elements::

        els = Wait(marionette).until(
            expected.elements_present(lambda m: m.find_elements(By.TAG_NAME, "a")))

    :param args: locator or function returning a list of web elements
    :returns: list of web elements once they are located, or False

    """

    def __init__(self, *args):
        if len(args) == 1 and isinstance(args[0], types.FunctionType):
            self.locator = args[0]
        else:
            self.locator = lambda m: m.find_elements(*args)

    def __call__(self, marionette):
        return _find(marionette, self.locator)


class elements_not_present(elements_present):
    """Checks that web elements are not present in the DOM of the
    current context.

    You can select which elements to be checked for not being present
    by supplying a locator::

        r = Wait(marionette).until(expected.elements_not_present(By.TAG_NAME, "a"))

    Or by using a function/lambda returning a list of elements::

        r = Wait(marionette).until(
            expected.elements_not_present(lambda m: m.find_elements(By.TAG_NAME, "a")))

    :param args: locator or function returning a list of web elements
    :returns: True if elements are missing, False if one or more are
        present

    """

    def __init__(self, *args):
        super(elements_not_present, self).__init__(*args)

    def __call__(self, marionette):
        return not super(elements_not_present, self).__call__(marionette)


class element_displayed(object):
    """An expectation for checking that an element is visible.

    Visibility means that the element is not only displayed, but also
    has a height and width that is greater than 0 pixels.

    Stale elements, meaning elements that have been detached from the
    DOM of the current context are treated as not being displayed,
    meaning this expectation is not analogous to the behaviour of
    calling :func:`~marionette_driver.marionette.HTMLElement.is_displayed`
    on an :class:`~marionette_driver.marionette.HTMLElement`.

    You can select which element to be checked for visibility by
    supplying a locator::

        displayed = Wait(marionette).until(expected.element_displayed(By.ID, "foo"))

    Or by supplying an element::

        el = marionette.find_element(By.ID, "foo")
        displayed = Wait(marionette).until(expected.element_displayed(el))

    :param args: locator or web element
    :returns: True if element is displayed, False if hidden

    """

    def __init__(self, *args):
        self.el = None
        if len(args) == 1 and isinstance(args[0], HTMLElement):
            self.el = args[0]
        else:
            self.locator = lambda m: m.find_element(*args)

    def __call__(self, marionette):
        if self.el is None:
            self.el = _find(marionette, self.locator)
        if not self.el:
            return False
        try:
            return self.el.is_displayed()
        except errors.StaleElementException:
            return False


class element_not_displayed(element_displayed):
    """An expectation for checking that an element is not visible.

    Visibility means that the element is not only displayed, but also
    has a height and width that is greater than 0 pixels.

    Stale elements, meaning elements that have been detached fom the
    DOM of the current context are treated as not being displayed,
    meaning this expectation is not analogous to the behaviour of
    calling :func:`~marionette_driver.marionette.HTMLElement.is_displayed`
    on an :class:`~marionette_driver.marionette.HTMLElement`.

    You can select which element to be checked for visibility by
    supplying a locator::

        hidden = Wait(marionette).until(expected.element_not_displayed(By.ID, "foo"))

    Or by supplying an element::

        el = marionette.find_element(By.ID, "foo")
        hidden = Wait(marionette).until(expected.element_not_displayed(el))

    :param args: locator or web element
    :returns: True if element is hidden, False if displayed

    """

    def __init__(self, *args):
        super(element_not_displayed, self).__init__(*args)

    def __call__(self, marionette):
        return not super(element_not_displayed, self).__call__(marionette)


class element_selected(object):
    """An expectation for checking that the given element is selected.

    :param element: the element to be selected
    :returns: True if element is selected, False otherwise

    """

    def __init__(self, element):
        self.el = element

    def __call__(self, marionette):
        return self.el.is_selected()


class element_not_selected(element_selected):
    """An expectation for checking that the given element is not
    selected.

    :param element: the element to not be selected
    :returns: True if element is not selected, False if selected

    """

    def __init__(self, element):
        super(element_not_selected, self).__init__(element)

    def __call__(self, marionette):
        return not super(element_not_selected, self).__call__(marionette)


class element_enabled(object):
    """An expectation for checking that the given element is enabled.

    :param element: the element to check if enabled
    :returns: True if element is enabled, False otherwise

    """

    def __init__(self, element):
        self.el = element

    def __call__(self, marionette):
        return self.el.is_enabled()


class element_not_enabled(element_enabled):
    """An expectation for checking that the given element is disabled.

    :param element: the element to check if disabled
    :returns: True if element is disabled, False if enabled

    """

    def __init__(self, element):
        super(element_not_enabled, self).__init__(element)

    def __call__(self, marionette):
        return not super(element_not_enabled, self).__call__(marionette)


def _find(marionette, func):
    el = None

    try:
        el = func(marionette)
    except errors.NoSuchElementException:
        pass

    if el is None:
        return False
    return el
