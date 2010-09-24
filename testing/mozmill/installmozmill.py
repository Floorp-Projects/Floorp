#!/usr/bin/env python

# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is
# Mozilla.org.
# Portions created by the Initial Developer are Copyright (C) 2010
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#     Jeff Hammel <jhammel@mozilla.com>     (Original author)
#
# Alternatively, the contents of this file may be used under the terms of
# either of the GNU General Public License Version 2 or later (the "GPL"),
# or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

"""
install mozmill and its dependencies
"""

import os
import sys
from subprocess import call

def main(args=None):
  """command line front-end function"""

  args = args or sys.argv[1:]

  # The data is kept in the same directory as the script
  source=os.path.abspath(os.path.dirname(__file__))

  # directory to install to
  if not len(args):
    destination = source
  elif len(args) == 1:
    destination = os.path.abspath(args[0])
  else:
    print "Usage: %s [destination]" % sys.argv[0]
    sys.exit(1)

  os.chdir(source)

  # check for existence of necessary files
  required = ('PACKAGES', 'virtualenv')
  for f in required:
    if not os.path.exists(f):
      print "File not found: " + f
      sys.exit(1)

  # packages to install in dependency order
  PACKAGES=file('PACKAGES').read().split()
  assert PACKAGES
  
  # create the virtualenv and install packages
  env = os.environ.copy()
  env.pop('PYTHONHOME', None)
  call([sys.executable, 'virtualenv/virtualenv.py', destination], env=env)
  if sys.platform.startswith('win'):
    pip = os.path.join(destination, 'Scripts', 'pip.exe')
  else:
    pip = os.path.join(destination, 'bin', 'pip')
  call([pip, 'install'] + PACKAGES, env=env)

if __name__ == '__main__':
  main()
