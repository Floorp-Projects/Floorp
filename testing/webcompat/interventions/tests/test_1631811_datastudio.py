import pytest

URL = "https://ageor.dipot.com/2020/03/Covid-19-in-Greece.html"
FRAME_CSS = "iframe[src^='https://datastudio.google.com/']"
FRAME_TEXT = "Coranavirus SARS-CoV-2 (Covid-19) in Greece"


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.load_page_and_wait_for_iframe(URL, client.css(FRAME_CSS))
    assert client.execute_script("return window.indexedDB === undefined;")
    success_text = client.await_text(FRAME_TEXT)
    assert client.is_displayed(success_text)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.load_page_and_wait_for_iframe(URL, client.css(FRAME_CSS))
    assert client.execute_script("return window.indexedDB !== undefined;")
