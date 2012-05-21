#!/bin/sh
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


# Edit the doc Makefile to just echo the commands
sed -e 's/latex2man/echo/' -e 's/pdflatex/echo/' -i.old "$1"
