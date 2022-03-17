from tokenize import Number
from typing import Any, Optional, Mapping, MutableMapping

from ._module import BidiModule, command


class BrowsingContext(BidiModule):
    @command
    def get_tree(self, max_depth: Optional[int] = None, parent: Optional[str] = None):
        params: MutableMapping[str, Any] = {}

        if max_depth is not None:
            params["maxDepth"] = max_depth
        if parent is not None:
            params["parent"] = parent

        return params

    @get_tree.result
    def _get_tree(self, result: Mapping[str, Any]) -> Any:
        assert result["contexts"] is not None
        assert isinstance(result["contexts"], list)

        return result["contexts"]
