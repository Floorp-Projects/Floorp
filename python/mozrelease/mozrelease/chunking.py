# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from copy import copy


class ChunkingError(Exception):
    pass


def getChunk(things, chunks, thisChunk):
    if thisChunk > chunks:
        raise ChunkingError(
            "thisChunk (%d) is greater than total chunks (%d)" % (thisChunk, chunks)
        )
    possibleThings = copy(things)
    nThings = len(possibleThings)
    for c in range(1, chunks + 1):
        n = nThings // chunks
        # If our things aren't evenly divisible by the number of chunks
        # we need to append one more onto some of them
        if c <= (nThings % chunks):
            n += 1
        if c == thisChunk:
            return possibleThings[0:n]
        del possibleThings[0:n]
