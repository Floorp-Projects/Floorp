import os
import sys

from os.path import dirname, join

import pytest

sys.path.insert(0, join(dirname(__file__), "..", ".."))

from wptrunner import browsers


products = browsers.product_list
active_products = set()
all_products = products

if "CURRENT_TOX_ENV" in os.environ:
    current_tox_env_split = os.environ["CURRENT_TOX_ENV"].split("-")

    tox_env_extra_browsers = {
        "chrome": {"chrome_android"},
        "servo": {"servodriver"},
    }

    active_products = set(products) & set(current_tox_env_split)
    for product in frozenset(active_products):
        active_products |= tox_env_extra_browsers.get(product, set())

    products = []
    for product in all_products:
        if product in active_products:
            products.append(product)
        else:
            products.append(pytest.param(product, marks=pytest.mark.skip))


def pytest_generate_tests(metafunc):
    if "product" in metafunc.fixturenames:
        metafunc.parametrize("product", products)
    elif "all_product" in metafunc.fixturenames:
        metafunc.parametrize("all_product", all_products)
