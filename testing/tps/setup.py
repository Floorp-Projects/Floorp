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
# The Original Code is TPS.
#    
# The Initial Developer of the Original Code is
# Mozilla foundation
# Portions created by the Initial Developer are Copyright (C) 2011
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Jonathan Griffin <jgriffin@mozilla.com>
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

import sys
from setuptools import setup, find_packages

version = '0.2.40'

deps = ['pulsebuildmonitor >= 0.2', 'MozillaPulse == .4', 
        'mozinfo == 0.3.1', 'mozprofile == 0.1t',
        'mozprocess == 0.1a', 'mozrunner == 3.0a', 'mozregression == 0.3',
        'mozautolog >= 0.2.1']

# we only support python 2.6+ right now
assert sys.version_info[0] == 2
assert sys.version_info[1] >= 6

setup(name='tps',
      version=version,
      description='run automated multi-profile sync tests',
      long_description="""\
""",
      classifiers=[], # Get strings from http://pypi.python.org/pypi?%3Aaction=list_classifiers
      keywords='',
      author='Jonathan Griffin',
      author_email='jgriffin@mozilla.com',
      url='http://hg.mozilla.org/services/services-central',
      license='MPL',
      dependency_links = [
         "http://people.mozilla.org/~jgriffin/packages/"
      ],
      packages=find_packages(exclude=['ez_setup', 'examples', 'tests']),
      include_package_data=True,
      zip_safe=False,
      install_requires=deps,
      entry_points="""
      # -*- Entry points: -*-
      [console_scripts]
      runtps = tps.cli:main
      """,
      data_files=[
        ('tps', ['config/config.json.in']),
      ],
      )
