import time
from datetime import datetime, timedelta

import pytest

today = datetime.now()
three_months = today + timedelta(days=90)
plus_week = three_months + timedelta(days=7)
formatted_start = three_months.strftime("%Y-%m-%d")
formatted_end = plus_week.strftime("%Y-%m-%d")

URL = f"https://www.vio.com/Hotel/Search?hotelId=17293575&checkIn={formatted_start}&checkOut={formatted_end}&rooms=2&homeSearch=1&userSearch=1&layout=map"
MAP_CSS = ".mapboxgl-map"
MIN_HEIGHT = 100


def get_elem_height(client):
    elem = client.await_css(MAP_CSS, is_displayed=True)
    assert elem
    return client.execute_script(
        """
        return arguments[0].getBoundingClientRect().height;
    """,
        elem,
    )


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    time.sleep(3)
    await client.navigate(URL)
    assert get_elem_height(client) > MIN_HEIGHT


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    time.sleep(3)
    await client.navigate(URL)
    assert get_elem_height(client) < MIN_HEIGHT
