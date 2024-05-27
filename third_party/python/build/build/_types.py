from __future__ import annotations

import os
import typing


ConfigSettings = typing.Mapping[str, typing.Union[str, typing.Sequence[str]]]
Distribution = typing.Literal['sdist', 'wheel']
StrPath = typing.Union[str, 'os.PathLike[str]']
SubprocessRunner = typing.Callable[
    [typing.Sequence[str], typing.Optional[str], typing.Optional[typing.Mapping[str, str]]], None
]
