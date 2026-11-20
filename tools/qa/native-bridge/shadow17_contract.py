"""Inactive two-slot structural contract; never grants authentication or success."""
from hashlib import sha256
from re import fullmatch

ROLES = ('State12', 'History5')
IDS = ('repository_id', 'run_id', 'attempt', 'job_id')
COMMITS = ('source', 'runtime')
MATERIALS = (*COMMITS, 'binary', 'overlay', 'support', 'manifest', 'profile')
PINNED_COMMITS = dict(zip(COMMITS, ('7b89a91d51534eeec9d591740f808dd49bb6133b',
                                 '6ffed8ad4dcfadeb85bb158c82395cefcc3e2c06')))
SLOTS = {'role', 'artifact_id', 'artifact_digest'}
LIMIT = 32 * 1024 * 1024

# Exact reviewed source-case IDs and separate role policy digests (not package hashes).
SEALED_SCOPE = {'History5': ('10b8bbf63f69136e2c8217edc5ccb2590f64d46bdd7cf9b9c993d49a112a1921',
              ('browser/components/sessionstore/test/browser_navigation_api_restore.js',
               'browser/components/sessionstore/test/browser_dynamic_frames.js',
               'browser/components/sessionstore/test/browser_frametree.js',
               'browser/components/sessionstore/test/browser_restore_srcdoc.js',
               'browser/components/sessionstore/test/browser_purge_shistory.js')),
 'State12': ('88ab5fff052f25f84e069ce70b58fe7a1ab22c786203a5393d9ebfc0a3339735',
             ('browser/components/tabbrowser/test/browser/tabs/browser_blank_tab_label.js',
              'browser/components/tabbrowser/test/browser/tabs/browser_close_tab_by_dblclick.js',
              'browser/components/tabbrowser/test/browser/tabs/browser_multiselect_tabs_using_Ctrl.js',
              'browser/components/tabbrowser/test/browser/tabs/browser_multiselect_tabs_using_Shift_and_Ctrl.js',
              'browser/components/tabbrowser/test/browser/tabs/browser_openURI_background.js',
              'browser/components/tabbrowser/test/browser/tabs/browser_tabswitch_updatecommands.js',
              'browser/components/tabbrowser/test/browser/tabs/browser_tab_label_character_cap.js',
              'browser/components/tabbrowser/test/browser/tabs/browser_long_data_url_label_truncation.js',
              'browser/components/tabbrowser/test/browser/tabs/browser_tab_aria_posinset_setsize.js',
              'browser/components/tabbrowser/test/browser/tabs/browser_positional_attributes.js',
              'browser/components/tabbrowser/test/browser/tabs/browser_tab_aria_level.js',
              'browser/components/tabbrowser/test/browser/tabs/browser_tab_label_reader_mode.js'))}



def require(condition, message):
    if not condition:
        raise ValueError(message)


def shape(value, keys):
    require(type(value) is dict and set(value) == set(keys), 'invalid exact shape')


def numeric(value):
    return type(value) is int and 0 < value < 10**20


def digest(value):
    return type(value) is str and fullmatch(r'[0-9a-f]{64}', value) is not None


def same_producer(value, expected, msg):
    shape(value, IDS)
    require(all(numeric(v) for v in value.values()) and value == expected, msg)


def check(*, producer, direct, policy, observations, archives, validate_slot):
    """Inputs must come independently from platform, policy, and exterior observer.

    validate_slot is trusted unchanged structural code, not candidate input.
    """
    shape(producer, IDS)
    require(all(numeric(v) for v in producer.values()), 'invalid producer identity')
    shape(direct, {'producer', 'slots'})
    same_producer(direct['producer'], producer, 'direct dependency producer/attempt mismatch')
    direct_slots = direct['slots']
    for obj in (direct_slots, policy, observations, archives):
        shape(obj, ROLES)
    ids, digests, profiles, manifests, paths = set(), set(), set(), set(), set()
    ready = []
    shared = None
    for role in ROLES:
        slot, expected, seen, archive = (
            direct_slots[role], policy[role], observations[role], archives[role]
        )
        shape(slot, SLOTS)
        shape(expected, {'role', 'tests', *MATERIALS})
        shape(seen, {'producer', 'process_exit', *SLOTS, *MATERIALS})
        shape(seen['producer'], IDS)
        same_producer(seen['producer'], producer, 'observer producer/attempt mismatch')
        require(slot['role'] == expected['role'] == seen['role'] == role,
                'fixed cohort role mismatch')
        aid, adigest = slot['artifact_id'], slot['artifact_digest']
        require(numeric(aid) and digest(adigest), 'invalid artifact identity')
        require(numeric(seen['artifact_id']) and seen['artifact_id'] == aid and
                seen['artifact_digest'] == adigest, 'observer artifact mismatch')
        require(aid not in ids and adigest not in digests, 'duplicate artifact ID or digest')
        ids.add(aid); digests.add(adigest)
        require(type(archive) is bytes and 0 < len(archive) <= LIMIT and
                sha256(archive).hexdigest() == adigest, 'archive bytes mismatch')
        for key in MATERIALS:
            value = expected[key]
            valid = value == PINNED_COMMITS[key] if key in COMMITS else digest(value)
            require(type(value) is str and valid and seen[key] == value,
                    'material identity mismatch: ' + key)
        pinned_manifest, pinned_tests = SEALED_SCOPE[role]
        require(expected['manifest'] == pinned_manifest and expected['tests'] == pinned_tests,
                'does not match exact role policy')
        materials = tuple(expected[key] for key in ('source', 'runtime', 'binary', 'overlay', 'support'))
        require(shared is None or shared == materials, 'cross-cohort material mismatch')
        shared = materials
        require(expected['profile'] not in profiles and expected['manifest'] not in manifests,
                'cohorts must have distinct owned profiles/manifests')
        profiles.add(expected['profile']); manifests.add(expected['manifest'])
        tests = expected['tests']
        require(type(tests) is tuple and tests and all(type(t) is str and t for t in tests)
                and len(set(tests)) == len(tests) and not (paths & set(tests)),
                'missing, duplicate or overlapping test inventory')
        paths.update(tests)
        code = seen['process_exit']
        require(type(code) is int and code == 0, 'cohort process did not exit zero')
        ready.append((archive, tests, code))
    for archive, tests, code in ready:
        result = validate_slot(archive, list(tests), code)
        require(type(result) is dict and result.get('state') == 'SOURCE_CONTRACT_ONLY'
                and result.get('raw_consistent') is True
                and result.get('authenticated') is False
                and result.get('native_pass') is False
                and type(result.get('required_tests_observed')) is int
                and result['required_tests_observed'] == len(tests),
                'cohort structural validation not established')
    return {'state': 'SOURCE_CONTRACT_ONLY', 'cohorts_checked': tuple(ROLES),
            'authenticated': False, 'native_pass': False, 'qualified': False}
