# META: timeout=long

import uuid

import pytest

pytestmark = pytest.mark.asyncio


async def test_session_id(new_session, add_browser_capabilities):
    bidi_session = await new_session(
        capabilities={"alwaysMatch": add_browser_capabilities({})}
    )
    assert isinstance(bidi_session.session_id, str)
    uuid.UUID(hex=bidi_session.session_id)


async def test_capability_type(new_session, add_browser_capabilities):
    bidi_session = await new_session(
        capabilities={"alwaysMatch": add_browser_capabilities({})}
    )

    default_capability_types = [
        ("acceptInsecureCerts", bool),
        ("browserName", str),
        ("browserVersion", str),
        ("platformName", str),
        ("proxy", dict),
        ("setWindowRect", bool),
    ]

    assert isinstance(bidi_session.capabilities, dict)

    for capability, type in default_capability_types:
        assert isinstance(bidi_session.capabilities[capability], type)


async def test_capability_default_value(new_session, add_browser_capabilities):
    bidi_session = await new_session(
        capabilities={"alwaysMatch": add_browser_capabilities({})}
    )
    assert isinstance(bidi_session.capabilities, dict)

    default_capability_values = [
        ("acceptInsecureCerts", False),
        ("proxy", {}),
    ]

    for capability, value in default_capability_values:
        assert bidi_session.capabilities[capability] == value


async def test_ignore_non_spec_fields_in_capabilities(
    new_session, add_browser_capabilities
):
    bidi_session = await new_session(
        capabilities={
            "alwaysMatch": add_browser_capabilities({}),
            "nonSpecCapabilities": {"acceptInsecureCerts": True},
        }
    )

    assert bidi_session.capabilities["acceptInsecureCerts"] is False


@pytest.mark.parametrize("match_type", ["alwaysMatch", "firstMatch"])
@pytest.mark.parametrize(
    "key, value",
    [
        ("pageLoadStrategy", "none"),
        ("strictFileInteractability", True),
        ("timeouts", {"script": 500}),
        ("unhandledPromptBehavior", "accept"),
    ],
)
async def test_with_webdriver_classic_capabilities(
    new_session, match_capabilities, match_type, key, value
):
    capabilities = match_capabilities(match_type, key, value)

    bidi_session = await new_session(capabilities=capabilities)

    assert isinstance(bidi_session.capabilities, dict)
    assert key not in bidi_session.capabilities
