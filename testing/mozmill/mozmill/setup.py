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
# The Original Code is Mozilla Corporation Code.
#
# The Initial Developer of the Original Code is
# Mikeal Rogers.
# Portions created by the Initial Developer are Copyright (C) 2008
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#  Mikeal Rogers <mikeal.rogers@gmail.com>
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

from setuptools import setup, find_packages

desc = """UI Automation tool for Mozilla applications."""
summ = """A tool for full UI automation of Mozilla applications."""

PACKAGE_NAME = "mozmill"
PACKAGE_VERSION = "1.5.1"

setup(name=PACKAGE_NAME,
      version=PACKAGE_VERSION,
      description=desc,
      long_description=summ,
      author='Mozilla, Mikeal Rogers',
      author_email='mikeal.rogers@gmail.com',
      url='http://github.com/mozautomation/mozmill',
      license='http://www.apache.org/licenses/LICENSE-2.0',
      packages=find_packages(exclude=['test']),
      include_package_data=True,
      package_data = {'': ['*.js', '*.css', '*.html', '*.txt', '*.xpi', '*.rdf', '*.xul', '*.jsm', '*.xml'],},
      entry_points="""
          [console_scripts]
          mozmill = mozmill:cli
          mozmill-thunderbird = mozmill:tbird_cli
          mozmill-restart = mozmill:restart_cli
        """,
      platforms =['Any'],
      install_requires = ['jsbridge == 2.4.1', 'mozrunner == 2.5.2'],
      classifiers=['Development Status :: 4 - Beta',
                   'Environment :: Console',
                   'Intended Audience :: Developers',
                   'License :: OSI Approved :: Apache Software License',
                   'Operating System :: OS Independent',
                   'Topic :: Software Development :: Libraries :: Python Modules',
                  ]
     )
