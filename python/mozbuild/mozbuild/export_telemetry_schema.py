#!/usr/bin/env python

from __future__ import print_function, unicode_literals

"""
This script converts the build system telemetry schema from voluptuous format to json-schema.
You should run it with `mach python`.
"""

import argparse
from mozbuild.base import MozbuildObject


def ensure_luscious(virtualenv_manager):
    virtualenv_manager.ensure()
    virtualenv_manager.activate()
    try:
        import luscious  # noqa: F401
    except (ImportError, AttributeError):
        # Ted's fork of luscious, on the `fixes` branch.
        virtualenv_manager.install_pip_package(
            'git+git://github.com/luser/luscious.git@cfc9b7a402e750d008c0255cd23ecbb3c401c053')


def main():
    config = MozbuildObject.from_environment()
    ensure_luscious(config.virtualenv_manager)

    import mozbuild.telemetry
    import luscious
    import json

    parser = argparse.ArgumentParser(
        description='Output build system telemetry schema in json-schema format')
    parser.add_argument('output', help='JSON output destination')
    args = parser.parse_args()

    schema = luscious.get_jsonschema(mozbuild.telemetry.schema)
    with open(args.output, 'wb') as f:
        json.dump(schema, f, indent=2, separators=(',', ': '), sort_keys=True)


if __name__ == '__main__':
    main()
