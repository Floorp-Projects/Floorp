config = {
    'src_mozconfig': 'browser/config/mozconfigs/macosx64/devedition',
    'force_clobber': True,
    'stage_platform': 'macosx64-devedition',
    'stage_product': 'devedition',

    # Enable sendchanges for bug 1372412
    'enable_talos_sendchange': False,
    'enable_unittest_sendchange': False,
}
