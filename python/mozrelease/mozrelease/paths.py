# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from urllib.parse import urlunsplit

product_ftp_map = {
    "fennec": "mobile",
}


def product2ftp(product):
    return product_ftp_map.get(product, product)


def getCandidatesDir(product, version, buildNumber, protocol=None, server=None):
    if protocol:
        assert server is not None, "server is required with protocol"

    product = product2ftp(product)
    directory = "/{}/candidates/{}-candidates/build{}".format(
        product,
        str(version),
        str(buildNumber),
    )

    if protocol:
        return urlunsplit((protocol, server, directory, None, None))
    else:
        return directory


def getReleasesDir(product, version=None, protocol=None, server=None):
    if protocol:
        assert server is not None, "server is required with protocol"

    directory = "/{}/releases".format(product)
    if version:
        directory = "{}/{}".format(directory, version)

    if protocol:
        return urlunsplit((protocol, server, directory, None, None))
    else:
        return directory


def getReleaseInstallerPath(productName, brandName, version, platform, locale="en-US"):
    if productName not in ("fennec",):
        if platform.startswith("linux"):
            return "/".join(
                [
                    p.strip("/")
                    for p in [
                        platform,
                        locale,
                        "%s-%s.tar.bz2" % (productName, version),
                    ]
                ]
            )
        elif "mac" in platform:
            return "/".join(
                [
                    p.strip("/")
                    for p in [platform, locale, "%s %s.dmg" % (brandName, version)]
                ]
            )
        elif platform.startswith("win"):
            return "/".join(
                [
                    p.strip("/")
                    for p in [
                        platform,
                        locale,
                        "%s Setup %s.exe" % (brandName, version),
                    ]
                ]
            )
        else:
            raise "Unsupported platform"
    else:
        if platform.startswith("android"):
            filename = "%s-%s.%s.android-arm.apk" % (productName, version, locale)
            return "/".join([p.strip("/") for p in [platform, locale, filename]])
        else:
            raise "Unsupported platform"
