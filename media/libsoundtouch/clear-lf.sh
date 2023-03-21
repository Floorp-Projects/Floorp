#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Remove the Windows line ending characters from the files.
for i in src/*
do
  cat $i | tr -d '\015' > $i.lf
  mv $i.lf $i
done
