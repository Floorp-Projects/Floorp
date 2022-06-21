import pytest
from webdriver.bidi.modules.script import ContextTarget, ScriptEvaluateResultException

from ... import any_int, any_string, recursive_compare
from .. import any_stack_trace


@pytest.mark.asyncio
async def test_eval(bidi_session, top_context):
    result = await bidi_session.script.evaluate(
        expression="1 + 2",
        target=ContextTarget(top_context["context"]))

    assert result == {
        "type": "number",
        "value": 3}


@pytest.mark.asyncio
async def test_params_expression_invalid_script(bidi_session, top_context):
    with pytest.raises(ScriptEvaluateResultException) as exception:
        await bidi_session.script.evaluate(
            expression='))) !!@@## some invalid JS script (((',
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
async def test_exception(bidi_session, top_context):
    with pytest.raises(ScriptEvaluateResultException) as exception:
        await bidi_session.script.evaluate(
            expression="throw Error('SOME_ERROR_MESSAGE')",
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
async def test_interact_with_dom(bidi_session, top_context):
    result = await bidi_session.script.evaluate(
        expression="'window.location.href: ' + window.location.href",
        target=ContextTarget(top_context["context"]))

    assert result == {
        "type": "string",
        "value": "window.location.href: about:blank"}


@pytest.mark.asyncio
async def test_resolved_promise_with_await_promise_false(bidi_session,
                                                         top_context):
    result = await bidi_session.script.evaluate(
        expression="Promise.resolve('SOME_RESOLVED_RESULT')",
        target=ContextTarget(top_context["context"]),
        await_promise=False)

    recursive_compare({
        "type": "promise",
        "handle": any_string},
        result)


@pytest.mark.asyncio
async def test_resolved_promise_with_await_promise_true(bidi_session,
                                                        top_context):
    result = await bidi_session.script.evaluate(
        expression="Promise.resolve('SOME_RESOLVED_RESULT')",
        target=ContextTarget(top_context["context"]),
        await_promise=True)

    assert result == {
        "type": "string",
        "value": "SOME_RESOLVED_RESULT"}


@pytest.mark.asyncio
async def test_resolved_promise_with_await_promise_omitted(bidi_session,
                                                           top_context):
    result = await bidi_session.script.evaluate(
        expression="Promise.resolve('SOME_RESOLVED_RESULT')",
        target=ContextTarget(top_context["context"]))

    assert result == {
        "type": "string",
        "value": "SOME_RESOLVED_RESULT"}


@pytest.mark.asyncio
async def test_rejected_promise_with_await_promise_false(bidi_session,
                                                         top_context):
    result = await bidi_session.script.evaluate(
        expression="Promise.reject('SOME_REJECTED_RESULT')",
        target=ContextTarget(top_context["context"]),
        await_promise=False)

    recursive_compare({
        "type": "promise",
        "handle": any_string},
        result)


@pytest.mark.asyncio
async def test_rejected_promise_with_await_promise_true(bidi_session,
                                                        top_context):
    with pytest.raises(ScriptEvaluateResultException) as exception:
        await bidi_session.script.evaluate(
            expression="Promise.reject('SOME_REJECTED_RESULT')",
            target=ContextTarget(top_context["context"]),
            await_promise=True)

    recursive_compare({
        'realm': any_string,
        'exceptionDetails': {
            'columnNumber': any_int,
            'exception': {'type': 'string',
                          'value': 'SOME_REJECTED_RESULT'},
            'lineNumber': any_int,
            'stackTrace': any_stack_trace,
            'text': any_string}},
        exception.value.result)


@pytest.mark.asyncio
async def test_rejected_promise_with_await_promise_omitted(bidi_session,
                                                           top_context):
    with pytest.raises(ScriptEvaluateResultException) as exception:
        await bidi_session.script.evaluate(
            expression="Promise.reject('SOME_REJECTED_RESULT')",
            target=ContextTarget(top_context["context"]))

    recursive_compare({
        'realm': any_string,
        'exceptionDetails': {
            'columnNumber': any_int,
            'exception': {'type': 'string',
                          'value': 'SOME_REJECTED_RESULT'},
            'lineNumber': any_int,
            'stackTrace': any_stack_trace,
            'text': any_string}},
        exception.value.result)
