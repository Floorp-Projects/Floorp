#!/usr/bin/env bash

set -ue # its like javascript, everything is allowed unless you prevent it.

topdir=$(git rev-parse --show-toplevel)

cd $topdir/.metrics

url="https://api.github.com/repos/mozilla-spidermonkey/jsparagus/issues?labels=libFuzzer&state=all"

curl $url > count/fuzzbug.json
python fuzzbug_count_badge.py
git add .
git commit -m"Add Fuzzbug date"
python fuzzbug_date_badge.py

git add .

git commit -m"Add Fuzzbug count"
