import pytest

URL = "https://www.axisbank.com/retail/cards/credit-card"
TARGET_CSS = ".loanBox"


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    loan_element = client.await_css(TARGET_CSS)
    assert client.is_displayed(loan_element)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert not client.find_css(TARGET_CSS)
