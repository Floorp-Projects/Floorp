import logging
import os
import re
import requests
from operator import itemgetter
from pkg_resources import parse_version
from simplejson import JSONDecodeError

BUGLIST_PREFIX = 'Bugs since previous changeset: '
BACKOUT_REGEX = r'back(\s?)out|backed out|backing out'
BACKOUT_PREFIX = 'Backouts since previous changeset: '
BUGZILLA_BUGLIST_TEMPLATE = 'https://bugzilla.mozilla.org/buglist.cgi?bug_id={bugs}'
BUG_NUMBER_REGEX = r'bug \d+'
CHANGELOG_TO_FROM_STRING = '{product}_{version}_RELEASE'
CHANGESET_URL_TEMPLATE = 'https://hg.mozilla.org/{release_branch}/{logtype}?fromchange={from_version}&tochange={to_version}&full=1'
FULL_CHANGESET_PREFIX = 'Full Mercurial changelog: '
LIST_DESCRIPTION_TEMPLATE = 'Comparing Mercurial tag {from_version} to {to_version}:'
MAX_BUGS_IN_BUGLIST = 250
MERCURIAL_TAGS_URL_TEMPLATE = 'https://hg.mozilla.org/{release_branch}/json-tags'
NO_BUGS = ''  # Return this when bug list can't be created
URL_SHORTENER_TEMPLATE = 'https://bugzilla.mozilla.org/rest/bitly/shorten?url={url}'

log = logging.getLogger(__name__)


def create_bugs_url(release):
    """
    Creates list of bugs and backout bugs for release-drivers email

    :param release: dict -> containing information about release, from Ship-It
    :return: str -> description of compared releases, with Bugzilla links containing all bugs in changeset
    """
    try:
        # Extract the important data, ignore if beta1 release
        current_version_dot = release['version']
        if re.search(r'b1$', current_version_dot):
            # If the version is beta 1, don't make any links
            return NO_BUGS

        product = release['product']
        branch = release['branch']
        current_revision = release['mozillaRevision']

        # Get the tag version, for display purposes
        current_version_tag = dot_version_to_tag_version(product, current_version_dot)

        # Get all Hg tags for this branch, determine the previous version
        tag_url = MERCURIAL_TAGS_URL_TEMPLATE.format(release_branch=branch)
        mercurial_tags_json = requests.get(tag_url).json()
        previous_version_tag = get_previous_tag_version(product, current_version_dot, current_version_tag, mercurial_tags_json)

        # Get the changeset between these versions, parse for all unique bugs and backout bugs
        resp = requests.get(CHANGESET_URL_TEMPLATE.format(release_branch=branch,
                                                          from_version=previous_version_tag,
                                                          to_version=current_revision,
                                                          logtype='json-pushes'))
        changeset_data = resp.json()
        unique_bugs, unique_backout_bugs = get_bugs_in_changeset(changeset_data)

        # Return a descriptive string with links if any relevant bugs are found
        if unique_bugs or unique_backout_bugs:
            description_string = LIST_DESCRIPTION_TEMPLATE.format(from_version=previous_version_tag,
                                                                  to_version=current_version_tag)

            changeset_html = CHANGESET_URL_TEMPLATE.format(release_branch=branch,
                                                           from_version=previous_version_tag,
                                                           to_version=current_revision,
                                                           logtype='pushloghtml')

            return format_return_value(description_string, unique_bugs, unique_backout_bugs, changeset_html)
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

            changeset_desc_lower = changeset['desc'].lower()
            bug_re = re.search(BUG_NUMBER_REGEX, changeset_desc_lower)

            if bug_re:
                bug_number = bug_re.group().split(' ')[1]

                if is_backout_bug(changeset_desc_lower):
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


def is_backout_bug(changeset_description_lowercase):
    return re.search(BACKOUT_REGEX, changeset_description_lowercase)


def create_short_url_with_prefix(buglist, backout_buglist):
    # Create link if there are bugs, else empty string
    urls = []
    for set_of_bugs, prefix in [(buglist, BUGLIST_PREFIX), (backout_buglist, BACKOUT_PREFIX)]:
        if set_of_bugs and len(set_of_bugs) < MAX_BUGS_IN_BUGLIST:
            try:
                long_bugzilla_link = BUGZILLA_BUGLIST_TEMPLATE.format(bugs='%2C'.join(set_of_bugs))
                url = requests.get(URL_SHORTENER_TEMPLATE.format(url=long_bugzilla_link)).json()['url']
                url = prefix + url + '\n'

            except (KeyError, JSONDecodeError,):
                # If the Bugzilla link fails despite limiting the number of bugs, don't make the url and continue
                url = ''
        else:
            url = ''

        urls.append(url)

    return urls[0], urls[1]


def dot_version_to_tag_version(product, dot_version):
    underscore_version = dot_version.replace('.', '_')
    return CHANGELOG_TO_FROM_STRING.format(product=product.upper(), version=underscore_version)


def tag_version_to_dot_version_parse(tag):
    dot_version = '.'.join(tag.split('_')[1:-1])
    return parse_version(dot_version)


def get_previous_tag_version(product, current_version_dot, current_version_tag, mercurial_tags_json):
    """Gets the previous hg version tag for the product and branch, given the current version tag"""

    def _invalid_tag_filter(tag):
        """Filters by product and removes incorrect major version + base, end releases"""
        major_version = current_version_dot.split('.')[0]
        prod_major_version_re = r'^{product}_{major_version}'.format(product=product.upper(),
                                                                     major_version=major_version)

        return 'BASE' not in tag and \
               'END' not in tag and \
               'RELEASE' in tag and \
               re.match(prod_major_version_re, tag)

    # Get rid of irrelevant tags, sort by date and extract the tag string
    tags = set(map(itemgetter('tag'), mercurial_tags_json['tags']))
    tags = filter(_invalid_tag_filter, tags)
    dot_tag_version_mapping = zip(map(tag_version_to_dot_version_parse, tags), tags)
    dot_tag_version_mapping.append(  # Add the current version to the list
        (parse_version(current_version_dot), current_version_tag)
    )
    dot_tag_version_mapping = sorted(dot_tag_version_mapping, key=itemgetter(0))

    # Find where the current version is and go back one to get the previous version
    next_version_index = map(itemgetter(0), dot_tag_version_mapping).index(parse_version(current_version_dot)) - 1

    return dot_tag_version_mapping[next_version_index][1]


def format_return_value(description, unique_bugs, unique_backout_bugs, changeset_html):
    reg_bugs_link, backout_bugs_link = create_short_url_with_prefix(unique_bugs, unique_backout_bugs)
    changeset_full = FULL_CHANGESET_PREFIX + changeset_html
    return_str = '{description}\n{regular_bz_url}{backout_bz_url}{changeset_full}'\
        .format(description=description, regular_bz_url=reg_bugs_link,
                backout_bz_url=backout_bugs_link, changeset_full=changeset_full)

    return return_str


