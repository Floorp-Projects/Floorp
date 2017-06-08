# Licensed under the Mozilla Public Licence 2.0.
# https://www.mozilla.org/en-US/MPL/2.0

import uuid
import slugid


def testEncode():
    """ Test that we can correctly encode a "non-nice" uuid (with first bit
    set) to its known slug. The specific uuid was chosen since it has a slug
    which contains both `-` and `_` characters."""

    # 10000000010011110011111111001000110111111100101101001011000001101000100111111011101011101111101011010101111000011000011101010100....
    # <8 ><0 ><4 ><f ><3 ><f ><c ><8 ><d ><f ><c ><b ><4 ><b ><0 ><6 ><8 ><9 ><f ><b ><a ><e ><f ><a ><d ><5 ><e ><1 ><8 ><7 ><5 ><4 >
    # < g  >< E  >< 8  >< _  >< y  >< N  >< _  >< L  >< S  >< w  >< a  >< J  >< -  >< 6  >< 7  >< 6  >< 1  >< e  >< G  >< H  >< V  >< A  >
    uuid_ = uuid.UUID('{804f3fc8-dfcb-4b06-89fb-aefad5e18754}')
    expectedSlug = 'gE8_yN_LSwaJ-6761eGHVA'
    actualSlug = slugid.encode(uuid_)

    assert expectedSlug == actualSlug, "UUID not correctly encoded into slug: '" + expectedSlug + "' != '" + actualSlug + "'"


def testDecode():
    """ Test that we can decode a "non-nice" slug (first bit of uuid is set)
    that begins with `-`"""

    # 11111011111011111011111011111011111011111011111001000011111011111011111111111111111111111111111111111111111111111111111111111101....
    # <f ><b ><e ><f ><b ><e ><f ><b ><e ><f ><b ><e ><4 ><3 ><e ><f ><b ><f ><f ><f ><f ><f ><f ><f ><f ><f ><f ><f ><f ><f ><f ><d >
    # < -  >< -  >< -  >< -  >< -  >< -  >< -  >< -  >< Q  >< -  >< -  >< -  >< _  >< _  >< _  >< _  >< _  >< _  >< _  >< _  >< _  >< Q  >
    slug = '--------Q--__________Q'
    expectedUuid = uuid.UUID('{fbefbefb-efbe-43ef-bfff-fffffffffffd}')
    actualUuid = slugid.decode(slug)

    assert expectedUuid == actualUuid, "Slug not correctly decoded into uuid: '" + str(expectedUuid) + "' != '" + str(actualUuid) + "'"


def testUuidEncodeDecode():
    """ Test that 10000 v4 uuids are unchanged after encoding and then decoding them"""

    for i in range(0, 10000):
        uuid1 = uuid.uuid4()
        slug = slugid.encode(uuid1)
        uuid2 = slugid.decode(slug)

        assert uuid1 == uuid2, "Encode and decode isn't identity: '" + str(uuid1) + "' != '" + str(uuid2) + "'"


def testSlugDecodeEncode():
    """ Test that 10000 v4 slugs are unchanged after decoding and then encoding them."""

    for i in range(0, 10000):
        slug1 = slugid.v4()
        uuid_ = slugid.decode(slug1)
        slug2 = slugid.encode(uuid_)

        assert slug1 == slug2, "Decode and encode isn't identity"


