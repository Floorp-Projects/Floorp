import pytest
from webdriver.bidi.error import InvalidArgumentException

from bidi.session.new.support.test_data import flat_invalid_data, invalid_extensions

pytestmark = pytest.mark.asyncio


@pytest.mark.parametrize("value", [None, True, 1, "{}", []])
async def test_params_capabilities_invalid_type(new_session, value):
    with pytest.raises(InvalidArgumentException):
        await new_session(capabilities=value)


@pytest.mark.parametrize("value", [True, 1, "{}", []])
async def test_params_always_match_invalid_type(new_session, value):
    with pytest.raises(InvalidArgumentException):
        await new_session(capabilities={"alwaysMatch": value})


@pytest.mark.parametrize("value", [True, 1, "{}", {}, []])
async def test_params_first_match_invalid_type(new_session, value):
    with pytest.raises(InvalidArgumentException):
        await new_session(capabilities={"firstMatch": value})


@pytest.mark.parametrize("value", [True, 1, "{}", None, []])
async def test_params_first_match_item_invalid_type(new_session, value):
    with pytest.raises(InvalidArgumentException):
        await new_session(capabilities={"firstMatch": [value]})


@pytest.mark.parametrize("match_type", ["alwaysMatch", "firstMatch"])
@pytest.mark.parametrize("key,value", flat_invalid_data)
async def test_invalid_value(new_session, match_capabilities, match_type, key, value):
    capabilities = match_capabilities(match_type, key, value)

    with pytest.raises(InvalidArgumentException):
        await new_session(capabilities=capabilities)


@pytest.mark.parametrize("match_type", ["alwaysMatch", "firstMatch"])
@pytest.mark.parametrize("key", invalid_extensions)
async def test_invalid_extension(new_session, match_capabilities, match_type, key):
    capabilities = match_capabilities(match_type, key, {})

    with pytest.raises(InvalidArgumentException):
        await new_session(capabilities=capabilities)


@pytest.mark.parametrize("match_type", ["alwaysMatch", "firstMatch"])
async def test_invalid_moz_extension(new_session, match_capabilities, match_type):
    capabilities = match_capabilities(match_type, "moz:someRandomString", {})

    with pytest.raises(InvalidArgumentException):
        await new_session(capabilities=capabilities)


async def test_params_with_shadowed_value(new_session):
    with pytest.raises(InvalidArgumentException):
        await new_session(
            capabilities={
                "firstMatch": [{"acceptInsecureCerts": True}],
                "alwaysMatch": {"acceptInsecureCerts": True},
            }
        )
