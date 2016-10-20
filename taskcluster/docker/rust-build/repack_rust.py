#!/bin/env python
'''
This script downloads and repacks official rust language builds
with the necessary tool and target support for the Firefox
build environment.
'''

import os.path
import requests
import subprocess
import toml


def fetch_file(url):
    '''Download a file from the given url if it's not already present.'''
    filename = os.path.basename(url)
    if os.path.exists(filename):
        return
    r = requests.get(url, stream=True)
    r.raise_for_status()
    with open(filename, 'wb') as fd:
        for chunk in r.iter_content(4096):
            fd.write(chunk)


def fetch(url):
    '''Download and verify a package url.'''
    base = os.path.basename(url)
    print('Fetching %s...' % base)
    fetch_file(url + '.asc')
    fetch_file(url)
    fetch_file(url + '.sha256')
    fetch_file(url + '.asc.sha256')
    print('Verifying %s...' % base)
    subprocess.check_call(['shasum', '-c', base + '.sha256'])
    subprocess.check_call(['shasum', '-c', base + '.asc.sha256'])
    subprocess.check_call(['gpg', '--verify', base + '.asc', base])
    if False:
        subprocess.check_call([
            'keybase', 'pgp', 'verify', '-d', base + '.asc', '  -i', base,
        ])


def install(filename, target):
    '''Run a package's installer script against the given target directory.'''
    print(' Unpacking %s...' % filename)
    subprocess.check_call(['tar', 'xf', filename])
    basename = filename.split('.tar')[0]
    print(' Installing %s...' % basename)
    install_cmd = [os.path.join(basename, 'install.sh')]
    install_cmd += ['--prefix=' + os.path.abspath(target)]
    install_cmd += ['--disable-ldconfig']
    subprocess.check_call(install_cmd)
    print(' Cleaning %s...' % basename)
    subprocess.check_call(['rm', '-rf', basename])


def package(manifest, pkg, target):
    '''Pull out the package dict for a particular package and target
    from the given manifest.'''
    version = manifest['pkg'][pkg]['version']
    info = manifest['pkg'][pkg]['target'][target]
    return (version, info)


def fetch_package(manifest, pkg, host):
    version, info = package(manifest, pkg, host)
    print('%s %s\n  %s\n  %s' % (pkg, version, info['url'], info['hash']))
    if not info['available']:
        print('%s marked unavailable for %s' % (pkg, host))
        raise AssertionError
    fetch(info['url'])
    return info


def fetch_std(manifest, targets):
    stds = []
    for target in targets:
        info = fetch_package(manifest, 'rust-std', target)
        stds.append(info)
    return stds


def tar_for_host(host):
    if 'linux' in host:
        tar_options = 'cJf'
        tar_ext = '.tar.xz'
    else:
        tar_options = 'cjf'
        tar_ext = '.tar.bz2'
    return tar_options, tar_ext


def repack(host, targets, channel='stable', suffix=''):
    print("Repacking rust for %s..." % host)
    url = 'https://static.rust-lang.org/dist/channel-rust-' + channel + '.toml'
    req = requests.get(url)
    req.raise_for_status()
    manifest = toml.loads(req.content)
    if manifest['manifest-version'] != '2':
        print('ERROR: unrecognized manifest version %s.' % manifest[
            'manifest-version'])
        return
    print('Using manifest for rust %s as of %s.' % (channel, manifest['date']))
    print('Fetching packages...')
    rustc = fetch_package(manifest, 'rustc', host)
    cargo = fetch_package(manifest, 'cargo', host)
    stds = fetch_std(manifest, targets)
    print('Installing packages...')
    tar_basename = 'rustc-' + host
    if suffix:
        tar_basename += '-' + suffix
    tar_basename += '-repack'
    install_dir = 'rustc'
    subprocess.check_call(['rm', '-rf', install_dir])
    install(os.path.basename(rustc['url']), install_dir)
    install(os.path.basename(cargo['url']), install_dir)
    for std in stds:
        install(os.path.basename(std['url']), install_dir)
        pass
    print('Tarring %s...' % tar_basename)
    tar_options, tar_ext = tar_for_host(host)
    subprocess.check_call(
        ['tar', tar_options, tar_basename + tar_ext, install_dir])
    subprocess.check_call(['rm', '-rf', install_dir])


def repack_cargo(host, channel='nightly'):
    print("Repacking cargo for %s..." % host)
    # Cargo doesn't seem to have a .toml manifest.
    base_url = 'https://static.rust-lang.org/cargo-dist/'
    req = requests.get(os.path.join(base_url, 'channel-cargo-' + channel))
    req.raise_for_status()
    file = ''
    for line in req.iter_lines():
        if line.find(host) != -1:
            file = line.strip()
    if not file:
        print('No manifest entry for %s!' % host)
        return
    manifest = {
        'date': req.headers['Last-Modified'],
        'pkg': {
            'cargo': {
                'version': channel,
                'target': {
                    host: {
                        'url': os.path.join(base_url, file),
                        'hash': None,
                        'available': True,
                    },
                },
            },
        },
    }
    print('Using manifest for cargo %s.' % channel)
    print('Fetching packages...')
    cargo = fetch_package(manifest, 'cargo', host)
    print('Installing packages...')
    install_dir = 'cargo'
    subprocess.check_call(['rm', '-rf', install_dir])
    install(os.path.basename(cargo['url']), install_dir)
    tar_basename = 'cargo-%s-repack' % host
    print('Tarring %s...' % tar_basename)
    tar_options, tar_ext = tar_for_host(host)
    subprocess.check_call(
        ['tar', tar_options, tar_basename + tar_ext, install_dir])
    subprocess.check_call(['rm', '-rf', install_dir])


# rust platform triples
android = "armv7-linux-androideabi"
android_x86 = "i686-linux-android"
linux64 = "x86_64-unknown-linux-gnu"
linux32 = "i686-unknown-linux-gnu"
mac64 = "x86_64-apple-darwin"
mac32 = "i686-apple-darwin"
win64 = "x86_64-pc-windows-msvc"
win32 = "i686-pc-windows-msvc"

if __name__ == '__main__':
    repack(mac64, [mac64, mac32])
    repack(win32, [win32])
    repack(win64, [win64])
    repack(linux64, [linux64, linux32])
    repack(linux64, [linux64, mac64, mac32], suffix='mac-cross')
    repack(linux64, [linux64, android, android_x86], suffix='android-cross')
