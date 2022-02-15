import pytest
from helpers import Css, is_float_cleared, find_element


URL = (
    "https://www.lcbo.com/webapp/wcs/stores/servlet/PhysicalStoreInventoryView"
    "?langId=-1&storeId=10203&catalogId=10051&productId=54875"
)


PRODUCT_INFO = Css("#content > div")
LOCATIONS = Css("#inventoryTable")


@pytest.mark.with_interventions
def test_enabled(session):
    session.get(URL)
    product_info = find_element(session, PRODUCT_INFO)
    locations = find_element(session, LOCATIONS)
    assert is_float_cleared(session, locations, product_info)


@pytest.mark.without_interventions
def test_disabled(session):
    session.get(URL)
    product_info = find_element(session, PRODUCT_INFO)
    locations = find_element(session, LOCATIONS)
    assert not is_float_cleared(session, locations, product_info)
