# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


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
