# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

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
    If extdir is not specified, extracts to os.path.dirname(path)
    If delete is set to True, deletes the bundle at path
    Returns the list of top level files that were extracted
    """
    assert not os.path.isfile(extdir), "extdir cannot be a file"
    if extdir is None:
        extdir = os.path.dirname(path)
    elif not os.path.isdir(extdir):
        os.makedirs(extdir)
    if zipfile.is_zipfile(path):
        bundle = zipfile.ZipFile(path)
        namelist = bundle.namelist()
        if hasattr(bundle, 'extractall'):
            bundle.extractall(path=extdir)
        # zipfile.extractall doesn't exist in Python 2.5
        else:
            for name in namelist:
                filename = os.path.realpath(os.path.join(extdir, name))
                if name.endswith("/"):
                    os.makedirs(filename)
                else:
                    path = os.path.dirname(filename)
                    if not os.path.isdir(path):
                        os.makedirs(path)
                    dest = open(filename, "wb")
                    dest.write(bundle.read(name))
                    dest.close()
    elif tarfile.is_tarfile(path):
        bundle = tarfile.open(path)
        namelist = bundle.getnames()
        if hasattr(bundle, 'extractall'):
            bundle.extractall(path=extdir)
        # tarfile.extractall doesn't exist in Python 2.4
        else:
            for name in namelist:
                bundle.extract(name, path=extdir)
    else:
        return
    bundle.close()
    if delete:
        os.remove(path)
    # namelist returns paths with forward slashes even in windows
    top_level_files = [os.path.join(extdir, name) for name in namelist
                             if len(name.rstrip('/').split('/')) == 1]
    # namelist doesn't include folders, append these to the list
    for name in namelist:
        root = os.path.join(extdir, name[:name.find('/')])
        if root not in top_level_files:
            top_level_files.append(root)
    return top_level_files
