import pytest
from tests.bidi import recursive_compare
from webdriver.bidi.modules.script import ContextTarget


@pytest.mark.asyncio
@pytest.mark.parametrize("await_promise", [True, False])
async def test_window_context_top_level(bidi_session, top_context, await_promise):
    function_declaration = "() => window"
    if await_promise:
        function_declaration = "async" + function_declaration

    result = await bidi_session.script.call_function(
        function_declaration=function_declaration,
        await_promise=await_promise,
        target=ContextTarget(top_context["context"]),
    )

    recursive_compare(
        {"type": "window", "value": {"context": top_context["context"]}}, result
    )


@pytest.mark.asyncio
@pytest.mark.parametrize("await_promise", [True, False])
async def test_window_context_iframe_window(
    bidi_session, top_context, test_page_same_origin_frame, await_promise
):
    await bidi_session.browsing_context.navigate(
        context=top_context["context"],
        url=test_page_same_origin_frame,
        wait="complete",
    )

    all_contexts = await bidi_session.browsing_context.get_tree()
    iframe_context = all_contexts[0]["children"][0]

    function_declaration = "() => window"
    if await_promise:
        function_declaration = "async" + function_declaration

    result = await bidi_session.script.call_function(
        function_declaration=function_declaration,
        await_promise=await_promise,
        target=ContextTarget(iframe_context["context"]),
    )

    recursive_compare(
        {"type": "window", "value": {"context": iframe_context["context"]}}, result
    )


@pytest.mark.asyncio
@pytest.mark.parametrize("await_promise", [True, False])
async def test_window_context_iframe_content_window(
    bidi_session, top_context, test_page_same_origin_frame, await_promise
):
    await bidi_session.browsing_context.navigate(
        context=top_context["context"],
        url=test_page_same_origin_frame,
        wait="complete",
    )

    all_contexts = await bidi_session.browsing_context.get_tree()
    iframe_context = all_contexts[0]["children"][0]

    # This is equivalent to `document.getElementsByTagName("iframe")[0].contentWindow`
    function_declaration = "() => window.frames[0]"
    if await_promise:
        function_declaration = "async" + function_declaration

    result = await bidi_session.script.call_function(
        function_declaration=function_declaration,
        await_promise=await_promise,
        target=ContextTarget(top_context["context"]),
    )

    recursive_compare(
        {"type": "window", "value": {"context": iframe_context["context"]}}, result
    )


@pytest.mark.asyncio
@pytest.mark.parametrize("await_promise", [True, False])
@pytest.mark.parametrize("domain", ["", "alt"], ids=["same_origin", "cross_origin"])
async def test_window_context_same_id_after_navigation(
    bidi_session, top_context, inline, await_promise, domain
):
    initial_page = inline("<div>initial</div>")
    await bidi_session.browsing_context.navigate(
        context=top_context["context"], url=initial_page, wait="complete"
    )

    url = inline(f"{domain}", domain=domain)

    function_declaration = "() => window"
    if await_promise:
        function_declaration = "async" + function_declaration

    result = await bidi_session.script.call_function(
        function_declaration=function_declaration,
        await_promise=await_promise,
        target=ContextTarget(top_context["context"]),
    )

    original_context_id = result["value"]["context"]

    await bidi_session.browsing_context.navigate(
        context=top_context["context"], url=url, wait="complete"
    )

    result = await bidi_session.script.call_function(
        function_declaration=function_declaration,
        await_promise=await_promise,
        target=ContextTarget(top_context["context"]),
    )

    navigated_context_id = result["value"]["context"]

    assert navigated_context_id == original_context_id
