# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import errno
import mozfile
import os
import shutil
import subprocess


def mkdir(dir):
    if not os.path.isdir(dir):
        try:
            os.makedirs(dir)
        except OSError as e:
            if e.errno != errno.EEXIST:
                raise


def chmod(dir):
    'Set permissions of DMG contents correctly'
    subprocess.check_call(['chmod', '-R', 'a+rX,a-st,u+w,go-w', dir])


def rsync(source, dest):
    'rsync the contents of directory source into directory dest'
    # Ensure a trailing slash so rsync copies the *contents* of source.
    if not source.endswith('/'):
        source += '/'
    subprocess.check_call(['rsync', '-a', '--copy-unsafe-links',
                           source, dest])


def set_folder_icon(dir):
    'Set HFS attributes of dir to use a custom icon'
    subprocess.check_call(['SetFile', '-a', 'C', dir])


def create_dmg_from_staged(stagedir, output_dmg, tmpdir, volume_name):
    'Given a prepared directory stagedir, produce a DMG at output_dmg.'
    hybrid = os.path.join(tmpdir, 'hybrid.dmg')
    subprocess.check_call(['hdiutil', 'makehybrid', '-hfs',
                           '-hfs-volume-name', volume_name,
                           '-hfs-openfolder', stagedir,
                           '-ov', stagedir,
                           '-o', hybrid])
    subprocess.check_call(['hdiutil', 'convert', '-format', 'UDBZ',
                           '-imagekey', 'bzip2-level=9',
                           '-ov', hybrid, '-o', output_dmg])


def create_dmg(source_directory, output_dmg, volume_name, extra_files):
    '''
    Create a DMG disk image at the path output_dmg from source_directory.

    Use volume_name as the disk image volume name, and
    use extra_files as a list of tuples of (filename, relative path) to copy
    into the disk image.
    '''
    with mozfile.TemporaryDirectory() as tmpdir:
        stagedir = os.path.join(tmpdir, 'stage')
        os.mkdir(stagedir)
        # Copy the app bundle over using rsync
        rsync(source_directory, stagedir)
        # Copy extra files
        for source, target in extra_files:
            full_target = os.path.join(stagedir, target)
            mkdir(os.path.dirname(full_target))
            shutil.copyfile(source, full_target)
        # Make a symlink to /Applications. The symlink name is a space
        # so we don't have to localize it. The Applications folder icon
        # will be shown in Finder, which should be clear enough for users.
        os.symlink('/Applications', os.path.join(stagedir, ' '))
        # Set the folder attributes to use a custom icon
        set_folder_icon(stagedir)
        chmod(stagedir)
        create_dmg_from_staged(stagedir, output_dmg, tmpdir, volume_name)
