# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

import logging
import os
import re
import requests
from taskcluster.notify import Notify
from operator import itemgetter

from mozilla_version.gecko import GeckoVersion

BUGLIST_PREFIX = 'Bugs since previous changeset: '
BACKOUT_REGEX = re.compile(r'back(\s?)out|backed out|backing out', re.IGNORECASE)
BACKOUT_PREFIX = 'Backouts since previous changeset: '
BUGZILLA_BUGLIST_TEMPLATE = 'https://bugzilla.mozilla.org/buglist.cgi?bug_id={bugs}'
BUG_NUMBER_REGEX = re.compile(r'bug \d+', re.IGNORECASE)
CHANGELOG_TO_FROM_STRING = '{product}_{version}_RELEASE'
CHANGESET_URL_TEMPLATE = (
    '{repo}/{logtype}'
    '?fromchange={from_version}&tochange={to_version}&full=1'
)
FULL_CHANGESET_PREFIX = 'Full Mercurial changelog: '
LIST_DESCRIPTION_TEMPLATE = 'Comparing Mercurial tag {from_version} to {to_version}:'
MAX_BUGS_IN_BUGLIST = 250
MERCURIAL_TAGS_URL_TEMPLATE = '{repo}/json-tags'
NO_BUGS = ''  # Return this when bug list can't be created
URL_SHORTENER_TEMPLATE = 'https://bugzilla.mozilla.org/rest/bitly/shorten?url={url}'

log = logging.getLogger(__name__)


def create_bugs_url(product, current_version, current_revision, repo=None):
    """
    Creates list of bugs and backout bugs for release-drivers email

    :param release: dict -> containing information about release, from Ship-It
    :return: str -> description of compared releases, with Bugzilla links
        containing all bugs in changeset
    """
    try:
        # Extract the important data, ignore if beta1 release
        if current_version.beta_number == 1:
            # If the version is beta 1, don't make any links
            return NO_BUGS

        if repo is None:
            repo = get_repo_by_version(current_version)
        # Get the tag version, for display purposes
        current_version_tag = tag_version(product, current_version)

        # Get all Hg tags for this branch, determine the previous version
        tag_url = MERCURIAL_TAGS_URL_TEMPLATE.format(repo=repo)
        mercurial_tags_json = requests.get(tag_url).json()
        previous_version_tag = get_previous_tag_version(
            product, current_version, current_version_tag, mercurial_tags_json)

        # Get the changeset between these versions, parse for all unique bugs and backout bugs
        resp = requests.get(CHANGESET_URL_TEMPLATE.format(repo=repo,
                                                          from_version=previous_version_tag,
                                                          to_version=current_revision,
                                                          logtype='json-pushes'))
        changeset_data = resp.json()
        unique_bugs, unique_backout_bugs = get_bugs_in_changeset(changeset_data)

        # Return a descriptive string with links if any relevant bugs are found
        if unique_bugs or unique_backout_bugs:
            description_string = LIST_DESCRIPTION_TEMPLATE.format(
                from_version=previous_version_tag,
                to_version=current_version_tag)

            changeset_html = CHANGESET_URL_TEMPLATE.format(repo=repo,
                                                           from_version=previous_version_tag,
                                                           to_version=current_revision,
                                                           logtype='pushloghtml')

            return format_return_value(
                description_string, unique_bugs, unique_backout_bugs, changeset_html)
        else:
            return NO_BUGS

    except Exception as err:
        log.info(err)
        return NO_BUGS


def get_bugs_in_changeset(changeset_data):
    unique_bugs, unique_backout_bugs = set(), set()
    for data in changeset_data.values():
        for changeset in data['changesets']:
            if is_excluded_change(changeset):
                continue

            changeset_desc = changeset['desc']
            bug_re = BUG_NUMBER_REGEX.search(changeset_desc)

            if bug_re:
                bug_number = bug_re.group().split(' ')[1]

                if is_backout_bug(changeset_desc):
                    unique_backout_bugs.add(bug_number)
                else:
                    unique_bugs.add(bug_number)

    return unique_bugs, unique_backout_bugs


def is_excluded_change(changeset):
    excluded_change_keywords = [
        'a=test-only',
        'a=release',
    ]
    return any(keyword in changeset['desc'] for keyword in excluded_change_keywords)


def is_backout_bug(changeset_description):
    return bool(BACKOUT_REGEX.search(changeset_description))


