# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import binascii


def _file_byte_generator(filename):
    with open(filename, "rb") as f:
        contents = f.read()

        # Treat empty files the same as a file containing a lone 0;
        # a single-element array will fail cert verifcation just as an
        # empty array would.
        if not contents:
            return ['\0']

        return contents


def _create_header(array_name, cert_bytes):
    hexified = ["0x" + binascii.hexlify(byte) for byte in cert_bytes]
    substs = {'array_name': array_name, 'bytes': ', '.join(hexified)}
    return "const uint8_t %(array_name)s[] = {\n%(bytes)s\n};\n" % substs


# Create functions named the same as the data arrays that we're going to
# write to the headers, so we don't have to duplicate the names like so:
#
#   def arrayName(header, cert_filename):
#     header.write(_create_header("arrayName", cert_filename))
array_names = [
    'xpcshellRoot',
    'addonsPublicRoot',
    'addonsStageRoot',
    'privilegedPackageRoot',
]

for n in array_names:
    # Make sure the lambda captures the right string.
    globals()[n] = lambda header, cert_filename, name=n: header.write(
        _create_header(name, _file_byte_generator(cert_filename)))
