import pytest

pytestmark = pytest.mark.asyncio


@pytest.mark.parametrize(
    "viewport",
    [
        {"width": 10000000, "height": 100},
        {"width": 100, "height": 10000000},
        {"width": 10000000, "height": 10000000},
    ],
    ids=["maximal width", "maximal height", "maximal width and height"],
)
async def test_params_viewport_max_value(bidi_session, new_tab, viewport):
    await bidi_session.browsing_context.set_viewport(
        context=new_tab["context"], viewport=viewport
    )
