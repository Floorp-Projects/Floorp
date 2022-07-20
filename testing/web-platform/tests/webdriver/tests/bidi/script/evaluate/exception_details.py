import pytest

from webdriver.bidi.modules.script import ContextTarget, ScriptEvaluateResultException

from ... import any_int, any_string, recursive_compare
from .. import any_stack_trace


@pytest.mark.asyncio
async def test_invalid_script(bidi_session, top_context):
    with pytest.raises(ScriptEvaluateResultException) as exception:
        await bidi_session.script.evaluate(
            expression='))) !!@@## some invalid JS script (((',
            target=ContextTarget(top_context["context"]),
            await_promise=True)
    recursive_compare({
        'realm': any_string,
        'exceptionDetails': {
            'columnNumber': any_int,
            'exception': {'type': 'error'},
            'lineNumber': any_int,
            'stackTrace': any_stack_trace,
            'text': any_string}},
        exception.value.result)


@pytest.mark.asyncio
async def test_reject_complex_value(bidi_session, top_context):
    with pytest.raises(ScriptEvaluateResultException) as exception:
        await bidi_session.script.evaluate(
            expression="Promise.reject(new Set(['SOME_REJECTED_RESULT']))",
            target=ContextTarget(top_context["context"]),
            await_promise=True,
        )

    recursive_compare(
        {
            "realm": any_string,
            "exceptionDetails": {
                "columnNumber": any_int,
                "exception": {
                    "type": "set", "value": [
                        {"type": "string", "value": "SOME_REJECTED_RESULT"}
                    ]
                },
                "lineNumber": any_int,
                "stackTrace": any_stack_trace,
                "text": any_string,
            },
        },
        exception.value.result,
    )


@pytest.mark.asyncio
async def test_reject_error(bidi_session, top_context):
    with pytest.raises(ScriptEvaluateResultException) as exception:
        await bidi_session.script.evaluate(
            expression="Promise.reject(new Error('SOME_ERROR_MESSAGE'))",
            target=ContextTarget(top_context["context"]),
            await_promise=True,
        )

    recursive_compare(
        {
            "realm": any_string,
            "exceptionDetails": {
                "columnNumber": any_int,
                "exception": {"type": "error"},
                "lineNumber": any_int,
                "stackTrace": any_stack_trace,
                "text": any_string,
            },
        },
        exception.value.result,
    )


@pytest.mark.asyncio
async def test_throw_error(bidi_session, top_context):
    with pytest.raises(ScriptEvaluateResultException) as exception:
        await bidi_session.script.evaluate(
            expression="throw Error('SOME_ERROR_MESSAGE')",
            target=ContextTarget(top_context["context"]),
            await_promise=True)

    recursive_compare({
        'realm': any_string,
        'exceptionDetails': {
            'columnNumber': any_int,
            'exception': {'type': 'error'},
            'lineNumber': any_int,
            'stackTrace': any_stack_trace,
            'text': any_string}},
        exception.value.result)


@pytest.mark.asyncio
async def test_throw_string(bidi_session, top_context):
    with pytest.raises(ScriptEvaluateResultException) as exception:
        await bidi_session.script.evaluate(
            expression="throw 'SOME_STRING_ERROR'",
            target=ContextTarget(top_context["context"]),
            await_promise=True)

    recursive_compare({
        'realm': any_string,
        'exceptionDetails': {
            'columnNumber': any_int,
            'exception': {
                'value': 'SOME_STRING_ERROR',
                'type': 'string'},
            'lineNumber': any_int,
            'stackTrace': any_stack_trace,
            'text': any_string}},
        exception.value.result)
