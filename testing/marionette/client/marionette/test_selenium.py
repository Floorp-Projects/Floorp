# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import threading

try:
    from selenium import webdriver
    from selenium.common.exceptions import *
    from selenium.webdriver.remote.webelement import WebElement
except:
    print 'requires selenium Python bindings; pip install selenium'
    raise
from selenium_proxy import SeleniumProxy
from testserver import TestServer

def test_find_element(driver, fn):
    """ Test a find_element_by_FOO method of both the webdriver
        and the WebElement class.  The former will return a WebElement which
        should have a method of the same name, which should also return
        a WebElement.
    """
    element = getattr(driver, fn)('foo')
    assert(isinstance(element, WebElement))
    assert(element.id == TestServer.TEST_FIND_ELEMENT)
    child = getattr(element, fn)('foo')
    assert(isinstance(child, WebElement))
    assert(child.id == TestServer.TEST_FIND_ELEMENT)

def test_find_elements(driver, fn):
    """ Test a find_elements_by_FOO method of both the webdriver
        and the WebElement class.  The former will return a list of 
        WebElements, each of which should have a method of the same name,
        and which in should turn should also return a list of WebElements.
    """
    elements = getattr(driver, fn)('foo')
    # elements should be a list
    assert(isinstance(elements, list))
    # elements should match the TEST_FIND_ELEMENTS list
    assert(map(lambda x: x.id, elements) == TestServer.TEST_FIND_ELEMENTS)
    # Each member of elements should be a WebElement that has the same
    # method, which should in turn return a list of WebElements when called.
    for element in elements:
        assert(isinstance(element, WebElement))
        children = getattr(element, fn)('foo')
        assert(isinstance(children, list))
        assert(map(lambda x: x.id, children) == TestServer.TEST_FIND_ELEMENTS)
        assert(len(filter(lambda x: not isinstance(x, WebElement), children)) == 0)

if __name__ == '__main__':
    # start the test server on port 2626
    server = TestServer(2626)
    thread = threading.Thread(target=server.run)
    thread.daemon = True
    thread.start()

    # Start the selenium proxy on port 4444, connecting to the test server
    # on port 2626.
    proxy = SeleniumProxy('127.0.0.1', 2626, proxy_port=4444)
    proxy_thread = threading.Thread(target=proxy.start)
    proxy_thread.daemon = True
    proxy_thread.start()

    # invoke selenium commands as tests
    driver = webdriver.Remote(command_executor='http://127.0.0.1:4444',
                              desired_capabilities=webdriver.DesiredCapabilities.FIREFOX)
    assert(driver)

    # test navigation methods
    driver.get(TestServer.TEST_URL)
    assert(driver.current_url == TestServer.TEST_URL)
    driver.back()
    driver.forward()
    driver.refresh()

    # test script methods
    driver.set_script_timeout(10) # in selenium the number is in seconds
    driver.implicitly_wait(10)    # ditto

    assert(TestServer.TEST_EXECUTE_RETURN_VALUE == driver.execute_script(TestServer.TEST_EXECUTE_SCRIPT))
    assert(TestServer.TEST_EXECUTE_RETURN_VALUE == driver.execute_script(TestServer.TEST_EXECUTE_SCRIPT,
                                                                         TestServer.TEST_EXECUTE_SCRIPT_ARGS))
    assert(TestServer.TEST_EXECUTE_RETURN_VALUE == driver.execute_async_script(TestServer.TEST_EXECUTE_SCRIPT))
    assert(TestServer.TEST_EXECUTE_RETURN_VALUE == driver.execute_async_script(TestServer.TEST_EXECUTE_SCRIPT,
                                                                               TestServer.TEST_EXECUTE_SCRIPT_ARGS))

    # test all the find_element_by_FOO methods
    test_find_element(driver, 'find_element_by_name')
    test_find_element(driver, 'find_element_by_id')
    test_find_element(driver, 'find_element_by_xpath')
    test_find_element(driver, 'find_element_by_link_text')
    test_find_element(driver, 'find_element_by_partial_link_text')
    test_find_element(driver, 'find_element_by_tag_name')
    test_find_element(driver, 'find_element_by_class_name')
    test_find_element(driver, 'find_element_by_css_selector')

    # test all the find_elements_by_FOO methods
    test_find_elements(driver, 'find_elements_by_name')
    test_find_elements(driver, 'find_elements_by_id')
    test_find_elements(driver, 'find_elements_by_xpath')
    test_find_elements(driver, 'find_elements_by_link_text')
    test_find_elements(driver, 'find_elements_by_partial_link_text')
    test_find_elements(driver, 'find_elements_by_tag_name')
    test_find_elements(driver, 'find_elements_by_class_name')
    test_find_elements(driver, 'find_elements_by_css_selector')

    # test WebElement methods
    element = driver.find_element_by_name('foo')
    element.click()
    assert(element.text == TestServer.TEST_GET_TEXT)
    element.send_keys('Mozilla Firefox')
    element.clear()
    assert(element.is_selected())
    assert(element.is_enabled())
    assert(element.is_displayed())
    assert(element.get_attribute('id') == TestServer.TEST_GET_VALUE)

    # make the server return error responses so we can test them
    server.responses = server.error_responses

    # test exception handling
    try:
        driver.execute_async_script(TestServer.TEST_EXECUTE_SCRIPT)
        assert(False)
    except TimeoutException:
        # the Selenium Python driver maps SCRIPT_TIMEOUT to TIMEOUT
        pass

    try:
        driver.execute_script(TestServer.TEST_EXECUTE_SCRIPT)
        assert(False)
    except WebDriverException:
        # the Selenium Python driver doesn't specifically support JAVASCRIPT_ERROR
        pass

    try:
        driver.find_element_by_name('foo')
        assert(False)
    except NoSuchElementException:
        pass

    try:
        driver.find_elements_by_name('foo')
        assert(False)
    except WebDriverException:
        # the Selenium Python driver doesn't specifically support XPATH_LOOKUP_ERROR
        pass

    try:
        driver.close()
        assert(False)
    except NoSuchWindowException:
        pass

    try:
        element.click()
        assert(False)
    except StaleElementReferenceException:
        pass

    try:
        element.send_keys('Mozilla Firefox')
        assert(False)
    except ElementNotVisibleException:
        pass

    try:
        driver.switch_to_frame('aframe')
        assert(False)
    except NoSuchFrameException:
        pass

    # restore normal test responses
    server.responses = server.test_responses

    # test window methods
    assert(driver.current_window_handle == TestServer.TEST_CURRENT_WINDOW)
    assert(driver.window_handles == TestServer.TEST_WINDOW_LIST)
    driver.switch_to_window(TestServer.TEST_CURRENT_WINDOW)

    # test frame methods
    driver.switch_to_frame('aframe') # by name or id
    driver.switch_to_frame(1)        # by index
    driver.switch_to_frame(element)  # by element reference
    driver.switch_to_frame(None)     # null; switch to default frame

    driver.close() # this is close_window

    print 'Tests complete!'

