import pytest


@pytest.mark.asyncio
async def test_unique_id(bidi_session, create_user_context):
    first_context = await create_user_context()
    assert isinstance(first_context, str)

    other_context = await create_user_context()
    assert isinstance(other_context, str)

    assert first_context != other_context
