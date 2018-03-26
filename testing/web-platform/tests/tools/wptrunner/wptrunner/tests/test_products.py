import os
import sys

from os.path import join, dirname

import pytest

sys.path.insert(0, join(dirname(__file__), "..", ".."))

from wptrunner import products


def test_load_active_product(product):
    """test we can successfully load the product of the current testenv"""
    products.load_product({}, product)
    # test passes if it doesn't throw


def test_load_all_products(all_product):
    """test every product either loads or throws ImportError"""
    try:
        products.load_product({}, all_product)
    except ImportError:
        pass
