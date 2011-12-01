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
# The Original Code is mozinstall.
#
# The Initial Developer of the Original Code is
#  The Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2011
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#  Clint Talbert <ctalbert@mozilla.com>
#  Andrew Halberstadt <halbersa@gmail.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

import os
from setuptools import setup

try:
    here = os.path.dirname(os.path.abspath(__file__))
    description = file(os.path.join(here, 'README.md')).read()
except IOError:
    description = None

version = '0.3'

deps = ['mozinfo']

setup(name='mozInstall',
      version=version,
      description="This is a utility package for installing Mozilla applications on various platforms.",
      long_description=description,
      classifiers=['Environment :: Console',
                   'Intended Audience :: Developers',
                   'License :: OSI Approved :: Mozilla Public License 1.1 (MPL 1.1)',
                   'Natural Language :: English',
                   'Operating System :: OS Independent',
                   'Programming Language :: Python',
                   'Topic :: Software Development :: Libraries :: Python Modules',
                  ], # Get strings from http://pypi.python.org/pypi?%3Aaction=list_classifiers
      keywords='mozilla',
      author='mdas',
      author_email='mdas@mozilla.com',
      url='https://github.com/mozilla/mozbase',
      license='MPL',
      py_modules=['mozinstall'],
      packages=[],
      include_package_data=True,
      zip_safe=False,
      install_requires=deps,
      entry_points="""
      # -*- Entry points: -*-
      [console_scripts]
      mozinstall = mozinstall:cli
      """,
      )
