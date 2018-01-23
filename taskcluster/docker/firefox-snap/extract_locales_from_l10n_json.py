#!/usr/bin/env python3

import sys
import json

l10n_changesets_json_path = sys.argv[1]
with open(l10n_changesets_json_path) as f:
    locales = json.load(f).keys()
linux_locales = [l for l in locales if l != 'ja-JP-mac']

print('\n'.join(sorted(linux_locales)))
