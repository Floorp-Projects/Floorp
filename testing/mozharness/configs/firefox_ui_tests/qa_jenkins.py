# Default configuration as used by Mozmill CI (Jenkins)


config = {
    # Tests run in mozmill-ci do not use RelEng infra
    'developer_mode': True,

    # PIP
    'find_links': ['http://pypi.pub.build.mozilla.org/pub'],
    'pip_index': False,

    # mozcrash support
    'download_minidump_stackwalk': True,
    'download_symbols': 'ondemand',
    'download_tooltool': True,
}