def testSpreadNice():
    """ Make sure that all allowed characters can appear in all allowed
    positions within the "nice" slug. In this test we generate over a thousand
    slugids, and make sure that every possible allowed character per position
    appears at least once in the sample of all slugids generated. We also make
    sure that no other characters appear in positions in which they are not
    allowed.

    base 64 encoding char -> value:
    ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_
    0         1         2         3         4         5          6
    0123456789012345678901234567890123456789012345678901234567890123

    e.g. from this we can see 'j' represents 35 in base64

    The following comments show the 128 bits of the v4 uuid in binary, hex and
    base 64 encodings. The 6 fixed bits (`0`/`1`) according to RFC 4122, plus
    the first (most significant) fixed bit (`0`) are shown among the 121
    arbitrary value bits (`.`/`x`). The `x` means the same as `.` but just
    highlights which bits are grouped together for the respective encoding.

    schema:
         <..........time_low............><...time_mid...><time_hi_+_vers><clk_hi><clk_lo><.....................node.....................>

    bin: 0xxx............................................0100............10xx............................................................
    hex:  $A <01><02><03><04><05><06><07><08><09><10><11> 4  <13><14><15> $B <17><18><19><20><21><22><23><24><25><26><27><28><29><30><31>

    => $A in {0, 1, 2, 3, 4, 5, 6, 7} (0b0xxx)
    => $B in {8, 9, A, B} (0b10xx)

    bin: 0xxxxx..........................................0100xx......xxxx10............................................................xx0000
    b64:   $C  < 01 >< 02 >< 03 >< 04 >< 05 >< 06 >< 07 >  $D  < 09 >  $E  < 11 >< 12 >< 13 >< 14 >< 15 >< 16 >< 17 >< 18 >< 19 >< 20 >  $F

    => $C in {A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z, a, b, c, d, e, f} (0b0xxxxx)
    => $D in {Q, R, S, T} (0b0100xx)
    => $E in {C, G, K, O, S, W, a, e, i, m, q, u, y, 2, 6, -} (0bxxxx10)
    => $F in {A, Q, g, w} (0bxx0000)"""

    charsAll = ''.join(sorted('ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_'))
    # 0 - 31: 0b0xxxxx
    charsC = ''.join(sorted('ABCDEFGHIJKLMNOPQRSTUVWXYZabcdef'))
    # 16, 17, 18, 19: 0b0100xx
    charsD = ''.join(sorted('QRST'))
    # 2, 6, 10, 14, 18, 22, 26, 30, 34, 38, 42, 46, 50, 54, 58, 62: 0bxxxx10
    charsE = ''.join(sorted('CGKOSWaeimquy26-'))
    # 0, 16, 32, 48: 0bxx0000
    charsF = ''.join(sorted('AQgw'))
    expected = [charsC, charsAll, charsAll, charsAll, charsAll, charsAll, charsAll, charsAll, charsD, charsAll, charsE, charsAll, charsAll, charsAll, charsAll, charsAll, charsAll, charsAll, charsAll, charsAll, charsAll, charsF]
    spreadTest(slugid.nice, expected)


def testSpreadV4():
    """ This test is the same as niceSpreadTest but for slugid.v4() rather than
    slugid.nice(). The only difference is that a v4() slug can start with any of
    the base64 characters since the first six bits of the uuid are random."""

    charsAll = ''.join(sorted('ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_'))
    # 16, 17, 18, 19: 0b0100xx
    charsD = ''.join(sorted('QRST'))
    # 2, 6, 10, 14, 18, 22, 26, 30, 34, 38, 42, 46, 50, 54, 58, 62: 0bxxxx10
    charsE = ''.join(sorted('CGKOSWaeimquy26-'))
    # 0, 16, 32, 48: 0bxx0000
    charsF = ''.join(sorted('AQgw'))
    expected = [charsAll, charsAll, charsAll, charsAll, charsAll, charsAll, charsAll, charsAll, charsD, charsAll, charsE, charsAll, charsAll, charsAll, charsAll, charsAll, charsAll, charsAll, charsAll, charsAll, charsAll, charsF]
    spreadTest(slugid.v4, expected)


def spreadTest(generator, expected):
    """ `spreadTest` runs a test against the `generator` function, to check that
    when calling it 64*40 times, the range of characters per string position it
    returns matches the array `expected`, where each entry in `expected` is a
    string of all possible characters that should appear in that position in the
    string, at least once in the sample of 64*40 responses from the `generator`
    function"""
    # k is an array which stores which characters were found at which
    # positions. It has one entry per slugid character, therefore 22 entries.
    # Each entry is a dict with a key for each character found, and its value
    # as the number of times that character appeared at that position in the
    # slugid in the large sample of slugids generated in this test.
    k = [{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}]

    # Generate a large sample of slugids, and record what characters appeared
    # where...  A monte-carlo test has demonstrated that with 64 * 20
    # iterations, no failure occurred in 1000 simulations, so 64 * 40 should be
    # suitably large to rule out false positives.
    for i in range(0, 64 * 40):
        slug = generator()
        assert len(slug) == 22
        for j in range(0, 22):
            if slug[j] in k[j]:
                k[j][slug[j]] = k[j][slug[j]] + 1
            else:
                k[j][slug[j]] = 1

    # Compose results into an array `actual`, for comparison with `expected`
    actual = []
    for j in range(0, len(k)):
        actual.append('')
        for a in k[j].keys():
            if k[j][a] > 0:
                actual[j] += a
        # sort for easy comparison
        actual[j] = ''.join(sorted(actual[j]))

    assert arraysEqual(expected, actual), "In a large sample of generated slugids, the range of characters found per character position in the sample did not match expected results.\n\nExpected: " + str(expected) + "\n\nActual: " + str(actual)

def arraysEqual(a, b):
    """ returns True if arrays a and b are equal"""
    return cmp(a, b) == 0
