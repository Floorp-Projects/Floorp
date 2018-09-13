from __future__ import absolute_import

# ftp -> update platform map
update_platform_map = {
    "android": ["Android_arm-eabi-gcc3"],
    "android-api-11": ["Android_arm-eabi-gcc3"],
    "android-api-15": ["Android_arm-eabi-gcc3"],
    "android-api-16": ["Android_arm-eabi-gcc3"],
    "android-x86": ["Android_x86-gcc3"],
    "android-x86_64": ["Android_x86-64-gcc3"],
    "android-aarch64": ["Android_aarch64-gcc3"],
    "linux-i686": ["Linux_x86-gcc3"],
    "linux-x86_64": ["Linux_x86_64-gcc3"],
    "mac": ["Darwin_x86_64-gcc3-u-i386-x86_64",  # The main platofrm
            "Darwin_x86-gcc3-u-i386-x86_64",
            # We don"t ship builds with these build targets, but some users
            # modify their builds in a way that has them report like these.
            # See bug 1071576 for details.
            "Darwin_x86-gcc3", "Darwin_x86_64-gcc3"],
    "win32": ["WINNT_x86-msvc", "WINNT_x86-msvc-x86", "WINNT_x86-msvc-x64"],
    "win64": ["WINNT_x86_64-msvc", "WINNT_x86_64-msvc-x64"],
}

# ftp -> shipped locales map
sl_platform_map = {
    "linux-i686": "linux",
    "linux-x86_64": "linux",
    "mac": "osx",
    "win32": "win32",
    "win64": "win64",
}

# ftp -> info file platform map
info_file_platform_map = {
    "linux-i686": "linux",
    "linux-x86_64": "linux64",
    "mac": "macosx64",
    "win32": "win32",
    "win64": "win64",
}

def ftp2updatePlatforms(platform):
    return update_platform_map.get(platform, platform)

def ftp2shippedLocales(platform):
    return sl_platform_map.get(platform, platform)

def shippedLocales2ftp(platform):
    matches = []
    try:
        [matches.append(
            k) for k, v in sl_platform_map.iteritems() if v == platform][0]
        return matches
    except IndexError:
        return [platform]

def ftp2infoFile(platform):
    return info_file_platform_map.get(platform, platform)
