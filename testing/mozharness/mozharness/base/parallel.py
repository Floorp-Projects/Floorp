#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
"""Generic ways to parallelize jobs.
"""


# ChunkingMixin {{{1
class ChunkingMixin(object):
    """Generic Chunking helper methods.
    """
    def query_chunked_list(self, possible_list, this_chunk, total_chunks,
                           sort=False):
        """Split a list of items into a certain number of chunks and
        return the subset of that will occur in this chunk.

        Ported from build.l10n.getLocalesForChunk in build/tools.
        """
        if sort:
            possible_list = sorted(possible_list)
        else:
            # Copy to prevent altering
            possible_list = possible_list[:]
        length = len(possible_list)
        for c in range(1, total_chunks + 1):
            n = length / total_chunks
            # If the total number of items isn't evenly divisible by the
            # number of chunks, we need to append one more onto some chunks
            if c <= (length % total_chunks):
                n += 1
            if c == this_chunk:
                return possible_list[0:n]
            del possible_list[0:n]
