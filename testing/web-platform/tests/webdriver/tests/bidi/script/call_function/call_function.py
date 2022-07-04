import pytest

from webdriver.bidi.modules.script import ContextTarget, ScriptEvaluateResultException
from ... import recursive_compare, any_string, any_int
from .. import any_stack_trace


@pytest.mark.asyncio
async def test_exception(bidi_session, top_context):
    with pytest.raises(ScriptEvaluateResultException) as exception:
        await bidi_session.script.call_function(
            function_declaration='()=>{throw 1}',
            await_promise=False,
            target=ContextTarget(top_context["context"]))

    recursive_compare({
        'realm': any_string,
        'exceptionDetails': {
            'columnNumber': any_int,
            'exception': {
                'value': 1,
                'type': 'number'},
            'lineNumber': any_int,
            'stackTrace': any_stack_trace,
            'text': any_string}},
        exception.value.result)


@pytest.mark.asyncio
async def test_invalid_function(bidi_session, top_context):
    with pytest.raises(ScriptEvaluateResultException) as exception:
        await bidi_session.script.call_function(
            function_declaration='))) !!@@## some invalid JS script (((',
            await_promise=False,
            target=ContextTarget(top_context["context"]))
    recursive_compare({
        'realm': any_string,
        'exceptionDetails': {
            'columnNumber': any_int,
            'exception': {
                'handle': any_string,
                'type': 'error'},
            'lineNumber': any_int,
            'stackTrace': any_stack_trace,
            'text': any_string}},
        exception.value.result)


@pytest.mark.asyncio
async def test_arrow_function(bidi_session, top_context):
    result = await bidi_session.script.call_function(
        function_declaration="()=>{return 1+2;}",
        await_promise=False,
        target=ContextTarget(top_context["context"]))

    assert result == {
        "type": "number",
        "value": 3}


@pytest.mark.asyncio
async def test_arguments(bidi_session, top_context):
    result = await bidi_session.script.call_function(
        function_declaration="(...args)=>{return args}",
        arguments=[{
            "type": "string",
            "value": "ARGUMENT_STRING_VALUE"
        }, {
            "type": "number",
            "value": 42}],
        await_promise=False,
        target=ContextTarget(top_context["context"]))

    recursive_compare({
        "type": "array",
        "handle": any_string,
        "value": [{
            "type": 'string',
            "value": 'ARGUMENT_STRING_VALUE'
        }, {
            "type": 'number',
            "value": 42}]},
        result)


@pytest.mark.asyncio
async def test_default_arguments(bidi_session, top_context):
    result = await bidi_session.script.call_function(
        function_declaration="(...args)=>{return args}",
        await_promise=False,
        target=ContextTarget(top_context["context"]))

    recursive_compare({
        "type": "array",
        "handle": any_string,
        "value": []
    }, result)


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

    # Note: https://github.com/w3c/webdriver-bidi/issues/251
    recursive_compare({
        "type": 'window',
        "handle": any_string,
    }, result)


@pytest.mark.asyncio
async def test_remote_value_argument(bidi_session, top_context):
    remote_value_result = await bidi_session.script.evaluate(
        expression="({SOME_PROPERTY:'SOME_VALUE'})",
        await_promise=False,
        target=ContextTarget(top_context["context"]))

    remote_value_handle = remote_value_result["handle"]

    result = await bidi_session.script.call_function(
        function_declaration="(obj)=>{return obj.SOME_PROPERTY;}",
        arguments=[{
            "handle": remote_value_handle}],
        await_promise=False,
        target=ContextTarget(top_context["context"]))

    assert result == {
        "type": "string",
        "value": "SOME_VALUE"}


@pytest.mark.asyncio
@pytest.mark.parametrize("await_promise", [True, False])
async def test_async_arrow_await_promise(bidi_session, top_context, await_promise):
    result = await bidi_session.script.call_function(
        function_declaration="async ()=>{return 'SOME_VALUE'}",
        await_promise=await_promise,
        target=ContextTarget(top_context["context"]))

    if await_promise:
        assert result == {
            "type": "string",
            "value": "SOME_VALUE"}
    else:
        recursive_compare({
            "type": "promise",
            "handle": any_string},
            result)


@pytest.mark.asyncio
@pytest.mark.parametrize("await_promise", [True, False])
async def test_async_classic_await_promise(bidi_session, top_context, await_promise):
    result = await bidi_session.script.call_function(
        function_declaration="async function(){return 'SOME_VALUE'}",
        await_promise=await_promise,
        target=ContextTarget(top_context["context"]))

    if await_promise:
        assert result == {
            "type": "string",
            "value": "SOME_VALUE"}
    else:
        recursive_compare({
            "type": "promise",
            "handle": any_string},
            result)
