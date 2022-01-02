# Compiled hyphenation table format for mapped_hyph

The file is a "flattened" representation of the list of `HyphenDict` structs
and descendant objects used by libhyphen
(see [hyphen.h](https://github.com/hunspell/hyphen/blob/master/hyphen.h)).

Note that multi-byte integer types in the file are stored in _little-endian_ byte order.

## Overall file header

The file begins with a 4-byte "signature", followed by a count of the number
of hyphenation levels, and an array of offsets to each hyphenation level.
A "level" is essentially equivalent to libhyphen's `HyphenDict`.

### Header (size: 8 bytes + 4 * numLevels)
Type | Name | Description
-----|------|------------
uint8[4] | magicNumber | 4-byte file identification code: ['H', 'y', 'f', '0']
uint32 | numLevels | number of hyphenation levels present
uint32[numLevels] | levelOffset | offset from start of file to each Level

Currently, there are normally 2 hyphenation levels, as the parser/compiler will
generate a default first level if no NEXTLEVEL keyword is present in the pattern file.

## Hyphenation Level

Each level of the hyphenation pattern begins with a Level header, followed by
the data for its states and the strings they refer to.
When the hyphenation machine is executed, we always begin at state offset 0
(from the level's stateDataBase); each transition to a new state represents the
target directly by its offset from stateDataBase.
A state offset of 0xFFFFFF is considered invalid.

Strings are represented as offsets from the level's stringDataBase; each string
is encoded as a one-byte length followed by `length` bytes of utf-8 data.
(So the maximum string length is 255 utf-8 code units; this is far more than any actual
hyphenation dictionary uses).
A string offset of 0xFFFF is considered invalid and represents an absent string.

The minimum number of characters that must be kept together at the start/end of a word,
or of a component of a compound (i.e. the `...Min` values) is a count of _Unicode characters_,
not UTF-8 code units. (Note that the presentation-form ligature characters U+FB00 'ﬀ' through U+FB06 'ﬆ'
are counted as 2 or 3 characters for this purpose.)

### Level (size: 16 bytes + state data + string data, padded to a 4-byte boundary)
Type | Name | Description
-----|------|------------
uint32  |  stateDataBase  |  offset from beginning of Level to start of level's State data
uint32  |  stringDataBase  |  offset from beginning of Level to start of level's packed String data
uint16  |  noHyphenStringOffset  |  from level's stringDataBase
uint16  |  noHyphenCount  |  number of (NUL-separated) strings in the nohyphen string
uint8   |  leftHyphenMin  |  minimum number of characters kept together at start of word
uint8   |  rightHyphenMin  |  minimum number of characters kept together at end of word
uint8   |  compoundLeftHyphenMin  |  minimum number of characters kept together at start of second component of a compound
uint8   |  compoundRightHyphenMin  |  minimum number of characters kept together at end of first component of a compound

## State

Each state, referred to by its offset from the level's stateDataBase, consists of a header
followed by an array of transitions for input bytes that need to be matched in this state.
The state also records a fallback state offset, which is the transition to be taken
if the next input byte does not match any of the transition records.

If a match string is present (i.e. `matchStringOffset` is not 0xFFFF), it is a string of hyphenation values
(encoded as ASCII digits '0'..'9') to be applied at the current position in the word.

### StateHeader (size: 8 bytes)
Type | Name | Description
-----|------|------------
uint32  |  fallbackStateOffset  |  (from level's stateDataBase)
uint16  |  matchStringOffset  |  (from level's stringDataBase)
uint8   |  numTransitions  |  count of Transitions that follow the StateHeader and optional StateHeaderExtension
uint8   |  isExtended  |  if non-zero, the StateHeader is immediately followed by a StateHeaderExtension

If the `isExtended` flag in the state header is set, this state includes a potential spelling change
and there is an extended form of the header present before the array of transitions.
(Note that extended states with spelling-change rules are not yet supported by the mapped_hyph engine;
none of the hyphenation dictionaries shipped with Firefox includes such rules.)

### StateHeaderExtension (size: 4 bytes)
Type | Name | Description
-----|------|------------
uint16  |  replacementStringOffset  |  (from level's stringDataBase) the replacement string
int8   |   replacementIndex  |  index of the byte position (relative to current position in the word) at which the spelling replacement should happen
int8   |   replacementCut  |  number of bytes to cut from the original word when making the replacement

## Transitions

The state's transitions are encoded as an array of Transition records, each corresponding to an input byte
and providing the offset of the new state. The transitions for each state are sorted by ascending value of input byte
(although in practice there are usually only a few valid transitions, and so a binary search does not seem to be
worthwhile).

### Transition (size: 4 bytes)
Type | Name | Description
-----|------|------------
uint24  |  newStateOffset  |  (from level's stateDataBase)
uint8   |  inputByte  |  the input byte (utf-8 code unit) for this transition