def create_short_url_with_prefix(buglist, backout_buglist):
    # Create link if there are bugs, else empty string
    urls = []
    for set_of_bugs, prefix in [(buglist, BUGLIST_PREFIX), (backout_buglist, BACKOUT_PREFIX)]:
        if set_of_bugs and len(set_of_bugs) < MAX_BUGS_IN_BUGLIST:
            try:
                long_bugzilla_link = BUGZILLA_BUGLIST_TEMPLATE.format(bugs='%2C'.join(set_of_bugs))
                response = requests.get(URL_SHORTENER_TEMPLATE.format(url=long_bugzilla_link))
                url = response.json()['url']
                url = prefix + url + '\n'

            except (KeyError, ValueError):
                # If the Bugzilla link fails despite limiting the number of
                # bugs, don't make the url and continue
                url = ''
        else:
            url = ''

        urls.append(url)

    return urls[0], urls[1]


def tag_version(product, version):
    underscore_version = str(version).replace('.', '_')
    return CHANGELOG_TO_FROM_STRING.format(product=product.upper(), version=underscore_version)


def parse_tag_version(tag):
    dot_version = '.'.join(tag.split('_')[1:-1])
    return GeckoVersion.parse(dot_version)


def get_previous_tag_version(
    product, current_version, current_version_tag, mercurial_tags_json,
):
    """
    Gets the previous hg version tag for the product and branch, given the current version tag
    """

    def _invalid_tag_filter(tag):
        """Filters by product and removes incorrect major version + base, end releases"""
        prod_major_version_re = r'^{product}_{major_version}'.format(
            product=product.upper(), major_version=current_version.major_number)

        return 'BASE' not in tag and \
               'END' not in tag and \
               'RELEASE' in tag and \
               re.match(prod_major_version_re, tag)

    # Get rid of irrelevant tags, sort by date and extract the tag string
    tags = {
        (parse_tag_version(item['tag']), item['tag'])
        for item in mercurial_tags_json['tags']
        if _invalid_tag_filter(item['tag'])
    }
    # Add the current version to the list
    tags.add((current_version, current_version_tag))
    tags = sorted(tags, key=lambda tag: tag[0])

    # Find where the current version is and go back one to get the previous version
    next_version_index = (map(itemgetter(0), tags).index(current_version) - 1)

    return tags[next_version_index][1]


def format_return_value(description, unique_bugs, unique_backout_bugs, changeset_html):
    reg_bugs_link, backout_bugs_link = create_short_url_with_prefix(
        unique_bugs, unique_backout_bugs)
    changeset_full = FULL_CHANGESET_PREFIX + changeset_html
    return_str = '{description}\n{regular_bz_url}{backout_bz_url}{changeset_full}'\
        .format(description=description, regular_bz_url=reg_bugs_link,
                backout_bz_url=backout_bugs_link, changeset_full=changeset_full)

    return return_str


def get_repo_by_version(version):
    """
    Get the repo a given version is found on.
    """
    if version.is_beta:
        return 'https://hg.mozilla.org/releases/mozilla-beta'
    elif version.is_release:
        return 'https://hg.mozilla.org/releases/mozilla-release'
    elif version.is_esr:
        return 'https://hg.mozilla.org/releases/mozilla-esr{}'.format(version.major_number)
    else:
        raise Exception(
            'Unsupported version type {}: {}'.format(
                version.version_type.name, version))


def email_release_drivers(
    addresses, product, version, build_number,
    repo, revision, task_group_id,
):
    # Send an email to the mailing after the build
    email_buglist_string = create_bugs_url(product, version, revision, repo=repo)

    content = """\
A new build has been started:

Commit: {repo}/rev/{revision}
Task group: https://tools.taskcluster.net/push-inspector/#/{task_group_id}

{email_buglist_string}
""".format(repo=repo, revision=revision,
           task_group_id=task_group_id,
           email_buglist_string=email_buglist_string)

    # On r-d, we prefix the subject of the email in order to simplify filtering
    subject_prefix = ""
    if product in {"fennec"}:
        subject_prefix = "[mobile] "
    if product in {"firefox", "devedition"}:
        subject_prefix = "[desktop] "

    subject = '{} Build of {} {} build {}'.format(subject_prefix, product, version, build_number)

    notify_options = {}
    if 'TASKCLUSTER_BASE_URL' in os.environ:
        base_url = os.environ['TASKCLUSTER_PROXY_URL'].rstrip('/')
        notify_options['baseUrl'] = '{}/notify/v1'.format(base_url)
    notify = Notify(notify_options)
    for address in addresses:
        notify.email({
            'address': address,
            'subject': subject,
            'content': content,
        })
