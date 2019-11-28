# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Write out scalar information for C++.  The scalars are defined
# in a file provided as a command-line argument.

from __future__ import print_function
from collections import OrderedDict
from mozparsers.shared_telemetry_utils import (
    StringTable,
    static_assert,
    ParserError
)
from mozparsers import parse_scalars

import json
import sys
import buildconfig

# The banner/text at the top of the generated file.
banner = """/* This file is auto-generated, only for internal use in TelemetryScalar.h,
   see gen_scalar_data.py. */
"""

file_header = """\
#ifndef mozilla_TelemetryScalarData_h
#define mozilla_TelemetryScalarData_h
#include "core/ScalarInfo.h"
namespace {
"""

file_footer = """\
} // namespace
#endif // mozilla_TelemetryScalarData_h
"""


def write_scalar_info(scalar, output, name_index, expiration_index, store_index, store_count,
                      key_count, key_index):
    """Writes a scalar entry to the output file.

    :param scalar: a ScalarType instance describing the scalar.
    :param output: the output stream.
    :param name_index: the index of the scalar name in the strings table.
    :param expiration_index: the index of the expiration version in the strings table.
    """
    if scalar.record_on_os(buildconfig.substs["OS_TARGET"]):
        print("  {{ {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {} }},"
              .format(scalar.nsITelemetry_kind,
                      name_index,
                      expiration_index,
                      scalar.dataset,
                      " | ".join(scalar.record_in_processes_enum),
                      "true" if scalar.keyed else "false",
                      key_count,
                      key_index,
                      " | ".join(scalar.products_enum),
                      store_count,
                      store_index),
              file=output)


def write_scalar_tables(scalars, output):
    """Writes the scalar and strings tables to an header file.

    :param scalars: a list of ScalarType instances describing the scalars.
    :param output: the output stream.
    """
    string_table = StringTable()

    store_table = []
    total_store_count = 0

    keys_table = []
    total_key_count = 0

    print("const ScalarInfo gScalars[] = {", file=output)
    for s in scalars:
        # We add both the scalar label and the expiration string to the strings
        # table.
        name_index = string_table.stringIndex(s.label)
        exp_index = string_table.stringIndex(s.expires)

        stores = s.record_into_store
        store_index = 0
        if stores == ["main"]:
            # if count == 1 && offset == UINT16_MAX -> only main store
            store_index = 'UINT16_MAX'
        else:
            store_index = total_store_count
            store_table.append((s.label, string_table.stringIndexes(stores)))
            total_store_count += len(stores)

        keys = s.keys
        key_index = 0
        if len(keys) > 0:
            key_index = total_key_count
            keys_table.append((s.label, string_table.stringIndexes(keys)))
            total_key_count += len(keys)

        # Write the scalar info entry.
        write_scalar_info(s, output, name_index, exp_index, store_index, len(stores),
                          len(keys), key_index)
    print("};", file=output)

    string_table_name = "gScalarsStringTable"
    string_table.writeDefinition(output, string_table_name)
    static_assert(output, "sizeof(%s) <= UINT32_MAX" % string_table_name,
                  "index overflow")

    print("\nconstexpr uint32_t gScalarKeysTable[] = {", file=output)
    for name, indexes in keys_table:
        print("/* %s */ %s," % (name, ", ".join(map(str, indexes))), file=output)
    print("};", file=output)

    store_table_name = "gScalarStoresTable"
    print("\n#if defined(_MSC_VER) && !defined(__clang__)", file=output)
    print("const uint32_t {}[] = {{".format(store_table_name), file=output)
    print("#else", file=output)
    print("constexpr uint32_t {}[] = {{".format(store_table_name), file=output)
    print("#endif", file=output)
    for name, indexes in store_table:
        print("/* %s */ %s," % (name, ", ".join(map(str, indexes))), file=output)
    print("};", file=output)
    static_assert(output, "sizeof(%s) <= UINT16_MAX" % store_table_name,
                  "index overflow")


def parse_scalar_definitions(filenames):
    scalars = []
    for filename in filenames:
        try:
            batch = parse_scalars.load_scalars(filename)
            scalars.extend(batch)
        except ParserError as ex:
            print("\nError processing %s:\n%s\n" % (filename, str(ex)), file=sys.stderr)
            sys.exit(1)
    return scalars


def generate_JSON_definitions(output, *filenames):
    """ Write the scalar definitions to a JSON file.

    :param output: the file to write the content to.
    :param filenames: a list of filenames provided by the build system.
           We only support a single file.
    """
    scalars = parse_scalar_definitions(filenames)

    scalar_definitions = OrderedDict()
    for scalar in scalars:
        category = scalar.category

        if category not in scalar_definitions:
            scalar_definitions[category] = OrderedDict()

        scalar_definitions[category][scalar.name] = OrderedDict({
            'kind': scalar.nsITelemetry_kind,
            'keyed': scalar.keyed,
            'keys': scalar.keys,
            'record_on_release': True if scalar.dataset_short == 'opt-out' else False,
            # We don't expire dynamic-builtin scalars: they're only meant for
            # use in local developer builds anyway. They will expire when rebuilding.
            'expired': False,
            'stores': scalar.record_into_store,
            'expires': scalar.expires,
            'products': scalar.products,
        })

    json.dump(scalar_definitions, output)


def main(output, *filenames):
    # Load the scalars first.
    scalars = parse_scalar_definitions(filenames)

    # Write the scalar data file.
    print(banner, file=output)
    print(file_header, file=output)
    write_scalar_tables(scalars, output)
    print(file_footer, file=output)


if __name__ == '__main__':
    main(sys.stdout, *sys.argv[1:])
