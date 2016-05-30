config = {
    'balrog_servers': [
        {
            'balrog_api_root': 'https://aus4-admin.mozilla.org/api',
            'ignore_failures': False,
            'url_replacements': [
                ('http://archive.mozilla.org/pub', 'http://download.cdn.mozilla.net/pub'),
            ],
            'balrog_usernames': {
                'b2g': 'b2gbld',
                'firefox': 'ffxbld',
                'thunderbird': 'tbirdbld',
                'mobile': 'ffxbld',
                'Fennec': 'ffxbld',
                'graphene': 'ffxbld',
                'horizon': 'ffxbld',
            }
        },
        # Bug 1275911 - get releng automation posting to cloudops balrog stage instance
        {
            'balrog_api_root': 'https://balrog-admin.stage.mozaws.net/api',
            'ignore_failures': True,
            'balrog_usernames': {
                'b2g': 'stage-b2gbld',
                'firefox': 'stage-ffxbld',
                'thunderbird': 'stage-tbirdbld',
                'mobile': 'stage-ffxbld',
                'Fennec': 'stage-ffxbld',
                'graphene': 'stage-ffxbld',
                'horizon': 'stage-ffxbld',
            }
        }
    ]
}
