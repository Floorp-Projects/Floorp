import re

from taskgraph.morph import _SCOPE_SUMMARY_REGEXPS

# TODO Bug 1805651: let taskgraph allow us to specify this route
_SCOPE_SUMMARY_REGEXPS.append(
    re.compile(r"(index:insert-task:mobile\.v3.[^.]*\.).*")
)
