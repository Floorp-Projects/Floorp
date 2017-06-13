# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import re


INTEGRATION_PROJECTS = {
    'mozilla-inbound',
    'autoland',
}

TRUNK_PROJECTS = INTEGRATION_PROJECTS | {'mozilla-central', }

RELEASE_PROJECTS = {
    'mozilla-central',
    'mozilla-aurora',
    'mozilla-beta',
    'mozilla-release',
}


def attrmatch(attributes, **kwargs):
    """Determine whether the given set of task attributes matches.  The
    conditions are given as keyword arguments, where each keyword names an
    attribute.  The keyword value can be a literal, a set, or a callable.  A
    literal must match the attribute exactly.  Given a set, the attribute value
    must be in the set.  A callable is called with the attribute value.  If an
    attribute is specified as a keyword argument but not present in the
    attributes, the result is False."""
    for kwkey, kwval in kwargs.iteritems():
        if kwkey not in attributes:
            return False
        attval = attributes[kwkey]
        if isinstance(kwval, set):
            if attval not in kwval:
                return False
        elif callable(kwval):
            if not kwval(attval):
                return False
        elif kwval != attributes[kwkey]:
            return False
    return True


def keymatch(attributes, target):
    """Determine if any keys in attributes are a match to target, then return
    a list of matching values. First exact matches will be checked. Failing
    that, regex matches and finally a default key.
    """
    # exact match
    if target in attributes:
        return [attributes[target]]

    # regular expression match
    matches = [v for k, v in attributes.iteritems() if re.match(k + '$', target)]
    if matches:
        return matches

    # default
    if 'default' in attributes:
        return [attributes['default']]

    return []


def match_run_on_projects(project, run_on_projects):
    """Determine whether the given project is included in the `run-on-projects`
    parameter, applying expansions for things like "integration" mentioned in
    the attribute documentation."""
    if 'all' in run_on_projects:
        return True
    if 'integration' in run_on_projects:
        if project in INTEGRATION_PROJECTS:
            return True
    if 'release' in run_on_projects:
        if project in RELEASE_PROJECTS:
            return True
    if 'trunk' in run_on_projects:
        if project in TRUNK_PROJECTS:
            return True

    return project in run_on_projects
