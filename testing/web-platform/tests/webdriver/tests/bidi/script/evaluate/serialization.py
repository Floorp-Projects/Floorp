import pytest
from webdriver.bidi.modules.script import ContextTarget


@pytest.mark.asyncio
@pytest.mark.parametrize(
    "expression, expected",
    [
        ("undefined", {"type": "undefined"}),
        ("null", {"type": "null"}),
        ("'foobar'", {"type": "string", "value": "foobar"}),
        ("Number.NaN", {"type": "number", "value": "NaN"}),
        ("-0", {"type": "number", "value": "-0"}),
        ("Infinity", {"type": "number", "value": "+Infinity"}),
        ("-Infinity", {"type": "number", "value": "-Infinity"}),
        ("3", {"type": "number", "value": 3}),
        ("1.4", {"type": "number", "value": 1.4}),
        ("true", {"type": "boolean", "value": True}),
        ("false", {"type": "boolean", "value": False}),
        ("42n", {"type": "bigint", "value": "42"}),
    ],
)
async def test_primitive_values(bidi_session, top_context, expression, expected):
    result = await bidi_session.script.evaluate(
        expression=expression,
        target=ContextTarget(top_context["context"]),
        await_promise=True,
    )

    assert result == expected
