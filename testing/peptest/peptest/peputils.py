# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/ #
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Peptest.
#
# The Initial Developer of the Original Code is
#   The Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2011
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Andrew Halberstadt <halbersa@gmail.com>
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

import urllib2
import urlparse
import os
import zipfile
import tarfile

def download(url, savepath=''):
    """
    Save the file located at 'url' into 'savepath'
    If savepath is None, use the last part of the url path.
    Returns the path of the saved file.
    """
    data = urllib2.urlopen(url)
    if savepath == '' or os.path.isdir(savepath):
        parsed = urlparse.urlsplit(url)
        filename = parsed.path[parsed.path.rfind('/')+1:]
        savepath = os.path.join(savepath, filename)
    savedir = os.path.dirname(savepath)
    if savedir != '' and not os.path.exists(savedir):
        os.makedirs(savedir)
    outfile = open(savepath, 'wb')
    outfile.write(data.read())
    outfile.close()
    return os.path.realpath(savepath)

def isURL(path):
    """Return True if path looks like a URL."""
    if path is not None:
        return urlparse.urlparse(path).scheme != ''
    return False

def extract(path, extdir=None, delete=False):
    """
    Takes in a tar or zip file and extracts it to extdir
    If extdir is not specified, extracts to path
    If delete is set to True, deletes the bundle at path
    Returns the list of top level files that were extracted
    """
    if zipfile.is_zipfile(path):
        bundle = zipfile.ZipFile(path)
        namelist = bundle.namelist()
    elif tarfile.is_tarfile(path):
        bundle = tarfile.open(path)
        namelist = bundle.getnames()
    else:
        return
    if extdir is None:
        extdir = os.path.dirname(path)
    elif not os.path.exists(extdir):
        os.makedirs(extdir)
    bundle.extractall(path=extdir)
    bundle.close()
    if delete:
        os.remove(path)
    return [os.path.join(extdir, name) for name in namelist
                if len(name.rstrip(os.sep).split(os.sep)) == 1]
