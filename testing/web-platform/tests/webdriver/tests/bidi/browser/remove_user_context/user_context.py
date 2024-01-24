import pytest

import webdriver.bidi.error as error


@pytest.mark.asyncio
async def test_remove_context(bidi_session, create_user_context):
    user_context = await create_user_context()
    await bidi_session.browser.remove_user_context(user_context=user_context)
