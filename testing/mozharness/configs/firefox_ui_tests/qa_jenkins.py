# Default configuration as used by Mozmill CI (Jenkins)


config = {
    # Tests run in mozmill-ci do not use RelEng infra
    'developer_mode': True,

    # mozcrash support
    'download_minidump_stackwalk': True,
    'download_symbols': 'ondemand',
}
