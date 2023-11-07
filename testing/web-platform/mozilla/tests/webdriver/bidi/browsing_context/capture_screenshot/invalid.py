import pytest
from webdriver.bidi import error

pytestmark = pytest.mark.asyncio


@pytest.mark.parametrize("value", [False, 42, [], {}])
async def test_params_origin_invalid_type(bidi_session, top_context, value):
    with pytest.raises(error.InvalidArgumentException):
        await bidi_session.browsing_context.capture_screenshot(
            context=top_context["context"], origin=value
        )


async def test_params_origin_invalid_value(bidi_session, top_context):
    with pytest.raises(error.InvalidArgumentException):
        await bidi_session.browsing_context.capture_screenshot(
            context=top_context["context"], origin="test"
        )
