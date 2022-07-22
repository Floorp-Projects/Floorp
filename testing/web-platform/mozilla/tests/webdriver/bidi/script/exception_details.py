import pytest

from webdriver.bidi.modules.script import ContextTarget, ScriptEvaluateResultException


@pytest.mark.asyncio
@pytest.mark.parametrize("await_promise", [True, False])
@pytest.mark.parametrize(
    "expression",
    [
        "null",
        "{ toString: 'not a function' }",
        "{ toString: () => {{ throw 'toString not allowed'; }} }",
        "{ toString: () => true }",
    ],
)
@pytest.mark.asyncio
async def test_call_function_without_to_string_interface(
    bidi_session, top_context, await_promise, expression
):
    function_declaration = "()=>{throw { toString: 'not a function' } }"
    if await_promise:
        function_declaration = "async" + function_declaration

    with pytest.raises(ScriptEvaluateResultException) as exception:
        await bidi_session.script.call_function(
            function_declaration=function_declaration,
            await_promise=await_promise,
            target=ContextTarget(top_context["context"]),
        )

    assert "exceptionDetails" in exception.value.result
    exceptionDetails = exception.value.result["exceptionDetails"]

    assert "text" in exceptionDetails
    assert isinstance(exceptionDetails["text"], str)


@pytest.mark.asyncio
@pytest.mark.parametrize("await_promise", [True, False])
@pytest.mark.parametrize(
    "expression",
    [
        "null",
        "{ toString: 'not a function' }",
        "{ toString: () => {{ throw 'toString not allowed'; }} }",
        "{ toString: () => true }",
    ],
)
@pytest.mark.asyncio
async def test_evaluate_without_to_string_interface(
    bidi_session, top_context, await_promise, expression
):
    if await_promise:
        expression = f"Promise.reject({expression})"
    else:
        expression = f"throw {expression}"

    with pytest.raises(ScriptEvaluateResultException) as exception:
        await bidi_session.script.evaluate(
            expression=expression,
            await_promise=await_promise,
            target=ContextTarget(top_context["context"]),
        )

    assert "exceptionDetails" in exception.value.result
    exceptionDetails = exception.value.result["exceptionDetails"]

    assert "text" in exceptionDetails
    assert isinstance(exceptionDetails["text"], str)
