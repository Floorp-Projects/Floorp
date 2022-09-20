import pytest

from webdriver.bidi.modules.script import ContextTarget

from ... import recursive_compare


@pytest.mark.asyncio
async def test_this(bidi_session, top_context):
    result = await bidi_session.script.call_function(
        function_declaration="function(){return this.some_property}",
        this={
            "type": "object",
            "value": [[
                "some_property",
                {
                    "type": "number",
                    "value": 42
                }]]},
        await_promise=False,
        target=ContextTarget(top_context["context"]))

    assert result == {
        'type': 'number',
        'value': 42}


@pytest.mark.asyncio
async def test_default_this(bidi_session, top_context):
    result = await bidi_session.script.call_function(
        function_declaration="function(){return this}",
        await_promise=False,
        target=ContextTarget(top_context["context"]))

    recursive_compare({
        "type": 'window',
    }, result)
