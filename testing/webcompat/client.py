# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import asyncio
import contextlib
import time
from urllib.parse import quote

import webdriver
from webdriver.bidi.modules.script import ContextTarget


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

    async def top_context(self):
        contexts = await self.session.bidi_session.browsing_context.get_tree()
        return contexts[0]

    async def navigate(self, url, timeout=None, **kwargs):
        return await asyncio.wait_for(
            asyncio.ensure_future(self._navigate(url, **kwargs)), timeout=timeout
        )

    async def _navigate(self, url, wait="complete", await_console_message=None):
        if self.session.test_config.get("use_pbm") or self.session.test_config.get(
            "use_strict_etp"
        ):
            print("waiting for content blocker...")
            self.wait_for_content_blocker()
        if await_console_message is not None:
            console_message = await self.promise_console_message_listener(
                await_console_message
            )
        if wait == "load":
            page_load = await self.promise_readystate_listener("load", url=url)
        try:
            await self.session.bidi_session.browsing_context.navigate(
                context=(await self.top_context())["context"],
                url=url,
                wait=wait if wait != "load" else None,
            )
        except webdriver.bidi.error.UnknownErrorException as u:
            m = str(u)
            if (
                "NS_BINDING_ABORTED" not in m
                and "NS_ERROR_ABORT" not in m
                and "NS_ERROR_WONT_HANDLE_CONTENT" not in m
            ):
                raise u
        if wait == "load":
            await page_load
        if await_console_message is not None:
            await console_message

    async def promise_event_listener(self, events, check_fn=None, timeout=20):
        if type(events) is not list:
            events = [events]

        await self.session.bidi_session.session.subscribe(events=events)

        future = self.event_loop.create_future()

        listener_removers = []

        def remove_listeners():
            for listener_remover in listener_removers:
                try:
                    listener_remover()
                except Exception:
                    pass

        async def on_event(method, data):
            print("on_event", method, data)
            val = None
            if check_fn is not None:
                val = check_fn(method, data)
                if val is None:
                    return
            future.set_result(val)

        for event in events:
            r = self.session.bidi_session.add_event_listener(event, on_event)
            listener_removers.append(r)

        async def task():
            try:
                return await asyncio.wait_for(future, timeout=timeout)
            finally:
                remove_listeners()
                try:
                    await asyncio.wait_for(
                        self.session.bidi_session.session.unsubscribe(events=events),
                        timeout=4,
                    )
                except asyncio.exceptions.TimeoutError:
                    print("Unexpectedly timed out unsubscribing", events)
                    pass

        return asyncio.create_task(task())

    async def promise_console_message_listener(self, msg, **kwargs):
        def check(method, data):
            if "text" in data:
                if msg in data["text"]:
                    return data
            if "args" in data and len(data["args"]):
                for arg in data["args"]:
                    if "value" in arg and msg in arg["value"]:
                        return data

        return await self.promise_event_listener("log.entryAdded", check, **kwargs)

    async def is_console_message(self, message):
        try:
            await (await self.promise_console_message_listener(message, timeout=2))
            return True
        except asyncio.exceptions.TimeoutError:
            return False

    async def promise_readystate_listener(self, state, url=None, **kwargs):
        event = f"browsingContext.{state}"

        def check(method, data):
            if url is None or url in data["url"]:
                return data

        return await self.promise_event_listener(event, check, **kwargs)

    async def promise_frame_listener(self, url, state="domContentLoaded", **kwargs):
        event = f"browsingContext.{state}"

        def check(method, data):
            if url is None or url in data["url"]:
                return Client.Context(self, data["context"])

        return await self.promise_event_listener(event, check, **kwargs)

    async def find_frame_context_by_url(self, url):
        def find_in(arr, url):
            for context in arr:
                if url in context["url"]:
                    return context
            for context in arr:
                found = find_in(context["children"], url)
                if found:
                    return found

        return find_in([await self.top_context()], url)

    class Context:
        def __init__(self, client, id):
            self.client = client
            self.target = ContextTarget(id)

        async def find_css(self, selector, all=False):
            all = "All" if all else ""
            return await self.client.session.bidi_session.script.evaluate(
                expression=f"document.querySelector{all}('{selector}')",
                target=self.target,
                await_promise=False,
            )

        def timed_js(self, timeout, poll, fn, is_displayed=False):
            return f"""() => new Promise((_good, _bad) => {{
                        {self.is_displayed_js()}
                        var _poll = {poll} * 1000;
                        var _time = {timeout} * 1000;
                        var _done = false;
                        var resolve = val => {{
                            if ({is_displayed}) {{
                                if (val.length) {{
                                    val = val.filter(v = is_displayed(v));
                                }} else {{
                                    val = is_displayed(val) && val;
                                }}
                                if (!val.length && !val.matches) {{
                                    return;
                                }}
                            }}
                            _done = true;
                            clearInterval(_int);
                            _good(val);
                        }};
                        var reject = str => {{
                            _done = true;
                            clearInterval(_int);
                            _bad(val);
                        }};
                        var _int = setInterval(() => {{
                            {fn};
                            if (!_done) {{
                                _time -= _poll;
                                if (_time <= 0) {{
                                    reject();
                                }}
                            }}
                        }}, poll);
                    }})"""

        def is_displayed_js(self):
            return """
                function is_displayed(e) {
                    const s = window.getComputedStyle(e),
                          v = s.visibility === "visible",
                          o = Math.abs(parseFloat(s.opacity));
                    return e.getClientRects().length > 0 && v && (isNaN(o) || o === 1.0);
                }
                """

        async def await_css(
            self,
            selector,
            all=False,
            timeout=10,
            poll=0.25,
            condition=False,
            is_displayed=False,
        ):
            all = "All" if all else ""
            condition = (
                f"var elem=arguments[0]; if ({condition})" if condition else False
            )
            return await self.client.session.bidi_session.script.evaluate(
                expression=self.timed_js(
                    timeout,
                    poll,
                    f"""
                    var ele = document.querySelector{all}('{selector}')";
                    if (ele && (!"length" in ele || ele.length > 0)) {{
                        '{condition}'
                        resolve(ele);
                    }}
                    """,
                ),
                target=self.target,
                await_promise=True,
            )

        async def await_text(self, text, **kwargs):
            xpath = f"//*[contains(text(),'{text}')]"
            return await self.await_xpath(self, xpath, **kwargs)

        async def await_xpath(
            self, xpath, all=False, timeout=10, poll=0.25, is_displayed=False
        ):
            all = "true" if all else "false"
            return await self.client.session.bidi_session.script.evaluate(
                expression=self.timed_js(
                    timeout,
                    poll,
                    """
                    var ret = [];
                    var r, res = document.evaluate(`{xpath}`, document, null, 4);
                    while (r = res.iterateNext()) {
                        ret.push(r);
                    }
                    resolve({all} ? ret : ret[0]);
                    """,
                ),
                target=self.target,
                await_promise=True,
            )

    def wrap_script_args(self, args):
        if args is None:
            return args
        out = []
        for arg in args:
            if arg is None:
                out.append({"type": "undefined"})
                continue
            t = type(arg)
            if t == int or t == float:
                out.append({"type": "number", "value": arg})
            elif t == bool:
                out.append({"type": "boolean", "value": arg})
            elif t == str:
                out.append({"type": "string", "value": arg})
            else:
                if "type" in arg:
                    out.push(arg)
                    continue
                raise ValueError(f"Unhandled argument type: {t}")
        return out

    class PreloadScript:
        def __init__(self, client, script, target):
            self.client = client
            self.script = script
            if type(target) == list:
                self.target = target[0]
            else:
                self.target = target

        def stop(self):
            return self.client.session.bidi_session.script.remove_preload_script(
                script=self.script
            )

        async def run(self, fn, *args, await_promise=False):
            val = await self.client.session.bidi_session.script.call_function(
                arguments=self.client.wrap_script_args(args),
                await_promise=await_promise,
                function_declaration=fn,
                target=self.target,
            )
            if val and "value" in val:
                return val["value"]
            return val

    async def make_preload_script(self, text, sandbox, args=None, context=None):
        if not context:
            context = (await self.top_context())["context"]
        target = ContextTarget(context, sandbox)
        if args is None:
            text = f"() => {{ {text} }}"
        script = await self.session.bidi_session.script.add_preload_script(
            function_declaration=text,
            arguments=self.wrap_script_args(args),
            sandbox=sandbox,
        )
        return Client.PreloadScript(self, script, target)

    async def await_alert(self, text):
        if not hasattr(self, "alert_preload_script"):
            self.alert_preload_script = await self.make_preload_script(
                """
                    window.__alerts = [];
                    window.wrappedJSObject.alert = function(text) {
                        window.__alerts.push(text);
                    }
                """,
                "alert_detector",
            )
        return self.alert_preload_script.run(
            """(msg) => new Promise(done => {
                    const to = setInterval(() => {
                        if (window.__alerts.includes(msg)) {
                            clearInterval(to);
                            done();
                        }
                    }, 200);
               })
            """,
            text,
            await_promise=True,
        )

    async def await_popup(self, url=None):
        if not hasattr(self, "popup_preload_script"):
            self.popup_preload_script = await self.make_preload_script(
                """
                    window.__popups = [];
                    window.wrappedJSObject.open = function(url) {
                        window.__popups.push(url);
                    }
                """,
                "popup_detector",
            )
        return self.popup_preload_script.run(
            """(url) => new Promise(done => {
                    const to = setInterval(() => {
                        if (url === undefined && window.__popups.length) {
                            clearInterval(to);
                            return done(window.__popups[0]);
                        }
                        const found = window.__popups.find(u => u.includes(url));
                        if (found !== undefined) {
                            clearInterval(to);
                            done(found);
                        }
                    }, 1000);
               })
            """,
            url,
            await_promise=True,
        )

    async def track_listener(self, type, selector):
        if not hasattr(self, "listener_preload_script"):
            self.listener_preload_script = await self.make_preload_script(
                """
                window.__listeners = {};
                var proto = EventTarget.wrappedJSObject.prototype;
                var def = Object.getOwnPropertyDescriptor(proto, "addEventListener");
                var old = def.value;
                def.value = function(type, fn, opts) {
                    if ("matches" in this) {
                        if (!window.__listeners[type]) {
                            window.__listeners[type] = new Set();
                        }
                        window.__listeners[type].add(this);
                    }
                    return old.call(this, type, fn, opts)
                };
                Object.defineProperty(proto, "addEventListener", def);
            """,
                "listener_detector",
            )
        return Client.ListenerTracker(self.listener_preload_script, type, selector)

    @contextlib.asynccontextmanager
    async def preload_script(self, text, *args):
        script = await self.make_preload_script(text, "preload", args=args)
        yield script
        await script.stop()

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

    def await_first_element_of(
        self, finders, timeout=None, delay=0.25, condition=False, **kwargs
    ):
        t0 = time.time()
        condition = f"var elem=arguments[0]; return {condition}" if condition else False

        if timeout is None:
            timeout = 10

        found = [None for finder in finders]

        exc = None
        while time.time() < t0 + timeout:
            for i, finder in enumerate(finders):
                try:
                    result = finder.find(self, **kwargs)
                    if result and (
                        not condition
                        or self.session.execute_script(condition, [result])
                    ):
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

    @contextlib.asynccontextmanager
    async def ensure_fastclick_activates(self):
        fastclick_preload_script = await self.make_preload_script(
            """
                var _ = document.createElement("webcompat_test");
                _.style = "position:absolute;right:-1px;width:1px;height:1px";
                document.documentElement.appendChild(_);
            """,
            "fastclick_forcer",
        )
        yield
        fastclick_preload_script.stop()

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
