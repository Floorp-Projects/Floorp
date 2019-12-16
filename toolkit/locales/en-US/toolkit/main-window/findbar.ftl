# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

### This file contains the entities needed to use the Find Bar.

findbar-next =
    .tooltiptext = Find the next occurrence of the phrase
findbar-previous =
    .tooltiptext = Find the previous occurrence of the phrase

findbar-find-button-close =
    .tooltiptext = Close find bar

findbar-highlight-all2 =
    .label = Highlight All
    .accesskey = { PLATFORM() ->
        [macos] l
       *[other] a
    }
    .tooltiptext = Highlight all occurrences of the phrase

findbar-case-sensitive =
    .label = Match Case
    .accesskey = C
    .tooltiptext = Search with case sensitivity

findbar-match-diacritics =
    .label = Match Diacritics
    .accesskey = i
    .tooltiptext = Distinguish between accented letters and their base letters (for example, when searching for “resume”, “résumé” will not be matched)

findbar-entire-word =
    .label = Whole Words
    .accesskey = W
    .tooltiptext = Search whole words only
