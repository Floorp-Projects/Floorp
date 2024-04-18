# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import re

_JOINED_SYMBOL_RE = re.compile(r"([^(]*)\(([^)]*)\)$")


def split_symbol(treeherder_symbol):
    """Split a symbol expressed as grp(sym) into its two parts.  If no group is
    given, the returned group is '?'"""
    groupSymbol = "?"
    symbol = treeherder_symbol
    if "(" in symbol:
        match = _JOINED_SYMBOL_RE.match(symbol)
        if match:
            groupSymbol, symbol = match.groups()
        else:
            raise Exception(f"`{symbol}` is not a valid treeherder symbol.")
    return groupSymbol, symbol


def join_symbol(group, symbol):
    """Perform the reverse of split_symbol, combining the given group and
    symbol.  If the group is '?', then it is omitted."""
    if group == "?":
        return symbol
    return f"{group}({symbol})"


def add_suffix(treeherder_symbol, suffix):
    """Add a suffix to a treeherder symbol that may contain a group."""
    group, symbol = split_symbol(treeherder_symbol)
    symbol += str(suffix)
    return join_symbol(group, symbol)


def replace_group(treeherder_symbol, new_group):
    """Add a suffix to a treeherder symbol that may contain a group."""
    _, symbol = split_symbol(treeherder_symbol)
    return join_symbol(new_group, symbol)


def inherit_treeherder_from_dep(job, dep_job):
    """Inherit treeherder defaults from dep_job"""
    treeherder = job.get("treeherder", {})

    dep_th_platform = (
        dep_job.task.get("extra", {})
        .get("treeherder", {})
        .get("machine", {})
        .get("platform", "")
    )
    dep_th_collection = list(
        dep_job.task.get("extra", {}).get("treeherder", {}).get("collection", {}).keys()
    )[0]
    treeherder.setdefault("platform", f"{dep_th_platform}/{dep_th_collection}")
    treeherder.setdefault(
        "tier", dep_job.task.get("extra", {}).get("treeherder", {}).get("tier", 1)
    )
    # Does not set symbol
    treeherder.setdefault("kind", "build")
    return treeherder


def treeherder_defaults(kind, label):
    defaults = {
        # Despite its name, this is expected to be a platform+collection
        "platform": "default/opt",
        "tier": 1,
    }
    if "build" in kind:
        defaults["kind"] = "build"
    elif "test" in kind:
        defaults["kind"] = "test"
    else:
        defaults["kind"] = "other"

    # Takes the uppercased first letter of each part of the kind name, eg:
    # apple-banana -> AB
    defaults["symbol"] = "".join([c[0] for c in kind.split("-")]).upper()

    return defaults
