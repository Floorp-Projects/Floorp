# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import pytest
import time

from selenium.common.exceptions import NoAlertPresentException, NoSuchElementException
from selenium.webdriver.common.by import By


def expect_alert(session, text=None):
    try:
        alert = session.switch_to.alert
        if text is not None:
            assert alert.text == text
        alert.dismiss()
    except NoAlertPresentException:
        pass


def is_float_cleared(session, elem1, elem2):
    return session.execute_script(
        """return (function(a, b) {
        // Ensure that a is placed under
        // (and not to the right of) b
        return a?.offsetTop >= b?.offsetTop + b?.offsetHeight &&
               a?.offsetLeft < b?.offsetLeft + b?.offsetWidth;
        }(arguments[0], arguments[1]));""",
        elem1,
        elem2,
    )


def await_getUserMedia_call_on_click(session, elem_to_click):
    return session.execute_script(
        """
        navigator.mediaDevices.getUserMedia =
            navigator.mozGetUserMedia =
            navigator.getUserMedia =
            () => { window.__gumCalled = true; };
    """
    )
    elem_to_click.click()
    return session.execute_async_script(
        """
        var done = arguments[0];
        setInterval(500, () => {
            if (window.__gumCalled === true) {
                done();
            }
        });
    """
    )


class Xpath:
    by = By.XPATH

    def __init__(self, value):
        self.value = value


class Css:
    by = By.CSS_SELECTOR

    def __init__(self, value):
        self.value = value


class Text:
    by = By.XPATH

    def __init__(self, value):
        self.value = f"//*[contains(text(),'{value}')]"


Missing = object()


def find_elements(session, selector, all=True, default=Missing):
    try:
        if all:
            return session.find_elements(selector.by, selector.value)
        return session.find_element(selector.by, selector.value)
    except NoSuchElementException:
        if default is not Missing:
            return default
        raise


def find_element(session, selector, default=Missing):
    return find_elements(session, selector, all=False, default=default)


def assert_not_element(session, sel):
    with pytest.raises(NoSuchElementException):
        find_element(session, sel)


def await_first_element_of(session, selectors, timeout=None, is_displayed=False):
    t0 = time.time()
    exc = None
    if timeout is None:
        timeout = 10
    found = [None for sel in selectors]
    while time.time() < t0 + timeout:
        for i, selector in enumerate(selectors):
            try:
                ele = find_element(session, selector)
                if not is_displayed or ele.is_displayed():
                    found[i] = ele
                    return found
            except NoSuchElementException as e:
                exc = e
        time.sleep(0.5)
    raise exc if exc is not None else NoSuchElementException
    return found


def await_element(session, selector, timeout=None, default=None):
    return await_first_element_of(session, [selector], timeout, default)[0]


def load_page_and_wait_for_iframe(session, url, selector, loads=1, timeout=None):
    while loads > 0:
        session.get(url)
        frame = await_element(session, selector, timeout=timeout)
        loads -= 1
    session.switch_to.frame(frame)
    return frame


def await_dom_ready(session):
    session.execute_async_script(
        """
        const cb = arguments[0];
        setInterval(() => {
            if (document.readyState === "complete") {
                cb();
            }
        }, 500);
    """
    )
