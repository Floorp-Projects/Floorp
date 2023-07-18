# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import re


def attrmatch(attributes, **kwargs):
    """Determine whether the given set of task attributes matches.

    The conditions are given as keyword arguments, where each keyword names an
    attribute. The keyword value can be a literal, a set, or a callable:

        * A literal must match the attribute exactly.
        * Given a set or list, the attribute value must be contained within it.
        * A callable is called with the attribute value and returns a boolean.

    If an attribute is specified as a keyword argument but not present in the
    task's attributes, the result is False.

    Args:
        attributes (dict): The task's attributes object.
        kwargs (dict): The conditions the task's attributes must satisfy in
                       order to match.
    Returns:
        bool: Whether the task's attributes match the conditions or not.
    """
    for kwkey, kwval in kwargs.items():
        if kwkey not in attributes:
            return False
        attval = attributes[kwkey]
        if isinstance(kwval, (set, list)):
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
    matches = [v for k, v in attributes.items() if re.match(k + "$", target)]
    if matches:
        return matches

    # default
    if "default" in attributes:
        return [attributes["default"]]

    return []


def _match_run_on(key, run_on):
    """
    Determine whether the given parameter is included in the corresponding `run-on-attribute`.
    """
    if "all" in run_on:
        return True
    return key in run_on


match_run_on_projects = _match_run_on
match_run_on_tasks_for = _match_run_on


def match_run_on_git_branches(git_branch, run_on_git_branches):
    """
    Determine whether the given project is included in the `run-on-git-branches` parameter.
    Allows 'all'.
    """
    if "all" in run_on_git_branches:
        return True

    for expected_git_branch_pattern in run_on_git_branches:
        if re.match(expected_git_branch_pattern, git_branch):
            return True

    return False


def sorted_unique_list(*args):
    """Join one or more lists, and return a sorted list of unique members"""
    combined = set().union(*args)
    return sorted(combined)
