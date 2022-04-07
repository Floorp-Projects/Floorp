import pytest
import webdriver.bidi.error as error

pytestmark = pytest.mark.asyncio


@pytest.mark.parametrize("value", [None, False, 42, {}, []])
async def test_params_context_invalid_type(bidi_session, inline, value):
    with pytest.raises(error.InvalidArgumentException):
        await bidi_session.browsing_context.navigate(
            context=value, url=inline("<p>foo")
        )


@pytest.mark.parametrize("value", ["", "somestring"])
async def test_params_context_invalid_value(bidi_session, inline, value):
    with pytest.raises(error.NoSuchFrameException):
        await bidi_session.browsing_context.navigate(
            context=value, url=inline("<p>foo")
        )


@pytest.mark.parametrize("value", [None, False, 42, {}, []])
async def test_params_url_invalid_type(bidi_session, top_context, value):
    with pytest.raises(error.InvalidArgumentException):
        await bidi_session.browsing_context.navigate(
            context=top_context["context"], url=value
        )


@pytest.mark.parametrize("value", ["http://:invalid", "http://#invalid"])
async def test_params_url_invalid_value(bidi_session, top_context, value):
    with pytest.raises(error.InvalidArgumentException):
        await bidi_session.browsing_context.navigate(
            context=top_context["context"], url=value
        )


@pytest.mark.parametrize("value", [False, 42, {}, []])
async def test_params_wait_invalid_type(bidi_session, inline, top_context, value):
    with pytest.raises(error.InvalidArgumentException):
        await bidi_session.browsing_context.navigate(
            context=top_context["context"], url=inline("<p>bar"), wait=value
        )


@pytest.mark.parametrize("value", ["", "somestring"])
async def test_params_wait_invalid_value(bidi_session, inline, top_context, value):
    with pytest.raises(error.InvalidArgumentException):
        await bidi_session.browsing_context.navigate(
            context=top_context["context"], url=inline("<p>bar"), wait=value
        )
