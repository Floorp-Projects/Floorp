#!/usr/bin/env python

"""list mozbase package dependencies"""

import os
import sys
import setup_development

def main(args=sys.argv[1:]):

    # get package information
    info = {}
    dependencies = {}
    for package in setup_development.mozbase_packages:
        directory = os.path.join(setup_development.here, package)
        info[directory] = setup_development.info(directory)
        name, _dependencies = setup_development.get_dependencies(directory)
        assert name == info[directory]['Name']
        dependencies[name] = _dependencies

    # print package version information
    for value in info.values():
        print '%s %s : %s' % (value['Name'], value['Version'],
                              ', '.join(dependencies[value['Name']]))

if __name__ == '__main__':
    main()
