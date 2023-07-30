import pytest

# the problem is intermittent, so we have to try loading the page a few times
# and check whether a SecurityError was thrown by any of them, and that text
# in the iframe didn't load in order to be relatively sure that it failed.
# The problem seems even more intermittent on Android, so not running there.


URL = "https://ageor.dipot.com/2020/03/Covid-19-in-Greece.html"
FRAME_FINAL_URL = "lookerstudio.google.com/embed/reporting/"
FRAME_TEXT = "Coranavirus SARS-CoV-2 (Covid-19) in Greece"
FAIL_CONSOLE_MSG = "SecurityError"


async def checkFrameFailsToLoadForAnyAttempt(client, timeout=10):
    for i in range(5):
        promise = await client.promise_frame_listener(FRAME_FINAL_URL, timeout=timeout)
        await client.navigate(URL, wait="none")
        frame = await promise
        if frame is None:
            return True
        if not await frame.await_text(FRAME_TEXT, is_displayed=True, timeout=5):
            return True
        if await client.is_console_message(FAIL_CONSOLE_MSG):
            return True
    return False


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert not await checkFrameFailsToLoadForAnyAttempt(client, timeout=None)


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert await checkFrameFailsToLoadForAnyAttempt(client)
