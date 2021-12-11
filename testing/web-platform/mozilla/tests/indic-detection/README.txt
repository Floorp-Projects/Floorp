The text (non-markup/JavaScript) content of the files in this directory originates from Wikipedia and
is licensed under the Creative Commons Attribution-ShareAlike 3.0 Unported license
<https://creativecommons.org/licenses/by-sa/3.0/legalcode>.

The content comes from the following articles (and their revisions):
https://hi.wikipedia.org/w/index.php?title=%E0%A4%AE%E0%A4%82%E0%A4%97%E0%A4%B2_%E0%A4%97%E0%A5%8D%E0%A4%B0%E0%A4%B9&oldid=5105576
https://ta.wikipedia.org/w/index.php?title=%E0%AE%9A%E0%AF%86%E0%AE%B5%E0%AF%8D%E0%AE%B5%E0%AE%BE%E0%AE%AF%E0%AF%8D_(%E0%AE%95%E0%AF%8B%E0%AE%B3%E0%AF%8D)&oldid=3129711

This directory tests that content meant for intentionally mis-encoded legacy Devanagari and Tamil fonts that Chrome's encoding detector knows about is detected as windows-1252. These fonts assign Devanagari or Tamil glyphs to code points that are symbols or Latin characters in windows-1252. In chardetng, the detection mechanism is determining that the content isn't in any chardetng-supported encoding and, therefore, the fallback is windows-1252.

Tests are missing for the following fonts that Chrome knows about:
LT TM Barani
TMNews
TamilWeb
