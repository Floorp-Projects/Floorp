# -*- Mode: python; indent-tabs-mode: nil; tab-width: 40 -*-
# vim: set filetype=python:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from os.path import abspath, relpath

from mozbuild.action.zip import main as create_zip

def main(output, input_dir, *files):
    output.close()

    if files:
      # The zip builder doesn't handle the files being an absolute path.
      in_files = [relpath(file, input_dir) for file in files]

      return create_zip(['-C', input_dir, abspath(output.name)] + in_files)
    else:
      return create_zip(['-C', input_dir, abspath(output.name), '**'])
