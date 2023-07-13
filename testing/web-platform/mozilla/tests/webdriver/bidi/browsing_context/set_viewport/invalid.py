import pytest
from webdriver.bidi import error

pytestmark = pytest.mark.asyncio


@pytest.mark.parametrize(
    "viewport",
    [
        {"width": 10000001, "height": 100},
        {"width": 100, "height": 10000001},
    ],
    ids=["width exceeded", "height exceeded"],
)
async def test_params_viewport_invalid_value(bidi_session, new_tab, viewport):
    with pytest.raises(error.UnsupportedOperationException):
        await bidi_session.browsing_context.set_viewport(
            context=new_tab["context"], viewport=viewport
        )
