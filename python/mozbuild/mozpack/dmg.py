# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import buildconfig
import errno
import mozfile
import os
import platform
import shutil
import subprocess

from mozbuild.util import ensureParentDir

is_linux = platform.system() == 'Linux'


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
    # Ensure a trailing slash on directories so rsync copies the *contents* of source.
    if not source.endswith('/') and os.path.isdir(source):
        source += '/'
    subprocess.check_call(['rsync', '-a', '--copy-unsafe-links',
                           source, dest])


def set_folder_icon(dir, tmpdir):
    'Set HFS attributes of dir to use a custom icon'
    if not is_linux:
        subprocess.check_call(['SetFile', '-a', 'C', dir])
    else:
        hfs = os.path.join(tmpdir, 'staged.hfs')
        subprocess.check_call([
            buildconfig.substs['HFS_TOOL'],  hfs, 'attr', '/', 'C'])


def generate_hfs_file(stagedir, tmpdir, volume_name):
    '''
    When cross compiling, we zero fill an hfs file, that we will turn into
    a DMG. To do so we test the size of the staged dir, and add some slight
    padding to that.
    '''
    if is_linux:
        hfs = os.path.join(tmpdir, 'staged.hfs')
        output = subprocess.check_output(['du', '-s', stagedir])
        size = (int(output.split()[0]) / 1000)  # Get in MB
        size = int(size * 1.02)  # Bump the used size slightly larger.
        # Setup a proper file sized out with zero's
        subprocess.check_call(['dd', 'if=/dev/zero', 'of={}'.format(hfs),
                               'bs=1M', 'count={}'.format(size)])
        subprocess.check_call([
            buildconfig.substs['MKFSHFS'], '-v', volume_name,
            hfs])


def create_app_symlink(stagedir, tmpdir):
    '''
    Make a symlink to /Applications. The symlink name is a space
    so we don't have to localize it. The Applications folder icon
    will be shown in Finder, which should be clear enough for users.
    '''
    if is_linux:
        hfs = os.path.join(tmpdir, 'staged.hfs')
        subprocess.check_call([
            buildconfig.substs['HFS_TOOL'], hfs, 'symlink',
            '/ ', '/Applications'])
    else:
        os.symlink('/Applications', os.path.join(stagedir, ' '))


def create_dmg_from_staged(stagedir, output_dmg, tmpdir, volume_name):
    'Given a prepared directory stagedir, produce a DMG at output_dmg.'
    if not is_linux:
        # Running on OS X
        hybrid = os.path.join(tmpdir, 'hybrid.dmg')
        subprocess.check_call(['hdiutil', 'makehybrid', '-hfs',
                               '-hfs-volume-name', volume_name,
                               '-hfs-openfolder', stagedir,
                               '-ov', stagedir,
                               '-o', hybrid])
        subprocess.check_call(['hdiutil', 'convert', '-format', 'UDBZ',
                               '-imagekey', 'bzip2-level=9',
                               '-ov', hybrid, '-o', output_dmg])
    else:
        # The dmg tool doesn't create the destination directories, and silently
        # returns success if the parent directory doesn't exist.
        ensureParentDir(output_dmg)

        hfs = os.path.join(tmpdir, 'staged.hfs')
        subprocess.check_call([
            buildconfig.substs['HFS_TOOL'], hfs, 'addall', stagedir])
        subprocess.check_call([
            buildconfig.substs['DMG_TOOL'],
            'build',
            hfs,
            output_dmg
        ],
                              # dmg is seriously chatty
                              stdout=open(os.devnull, 'wb'))


def check_tools(*tools):
    '''
    Check that each tool named in tools exists in SUBSTS and is executable.
    '''
    for tool in tools:
        path = buildconfig.substs[tool]
        if not path:
            raise Exception('Required tool "%s" not found' % tool)
        if not os.path.isfile(path):
            raise Exception('Required tool "%s" not found at path "%s"' % (tool, path))
        if not os.access(path, os.X_OK):
            raise Exception('Required tool "%s" at path "%s" is not executable' % (tool, path))


def create_dmg(source_directory, output_dmg, volume_name, extra_files):
    '''
    Create a DMG disk image at the path output_dmg from source_directory.

    Use volume_name as the disk image volume name, and
    use extra_files as a list of tuples of (filename, relative path) to copy
    into the disk image.
    '''
    if platform.system() not in ('Darwin', 'Linux'):
        raise Exception("Don't know how to build a DMG on '%s'" % platform.system())

    if is_linux:
        check_tools('DMG_TOOL', 'MKFSHFS', 'HFS_TOOL')
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
        generate_hfs_file(stagedir, tmpdir, volume_name)
        create_app_symlink(stagedir, tmpdir)
        # Set the folder attributes to use a custom icon
        set_folder_icon(stagedir, tmpdir)
        chmod(stagedir)
        create_dmg_from_staged(stagedir, output_dmg, tmpdir, volume_name)


def extract_dmg_contents(dmgfile, destdir):
    import buildconfig
    if is_linux:
        with mozfile.TemporaryDirectory() as tmpdir:
            hfs_file = os.path.join(tmpdir, 'firefox.hfs')
            subprocess.check_call([
                    buildconfig.substs['DMG_TOOL'],
                    'extract',
                    dmgfile,
                    hfs_file
                ],
                # dmg is seriously chatty
                stdout=open(os.devnull, 'wb'))
            subprocess.check_call([
                buildconfig.substs['HFS_TOOL'], hfs_file, 'extractall', '/', destdir])
    else:
        unpack_diskimage = os.path.join(buildconfig.topsrcdir, 'build', 'package',
                                        'mac_osx', 'unpack-diskimage')
        unpack_mountpoint = os.path.join(
            '/tmp', '{}-unpack'.format(buildconfig.substs['MOZ_APP_NAME']))
        subprocess.check_call([unpack_diskimage, dmgfile, unpack_mountpoint,
                               destdir])


def extract_dmg(dmgfile, output, dsstore=None, icon=None, background=None):
    if platform.system() not in ('Darwin', 'Linux'):
        raise Exception("Don't know how to extract a DMG on '%s'" % platform.system())

    if is_linux:
        check_tools('DMG_TOOL', 'MKFSHFS', 'HFS_TOOL')

    with mozfile.TemporaryDirectory() as tmpdir:
        extract_dmg_contents(dmgfile, tmpdir)
        if os.path.islink(os.path.join(tmpdir, ' ')):
            # Rsync will fail on the presence of this symlink
            os.remove(os.path.join(tmpdir, ' '))
        rsync(tmpdir, output)

        if dsstore:
            mkdir(os.path.dirname(dsstore))
            rsync(os.path.join(tmpdir, '.DS_Store'), dsstore)
        if background:
            mkdir(os.path.dirname(background))
            rsync(os.path.join(tmpdir, '.background', os.path.basename(background)),
                  background)
        if icon:
            mkdir(os.path.dirname(icon))
            rsync(os.path.join(tmpdir, '.VolumeIcon.icns'), icon)
