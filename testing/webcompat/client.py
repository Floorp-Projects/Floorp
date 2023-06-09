# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import asyncio
import contextlib
import time
from urllib.parse import quote

import webdriver


class Client:
    def __init__(self, session, event_loop):
        self.session = session
        self.event_loop = event_loop
        self.content_blocker_loaded = False

    @property
    def current_url(self):
        return self.session.url

    @property
    def alert(self):
        return self.session.alert

    @property
    def context(self):
        return self.session.send_session_command("GET", "moz/context")

    @context.setter
    def context(self, context):
        self.session.send_session_command("POST", "moz/context", {"context": context})

    @contextlib.contextmanager
    def using_context(self, context):
        orig_context = self.context
        needs_change = context != orig_context

        if needs_change:
            self.context = context

        try:
            yield
        finally:
            if needs_change:
                self.context = orig_context

    def wait_for_content_blocker(self):
        if not self.content_blocker_loaded:
            with self.using_context("chrome"):
                self.session.execute_async_script(
                    """
                    const done = arguments[0],
                          signal = "safebrowsing-update-finished";
                    function finish() {
                        Services.obs.removeObserver(finish, signal);
                        done();
                    }
                    Services.obs.addObserver(finish, signal);
                """
                )
                self.content_blocker_loaded = True

    @property
    def keyboard(self):
        return self.session.actions.sequence("key", "keyboard_id")

    @property
    def mouse(self):
        return self.session.actions.sequence(
            "pointer", "pointer_id", {"pointerType": "mouse"}
        )

    @property
    def pen(self):
        return self.session.actions.sequence(
            "pointer", "pointer_id", {"pointerType": "pen"}
        )

    @property
    def touch(self):
        return self.session.actions.sequence(
            "pointer", "pointer_id", {"pointerType": "touch"}
        )

    @property
    def wheel(self):
        return self.session.actions.sequence("wheel", "wheel_id")

    @property
    def modifier_key(self):
        if self.session.capabilities["platformName"] == "mac":
            return "\ue03d"  # meta (command)
        else:
            return "\ue009"  # control

    def inline(self, doc):
        return "data:text/html;charset=utf-8,{}".format(quote(doc))

    async def navigate(self, url, timeout=None, await_console_message=None):
        if timeout is not None:
            old_timeout = self.session.timeouts.page_load
            self.session.timeouts.page_load = timeout
        if self.session.test_config.get("use_pbm") or self.session.test_config.get(
            "use_strict_etp"
        ):
            print("waiting for content blocker...")
            self.wait_for_content_blocker()
        if await_console_message is not None:
            console_message = self.monitor_for_console_message(await_console_message)
        await self.session.bidi_session.session.subscribe(events=["log.entryAdded"])
        try:
            self.session.url = url
        except webdriver.error.TimeoutException as e:
            if timeout is None:
                raise e
        if await_console_message is not None:
            await console_message
            await self.session.bidi_session.session.unsubscribe(
                events=["log.entryAdded"]
            )
        if timeout is not None:
            self.session.timeouts.page_load = old_timeout

    async def promise_console_message(self, message, timeout=20):
        promise = self.monitor_for_console_message(message, timeout)
        await self.session.bidi_session.session.subscribe(events=["log.entryAdded"])
        await promise
        await self.session.bidi_session.session.unsubscribe(events=["log.entryAdded"])

    def back(self):
        self.session.back()

    def switch_to_frame(self, frame):
        return self.session.transport.send(
            "POST",
            "session/{session_id}/frame".format(**vars(self.session)),
            {"id": frame},
            encoder=webdriver.protocol.Encoder,
            decoder=webdriver.protocol.Decoder,
            session=self.session,
        )

    def switch_frame(self, frame):
        self.session.switch_frame(frame)

    async def load_page_and_wait_for_iframe(
        self, url, finder, loads=1, timeout=None, **kwargs
    ):
        while loads > 0:
            await self.navigate(url, **kwargs)
            frame = self.await_element(finder, timeout=timeout)
            loads -= 1
        self.switch_frame(frame)
        return frame

    def promise_bidi_event(self, event_name: str, check_fn=None, timeout=5):
        future = self.event_loop.create_future()

        async def on_event(method, data):
            print("on_event", method, data)
            if check_fn is not None and not check_fn(method, data):
                return
            remove_listener()
            future.set_result(data)

        remove_listener = self.session.bidi_session.add_event_listener(
            event_name, on_event
        )
        return asyncio.wait_for(future, timeout=timeout)

    def monitor_for_console_message(self, msg, timeout=20):
        def check_messages(method, data):
            if "text" in data:
                if msg in data["text"]:
                    return True
            if "args" in data and len(data["args"]) and "value" in data["args"][0]:
                if msg in data["args"][0]["value"]:
                    return True

        return self.promise_bidi_event(
            "log.entryAdded", check_messages, timeout=timeout
        )

    def execute_script(self, script, *args):
        return self.session.execute_script(script, args=args)

    def execute_async_script(self, script, *args, **kwargs):
        return self.session.execute_async_script(script, args, **kwargs)

    def clear_all_cookies(self):
        self.session.transport.send(
            "DELETE", "session/%s/cookie" % self.session.session_id
        )

    def send_element_command(self, element, method, uri, body=None):
        url = "element/%s/%s" % (element.id, uri)
        return self.session.send_session_command(method, url, body)

    def get_element_attribute(self, element, name):
        return self.send_element_command(element, "GET", "attribute/%s" % name)

    def _do_is_displayed_check(self, ele, is_displayed):
        if ele is None:
            return None

        if type(ele) in [list, tuple]:
            return [x for x in ele if self._do_is_displayed_check(x, is_displayed)]

        if is_displayed is False and ele and self.is_displayed(ele):
            return None
        if is_displayed is True and ele and not self.is_displayed(ele):
            return None
        return ele

    def find_css(self, *args, all=False, is_displayed=None, **kwargs):
        try:
            ele = self.session.find.css(*args, all=all, **kwargs)
            return self._do_is_displayed_check(ele, is_displayed)
        except webdriver.error.NoSuchElementException:
            return None

    def find_xpath(self, xpath, all=False, is_displayed=None):
        route = "elements" if all else "element"
        body = {"using": "xpath", "value": xpath}
        try:
            ele = self.session.send_session_command("POST", route, body)
            return self._do_is_displayed_check(ele, is_displayed)
        except webdriver.error.NoSuchElementException:
            return None

    def find_text(self, text, is_displayed=None, **kwargs):
        try:
            ele = self.find_xpath(f"//*[contains(text(),'{text}')]", **kwargs)
            return self._do_is_displayed_check(ele, is_displayed)
        except webdriver.error.NoSuchElementException:
            return None

    def find_element(self, finder, is_displayed=None, **kwargs):
        ele = finder.find(self, **kwargs)
        return self._do_is_displayed_check(ele, is_displayed)

    def await_css(self, selector, **kwargs):
        return self.await_element(self.css(selector), **kwargs)

    def await_xpath(self, selector, **kwargs):
        return self.await_element(self.xpath(selector), **kwargs)

    def await_text(self, selector, *args, **kwargs):
        return self.await_element(self.text(selector), **kwargs)

    def await_element(self, finder, **kwargs):
        return self.await_first_element_of([finder], **kwargs)[0]

    class css:
        def __init__(self, selector):
            self.selector = selector

        def find(self, client, **kwargs):
            return client.find_css(self.selector, **kwargs)

    class xpath:
        def __init__(self, selector):
            self.selector = selector

        def find(self, client, **kwargs):
            return client.find_xpath(self.selector, **kwargs)

    class text:
        def __init__(self, selector):
            self.selector = selector

        def find(self, client, **kwargs):
            return client.find_text(self.selector, **kwargs)

    def await_first_element_of(self, finders, timeout=None, delay=0.25, **kwargs):
        t0 = time.time()

        if timeout is None:
            timeout = 10

        found = [None for finder in finders]

        exc = None
        while time.time() < t0 + timeout:
            for i, finder in enumerate(finders):
                try:
                    result = finder.find(self, **kwargs)
                    if result:
                        found[i] = result
                        return found
                except webdriver.error.NoSuchElementException as e:
                    exc = e
            time.sleep(delay)
        raise exc if exc is not None else webdriver.error.NoSuchElementException
        return found

    async def dom_ready(self, timeout=None):
        if timeout is None:
            timeout = 20

        async def wait():
            return self.session.execute_async_script(
                """
                const cb = arguments[0];
                setInterval(() => {
                    if (document.readyState === "complete") {
                        cb();
                    }
                }, 500);
            """
            )

        task = asyncio.create_task(wait())
        return await asyncio.wait_for(task, timeout)

    def is_float_cleared(self, elem1, elem2):
        return self.session.execute_script(
            """return (function(a, b) {
            // Ensure that a is placed under b (and not to its right)
            return a?.offsetTop >= b?.offsetTop + b?.offsetHeight &&
                   a?.offsetLeft < b?.offsetLeft + b?.offsetWidth;
            }(arguments[0], arguments[1]));""",
            elem1,
            elem2,
        )

    @contextlib.contextmanager
    def assert_getUserMedia_called(self):
        self.execute_script(
            """
            navigator.mediaDevices.getUserMedia =
                navigator.mozGetUserMedia =
                navigator.getUserMedia =
                () => { window.__gumCalled = true; };
        """
        )
        yield
        assert self.execute_script("return window.__gumCalled === true;")

    def await_element_hidden(self, finder, timeout=None, delay=0.25):
        t0 = time.time()

        if timeout is None:
            timeout = 20

        elem = finder.find(self)
        while time.time() < t0 + timeout:
            try:
                if not self.is_displayed(elem):
                    return
                time.sleep(delay)
            except webdriver.error.StaleElementReferenceException:
                return

    def soft_click(self, element):
        self.execute_script("arguments[0].click()", element)

    def remove_element(self, element):
        self.execute_script("arguments[0].remove()", element)

    def scroll_into_view(self, element):
        self.execute_script(
            "arguments[0].scrollIntoView({block:'center', inline:'center', behavior: 'instant'})",
            element,
        )

    def test_for_fastclick(self, element):
        # FastClick cancels touchend, breaking default actions on Fenix.
        # It instead fires a mousedown or click, which we can detect.
        self.execute_script(
            """
                const sel = arguments[0];
                window.fastclicked = false;
                const evt = sel.nodeName === "SELECT" ? "mousedown" : "click";
                document.addEventListener(evt, e => {
                    if (e.target === sel && !e.isTrusted) {
                        window.fastclicked = true;
                    }
                }, true);
            """,
            element,
        )
        self.scroll_into_view(element)
        # tap a few times in case the site's other code interferes
        self.touch.click(element=element).perform()
        self.touch.click(element=element).perform()
        self.touch.click(element=element).perform()
        return self.execute_script("return window.fastclicked")

    def is_displayed(self, element):
        if element is None:
            return False

        return self.session.execute_script(
            """
              const e = arguments[0],
                    s = window.getComputedStyle(e),
                    v = s.visibility === "visible",
                    o = Math.abs(parseFloat(s.opacity));
              return e.getClientRects().length > 0 && v && (isNaN(o) || o === 1.0);
          """,
            args=[element],
        )
