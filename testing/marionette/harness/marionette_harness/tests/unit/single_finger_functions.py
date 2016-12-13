from marionette_driver.marionette import Actions
from marionette_driver.errors import TimeoutException
from marionette_driver.by import By


def wait_for_condition_else_raise(marionette, wait_for_condition, expected, script):
    try:
        wait_for_condition(lambda m: expected in m.execute_script(script))
    except TimeoutException as e:
        raise TimeoutException("{0} got {1} instead of {2}".format(
            e.message, marionette.execute_script(script), expected))

def press_release(marionette, times, wait_for_condition, expected):
    testAction = marionette.absolute_url("testAction.html")
    marionette.navigate(testAction)
    action = Actions(marionette)
    button = marionette.find_element(By.ID, "button1")
    action.press(button).release()
    # Insert wait between each press and release chain.
    for _ in range(times-1):
        action.wait(0.1)
        action.press(button).release()
    action.perform()
    wait_for_condition_else_raise(marionette, wait_for_condition, expected, "return document.getElementById('button1').innerHTML;")

def move_element(marionette, wait_for_condition, expected1, expected2):
    testAction = marionette.absolute_url("testAction.html")
    marionette.navigate(testAction)
    ele = marionette.find_element(By.ID, "button1")
    drop = marionette.find_element(By.ID, "button2")
    action = Actions(marionette)
    action.press(ele).move(drop).release()
    action.perform()
    wait_for_condition_else_raise(marionette, wait_for_condition, expected1, "return document.getElementById('button1').innerHTML;")
    wait_for_condition_else_raise(marionette, wait_for_condition, expected2, "return document.getElementById('button2').innerHTML;")

def move_element_offset(marionette, wait_for_condition, expected1, expected2):
    testAction = marionette.absolute_url("testAction.html")
    marionette.navigate(testAction)
    ele = marionette.find_element(By.ID, "button1")
    action = Actions(marionette)
    action.press(ele).move_by_offset(0,150).move_by_offset(0, 150).release()
    action.perform()
    wait_for_condition_else_raise(marionette, wait_for_condition, expected1, "return document.getElementById('button1').innerHTML;")
    wait_for_condition_else_raise(marionette, wait_for_condition, expected2, "return document.getElementById('button2').innerHTML;")

def chain(marionette, wait_for_condition, expected1, expected2):
    testAction = marionette.absolute_url("testAction.html")
    marionette.navigate(testAction)
    marionette.timeout.implicit = 15
    action = Actions(marionette)
    button1 = marionette.find_element(By.ID, "button1")
    action.press(button1).perform()
    button2 = marionette.find_element(By.ID, "delayed")
    wait_for_condition_else_raise(marionette, wait_for_condition, expected1, "return document.getElementById('button1').innerHTML;")
    action.move(button2).release().perform()
    wait_for_condition_else_raise(marionette, wait_for_condition, expected2, "return document.getElementById('delayed').innerHTML;")

def chain_flick(marionette, wait_for_condition, expected1, expected2):
    testAction = marionette.absolute_url("testAction.html")
    marionette.navigate(testAction)
    button = marionette.find_element(By.ID, "button1")
    action = Actions(marionette)
    action.flick(button, 0, 0, 0, 200).perform()
    wait_for_condition_else_raise(marionette, wait_for_condition, expected1,"return document.getElementById('button1').innerHTML;")
    wait_for_condition_else_raise(marionette, wait_for_condition, expected2,"return document.getElementById('buttonFlick').innerHTML;")


def wait(marionette, wait_for_condition, expected):
    testAction = marionette.absolute_url("testAction.html")
    marionette.navigate(testAction)
    action = Actions(marionette)
    button = marionette.find_element(By.ID, "button1")
    action.press(button).wait().release().perform()
    wait_for_condition_else_raise(marionette, wait_for_condition, expected, "return document.getElementById('button1').innerHTML;")

def wait_with_value(marionette, wait_for_condition, expected):
    testAction = marionette.absolute_url("testAction.html")
    marionette.navigate(testAction)
    button = marionette.find_element(By.ID, "button1")
    action = Actions(marionette)
    action.press(button).wait(0.01).release()
    action.perform()
    wait_for_condition_else_raise(marionette, wait_for_condition, expected, "return document.getElementById('button1').innerHTML;")

def context_menu(marionette, wait_for_condition, expected1, expected2):
    testAction = marionette.absolute_url("testAction.html")
    marionette.navigate(testAction)
    button = marionette.find_element(By.ID, "button1")
    action = Actions(marionette)
    action.press(button).wait(5).perform()
    wait_for_condition_else_raise(marionette, wait_for_condition, expected1, "return document.getElementById('button1').innerHTML;")
    action.release().perform()
    wait_for_condition_else_raise(marionette, wait_for_condition, expected2, "return document.getElementById('button1').innerHTML;")

def long_press_action(marionette, wait_for_condition, expected):
    testAction = marionette.absolute_url("testAction.html")
    marionette.navigate(testAction)
    button = marionette.find_element(By.ID, "button1")
    action = Actions(marionette)
    action.long_press(button, 5).perform()
    wait_for_condition_else_raise(marionette, wait_for_condition, expected, "return document.getElementById('button1').innerHTML;")

def long_press_on_xy_action(marionette, wait_for_condition, expected):
    testAction = marionette.absolute_url("testAction.html")
    marionette.navigate(testAction)
    html = marionette.find_element(By.TAG_NAME, "html")
    button = marionette.find_element(By.ID, "button1")
    action = Actions(marionette)

    # Press the center of the button with respect to html.
    x = button.rect['x'] + button.rect['width'] / 2.0
    y = button.rect['y'] + button.rect['height'] / 2.0
    action.long_press(html, 5, x, y).perform()
    wait_for_condition_else_raise(marionette, wait_for_condition, expected, "return document.getElementById('button1').innerHTML;")

def single_tap(marionette, wait_for_condition, expected):
    testAction = marionette.absolute_url("testAction.html")
    marionette.navigate(testAction)
    button = marionette.find_element(By.ID, "button1")
    action = Actions(marionette)
    action.tap(button).perform()
    wait_for_condition_else_raise(marionette, wait_for_condition, expected, "return document.getElementById('button1').innerHTML;")

def double_tap(marionette, wait_for_condition, expected):
    testAction = marionette.absolute_url("testAction.html")
    marionette.navigate(testAction)
    button = marionette.find_element(By.ID, "button1")
    action = Actions(marionette)
    action.double_tap(button).perform()
    wait_for_condition_else_raise(marionette, wait_for_condition, expected, "return document.getElementById('button1').innerHTML;")
