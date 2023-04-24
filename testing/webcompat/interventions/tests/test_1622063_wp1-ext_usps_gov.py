import pytest

URL = "https://wp1-ext.usps.gov/sap/bc/webdynpro/sap/hrrcf_a_unreg_job_search"
BLOCKED_TEXT = "This browser is not supported"
UNBLOCKED_CSS = "#sapwd_main_window_root_"


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert client.await_css(UNBLOCKED_CSS)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert client.await_text(BLOCKED_TEXT)
