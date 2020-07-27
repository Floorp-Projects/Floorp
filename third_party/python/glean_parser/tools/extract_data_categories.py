#!/usr/bin/env python3

# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Usage:
   python extract_data_categories.py metrics.yaml

Automatically extract the data collection categories for all the metrics in a
metrics.yaml file by consulting the linked data reviews.

This script reads a metrics.yaml file, visits all of the associated data
reviews, trying to determine the associated data categories, and inserts them
(in place) to the original metrics.yaml file.

A very simple heuristic is used: to look for the question about data categories
used in all data reviews, and then find any numbers between it and the next
question. When this simple heuristic fails, comments with "!!!" are inserted in
the output as a recommendation to manually investigate and enter the data
categories.

Requirements from PyPI: BeautifulSoup4, PyYAML
"""

import dbm
import functools
import re
import sys
import time
from typing import List, Set
from urllib.request import urlopen


from bs4 import BeautifulSoup
import yaml


cache = dbm.open("bugzilla-cache.db", "c")


QUESTION = "what collection type of data do the requested measurements fall under?"


CATEGORY_MAP = {
    1: "technical",
    2: "interaction",
    3: "web_activity",
    4: "highly_sensitive",
}


def fetch_url(url: str) -> str:
    """
    Fetch a web page containing a data review, caching it to avoid
    over-fetching.
    """
    content = cache.get(url)
    if content is not None:
        return content

    print(f"Fetching {url}")
    content = urlopen(url).read()
    cache[url] = content
    time.sleep(0.5)
    return content


@functools.lru_cache(1000)
def parse_data_review(html: str) -> Set[int]:
    """
    Parse a single data review.
    """
    soup = BeautifulSoup(html, features="html.parser")
    text = soup.get_text()
    lines = iter(text.splitlines())
    for line in lines:
        if QUESTION in line.strip():
            break

    categories: Set[int] = set()
    for line in lines:
        if "?" in line:
            break
        categories.update(int(x) for x in re.findall("[0-9]+", line))

    return categories


def categories_as_strings(categories: Set[int]) -> List[str]:
    """
    From a set of numeric categories, return the strings used in a metrics.yaml
    file. This may contain strings representing errors.
    """
    if len(categories):
        return [
            CATEGORY_MAP.get(x, f"!!!UNKNOWN CATEGORY {x}")
            for x in sorted(list(categories))
        ]
    else:
        return ["!!! NO DATA CATEGORIES FOUND"]


def update_lines(
    lines: List[str],
    category_name: str,
    metric_name: str,
        data_sensitivity_values: List[str],
) -> List[str]:
    """
    Update the lines of a YAML file in place to include the data_sensitivity
    for the given metric, returning the lines of the result.
    """
    output = []
    lines_iter = iter(lines)

    for line in lines_iter:
        output.append(line)
        if line.startswith(f"{category_name}:"):
            break

    for line in lines_iter:
        output.append(line)
        if line.startswith(f"  {metric_name}:"):
            break

    for line in lines_iter:
        output.append(line)
        if line.startswith(f"    data_reviews:"):
            break

    for line in lines_iter:
        if not line.strip().startswith("- "):
            output.append("    data_sensitivity:\n")
            for data_sensitivity in data_sensitivity_values:
                output.append(f"      - {data_sensitivity}\n")
            output.append(line)
            break
        else:
            output.append(line)

    for line in lines_iter:
        output.append(line)

    return output


def parse_yaml(yamlpath: str):
    with open(yamlpath) as fd:
        content = yaml.safe_load(fd)

    with open(yamlpath) as fd:
        lines = list(fd.readlines())

    for category_name, category in content.items():
        if category_name.startswith("$") or category_name == "no_lint":
            continue
        for metric_name, metric in category.items():
            categories = set()
            for data_review_url in metric["data_reviews"]:
                html = fetch_url(data_review_url)
                categories.update(parse_data_review(html))
            lines = update_lines(
                lines, category_name, metric_name, categories_as_strings(categories)
            )

    with open(yamlpath, "w") as fd:
        for line in lines:
            fd.write(line)


if __name__ == "__main__":
    parse_yaml(sys.argv[-1])
