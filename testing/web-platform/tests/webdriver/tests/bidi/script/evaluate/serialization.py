import pytest
from webdriver.bidi.modules.script import ContextTarget


@pytest.mark.asyncio
@pytest.mark.parametrize(
    "expression, expected",
    [
        ("undefined", {"type": "undefined"}),
        ("null", {"type": "null"}),
        ("'foobar'", {"type": "string", "value": "foobar"}),
        ("'2'", {"type": "string", "value": "2"}),
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


@pytest.mark.asyncio
@pytest.mark.parametrize(
    "expression, expected",
    [
        (
            "new RegExp(/foo/g)",
            {
                "type": "regexp",
                "value": {
                    "pattern": "foo",
                    "flags": "g",
                },
            },
        ),
        (
            "new Date(1654004849000)",
            {
                "type": "date",
                "value": "2022-05-31T13:47:29.000Z",
            },
        ),
        (
            "[1, 'foo', true, new RegExp(/foo/g), [1]]",
            {
                "type": "array",
                "value": [
                    {"type": "number", "value": 1},
                    {"type": "string", "value": "foo"},
                    {"type": "boolean", "value": True},
                    {
                        "type": "regexp",
                        "value": {
                            "pattern": "foo",
                            "flags": "g",
                        },
                    },
                    {"type": "array"},
                ],
            },
        ),
        (
            "new Map([[1, 2], ['foo', 'bar'], [true, false], ['baz', [1]]])",
            {
                "type": "map",
                "value": [
                    [
                        {"type": "number", "value": 1},
                        {"type": "number", "value": 2},
                    ],
                    ["foo", {"type": "string", "value": "bar"}],
                    [
                        {"type": "boolean", "value": True},
                        {"type": "boolean", "value": False},
                    ],
                    ["baz", {"type": "array"}],
                ],
            },
        ),
        (
            "new Set([1, 'foo', true, [1], new Map([[1,2]])])",
            {
                "type": "set",
                "value": [
                    {"type": "number", "value": 1},
                    {"type": "string", "value": "foo"},
                    {"type": "boolean", "value": True},
                    {"type": "array"},
                    {"type": "map"},
                ],
            },
        ),
        (
            "({'foo': {'bar': 'baz'}, 'qux': 'quux'})",
            {
                "type": "object",
                "value": [
                    ["foo", {"type": "object"}],
                    ["qux", {"type": "string", "value": "quux"}],
                ],
            },
        ),
    ],
    ids=["regexp", "date", "array", "map", "set", "object"],
)
async def test_remote_values(bidi_session, top_context, expression, expected):
    result = await bidi_session.script.evaluate(
        expression=expression,
        target=ContextTarget(top_context["context"]),
        await_promise=True,
    )

    assert result == expected
