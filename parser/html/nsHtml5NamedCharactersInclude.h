/*
 * Copyright 2004-2010 Apple Computer, Inc., Mozilla Foundation, and Opera
 * Software ASA.
 *
 * You are granted a license to use, reproduce and create derivative works of
 * this document.
 */

/* Data generated from the table of named character references found at
 *
 *   http://www.whatwg.org/specs/web-apps/current-work/multipage/named-character-references.html#named-character-references
 *
 * Files that #include this file must #define NAMED_CHARACTER_REFERENCE as a
 * macro of four parameters:
 *
 *   1.  a unique integer N identifying the Nth [0,1,..] macro expansion in this
 * file,
 *   2.  a comma-separated sequence of characters comprising the character name,
 *       without the first two letters or 0 if the sequence would be empty.
 *       See Tokenizer.java.
 *   3.  the length of this sequence of characters,
 *   4.  placeholder flag (0 if argument #is not a placeholder and 1 if it is),
 *   5.  a comma-separated sequence of char16_t literals corresponding
 *       to the code-point(s) of the named character.
 *
 * The macro expansion doesn't have to refer to all or any of these parameters,
 * but common sense dictates that it should involve at least one of them.
 */

// This #define allows the NAMED_CHARACTER_REFERENCE macro to accept comma-
// separated sequences as single macro arguments.  Using commas directly would
// split the sequence into multiple macro arguments.
#define _ ,

NAMED_CHARACTER_REFERENCE(0, /* A E */ 'l' _ 'i' _ 'g', 3, 0, 0x00c6 _ 0)
NAMED_CHARACTER_REFERENCE(1, /* A E */ 'l' _ 'i' _ 'g' _ ';', 4, 0, 0x00c6 _ 0)
NAMED_CHARACTER_REFERENCE(2, /* A M */ 'P', 1, 0, 0x0026 _ 0)
NAMED_CHARACTER_REFERENCE(3, /* A M */ 'P' _ ';', 2, 0, 0x0026 _ 0)
NAMED_CHARACTER_REFERENCE(4, /* A a */ 'c' _ 'u' _ 't' _ 'e', 4, 0, 0x00c1 _ 0)
NAMED_CHARACTER_REFERENCE(5,
                          /* A a */ 'c' _ 'u' _ 't' _ 'e' _ ';', 5, 0,
                          0x00c1 _ 0)
NAMED_CHARACTER_REFERENCE(6,
                          /* A b */ 'r' _ 'e' _ 'v' _ 'e' _ ';', 5, 0,
                          0x0102 _ 0)
NAMED_CHARACTER_REFERENCE(7, /* A c */ 'i' _ 'r' _ 'c', 3, 0, 0x00c2 _ 0)
NAMED_CHARACTER_REFERENCE(8, /* A c */ 'i' _ 'r' _ 'c' _ ';', 4, 0, 0x00c2 _ 0)
NAMED_CHARACTER_REFERENCE(9, /* A c */ 'y' _ ';', 2, 0, 0x0410 _ 0)
NAMED_CHARACTER_REFERENCE(10, /* A f */ 'r' _ ';', 2, 0, 0xd835 _ 0xdd04)
NAMED_CHARACTER_REFERENCE(11, /* A g */ 'r' _ 'a' _ 'v' _ 'e', 4, 0, 0x00c0 _ 0)
NAMED_CHARACTER_REFERENCE(12,
                          /* A g */ 'r' _ 'a' _ 'v' _ 'e' _ ';', 5, 0,
                          0x00c0 _ 0)
NAMED_CHARACTER_REFERENCE(13, /* A l */ 'p' _ 'h' _ 'a' _ ';', 4, 0, 0x0391 _ 0)
NAMED_CHARACTER_REFERENCE(14, /* A m */ 'a' _ 'c' _ 'r' _ ';', 4, 0, 0x0100 _ 0)
NAMED_CHARACTER_REFERENCE(15, /* A n */ 'd' _ ';', 2, 0, 0x2a53 _ 0)
NAMED_CHARACTER_REFERENCE(16, /* A o */ 'g' _ 'o' _ 'n' _ ';', 4, 0, 0x0104 _ 0)
NAMED_CHARACTER_REFERENCE(17, /* A o */ 'p' _ 'f' _ ';', 3, 0, 0xd835 _ 0xdd38)
NAMED_CHARACTER_REFERENCE(
    18,
    /* A p */
    'p' _ 'l' _ 'y' _ 'F' _ 'u' _ 'n' _ 'c' _ 't' _ 'i' _ 'o' _ 'n' _ ';', 12,
    0, 0x2061 _ 0)
NAMED_CHARACTER_REFERENCE(19, /* A r */ 'i' _ 'n' _ 'g', 3, 0, 0x00c5 _ 0)
NAMED_CHARACTER_REFERENCE(20, /* A r */ 'i' _ 'n' _ 'g' _ ';', 4, 0, 0x00c5 _ 0)
NAMED_CHARACTER_REFERENCE(21, /* A s */ 'c' _ 'r' _ ';', 3, 0, 0xd835 _ 0xdc9c)
NAMED_CHARACTER_REFERENCE(22,
                          /* A s */ 's' _ 'i' _ 'g' _ 'n' _ ';', 5, 0,
                          0x2254 _ 0)
NAMED_CHARACTER_REFERENCE(23, /* A t */ 'i' _ 'l' _ 'd' _ 'e', 4, 0, 0x00c3 _ 0)
NAMED_CHARACTER_REFERENCE(24,
                          /* A t */ 'i' _ 'l' _ 'd' _ 'e' _ ';', 5, 0,
                          0x00c3 _ 0)
NAMED_CHARACTER_REFERENCE(25, /* A u */ 'm' _ 'l', 2, 0, 0x00c4 _ 0)
NAMED_CHARACTER_REFERENCE(26, /* A u */ 'm' _ 'l' _ ';', 3, 0, 0x00c4 _ 0)
NAMED_CHARACTER_REFERENCE(
    27,
    /* B a */ 'c' _ 'k' _ 's' _ 'l' _ 'a' _ 's' _ 'h' _ ';', 8, 0, 0x2216 _ 0)
NAMED_CHARACTER_REFERENCE(28, /* B a */ 'r' _ 'v' _ ';', 3, 0, 0x2ae7 _ 0)
NAMED_CHARACTER_REFERENCE(29,
                          /* B a */ 'r' _ 'w' _ 'e' _ 'd' _ ';', 5, 0,
                          0x2306 _ 0)
NAMED_CHARACTER_REFERENCE(30, /* B c */ 'y' _ ';', 2, 0, 0x0411 _ 0)
NAMED_CHARACTER_REFERENCE(31,
                          /* B e */ 'c' _ 'a' _ 'u' _ 's' _ 'e' _ ';', 6, 0,
                          0x2235 _ 0)
NAMED_CHARACTER_REFERENCE(
    32,
    /* B e */ 'r' _ 'n' _ 'o' _ 'u' _ 'l' _ 'l' _ 'i' _ 's' _ ';', 9, 0,
    0x212c _ 0)
NAMED_CHARACTER_REFERENCE(33, /* B e */ 't' _ 'a' _ ';', 3, 0, 0x0392 _ 0)
NAMED_CHARACTER_REFERENCE(34, /* B f */ 'r' _ ';', 2, 0, 0xd835 _ 0xdd05)
NAMED_CHARACTER_REFERENCE(35, /* B o */ 'p' _ 'f' _ ';', 3, 0, 0xd835 _ 0xdd39)
NAMED_CHARACTER_REFERENCE(36, /* B r */ 'e' _ 'v' _ 'e' _ ';', 4, 0, 0x02d8 _ 0)
NAMED_CHARACTER_REFERENCE(37, /* B s */ 'c' _ 'r' _ ';', 3, 0, 0x212c _ 0)
NAMED_CHARACTER_REFERENCE(38,
                          /* B u */ 'm' _ 'p' _ 'e' _ 'q' _ ';', 5, 0,
                          0x224e _ 0)
NAMED_CHARACTER_REFERENCE(39, /* C H */ 'c' _ 'y' _ ';', 3, 0, 0x0427 _ 0)
NAMED_CHARACTER_REFERENCE(40, /* C O */ 'P' _ 'Y', 2, 0, 0x00a9 _ 0)
NAMED_CHARACTER_REFERENCE(41, /* C O */ 'P' _ 'Y' _ ';', 3, 0, 0x00a9 _ 0)
NAMED_CHARACTER_REFERENCE(42,
                          /* C a */ 'c' _ 'u' _ 't' _ 'e' _ ';', 5, 0,
                          0x0106 _ 0)
NAMED_CHARACTER_REFERENCE(43, /* C a */ 'p' _ ';', 2, 0, 0x22d2 _ 0)
NAMED_CHARACTER_REFERENCE(
    44,
    /* C a */
    'p' _ 'i' _ 't' _ 'a' _ 'l' _ 'D' _ 'i' _ 'f' _ 'f' _ 'e' _ 'r' _ 'e' _ 'n' _ 't' _ 'i' _ 'a' _ 'l' _ 'D' _ ';',
    19, 0, 0x2145 _ 0)
NAMED_CHARACTER_REFERENCE(45,
                          /* C a */ 'y' _ 'l' _ 'e' _ 'y' _ 's' _ ';', 6, 0,
                          0x212d _ 0)
NAMED_CHARACTER_REFERENCE(46,
                          /* C c */ 'a' _ 'r' _ 'o' _ 'n' _ ';', 5, 0,
                          0x010c _ 0)
NAMED_CHARACTER_REFERENCE(47, /* C c */ 'e' _ 'd' _ 'i' _ 'l', 4, 0, 0x00c7 _ 0)
NAMED_CHARACTER_REFERENCE(48,
                          /* C c */ 'e' _ 'd' _ 'i' _ 'l' _ ';', 5, 0,
                          0x00c7 _ 0)
NAMED_CHARACTER_REFERENCE(49, /* C c */ 'i' _ 'r' _ 'c' _ ';', 4, 0, 0x0108 _ 0)
NAMED_CHARACTER_REFERENCE(50,
                          /* C c */ 'o' _ 'n' _ 'i' _ 'n' _ 't' _ ';', 6, 0,
                          0x2230 _ 0)
NAMED_CHARACTER_REFERENCE(51, /* C d */ 'o' _ 't' _ ';', 3, 0, 0x010a _ 0)
NAMED_CHARACTER_REFERENCE(52,
                          /* C e */ 'd' _ 'i' _ 'l' _ 'l' _ 'a' _ ';', 6, 0,
                          0x00b8 _ 0)
NAMED_CHARACTER_REFERENCE(
    53,
    /* C e */ 'n' _ 't' _ 'e' _ 'r' _ 'D' _ 'o' _ 't' _ ';', 8, 0, 0x00b7 _ 0)
NAMED_CHARACTER_REFERENCE(54, /* C f */ 'r' _ ';', 2, 0, 0x212d _ 0)
NAMED_CHARACTER_REFERENCE(55, /* C h */ 'i' _ ';', 2, 0, 0x03a7 _ 0)
NAMED_CHARACTER_REFERENCE(
    56,
    /* C i */ 'r' _ 'c' _ 'l' _ 'e' _ 'D' _ 'o' _ 't' _ ';', 8, 0, 0x2299 _ 0)
NAMED_CHARACTER_REFERENCE(
    57,
    /* C i */ 'r' _ 'c' _ 'l' _ 'e' _ 'M' _ 'i' _ 'n' _ 'u' _ 's' _ ';', 10, 0,
    0x2296 _ 0)
NAMED_CHARACTER_REFERENCE(
    58,
    /* C i */ 'r' _ 'c' _ 'l' _ 'e' _ 'P' _ 'l' _ 'u' _ 's' _ ';', 9, 0,
    0x2295 _ 0)
NAMED_CHARACTER_REFERENCE(
    59,
    /* C i */ 'r' _ 'c' _ 'l' _ 'e' _ 'T' _ 'i' _ 'm' _ 'e' _ 's' _ ';', 10, 0,
    0x2297 _ 0)
NAMED_CHARACTER_REFERENCE(
    60,
    /* C l */
    'o' _ 'c' _ 'k' _ 'w' _ 'i' _ 's' _ 'e' _ 'C' _ 'o' _ 'n' _ 't' _ 'o' _ 'u' _ 'r' _ 'I' _ 'n' _ 't' _ 'e' _ 'g' _ 'r' _ 'a' _ 'l' _ ';',
    23, 0, 0x2232 _ 0)
NAMED_CHARACTER_REFERENCE(
    61,
    /* C l */
    'o' _ 's' _ 'e' _ 'C' _ 'u' _ 'r' _ 'l' _ 'y' _ 'D' _ 'o' _ 'u' _ 'b' _ 'l' _ 'e' _ 'Q' _ 'u' _ 'o' _ 't' _ 'e' _ ';',
    20, 0, 0x201d _ 0)
NAMED_CHARACTER_REFERENCE(
    62,
    /* C l */
    'o' _ 's' _ 'e' _ 'C' _ 'u' _ 'r' _ 'l' _ 'y' _ 'Q' _ 'u' _ 'o' _ 't' _ 'e' _ ';',
    14, 0, 0x2019 _ 0)
NAMED_CHARACTER_REFERENCE(63, /* C o */ 'l' _ 'o' _ 'n' _ ';', 4, 0, 0x2237 _ 0)
NAMED_CHARACTER_REFERENCE(64,
                          /* C o */ 'l' _ 'o' _ 'n' _ 'e' _ ';', 5, 0,
                          0x2a74 _ 0)
NAMED_CHARACTER_REFERENCE(
    65,
    /* C o */ 'n' _ 'g' _ 'r' _ 'u' _ 'e' _ 'n' _ 't' _ ';', 8, 0, 0x2261 _ 0)
NAMED_CHARACTER_REFERENCE(66,
                          /* C o */ 'n' _ 'i' _ 'n' _ 't' _ ';', 5, 0,
                          0x222f _ 0)
NAMED_CHARACTER_REFERENCE(
    67,
    /* C o */
    'n' _ 't' _ 'o' _ 'u' _ 'r' _ 'I' _ 'n' _ 't' _ 'e' _ 'g' _ 'r' _ 'a' _ 'l' _ ';',
    14, 0, 0x222e _ 0)
NAMED_CHARACTER_REFERENCE(68, /* C o */ 'p' _ 'f' _ ';', 3, 0, 0x2102 _ 0)
NAMED_CHARACTER_REFERENCE(
    69,
    /* C o */ 'p' _ 'r' _ 'o' _ 'd' _ 'u' _ 'c' _ 't' _ ';', 8, 0, 0x2210 _ 0)
NAMED_CHARACTER_REFERENCE(
    70,
    /* C o */
    'u' _ 'n' _ 't' _ 'e' _ 'r' _ 'C' _ 'l' _ 'o' _ 'c' _ 'k' _ 'w' _ 'i' _ 's' _ 'e' _ 'C' _ 'o' _ 'n' _ 't' _ 'o' _ 'u' _ 'r' _ 'I' _ 'n' _ 't' _ 'e' _ 'g' _ 'r' _ 'a' _ 'l' _ ';',
    30, 0, 0x2233 _ 0)
NAMED_CHARACTER_REFERENCE(71, /* C r */ 'o' _ 's' _ 's' _ ';', 4, 0, 0x2a2f _ 0)
NAMED_CHARACTER_REFERENCE(72, /* C s */ 'c' _ 'r' _ ';', 3, 0, 0xd835 _ 0xdc9e)
NAMED_CHARACTER_REFERENCE(73, /* C u */ 'p' _ ';', 2, 0, 0x22d3 _ 0)
NAMED_CHARACTER_REFERENCE(74,
                          /* C u */ 'p' _ 'C' _ 'a' _ 'p' _ ';', 5, 0,
                          0x224d _ 0)
NAMED_CHARACTER_REFERENCE(75, /* D D */ ';', 1, 0, 0x2145 _ 0)
NAMED_CHARACTER_REFERENCE(76,
                          /* D D */ 'o' _ 't' _ 'r' _ 'a' _ 'h' _ 'd' _ ';', 7,
                          0, 0x2911 _ 0)
NAMED_CHARACTER_REFERENCE(77, /* D J */ 'c' _ 'y' _ ';', 3, 0, 0x0402 _ 0)
NAMED_CHARACTER_REFERENCE(78, /* D S */ 'c' _ 'y' _ ';', 3, 0, 0x0405 _ 0)
NAMED_CHARACTER_REFERENCE(79, /* D Z */ 'c' _ 'y' _ ';', 3, 0, 0x040f _ 0)
NAMED_CHARACTER_REFERENCE(80,
                          /* D a */ 'g' _ 'g' _ 'e' _ 'r' _ ';', 5, 0,
                          0x2021 _ 0)
NAMED_CHARACTER_REFERENCE(81, /* D a */ 'r' _ 'r' _ ';', 3, 0, 0x21a1 _ 0)
NAMED_CHARACTER_REFERENCE(82, /* D a */ 's' _ 'h' _ 'v' _ ';', 4, 0, 0x2ae4 _ 0)
NAMED_CHARACTER_REFERENCE(83,
                          /* D c */ 'a' _ 'r' _ 'o' _ 'n' _ ';', 5, 0,
                          0x010e _ 0)
NAMED_CHARACTER_REFERENCE(84, /* D c */ 'y' _ ';', 2, 0, 0x0414 _ 0)
NAMED_CHARACTER_REFERENCE(85, /* D e */ 'l' _ ';', 2, 0, 0x2207 _ 0)
NAMED_CHARACTER_REFERENCE(86, /* D e */ 'l' _ 't' _ 'a' _ ';', 4, 0, 0x0394 _ 0)
NAMED_CHARACTER_REFERENCE(87, /* D f */ 'r' _ ';', 2, 0, 0xd835 _ 0xdd07)
NAMED_CHARACTER_REFERENCE(
    88,
    /* D i */
    'a' _ 'c' _ 'r' _ 'i' _ 't' _ 'i' _ 'c' _ 'a' _ 'l' _ 'A' _ 'c' _ 'u' _ 't' _ 'e' _ ';',
    15, 0, 0x00b4 _ 0)
NAMED_CHARACTER_REFERENCE(
    89,
    /* D i */
    'a' _ 'c' _ 'r' _ 'i' _ 't' _ 'i' _ 'c' _ 'a' _ 'l' _ 'D' _ 'o' _ 't' _ ';',
    13, 0, 0x02d9 _ 0)
NAMED_CHARACTER_REFERENCE(
    90,
    /* D i */
    'a' _ 'c' _ 'r' _ 'i' _ 't' _ 'i' _ 'c' _ 'a' _ 'l' _ 'D' _ 'o' _ 'u' _ 'b' _ 'l' _ 'e' _ 'A' _ 'c' _ 'u' _ 't' _ 'e' _ ';',
    21, 0, 0x02dd _ 0)
NAMED_CHARACTER_REFERENCE(
    91,
    /* D i */
    'a' _ 'c' _ 'r' _ 'i' _ 't' _ 'i' _ 'c' _ 'a' _ 'l' _ 'G' _ 'r' _ 'a' _ 'v' _ 'e' _ ';',
    15, 0, 0x0060 _ 0)
NAMED_CHARACTER_REFERENCE(
    92,
    /* D i */
    'a' _ 'c' _ 'r' _ 'i' _ 't' _ 'i' _ 'c' _ 'a' _ 'l' _ 'T' _ 'i' _ 'l' _ 'd' _ 'e' _ ';',
    15, 0, 0x02dc _ 0)
NAMED_CHARACTER_REFERENCE(93,
                          /* D i */ 'a' _ 'm' _ 'o' _ 'n' _ 'd' _ ';', 6, 0,
                          0x22c4 _ 0)
NAMED_CHARACTER_REFERENCE(
    94,
    /* D i */
    'f' _ 'f' _ 'e' _ 'r' _ 'e' _ 'n' _ 't' _ 'i' _ 'a' _ 'l' _ 'D' _ ';', 12,
    0, 0x2146 _ 0)
NAMED_CHARACTER_REFERENCE(95, /* D o */ 'p' _ 'f' _ ';', 3, 0, 0xd835 _ 0xdd3b)
NAMED_CHARACTER_REFERENCE(96, /* D o */ 't' _ ';', 2, 0, 0x00a8 _ 0)
NAMED_CHARACTER_REFERENCE(97,
                          /* D o */ 't' _ 'D' _ 'o' _ 't' _ ';', 5, 0,
                          0x20dc _ 0)
NAMED_CHARACTER_REFERENCE(98,
                          /* D o */ 't' _ 'E' _ 'q' _ 'u' _ 'a' _ 'l' _ ';', 7,
                          0, 0x2250 _ 0)
NAMED_CHARACTER_REFERENCE(
    99,
    /* D o */
    'u' _ 'b' _ 'l' _ 'e' _ 'C' _ 'o' _ 'n' _ 't' _ 'o' _ 'u' _ 'r' _ 'I' _ 'n' _ 't' _ 'e' _ 'g' _ 'r' _ 'a' _ 'l' _ ';',
    20, 0, 0x222f _ 0)
NAMED_CHARACTER_REFERENCE(
    100,
    /* D o */ 'u' _ 'b' _ 'l' _ 'e' _ 'D' _ 'o' _ 't' _ ';', 8, 0, 0x00a8 _ 0)
NAMED_CHARACTER_REFERENCE(
    101,
    /* D o */
    'u' _ 'b' _ 'l' _ 'e' _ 'D' _ 'o' _ 'w' _ 'n' _ 'A' _ 'r' _ 'r' _ 'o' _ 'w' _ ';',
    14, 0, 0x21d3 _ 0)
NAMED_CHARACTER_REFERENCE(
    102,
    /* D o */
    'u' _ 'b' _ 'l' _ 'e' _ 'L' _ 'e' _ 'f' _ 't' _ 'A' _ 'r' _ 'r' _ 'o' _ 'w' _ ';',
    14, 0, 0x21d0 _ 0)
NAMED_CHARACTER_REFERENCE(
    103,
    /* D o */
    'u' _ 'b' _ 'l' _ 'e' _ 'L' _ 'e' _ 'f' _ 't' _ 'R' _ 'i' _ 'g' _ 'h' _ 't' _ 'A' _ 'r' _ 'r' _ 'o' _ 'w' _ ';',
    19, 0, 0x21d4 _ 0)
NAMED_CHARACTER_REFERENCE(
    104,
    /* D o */
    'u' _ 'b' _ 'l' _ 'e' _ 'L' _ 'e' _ 'f' _ 't' _ 'T' _ 'e' _ 'e' _ ';', 12,
    0, 0x2ae4 _ 0)
NAMED_CHARACTER_REFERENCE(
    105,
    /* D o */
    'u' _ 'b' _ 'l' _ 'e' _ 'L' _ 'o' _ 'n' _ 'g' _ 'L' _ 'e' _ 'f' _ 't' _ 'A' _ 'r' _ 'r' _ 'o' _ 'w' _ ';',
    18, 0, 0x27f8 _ 0)
NAMED_CHARACTER_REFERENCE(
    106,
    /* D o */
    'u' _ 'b' _ 'l' _ 'e' _ 'L' _ 'o' _ 'n' _ 'g' _ 'L' _ 'e' _ 'f' _ 't' _ 'R' _ 'i' _ 'g' _ 'h' _ 't' _ 'A' _ 'r' _ 'r' _ 'o' _ 'w' _ ';',
    23, 0, 0x27fa _ 0)
NAMED_CHARACTER_REFERENCE(
    107,
    /* D o */
    'u' _ 'b' _ 'l' _ 'e' _ 'L' _ 'o' _ 'n' _ 'g' _ 'R' _ 'i' _ 'g' _ 'h' _ 't' _ 'A' _ 'r' _ 'r' _ 'o' _ 'w' _ ';',
    19, 0, 0x27f9 _ 0)
NAMED_CHARACTER_REFERENCE(
    108,
    /* D o */
    'u' _ 'b' _ 'l' _ 'e' _ 'R' _ 'i' _ 'g' _ 'h' _ 't' _ 'A' _ 'r' _ 'r' _ 'o' _ 'w' _ ';',
    15, 0, 0x21d2 _ 0)
NAMED_CHARACTER_REFERENCE(
    109,
    /* D o */
    'u' _ 'b' _ 'l' _ 'e' _ 'R' _ 'i' _ 'g' _ 'h' _ 't' _ 'T' _ 'e' _ 'e' _ ';',
    13, 0, 0x22a8 _ 0)
NAMED_CHARACTER_REFERENCE(
    110,
    /* D o */
    'u' _ 'b' _ 'l' _ 'e' _ 'U' _ 'p' _ 'A' _ 'r' _ 'r' _ 'o' _ 'w' _ ';', 12,
    0, 0x21d1 _ 0)
NAMED_CHARACTER_REFERENCE(
    111,
    /* D o */
    'u' _ 'b' _ 'l' _ 'e' _ 'U' _ 'p' _ 'D' _ 'o' _ 'w' _ 'n' _ 'A' _ 'r' _ 'r' _ 'o' _ 'w' _ ';',
    16, 0, 0x21d5 _ 0)
NAMED_CHARACTER_REFERENCE(
    112,
    /* D o */
    'u' _ 'b' _ 'l' _ 'e' _ 'V' _ 'e' _ 'r' _ 't' _ 'i' _ 'c' _ 'a' _ 'l' _ 'B' _ 'a' _ 'r' _ ';',
    16, 0, 0x2225 _ 0)
NAMED_CHARACTER_REFERENCE(
    113,
    /* D o */ 'w' _ 'n' _ 'A' _ 'r' _ 'r' _ 'o' _ 'w' _ ';', 8, 0, 0x2193 _ 0)
NAMED_CHARACTER_REFERENCE(
    114,
    /* D o */ 'w' _ 'n' _ 'A' _ 'r' _ 'r' _ 'o' _ 'w' _ 'B' _ 'a' _ 'r' _ ';',
    11, 0, 0x2913 _ 0)
NAMED_CHARACTER_REFERENCE(
    115,
    /* D o */
    'w' _ 'n' _ 'A' _ 'r' _ 'r' _ 'o' _ 'w' _ 'U' _ 'p' _ 'A' _ 'r' _ 'r' _ 'o' _ 'w' _ ';',
    15, 0, 0x21f5 _ 0)
NAMED_CHARACTER_REFERENCE(
    116,
    /* D o */ 'w' _ 'n' _ 'B' _ 'r' _ 'e' _ 'v' _ 'e' _ ';', 8, 0, 0x0311 _ 0)
NAMED_CHARACTER_REFERENCE(
    117,
    /* D o */
    'w' _ 'n' _ 'L' _ 'e' _ 'f' _ 't' _ 'R' _ 'i' _ 'g' _ 'h' _ 't' _ 'V' _ 'e' _ 'c' _ 't' _ 'o' _ 'r' _ ';',
    18, 0, 0x2950 _ 0)
NAMED_CHARACTER_REFERENCE(
    118,
    /* D o */
    'w' _ 'n' _ 'L' _ 'e' _ 'f' _ 't' _ 'T' _ 'e' _ 'e' _ 'V' _ 'e' _ 'c' _ 't' _ 'o' _ 'r' _ ';',
    16, 0, 0x295e _ 0)
NAMED_CHARACTER_REFERENCE(
    119,
    /* D o */
    'w' _ 'n' _ 'L' _ 'e' _ 'f' _ 't' _ 'V' _ 'e' _ 'c' _ 't' _ 'o' _ 'r' _ ';',
    13, 0, 0x21bd _ 0)
NAMED_CHARACTER_REFERENCE(
    120,
    /* D o */
    'w' _ 'n' _ 'L' _ 'e' _ 'f' _ 't' _ 'V' _ 'e' _ 'c' _ 't' _ 'o' _ 'r' _ 'B' _ 'a' _ 'r' _ ';',
    16, 0, 0x2956 _ 0)
NAMED_CHARACTER_REFERENCE(
    121,
    /* D o */
    'w' _ 'n' _ 'R' _ 'i' _ 'g' _ 'h' _ 't' _ 'T' _ 'e' _ 'e' _ 'V' _ 'e' _ 'c' _ 't' _ 'o' _ 'r' _ ';',
    17, 0, 0x295f _ 0)
NAMED_CHARACTER_REFERENCE(
    122,
    /* D o */
    'w' _ 'n' _ 'R' _ 'i' _ 'g' _ 'h' _ 't' _ 'V' _ 'e' _ 'c' _ 't' _ 'o' _ 'r' _ ';',
    14, 0, 0x21c1 _ 0)
NAMED_CHARACTER_REFERENCE(
    123,
    /* D o */
    'w' _ 'n' _ 'R' _ 'i' _ 'g' _ 'h' _ 't' _ 'V' _ 'e' _ 'c' _ 't' _ 'o' _ 'r' _ 'B' _ 'a' _ 'r' _ ';',
    17, 0, 0x2957 _ 0)
NAMED_CHARACTER_REFERENCE(124,
                          /* D o */ 'w' _ 'n' _ 'T' _ 'e' _ 'e' _ ';', 6, 0,
                          0x22a4 _ 0)
NAMED_CHARACTER_REFERENCE(
    125,
    /* D o */ 'w' _ 'n' _ 'T' _ 'e' _ 'e' _ 'A' _ 'r' _ 'r' _ 'o' _ 'w' _ ';',
    11, 0, 0x21a7 _ 0)
NAMED_CHARACTER_REFERENCE(
    126,
    /* D o */ 'w' _ 'n' _ 'a' _ 'r' _ 'r' _ 'o' _ 'w' _ ';', 8, 0, 0x21d3 _ 0)
NAMED_CHARACTER_REFERENCE(127, /* D s */ 'c' _ 'r' _ ';', 3, 0, 0xd835 _ 0xdc9f)
NAMED_CHARACTER_REFERENCE(128,
                          /* D s */ 't' _ 'r' _ 'o' _ 'k' _ ';', 5, 0,
                          0x0110 _ 0)
NAMED_CHARACTER_REFERENCE(129, /* E N */ 'G' _ ';', 2, 0, 0x014a _ 0)
NAMED_CHARACTER_REFERENCE(130, /* E T */ 'H', 1, 0, 0x00d0 _ 0)
NAMED_CHARACTER_REFERENCE(131, /* E T */ 'H' _ ';', 2, 0, 0x00d0 _ 0)
NAMED_CHARACTER_REFERENCE(132,
                          /* E a */ 'c' _ 'u' _ 't' _ 'e', 4, 0, 0x00c9 _ 0)
NAMED_CHARACTER_REFERENCE(133,
                          /* E a */ 'c' _ 'u' _ 't' _ 'e' _ ';', 5, 0,
                          0x00c9 _ 0)
NAMED_CHARACTER_REFERENCE(134,
                          /* E c */ 'a' _ 'r' _ 'o' _ 'n' _ ';', 5, 0,
                          0x011a _ 0)
NAMED_CHARACTER_REFERENCE(135, /* E c */ 'i' _ 'r' _ 'c', 3, 0, 0x00ca _ 0)
NAMED_CHARACTER_REFERENCE(136,
                          /* E c */ 'i' _ 'r' _ 'c' _ ';', 4, 0, 0x00ca _ 0)
NAMED_CHARACTER_REFERENCE(137, /* E c */ 'y' _ ';', 2, 0, 0x042d _ 0)
NAMED_CHARACTER_REFERENCE(138, /* E d */ 'o' _ 't' _ ';', 3, 0, 0x0116 _ 0)
NAMED_CHARACTER_REFERENCE(139, /* E f */ 'r' _ ';', 2, 0, 0xd835 _ 0xdd08)
NAMED_CHARACTER_REFERENCE(140,
                          /* E g */ 'r' _ 'a' _ 'v' _ 'e', 4, 0, 0x00c8 _ 0)
NAMED_CHARACTER_REFERENCE(141,
                          /* E g */ 'r' _ 'a' _ 'v' _ 'e' _ ';', 5, 0,
                          0x00c8 _ 0)
NAMED_CHARACTER_REFERENCE(142,
                          /* E l */ 'e' _ 'm' _ 'e' _ 'n' _ 't' _ ';', 6, 0,
                          0x2208 _ 0)
NAMED_CHARACTER_REFERENCE(143,
                          /* E m */ 'a' _ 'c' _ 'r' _ ';', 4, 0, 0x0112 _ 0)
NAMED_CHARACTER_REFERENCE(
    144,
    /* E m */
    'p' _ 't' _ 'y' _ 'S' _ 'm' _ 'a' _ 'l' _ 'l' _ 'S' _ 'q' _ 'u' _ 'a' _ 'r' _ 'e' _ ';',
    15, 0, 0x25fb _ 0)
NAMED_CHARACTER_REFERENCE(
    145,
    /* E m */
    'p' _ 't' _ 'y' _ 'V' _ 'e' _ 'r' _ 'y' _ 'S' _ 'm' _ 'a' _ 'l' _ 'l' _ 'S' _ 'q' _ 'u' _ 'a' _ 'r' _ 'e' _ ';',
    19, 0, 0x25ab _ 0)
NAMED_CHARACTER_REFERENCE(146,
                          /* E o */ 'g' _ 'o' _ 'n' _ ';', 4, 0, 0x0118 _ 0)
NAMED_CHARACTER_REFERENCE(147, /* E o */ 'p' _ 'f' _ ';', 3, 0, 0xd835 _ 0xdd3c)
NAMED_CHARACTER_REFERENCE(148,
                          /* E p */ 's' _ 'i' _ 'l' _ 'o' _ 'n' _ ';', 6, 0,
                          0x0395 _ 0)
NAMED_CHARACTER_REFERENCE(149,
                          /* E q */ 'u' _ 'a' _ 'l' _ ';', 4, 0, 0x2a75 _ 0)
NAMED_CHARACTER_REFERENCE(
    150,
    /* E q */ 'u' _ 'a' _ 'l' _ 'T' _ 'i' _ 'l' _ 'd' _ 'e' _ ';', 9, 0,
    0x2242 _ 0)
NAMED_CHARACTER_REFERENCE(
    151,
    /* E q */ 'u' _ 'i' _ 'l' _ 'i' _ 'b' _ 'r' _ 'i' _ 'u' _ 'm' _ ';', 10, 0,
    0x21cc _ 0)
NAMED_CHARACTER_REFERENCE(152, /* E s */ 'c' _ 'r' _ ';', 3, 0, 0x2130 _ 0)
NAMED_CHARACTER_REFERENCE(153, /* E s */ 'i' _ 'm' _ ';', 3, 0, 0x2a73 _ 0)
NAMED_CHARACTER_REFERENCE(154, /* E t */ 'a' _ ';', 2, 0, 0x0397 _ 0)
NAMED_CHARACTER_REFERENCE(155, /* E u */ 'm' _ 'l', 2, 0, 0x00cb _ 0)
NAMED_CHARACTER_REFERENCE(156, /* E u */ 'm' _ 'l' _ ';', 3, 0, 0x00cb _ 0)
NAMED_CHARACTER_REFERENCE(157,
                          /* E x */ 'i' _ 's' _ 't' _ 's' _ ';', 5, 0,
                          0x2203 _ 0)
NAMED_CHARACTER_REFERENCE(
    158,
    /* E x */ 'p' _ 'o' _ 'n' _ 'e' _ 'n' _ 't' _ 'i' _ 'a' _ 'l' _ 'E' _ ';',
    11, 0, 0x2147 _ 0)
NAMED_CHARACTER_REFERENCE(159, /* F c */ 'y' _ ';', 2, 0, 0x0424 _ 0)
NAMED_CHARACTER_REFERENCE(160, /* F f */ 'r' _ ';', 2, 0, 0xd835 _ 0xdd09)
NAMED_CHARACTER_REFERENCE(
    161,
    /* F i */
    'l' _ 'l' _ 'e' _ 'd' _ 'S' _ 'm' _ 'a' _ 'l' _ 'l' _ 'S' _ 'q' _ 'u' _ 'a' _ 'r' _ 'e' _ ';',
    16, 0, 0x25fc _ 0)
NAMED_CHARACTER_REFERENCE(
    162,
    /* F i */
    'l' _ 'l' _ 'e' _ 'd' _ 'V' _ 'e' _ 'r' _ 'y' _ 'S' _ 'm' _ 'a' _ 'l' _ 'l' _ 'S' _ 'q' _ 'u' _ 'a' _ 'r' _ 'e' _ ';',
    20, 0, 0x25aa _ 0)
NAMED_CHARACTER_REFERENCE(163, /* F o */ 'p' _ 'f' _ ';', 3, 0, 0xd835 _ 0xdd3d)
NAMED_CHARACTER_REFERENCE(164,
                          /* F o */ 'r' _ 'A' _ 'l' _ 'l' _ ';', 5, 0,
                          0x2200 _ 0)
NAMED_CHARACTER_REFERENCE(
    165,
    /* F o */ 'u' _ 'r' _ 'i' _ 'e' _ 'r' _ 't' _ 'r' _ 'f' _ ';', 9, 0,
    0x2131 _ 0)
NAMED_CHARACTER_REFERENCE(166, /* F s */ 'c' _ 'r' _ ';', 3, 0, 0x2131 _ 0)
NAMED_CHARACTER_REFERENCE(167, /* G J */ 'c' _ 'y' _ ';', 3, 0, 0x0403 _ 0)
NAMED_CHARACTER_REFERENCE(168, /* G T */ 0, 0, 1, 0x003e _ 0)
NAMED_CHARACTER_REFERENCE(169, /* G T */ ';', 1, 0, 0x003e _ 0)
NAMED_CHARACTER_REFERENCE(170,
                          /* G a */ 'm' _ 'm' _ 'a' _ ';', 4, 0, 0x0393 _ 0)
NAMED_CHARACTER_REFERENCE(171,
                          /* G a */ 'm' _ 'm' _ 'a' _ 'd' _ ';', 5, 0,
                          0x03dc _ 0)
NAMED_CHARACTER_REFERENCE(172,
                          /* G b */ 'r' _ 'e' _ 'v' _ 'e' _ ';', 5, 0,
                          0x011e _ 0)
NAMED_CHARACTER_REFERENCE(173,
                          /* G c */ 'e' _ 'd' _ 'i' _ 'l' _ ';', 5, 0,
                          0x0122 _ 0)
NAMED_CHARACTER_REFERENCE(174,
                          /* G c */ 'i' _ 'r' _ 'c' _ ';', 4, 0, 0x011c _ 0)
NAMED_CHARACTER_REFERENCE(175, /* G c */ 'y' _ ';', 2, 0, 0x0413 _ 0)
NAMED_CHARACTER_REFERENCE(176, /* G d */ 'o' _ 't' _ ';', 3, 0, 0x0120 _ 0)
NAMED_CHARACTER_REFERENCE(177, /* G f */ 'r' _ ';', 2, 0, 0xd835 _ 0xdd0a)
NAMED_CHARACTER_REFERENCE(178, /* G g */ ';', 1, 0, 0x22d9 _ 0)
NAMED_CHARACTER_REFERENCE(179, /* G o */ 'p' _ 'f' _ ';', 3, 0, 0xd835 _ 0xdd3e)
NAMED_CHARACTER_REFERENCE(
    180,
    /* G r */ 'e' _ 'a' _ 't' _ 'e' _ 'r' _ 'E' _ 'q' _ 'u' _ 'a' _ 'l' _ ';',
    11, 0, 0x2265 _ 0)
NAMED_CHARACTER_REFERENCE(
    181,
    /* G r */
    'e' _ 'a' _ 't' _ 'e' _ 'r' _ 'E' _ 'q' _ 'u' _ 'a' _ 'l' _ 'L' _ 'e' _ 's' _ 's' _ ';',
    15, 0, 0x22db _ 0)
NAMED_CHARACTER_REFERENCE(
    182,
    /* G r */
    'e' _ 'a' _ 't' _ 'e' _ 'r' _ 'F' _ 'u' _ 'l' _ 'l' _ 'E' _ 'q' _ 'u' _ 'a' _ 'l' _ ';',
    15, 0, 0x2267 _ 0)
NAMED_CHARACTER_REFERENCE(
    183,
    /* G r */
    'e' _ 'a' _ 't' _ 'e' _ 'r' _ 'G' _ 'r' _ 'e' _ 'a' _ 't' _ 'e' _ 'r' _ ';',
    13, 0, 0x2aa2 _ 0)
NAMED_CHARACTER_REFERENCE(
    184,
    /* G r */ 'e' _ 'a' _ 't' _ 'e' _ 'r' _ 'L' _ 'e' _ 's' _ 's' _ ';', 10, 0,
    0x2277 _ 0)
NAMED_CHARACTER_REFERENCE(
    185,
    /* G r */
    'e' _ 'a' _ 't' _ 'e' _ 'r' _ 'S' _ 'l' _ 'a' _ 'n' _ 't' _ 'E' _ 'q' _ 'u' _ 'a' _ 'l' _ ';',
    16, 0, 0x2a7e _ 0)
NAMED_CHARACTER_REFERENCE(
    186,
    /* G r */ 'e' _ 'a' _ 't' _ 'e' _ 'r' _ 'T' _ 'i' _ 'l' _ 'd' _ 'e' _ ';',
    11, 0, 0x2273 _ 0)
NAMED_CHARACTER_REFERENCE(187, /* G s */ 'c' _ 'r' _ ';', 3, 0, 0xd835 _ 0xdca2)
NAMED_CHARACTER_REFERENCE(188, /* G t */ ';', 1, 0, 0x226b _ 0)
NAMED_CHARACTER_REFERENCE(189,
                          /* H A */ 'R' _ 'D' _ 'c' _ 'y' _ ';', 5, 0,
                          0x042a _ 0)
NAMED_CHARACTER_REFERENCE(190,
                          /* H a */ 'c' _ 'e' _ 'k' _ ';', 4, 0, 0x02c7 _ 0)
NAMED_CHARACTER_REFERENCE(191, /* H a */ 't' _ ';', 2, 0, 0x005e _ 0)
NAMED_CHARACTER_REFERENCE(192,
                          /* H c */ 'i' _ 'r' _ 'c' _ ';', 4, 0, 0x0124 _ 0)
NAMED_CHARACTER_REFERENCE(193, /* H f */ 'r' _ ';', 2, 0, 0x210c _ 0)
NAMED_CHARACTER_REFERENCE(
    194,
    /* H i */ 'l' _ 'b' _ 'e' _ 'r' _ 't' _ 'S' _ 'p' _ 'a' _ 'c' _ 'e' _ ';',
    11, 0, 0x210b _ 0)
NAMED_CHARACTER_REFERENCE(195, /* H o */ 'p' _ 'f' _ ';', 3, 0, 0x210d _ 0)
NAMED_CHARACTER_REFERENCE(
    196,
    /* H o */
    'r' _ 'i' _ 'z' _ 'o' _ 'n' _ 't' _ 'a' _ 'l' _ 'L' _ 'i' _ 'n' _ 'e' _ ';',
    13, 0, 0x2500 _ 0)
NAMED_CHARACTER_REFERENCE(197, /* H s */ 'c' _ 'r' _ ';', 3, 0, 0x210b _ 0)
NAMED_CHARACTER_REFERENCE(198,
                          /* H s */ 't' _ 'r' _ 'o' _ 'k' _ ';', 5, 0,
                          0x0126 _ 0)
NAMED_CHARACTER_REFERENCE(
    199,
    /* H u */ 'm' _ 'p' _ 'D' _ 'o' _ 'w' _ 'n' _ 'H' _ 'u' _ 'm' _ 'p' _ ';',
    11, 0, 0x224e _ 0)
NAMED_CHARACTER_REFERENCE(
    200,
    /* H u */ 'm' _ 'p' _ 'E' _ 'q' _ 'u' _ 'a' _ 'l' _ ';', 8, 0, 0x224f _ 0)
NAMED_CHARACTER_REFERENCE(201, /* I E */ 'c' _ 'y' _ ';', 3, 0, 0x0415 _ 0)
NAMED_CHARACTER_REFERENCE(202,
                          /* I J */ 'l' _ 'i' _ 'g' _ ';', 4, 0, 0x0132 _ 0)
NAMED_CHARACTER_REFERENCE(203, /* I O */ 'c' _ 'y' _ ';', 3, 0, 0x0401 _ 0)
NAMED_CHARACTER_REFERENCE(204,
                          /* I a */ 'c' _ 'u' _ 't' _ 'e', 4, 0, 0x00cd _ 0)
NAMED_CHARACTER_REFERENCE(205,
                          /* I a */ 'c' _ 'u' _ 't' _ 'e' _ ';', 5, 0,
                          0x00cd _ 0)
NAMED_CHARACTER_REFERENCE(206, /* I c */ 'i' _ 'r' _ 'c', 3, 0, 0x00ce _ 0)
NAMED_CHARACTER_REFERENCE(207,
                          /* I c */ 'i' _ 'r' _ 'c' _ ';', 4, 0, 0x00ce _ 0)
NAMED_CHARACTER_REFERENCE(208, /* I c */ 'y' _ ';', 2, 0, 0x0418 _ 0)
NAMED_CHARACTER_REFERENCE(209, /* I d */ 'o' _ 't' _ ';', 3, 0, 0x0130 _ 0)
NAMED_CHARACTER_REFERENCE(210, /* I f */ 'r' _ ';', 2, 0, 0x2111 _ 0)
NAMED_CHARACTER_REFERENCE(211,
                          /* I g */ 'r' _ 'a' _ 'v' _ 'e', 4, 0, 0x00cc _ 0)
NAMED_CHARACTER_REFERENCE(212,
                          /* I g */ 'r' _ 'a' _ 'v' _ 'e' _ ';', 5, 0,
                          0x00cc _ 0)
NAMED_CHARACTER_REFERENCE(213, /* I m */ ';', 1, 0, 0x2111 _ 0)
NAMED_CHARACTER_REFERENCE(214,
                          /* I m */ 'a' _ 'c' _ 'r' _ ';', 4, 0, 0x012a _ 0)
NAMED_CHARACTER_REFERENCE(
    215,
    /* I m */ 'a' _ 'g' _ 'i' _ 'n' _ 'a' _ 'r' _ 'y' _ 'I' _ ';', 9, 0,
    0x2148 _ 0)
NAMED_CHARACTER_REFERENCE(216,
                          /* I m */ 'p' _ 'l' _ 'i' _ 'e' _ 's' _ ';', 6, 0,
                          0x21d2 _ 0)
NAMED_CHARACTER_REFERENCE(217, /* I n */ 't' _ ';', 2, 0, 0x222c _ 0)
NAMED_CHARACTER_REFERENCE(218,
                          /* I n */ 't' _ 'e' _ 'g' _ 'r' _ 'a' _ 'l' _ ';', 7,
                          0, 0x222b _ 0)
NAMED_CHARACTER_REFERENCE(
    219,
    /* I n */ 't' _ 'e' _ 'r' _ 's' _ 'e' _ 'c' _ 't' _ 'i' _ 'o' _ 'n' _ ';',
    11, 0, 0x22c2 _ 0)
NAMED_CHARACTER_REFERENCE(
    220,
    /* I n */
    'v' _ 'i' _ 's' _ 'i' _ 'b' _ 'l' _ 'e' _ 'C' _ 'o' _ 'm' _ 'm' _ 'a' _ ';',
    13, 0, 0x2063 _ 0)
NAMED_CHARACTER_REFERENCE(
    221,
    /* I n */
    'v' _ 'i' _ 's' _ 'i' _ 'b' _ 'l' _ 'e' _ 'T' _ 'i' _ 'm' _ 'e' _ 's' _ ';',
    13, 0, 0x2062 _ 0)
NAMED_CHARACTER_REFERENCE(222,
                          /* I o */ 'g' _ 'o' _ 'n' _ ';', 4, 0, 0x012e _ 0)
NAMED_CHARACTER_REFERENCE(223, /* I o */ 'p' _ 'f' _ ';', 3, 0, 0xd835 _ 0xdd40)
NAMED_CHARACTER_REFERENCE(224, /* I o */ 't' _ 'a' _ ';', 3, 0, 0x0399 _ 0)
NAMED_CHARACTER_REFERENCE(225, /* I s */ 'c' _ 'r' _ ';', 3, 0, 0x2110 _ 0)
NAMED_CHARACTER_REFERENCE(226,
                          /* I t */ 'i' _ 'l' _ 'd' _ 'e' _ ';', 5, 0,
                          0x0128 _ 0)
NAMED_CHARACTER_REFERENCE(227,
                          /* I u */ 'k' _ 'c' _ 'y' _ ';', 4, 0, 0x0406 _ 0)
NAMED_CHARACTER_REFERENCE(228, /* I u */ 'm' _ 'l', 2, 0, 0x00cf _ 0)
NAMED_CHARACTER_REFERENCE(229, /* I u */ 'm' _ 'l' _ ';', 3, 0, 0x00cf _ 0)
NAMED_CHARACTER_REFERENCE(230,
                          /* J c */ 'i' _ 'r' _ 'c' _ ';', 4, 0, 0x0134 _ 0)
NAMED_CHARACTER_REFERENCE(231, /* J c */ 'y' _ ';', 2, 0, 0x0419 _ 0)
NAMED_CHARACTER_REFERENCE(232, /* J f */ 'r' _ ';', 2, 0, 0xd835 _ 0xdd0d)
NAMED_CHARACTER_REFERENCE(233, /* J o */ 'p' _ 'f' _ ';', 3, 0, 0xd835 _ 0xdd41)
NAMED_CHARACTER_REFERENCE(234, /* J s */ 'c' _ 'r' _ ';', 3, 0, 0xd835 _ 0xdca5)
NAMED_CHARACTER_REFERENCE(235,
                          /* J s */ 'e' _ 'r' _ 'c' _ 'y' _ ';', 5, 0,
                          0x0408 _ 0)
NAMED_CHARACTER_REFERENCE(236,
                          /* J u */ 'k' _ 'c' _ 'y' _ ';', 4, 0, 0x0404 _ 0)
NAMED_CHARACTER_REFERENCE(237, /* K H */ 'c' _ 'y' _ ';', 3, 0, 0x0425 _ 0)
NAMED_CHARACTER_REFERENCE(238, /* K J */ 'c' _ 'y' _ ';', 3, 0, 0x040c _ 0)
NAMED_CHARACTER_REFERENCE(239,
                          /* K a */ 'p' _ 'p' _ 'a' _ ';', 4, 0, 0x039a _ 0)
NAMED_CHARACTER_REFERENCE(240,
                          /* K c */ 'e' _ 'd' _ 'i' _ 'l' _ ';', 5, 0,
                          0x0136 _ 0)
NAMED_CHARACTER_REFERENCE(241, /* K c */ 'y' _ ';', 2, 0, 0x041a _ 0)
NAMED_CHARACTER_REFERENCE(242, /* K f */ 'r' _ ';', 2, 0, 0xd835 _ 0xdd0e)
NAMED_CHARACTER_REFERENCE(243, /* K o */ 'p' _ 'f' _ ';', 3, 0, 0xd835 _ 0xdd42)
NAMED_CHARACTER_REFERENCE(244, /* K s */ 'c' _ 'r' _ ';', 3, 0, 0xd835 _ 0xdca6)
NAMED_CHARACTER_REFERENCE(245, /* L J */ 'c' _ 'y' _ ';', 3, 0, 0x0409 _ 0)
NAMED_CHARACTER_REFERENCE(246, /* L T */ 0, 0, 1, 0x003c _ 0)
NAMED_CHARACTER_REFERENCE(247, /* L T */ ';', 1, 0, 0x003c _ 0)
NAMED_CHARACTER_REFERENCE(248,
                          /* L a */ 'c' _ 'u' _ 't' _ 'e' _ ';', 5, 0,
                          0x0139 _ 0)
NAMED_CHARACTER_REFERENCE(249,
                          /* L a */ 'm' _ 'b' _ 'd' _ 'a' _ ';', 5, 0,
                          0x039b _ 0)
NAMED_CHARACTER_REFERENCE(250, /* L a */ 'n' _ 'g' _ ';', 3, 0, 0x27ea _ 0)
NAMED_CHARACTER_REFERENCE(
    251,
    /* L a */ 'p' _ 'l' _ 'a' _ 'c' _ 'e' _ 't' _ 'r' _ 'f' _ ';', 9, 0,
    0x2112 _ 0)
NAMED_CHARACTER_REFERENCE(252, /* L a */ 'r' _ 'r' _ ';', 3, 0, 0x219e _ 0)
NAMED_CHARACTER_REFERENCE(253,
                          /* L c */ 'a' _ 'r' _ 'o' _ 'n' _ ';', 5, 0,
                          0x013d _ 0)
NAMED_CHARACTER_REFERENCE(254,
                          /* L c */ 'e' _ 'd' _ 'i' _ 'l' _ ';', 5, 0,
                          0x013b _ 0)
NAMED_CHARACTER_REFERENCE(255, /* L c */ 'y' _ ';', 2, 0, 0x041b _ 0)
NAMED_CHARACTER_REFERENCE(
    256,
    /* L e */
    'f' _ 't' _ 'A' _ 'n' _ 'g' _ 'l' _ 'e' _ 'B' _ 'r' _ 'a' _ 'c' _ 'k' _ 'e' _ 't' _ ';',
    15, 0, 0x27e8 _ 0)
NAMED_CHARACTER_REFERENCE(
    257,
    /* L e */ 'f' _ 't' _ 'A' _ 'r' _ 'r' _ 'o' _ 'w' _ ';', 8, 0, 0x2190 _ 0)
NAMED_CHARACTER_REFERENCE(
    258,
    /* L e */ 'f' _ 't' _ 'A' _ 'r' _ 'r' _ 'o' _ 'w' _ 'B' _ 'a' _ 'r' _ ';',
    11, 0, 0x21e4 _ 0)
NAMED_CHARACTER_REFERENCE(
    259,
    /* L e */
    'f' _ 't' _ 'A' _ 'r' _ 'r' _ 'o' _ 'w' _ 'R' _ 'i' _ 'g' _ 'h' _ 't' _ 'A' _ 'r' _ 'r' _ 'o' _ 'w' _ ';',
    18, 0, 0x21c6 _ 0)
NAMED_CHARACTER_REFERENCE(
    260,
    /* L e */ 'f' _ 't' _ 'C' _ 'e' _ 'i' _ 'l' _ 'i' _ 'n' _ 'g' _ ';', 10, 0,
    0x2308 _ 0)
NAMED_CHARACTER_REFERENCE(
    261,
    /* L e */
    'f' _ 't' _ 'D' _ 'o' _ 'u' _ 'b' _ 'l' _ 'e' _ 'B' _ 'r' _ 'a' _ 'c' _ 'k' _ 'e' _ 't' _ ';',
    16, 0, 0x27e6 _ 0)
NAMED_CHARACTER_REFERENCE(
    262,
    /* L e */
    'f' _ 't' _ 'D' _ 'o' _ 'w' _ 'n' _ 'T' _ 'e' _ 'e' _ 'V' _ 'e' _ 'c' _ 't' _ 'o' _ 'r' _ ';',
    16, 0, 0x2961 _ 0)
NAMED_CHARACTER_REFERENCE(
    263,
    /* L e */
    'f' _ 't' _ 'D' _ 'o' _ 'w' _ 'n' _ 'V' _ 'e' _ 'c' _ 't' _ 'o' _ 'r' _ ';',
    13, 0, 0x21c3 _ 0)
NAMED_CHARACTER_REFERENCE(
    264,
    /* L e */
    'f' _ 't' _ 'D' _ 'o' _ 'w' _ 'n' _ 'V' _ 'e' _ 'c' _ 't' _ 'o' _ 'r' _ 'B' _ 'a' _ 'r' _ ';',
    16, 0, 0x2959 _ 0)
NAMED_CHARACTER_REFERENCE(
    265,
    /* L e */ 'f' _ 't' _ 'F' _ 'l' _ 'o' _ 'o' _ 'r' _ ';', 8, 0, 0x230a _ 0)
NAMED_CHARACTER_REFERENCE(
    266,
    /* L e */
    'f' _ 't' _ 'R' _ 'i' _ 'g' _ 'h' _ 't' _ 'A' _ 'r' _ 'r' _ 'o' _ 'w' _ ';',
    13, 0, 0x2194 _ 0)
NAMED_CHARACTER_REFERENCE(
    267,
    /* L e */
    'f' _ 't' _ 'R' _ 'i' _ 'g' _ 'h' _ 't' _ 'V' _ 'e' _ 'c' _ 't' _ 'o' _ 'r' _ ';',
    14, 0, 0x294e _ 0)
NAMED_CHARACTER_REFERENCE(268,
                          /* L e */ 'f' _ 't' _ 'T' _ 'e' _ 'e' _ ';', 6, 0,
                          0x22a3 _ 0)
NAMED_CHARACTER_REFERENCE(
    269,
    /* L e */ 'f' _ 't' _ 'T' _ 'e' _ 'e' _ 'A' _ 'r' _ 'r' _ 'o' _ 'w' _ ';',
    11, 0, 0x21a4 _ 0)
NAMED_CHARACTER_REFERENCE(
    270,
    /* L e */
    'f' _ 't' _ 'T' _ 'e' _ 'e' _ 'V' _ 'e' _ 'c' _ 't' _ 'o' _ 'r' _ ';', 12,
    0, 0x295a _ 0)
NAMED_CHARACTER_REFERENCE(
    271,
    /* L e */ 'f' _ 't' _ 'T' _ 'r' _ 'i' _ 'a' _ 'n' _ 'g' _ 'l' _ 'e' _ ';',
    11, 0, 0x22b2 _ 0)
NAMED_CHARACTER_REFERENCE(
    272,
    /* L e */
    'f' _ 't' _ 'T' _ 'r' _ 'i' _ 'a' _ 'n' _ 'g' _ 'l' _ 'e' _ 'B' _ 'a' _ 'r' _ ';',
    14, 0, 0x29cf _ 0)
NAMED_CHARACTER_REFERENCE(
    273,
    /* L e */
    'f' _ 't' _ 'T' _ 'r' _ 'i' _ 'a' _ 'n' _ 'g' _ 'l' _ 'e' _ 'E' _ 'q' _ 'u' _ 'a' _ 'l' _ ';',
    16, 0, 0x22b4 _ 0)
NAMED_CHARACTER_REFERENCE(
    274,
    /* L e */
    'f' _ 't' _ 'U' _ 'p' _ 'D' _ 'o' _ 'w' _ 'n' _ 'V' _ 'e' _ 'c' _ 't' _ 'o' _ 'r' _ ';',
    15, 0, 0x2951 _ 0)
NAMED_CHARACTER_REFERENCE(
    275,
    /* L e */
    'f' _ 't' _ 'U' _ 'p' _ 'T' _ 'e' _ 'e' _ 'V' _ 'e' _ 'c' _ 't' _ 'o' _ 'r' _ ';',
    14, 0, 0x2960 _ 0)
NAMED_CHARACTER_REFERENCE(
    276,
    /* L e */ 'f' _ 't' _ 'U' _ 'p' _ 'V' _ 'e' _ 'c' _ 't' _ 'o' _ 'r' _ ';',
    11, 0, 0x21bf _ 0)
NAMED_CHARACTER_REFERENCE(
    277,
    /* L e */
    'f' _ 't' _ 'U' _ 'p' _ 'V' _ 'e' _ 'c' _ 't' _ 'o' _ 'r' _ 'B' _ 'a' _ 'r' _ ';',
    14, 0, 0x2958 _ 0)
NAMED_CHARACTER_REFERENCE(
    278,
    /* L e */ 'f' _ 't' _ 'V' _ 'e' _ 'c' _ 't' _ 'o' _ 'r' _ ';', 9, 0,
    0x21bc _ 0)
NAMED_CHARACTER_REFERENCE(
    279,
    /* L e */
    'f' _ 't' _ 'V' _ 'e' _ 'c' _ 't' _ 'o' _ 'r' _ 'B' _ 'a' _ 'r' _ ';', 12,
    0, 0x2952 _ 0)
NAMED_CHARACTER_REFERENCE(
    280,
    /* L e */ 'f' _ 't' _ 'a' _ 'r' _ 'r' _ 'o' _ 'w' _ ';', 8, 0, 0x21d0 _ 0)
NAMED_CHARACTER_REFERENCE(
    281,
    /* L e */
    'f' _ 't' _ 'r' _ 'i' _ 'g' _ 'h' _ 't' _ 'a' _ 'r' _ 'r' _ 'o' _ 'w' _ ';',
    13, 0, 0x21d4 _ 0)
NAMED_CHARACTER_REFERENCE(
    282,
    /* L e */
    's' _ 's' _ 'E' _ 'q' _ 'u' _ 'a' _ 'l' _ 'G' _ 'r' _ 'e' _ 'a' _ 't' _ 'e' _ 'r' _ ';',
    15, 0, 0x22da _ 0)
NAMED_CHARACTER_REFERENCE(
    283,
    /* L e */
    's' _ 's' _ 'F' _ 'u' _ 'l' _ 'l' _ 'E' _ 'q' _ 'u' _ 'a' _ 'l' _ ';', 12,
    0, 0x2266 _ 0)
NAMED_CHARACTER_REFERENCE(
    284,
    /* L e */ 's' _ 's' _ 'G' _ 'r' _ 'e' _ 'a' _ 't' _ 'e' _ 'r' _ ';', 10, 0,
    0x2276 _ 0)
NAMED_CHARACTER_REFERENCE(285,
                          /* L e */ 's' _ 's' _ 'L' _ 'e' _ 's' _ 's' _ ';', 7,
                          0, 0x2aa1 _ 0)
NAMED_CHARACTER_REFERENCE(
    286,
    /* L e */
    's' _ 's' _ 'S' _ 'l' _ 'a' _ 'n' _ 't' _ 'E' _ 'q' _ 'u' _ 'a' _ 'l' _ ';',
    13, 0, 0x2a7d _ 0)
NAMED_CHARACTER_REFERENCE(
    287,
    /* L e */ 's' _ 's' _ 'T' _ 'i' _ 'l' _ 'd' _ 'e' _ ';', 8, 0, 0x2272 _ 0)
NAMED_CHARACTER_REFERENCE(288, /* L f */ 'r' _ ';', 2, 0, 0xd835 _ 0xdd0f)
NAMED_CHARACTER_REFERENCE(289, /* L l */ ';', 1, 0, 0x22d8 _ 0)
NAMED_CHARACTER_REFERENCE(
    290,
    /* L l */ 'e' _ 'f' _ 't' _ 'a' _ 'r' _ 'r' _ 'o' _ 'w' _ ';', 9, 0,
    0x21da _ 0)
NAMED_CHARACTER_REFERENCE(291,
                          /* L m */ 'i' _ 'd' _ 'o' _ 't' _ ';', 5, 0,
                          0x013f _ 0)
NAMED_CHARACTER_REFERENCE(
    292,
    /* L o */
    'n' _ 'g' _ 'L' _ 'e' _ 'f' _ 't' _ 'A' _ 'r' _ 'r' _ 'o' _ 'w' _ ';', 12,
    0, 0x27f5 _ 0)
NAMED_CHARACTER_REFERENCE(
    293,
    /* L o */
    'n' _ 'g' _ 'L' _ 'e' _ 'f' _ 't' _ 'R' _ 'i' _ 'g' _ 'h' _ 't' _ 'A' _ 'r' _ 'r' _ 'o' _ 'w' _ ';',
    17, 0, 0x27f7 _ 0)
NAMED_CHARACTER_REFERENCE(
    294,
    /* L o */
    'n' _ 'g' _ 'R' _ 'i' _ 'g' _ 'h' _ 't' _ 'A' _ 'r' _ 'r' _ 'o' _ 'w' _ ';',
    13, 0, 0x27f6 _ 0)
NAMED_CHARACTER_REFERENCE(
    295,
    /* L o */
    'n' _ 'g' _ 'l' _ 'e' _ 'f' _ 't' _ 'a' _ 'r' _ 'r' _ 'o' _ 'w' _ ';', 12,
    0, 0x27f8 _ 0)
NAMED_CHARACTER_REFERENCE(
    296,
    /* L o */
    'n' _ 'g' _ 'l' _ 'e' _ 'f' _ 't' _ 'r' _ 'i' _ 'g' _ 'h' _ 't' _ 'a' _ 'r' _ 'r' _ 'o' _ 'w' _ ';',
    17, 0, 0x27fa _ 0)
NAMED_CHARACTER_REFERENCE(
    297,
    /* L o */
    'n' _ 'g' _ 'r' _ 'i' _ 'g' _ 'h' _ 't' _ 'a' _ 'r' _ 'r' _ 'o' _ 'w' _ ';',
    13, 0, 0x27f9 _ 0)
NAMED_CHARACTER_REFERENCE(298, /* L o */ 'p' _ 'f' _ ';', 3, 0, 0xd835 _ 0xdd43)
NAMED_CHARACTER_REFERENCE(
    299,
    /* L o */
    'w' _ 'e' _ 'r' _ 'L' _ 'e' _ 'f' _ 't' _ 'A' _ 'r' _ 'r' _ 'o' _ 'w' _ ';',
    13, 0, 0x2199 _ 0)
NAMED_CHARACTER_REFERENCE(
    300,
    /* L o */
    'w' _ 'e' _ 'r' _ 'R' _ 'i' _ 'g' _ 'h' _ 't' _ 'A' _ 'r' _ 'r' _ 'o' _ 'w' _ ';',
    14, 0, 0x2198 _ 0)
NAMED_CHARACTER_REFERENCE(301, /* L s */ 'c' _ 'r' _ ';', 3, 0, 0x2112 _ 0)
NAMED_CHARACTER_REFERENCE(302, /* L s */ 'h' _ ';', 2, 0, 0x21b0 _ 0)
NAMED_CHARACTER_REFERENCE(303,
                          /* L s */ 't' _ 'r' _ 'o' _ 'k' _ ';', 5, 0,
                          0x0141 _ 0)
NAMED_CHARACTER_REFERENCE(304, /* L t */ ';', 1, 0, 0x226a _ 0)
NAMED_CHARACTER_REFERENCE(305, /* M a */ 'p' _ ';', 2, 0, 0x2905 _ 0)
NAMED_CHARACTER_REFERENCE(306, /* M c */ 'y' _ ';', 2, 0, 0x041c _ 0)
NAMED_CHARACTER_REFERENCE(
    307,
    /* M e */ 'd' _ 'i' _ 'u' _ 'm' _ 'S' _ 'p' _ 'a' _ 'c' _ 'e' _ ';', 10, 0,
    0x205f _ 0)
NAMED_CHARACTER_REFERENCE(
    308,
    /* M e */ 'l' _ 'l' _ 'i' _ 'n' _ 't' _ 'r' _ 'f' _ ';', 8, 0, 0x2133 _ 0)
NAMED_CHARACTER_REFERENCE(309, /* M f */ 'r' _ ';', 2, 0, 0xd835 _ 0xdd10)
NAMED_CHARACTER_REFERENCE(
    310,
    /* M i */ 'n' _ 'u' _ 's' _ 'P' _ 'l' _ 'u' _ 's' _ ';', 8, 0, 0x2213 _ 0)
NAMED_CHARACTER_REFERENCE(311, /* M o */ 'p' _ 'f' _ ';', 3, 0, 0xd835 _ 0xdd44)
NAMED_CHARACTER_REFERENCE(312, /* M s */ 'c' _ 'r' _ ';', 3, 0, 0x2133 _ 0)
NAMED_CHARACTER_REFERENCE(313, /* M u */ ';', 1, 0, 0x039c _ 0)
NAMED_CHARACTER_REFERENCE(314, /* N J */ 'c' _ 'y' _ ';', 3, 0, 0x040a _ 0)
NAMED_CHARACTER_REFERENCE(315,
                          /* N a */ 'c' _ 'u' _ 't' _ 'e' _ ';', 5, 0,
                          0x0143 _ 0)
NAMED_CHARACTER_REFERENCE(316,
                          /* N c */ 'a' _ 'r' _ 'o' _ 'n' _ ';', 5, 0,
                          0x0147 _ 0)
NAMED_CHARACTER_REFERENCE(317,
                          /* N c */ 'e' _ 'd' _ 'i' _ 'l' _ ';', 5, 0,
                          0x0145 _ 0)
NAMED_CHARACTER_REFERENCE(318, /* N c */ 'y' _ ';', 2, 0, 0x041d _ 0)
NAMED_CHARACTER_REFERENCE(
    319,
    /* N e */
    'g' _ 'a' _ 't' _ 'i' _ 'v' _ 'e' _ 'M' _ 'e' _ 'd' _ 'i' _ 'u' _ 'm' _ 'S' _ 'p' _ 'a' _ 'c' _ 'e' _ ';',
    18, 0, 0x200b _ 0)
NAMED_CHARACTER_REFERENCE(
    320,
    /* N e */
    'g' _ 'a' _ 't' _ 'i' _ 'v' _ 'e' _ 'T' _ 'h' _ 'i' _ 'c' _ 'k' _ 'S' _ 'p' _ 'a' _ 'c' _ 'e' _ ';',
    17, 0, 0x200b _ 0)
NAMED_CHARACTER_REFERENCE(
    321,
    /* N e */
    'g' _ 'a' _ 't' _ 'i' _ 'v' _ 'e' _ 'T' _ 'h' _ 'i' _ 'n' _ 'S' _ 'p' _ 'a' _ 'c' _ 'e' _ ';',
    16, 0, 0x200b _ 0)
NAMED_CHARACTER_REFERENCE(
    322,
    /* N e */
    'g' _ 'a' _ 't' _ 'i' _ 'v' _ 'e' _ 'V' _ 'e' _ 'r' _ 'y' _ 'T' _ 'h' _ 'i' _ 'n' _ 'S' _ 'p' _ 'a' _ 'c' _ 'e' _ ';',
    20, 0, 0x200b _ 0)
NAMED_CHARACTER_REFERENCE(
    323,
    /* N e */
    's' _ 't' _ 'e' _ 'd' _ 'G' _ 'r' _ 'e' _ 'a' _ 't' _ 'e' _ 'r' _ 'G' _ 'r' _ 'e' _ 'a' _ 't' _ 'e' _ 'r' _ ';',
    19, 0, 0x226b _ 0)
NAMED_CHARACTER_REFERENCE(
    324,
    /* N e */
    's' _ 't' _ 'e' _ 'd' _ 'L' _ 'e' _ 's' _ 's' _ 'L' _ 'e' _ 's' _ 's' _ ';',
    13, 0, 0x226a _ 0)
NAMED_CHARACTER_REFERENCE(325,
                          /* N e */ 'w' _ 'L' _ 'i' _ 'n' _ 'e' _ ';', 6, 0,
                          0x000a _ 0)
NAMED_CHARACTER_REFERENCE(326, /* N f */ 'r' _ ';', 2, 0, 0xd835 _ 0xdd11)
NAMED_CHARACTER_REFERENCE(327,
                          /* N o */ 'B' _ 'r' _ 'e' _ 'a' _ 'k' _ ';', 6, 0,
                          0x2060 _ 0)
NAMED_CHARACTER_REFERENCE(
    328,
    /* N o */
    'n' _ 'B' _ 'r' _ 'e' _ 'a' _ 'k' _ 'i' _ 'n' _ 'g' _ 'S' _ 'p' _ 'a' _ 'c' _ 'e' _ ';',
    15, 0, 0x00a0 _ 0)
NAMED_CHARACTER_REFERENCE(329, /* N o */ 'p' _ 'f' _ ';', 3, 0, 0x2115 _ 0)
NAMED_CHARACTER_REFERENCE(330, /* N o */ 't' _ ';', 2, 0, 0x2aec _ 0)
NAMED_CHARACTER_REFERENCE(
    331,
    /* N o */ 't' _ 'C' _ 'o' _ 'n' _ 'g' _ 'r' _ 'u' _ 'e' _ 'n' _ 't' _ ';',
    11, 0, 0x2262 _ 0)
NAMED_CHARACTER_REFERENCE(
    332,
    /* N o */ 't' _ 'C' _ 'u' _ 'p' _ 'C' _ 'a' _ 'p' _ ';', 8, 0, 0x226d _ 0)
NAMED_CHARACTER_REFERENCE(
    333,
    /* N o */
    't' _ 'D' _ 'o' _ 'u' _ 'b' _ 'l' _ 'e' _ 'V' _ 'e' _ 'r' _ 't' _ 'i' _ 'c' _ 'a' _ 'l' _ 'B' _ 'a' _ 'r' _ ';',
    19, 0, 0x2226 _ 0)
NAMED_CHARACTER_REFERENCE(
    334,
    /* N o */ 't' _ 'E' _ 'l' _ 'e' _ 'm' _ 'e' _ 'n' _ 't' _ ';', 9, 0,
    0x2209 _ 0)
NAMED_CHARACTER_REFERENCE(335,
                          /* N o */ 't' _ 'E' _ 'q' _ 'u' _ 'a' _ 'l' _ ';', 7,
                          0, 0x2260 _ 0)
NAMED_CHARACTER_REFERENCE(
    336,
    /* N o */
    't' _ 'E' _ 'q' _ 'u' _ 'a' _ 'l' _ 'T' _ 'i' _ 'l' _ 'd' _ 'e' _ ';', 12,
    0, 0x2242 _ 0x0338)
NAMED_CHARACTER_REFERENCE(
    337,
    /* N o */ 't' _ 'E' _ 'x' _ 'i' _ 's' _ 't' _ 's' _ ';', 8, 0, 0x2204 _ 0)
NAMED_CHARACTER_REFERENCE(
    338,
    /* N o */ 't' _ 'G' _ 'r' _ 'e' _ 'a' _ 't' _ 'e' _ 'r' _ ';', 9, 0,
    0x226f _ 0)
NAMED_CHARACTER_REFERENCE(
    339,
    /* N o */
    't' _ 'G' _ 'r' _ 'e' _ 'a' _ 't' _ 'e' _ 'r' _ 'E' _ 'q' _ 'u' _ 'a' _ 'l' _ ';',
    14, 0, 0x2271 _ 0)
NAMED_CHARACTER_REFERENCE(
    340,
    /* N o */
    't' _ 'G' _ 'r' _ 'e' _ 'a' _ 't' _ 'e' _ 'r' _ 'F' _ 'u' _ 'l' _ 'l' _ 'E' _ 'q' _ 'u' _ 'a' _ 'l' _ ';',
    18, 0, 0x2267 _ 0x0338)
NAMED_CHARACTER_REFERENCE(
    341,
    /* N o */
    't' _ 'G' _ 'r' _ 'e' _ 'a' _ 't' _ 'e' _ 'r' _ 'G' _ 'r' _ 'e' _ 'a' _ 't' _ 'e' _ 'r' _ ';',
    16, 0, 0x226b _ 0x0338)
NAMED_CHARACTER_REFERENCE(
    342,
    /* N o */
    't' _ 'G' _ 'r' _ 'e' _ 'a' _ 't' _ 'e' _ 'r' _ 'L' _ 'e' _ 's' _ 's' _ ';',
    13, 0, 0x2279 _ 0)
NAMED_CHARACTER_REFERENCE(
    343,
    /* N o */
    't' _ 'G' _ 'r' _ 'e' _ 'a' _ 't' _ 'e' _ 'r' _ 'S' _ 'l' _ 'a' _ 'n' _ 't' _ 'E' _ 'q' _ 'u' _ 'a' _ 'l' _ ';',
    19, 0, 0x2a7e _ 0x0338)
NAMED_CHARACTER_REFERENCE(
    344,
    /* N o */
    't' _ 'G' _ 'r' _ 'e' _ 'a' _ 't' _ 'e' _ 'r' _ 'T' _ 'i' _ 'l' _ 'd' _ 'e' _ ';',
    14, 0, 0x2275 _ 0)
NAMED_CHARACTER_REFERENCE(
    345,
    /* N o */
    't' _ 'H' _ 'u' _ 'm' _ 'p' _ 'D' _ 'o' _ 'w' _ 'n' _ 'H' _ 'u' _ 'm' _ 'p' _ ';',
    14, 0, 0x224e _ 0x0338)
NAMED_CHARACTER_REFERENCE(
    346,
    /* N o */ 't' _ 'H' _ 'u' _ 'm' _ 'p' _ 'E' _ 'q' _ 'u' _ 'a' _ 'l' _ ';',
    11, 0, 0x224f _ 0x0338)
NAMED_CHARACTER_REFERENCE(
    347,
    /* N o */
    't' _ 'L' _ 'e' _ 'f' _ 't' _ 'T' _ 'r' _ 'i' _ 'a' _ 'n' _ 'g' _ 'l' _ 'e' _ ';',
    14, 0, 0x22ea _ 0)
NAMED_CHARACTER_REFERENCE(
    348,
    /* N o */
    't' _ 'L' _ 'e' _ 'f' _ 't' _ 'T' _ 'r' _ 'i' _ 'a' _ 'n' _ 'g' _ 'l' _ 'e' _ 'B' _ 'a' _ 'r' _ ';',
    17, 0, 0x29cf _ 0x0338)
NAMED_CHARACTER_REFERENCE(
    349,
    /* N o */
    't' _ 'L' _ 'e' _ 'f' _ 't' _ 'T' _ 'r' _ 'i' _ 'a' _ 'n' _ 'g' _ 'l' _ 'e' _ 'E' _ 'q' _ 'u' _ 'a' _ 'l' _ ';',
    19, 0, 0x22ec _ 0)
NAMED_CHARACTER_REFERENCE(350,
                          /* N o */ 't' _ 'L' _ 'e' _ 's' _ 's' _ ';', 6, 0,
                          0x226e _ 0)
NAMED_CHARACTER_REFERENCE(
    351,
    /* N o */ 't' _ 'L' _ 'e' _ 's' _ 's' _ 'E' _ 'q' _ 'u' _ 'a' _ 'l' _ ';',
    11, 0, 0x2270 _ 0)
NAMED_CHARACTER_REFERENCE(
    352,
    /* N o */
    't' _ 'L' _ 'e' _ 's' _ 's' _ 'G' _ 'r' _ 'e' _ 'a' _ 't' _ 'e' _ 'r' _ ';',
    13, 0, 0x2278 _ 0)
NAMED_CHARACTER_REFERENCE(
    353,
    /* N o */ 't' _ 'L' _ 'e' _ 's' _ 's' _ 'L' _ 'e' _ 's' _ 's' _ ';', 10, 0,
    0x226a _ 0x0338)
NAMED_CHARACTER_REFERENCE(
    354,
    /* N o */
    't' _ 'L' _ 'e' _ 's' _ 's' _ 'S' _ 'l' _ 'a' _ 'n' _ 't' _ 'E' _ 'q' _ 'u' _ 'a' _ 'l' _ ';',
    16, 0, 0x2a7d _ 0x0338)
NAMED_CHARACTER_REFERENCE(
    355,
    /* N o */ 't' _ 'L' _ 'e' _ 's' _ 's' _ 'T' _ 'i' _ 'l' _ 'd' _ 'e' _ ';',
    11, 0, 0x2274 _ 0)
NAMED_CHARACTER_REFERENCE(
    356,
    /* N o */
    't' _ 'N' _ 'e' _ 's' _ 't' _ 'e' _ 'd' _ 'G' _ 'r' _ 'e' _ 'a' _ 't' _ 'e' _ 'r' _ 'G' _ 'r' _ 'e' _ 'a' _ 't' _ 'e' _ 'r' _ ';',
    22, 0, 0x2aa2 _ 0x0338)
NAMED_CHARACTER_REFERENCE(
    357,
    /* N o */
    't' _ 'N' _ 'e' _ 's' _ 't' _ 'e' _ 'd' _ 'L' _ 'e' _ 's' _ 's' _ 'L' _ 'e' _ 's' _ 's' _ ';',
    16, 0, 0x2aa1 _ 0x0338)
NAMED_CHARACTER_REFERENCE(
    358,
    /* N o */ 't' _ 'P' _ 'r' _ 'e' _ 'c' _ 'e' _ 'd' _ 'e' _ 's' _ ';', 10, 0,
    0x2280 _ 0)
NAMED_CHARACTER_REFERENCE(
    359,
    /* N o */
    't' _ 'P' _ 'r' _ 'e' _ 'c' _ 'e' _ 'd' _ 'e' _ 's' _ 'E' _ 'q' _ 'u' _ 'a' _ 'l' _ ';',
    15, 0, 0x2aaf _ 0x0338)
NAMED_CHARACTER_REFERENCE(
    360,
    /* N o */
    't' _ 'P' _ 'r' _ 'e' _ 'c' _ 'e' _ 'd' _ 'e' _ 's' _ 'S' _ 'l' _ 'a' _ 'n' _ 't' _ 'E' _ 'q' _ 'u' _ 'a' _ 'l' _ ';',
    20, 0, 0x22e0 _ 0)
NAMED_CHARACTER_REFERENCE(
    361,
    /* N o */
    't' _ 'R' _ 'e' _ 'v' _ 'e' _ 'r' _ 's' _ 'e' _ 'E' _ 'l' _ 'e' _ 'm' _ 'e' _ 'n' _ 't' _ ';',
    16, 0, 0x220c _ 0)
NAMED_CHARACTER_REFERENCE(
    362,
    /* N o */
    't' _ 'R' _ 'i' _ 'g' _ 'h' _ 't' _ 'T' _ 'r' _ 'i' _ 'a' _ 'n' _ 'g' _ 'l' _ 'e' _ ';',
    15, 0, 0x22eb _ 0)
NAMED_CHARACTER_REFERENCE(
    363,
    /* N o */
    't' _ 'R' _ 'i' _ 'g' _ 'h' _ 't' _ 'T' _ 'r' _ 'i' _ 'a' _ 'n' _ 'g' _ 'l' _ 'e' _ 'B' _ 'a' _ 'r' _ ';',
    18, 0, 0x29d0 _ 0x0338)
NAMED_CHARACTER_REFERENCE(
    364,
    /* N o */
    't' _ 'R' _ 'i' _ 'g' _ 'h' _ 't' _ 'T' _ 'r' _ 'i' _ 'a' _ 'n' _ 'g' _ 'l' _ 'e' _ 'E' _ 'q' _ 'u' _ 'a' _ 'l' _ ';',
    20, 0, 0x22ed _ 0)
NAMED_CHARACTER_REFERENCE(
    365,
    /* N o */
    't' _ 'S' _ 'q' _ 'u' _ 'a' _ 'r' _ 'e' _ 'S' _ 'u' _ 'b' _ 's' _ 'e' _ 't' _ ';',
    14, 0, 0x228f _ 0x0338)
NAMED_CHARACTER_REFERENCE(
    366,
    /* N o */
    't' _ 'S' _ 'q' _ 'u' _ 'a' _ 'r' _ 'e' _ 'S' _ 'u' _ 'b' _ 's' _ 'e' _ 't' _ 'E' _ 'q' _ 'u' _ 'a' _ 'l' _ ';',
    19, 0, 0x22e2 _ 0)
NAMED_CHARACTER_REFERENCE(
    367,
    /* N o */
    't' _ 'S' _ 'q' _ 'u' _ 'a' _ 'r' _ 'e' _ 'S' _ 'u' _ 'p' _ 'e' _ 'r' _ 's' _ 'e' _ 't' _ ';',
    16, 0, 0x2290 _ 0x0338)
NAMED_CHARACTER_REFERENCE(
    368,
    /* N o */
    't' _ 'S' _ 'q' _ 'u' _ 'a' _ 'r' _ 'e' _ 'S' _ 'u' _ 'p' _ 'e' _ 'r' _ 's' _ 'e' _ 't' _ 'E' _ 'q' _ 'u' _ 'a' _ 'l' _ ';',
    21, 0, 0x22e3 _ 0)
NAMED_CHARACTER_REFERENCE(
    369,
    /* N o */ 't' _ 'S' _ 'u' _ 'b' _ 's' _ 'e' _ 't' _ ';', 8, 0,
    0x2282 _ 0x20d2)
NAMED_CHARACTER_REFERENCE(
    370,
    /* N o */
    't' _ 'S' _ 'u' _ 'b' _ 's' _ 'e' _ 't' _ 'E' _ 'q' _ 'u' _ 'a' _ 'l' _ ';',
    13, 0, 0x2288 _ 0)
NAMED_CHARACTER_REFERENCE(
    371,
    /* N o */ 't' _ 'S' _ 'u' _ 'c' _ 'c' _ 'e' _ 'e' _ 'd' _ 's' _ ';', 10, 0,
    0x2281 _ 0)
NAMED_CHARACTER_REFERENCE(
    372,
    /* N o */
    't' _ 'S' _ 'u' _ 'c' _ 'c' _ 'e' _ 'e' _ 'd' _ 's' _ 'E' _ 'q' _ 'u' _ 'a' _ 'l' _ ';',
    15, 0, 0x2ab0 _ 0x0338)
NAMED_CHARACTER_REFERENCE(
    373,
    /* N o */
    't' _ 'S' _ 'u' _ 'c' _ 'c' _ 'e' _ 'e' _ 'd' _ 's' _ 'S' _ 'l' _ 'a' _ 'n' _ 't' _ 'E' _ 'q' _ 'u' _ 'a' _ 'l' _ ';',
    20, 0, 0x22e1 _ 0)
NAMED_CHARACTER_REFERENCE(
    374,
    /* N o */
    't' _ 'S' _ 'u' _ 'c' _ 'c' _ 'e' _ 'e' _ 'd' _ 's' _ 'T' _ 'i' _ 'l' _ 'd' _ 'e' _ ';',
    15, 0, 0x227f _ 0x0338)
NAMED_CHARACTER_REFERENCE(
    375,
    /* N o */ 't' _ 'S' _ 'u' _ 'p' _ 'e' _ 'r' _ 's' _ 'e' _ 't' _ ';', 10, 0,
    0x2283 _ 0x20d2)
NAMED_CHARACTER_REFERENCE(
    376,
    /* N o */
    't' _ 'S' _ 'u' _ 'p' _ 'e' _ 'r' _ 's' _ 'e' _ 't' _ 'E' _ 'q' _ 'u' _ 'a' _ 'l' _ ';',
    15, 0, 0x2289 _ 0)
NAMED_CHARACTER_REFERENCE(377,
                          /* N o */ 't' _ 'T' _ 'i' _ 'l' _ 'd' _ 'e' _ ';', 7,
                          0, 0x2241 _ 0)
NAMED_CHARACTER_REFERENCE(
    378,
    /* N o */
    't' _ 'T' _ 'i' _ 'l' _ 'd' _ 'e' _ 'E' _ 'q' _ 'u' _ 'a' _ 'l' _ ';', 12,
    0, 0x2244 _ 0)
NAMED_CHARACTER_REFERENCE(
    379,
    /* N o */
    't' _ 'T' _ 'i' _ 'l' _ 'd' _ 'e' _ 'F' _ 'u' _ 'l' _ 'l' _ 'E' _ 'q' _ 'u' _ 'a' _ 'l' _ ';',
    16, 0, 0x2247 _ 0)
NAMED_CHARACTER_REFERENCE(
    380,
    /* N o */
    't' _ 'T' _ 'i' _ 'l' _ 'd' _ 'e' _ 'T' _ 'i' _ 'l' _ 'd' _ 'e' _ ';', 12,
    0, 0x2249 _ 0)
NAMED_CHARACTER_REFERENCE(
    381,
    /* N o */
    't' _ 'V' _ 'e' _ 'r' _ 't' _ 'i' _ 'c' _ 'a' _ 'l' _ 'B' _ 'a' _ 'r' _ ';',
    13, 0, 0x2224 _ 0)
NAMED_CHARACTER_REFERENCE(382, /* N s */ 'c' _ 'r' _ ';', 3, 0, 0xd835 _ 0xdca9)
NAMED_CHARACTER_REFERENCE(383,
                          /* N t */ 'i' _ 'l' _ 'd' _ 'e', 4, 0, 0x00d1 _ 0)
NAMED_CHARACTER_REFERENCE(384,
                          /* N t */ 'i' _ 'l' _ 'd' _ 'e' _ ';', 5, 0,
                          0x00d1 _ 0)
NAMED_CHARACTER_REFERENCE(385, /* N u */ ';', 1, 0, 0x039d _ 0)
NAMED_CHARACTER_REFERENCE(386,
                          /* O E */ 'l' _ 'i' _ 'g' _ ';', 4, 0, 0x0152 _ 0)
NAMED_CHARACTER_REFERENCE(387,
                          /* O a */ 'c' _ 'u' _ 't' _ 'e', 4, 0, 0x00d3 _ 0)
NAMED_CHARACTER_REFERENCE(388,
                          /* O a */ 'c' _ 'u' _ 't' _ 'e' _ ';', 5, 0,
                          0x00d3 _ 0)
NAMED_CHARACTER_REFERENCE(389, /* O c */ 'i' _ 'r' _ 'c', 3, 0, 0x00d4 _ 0)
NAMED_CHARACTER_REFERENCE(390,
                          /* O c */ 'i' _ 'r' _ 'c' _ ';', 4, 0, 0x00d4 _ 0)
NAMED_CHARACTER_REFERENCE(391, /* O c */ 'y' _ ';', 2, 0, 0x041e _ 0)
NAMED_CHARACTER_REFERENCE(392,
                          /* O d */ 'b' _ 'l' _ 'a' _ 'c' _ ';', 5, 0,
                          0x0150 _ 0)
NAMED_CHARACTER_REFERENCE(393, /* O f */ 'r' _ ';', 2, 0, 0xd835 _ 0xdd12)
NAMED_CHARACTER_REFERENCE(394,
                          /* O g */ 'r' _ 'a' _ 'v' _ 'e', 4, 0, 0x00d2 _ 0)
NAMED_CHARACTER_REFERENCE(395,
                          /* O g */ 'r' _ 'a' _ 'v' _ 'e' _ ';', 5, 0,
                          0x00d2 _ 0)
NAMED_CHARACTER_REFERENCE(396,
                          /* O m */ 'a' _ 'c' _ 'r' _ ';', 4, 0, 0x014c _ 0)
NAMED_CHARACTER_REFERENCE(397,
                          /* O m */ 'e' _ 'g' _ 'a' _ ';', 4, 0, 0x03a9 _ 0)
NAMED_CHARACTER_REFERENCE(398,
                          /* O m */ 'i' _ 'c' _ 'r' _ 'o' _ 'n' _ ';', 6, 0,
                          0x039f _ 0)
NAMED_CHARACTER_REFERENCE(399, /* O o */ 'p' _ 'f' _ ';', 3, 0, 0xd835 _ 0xdd46)
NAMED_CHARACTER_REFERENCE(
    400,
    /* O p */
    'e' _ 'n' _ 'C' _ 'u' _ 'r' _ 'l' _ 'y' _ 'D' _ 'o' _ 'u' _ 'b' _ 'l' _ 'e' _ 'Q' _ 'u' _ 'o' _ 't' _ 'e' _ ';',
    19, 0, 0x201c _ 0)
NAMED_CHARACTER_REFERENCE(
    401,
    /* O p */
    'e' _ 'n' _ 'C' _ 'u' _ 'r' _ 'l' _ 'y' _ 'Q' _ 'u' _ 'o' _ 't' _ 'e' _ ';',
    13, 0, 0x2018 _ 0)
NAMED_CHARACTER_REFERENCE(402, /* O r */ ';', 1, 0, 0x2a54 _ 0)
NAMED_CHARACTER_REFERENCE(403, /* O s */ 'c' _ 'r' _ ';', 3, 0, 0xd835 _ 0xdcaa)
NAMED_CHARACTER_REFERENCE(404,
                          /* O s */ 'l' _ 'a' _ 's' _ 'h', 4, 0, 0x00d8 _ 0)
NAMED_CHARACTER_REFERENCE(405,
                          /* O s */ 'l' _ 'a' _ 's' _ 'h' _ ';', 5, 0,
                          0x00d8 _ 0)
NAMED_CHARACTER_REFERENCE(406,
                          /* O t */ 'i' _ 'l' _ 'd' _ 'e', 4, 0, 0x00d5 _ 0)
NAMED_CHARACTER_REFERENCE(407,
                          /* O t */ 'i' _ 'l' _ 'd' _ 'e' _ ';', 5, 0,
                          0x00d5 _ 0)
NAMED_CHARACTER_REFERENCE(408,
                          /* O t */ 'i' _ 'm' _ 'e' _ 's' _ ';', 5, 0,
                          0x2a37 _ 0)
NAMED_CHARACTER_REFERENCE(409, /* O u */ 'm' _ 'l', 2, 0, 0x00d6 _ 0)
NAMED_CHARACTER_REFERENCE(410, /* O u */ 'm' _ 'l' _ ';', 3, 0, 0x00d6 _ 0)
NAMED_CHARACTER_REFERENCE(411,
                          /* O v */ 'e' _ 'r' _ 'B' _ 'a' _ 'r' _ ';', 6, 0,
                          0x203e _ 0)
NAMED_CHARACTER_REFERENCE(
    412,
    /* O v */ 'e' _ 'r' _ 'B' _ 'r' _ 'a' _ 'c' _ 'e' _ ';', 8, 0, 0x23de _ 0)
NAMED_CHARACTER_REFERENCE(
    413,
    /* O v */ 'e' _ 'r' _ 'B' _ 'r' _ 'a' _ 'c' _ 'k' _ 'e' _ 't' _ ';', 10, 0,
    0x23b4 _ 0)
NAMED_CHARACTER_REFERENCE(
    414,
    /* O v */
    'e' _ 'r' _ 'P' _ 'a' _ 'r' _ 'e' _ 'n' _ 't' _ 'h' _ 'e' _ 's' _ 'i' _ 's' _ ';',
    14, 0, 0x23dc _ 0)
NAMED_CHARACTER_REFERENCE(415,
                          /* P a */ 'r' _ 't' _ 'i' _ 'a' _ 'l' _ 'D' _ ';', 7,
                          0, 0x2202 _ 0)
NAMED_CHARACTER_REFERENCE(416, /* P c */ 'y' _ ';', 2, 0, 0x041f _ 0)
NAMED_CHARACTER_REFERENCE(417, /* P f */ 'r' _ ';', 2, 0, 0xd835 _ 0xdd13)
NAMED_CHARACTER_REFERENCE(418, /* P h */ 'i' _ ';', 2, 0, 0x03a6 _ 0)
NAMED_CHARACTER_REFERENCE(419, /* P i */ ';', 1, 0, 0x03a0 _ 0)
NAMED_CHARACTER_REFERENCE(
    420,
    /* P l */ 'u' _ 's' _ 'M' _ 'i' _ 'n' _ 'u' _ 's' _ ';', 8, 0, 0x00b1 _ 0)
NAMED_CHARACTER_REFERENCE(
    421,
    /* P o */
    'i' _ 'n' _ 'c' _ 'a' _ 'r' _ 'e' _ 'p' _ 'l' _ 'a' _ 'n' _ 'e' _ ';', 12,
    0, 0x210c _ 0)
NAMED_CHARACTER_REFERENCE(422, /* P o */ 'p' _ 'f' _ ';', 3, 0, 0x2119 _ 0)
NAMED_CHARACTER_REFERENCE(423, /* P r */ ';', 1, 0, 0x2abb _ 0)
NAMED_CHARACTER_REFERENCE(424,
                          /* P r */ 'e' _ 'c' _ 'e' _ 'd' _ 'e' _ 's' _ ';', 7,
                          0, 0x227a _ 0)
NAMED_CHARACTER_REFERENCE(
    425,
    /* P r */
    'e' _ 'c' _ 'e' _ 'd' _ 'e' _ 's' _ 'E' _ 'q' _ 'u' _ 'a' _ 'l' _ ';', 12,
    0, 0x2aaf _ 0)
NAMED_CHARACTER_REFERENCE(
    426,
    /* P r */
    'e' _ 'c' _ 'e' _ 'd' _ 'e' _ 's' _ 'S' _ 'l' _ 'a' _ 'n' _ 't' _ 'E' _ 'q' _ 'u' _ 'a' _ 'l' _ ';',
    17, 0, 0x227c _ 0)
NAMED_CHARACTER_REFERENCE(
    427,
    /* P r */
    'e' _ 'c' _ 'e' _ 'd' _ 'e' _ 's' _ 'T' _ 'i' _ 'l' _ 'd' _ 'e' _ ';', 12,
    0, 0x227e _ 0)
NAMED_CHARACTER_REFERENCE(428,
                          /* P r */ 'i' _ 'm' _ 'e' _ ';', 4, 0, 0x2033 _ 0)
NAMED_CHARACTER_REFERENCE(429,
                          /* P r */ 'o' _ 'd' _ 'u' _ 'c' _ 't' _ ';', 6, 0,
                          0x220f _ 0)
NAMED_CHARACTER_REFERENCE(
    430,
    /* P r */ 'o' _ 'p' _ 'o' _ 'r' _ 't' _ 'i' _ 'o' _ 'n' _ ';', 9, 0,
    0x2237 _ 0)
NAMED_CHARACTER_REFERENCE(
    431,
    /* P r */ 'o' _ 'p' _ 'o' _ 'r' _ 't' _ 'i' _ 'o' _ 'n' _ 'a' _ 'l' _ ';',
    11, 0, 0x221d _ 0)
NAMED_CHARACTER_REFERENCE(432, /* P s */ 'c' _ 'r' _ ';', 3, 0, 0xd835 _ 0xdcab)
NAMED_CHARACTER_REFERENCE(433, /* P s */ 'i' _ ';', 2, 0, 0x03a8 _ 0)
NAMED_CHARACTER_REFERENCE(434, /* Q U */ 'O' _ 'T', 2, 0, 0x0022 _ 0)
NAMED_CHARACTER_REFERENCE(435, /* Q U */ 'O' _ 'T' _ ';', 3, 0, 0x0022 _ 0)
NAMED_CHARACTER_REFERENCE(436, /* Q f */ 'r' _ ';', 2, 0, 0xd835 _ 0xdd14)
NAMED_CHARACTER_REFERENCE(437, /* Q o */ 'p' _ 'f' _ ';', 3, 0, 0x211a _ 0)
NAMED_CHARACTER_REFERENCE(438, /* Q s */ 'c' _ 'r' _ ';', 3, 0, 0xd835 _ 0xdcac)
NAMED_CHARACTER_REFERENCE(439,
                          /* R B */ 'a' _ 'r' _ 'r' _ ';', 4, 0, 0x2910 _ 0)
NAMED_CHARACTER_REFERENCE(440, /* R E */ 'G', 1, 0, 0x00ae _ 0)
NAMED_CHARACTER_REFERENCE(441, /* R E */ 'G' _ ';', 2, 0, 0x00ae _ 0)
NAMED_CHARACTER_REFERENCE(442,
                          /* R a */ 'c' _ 'u' _ 't' _ 'e' _ ';', 5, 0,
                          0x0154 _ 0)
NAMED_CHARACTER_REFERENCE(443, /* R a */ 'n' _ 'g' _ ';', 3, 0, 0x27eb _ 0)
NAMED_CHARACTER_REFERENCE(444, /* R a */ 'r' _ 'r' _ ';', 3, 0, 0x21a0 _ 0)
NAMED_CHARACTER_REFERENCE(445,
                          /* R a */ 'r' _ 'r' _ 't' _ 'l' _ ';', 5, 0,
                          0x2916 _ 0)
NAMED_CHARACTER_REFERENCE(446,
                          /* R c */ 'a' _ 'r' _ 'o' _ 'n' _ ';', 5, 0,
                          0x0158 _ 0)
NAMED_CHARACTER_REFERENCE(447,
                          /* R c */ 'e' _ 'd' _ 'i' _ 'l' _ ';', 5, 0,
                          0x0156 _ 0)
NAMED_CHARACTER_REFERENCE(448, /* R c */ 'y' _ ';', 2, 0, 0x0420 _ 0)
NAMED_CHARACTER_REFERENCE(449, /* R e */ ';', 1, 0, 0x211c _ 0)
NAMED_CHARACTER_REFERENCE(
    450,
    /* R e */
    'v' _ 'e' _ 'r' _ 's' _ 'e' _ 'E' _ 'l' _ 'e' _ 'm' _ 'e' _ 'n' _ 't' _ ';',
    13, 0, 0x220b _ 0)
NAMED_CHARACTER_REFERENCE(
    451,
    /* R e */
    'v' _ 'e' _ 'r' _ 's' _ 'e' _ 'E' _ 'q' _ 'u' _ 'i' _ 'l' _ 'i' _ 'b' _ 'r' _ 'i' _ 'u' _ 'm' _ ';',
    17, 0, 0x21cb _ 0)
NAMED_CHARACTER_REFERENCE(
    452,
    /* R e */
    'v' _ 'e' _ 'r' _ 's' _ 'e' _ 'U' _ 'p' _ 'E' _ 'q' _ 'u' _ 'i' _ 'l' _ 'i' _ 'b' _ 'r' _ 'i' _ 'u' _ 'm' _ ';',
    19, 0, 0x296f _ 0)
NAMED_CHARACTER_REFERENCE(453, /* R f */ 'r' _ ';', 2, 0, 0x211c _ 0)
NAMED_CHARACTER_REFERENCE(454, /* R h */ 'o' _ ';', 2, 0, 0x03a1 _ 0)
NAMED_CHARACTER_REFERENCE(
    455,
    /* R i */
    'g' _ 'h' _ 't' _ 'A' _ 'n' _ 'g' _ 'l' _ 'e' _ 'B' _ 'r' _ 'a' _ 'c' _ 'k' _ 'e' _ 't' _ ';',
    16, 0, 0x27e9 _ 0)
NAMED_CHARACTER_REFERENCE(
    456,
    /* R i */ 'g' _ 'h' _ 't' _ 'A' _ 'r' _ 'r' _ 'o' _ 'w' _ ';', 9, 0,
    0x2192 _ 0)
NAMED_CHARACTER_REFERENCE(
    457,
    /* R i */
    'g' _ 'h' _ 't' _ 'A' _ 'r' _ 'r' _ 'o' _ 'w' _ 'B' _ 'a' _ 'r' _ ';', 12,
    0, 0x21e5 _ 0)
NAMED_CHARACTER_REFERENCE(
    458,
    /* R i */
    'g' _ 'h' _ 't' _ 'A' _ 'r' _ 'r' _ 'o' _ 'w' _ 'L' _ 'e' _ 'f' _ 't' _ 'A' _ 'r' _ 'r' _ 'o' _ 'w' _ ';',
    18, 0, 0x21c4 _ 0)
NAMED_CHARACTER_REFERENCE(
    459,
    /* R i */ 'g' _ 'h' _ 't' _ 'C' _ 'e' _ 'i' _ 'l' _ 'i' _ 'n' _ 'g' _ ';',
    11, 0, 0x2309 _ 0)
NAMED_CHARACTER_REFERENCE(
    460,
    /* R i */
    'g' _ 'h' _ 't' _ 'D' _ 'o' _ 'u' _ 'b' _ 'l' _ 'e' _ 'B' _ 'r' _ 'a' _ 'c' _ 'k' _ 'e' _ 't' _ ';',
    17, 0, 0x27e7 _ 0)
NAMED_CHARACTER_REFERENCE(
    461,
    /* R i */
    'g' _ 'h' _ 't' _ 'D' _ 'o' _ 'w' _ 'n' _ 'T' _ 'e' _ 'e' _ 'V' _ 'e' _ 'c' _ 't' _ 'o' _ 'r' _ ';',
    17, 0, 0x295d _ 0)
NAMED_CHARACTER_REFERENCE(
    462,
    /* R i */
    'g' _ 'h' _ 't' _ 'D' _ 'o' _ 'w' _ 'n' _ 'V' _ 'e' _ 'c' _ 't' _ 'o' _ 'r' _ ';',
    14, 0, 0x21c2 _ 0)
NAMED_CHARACTER_REFERENCE(
    463,
    /* R i */
    'g' _ 'h' _ 't' _ 'D' _ 'o' _ 'w' _ 'n' _ 'V' _ 'e' _ 'c' _ 't' _ 'o' _ 'r' _ 'B' _ 'a' _ 'r' _ ';',
    17, 0, 0x2955 _ 0)
NAMED_CHARACTER_REFERENCE(
    464,
    /* R i */ 'g' _ 'h' _ 't' _ 'F' _ 'l' _ 'o' _ 'o' _ 'r' _ ';', 9, 0,
    0x230b _ 0)
NAMED_CHARACTER_REFERENCE(465,
                          /* R i */ 'g' _ 'h' _ 't' _ 'T' _ 'e' _ 'e' _ ';', 7,
                          0, 0x22a2 _ 0)
NAMED_CHARACTER_REFERENCE(
    466,
    /* R i */
    'g' _ 'h' _ 't' _ 'T' _ 'e' _ 'e' _ 'A' _ 'r' _ 'r' _ 'o' _ 'w' _ ';', 12,
    0, 0x21a6 _ 0)
NAMED_CHARACTER_REFERENCE(
    467,
    /* R i */
    'g' _ 'h' _ 't' _ 'T' _ 'e' _ 'e' _ 'V' _ 'e' _ 'c' _ 't' _ 'o' _ 'r' _ ';',
    13, 0, 0x295b _ 0)
NAMED_CHARACTER_REFERENCE(
    468,
    /* R i */
    'g' _ 'h' _ 't' _ 'T' _ 'r' _ 'i' _ 'a' _ 'n' _ 'g' _ 'l' _ 'e' _ ';', 12,
    0, 0x22b3 _ 0)
NAMED_CHARACTER_REFERENCE(
    469,
    /* R i */
    'g' _ 'h' _ 't' _ 'T' _ 'r' _ 'i' _ 'a' _ 'n' _ 'g' _ 'l' _ 'e' _ 'B' _ 'a' _ 'r' _ ';',
    15, 0, 0x29d0 _ 0)
NAMED_CHARACTER_REFERENCE(
    470,
    /* R i */
    'g' _ 'h' _ 't' _ 'T' _ 'r' _ 'i' _ 'a' _ 'n' _ 'g' _ 'l' _ 'e' _ 'E' _ 'q' _ 'u' _ 'a' _ 'l' _ ';',
    17, 0, 0x22b5 _ 0)
NAMED_CHARACTER_REFERENCE(
    471,
    /* R i */
    'g' _ 'h' _ 't' _ 'U' _ 'p' _ 'D' _ 'o' _ 'w' _ 'n' _ 'V' _ 'e' _ 'c' _ 't' _ 'o' _ 'r' _ ';',
    16, 0, 0x294f _ 0)
NAMED_CHARACTER_REFERENCE(
    472,
    /* R i */
    'g' _ 'h' _ 't' _ 'U' _ 'p' _ 'T' _ 'e' _ 'e' _ 'V' _ 'e' _ 'c' _ 't' _ 'o' _ 'r' _ ';',
    15, 0, 0x295c _ 0)
NAMED_CHARACTER_REFERENCE(
    473,
    /* R i */
    'g' _ 'h' _ 't' _ 'U' _ 'p' _ 'V' _ 'e' _ 'c' _ 't' _ 'o' _ 'r' _ ';', 12,
    0, 0x21be _ 0)
NAMED_CHARACTER_REFERENCE(
    474,
    /* R i */
    'g' _ 'h' _ 't' _ 'U' _ 'p' _ 'V' _ 'e' _ 'c' _ 't' _ 'o' _ 'r' _ 'B' _ 'a' _ 'r' _ ';',
    15, 0, 0x2954 _ 0)
NAMED_CHARACTER_REFERENCE(
    475,
    /* R i */ 'g' _ 'h' _ 't' _ 'V' _ 'e' _ 'c' _ 't' _ 'o' _ 'r' _ ';', 10, 0,
    0x21c0 _ 0)
NAMED_CHARACTER_REFERENCE(
    476,
    /* R i */
    'g' _ 'h' _ 't' _ 'V' _ 'e' _ 'c' _ 't' _ 'o' _ 'r' _ 'B' _ 'a' _ 'r' _ ';',
    13, 0, 0x2953 _ 0)
NAMED_CHARACTER_REFERENCE(
    477,
    /* R i */ 'g' _ 'h' _ 't' _ 'a' _ 'r' _ 'r' _ 'o' _ 'w' _ ';', 9, 0,
    0x21d2 _ 0)
NAMED_CHARACTER_REFERENCE(478, /* R o */ 'p' _ 'f' _ ';', 3, 0, 0x211d _ 0)
NAMED_CHARACTER_REFERENCE(
    479,
    /* R o */ 'u' _ 'n' _ 'd' _ 'I' _ 'm' _ 'p' _ 'l' _ 'i' _ 'e' _ 's' _ ';',
    11, 0, 0x2970 _ 0)
NAMED_CHARACTER_REFERENCE(
    480,
    /* R r */ 'i' _ 'g' _ 'h' _ 't' _ 'a' _ 'r' _ 'r' _ 'o' _ 'w' _ ';', 10, 0,
    0x21db _ 0)
NAMED_CHARACTER_REFERENCE(481, /* R s */ 'c' _ 'r' _ ';', 3, 0, 0x211b _ 0)
NAMED_CHARACTER_REFERENCE(482, /* R s */ 'h' _ ';', 2, 0, 0x21b1 _ 0)
NAMED_CHARACTER_REFERENCE(
    483,
    /* R u */ 'l' _ 'e' _ 'D' _ 'e' _ 'l' _ 'a' _ 'y' _ 'e' _ 'd' _ ';', 10, 0,
    0x29f4 _ 0)
NAMED_CHARACTER_REFERENCE(484,
                          /* S H */ 'C' _ 'H' _ 'c' _ 'y' _ ';', 5, 0,
                          0x0429 _ 0)
NAMED_CHARACTER_REFERENCE(485, /* S H */ 'c' _ 'y' _ ';', 3, 0, 0x0428 _ 0)
NAMED_CHARACTER_REFERENCE(486,
                          /* S O */ 'F' _ 'T' _ 'c' _ 'y' _ ';', 5, 0,
                          0x042c _ 0)
NAMED_CHARACTER_REFERENCE(487,
                          /* S a */ 'c' _ 'u' _ 't' _ 'e' _ ';', 5, 0,
                          0x015a _ 0)
NAMED_CHARACTER_REFERENCE(488, /* S c */ ';', 1, 0, 0x2abc _ 0)
NAMED_CHARACTER_REFERENCE(489,
                          /* S c */ 'a' _ 'r' _ 'o' _ 'n' _ ';', 5, 0,
                          0x0160 _ 0)
NAMED_CHARACTER_REFERENCE(490,
                          /* S c */ 'e' _ 'd' _ 'i' _ 'l' _ ';', 5, 0,
                          0x015e _ 0)
NAMED_CHARACTER_REFERENCE(491,
                          /* S c */ 'i' _ 'r' _ 'c' _ ';', 4, 0, 0x015c _ 0)
NAMED_CHARACTER_REFERENCE(492, /* S c */ 'y' _ ';', 2, 0, 0x0421 _ 0)
NAMED_CHARACTER_REFERENCE(493, /* S f */ 'r' _ ';', 2, 0, 0xd835 _ 0xdd16)
NAMED_CHARACTER_REFERENCE(
    494,
    /* S h */
    'o' _ 'r' _ 't' _ 'D' _ 'o' _ 'w' _ 'n' _ 'A' _ 'r' _ 'r' _ 'o' _ 'w' _ ';',
    13, 0, 0x2193 _ 0)
NAMED_CHARACTER_REFERENCE(
    495,
    /* S h */
    'o' _ 'r' _ 't' _ 'L' _ 'e' _ 'f' _ 't' _ 'A' _ 'r' _ 'r' _ 'o' _ 'w' _ ';',
    13, 0, 0x2190 _ 0)
NAMED_CHARACTER_REFERENCE(
    496,
    /* S h */
    'o' _ 'r' _ 't' _ 'R' _ 'i' _ 'g' _ 'h' _ 't' _ 'A' _ 'r' _ 'r' _ 'o' _ 'w' _ ';',
    14, 0, 0x2192 _ 0)
NAMED_CHARACTER_REFERENCE(
    497,
    /* S h */ 'o' _ 'r' _ 't' _ 'U' _ 'p' _ 'A' _ 'r' _ 'r' _ 'o' _ 'w' _ ';',
    11, 0, 0x2191 _ 0)
NAMED_CHARACTER_REFERENCE(498,
                          /* S i */ 'g' _ 'm' _ 'a' _ ';', 4, 0, 0x03a3 _ 0)
NAMED_CHARACTER_REFERENCE(
    499,
    /* S m */ 'a' _ 'l' _ 'l' _ 'C' _ 'i' _ 'r' _ 'c' _ 'l' _ 'e' _ ';', 10, 0,
    0x2218 _ 0)
NAMED_CHARACTER_REFERENCE(500, /* S o */ 'p' _ 'f' _ ';', 3, 0, 0xd835 _ 0xdd4a)
NAMED_CHARACTER_REFERENCE(501, /* S q */ 'r' _ 't' _ ';', 3, 0, 0x221a _ 0)
NAMED_CHARACTER_REFERENCE(502,
                          /* S q */ 'u' _ 'a' _ 'r' _ 'e' _ ';', 5, 0,
                          0x25a1 _ 0)
NAMED_CHARACTER_REFERENCE(
    503,
    /* S q */
    'u' _ 'a' _ 'r' _ 'e' _ 'I' _ 'n' _ 't' _ 'e' _ 'r' _ 's' _ 'e' _ 'c' _ 't' _ 'i' _ 'o' _ 'n' _ ';',
    17, 0, 0x2293 _ 0)
NAMED_CHARACTER_REFERENCE(
    504,
    /* S q */ 'u' _ 'a' _ 'r' _ 'e' _ 'S' _ 'u' _ 'b' _ 's' _ 'e' _ 't' _ ';',
    11, 0, 0x228f _ 0)
NAMED_CHARACTER_REFERENCE(
    505,
    /* S q */
    'u' _ 'a' _ 'r' _ 'e' _ 'S' _ 'u' _ 'b' _ 's' _ 'e' _ 't' _ 'E' _ 'q' _ 'u' _ 'a' _ 'l' _ ';',
    16, 0, 0x2291 _ 0)
NAMED_CHARACTER_REFERENCE(
    506,
    /* S q */
    'u' _ 'a' _ 'r' _ 'e' _ 'S' _ 'u' _ 'p' _ 'e' _ 'r' _ 's' _ 'e' _ 't' _ ';',
    13, 0, 0x2290 _ 0)
NAMED_CHARACTER_REFERENCE(
    507,
    /* S q */
    'u' _ 'a' _ 'r' _ 'e' _ 'S' _ 'u' _ 'p' _ 'e' _ 'r' _ 's' _ 'e' _ 't' _ 'E' _ 'q' _ 'u' _ 'a' _ 'l' _ ';',
    18, 0, 0x2292 _ 0)
NAMED_CHARACTER_REFERENCE(
    508,
    /* S q */ 'u' _ 'a' _ 'r' _ 'e' _ 'U' _ 'n' _ 'i' _ 'o' _ 'n' _ ';', 10, 0,
    0x2294 _ 0)
NAMED_CHARACTER_REFERENCE(509, /* S s */ 'c' _ 'r' _ ';', 3, 0, 0xd835 _ 0xdcae)
NAMED_CHARACTER_REFERENCE(510, /* S t */ 'a' _ 'r' _ ';', 3, 0, 0x22c6 _ 0)
NAMED_CHARACTER_REFERENCE(511, /* S u */ 'b' _ ';', 2, 0, 0x22d0 _ 0)
NAMED_CHARACTER_REFERENCE(512,
                          /* S u */ 'b' _ 's' _ 'e' _ 't' _ ';', 5, 0,
                          0x22d0 _ 0)
NAMED_CHARACTER_REFERENCE(
    513,
    /* S u */ 'b' _ 's' _ 'e' _ 't' _ 'E' _ 'q' _ 'u' _ 'a' _ 'l' _ ';', 10, 0,
    0x2286 _ 0)
NAMED_CHARACTER_REFERENCE(514,
                          /* S u */ 'c' _ 'c' _ 'e' _ 'e' _ 'd' _ 's' _ ';', 7,
                          0, 0x227b _ 0)
NAMED_CHARACTER_REFERENCE(
    515,
    /* S u */
    'c' _ 'c' _ 'e' _ 'e' _ 'd' _ 's' _ 'E' _ 'q' _ 'u' _ 'a' _ 'l' _ ';', 12,
    0, 0x2ab0 _ 0)
NAMED_CHARACTER_REFERENCE(
    516,
    /* S u */
    'c' _ 'c' _ 'e' _ 'e' _ 'd' _ 's' _ 'S' _ 'l' _ 'a' _ 'n' _ 't' _ 'E' _ 'q' _ 'u' _ 'a' _ 'l' _ ';',
    17, 0, 0x227d _ 0)
NAMED_CHARACTER_REFERENCE(
    517,
    /* S u */
    'c' _ 'c' _ 'e' _ 'e' _ 'd' _ 's' _ 'T' _ 'i' _ 'l' _ 'd' _ 'e' _ ';', 12,
    0, 0x227f _ 0)
NAMED_CHARACTER_REFERENCE(518,
                          /* S u */ 'c' _ 'h' _ 'T' _ 'h' _ 'a' _ 't' _ ';', 7,
                          0, 0x220b _ 0)
NAMED_CHARACTER_REFERENCE(519, /* S u */ 'm' _ ';', 2, 0, 0x2211 _ 0)
NAMED_CHARACTER_REFERENCE(520, /* S u */ 'p' _ ';', 2, 0, 0x22d1 _ 0)
NAMED_CHARACTER_REFERENCE(521,
                          /* S u */ 'p' _ 'e' _ 'r' _ 's' _ 'e' _ 't' _ ';', 7,
                          0, 0x2283 _ 0)
NAMED_CHARACTER_REFERENCE(
    522,
    /* S u */
    'p' _ 'e' _ 'r' _ 's' _ 'e' _ 't' _ 'E' _ 'q' _ 'u' _ 'a' _ 'l' _ ';', 12,
    0, 0x2287 _ 0)
NAMED_CHARACTER_REFERENCE(523,
                          /* S u */ 'p' _ 's' _ 'e' _ 't' _ ';', 5, 0,
                          0x22d1 _ 0)
NAMED_CHARACTER_REFERENCE(524, /* T H */ 'O' _ 'R' _ 'N', 3, 0, 0x00de _ 0)
NAMED_CHARACTER_REFERENCE(525,
                          /* T H */ 'O' _ 'R' _ 'N' _ ';', 4, 0, 0x00de _ 0)
NAMED_CHARACTER_REFERENCE(526,
                          /* T R */ 'A' _ 'D' _ 'E' _ ';', 4, 0, 0x2122 _ 0)
NAMED_CHARACTER_REFERENCE(527,
                          /* T S */ 'H' _ 'c' _ 'y' _ ';', 4, 0, 0x040b _ 0)
NAMED_CHARACTER_REFERENCE(528, /* T S */ 'c' _ 'y' _ ';', 3, 0, 0x0426 _ 0)
NAMED_CHARACTER_REFERENCE(529, /* T a */ 'b' _ ';', 2, 0, 0x0009 _ 0)
NAMED_CHARACTER_REFERENCE(530, /* T a */ 'u' _ ';', 2, 0, 0x03a4 _ 0)
NAMED_CHARACTER_REFERENCE(531,
                          /* T c */ 'a' _ 'r' _ 'o' _ 'n' _ ';', 5, 0,
                          0x0164 _ 0)
NAMED_CHARACTER_REFERENCE(532,
                          /* T c */ 'e' _ 'd' _ 'i' _ 'l' _ ';', 5, 0,
                          0x0162 _ 0)
NAMED_CHARACTER_REFERENCE(533, /* T c */ 'y' _ ';', 2, 0, 0x0422 _ 0)
NAMED_CHARACTER_REFERENCE(534, /* T f */ 'r' _ ';', 2, 0, 0xd835 _ 0xdd17)
NAMED_CHARACTER_REFERENCE(
    535,
    /* T h */ 'e' _ 'r' _ 'e' _ 'f' _ 'o' _ 'r' _ 'e' _ ';', 8, 0, 0x2234 _ 0)
NAMED_CHARACTER_REFERENCE(536,
                          /* T h */ 'e' _ 't' _ 'a' _ ';', 4, 0, 0x0398 _ 0)
NAMED_CHARACTER_REFERENCE(
    537,
    /* T h */ 'i' _ 'c' _ 'k' _ 'S' _ 'p' _ 'a' _ 'c' _ 'e' _ ';', 9, 0,
    0x205f _ 0x200a)
NAMED_CHARACTER_REFERENCE(
    538,
    /* T h */ 'i' _ 'n' _ 'S' _ 'p' _ 'a' _ 'c' _ 'e' _ ';', 8, 0, 0x2009 _ 0)
NAMED_CHARACTER_REFERENCE(539,
                          /* T i */ 'l' _ 'd' _ 'e' _ ';', 4, 0, 0x223c _ 0)
NAMED_CHARACTER_REFERENCE(
    540,
    /* T i */ 'l' _ 'd' _ 'e' _ 'E' _ 'q' _ 'u' _ 'a' _ 'l' _ ';', 9, 0,
    0x2243 _ 0)
NAMED_CHARACTER_REFERENCE(
    541,
    /* T i */
    'l' _ 'd' _ 'e' _ 'F' _ 'u' _ 'l' _ 'l' _ 'E' _ 'q' _ 'u' _ 'a' _ 'l' _ ';',
    13, 0, 0x2245 _ 0)
NAMED_CHARACTER_REFERENCE(
    542,
    /* T i */ 'l' _ 'd' _ 'e' _ 'T' _ 'i' _ 'l' _ 'd' _ 'e' _ ';', 9, 0,
    0x2248 _ 0)
NAMED_CHARACTER_REFERENCE(543, /* T o */ 'p' _ 'f' _ ';', 3, 0, 0xd835 _ 0xdd4b)
NAMED_CHARACTER_REFERENCE(
    544,
    /* T r */ 'i' _ 'p' _ 'l' _ 'e' _ 'D' _ 'o' _ 't' _ ';', 8, 0, 0x20db _ 0)
NAMED_CHARACTER_REFERENCE(545, /* T s */ 'c' _ 'r' _ ';', 3, 0, 0xd835 _ 0xdcaf)
NAMED_CHARACTER_REFERENCE(546,
                          /* T s */ 't' _ 'r' _ 'o' _ 'k' _ ';', 5, 0,
                          0x0166 _ 0)
NAMED_CHARACTER_REFERENCE(547,
                          /* U a */ 'c' _ 'u' _ 't' _ 'e', 4, 0, 0x00da _ 0)
NAMED_CHARACTER_REFERENCE(548,
                          /* U a */ 'c' _ 'u' _ 't' _ 'e' _ ';', 5, 0,
                          0x00da _ 0)
NAMED_CHARACTER_REFERENCE(549, /* U a */ 'r' _ 'r' _ ';', 3, 0, 0x219f _ 0)
NAMED_CHARACTER_REFERENCE(550,
                          /* U a */ 'r' _ 'r' _ 'o' _ 'c' _ 'i' _ 'r' _ ';', 7,
                          0, 0x2949 _ 0)
NAMED_CHARACTER_REFERENCE(551,
                          /* U b */ 'r' _ 'c' _ 'y' _ ';', 4, 0, 0x040e _ 0)
NAMED_CHARACTER_REFERENCE(552,
                          /* U b */ 'r' _ 'e' _ 'v' _ 'e' _ ';', 5, 0,
                          0x016c _ 0)
NAMED_CHARACTER_REFERENCE(553, /* U c */ 'i' _ 'r' _ 'c', 3, 0, 0x00db _ 0)
NAMED_CHARACTER_REFERENCE(554,
                          /* U c */ 'i' _ 'r' _ 'c' _ ';', 4, 0, 0x00db _ 0)
NAMED_CHARACTER_REFERENCE(555, /* U c */ 'y' _ ';', 2, 0, 0x0423 _ 0)
NAMED_CHARACTER_REFERENCE(556,
                          /* U d */ 'b' _ 'l' _ 'a' _ 'c' _ ';', 5, 0,
                          0x0170 _ 0)
NAMED_CHARACTER_REFERENCE(557, /* U f */ 'r' _ ';', 2, 0, 0xd835 _ 0xdd18)
NAMED_CHARACTER_REFERENCE(558,
                          /* U g */ 'r' _ 'a' _ 'v' _ 'e', 4, 0, 0x00d9 _ 0)
NAMED_CHARACTER_REFERENCE(559,
                          /* U g */ 'r' _ 'a' _ 'v' _ 'e' _ ';', 5, 0,
                          0x00d9 _ 0)
NAMED_CHARACTER_REFERENCE(560,
                          /* U m */ 'a' _ 'c' _ 'r' _ ';', 4, 0, 0x016a _ 0)
NAMED_CHARACTER_REFERENCE(561,
                          /* U n */ 'd' _ 'e' _ 'r' _ 'B' _ 'a' _ 'r' _ ';', 7,
                          0, 0x005f _ 0)
NAMED_CHARACTER_REFERENCE(
    562,
    /* U n */ 'd' _ 'e' _ 'r' _ 'B' _ 'r' _ 'a' _ 'c' _ 'e' _ ';', 9, 0,
    0x23df _ 0)
NAMED_CHARACTER_REFERENCE(
    563,
    /* U n */ 'd' _ 'e' _ 'r' _ 'B' _ 'r' _ 'a' _ 'c' _ 'k' _ 'e' _ 't' _ ';',
    11, 0, 0x23b5 _ 0)
NAMED_CHARACTER_REFERENCE(
    564,
    /* U n */
    'd' _ 'e' _ 'r' _ 'P' _ 'a' _ 'r' _ 'e' _ 'n' _ 't' _ 'h' _ 'e' _ 's' _ 'i' _ 's' _ ';',
    15, 0, 0x23dd _ 0)
NAMED_CHARACTER_REFERENCE(565,
                          /* U n */ 'i' _ 'o' _ 'n' _ ';', 4, 0, 0x22c3 _ 0)
NAMED_CHARACTER_REFERENCE(
    566,
    /* U n */ 'i' _ 'o' _ 'n' _ 'P' _ 'l' _ 'u' _ 's' _ ';', 8, 0, 0x228e _ 0)
NAMED_CHARACTER_REFERENCE(567,
                          /* U o */ 'g' _ 'o' _ 'n' _ ';', 4, 0, 0x0172 _ 0)
NAMED_CHARACTER_REFERENCE(568, /* U o */ 'p' _ 'f' _ ';', 3, 0, 0xd835 _ 0xdd4c)
NAMED_CHARACTER_REFERENCE(569,
                          /* U p */ 'A' _ 'r' _ 'r' _ 'o' _ 'w' _ ';', 6, 0,
                          0x2191 _ 0)
NAMED_CHARACTER_REFERENCE(
    570,
    /* U p */ 'A' _ 'r' _ 'r' _ 'o' _ 'w' _ 'B' _ 'a' _ 'r' _ ';', 9, 0,
    0x2912 _ 0)
NAMED_CHARACTER_REFERENCE(
    571,
    /* U p */
    'A' _ 'r' _ 'r' _ 'o' _ 'w' _ 'D' _ 'o' _ 'w' _ 'n' _ 'A' _ 'r' _ 'r' _ 'o' _ 'w' _ ';',
    15, 0, 0x21c5 _ 0)
NAMED_CHARACTER_REFERENCE(
    572,
    /* U p */ 'D' _ 'o' _ 'w' _ 'n' _ 'A' _ 'r' _ 'r' _ 'o' _ 'w' _ ';', 10, 0,
    0x2195 _ 0)
NAMED_CHARACTER_REFERENCE(
    573,
    /* U p */
    'E' _ 'q' _ 'u' _ 'i' _ 'l' _ 'i' _ 'b' _ 'r' _ 'i' _ 'u' _ 'm' _ ';', 12,
    0, 0x296e _ 0)
NAMED_CHARACTER_REFERENCE(574,
                          /* U p */ 'T' _ 'e' _ 'e' _ ';', 4, 0, 0x22a5 _ 0)
NAMED_CHARACTER_REFERENCE(
    575,
    /* U p */ 'T' _ 'e' _ 'e' _ 'A' _ 'r' _ 'r' _ 'o' _ 'w' _ ';', 9, 0,
    0x21a5 _ 0)
NAMED_CHARACTER_REFERENCE(576,
                          /* U p */ 'a' _ 'r' _ 'r' _ 'o' _ 'w' _ ';', 6, 0,
                          0x21d1 _ 0)
NAMED_CHARACTER_REFERENCE(
    577,
    /* U p */ 'd' _ 'o' _ 'w' _ 'n' _ 'a' _ 'r' _ 'r' _ 'o' _ 'w' _ ';', 10, 0,
    0x21d5 _ 0)
NAMED_CHARACTER_REFERENCE(
    578,
    /* U p */
    'p' _ 'e' _ 'r' _ 'L' _ 'e' _ 'f' _ 't' _ 'A' _ 'r' _ 'r' _ 'o' _ 'w' _ ';',
    13, 0, 0x2196 _ 0)
NAMED_CHARACTER_REFERENCE(
    579,
    /* U p */
    'p' _ 'e' _ 'r' _ 'R' _ 'i' _ 'g' _ 'h' _ 't' _ 'A' _ 'r' _ 'r' _ 'o' _ 'w' _ ';',
    14, 0, 0x2197 _ 0)
NAMED_CHARACTER_REFERENCE(580, /* U p */ 's' _ 'i' _ ';', 3, 0, 0x03d2 _ 0)
NAMED_CHARACTER_REFERENCE(581,
                          /* U p */ 's' _ 'i' _ 'l' _ 'o' _ 'n' _ ';', 6, 0,
                          0x03a5 _ 0)
NAMED_CHARACTER_REFERENCE(582,
                          /* U r */ 'i' _ 'n' _ 'g' _ ';', 4, 0, 0x016e _ 0)
NAMED_CHARACTER_REFERENCE(583, /* U s */ 'c' _ 'r' _ ';', 3, 0, 0xd835 _ 0xdcb0)
NAMED_CHARACTER_REFERENCE(584,
                          /* U t */ 'i' _ 'l' _ 'd' _ 'e' _ ';', 5, 0,
                          0x0168 _ 0)
NAMED_CHARACTER_REFERENCE(585, /* U u */ 'm' _ 'l', 2, 0, 0x00dc _ 0)
NAMED_CHARACTER_REFERENCE(586, /* U u */ 'm' _ 'l' _ ';', 3, 0, 0x00dc _ 0)
NAMED_CHARACTER_REFERENCE(587,
                          /* V D */ 'a' _ 's' _ 'h' _ ';', 4, 0, 0x22ab _ 0)
NAMED_CHARACTER_REFERENCE(588, /* V b */ 'a' _ 'r' _ ';', 3, 0, 0x2aeb _ 0)
NAMED_CHARACTER_REFERENCE(589, /* V c */ 'y' _ ';', 2, 0, 0x0412 _ 0)
NAMED_CHARACTER_REFERENCE(590,
                          /* V d */ 'a' _ 's' _ 'h' _ ';', 4, 0, 0x22a9 _ 0)
NAMED_CHARACTER_REFERENCE(591,
                          /* V d */ 'a' _ 's' _ 'h' _ 'l' _ ';', 5, 0,
                          0x2ae6 _ 0)
NAMED_CHARACTER_REFERENCE(592, /* V e */ 'e' _ ';', 2, 0, 0x22c1 _ 0)
NAMED_CHARACTER_REFERENCE(593,
                          /* V e */ 'r' _ 'b' _ 'a' _ 'r' _ ';', 5, 0,
                          0x2016 _ 0)
NAMED_CHARACTER_REFERENCE(594, /* V e */ 'r' _ 't' _ ';', 3, 0, 0x2016 _ 0)
NAMED_CHARACTER_REFERENCE(
    595,
    /* V e */ 'r' _ 't' _ 'i' _ 'c' _ 'a' _ 'l' _ 'B' _ 'a' _ 'r' _ ';', 10, 0,
    0x2223 _ 0)
NAMED_CHARACTER_REFERENCE(
    596,
    /* V e */ 'r' _ 't' _ 'i' _ 'c' _ 'a' _ 'l' _ 'L' _ 'i' _ 'n' _ 'e' _ ';',
    11, 0, 0x007c _ 0)
NAMED_CHARACTER_REFERENCE(
    597,
    /* V e */
    'r' _ 't' _ 'i' _ 'c' _ 'a' _ 'l' _ 'S' _ 'e' _ 'p' _ 'a' _ 'r' _ 'a' _ 't' _ 'o' _ 'r' _ ';',
    16, 0, 0x2758 _ 0)
NAMED_CHARACTER_REFERENCE(
    598,
    /* V e */
    'r' _ 't' _ 'i' _ 'c' _ 'a' _ 'l' _ 'T' _ 'i' _ 'l' _ 'd' _ 'e' _ ';', 12,
    0, 0x2240 _ 0)
NAMED_CHARACTER_REFERENCE(
    599,
    /* V e */
    'r' _ 'y' _ 'T' _ 'h' _ 'i' _ 'n' _ 'S' _ 'p' _ 'a' _ 'c' _ 'e' _ ';', 12,
    0, 0x200a _ 0)
NAMED_CHARACTER_REFERENCE(600, /* V f */ 'r' _ ';', 2, 0, 0xd835 _ 0xdd19)
NAMED_CHARACTER_REFERENCE(601, /* V o */ 'p' _ 'f' _ ';', 3, 0, 0xd835 _ 0xdd4d)
NAMED_CHARACTER_REFERENCE(602, /* V s */ 'c' _ 'r' _ ';', 3, 0, 0xd835 _ 0xdcb1)
NAMED_CHARACTER_REFERENCE(603,
                          /* V v */ 'd' _ 'a' _ 's' _ 'h' _ ';', 5, 0,
                          0x22aa _ 0)
NAMED_CHARACTER_REFERENCE(604,
                          /* W c */ 'i' _ 'r' _ 'c' _ ';', 4, 0, 0x0174 _ 0)
NAMED_CHARACTER_REFERENCE(605,
                          /* W e */ 'd' _ 'g' _ 'e' _ ';', 4, 0, 0x22c0 _ 0)
NAMED_CHARACTER_REFERENCE(606, /* W f */ 'r' _ ';', 2, 0, 0xd835 _ 0xdd1a)
NAMED_CHARACTER_REFERENCE(607, /* W o */ 'p' _ 'f' _ ';', 3, 0, 0xd835 _ 0xdd4e)
NAMED_CHARACTER_REFERENCE(608, /* W s */ 'c' _ 'r' _ ';', 3, 0, 0xd835 _ 0xdcb2)
NAMED_CHARACTER_REFERENCE(609, /* X f */ 'r' _ ';', 2, 0, 0xd835 _ 0xdd1b)
NAMED_CHARACTER_REFERENCE(610, /* X i */ ';', 1, 0, 0x039e _ 0)
NAMED_CHARACTER_REFERENCE(611, /* X o */ 'p' _ 'f' _ ';', 3, 0, 0xd835 _ 0xdd4f)
NAMED_CHARACTER_REFERENCE(612, /* X s */ 'c' _ 'r' _ ';', 3, 0, 0xd835 _ 0xdcb3)
NAMED_CHARACTER_REFERENCE(613, /* Y A */ 'c' _ 'y' _ ';', 3, 0, 0x042f _ 0)
NAMED_CHARACTER_REFERENCE(614, /* Y I */ 'c' _ 'y' _ ';', 3, 0, 0x0407 _ 0)
NAMED_CHARACTER_REFERENCE(615, /* Y U */ 'c' _ 'y' _ ';', 3, 0, 0x042e _ 0)
NAMED_CHARACTER_REFERENCE(616,
                          /* Y a */ 'c' _ 'u' _ 't' _ 'e', 4, 0, 0x00dd _ 0)
NAMED_CHARACTER_REFERENCE(617,
                          /* Y a */ 'c' _ 'u' _ 't' _ 'e' _ ';', 5, 0,
                          0x00dd _ 0)
NAMED_CHARACTER_REFERENCE(618,
                          /* Y c */ 'i' _ 'r' _ 'c' _ ';', 4, 0, 0x0176 _ 0)
NAMED_CHARACTER_REFERENCE(619, /* Y c */ 'y' _ ';', 2, 0, 0x042b _ 0)
NAMED_CHARACTER_REFERENCE(620, /* Y f */ 'r' _ ';', 2, 0, 0xd835 _ 0xdd1c)
NAMED_CHARACTER_REFERENCE(621, /* Y o */ 'p' _ 'f' _ ';', 3, 0, 0xd835 _ 0xdd50)
NAMED_CHARACTER_REFERENCE(622, /* Y s */ 'c' _ 'r' _ ';', 3, 0, 0xd835 _ 0xdcb4)
NAMED_CHARACTER_REFERENCE(623, /* Y u */ 'm' _ 'l' _ ';', 3, 0, 0x0178 _ 0)
NAMED_CHARACTER_REFERENCE(624, /* Z H */ 'c' _ 'y' _ ';', 3, 0, 0x0416 _ 0)
NAMED_CHARACTER_REFERENCE(625,
                          /* Z a */ 'c' _ 'u' _ 't' _ 'e' _ ';', 5, 0,
                          0x0179 _ 0)
NAMED_CHARACTER_REFERENCE(626,
                          /* Z c */ 'a' _ 'r' _ 'o' _ 'n' _ ';', 5, 0,
                          0x017d _ 0)
NAMED_CHARACTER_REFERENCE(627, /* Z c */ 'y' _ ';', 2, 0, 0x0417 _ 0)
NAMED_CHARACTER_REFERENCE(628, /* Z d */ 'o' _ 't' _ ';', 3, 0, 0x017b _ 0)
NAMED_CHARACTER_REFERENCE(
    629,
    /* Z e */
    'r' _ 'o' _ 'W' _ 'i' _ 'd' _ 't' _ 'h' _ 'S' _ 'p' _ 'a' _ 'c' _ 'e' _ ';',
    13, 0, 0x200b _ 0)
NAMED_CHARACTER_REFERENCE(630, /* Z e */ 't' _ 'a' _ ';', 3, 0, 0x0396 _ 0)
NAMED_CHARACTER_REFERENCE(631, /* Z f */ 'r' _ ';', 2, 0, 0x2128 _ 0)
NAMED_CHARACTER_REFERENCE(632, /* Z o */ 'p' _ 'f' _ ';', 3, 0, 0x2124 _ 0)
NAMED_CHARACTER_REFERENCE(633, /* Z s */ 'c' _ 'r' _ ';', 3, 0, 0xd835 _ 0xdcb5)
NAMED_CHARACTER_REFERENCE(634,
                          /* a a */ 'c' _ 'u' _ 't' _ 'e', 4, 0, 0x00e1 _ 0)
NAMED_CHARACTER_REFERENCE(635,
                          /* a a */ 'c' _ 'u' _ 't' _ 'e' _ ';', 5, 0,
                          0x00e1 _ 0)
NAMED_CHARACTER_REFERENCE(636,
                          /* a b */ 'r' _ 'e' _ 'v' _ 'e' _ ';', 5, 0,
                          0x0103 _ 0)
NAMED_CHARACTER_REFERENCE(637, /* a c */ ';', 1, 0, 0x223e _ 0)
NAMED_CHARACTER_REFERENCE(638, /* a c */ 'E' _ ';', 2, 0, 0x223e _ 0x0333)
NAMED_CHARACTER_REFERENCE(639, /* a c */ 'd' _ ';', 2, 0, 0x223f _ 0)
NAMED_CHARACTER_REFERENCE(640, /* a c */ 'i' _ 'r' _ 'c', 3, 0, 0x00e2 _ 0)
NAMED_CHARACTER_REFERENCE(641,
                          /* a c */ 'i' _ 'r' _ 'c' _ ';', 4, 0, 0x00e2 _ 0)
NAMED_CHARACTER_REFERENCE(642, /* a c */ 'u' _ 't' _ 'e', 3, 0, 0x00b4 _ 0)
NAMED_CHARACTER_REFERENCE(643,
                          /* a c */ 'u' _ 't' _ 'e' _ ';', 4, 0, 0x00b4 _ 0)
NAMED_CHARACTER_REFERENCE(644, /* a c */ 'y' _ ';', 2, 0, 0x0430 _ 0)
NAMED_CHARACTER_REFERENCE(645, /* a e */ 'l' _ 'i' _ 'g', 3, 0, 0x00e6 _ 0)
NAMED_CHARACTER_REFERENCE(646,
                          /* a e */ 'l' _ 'i' _ 'g' _ ';', 4, 0, 0x00e6 _ 0)
NAMED_CHARACTER_REFERENCE(647, /* a f */ ';', 1, 0, 0x2061 _ 0)
NAMED_CHARACTER_REFERENCE(648, /* a f */ 'r' _ ';', 2, 0, 0xd835 _ 0xdd1e)
NAMED_CHARACTER_REFERENCE(649,
                          /* a g */ 'r' _ 'a' _ 'v' _ 'e', 4, 0, 0x00e0 _ 0)
NAMED_CHARACTER_REFERENCE(650,
                          /* a g */ 'r' _ 'a' _ 'v' _ 'e' _ ';', 5, 0,
                          0x00e0 _ 0)
NAMED_CHARACTER_REFERENCE(651,
                          /* a l */ 'e' _ 'f' _ 's' _ 'y' _ 'm' _ ';', 6, 0,
                          0x2135 _ 0)
NAMED_CHARACTER_REFERENCE(652,
                          /* a l */ 'e' _ 'p' _ 'h' _ ';', 4, 0, 0x2135 _ 0)
NAMED_CHARACTER_REFERENCE(653,
                          /* a l */ 'p' _ 'h' _ 'a' _ ';', 4, 0, 0x03b1 _ 0)
NAMED_CHARACTER_REFERENCE(654,
                          /* a m */ 'a' _ 'c' _ 'r' _ ';', 4, 0, 0x0101 _ 0)
NAMED_CHARACTER_REFERENCE(655,
                          /* a m */ 'a' _ 'l' _ 'g' _ ';', 4, 0, 0x2a3f _ 0)
NAMED_CHARACTER_REFERENCE(656, /* a m */ 'p', 1, 0, 0x0026 _ 0)
NAMED_CHARACTER_REFERENCE(657, /* a m */ 'p' _ ';', 2, 0, 0x0026 _ 0)
NAMED_CHARACTER_REFERENCE(658, /* a n */ 'd' _ ';', 2, 0, 0x2227 _ 0)
NAMED_CHARACTER_REFERENCE(659,
                          /* a n */ 'd' _ 'a' _ 'n' _ 'd' _ ';', 5, 0,
                          0x2a55 _ 0)
NAMED_CHARACTER_REFERENCE(660, /* a n */ 'd' _ 'd' _ ';', 3, 0, 0x2a5c _ 0)
NAMED_CHARACTER_REFERENCE(661,
                          /* a n */ 'd' _ 's' _ 'l' _ 'o' _ 'p' _ 'e' _ ';', 7,
                          0, 0x2a58 _ 0)
NAMED_CHARACTER_REFERENCE(662, /* a n */ 'd' _ 'v' _ ';', 3, 0, 0x2a5a _ 0)
NAMED_CHARACTER_REFERENCE(663, /* a n */ 'g' _ ';', 2, 0, 0x2220 _ 0)
NAMED_CHARACTER_REFERENCE(664, /* a n */ 'g' _ 'e' _ ';', 3, 0, 0x29a4 _ 0)
NAMED_CHARACTER_REFERENCE(665,
                          /* a n */ 'g' _ 'l' _ 'e' _ ';', 4, 0, 0x2220 _ 0)
NAMED_CHARACTER_REFERENCE(666,
                          /* a n */ 'g' _ 'm' _ 's' _ 'd' _ ';', 5, 0,
                          0x2221 _ 0)
NAMED_CHARACTER_REFERENCE(667,
                          /* a n */ 'g' _ 'm' _ 's' _ 'd' _ 'a' _ 'a' _ ';', 7,
                          0, 0x29a8 _ 0)
NAMED_CHARACTER_REFERENCE(668,
                          /* a n */ 'g' _ 'm' _ 's' _ 'd' _ 'a' _ 'b' _ ';', 7,
                          0, 0x29a9 _ 0)
NAMED_CHARACTER_REFERENCE(669,
                          /* a n */ 'g' _ 'm' _ 's' _ 'd' _ 'a' _ 'c' _ ';', 7,
                          0, 0x29aa _ 0)
NAMED_CHARACTER_REFERENCE(670,
                          /* a n */ 'g' _ 'm' _ 's' _ 'd' _ 'a' _ 'd' _ ';', 7,
                          0, 0x29ab _ 0)
NAMED_CHARACTER_REFERENCE(671,
                          /* a n */ 'g' _ 'm' _ 's' _ 'd' _ 'a' _ 'e' _ ';', 7,
                          0, 0x29ac _ 0)
NAMED_CHARACTER_REFERENCE(672,
                          /* a n */ 'g' _ 'm' _ 's' _ 'd' _ 'a' _ 'f' _ ';', 7,
                          0, 0x29ad _ 0)
NAMED_CHARACTER_REFERENCE(673,
                          /* a n */ 'g' _ 'm' _ 's' _ 'd' _ 'a' _ 'g' _ ';', 7,
                          0, 0x29ae _ 0)
NAMED_CHARACTER_REFERENCE(674,
                          /* a n */ 'g' _ 'm' _ 's' _ 'd' _ 'a' _ 'h' _ ';', 7,
                          0, 0x29af _ 0)
NAMED_CHARACTER_REFERENCE(675,
                          /* a n */ 'g' _ 'r' _ 't' _ ';', 4, 0, 0x221f _ 0)
NAMED_CHARACTER_REFERENCE(676,
                          /* a n */ 'g' _ 'r' _ 't' _ 'v' _ 'b' _ ';', 6, 0,
                          0x22be _ 0)
NAMED_CHARACTER_REFERENCE(677,
                          /* a n */ 'g' _ 'r' _ 't' _ 'v' _ 'b' _ 'd' _ ';', 7,
                          0, 0x299d _ 0)
NAMED_CHARACTER_REFERENCE(678,
                          /* a n */ 'g' _ 's' _ 'p' _ 'h' _ ';', 5, 0,
                          0x2222 _ 0)
NAMED_CHARACTER_REFERENCE(679,
                          /* a n */ 'g' _ 's' _ 't' _ ';', 4, 0, 0x00c5 _ 0)
NAMED_CHARACTER_REFERENCE(680,
                          /* a n */ 'g' _ 'z' _ 'a' _ 'r' _ 'r' _ ';', 6, 0,
                          0x237c _ 0)
NAMED_CHARACTER_REFERENCE(681,
                          /* a o */ 'g' _ 'o' _ 'n' _ ';', 4, 0, 0x0105 _ 0)
NAMED_CHARACTER_REFERENCE(682, /* a o */ 'p' _ 'f' _ ';', 3, 0, 0xd835 _ 0xdd52)
NAMED_CHARACTER_REFERENCE(683, /* a p */ ';', 1, 0, 0x2248 _ 0)
NAMED_CHARACTER_REFERENCE(684, /* a p */ 'E' _ ';', 2, 0, 0x2a70 _ 0)
NAMED_CHARACTER_REFERENCE(685,
                          /* a p */ 'a' _ 'c' _ 'i' _ 'r' _ ';', 5, 0,
                          0x2a6f _ 0)
NAMED_CHARACTER_REFERENCE(686, /* a p */ 'e' _ ';', 2, 0, 0x224a _ 0)
NAMED_CHARACTER_REFERENCE(687, /* a p */ 'i' _ 'd' _ ';', 3, 0, 0x224b _ 0)
NAMED_CHARACTER_REFERENCE(688, /* a p */ 'o' _ 's' _ ';', 3, 0, 0x0027 _ 0)
NAMED_CHARACTER_REFERENCE(689,
                          /* a p */ 'p' _ 'r' _ 'o' _ 'x' _ ';', 5, 0,
                          0x2248 _ 0)
NAMED_CHARACTER_REFERENCE(690,
                          /* a p */ 'p' _ 'r' _ 'o' _ 'x' _ 'e' _ 'q' _ ';', 7,
                          0, 0x224a _ 0)
NAMED_CHARACTER_REFERENCE(691, /* a r */ 'i' _ 'n' _ 'g', 3, 0, 0x00e5 _ 0)
NAMED_CHARACTER_REFERENCE(692,
                          /* a r */ 'i' _ 'n' _ 'g' _ ';', 4, 0, 0x00e5 _ 0)
NAMED_CHARACTER_REFERENCE(693, /* a s */ 'c' _ 'r' _ ';', 3, 0, 0xd835 _ 0xdcb6)
NAMED_CHARACTER_REFERENCE(694, /* a s */ 't' _ ';', 2, 0, 0x002a _ 0)
NAMED_CHARACTER_REFERENCE(695,
                          /* a s */ 'y' _ 'm' _ 'p' _ ';', 4, 0, 0x2248 _ 0)
NAMED_CHARACTER_REFERENCE(696,
                          /* a s */ 'y' _ 'm' _ 'p' _ 'e' _ 'q' _ ';', 6, 0,
                          0x224d _ 0)
NAMED_CHARACTER_REFERENCE(697,
                          /* a t */ 'i' _ 'l' _ 'd' _ 'e', 4, 0, 0x00e3 _ 0)
NAMED_CHARACTER_REFERENCE(698,
                          /* a t */ 'i' _ 'l' _ 'd' _ 'e' _ ';', 5, 0,
                          0x00e3 _ 0)
NAMED_CHARACTER_REFERENCE(699, /* a u */ 'm' _ 'l', 2, 0, 0x00e4 _ 0)
NAMED_CHARACTER_REFERENCE(700, /* a u */ 'm' _ 'l' _ ';', 3, 0, 0x00e4 _ 0)
NAMED_CHARACTER_REFERENCE(701,
                          /* a w */ 'c' _ 'o' _ 'n' _ 'i' _ 'n' _ 't' _ ';', 7,
                          0, 0x2233 _ 0)
NAMED_CHARACTER_REFERENCE(702,
                          /* a w */ 'i' _ 'n' _ 't' _ ';', 4, 0, 0x2a11 _ 0)
NAMED_CHARACTER_REFERENCE(703, /* b N */ 'o' _ 't' _ ';', 3, 0, 0x2aed _ 0)
NAMED_CHARACTER_REFERENCE(704,
                          /* b a */ 'c' _ 'k' _ 'c' _ 'o' _ 'n' _ 'g' _ ';', 7,
                          0, 0x224c _ 0)
NAMED_CHARACTER_REFERENCE(
    705,
    /* b a */ 'c' _ 'k' _ 'e' _ 'p' _ 's' _ 'i' _ 'l' _ 'o' _ 'n' _ ';', 10, 0,
    0x03f6 _ 0)
NAMED_CHARACTER_REFERENCE(
    706,
    /* b a */ 'c' _ 'k' _ 'p' _ 'r' _ 'i' _ 'm' _ 'e' _ ';', 8, 0, 0x2035 _ 0)
NAMED_CHARACTER_REFERENCE(707,
                          /* b a */ 'c' _ 'k' _ 's' _ 'i' _ 'm' _ ';', 6, 0,
                          0x223d _ 0)
NAMED_CHARACTER_REFERENCE(
    708,
    /* b a */ 'c' _ 'k' _ 's' _ 'i' _ 'm' _ 'e' _ 'q' _ ';', 8, 0, 0x22cd _ 0)
NAMED_CHARACTER_REFERENCE(709,
                          /* b a */ 'r' _ 'v' _ 'e' _ 'e' _ ';', 5, 0,
                          0x22bd _ 0)
NAMED_CHARACTER_REFERENCE(710,
                          /* b a */ 'r' _ 'w' _ 'e' _ 'd' _ ';', 5, 0,
                          0x2305 _ 0)
NAMED_CHARACTER_REFERENCE(711,
                          /* b a */ 'r' _ 'w' _ 'e' _ 'd' _ 'g' _ 'e' _ ';', 7,
                          0, 0x2305 _ 0)
NAMED_CHARACTER_REFERENCE(712, /* b b */ 'r' _ 'k' _ ';', 3, 0, 0x23b5 _ 0)
NAMED_CHARACTER_REFERENCE(713,
                          /* b b */ 'r' _ 'k' _ 't' _ 'b' _ 'r' _ 'k' _ ';', 7,
                          0, 0x23b6 _ 0)
NAMED_CHARACTER_REFERENCE(714,
                          /* b c */ 'o' _ 'n' _ 'g' _ ';', 4, 0, 0x224c _ 0)
NAMED_CHARACTER_REFERENCE(715, /* b c */ 'y' _ ';', 2, 0, 0x0431 _ 0)
NAMED_CHARACTER_REFERENCE(716,
                          /* b d */ 'q' _ 'u' _ 'o' _ ';', 4, 0, 0x201e _ 0)
NAMED_CHARACTER_REFERENCE(717,
                          /* b e */ 'c' _ 'a' _ 'u' _ 's' _ ';', 5, 0,
                          0x2235 _ 0)
NAMED_CHARACTER_REFERENCE(718,
                          /* b e */ 'c' _ 'a' _ 'u' _ 's' _ 'e' _ ';', 6, 0,
                          0x2235 _ 0)
NAMED_CHARACTER_REFERENCE(719,
                          /* b e */ 'm' _ 'p' _ 't' _ 'y' _ 'v' _ ';', 6, 0,
                          0x29b0 _ 0)
NAMED_CHARACTER_REFERENCE(720,
                          /* b e */ 'p' _ 's' _ 'i' _ ';', 4, 0, 0x03f6 _ 0)
NAMED_CHARACTER_REFERENCE(721,
                          /* b e */ 'r' _ 'n' _ 'o' _ 'u' _ ';', 5, 0,
                          0x212c _ 0)
NAMED_CHARACTER_REFERENCE(722, /* b e */ 't' _ 'a' _ ';', 3, 0, 0x03b2 _ 0)
NAMED_CHARACTER_REFERENCE(723, /* b e */ 't' _ 'h' _ ';', 3, 0, 0x2136 _ 0)
NAMED_CHARACTER_REFERENCE(724,
                          /* b e */ 't' _ 'w' _ 'e' _ 'e' _ 'n' _ ';', 6, 0,
                          0x226c _ 0)
NAMED_CHARACTER_REFERENCE(725, /* b f */ 'r' _ ';', 2, 0, 0xd835 _ 0xdd1f)
NAMED_CHARACTER_REFERENCE(726,
                          /* b i */ 'g' _ 'c' _ 'a' _ 'p' _ ';', 5, 0,
                          0x22c2 _ 0)
NAMED_CHARACTER_REFERENCE(727,
                          /* b i */ 'g' _ 'c' _ 'i' _ 'r' _ 'c' _ ';', 6, 0,
                          0x25ef _ 0)
NAMED_CHARACTER_REFERENCE(728,
                          /* b i */ 'g' _ 'c' _ 'u' _ 'p' _ ';', 5, 0,
                          0x22c3 _ 0)
NAMED_CHARACTER_REFERENCE(729,
                          /* b i */ 'g' _ 'o' _ 'd' _ 'o' _ 't' _ ';', 6, 0,
                          0x2a00 _ 0)
NAMED_CHARACTER_REFERENCE(730,
                          /* b i */ 'g' _ 'o' _ 'p' _ 'l' _ 'u' _ 's' _ ';', 7,
                          0, 0x2a01 _ 0)
NAMED_CHARACTER_REFERENCE(
    731,
    /* b i */ 'g' _ 'o' _ 't' _ 'i' _ 'm' _ 'e' _ 's' _ ';', 8, 0, 0x2a02 _ 0)
NAMED_CHARACTER_REFERENCE(732,
                          /* b i */ 'g' _ 's' _ 'q' _ 'c' _ 'u' _ 'p' _ ';', 7,
                          0, 0x2a06 _ 0)
NAMED_CHARACTER_REFERENCE(733,
                          /* b i */ 'g' _ 's' _ 't' _ 'a' _ 'r' _ ';', 6, 0,
                          0x2605 _ 0)
NAMED_CHARACTER_REFERENCE(
    734,
    /* b i */
    'g' _ 't' _ 'r' _ 'i' _ 'a' _ 'n' _ 'g' _ 'l' _ 'e' _ 'd' _ 'o' _ 'w' _ 'n' _ ';',
    14, 0, 0x25bd _ 0)
NAMED_CHARACTER_REFERENCE(
    735,
    /* b i */
    'g' _ 't' _ 'r' _ 'i' _ 'a' _ 'n' _ 'g' _ 'l' _ 'e' _ 'u' _ 'p' _ ';', 12,
    0, 0x25b3 _ 0)
NAMED_CHARACTER_REFERENCE(736,
                          /* b i */ 'g' _ 'u' _ 'p' _ 'l' _ 'u' _ 's' _ ';', 7,
                          0, 0x2a04 _ 0)
NAMED_CHARACTER_REFERENCE(737,
                          /* b i */ 'g' _ 'v' _ 'e' _ 'e' _ ';', 5, 0,
                          0x22c1 _ 0)
NAMED_CHARACTER_REFERENCE(738,
                          /* b i */ 'g' _ 'w' _ 'e' _ 'd' _ 'g' _ 'e' _ ';', 7,
                          0, 0x22c0 _ 0)
NAMED_CHARACTER_REFERENCE(739,
                          /* b k */ 'a' _ 'r' _ 'o' _ 'w' _ ';', 5, 0,
                          0x290d _ 0)
NAMED_CHARACTER_REFERENCE(
    740,
    /* b l */ 'a' _ 'c' _ 'k' _ 'l' _ 'o' _ 'z' _ 'e' _ 'n' _ 'g' _ 'e' _ ';',
    11, 0, 0x29eb _ 0)
NAMED_CHARACTER_REFERENCE(
    741,
    /* b l */ 'a' _ 'c' _ 'k' _ 's' _ 'q' _ 'u' _ 'a' _ 'r' _ 'e' _ ';', 10, 0,
    0x25aa _ 0)
NAMED_CHARACTER_REFERENCE(
    742,
    /* b l */
    'a' _ 'c' _ 'k' _ 't' _ 'r' _ 'i' _ 'a' _ 'n' _ 'g' _ 'l' _ 'e' _ ';', 12,
    0, 0x25b4 _ 0)
NAMED_CHARACTER_REFERENCE(
    743,
    /* b l */
    'a' _ 'c' _ 'k' _ 't' _ 'r' _ 'i' _ 'a' _ 'n' _ 'g' _ 'l' _ 'e' _ 'd' _ 'o' _ 'w' _ 'n' _ ';',
    16, 0, 0x25be _ 0)
NAMED_CHARACTER_REFERENCE(
    744,
    /* b l */
    'a' _ 'c' _ 'k' _ 't' _ 'r' _ 'i' _ 'a' _ 'n' _ 'g' _ 'l' _ 'e' _ 'l' _ 'e' _ 'f' _ 't' _ ';',
    16, 0, 0x25c2 _ 0)
NAMED_CHARACTER_REFERENCE(
    745,
    /* b l */
    'a' _ 'c' _ 'k' _ 't' _ 'r' _ 'i' _ 'a' _ 'n' _ 'g' _ 'l' _ 'e' _ 'r' _ 'i' _ 'g' _ 'h' _ 't' _ ';',
    17, 0, 0x25b8 _ 0)
NAMED_CHARACTER_REFERENCE(746,
                          /* b l */ 'a' _ 'n' _ 'k' _ ';', 4, 0, 0x2423 _ 0)
NAMED_CHARACTER_REFERENCE(747,
                          /* b l */ 'k' _ '1' _ '2' _ ';', 4, 0, 0x2592 _ 0)
NAMED_CHARACTER_REFERENCE(748,
                          /* b l */ 'k' _ '1' _ '4' _ ';', 4, 0, 0x2591 _ 0)
NAMED_CHARACTER_REFERENCE(749,
                          /* b l */ 'k' _ '3' _ '4' _ ';', 4, 0, 0x2593 _ 0)
NAMED_CHARACTER_REFERENCE(750,
                          /* b l */ 'o' _ 'c' _ 'k' _ ';', 4, 0, 0x2588 _ 0)
NAMED_CHARACTER_REFERENCE(751, /* b n */ 'e' _ ';', 2, 0, 0x003d _ 0x20e5)
NAMED_CHARACTER_REFERENCE(752,
                          /* b n */ 'e' _ 'q' _ 'u' _ 'i' _ 'v' _ ';', 6, 0,
                          0x2261 _ 0x20e5)
NAMED_CHARACTER_REFERENCE(753, /* b n */ 'o' _ 't' _ ';', 3, 0, 0x2310 _ 0)
NAMED_CHARACTER_REFERENCE(754, /* b o */ 'p' _ 'f' _ ';', 3, 0, 0xd835 _ 0xdd53)
NAMED_CHARACTER_REFERENCE(755, /* b o */ 't' _ ';', 2, 0, 0x22a5 _ 0)
NAMED_CHARACTER_REFERENCE(756,
                          /* b o */ 't' _ 't' _ 'o' _ 'm' _ ';', 5, 0,
                          0x22a5 _ 0)
NAMED_CHARACTER_REFERENCE(757,
                          /* b o */ 'w' _ 't' _ 'i' _ 'e' _ ';', 5, 0,
                          0x22c8 _ 0)
NAMED_CHARACTER_REFERENCE(758,
                          /* b o */ 'x' _ 'D' _ 'L' _ ';', 4, 0, 0x2557 _ 0)
NAMED_CHARACTER_REFERENCE(759,
                          /* b o */ 'x' _ 'D' _ 'R' _ ';', 4, 0, 0x2554 _ 0)
NAMED_CHARACTER_REFERENCE(760,
                          /* b o */ 'x' _ 'D' _ 'l' _ ';', 4, 0, 0x2556 _ 0)
NAMED_CHARACTER_REFERENCE(761,
                          /* b o */ 'x' _ 'D' _ 'r' _ ';', 4, 0, 0x2553 _ 0)
NAMED_CHARACTER_REFERENCE(762, /* b o */ 'x' _ 'H' _ ';', 3, 0, 0x2550 _ 0)
NAMED_CHARACTER_REFERENCE(763,
                          /* b o */ 'x' _ 'H' _ 'D' _ ';', 4, 0, 0x2566 _ 0)
NAMED_CHARACTER_REFERENCE(764,
                          /* b o */ 'x' _ 'H' _ 'U' _ ';', 4, 0, 0x2569 _ 0)
NAMED_CHARACTER_REFERENCE(765,
                          /* b o */ 'x' _ 'H' _ 'd' _ ';', 4, 0, 0x2564 _ 0)
NAMED_CHARACTER_REFERENCE(766,
                          /* b o */ 'x' _ 'H' _ 'u' _ ';', 4, 0, 0x2567 _ 0)
NAMED_CHARACTER_REFERENCE(767,
                          /* b o */ 'x' _ 'U' _ 'L' _ ';', 4, 0, 0x255d _ 0)
NAMED_CHARACTER_REFERENCE(768,
                          /* b o */ 'x' _ 'U' _ 'R' _ ';', 4, 0, 0x255a _ 0)
NAMED_CHARACTER_REFERENCE(769,
                          /* b o */ 'x' _ 'U' _ 'l' _ ';', 4, 0, 0x255c _ 0)
NAMED_CHARACTER_REFERENCE(770,
                          /* b o */ 'x' _ 'U' _ 'r' _ ';', 4, 0, 0x2559 _ 0)
NAMED_CHARACTER_REFERENCE(771, /* b o */ 'x' _ 'V' _ ';', 3, 0, 0x2551 _ 0)
NAMED_CHARACTER_REFERENCE(772,
                          /* b o */ 'x' _ 'V' _ 'H' _ ';', 4, 0, 0x256c _ 0)
NAMED_CHARACTER_REFERENCE(773,
                          /* b o */ 'x' _ 'V' _ 'L' _ ';', 4, 0, 0x2563 _ 0)
NAMED_CHARACTER_REFERENCE(774,
                          /* b o */ 'x' _ 'V' _ 'R' _ ';', 4, 0, 0x2560 _ 0)
NAMED_CHARACTER_REFERENCE(775,
                          /* b o */ 'x' _ 'V' _ 'h' _ ';', 4, 0, 0x256b _ 0)
NAMED_CHARACTER_REFERENCE(776,
                          /* b o */ 'x' _ 'V' _ 'l' _ ';', 4, 0, 0x2562 _ 0)
NAMED_CHARACTER_REFERENCE(777,
                          /* b o */ 'x' _ 'V' _ 'r' _ ';', 4, 0, 0x255f _ 0)
NAMED_CHARACTER_REFERENCE(778,
                          /* b o */ 'x' _ 'b' _ 'o' _ 'x' _ ';', 5, 0,
                          0x29c9 _ 0)
NAMED_CHARACTER_REFERENCE(779,
                          /* b o */ 'x' _ 'd' _ 'L' _ ';', 4, 0, 0x2555 _ 0)
NAMED_CHARACTER_REFERENCE(780,
                          /* b o */ 'x' _ 'd' _ 'R' _ ';', 4, 0, 0x2552 _ 0)
NAMED_CHARACTER_REFERENCE(781,
                          /* b o */ 'x' _ 'd' _ 'l' _ ';', 4, 0, 0x2510 _ 0)
NAMED_CHARACTER_REFERENCE(782,
                          /* b o */ 'x' _ 'd' _ 'r' _ ';', 4, 0, 0x250c _ 0)
NAMED_CHARACTER_REFERENCE(783, /* b o */ 'x' _ 'h' _ ';', 3, 0, 0x2500 _ 0)
NAMED_CHARACTER_REFERENCE(784,
                          /* b o */ 'x' _ 'h' _ 'D' _ ';', 4, 0, 0x2565 _ 0)
NAMED_CHARACTER_REFERENCE(785,
                          /* b o */ 'x' _ 'h' _ 'U' _ ';', 4, 0, 0x2568 _ 0)
NAMED_CHARACTER_REFERENCE(786,
                          /* b o */ 'x' _ 'h' _ 'd' _ ';', 4, 0, 0x252c _ 0)
NAMED_CHARACTER_REFERENCE(787,
                          /* b o */ 'x' _ 'h' _ 'u' _ ';', 4, 0, 0x2534 _ 0)
NAMED_CHARACTER_REFERENCE(788,
                          /* b o */ 'x' _ 'm' _ 'i' _ 'n' _ 'u' _ 's' _ ';', 7,
                          0, 0x229f _ 0)
NAMED_CHARACTER_REFERENCE(789,
                          /* b o */ 'x' _ 'p' _ 'l' _ 'u' _ 's' _ ';', 6, 0,
                          0x229e _ 0)
NAMED_CHARACTER_REFERENCE(790,
                          /* b o */ 'x' _ 't' _ 'i' _ 'm' _ 'e' _ 's' _ ';', 7,
                          0, 0x22a0 _ 0)
NAMED_CHARACTER_REFERENCE(791,
                          /* b o */ 'x' _ 'u' _ 'L' _ ';', 4, 0, 0x255b _ 0)
NAMED_CHARACTER_REFERENCE(792,
                          /* b o */ 'x' _ 'u' _ 'R' _ ';', 4, 0, 0x2558 _ 0)
NAMED_CHARACTER_REFERENCE(793,
                          /* b o */ 'x' _ 'u' _ 'l' _ ';', 4, 0, 0x2518 _ 0)
NAMED_CHARACTER_REFERENCE(794,
                          /* b o */ 'x' _ 'u' _ 'r' _ ';', 4, 0, 0x2514 _ 0)
NAMED_CHARACTER_REFERENCE(795, /* b o */ 'x' _ 'v' _ ';', 3, 0, 0x2502 _ 0)
NAMED_CHARACTER_REFERENCE(796,
                          /* b o */ 'x' _ 'v' _ 'H' _ ';', 4, 0, 0x256a _ 0)
NAMED_CHARACTER_REFERENCE(797,
                          /* b o */ 'x' _ 'v' _ 'L' _ ';', 4, 0, 0x2561 _ 0)
NAMED_CHARACTER_REFERENCE(798,
                          /* b o */ 'x' _ 'v' _ 'R' _ ';', 4, 0, 0x255e _ 0)
NAMED_CHARACTER_REFERENCE(799,
                          /* b o */ 'x' _ 'v' _ 'h' _ ';', 4, 0, 0x253c _ 0)
NAMED_CHARACTER_REFERENCE(800,
                          /* b o */ 'x' _ 'v' _ 'l' _ ';', 4, 0, 0x2524 _ 0)
NAMED_CHARACTER_REFERENCE(801,
                          /* b o */ 'x' _ 'v' _ 'r' _ ';', 4, 0, 0x251c _ 0)
NAMED_CHARACTER_REFERENCE(802,
                          /* b p */ 'r' _ 'i' _ 'm' _ 'e' _ ';', 5, 0,
                          0x2035 _ 0)
NAMED_CHARACTER_REFERENCE(803,
                          /* b r */ 'e' _ 'v' _ 'e' _ ';', 4, 0, 0x02d8 _ 0)
NAMED_CHARACTER_REFERENCE(804,
                          /* b r */ 'v' _ 'b' _ 'a' _ 'r', 4, 0, 0x00a6 _ 0)
NAMED_CHARACTER_REFERENCE(805,
                          /* b r */ 'v' _ 'b' _ 'a' _ 'r' _ ';', 5, 0,
                          0x00a6 _ 0)
NAMED_CHARACTER_REFERENCE(806, /* b s */ 'c' _ 'r' _ ';', 3, 0, 0xd835 _ 0xdcb7)
NAMED_CHARACTER_REFERENCE(807,
                          /* b s */ 'e' _ 'm' _ 'i' _ ';', 4, 0, 0x204f _ 0)
NAMED_CHARACTER_REFERENCE(808, /* b s */ 'i' _ 'm' _ ';', 3, 0, 0x223d _ 0)
NAMED_CHARACTER_REFERENCE(809,
                          /* b s */ 'i' _ 'm' _ 'e' _ ';', 4, 0, 0x22cd _ 0)
NAMED_CHARACTER_REFERENCE(810, /* b s */ 'o' _ 'l' _ ';', 3, 0, 0x005c _ 0)
NAMED_CHARACTER_REFERENCE(811,
                          /* b s */ 'o' _ 'l' _ 'b' _ ';', 4, 0, 0x29c5 _ 0)
NAMED_CHARACTER_REFERENCE(812,
                          /* b s */ 'o' _ 'l' _ 'h' _ 's' _ 'u' _ 'b' _ ';', 7,
                          0, 0x27c8 _ 0)
NAMED_CHARACTER_REFERENCE(813, /* b u */ 'l' _ 'l' _ ';', 3, 0, 0x2022 _ 0)
NAMED_CHARACTER_REFERENCE(814,
                          /* b u */ 'l' _ 'l' _ 'e' _ 't' _ ';', 5, 0,
                          0x2022 _ 0)
NAMED_CHARACTER_REFERENCE(815, /* b u */ 'm' _ 'p' _ ';', 3, 0, 0x224e _ 0)
NAMED_CHARACTER_REFERENCE(816,
                          /* b u */ 'm' _ 'p' _ 'E' _ ';', 4, 0, 0x2aae _ 0)
NAMED_CHARACTER_REFERENCE(817,
                          /* b u */ 'm' _ 'p' _ 'e' _ ';', 4, 0, 0x224f _ 0)
NAMED_CHARACTER_REFERENCE(818,
                          /* b u */ 'm' _ 'p' _ 'e' _ 'q' _ ';', 5, 0,
                          0x224f _ 0)
NAMED_CHARACTER_REFERENCE(819,
                          /* c a */ 'c' _ 'u' _ 't' _ 'e' _ ';', 5, 0,
                          0x0107 _ 0)
NAMED_CHARACTER_REFERENCE(820, /* c a */ 'p' _ ';', 2, 0, 0x2229 _ 0)
NAMED_CHARACTER_REFERENCE(821,
                          /* c a */ 'p' _ 'a' _ 'n' _ 'd' _ ';', 5, 0,
                          0x2a44 _ 0)
NAMED_CHARACTER_REFERENCE(822,
                          /* c a */ 'p' _ 'b' _ 'r' _ 'c' _ 'u' _ 'p' _ ';', 7,
                          0, 0x2a49 _ 0)
NAMED_CHARACTER_REFERENCE(823,
                          /* c a */ 'p' _ 'c' _ 'a' _ 'p' _ ';', 5, 0,
                          0x2a4b _ 0)
NAMED_CHARACTER_REFERENCE(824,
                          /* c a */ 'p' _ 'c' _ 'u' _ 'p' _ ';', 5, 0,
                          0x2a47 _ 0)
NAMED_CHARACTER_REFERENCE(825,
                          /* c a */ 'p' _ 'd' _ 'o' _ 't' _ ';', 5, 0,
                          0x2a40 _ 0)
NAMED_CHARACTER_REFERENCE(826, /* c a */ 'p' _ 's' _ ';', 3, 0, 0x2229 _ 0xfe00)
NAMED_CHARACTER_REFERENCE(827,
                          /* c a */ 'r' _ 'e' _ 't' _ ';', 4, 0, 0x2041 _ 0)
NAMED_CHARACTER_REFERENCE(828,
                          /* c a */ 'r' _ 'o' _ 'n' _ ';', 4, 0, 0x02c7 _ 0)
NAMED_CHARACTER_REFERENCE(829,
                          /* c c */ 'a' _ 'p' _ 's' _ ';', 4, 0, 0x2a4d _ 0)
NAMED_CHARACTER_REFERENCE(830,
                          /* c c */ 'a' _ 'r' _ 'o' _ 'n' _ ';', 5, 0,
                          0x010d _ 0)
NAMED_CHARACTER_REFERENCE(831,
                          /* c c */ 'e' _ 'd' _ 'i' _ 'l', 4, 0, 0x00e7 _ 0)
NAMED_CHARACTER_REFERENCE(832,
                          /* c c */ 'e' _ 'd' _ 'i' _ 'l' _ ';', 5, 0,
                          0x00e7 _ 0)
NAMED_CHARACTER_REFERENCE(833,
                          /* c c */ 'i' _ 'r' _ 'c' _ ';', 4, 0, 0x0109 _ 0)
NAMED_CHARACTER_REFERENCE(834,
                          /* c c */ 'u' _ 'p' _ 's' _ ';', 4, 0, 0x2a4c _ 0)
NAMED_CHARACTER_REFERENCE(835,
                          /* c c */ 'u' _ 'p' _ 's' _ 's' _ 'm' _ ';', 6, 0,
                          0x2a50 _ 0)
NAMED_CHARACTER_REFERENCE(836, /* c d */ 'o' _ 't' _ ';', 3, 0, 0x010b _ 0)
NAMED_CHARACTER_REFERENCE(837, /* c e */ 'd' _ 'i' _ 'l', 3, 0, 0x00b8 _ 0)
NAMED_CHARACTER_REFERENCE(838,
                          /* c e */ 'd' _ 'i' _ 'l' _ ';', 4, 0, 0x00b8 _ 0)
NAMED_CHARACTER_REFERENCE(839,
                          /* c e */ 'm' _ 'p' _ 't' _ 'y' _ 'v' _ ';', 6, 0,
                          0x29b2 _ 0)
NAMED_CHARACTER_REFERENCE(840, /* c e */ 'n' _ 't', 2, 0, 0x00a2 _ 0)
NAMED_CHARACTER_REFERENCE(841, /* c e */ 'n' _ 't' _ ';', 3, 0, 0x00a2 _ 0)
NAMED_CHARACTER_REFERENCE(
    842,
    /* c e */ 'n' _ 't' _ 'e' _ 'r' _ 'd' _ 'o' _ 't' _ ';', 8, 0, 0x00b7 _ 0)
NAMED_CHARACTER_REFERENCE(843, /* c f */ 'r' _ ';', 2, 0, 0xd835 _ 0xdd20)
NAMED_CHARACTER_REFERENCE(844, /* c h */ 'c' _ 'y' _ ';', 3, 0, 0x0447 _ 0)
NAMED_CHARACTER_REFERENCE(845,
                          /* c h */ 'e' _ 'c' _ 'k' _ ';', 4, 0, 0x2713 _ 0)
NAMED_CHARACTER_REFERENCE(
    846,
    /* c h */ 'e' _ 'c' _ 'k' _ 'm' _ 'a' _ 'r' _ 'k' _ ';', 8, 0, 0x2713 _ 0)
NAMED_CHARACTER_REFERENCE(847, /* c h */ 'i' _ ';', 2, 0, 0x03c7 _ 0)
NAMED_CHARACTER_REFERENCE(848, /* c i */ 'r' _ ';', 2, 0, 0x25cb _ 0)
NAMED_CHARACTER_REFERENCE(849, /* c i */ 'r' _ 'E' _ ';', 3, 0, 0x29c3 _ 0)
NAMED_CHARACTER_REFERENCE(850, /* c i */ 'r' _ 'c' _ ';', 3, 0, 0x02c6 _ 0)
NAMED_CHARACTER_REFERENCE(851,
                          /* c i */ 'r' _ 'c' _ 'e' _ 'q' _ ';', 5, 0,
                          0x2257 _ 0)
NAMED_CHARACTER_REFERENCE(
    852,
    /* c i */
    'r' _ 'c' _ 'l' _ 'e' _ 'a' _ 'r' _ 'r' _ 'o' _ 'w' _ 'l' _ 'e' _ 'f' _ 't' _ ';',
    14, 0, 0x21ba _ 0)
NAMED_CHARACTER_REFERENCE(
    853,
    /* c i */
    'r' _ 'c' _ 'l' _ 'e' _ 'a' _ 'r' _ 'r' _ 'o' _ 'w' _ 'r' _ 'i' _ 'g' _ 'h' _ 't' _ ';',
    15, 0, 0x21bb _ 0)
NAMED_CHARACTER_REFERENCE(854,
                          /* c i */ 'r' _ 'c' _ 'l' _ 'e' _ 'd' _ 'R' _ ';', 7,
                          0, 0x00ae _ 0)
NAMED_CHARACTER_REFERENCE(855,
                          /* c i */ 'r' _ 'c' _ 'l' _ 'e' _ 'd' _ 'S' _ ';', 7,
                          0, 0x24c8 _ 0)
NAMED_CHARACTER_REFERENCE(
    856,
    /* c i */ 'r' _ 'c' _ 'l' _ 'e' _ 'd' _ 'a' _ 's' _ 't' _ ';', 9, 0,
    0x229b _ 0)
NAMED_CHARACTER_REFERENCE(
    857,
    /* c i */ 'r' _ 'c' _ 'l' _ 'e' _ 'd' _ 'c' _ 'i' _ 'r' _ 'c' _ ';', 10, 0,
    0x229a _ 0)
NAMED_CHARACTER_REFERENCE(
    858,
    /* c i */ 'r' _ 'c' _ 'l' _ 'e' _ 'd' _ 'd' _ 'a' _ 's' _ 'h' _ ';', 10, 0,
    0x229d _ 0)
NAMED_CHARACTER_REFERENCE(859, /* c i */ 'r' _ 'e' _ ';', 3, 0, 0x2257 _ 0)
NAMED_CHARACTER_REFERENCE(860,
                          /* c i */ 'r' _ 'f' _ 'n' _ 'i' _ 'n' _ 't' _ ';', 7,
                          0, 0x2a10 _ 0)
NAMED_CHARACTER_REFERENCE(861,
                          /* c i */ 'r' _ 'm' _ 'i' _ 'd' _ ';', 5, 0,
                          0x2aef _ 0)
NAMED_CHARACTER_REFERENCE(862,
                          /* c i */ 'r' _ 's' _ 'c' _ 'i' _ 'r' _ ';', 6, 0,
                          0x29c2 _ 0)
NAMED_CHARACTER_REFERENCE(863,
                          /* c l */ 'u' _ 'b' _ 's' _ ';', 4, 0, 0x2663 _ 0)
NAMED_CHARACTER_REFERENCE(864,
                          /* c l */ 'u' _ 'b' _ 's' _ 'u' _ 'i' _ 't' _ ';', 7,
                          0, 0x2663 _ 0)
NAMED_CHARACTER_REFERENCE(865,
                          /* c o */ 'l' _ 'o' _ 'n' _ ';', 4, 0, 0x003a _ 0)
NAMED_CHARACTER_REFERENCE(866,
                          /* c o */ 'l' _ 'o' _ 'n' _ 'e' _ ';', 5, 0,
                          0x2254 _ 0)
NAMED_CHARACTER_REFERENCE(867,
                          /* c o */ 'l' _ 'o' _ 'n' _ 'e' _ 'q' _ ';', 6, 0,
                          0x2254 _ 0)
NAMED_CHARACTER_REFERENCE(868,
                          /* c o */ 'm' _ 'm' _ 'a' _ ';', 4, 0, 0x002c _ 0)
NAMED_CHARACTER_REFERENCE(869,
                          /* c o */ 'm' _ 'm' _ 'a' _ 't' _ ';', 5, 0,
                          0x0040 _ 0)
NAMED_CHARACTER_REFERENCE(870, /* c o */ 'm' _ 'p' _ ';', 3, 0, 0x2201 _ 0)
NAMED_CHARACTER_REFERENCE(871,
                          /* c o */ 'm' _ 'p' _ 'f' _ 'n' _ ';', 5, 0,
                          0x2218 _ 0)
NAMED_CHARACTER_REFERENCE(
    872,
    /* c o */ 'm' _ 'p' _ 'l' _ 'e' _ 'm' _ 'e' _ 'n' _ 't' _ ';', 9, 0,
    0x2201 _ 0)
NAMED_CHARACTER_REFERENCE(
    873,
    /* c o */ 'm' _ 'p' _ 'l' _ 'e' _ 'x' _ 'e' _ 's' _ ';', 8, 0, 0x2102 _ 0)
NAMED_CHARACTER_REFERENCE(874, /* c o */ 'n' _ 'g' _ ';', 3, 0, 0x2245 _ 0)
NAMED_CHARACTER_REFERENCE(875,
                          /* c o */ 'n' _ 'g' _ 'd' _ 'o' _ 't' _ ';', 6, 0,
                          0x2a6d _ 0)
NAMED_CHARACTER_REFERENCE(876,
                          /* c o */ 'n' _ 'i' _ 'n' _ 't' _ ';', 5, 0,
                          0x222e _ 0)
NAMED_CHARACTER_REFERENCE(877, /* c o */ 'p' _ 'f' _ ';', 3, 0, 0xd835 _ 0xdd54)
NAMED_CHARACTER_REFERENCE(878,
                          /* c o */ 'p' _ 'r' _ 'o' _ 'd' _ ';', 5, 0,
                          0x2210 _ 0)
NAMED_CHARACTER_REFERENCE(879, /* c o */ 'p' _ 'y', 2, 0, 0x00a9 _ 0)
NAMED_CHARACTER_REFERENCE(880, /* c o */ 'p' _ 'y' _ ';', 3, 0, 0x00a9 _ 0)
NAMED_CHARACTER_REFERENCE(881,
                          /* c o */ 'p' _ 'y' _ 's' _ 'r' _ ';', 5, 0,
                          0x2117 _ 0)
NAMED_CHARACTER_REFERENCE(882,
                          /* c r */ 'a' _ 'r' _ 'r' _ ';', 4, 0, 0x21b5 _ 0)
NAMED_CHARACTER_REFERENCE(883,
                          /* c r */ 'o' _ 's' _ 's' _ ';', 4, 0, 0x2717 _ 0)
NAMED_CHARACTER_REFERENCE(884, /* c s */ 'c' _ 'r' _ ';', 3, 0, 0xd835 _ 0xdcb8)
NAMED_CHARACTER_REFERENCE(885, /* c s */ 'u' _ 'b' _ ';', 3, 0, 0x2acf _ 0)
NAMED_CHARACTER_REFERENCE(886,
                          /* c s */ 'u' _ 'b' _ 'e' _ ';', 4, 0, 0x2ad1 _ 0)
NAMED_CHARACTER_REFERENCE(887, /* c s */ 'u' _ 'p' _ ';', 3, 0, 0x2ad0 _ 0)
NAMED_CHARACTER_REFERENCE(888,
                          /* c s */ 'u' _ 'p' _ 'e' _ ';', 4, 0, 0x2ad2 _ 0)
NAMED_CHARACTER_REFERENCE(889,
                          /* c t */ 'd' _ 'o' _ 't' _ ';', 4, 0, 0x22ef _ 0)
NAMED_CHARACTER_REFERENCE(890,
                          /* c u */ 'd' _ 'a' _ 'r' _ 'r' _ 'l' _ ';', 6, 0,
                          0x2938 _ 0)
NAMED_CHARACTER_REFERENCE(891,
                          /* c u */ 'd' _ 'a' _ 'r' _ 'r' _ 'r' _ ';', 6, 0,
                          0x2935 _ 0)
NAMED_CHARACTER_REFERENCE(892,
                          /* c u */ 'e' _ 'p' _ 'r' _ ';', 4, 0, 0x22de _ 0)
NAMED_CHARACTER_REFERENCE(893,
                          /* c u */ 'e' _ 's' _ 'c' _ ';', 4, 0, 0x22df _ 0)
NAMED_CHARACTER_REFERENCE(894,
                          /* c u */ 'l' _ 'a' _ 'r' _ 'r' _ ';', 5, 0,
                          0x21b6 _ 0)
NAMED_CHARACTER_REFERENCE(895,
                          /* c u */ 'l' _ 'a' _ 'r' _ 'r' _ 'p' _ ';', 6, 0,
                          0x293d _ 0)
NAMED_CHARACTER_REFERENCE(896, /* c u */ 'p' _ ';', 2, 0, 0x222a _ 0)
NAMED_CHARACTER_REFERENCE(897,
                          /* c u */ 'p' _ 'b' _ 'r' _ 'c' _ 'a' _ 'p' _ ';', 7,
                          0, 0x2a48 _ 0)
NAMED_CHARACTER_REFERENCE(898,
                          /* c u */ 'p' _ 'c' _ 'a' _ 'p' _ ';', 5, 0,
                          0x2a46 _ 0)
NAMED_CHARACTER_REFERENCE(899,
                          /* c u */ 'p' _ 'c' _ 'u' _ 'p' _ ';', 5, 0,
                          0x2a4a _ 0)
NAMED_CHARACTER_REFERENCE(900,
                          /* c u */ 'p' _ 'd' _ 'o' _ 't' _ ';', 5, 0,
                          0x228d _ 0)
NAMED_CHARACTER_REFERENCE(901,
                          /* c u */ 'p' _ 'o' _ 'r' _ ';', 4, 0, 0x2a45 _ 0)
NAMED_CHARACTER_REFERENCE(902, /* c u */ 'p' _ 's' _ ';', 3, 0, 0x222a _ 0xfe00)
NAMED_CHARACTER_REFERENCE(903,
                          /* c u */ 'r' _ 'a' _ 'r' _ 'r' _ ';', 5, 0,
                          0x21b7 _ 0)
NAMED_CHARACTER_REFERENCE(904,
                          /* c u */ 'r' _ 'a' _ 'r' _ 'r' _ 'm' _ ';', 6, 0,
                          0x293c _ 0)
NAMED_CHARACTER_REFERENCE(
    905,
    /* c u */ 'r' _ 'l' _ 'y' _ 'e' _ 'q' _ 'p' _ 'r' _ 'e' _ 'c' _ ';', 10, 0,
    0x22de _ 0)
NAMED_CHARACTER_REFERENCE(
    906,
    /* c u */ 'r' _ 'l' _ 'y' _ 'e' _ 'q' _ 's' _ 'u' _ 'c' _ 'c' _ ';', 10, 0,
    0x22df _ 0)
NAMED_CHARACTER_REFERENCE(907,
                          /* c u */ 'r' _ 'l' _ 'y' _ 'v' _ 'e' _ 'e' _ ';', 7,
                          0, 0x22ce _ 0)
NAMED_CHARACTER_REFERENCE(
    908,
    /* c u */ 'r' _ 'l' _ 'y' _ 'w' _ 'e' _ 'd' _ 'g' _ 'e' _ ';', 9, 0,
    0x22cf _ 0)
NAMED_CHARACTER_REFERENCE(909,
                          /* c u */ 'r' _ 'r' _ 'e' _ 'n', 4, 0, 0x00a4 _ 0)
NAMED_CHARACTER_REFERENCE(910,
                          /* c u */ 'r' _ 'r' _ 'e' _ 'n' _ ';', 5, 0,
                          0x00a4 _ 0)
NAMED_CHARACTER_REFERENCE(
    911,
    /* c u */
    'r' _ 'v' _ 'e' _ 'a' _ 'r' _ 'r' _ 'o' _ 'w' _ 'l' _ 'e' _ 'f' _ 't' _ ';',
    13, 0, 0x21b6 _ 0)
NAMED_CHARACTER_REFERENCE(
    912,
    /* c u */
    'r' _ 'v' _ 'e' _ 'a' _ 'r' _ 'r' _ 'o' _ 'w' _ 'r' _ 'i' _ 'g' _ 'h' _ 't' _ ';',
    14, 0, 0x21b7 _ 0)
NAMED_CHARACTER_REFERENCE(913,
                          /* c u */ 'v' _ 'e' _ 'e' _ ';', 4, 0, 0x22ce _ 0)
NAMED_CHARACTER_REFERENCE(914,
                          /* c u */ 'w' _ 'e' _ 'd' _ ';', 4, 0, 0x22cf _ 0)
NAMED_CHARACTER_REFERENCE(915,
                          /* c w */ 'c' _ 'o' _ 'n' _ 'i' _ 'n' _ 't' _ ';', 7,
                          0, 0x2232 _ 0)
NAMED_CHARACTER_REFERENCE(916,
                          /* c w */ 'i' _ 'n' _ 't' _ ';', 4, 0, 0x2231 _ 0)
NAMED_CHARACTER_REFERENCE(917,
                          /* c y */ 'l' _ 'c' _ 't' _ 'y' _ ';', 5, 0,
                          0x232d _ 0)
NAMED_CHARACTER_REFERENCE(918, /* d A */ 'r' _ 'r' _ ';', 3, 0, 0x21d3 _ 0)
NAMED_CHARACTER_REFERENCE(919, /* d H */ 'a' _ 'r' _ ';', 3, 0, 0x2965 _ 0)
NAMED_CHARACTER_REFERENCE(920,
                          /* d a */ 'g' _ 'g' _ 'e' _ 'r' _ ';', 5, 0,
                          0x2020 _ 0)
NAMED_CHARACTER_REFERENCE(921,
                          /* d a */ 'l' _ 'e' _ 't' _ 'h' _ ';', 5, 0,
                          0x2138 _ 0)
NAMED_CHARACTER_REFERENCE(922, /* d a */ 'r' _ 'r' _ ';', 3, 0, 0x2193 _ 0)
NAMED_CHARACTER_REFERENCE(923, /* d a */ 's' _ 'h' _ ';', 3, 0, 0x2010 _ 0)
NAMED_CHARACTER_REFERENCE(924,
                          /* d a */ 's' _ 'h' _ 'v' _ ';', 4, 0, 0x22a3 _ 0)
NAMED_CHARACTER_REFERENCE(925,
                          /* d b */ 'k' _ 'a' _ 'r' _ 'o' _ 'w' _ ';', 6, 0,
                          0x290f _ 0)
NAMED_CHARACTER_REFERENCE(926,
                          /* d b */ 'l' _ 'a' _ 'c' _ ';', 4, 0, 0x02dd _ 0)
NAMED_CHARACTER_REFERENCE(927,
                          /* d c */ 'a' _ 'r' _ 'o' _ 'n' _ ';', 5, 0,
                          0x010f _ 0)
NAMED_CHARACTER_REFERENCE(928, /* d c */ 'y' _ ';', 2, 0, 0x0434 _ 0)
NAMED_CHARACTER_REFERENCE(929, /* d d */ ';', 1, 0, 0x2146 _ 0)
NAMED_CHARACTER_REFERENCE(930,
                          /* d d */ 'a' _ 'g' _ 'g' _ 'e' _ 'r' _ ';', 6, 0,
                          0x2021 _ 0)
NAMED_CHARACTER_REFERENCE(931,
                          /* d d */ 'a' _ 'r' _ 'r' _ ';', 4, 0, 0x21ca _ 0)
NAMED_CHARACTER_REFERENCE(932,
                          /* d d */ 'o' _ 't' _ 's' _ 'e' _ 'q' _ ';', 6, 0,
                          0x2a77 _ 0)
NAMED_CHARACTER_REFERENCE(933, /* d e */ 'g', 1, 0, 0x00b0 _ 0)
NAMED_CHARACTER_REFERENCE(934, /* d e */ 'g' _ ';', 2, 0, 0x00b0 _ 0)
NAMED_CHARACTER_REFERENCE(935,
                          /* d e */ 'l' _ 't' _ 'a' _ ';', 4, 0, 0x03b4 _ 0)
NAMED_CHARACTER_REFERENCE(936,
                          /* d e */ 'm' _ 'p' _ 't' _ 'y' _ 'v' _ ';', 6, 0,
                          0x29b1 _ 0)
NAMED_CHARACTER_REFERENCE(937,
                          /* d f */ 'i' _ 's' _ 'h' _ 't' _ ';', 5, 0,
                          0x297f _ 0)
NAMED_CHARACTER_REFERENCE(938, /* d f */ 'r' _ ';', 2, 0, 0xd835 _ 0xdd21)
NAMED_CHARACTER_REFERENCE(939,
                          /* d h */ 'a' _ 'r' _ 'l' _ ';', 4, 0, 0x21c3 _ 0)
NAMED_CHARACTER_REFERENCE(940,
                          /* d h */ 'a' _ 'r' _ 'r' _ ';', 4, 0, 0x21c2 _ 0)
NAMED_CHARACTER_REFERENCE(941, /* d i */ 'a' _ 'm' _ ';', 3, 0, 0x22c4 _ 0)
NAMED_CHARACTER_REFERENCE(942,
                          /* d i */ 'a' _ 'm' _ 'o' _ 'n' _ 'd' _ ';', 6, 0,
                          0x22c4 _ 0)
NAMED_CHARACTER_REFERENCE(
    943,
    /* d i */ 'a' _ 'm' _ 'o' _ 'n' _ 'd' _ 's' _ 'u' _ 'i' _ 't' _ ';', 10, 0,
    0x2666 _ 0)
NAMED_CHARACTER_REFERENCE(944,
                          /* d i */ 'a' _ 'm' _ 's' _ ';', 4, 0, 0x2666 _ 0)
NAMED_CHARACTER_REFERENCE(945, /* d i */ 'e' _ ';', 2, 0, 0x00a8 _ 0)
NAMED_CHARACTER_REFERENCE(946,
                          /* d i */ 'g' _ 'a' _ 'm' _ 'm' _ 'a' _ ';', 6, 0,
                          0x03dd _ 0)
NAMED_CHARACTER_REFERENCE(947,
                          /* d i */ 's' _ 'i' _ 'n' _ ';', 4, 0, 0x22f2 _ 0)
NAMED_CHARACTER_REFERENCE(948, /* d i */ 'v' _ ';', 2, 0, 0x00f7 _ 0)
NAMED_CHARACTER_REFERENCE(949,
                          /* d i */ 'v' _ 'i' _ 'd' _ 'e', 4, 0, 0x00f7 _ 0)
NAMED_CHARACTER_REFERENCE(950,
                          /* d i */ 'v' _ 'i' _ 'd' _ 'e' _ ';', 5, 0,
                          0x00f7 _ 0)
NAMED_CHARACTER_REFERENCE(
    951,
    /* d i */
    'v' _ 'i' _ 'd' _ 'e' _ 'o' _ 'n' _ 't' _ 'i' _ 'm' _ 'e' _ 's' _ ';', 12,
    0, 0x22c7 _ 0)
NAMED_CHARACTER_REFERENCE(952,
                          /* d i */ 'v' _ 'o' _ 'n' _ 'x' _ ';', 5, 0,
                          0x22c7 _ 0)
NAMED_CHARACTER_REFERENCE(953, /* d j */ 'c' _ 'y' _ ';', 3, 0, 0x0452 _ 0)
NAMED_CHARACTER_REFERENCE(954,
                          /* d l */ 'c' _ 'o' _ 'r' _ 'n' _ ';', 5, 0,
                          0x231e _ 0)
NAMED_CHARACTER_REFERENCE(955,
                          /* d l */ 'c' _ 'r' _ 'o' _ 'p' _ ';', 5, 0,
                          0x230d _ 0)
NAMED_CHARACTER_REFERENCE(956,
                          /* d o */ 'l' _ 'l' _ 'a' _ 'r' _ ';', 5, 0,
                          0x0024 _ 0)
NAMED_CHARACTER_REFERENCE(957, /* d o */ 'p' _ 'f' _ ';', 3, 0, 0xd835 _ 0xdd55)
NAMED_CHARACTER_REFERENCE(958, /* d o */ 't' _ ';', 2, 0, 0x02d9 _ 0)
NAMED_CHARACTER_REFERENCE(959,
                          /* d o */ 't' _ 'e' _ 'q' _ ';', 4, 0, 0x2250 _ 0)
NAMED_CHARACTER_REFERENCE(960,
                          /* d o */ 't' _ 'e' _ 'q' _ 'd' _ 'o' _ 't' _ ';', 7,
                          0, 0x2251 _ 0)
NAMED_CHARACTER_REFERENCE(961,
                          /* d o */ 't' _ 'm' _ 'i' _ 'n' _ 'u' _ 's' _ ';', 7,
                          0, 0x2238 _ 0)
NAMED_CHARACTER_REFERENCE(962,
                          /* d o */ 't' _ 'p' _ 'l' _ 'u' _ 's' _ ';', 6, 0,
                          0x2214 _ 0)
NAMED_CHARACTER_REFERENCE(
    963,
    /* d o */ 't' _ 's' _ 'q' _ 'u' _ 'a' _ 'r' _ 'e' _ ';', 8, 0, 0x22a1 _ 0)
NAMED_CHARACTER_REFERENCE(
    964,
    /* d o */
    'u' _ 'b' _ 'l' _ 'e' _ 'b' _ 'a' _ 'r' _ 'w' _ 'e' _ 'd' _ 'g' _ 'e' _ ';',
    13, 0, 0x2306 _ 0)
NAMED_CHARACTER_REFERENCE(
    965,
    /* d o */ 'w' _ 'n' _ 'a' _ 'r' _ 'r' _ 'o' _ 'w' _ ';', 8, 0, 0x2193 _ 0)
NAMED_CHARACTER_REFERENCE(
    966,
    /* d o */
    'w' _ 'n' _ 'd' _ 'o' _ 'w' _ 'n' _ 'a' _ 'r' _ 'r' _ 'o' _ 'w' _ 's' _ ';',
    13, 0, 0x21ca _ 0)
NAMED_CHARACTER_REFERENCE(
    967,
    /* d o */
    'w' _ 'n' _ 'h' _ 'a' _ 'r' _ 'p' _ 'o' _ 'o' _ 'n' _ 'l' _ 'e' _ 'f' _ 't' _ ';',
    14, 0, 0x21c3 _ 0)
NAMED_CHARACTER_REFERENCE(
    968,
    /* d o */
    'w' _ 'n' _ 'h' _ 'a' _ 'r' _ 'p' _ 'o' _ 'o' _ 'n' _ 'r' _ 'i' _ 'g' _ 'h' _ 't' _ ';',
    15, 0, 0x21c2 _ 0)
NAMED_CHARACTER_REFERENCE(969,
                          /* d r */ 'b' _ 'k' _ 'a' _ 'r' _ 'o' _ 'w' _ ';', 7,
                          0, 0x2910 _ 0)
NAMED_CHARACTER_REFERENCE(970,
                          /* d r */ 'c' _ 'o' _ 'r' _ 'n' _ ';', 5, 0,
                          0x231f _ 0)
NAMED_CHARACTER_REFERENCE(971,
                          /* d r */ 'c' _ 'r' _ 'o' _ 'p' _ ';', 5, 0,
                          0x230c _ 0)
NAMED_CHARACTER_REFERENCE(972, /* d s */ 'c' _ 'r' _ ';', 3, 0, 0xd835 _ 0xdcb9)
NAMED_CHARACTER_REFERENCE(973, /* d s */ 'c' _ 'y' _ ';', 3, 0, 0x0455 _ 0)
NAMED_CHARACTER_REFERENCE(974, /* d s */ 'o' _ 'l' _ ';', 3, 0, 0x29f6 _ 0)
NAMED_CHARACTER_REFERENCE(975,
                          /* d s */ 't' _ 'r' _ 'o' _ 'k' _ ';', 5, 0,
                          0x0111 _ 0)
NAMED_CHARACTER_REFERENCE(976,
                          /* d t */ 'd' _ 'o' _ 't' _ ';', 4, 0, 0x22f1 _ 0)
NAMED_CHARACTER_REFERENCE(977, /* d t */ 'r' _ 'i' _ ';', 3, 0, 0x25bf _ 0)
NAMED_CHARACTER_REFERENCE(978,
                          /* d t */ 'r' _ 'i' _ 'f' _ ';', 4, 0, 0x25be _ 0)
NAMED_CHARACTER_REFERENCE(979,
                          /* d u */ 'a' _ 'r' _ 'r' _ ';', 4, 0, 0x21f5 _ 0)
NAMED_CHARACTER_REFERENCE(980,
                          /* d u */ 'h' _ 'a' _ 'r' _ ';', 4, 0, 0x296f _ 0)
NAMED_CHARACTER_REFERENCE(981,
                          /* d w */ 'a' _ 'n' _ 'g' _ 'l' _ 'e' _ ';', 6, 0,
                          0x29a6 _ 0)
NAMED_CHARACTER_REFERENCE(982, /* d z */ 'c' _ 'y' _ ';', 3, 0, 0x045f _ 0)
NAMED_CHARACTER_REFERENCE(983,
                          /* d z */ 'i' _ 'g' _ 'r' _ 'a' _ 'r' _ 'r' _ ';', 7,
                          0, 0x27ff _ 0)
NAMED_CHARACTER_REFERENCE(984,
                          /* e D */ 'D' _ 'o' _ 't' _ ';', 4, 0, 0x2a77 _ 0)
NAMED_CHARACTER_REFERENCE(985, /* e D */ 'o' _ 't' _ ';', 3, 0, 0x2251 _ 0)
NAMED_CHARACTER_REFERENCE(986,
                          /* e a */ 'c' _ 'u' _ 't' _ 'e', 4, 0, 0x00e9 _ 0)
NAMED_CHARACTER_REFERENCE(987,
                          /* e a */ 'c' _ 'u' _ 't' _ 'e' _ ';', 5, 0,
                          0x00e9 _ 0)
NAMED_CHARACTER_REFERENCE(988,
                          /* e a */ 's' _ 't' _ 'e' _ 'r' _ ';', 5, 0,
                          0x2a6e _ 0)
NAMED_CHARACTER_REFERENCE(989,
                          /* e c */ 'a' _ 'r' _ 'o' _ 'n' _ ';', 5, 0,
                          0x011b _ 0)
NAMED_CHARACTER_REFERENCE(990, /* e c */ 'i' _ 'r' _ ';', 3, 0, 0x2256 _ 0)
NAMED_CHARACTER_REFERENCE(991, /* e c */ 'i' _ 'r' _ 'c', 3, 0, 0x00ea _ 0)
NAMED_CHARACTER_REFERENCE(992,
                          /* e c */ 'i' _ 'r' _ 'c' _ ';', 4, 0, 0x00ea _ 0)
NAMED_CHARACTER_REFERENCE(993,
                          /* e c */ 'o' _ 'l' _ 'o' _ 'n' _ ';', 5, 0,
                          0x2255 _ 0)
NAMED_CHARACTER_REFERENCE(994, /* e c */ 'y' _ ';', 2, 0, 0x044d _ 0)
NAMED_CHARACTER_REFERENCE(995, /* e d */ 'o' _ 't' _ ';', 3, 0, 0x0117 _ 0)
NAMED_CHARACTER_REFERENCE(996, /* e e */ ';', 1, 0, 0x2147 _ 0)
NAMED_CHARACTER_REFERENCE(997,
                          /* e f */ 'D' _ 'o' _ 't' _ ';', 4, 0, 0x2252 _ 0)
NAMED_CHARACTER_REFERENCE(998, /* e f */ 'r' _ ';', 2, 0, 0xd835 _ 0xdd22)
NAMED_CHARACTER_REFERENCE(999, /* e g */ ';', 1, 0, 0x2a9a _ 0)
NAMED_CHARACTER_REFERENCE(1000,
                          /* e g */ 'r' _ 'a' _ 'v' _ 'e', 4, 0, 0x00e8 _ 0)
NAMED_CHARACTER_REFERENCE(1001,
                          /* e g */ 'r' _ 'a' _ 'v' _ 'e' _ ';', 5, 0,
                          0x00e8 _ 0)
NAMED_CHARACTER_REFERENCE(1002, /* e g */ 's' _ ';', 2, 0, 0x2a96 _ 0)
NAMED_CHARACTER_REFERENCE(1003,
                          /* e g */ 's' _ 'd' _ 'o' _ 't' _ ';', 5, 0,
                          0x2a98 _ 0)
NAMED_CHARACTER_REFERENCE(1004, /* e l */ ';', 1, 0, 0x2a99 _ 0)
NAMED_CHARACTER_REFERENCE(1005,
                          /* e l */ 'i' _ 'n' _ 't' _ 'e' _ 'r' _ 's' _ ';', 7,
                          0, 0x23e7 _ 0)
NAMED_CHARACTER_REFERENCE(1006, /* e l */ 'l' _ ';', 2, 0, 0x2113 _ 0)
NAMED_CHARACTER_REFERENCE(1007, /* e l */ 's' _ ';', 2, 0, 0x2a95 _ 0)
NAMED_CHARACTER_REFERENCE(1008,
                          /* e l */ 's' _ 'd' _ 'o' _ 't' _ ';', 5, 0,
                          0x2a97 _ 0)
NAMED_CHARACTER_REFERENCE(1009,
                          /* e m */ 'a' _ 'c' _ 'r' _ ';', 4, 0, 0x0113 _ 0)
NAMED_CHARACTER_REFERENCE(1010,
                          /* e m */ 'p' _ 't' _ 'y' _ ';', 4, 0, 0x2205 _ 0)
NAMED_CHARACTER_REFERENCE(1011,
                          /* e m */ 'p' _ 't' _ 'y' _ 's' _ 'e' _ 't' _ ';', 7,
                          0, 0x2205 _ 0)
NAMED_CHARACTER_REFERENCE(1012,
                          /* e m */ 'p' _ 't' _ 'y' _ 'v' _ ';', 5, 0,
                          0x2205 _ 0)
NAMED_CHARACTER_REFERENCE(1013,
                          /* e m */ 's' _ 'p' _ '1' _ '3' _ ';', 5, 0,
                          0x2004 _ 0)
NAMED_CHARACTER_REFERENCE(1014,
                          /* e m */ 's' _ 'p' _ '1' _ '4' _ ';', 5, 0,
                          0x2005 _ 0)
NAMED_CHARACTER_REFERENCE(1015, /* e m */ 's' _ 'p' _ ';', 3, 0, 0x2003 _ 0)
NAMED_CHARACTER_REFERENCE(1016, /* e n */ 'g' _ ';', 2, 0, 0x014b _ 0)
NAMED_CHARACTER_REFERENCE(1017, /* e n */ 's' _ 'p' _ ';', 3, 0, 0x2002 _ 0)
NAMED_CHARACTER_REFERENCE(1018,
                          /* e o */ 'g' _ 'o' _ 'n' _ ';', 4, 0, 0x0119 _ 0)
NAMED_CHARACTER_REFERENCE(1019,
                          /* e o */ 'p' _ 'f' _ ';', 3, 0, 0xd835 _ 0xdd56)
NAMED_CHARACTER_REFERENCE(1020, /* e p */ 'a' _ 'r' _ ';', 3, 0, 0x22d5 _ 0)
NAMED_CHARACTER_REFERENCE(1021,
                          /* e p */ 'a' _ 'r' _ 's' _ 'l' _ ';', 5, 0,
                          0x29e3 _ 0)
NAMED_CHARACTER_REFERENCE(1022,
                          /* e p */ 'l' _ 'u' _ 's' _ ';', 4, 0, 0x2a71 _ 0)
NAMED_CHARACTER_REFERENCE(1023, /* e p */ 's' _ 'i' _ ';', 3, 0, 0x03b5 _ 0)
NAMED_CHARACTER_REFERENCE(1024,
                          /* e p */ 's' _ 'i' _ 'l' _ 'o' _ 'n' _ ';', 6, 0,
                          0x03b5 _ 0)
NAMED_CHARACTER_REFERENCE(1025,
                          /* e p */ 's' _ 'i' _ 'v' _ ';', 4, 0, 0x03f5 _ 0)
NAMED_CHARACTER_REFERENCE(1026,
                          /* e q */ 'c' _ 'i' _ 'r' _ 'c' _ ';', 5, 0,
                          0x2256 _ 0)
NAMED_CHARACTER_REFERENCE(1027,
                          /* e q */ 'c' _ 'o' _ 'l' _ 'o' _ 'n' _ ';', 6, 0,
                          0x2255 _ 0)
NAMED_CHARACTER_REFERENCE(1028,
                          /* e q */ 's' _ 'i' _ 'm' _ ';', 4, 0, 0x2242 _ 0)
NAMED_CHARACTER_REFERENCE(
    1029,
    /* e q */ 's' _ 'l' _ 'a' _ 'n' _ 't' _ 'g' _ 't' _ 'r' _ ';', 9, 0,
    0x2a96 _ 0)
NAMED_CHARACTER_REFERENCE(
    1030,
    /* e q */ 's' _ 'l' _ 'a' _ 'n' _ 't' _ 'l' _ 'e' _ 's' _ 's' _ ';', 10, 0,
    0x2a95 _ 0)
NAMED_CHARACTER_REFERENCE(1031,
                          /* e q */ 'u' _ 'a' _ 'l' _ 's' _ ';', 5, 0,
                          0x003d _ 0)
NAMED_CHARACTER_REFERENCE(1032,
                          /* e q */ 'u' _ 'e' _ 's' _ 't' _ ';', 5, 0,
                          0x225f _ 0)
NAMED_CHARACTER_REFERENCE(1033,
                          /* e q */ 'u' _ 'i' _ 'v' _ ';', 4, 0, 0x2261 _ 0)
NAMED_CHARACTER_REFERENCE(1034,
                          /* e q */ 'u' _ 'i' _ 'v' _ 'D' _ 'D' _ ';', 6, 0,
                          0x2a78 _ 0)
NAMED_CHARACTER_REFERENCE(1035,
                          /* e q */ 'v' _ 'p' _ 'a' _ 'r' _ 's' _ 'l' _ ';', 7,
                          0, 0x29e5 _ 0)
NAMED_CHARACTER_REFERENCE(1036,
                          /* e r */ 'D' _ 'o' _ 't' _ ';', 4, 0, 0x2253 _ 0)
NAMED_CHARACTER_REFERENCE(1037,
                          /* e r */ 'a' _ 'r' _ 'r' _ ';', 4, 0, 0x2971 _ 0)
NAMED_CHARACTER_REFERENCE(1038, /* e s */ 'c' _ 'r' _ ';', 3, 0, 0x212f _ 0)
NAMED_CHARACTER_REFERENCE(1039,
                          /* e s */ 'd' _ 'o' _ 't' _ ';', 4, 0, 0x2250 _ 0)
NAMED_CHARACTER_REFERENCE(1040, /* e s */ 'i' _ 'm' _ ';', 3, 0, 0x2242 _ 0)
NAMED_CHARACTER_REFERENCE(1041, /* e t */ 'a' _ ';', 2, 0, 0x03b7 _ 0)
NAMED_CHARACTER_REFERENCE(1042, /* e t */ 'h', 1, 0, 0x00f0 _ 0)
NAMED_CHARACTER_REFERENCE(1043, /* e t */ 'h' _ ';', 2, 0, 0x00f0 _ 0)
NAMED_CHARACTER_REFERENCE(1044, /* e u */ 'm' _ 'l', 2, 0, 0x00eb _ 0)
NAMED_CHARACTER_REFERENCE(1045, /* e u */ 'm' _ 'l' _ ';', 3, 0, 0x00eb _ 0)
NAMED_CHARACTER_REFERENCE(1046, /* e u */ 'r' _ 'o' _ ';', 3, 0, 0x20ac _ 0)
NAMED_CHARACTER_REFERENCE(1047, /* e x */ 'c' _ 'l' _ ';', 3, 0, 0x0021 _ 0)
NAMED_CHARACTER_REFERENCE(1048,
                          /* e x */ 'i' _ 's' _ 't' _ ';', 4, 0, 0x2203 _ 0)
NAMED_CHARACTER_REFERENCE(
    1049,
    /* e x */ 'p' _ 'e' _ 'c' _ 't' _ 'a' _ 't' _ 'i' _ 'o' _ 'n' _ ';', 10, 0,
    0x2130 _ 0)
NAMED_CHARACTER_REFERENCE(
    1050,
    /* e x */ 'p' _ 'o' _ 'n' _ 'e' _ 'n' _ 't' _ 'i' _ 'a' _ 'l' _ 'e' _ ';',
    11, 0, 0x2147 _ 0)
NAMED_CHARACTER_REFERENCE(
    1051,
    /* f a */
    'l' _ 'l' _ 'i' _ 'n' _ 'g' _ 'd' _ 'o' _ 't' _ 's' _ 'e' _ 'q' _ ';', 12,
    0, 0x2252 _ 0)
NAMED_CHARACTER_REFERENCE(1052, /* f c */ 'y' _ ';', 2, 0, 0x0444 _ 0)
NAMED_CHARACTER_REFERENCE(1053,
                          /* f e */ 'm' _ 'a' _ 'l' _ 'e' _ ';', 5, 0,
                          0x2640 _ 0)
NAMED_CHARACTER_REFERENCE(1054,
                          /* f f */ 'i' _ 'l' _ 'i' _ 'g' _ ';', 5, 0,
                          0xfb03 _ 0)
NAMED_CHARACTER_REFERENCE(1055,
                          /* f f */ 'l' _ 'i' _ 'g' _ ';', 4, 0, 0xfb00 _ 0)
NAMED_CHARACTER_REFERENCE(1056,
                          /* f f */ 'l' _ 'l' _ 'i' _ 'g' _ ';', 5, 0,
                          0xfb04 _ 0)
NAMED_CHARACTER_REFERENCE(1057, /* f f */ 'r' _ ';', 2, 0, 0xd835 _ 0xdd23)
NAMED_CHARACTER_REFERENCE(1058,
                          /* f i */ 'l' _ 'i' _ 'g' _ ';', 4, 0, 0xfb01 _ 0)
NAMED_CHARACTER_REFERENCE(1059,
                          /* f j */ 'l' _ 'i' _ 'g' _ ';', 4, 0,
                          0x0066 _ 0x006a)
NAMED_CHARACTER_REFERENCE(1060, /* f l */ 'a' _ 't' _ ';', 3, 0, 0x266d _ 0)
NAMED_CHARACTER_REFERENCE(1061,
                          /* f l */ 'l' _ 'i' _ 'g' _ ';', 4, 0, 0xfb02 _ 0)
NAMED_CHARACTER_REFERENCE(1062,
                          /* f l */ 't' _ 'n' _ 's' _ ';', 4, 0, 0x25b1 _ 0)
NAMED_CHARACTER_REFERENCE(1063, /* f n */ 'o' _ 'f' _ ';', 3, 0, 0x0192 _ 0)
NAMED_CHARACTER_REFERENCE(1064,
                          /* f o */ 'p' _ 'f' _ ';', 3, 0, 0xd835 _ 0xdd57)
NAMED_CHARACTER_REFERENCE(1065,
                          /* f o */ 'r' _ 'a' _ 'l' _ 'l' _ ';', 5, 0,
                          0x2200 _ 0)
NAMED_CHARACTER_REFERENCE(1066, /* f o */ 'r' _ 'k' _ ';', 3, 0, 0x22d4 _ 0)
NAMED_CHARACTER_REFERENCE(1067,
                          /* f o */ 'r' _ 'k' _ 'v' _ ';', 4, 0, 0x2ad9 _ 0)
NAMED_CHARACTER_REFERENCE(1068,
                          /* f p */ 'a' _ 'r' _ 't' _ 'i' _ 'n' _ 't' _ ';', 7,
                          0, 0x2a0d _ 0)
NAMED_CHARACTER_REFERENCE(1069,
                          /* f r */ 'a' _ 'c' _ '1' _ '2', 4, 0, 0x00bd _ 0)
NAMED_CHARACTER_REFERENCE(1070,
                          /* f r */ 'a' _ 'c' _ '1' _ '2' _ ';', 5, 0,
                          0x00bd _ 0)
NAMED_CHARACTER_REFERENCE(1071,
                          /* f r */ 'a' _ 'c' _ '1' _ '3' _ ';', 5, 0,
                          0x2153 _ 0)
NAMED_CHARACTER_REFERENCE(1072,
                          /* f r */ 'a' _ 'c' _ '1' _ '4', 4, 0, 0x00bc _ 0)
NAMED_CHARACTER_REFERENCE(1073,
                          /* f r */ 'a' _ 'c' _ '1' _ '4' _ ';', 5, 0,
                          0x00bc _ 0)
NAMED_CHARACTER_REFERENCE(1074,
                          /* f r */ 'a' _ 'c' _ '1' _ '5' _ ';', 5, 0,
                          0x2155 _ 0)
NAMED_CHARACTER_REFERENCE(1075,
                          /* f r */ 'a' _ 'c' _ '1' _ '6' _ ';', 5, 0,
                          0x2159 _ 0)
NAMED_CHARACTER_REFERENCE(1076,
                          /* f r */ 'a' _ 'c' _ '1' _ '8' _ ';', 5, 0,
                          0x215b _ 0)
NAMED_CHARACTER_REFERENCE(1077,
                          /* f r */ 'a' _ 'c' _ '2' _ '3' _ ';', 5, 0,
                          0x2154 _ 0)
NAMED_CHARACTER_REFERENCE(1078,
                          /* f r */ 'a' _ 'c' _ '2' _ '5' _ ';', 5, 0,
                          0x2156 _ 0)
NAMED_CHARACTER_REFERENCE(1079,
                          /* f r */ 'a' _ 'c' _ '3' _ '4', 4, 0, 0x00be _ 0)
NAMED_CHARACTER_REFERENCE(1080,
                          /* f r */ 'a' _ 'c' _ '3' _ '4' _ ';', 5, 0,
                          0x00be _ 0)
NAMED_CHARACTER_REFERENCE(1081,
                          /* f r */ 'a' _ 'c' _ '3' _ '5' _ ';', 5, 0,
                          0x2157 _ 0)
NAMED_CHARACTER_REFERENCE(1082,
                          /* f r */ 'a' _ 'c' _ '3' _ '8' _ ';', 5, 0,
                          0x215c _ 0)
NAMED_CHARACTER_REFERENCE(1083,
                          /* f r */ 'a' _ 'c' _ '4' _ '5' _ ';', 5, 0,
                          0x2158 _ 0)
NAMED_CHARACTER_REFERENCE(1084,
                          /* f r */ 'a' _ 'c' _ '5' _ '6' _ ';', 5, 0,
                          0x215a _ 0)
NAMED_CHARACTER_REFERENCE(1085,
                          /* f r */ 'a' _ 'c' _ '5' _ '8' _ ';', 5, 0,
                          0x215d _ 0)
NAMED_CHARACTER_REFERENCE(1086,
                          /* f r */ 'a' _ 'c' _ '7' _ '8' _ ';', 5, 0,
                          0x215e _ 0)
NAMED_CHARACTER_REFERENCE(1087,
                          /* f r */ 'a' _ 's' _ 'l' _ ';', 4, 0, 0x2044 _ 0)
NAMED_CHARACTER_REFERENCE(1088,
                          /* f r */ 'o' _ 'w' _ 'n' _ ';', 4, 0, 0x2322 _ 0)
NAMED_CHARACTER_REFERENCE(1089,
                          /* f s */ 'c' _ 'r' _ ';', 3, 0, 0xd835 _ 0xdcbb)
NAMED_CHARACTER_REFERENCE(1090, /* g E */ ';', 1, 0, 0x2267 _ 0)
NAMED_CHARACTER_REFERENCE(1091, /* g E */ 'l' _ ';', 2, 0, 0x2a8c _ 0)
NAMED_CHARACTER_REFERENCE(1092,
                          /* g a */ 'c' _ 'u' _ 't' _ 'e' _ ';', 5, 0,
                          0x01f5 _ 0)
NAMED_CHARACTER_REFERENCE(1093,
                          /* g a */ 'm' _ 'm' _ 'a' _ ';', 4, 0, 0x03b3 _ 0)
NAMED_CHARACTER_REFERENCE(1094,
                          /* g a */ 'm' _ 'm' _ 'a' _ 'd' _ ';', 5, 0,
                          0x03dd _ 0)
NAMED_CHARACTER_REFERENCE(1095, /* g a */ 'p' _ ';', 2, 0, 0x2a86 _ 0)
NAMED_CHARACTER_REFERENCE(1096,
                          /* g b */ 'r' _ 'e' _ 'v' _ 'e' _ ';', 5, 0,
                          0x011f _ 0)
NAMED_CHARACTER_REFERENCE(1097,
                          /* g c */ 'i' _ 'r' _ 'c' _ ';', 4, 0, 0x011d _ 0)
NAMED_CHARACTER_REFERENCE(1098, /* g c */ 'y' _ ';', 2, 0, 0x0433 _ 0)
NAMED_CHARACTER_REFERENCE(1099, /* g d */ 'o' _ 't' _ ';', 3, 0, 0x0121 _ 0)
NAMED_CHARACTER_REFERENCE(1100, /* g e */ ';', 1, 0, 0x2265 _ 0)
NAMED_CHARACTER_REFERENCE(1101, /* g e */ 'l' _ ';', 2, 0, 0x22db _ 0)
NAMED_CHARACTER_REFERENCE(1102, /* g e */ 'q' _ ';', 2, 0, 0x2265 _ 0)
NAMED_CHARACTER_REFERENCE(1103, /* g e */ 'q' _ 'q' _ ';', 3, 0, 0x2267 _ 0)
NAMED_CHARACTER_REFERENCE(1104,
                          /* g e */ 'q' _ 's' _ 'l' _ 'a' _ 'n' _ 't' _ ';', 7,
                          0, 0x2a7e _ 0)
NAMED_CHARACTER_REFERENCE(1105, /* g e */ 's' _ ';', 2, 0, 0x2a7e _ 0)
NAMED_CHARACTER_REFERENCE(1106,
                          /* g e */ 's' _ 'c' _ 'c' _ ';', 4, 0, 0x2aa9 _ 0)
NAMED_CHARACTER_REFERENCE(1107,
                          /* g e */ 's' _ 'd' _ 'o' _ 't' _ ';', 5, 0,
                          0x2a80 _ 0)
NAMED_CHARACTER_REFERENCE(1108,
                          /* g e */ 's' _ 'd' _ 'o' _ 't' _ 'o' _ ';', 6, 0,
                          0x2a82 _ 0)
NAMED_CHARACTER_REFERENCE(1109,
                          /* g e */ 's' _ 'd' _ 'o' _ 't' _ 'o' _ 'l' _ ';', 7,
                          0, 0x2a84 _ 0)
NAMED_CHARACTER_REFERENCE(1110,
                          /* g e */ 's' _ 'l' _ ';', 3, 0, 0x22db _ 0xfe00)
NAMED_CHARACTER_REFERENCE(1111,
                          /* g e */ 's' _ 'l' _ 'e' _ 's' _ ';', 5, 0,
                          0x2a94 _ 0)
NAMED_CHARACTER_REFERENCE(1112, /* g f */ 'r' _ ';', 2, 0, 0xd835 _ 0xdd24)
NAMED_CHARACTER_REFERENCE(1113, /* g g */ ';', 1, 0, 0x226b _ 0)
NAMED_CHARACTER_REFERENCE(1114, /* g g */ 'g' _ ';', 2, 0, 0x22d9 _ 0)
NAMED_CHARACTER_REFERENCE(1115,
                          /* g i */ 'm' _ 'e' _ 'l' _ ';', 4, 0, 0x2137 _ 0)
NAMED_CHARACTER_REFERENCE(1116, /* g j */ 'c' _ 'y' _ ';', 3, 0, 0x0453 _ 0)
NAMED_CHARACTER_REFERENCE(1117, /* g l */ ';', 1, 0, 0x2277 _ 0)
NAMED_CHARACTER_REFERENCE(1118, /* g l */ 'E' _ ';', 2, 0, 0x2a92 _ 0)
NAMED_CHARACTER_REFERENCE(1119, /* g l */ 'a' _ ';', 2, 0, 0x2aa5 _ 0)
NAMED_CHARACTER_REFERENCE(1120, /* g l */ 'j' _ ';', 2, 0, 0x2aa4 _ 0)
NAMED_CHARACTER_REFERENCE(1121, /* g n */ 'E' _ ';', 2, 0, 0x2269 _ 0)
NAMED_CHARACTER_REFERENCE(1122, /* g n */ 'a' _ 'p' _ ';', 3, 0, 0x2a8a _ 0)
NAMED_CHARACTER_REFERENCE(1123,
                          /* g n */ 'a' _ 'p' _ 'p' _ 'r' _ 'o' _ 'x' _ ';', 7,
                          0, 0x2a8a _ 0)
NAMED_CHARACTER_REFERENCE(1124, /* g n */ 'e' _ ';', 2, 0, 0x2a88 _ 0)
NAMED_CHARACTER_REFERENCE(1125, /* g n */ 'e' _ 'q' _ ';', 3, 0, 0x2a88 _ 0)
NAMED_CHARACTER_REFERENCE(1126,
                          /* g n */ 'e' _ 'q' _ 'q' _ ';', 4, 0, 0x2269 _ 0)
NAMED_CHARACTER_REFERENCE(1127,
                          /* g n */ 's' _ 'i' _ 'm' _ ';', 4, 0, 0x22e7 _ 0)
NAMED_CHARACTER_REFERENCE(1128,
                          /* g o */ 'p' _ 'f' _ ';', 3, 0, 0xd835 _ 0xdd58)
NAMED_CHARACTER_REFERENCE(1129,
                          /* g r */ 'a' _ 'v' _ 'e' _ ';', 4, 0, 0x0060 _ 0)
NAMED_CHARACTER_REFERENCE(1130, /* g s */ 'c' _ 'r' _ ';', 3, 0, 0x210a _ 0)
NAMED_CHARACTER_REFERENCE(1131, /* g s */ 'i' _ 'm' _ ';', 3, 0, 0x2273 _ 0)
NAMED_CHARACTER_REFERENCE(1132,
                          /* g s */ 'i' _ 'm' _ 'e' _ ';', 4, 0, 0x2a8e _ 0)
NAMED_CHARACTER_REFERENCE(1133,
                          /* g s */ 'i' _ 'm' _ 'l' _ ';', 4, 0, 0x2a90 _ 0)
NAMED_CHARACTER_REFERENCE(1134, /* g t */ 0, 0, 1, 0x003e _ 0)
NAMED_CHARACTER_REFERENCE(1135, /* g t */ ';', 1, 0, 0x003e _ 0)
NAMED_CHARACTER_REFERENCE(1136, /* g t */ 'c' _ 'c' _ ';', 3, 0, 0x2aa7 _ 0)
NAMED_CHARACTER_REFERENCE(1137,
                          /* g t */ 'c' _ 'i' _ 'r' _ ';', 4, 0, 0x2a7a _ 0)
NAMED_CHARACTER_REFERENCE(1138,
                          /* g t */ 'd' _ 'o' _ 't' _ ';', 4, 0, 0x22d7 _ 0)
NAMED_CHARACTER_REFERENCE(1139,
                          /* g t */ 'l' _ 'P' _ 'a' _ 'r' _ ';', 5, 0,
                          0x2995 _ 0)
NAMED_CHARACTER_REFERENCE(1140,
                          /* g t */ 'q' _ 'u' _ 'e' _ 's' _ 't' _ ';', 6, 0,
                          0x2a7c _ 0)
NAMED_CHARACTER_REFERENCE(
    1141,
    /* g t */ 'r' _ 'a' _ 'p' _ 'p' _ 'r' _ 'o' _ 'x' _ ';', 8, 0, 0x2a86 _ 0)
NAMED_CHARACTER_REFERENCE(1142,
                          /* g t */ 'r' _ 'a' _ 'r' _ 'r' _ ';', 5, 0,
                          0x2978 _ 0)
NAMED_CHARACTER_REFERENCE(1143,
                          /* g t */ 'r' _ 'd' _ 'o' _ 't' _ ';', 5, 0,
                          0x22d7 _ 0)
NAMED_CHARACTER_REFERENCE(
    1144,
    /* g t */ 'r' _ 'e' _ 'q' _ 'l' _ 'e' _ 's' _ 's' _ ';', 8, 0, 0x22db _ 0)
NAMED_CHARACTER_REFERENCE(
    1145,
    /* g t */ 'r' _ 'e' _ 'q' _ 'q' _ 'l' _ 'e' _ 's' _ 's' _ ';', 9, 0,
    0x2a8c _ 0)
NAMED_CHARACTER_REFERENCE(1146,
                          /* g t */ 'r' _ 'l' _ 'e' _ 's' _ 's' _ ';', 6, 0,
                          0x2277 _ 0)
NAMED_CHARACTER_REFERENCE(1147,
                          /* g t */ 'r' _ 's' _ 'i' _ 'm' _ ';', 5, 0,
                          0x2273 _ 0)
NAMED_CHARACTER_REFERENCE(
    1148,
    /* g v */ 'e' _ 'r' _ 't' _ 'n' _ 'e' _ 'q' _ 'q' _ ';', 8, 0,
    0x2269 _ 0xfe00)
NAMED_CHARACTER_REFERENCE(1149,
                          /* g v */ 'n' _ 'E' _ ';', 3, 0, 0x2269 _ 0xfe00)
NAMED_CHARACTER_REFERENCE(1150, /* h A */ 'r' _ 'r' _ ';', 3, 0, 0x21d4 _ 0)
NAMED_CHARACTER_REFERENCE(1151,
                          /* h a */ 'i' _ 'r' _ 's' _ 'p' _ ';', 5, 0,
                          0x200a _ 0)
NAMED_CHARACTER_REFERENCE(1152, /* h a */ 'l' _ 'f' _ ';', 3, 0, 0x00bd _ 0)
NAMED_CHARACTER_REFERENCE(1153,
                          /* h a */ 'm' _ 'i' _ 'l' _ 't' _ ';', 5, 0,
                          0x210b _ 0)
NAMED_CHARACTER_REFERENCE(1154,
                          /* h a */ 'r' _ 'd' _ 'c' _ 'y' _ ';', 5, 0,
                          0x044a _ 0)
NAMED_CHARACTER_REFERENCE(1155, /* h a */ 'r' _ 'r' _ ';', 3, 0, 0x2194 _ 0)
NAMED_CHARACTER_REFERENCE(1156,
                          /* h a */ 'r' _ 'r' _ 'c' _ 'i' _ 'r' _ ';', 6, 0,
                          0x2948 _ 0)
NAMED_CHARACTER_REFERENCE(1157,
                          /* h a */ 'r' _ 'r' _ 'w' _ ';', 4, 0, 0x21ad _ 0)
NAMED_CHARACTER_REFERENCE(1158, /* h b */ 'a' _ 'r' _ ';', 3, 0, 0x210f _ 0)
NAMED_CHARACTER_REFERENCE(1159,
                          /* h c */ 'i' _ 'r' _ 'c' _ ';', 4, 0, 0x0125 _ 0)
NAMED_CHARACTER_REFERENCE(1160,
                          /* h e */ 'a' _ 'r' _ 't' _ 's' _ ';', 5, 0,
                          0x2665 _ 0)
NAMED_CHARACTER_REFERENCE(
    1161,
    /* h e */ 'a' _ 'r' _ 't' _ 's' _ 'u' _ 'i' _ 't' _ ';', 8, 0, 0x2665 _ 0)
NAMED_CHARACTER_REFERENCE(1162,
                          /* h e */ 'l' _ 'l' _ 'i' _ 'p' _ ';', 5, 0,
                          0x2026 _ 0)
NAMED_CHARACTER_REFERENCE(1163,
                          /* h e */ 'r' _ 'c' _ 'o' _ 'n' _ ';', 5, 0,
                          0x22b9 _ 0)
NAMED_CHARACTER_REFERENCE(1164, /* h f */ 'r' _ ';', 2, 0, 0xd835 _ 0xdd25)
NAMED_CHARACTER_REFERENCE(1165,
                          /* h k */ 's' _ 'e' _ 'a' _ 'r' _ 'o' _ 'w' _ ';', 7,
                          0, 0x2925 _ 0)
NAMED_CHARACTER_REFERENCE(1166,
                          /* h k */ 's' _ 'w' _ 'a' _ 'r' _ 'o' _ 'w' _ ';', 7,
                          0, 0x2926 _ 0)
NAMED_CHARACTER_REFERENCE(1167,
                          /* h o */ 'a' _ 'r' _ 'r' _ ';', 4, 0, 0x21ff _ 0)
NAMED_CHARACTER_REFERENCE(1168,
                          /* h o */ 'm' _ 't' _ 'h' _ 't' _ ';', 5, 0,
                          0x223b _ 0)
NAMED_CHARACTER_REFERENCE(
    1169,
    /* h o */
    'o' _ 'k' _ 'l' _ 'e' _ 'f' _ 't' _ 'a' _ 'r' _ 'r' _ 'o' _ 'w' _ ';', 12,
    0, 0x21a9 _ 0)
NAMED_CHARACTER_REFERENCE(
    1170,
    /* h o */
    'o' _ 'k' _ 'r' _ 'i' _ 'g' _ 'h' _ 't' _ 'a' _ 'r' _ 'r' _ 'o' _ 'w' _ ';',
    13, 0, 0x21aa _ 0)
NAMED_CHARACTER_REFERENCE(1171,
                          /* h o */ 'p' _ 'f' _ ';', 3, 0, 0xd835 _ 0xdd59)
NAMED_CHARACTER_REFERENCE(1172,
                          /* h o */ 'r' _ 'b' _ 'a' _ 'r' _ ';', 5, 0,
                          0x2015 _ 0)
NAMED_CHARACTER_REFERENCE(1173,
                          /* h s */ 'c' _ 'r' _ ';', 3, 0, 0xd835 _ 0xdcbd)
NAMED_CHARACTER_REFERENCE(1174,
                          /* h s */ 'l' _ 'a' _ 's' _ 'h' _ ';', 5, 0,
                          0x210f _ 0)
NAMED_CHARACTER_REFERENCE(1175,
                          /* h s */ 't' _ 'r' _ 'o' _ 'k' _ ';', 5, 0,
                          0x0127 _ 0)
NAMED_CHARACTER_REFERENCE(1176,
                          /* h y */ 'b' _ 'u' _ 'l' _ 'l' _ ';', 5, 0,
                          0x2043 _ 0)
NAMED_CHARACTER_REFERENCE(1177,
                          /* h y */ 'p' _ 'h' _ 'e' _ 'n' _ ';', 5, 0,
                          0x2010 _ 0)
NAMED_CHARACTER_REFERENCE(1178,
                          /* i a */ 'c' _ 'u' _ 't' _ 'e', 4, 0, 0x00ed _ 0)
NAMED_CHARACTER_REFERENCE(1179,
                          /* i a */ 'c' _ 'u' _ 't' _ 'e' _ ';', 5, 0,
                          0x00ed _ 0)
NAMED_CHARACTER_REFERENCE(1180, /* i c */ ';', 1, 0, 0x2063 _ 0)
NAMED_CHARACTER_REFERENCE(1181, /* i c */ 'i' _ 'r' _ 'c', 3, 0, 0x00ee _ 0)
NAMED_CHARACTER_REFERENCE(1182,
                          /* i c */ 'i' _ 'r' _ 'c' _ ';', 4, 0, 0x00ee _ 0)
NAMED_CHARACTER_REFERENCE(1183, /* i c */ 'y' _ ';', 2, 0, 0x0438 _ 0)
NAMED_CHARACTER_REFERENCE(1184, /* i e */ 'c' _ 'y' _ ';', 3, 0, 0x0435 _ 0)
NAMED_CHARACTER_REFERENCE(1185, /* i e */ 'x' _ 'c' _ 'l', 3, 0, 0x00a1 _ 0)
NAMED_CHARACTER_REFERENCE(1186,
                          /* i e */ 'x' _ 'c' _ 'l' _ ';', 4, 0, 0x00a1 _ 0)
NAMED_CHARACTER_REFERENCE(1187, /* i f */ 'f' _ ';', 2, 0, 0x21d4 _ 0)
NAMED_CHARACTER_REFERENCE(1188, /* i f */ 'r' _ ';', 2, 0, 0xd835 _ 0xdd26)
NAMED_CHARACTER_REFERENCE(1189,
                          /* i g */ 'r' _ 'a' _ 'v' _ 'e', 4, 0, 0x00ec _ 0)
NAMED_CHARACTER_REFERENCE(1190,
                          /* i g */ 'r' _ 'a' _ 'v' _ 'e' _ ';', 5, 0,
                          0x00ec _ 0)
NAMED_CHARACTER_REFERENCE(1191, /* i i */ ';', 1, 0, 0x2148 _ 0)
NAMED_CHARACTER_REFERENCE(1192,
                          /* i i */ 'i' _ 'i' _ 'n' _ 't' _ ';', 5, 0,
                          0x2a0c _ 0)
NAMED_CHARACTER_REFERENCE(1193,
                          /* i i */ 'i' _ 'n' _ 't' _ ';', 4, 0, 0x222d _ 0)
NAMED_CHARACTER_REFERENCE(1194,
                          /* i i */ 'n' _ 'f' _ 'i' _ 'n' _ ';', 5, 0,
                          0x29dc _ 0)
NAMED_CHARACTER_REFERENCE(1195,
                          /* i i */ 'o' _ 't' _ 'a' _ ';', 4, 0, 0x2129 _ 0)
NAMED_CHARACTER_REFERENCE(1196,
                          /* i j */ 'l' _ 'i' _ 'g' _ ';', 4, 0, 0x0133 _ 0)
NAMED_CHARACTER_REFERENCE(1197,
                          /* i m */ 'a' _ 'c' _ 'r' _ ';', 4, 0, 0x012b _ 0)
NAMED_CHARACTER_REFERENCE(1198,
                          /* i m */ 'a' _ 'g' _ 'e' _ ';', 4, 0, 0x2111 _ 0)
NAMED_CHARACTER_REFERENCE(1199,
                          /* i m */ 'a' _ 'g' _ 'l' _ 'i' _ 'n' _ 'e' _ ';', 7,
                          0, 0x2110 _ 0)
NAMED_CHARACTER_REFERENCE(1200,
                          /* i m */ 'a' _ 'g' _ 'p' _ 'a' _ 'r' _ 't' _ ';', 7,
                          0, 0x2111 _ 0)
NAMED_CHARACTER_REFERENCE(1201,
                          /* i m */ 'a' _ 't' _ 'h' _ ';', 4, 0, 0x0131 _ 0)
NAMED_CHARACTER_REFERENCE(1202, /* i m */ 'o' _ 'f' _ ';', 3, 0, 0x22b7 _ 0)
NAMED_CHARACTER_REFERENCE(1203,
                          /* i m */ 'p' _ 'e' _ 'd' _ ';', 4, 0, 0x01b5 _ 0)
NAMED_CHARACTER_REFERENCE(1204, /* i n */ ';', 1, 0, 0x2208 _ 0)
NAMED_CHARACTER_REFERENCE(1205,
                          /* i n */ 'c' _ 'a' _ 'r' _ 'e' _ ';', 5, 0,
                          0x2105 _ 0)
NAMED_CHARACTER_REFERENCE(1206,
                          /* i n */ 'f' _ 'i' _ 'n' _ ';', 4, 0, 0x221e _ 0)
NAMED_CHARACTER_REFERENCE(1207,
                          /* i n */ 'f' _ 'i' _ 'n' _ 't' _ 'i' _ 'e' _ ';', 7,
                          0, 0x29dd _ 0)
NAMED_CHARACTER_REFERENCE(1208,
                          /* i n */ 'o' _ 'd' _ 'o' _ 't' _ ';', 5, 0,
                          0x0131 _ 0)
NAMED_CHARACTER_REFERENCE(1209, /* i n */ 't' _ ';', 2, 0, 0x222b _ 0)
NAMED_CHARACTER_REFERENCE(1210,
                          /* i n */ 't' _ 'c' _ 'a' _ 'l' _ ';', 5, 0,
                          0x22ba _ 0)
NAMED_CHARACTER_REFERENCE(1211,
                          /* i n */ 't' _ 'e' _ 'g' _ 'e' _ 'r' _ 's' _ ';', 7,
                          0, 0x2124 _ 0)
NAMED_CHARACTER_REFERENCE(1212,
                          /* i n */ 't' _ 'e' _ 'r' _ 'c' _ 'a' _ 'l' _ ';', 7,
                          0, 0x22ba _ 0)
NAMED_CHARACTER_REFERENCE(1213,
                          /* i n */ 't' _ 'l' _ 'a' _ 'r' _ 'h' _ 'k' _ ';', 7,
                          0, 0x2a17 _ 0)
NAMED_CHARACTER_REFERENCE(1214,
                          /* i n */ 't' _ 'p' _ 'r' _ 'o' _ 'd' _ ';', 6, 0,
                          0x2a3c _ 0)
NAMED_CHARACTER_REFERENCE(1215, /* i o */ 'c' _ 'y' _ ';', 3, 0, 0x0451 _ 0)
NAMED_CHARACTER_REFERENCE(1216,
                          /* i o */ 'g' _ 'o' _ 'n' _ ';', 4, 0, 0x012f _ 0)
NAMED_CHARACTER_REFERENCE(1217,
                          /* i o */ 'p' _ 'f' _ ';', 3, 0, 0xd835 _ 0xdd5a)
NAMED_CHARACTER_REFERENCE(1218, /* i o */ 't' _ 'a' _ ';', 3, 0, 0x03b9 _ 0)
NAMED_CHARACTER_REFERENCE(1219,
                          /* i p */ 'r' _ 'o' _ 'd' _ ';', 4, 0, 0x2a3c _ 0)
NAMED_CHARACTER_REFERENCE(1220,
                          /* i q */ 'u' _ 'e' _ 's' _ 't', 4, 0, 0x00bf _ 0)
NAMED_CHARACTER_REFERENCE(1221,
                          /* i q */ 'u' _ 'e' _ 's' _ 't' _ ';', 5, 0,
                          0x00bf _ 0)
NAMED_CHARACTER_REFERENCE(1222,
                          /* i s */ 'c' _ 'r' _ ';', 3, 0, 0xd835 _ 0xdcbe)
NAMED_CHARACTER_REFERENCE(1223, /* i s */ 'i' _ 'n' _ ';', 3, 0, 0x2208 _ 0)
NAMED_CHARACTER_REFERENCE(1224,
                          /* i s */ 'i' _ 'n' _ 'E' _ ';', 4, 0, 0x22f9 _ 0)
NAMED_CHARACTER_REFERENCE(1225,
                          /* i s */ 'i' _ 'n' _ 'd' _ 'o' _ 't' _ ';', 6, 0,
                          0x22f5 _ 0)
NAMED_CHARACTER_REFERENCE(1226,
                          /* i s */ 'i' _ 'n' _ 's' _ ';', 4, 0, 0x22f4 _ 0)
NAMED_CHARACTER_REFERENCE(1227,
                          /* i s */ 'i' _ 'n' _ 's' _ 'v' _ ';', 5, 0,
                          0x22f3 _ 0)
NAMED_CHARACTER_REFERENCE(1228,
                          /* i s */ 'i' _ 'n' _ 'v' _ ';', 4, 0, 0x2208 _ 0)
NAMED_CHARACTER_REFERENCE(1229, /* i t */ ';', 1, 0, 0x2062 _ 0)
NAMED_CHARACTER_REFERENCE(1230,
                          /* i t */ 'i' _ 'l' _ 'd' _ 'e' _ ';', 5, 0,
                          0x0129 _ 0)
NAMED_CHARACTER_REFERENCE(1231,
                          /* i u */ 'k' _ 'c' _ 'y' _ ';', 4, 0, 0x0456 _ 0)
NAMED_CHARACTER_REFERENCE(1232, /* i u */ 'm' _ 'l', 2, 0, 0x00ef _ 0)
NAMED_CHARACTER_REFERENCE(1233, /* i u */ 'm' _ 'l' _ ';', 3, 0, 0x00ef _ 0)
NAMED_CHARACTER_REFERENCE(1234,
                          /* j c */ 'i' _ 'r' _ 'c' _ ';', 4, 0, 0x0135 _ 0)
NAMED_CHARACTER_REFERENCE(1235, /* j c */ 'y' _ ';', 2, 0, 0x0439 _ 0)
NAMED_CHARACTER_REFERENCE(1236, /* j f */ 'r' _ ';', 2, 0, 0xd835 _ 0xdd27)
NAMED_CHARACTER_REFERENCE(1237,
                          /* j m */ 'a' _ 't' _ 'h' _ ';', 4, 0, 0x0237 _ 0)
NAMED_CHARACTER_REFERENCE(1238,
                          /* j o */ 'p' _ 'f' _ ';', 3, 0, 0xd835 _ 0xdd5b)
NAMED_CHARACTER_REFERENCE(1239,
                          /* j s */ 'c' _ 'r' _ ';', 3, 0, 0xd835 _ 0xdcbf)
NAMED_CHARACTER_REFERENCE(1240,
                          /* j s */ 'e' _ 'r' _ 'c' _ 'y' _ ';', 5, 0,
                          0x0458 _ 0)
NAMED_CHARACTER_REFERENCE(1241,
                          /* j u */ 'k' _ 'c' _ 'y' _ ';', 4, 0, 0x0454 _ 0)
NAMED_CHARACTER_REFERENCE(1242,
                          /* k a */ 'p' _ 'p' _ 'a' _ ';', 4, 0, 0x03ba _ 0)
NAMED_CHARACTER_REFERENCE(1243,
                          /* k a */ 'p' _ 'p' _ 'a' _ 'v' _ ';', 5, 0,
                          0x03f0 _ 0)
NAMED_CHARACTER_REFERENCE(1244,
                          /* k c */ 'e' _ 'd' _ 'i' _ 'l' _ ';', 5, 0,
                          0x0137 _ 0)
NAMED_CHARACTER_REFERENCE(1245, /* k c */ 'y' _ ';', 2, 0, 0x043a _ 0)
NAMED_CHARACTER_REFERENCE(1246, /* k f */ 'r' _ ';', 2, 0, 0xd835 _ 0xdd28)
NAMED_CHARACTER_REFERENCE(1247,
                          /* k g */ 'r' _ 'e' _ 'e' _ 'n' _ ';', 5, 0,
                          0x0138 _ 0)
NAMED_CHARACTER_REFERENCE(1248, /* k h */ 'c' _ 'y' _ ';', 3, 0, 0x0445 _ 0)
NAMED_CHARACTER_REFERENCE(1249, /* k j */ 'c' _ 'y' _ ';', 3, 0, 0x045c _ 0)
NAMED_CHARACTER_REFERENCE(1250,
                          /* k o */ 'p' _ 'f' _ ';', 3, 0, 0xd835 _ 0xdd5c)
NAMED_CHARACTER_REFERENCE(1251,
                          /* k s */ 'c' _ 'r' _ ';', 3, 0, 0xd835 _ 0xdcc0)
NAMED_CHARACTER_REFERENCE(1252,
                          /* l A */ 'a' _ 'r' _ 'r' _ ';', 4, 0, 0x21da _ 0)
NAMED_CHARACTER_REFERENCE(1253, /* l A */ 'r' _ 'r' _ ';', 3, 0, 0x21d0 _ 0)
NAMED_CHARACTER_REFERENCE(1254,
                          /* l A */ 't' _ 'a' _ 'i' _ 'l' _ ';', 5, 0,
                          0x291b _ 0)
NAMED_CHARACTER_REFERENCE(1255,
                          /* l B */ 'a' _ 'r' _ 'r' _ ';', 4, 0, 0x290e _ 0)
NAMED_CHARACTER_REFERENCE(1256, /* l E */ ';', 1, 0, 0x2266 _ 0)
NAMED_CHARACTER_REFERENCE(1257, /* l E */ 'g' _ ';', 2, 0, 0x2a8b _ 0)
NAMED_CHARACTER_REFERENCE(1258, /* l H */ 'a' _ 'r' _ ';', 3, 0, 0x2962 _ 0)
NAMED_CHARACTER_REFERENCE(1259,
                          /* l a */ 'c' _ 'u' _ 't' _ 'e' _ ';', 5, 0,
                          0x013a _ 0)
NAMED_CHARACTER_REFERENCE(1260,
                          /* l a */ 'e' _ 'm' _ 'p' _ 't' _ 'y' _ 'v' _ ';', 7,
                          0, 0x29b4 _ 0)
NAMED_CHARACTER_REFERENCE(1261,
                          /* l a */ 'g' _ 'r' _ 'a' _ 'n' _ ';', 5, 0,
                          0x2112 _ 0)
NAMED_CHARACTER_REFERENCE(1262,
                          /* l a */ 'm' _ 'b' _ 'd' _ 'a' _ ';', 5, 0,
                          0x03bb _ 0)
NAMED_CHARACTER_REFERENCE(1263, /* l a */ 'n' _ 'g' _ ';', 3, 0, 0x27e8 _ 0)
NAMED_CHARACTER_REFERENCE(1264,
                          /* l a */ 'n' _ 'g' _ 'd' _ ';', 4, 0, 0x2991 _ 0)
NAMED_CHARACTER_REFERENCE(1265,
                          /* l a */ 'n' _ 'g' _ 'l' _ 'e' _ ';', 5, 0,
                          0x27e8 _ 0)
NAMED_CHARACTER_REFERENCE(1266, /* l a */ 'p' _ ';', 2, 0, 0x2a85 _ 0)
NAMED_CHARACTER_REFERENCE(1267, /* l a */ 'q' _ 'u' _ 'o', 3, 0, 0x00ab _ 0)
NAMED_CHARACTER_REFERENCE(1268,
                          /* l a */ 'q' _ 'u' _ 'o' _ ';', 4, 0, 0x00ab _ 0)
NAMED_CHARACTER_REFERENCE(1269, /* l a */ 'r' _ 'r' _ ';', 3, 0, 0x2190 _ 0)
NAMED_CHARACTER_REFERENCE(1270,
                          /* l a */ 'r' _ 'r' _ 'b' _ ';', 4, 0, 0x21e4 _ 0)
NAMED_CHARACTER_REFERENCE(1271,
                          /* l a */ 'r' _ 'r' _ 'b' _ 'f' _ 's' _ ';', 6, 0,
                          0x291f _ 0)
NAMED_CHARACTER_REFERENCE(1272,
                          /* l a */ 'r' _ 'r' _ 'f' _ 's' _ ';', 5, 0,
                          0x291d _ 0)
NAMED_CHARACTER_REFERENCE(1273,
                          /* l a */ 'r' _ 'r' _ 'h' _ 'k' _ ';', 5, 0,
                          0x21a9 _ 0)
NAMED_CHARACTER_REFERENCE(1274,
                          /* l a */ 'r' _ 'r' _ 'l' _ 'p' _ ';', 5, 0,
                          0x21ab _ 0)
NAMED_CHARACTER_REFERENCE(1275,
                          /* l a */ 'r' _ 'r' _ 'p' _ 'l' _ ';', 5, 0,
                          0x2939 _ 0)
NAMED_CHARACTER_REFERENCE(1276,
                          /* l a */ 'r' _ 'r' _ 's' _ 'i' _ 'm' _ ';', 6, 0,
                          0x2973 _ 0)
NAMED_CHARACTER_REFERENCE(1277,
                          /* l a */ 'r' _ 'r' _ 't' _ 'l' _ ';', 5, 0,
                          0x21a2 _ 0)
NAMED_CHARACTER_REFERENCE(1278, /* l a */ 't' _ ';', 2, 0, 0x2aab _ 0)
NAMED_CHARACTER_REFERENCE(1279,
                          /* l a */ 't' _ 'a' _ 'i' _ 'l' _ ';', 5, 0,
                          0x2919 _ 0)
NAMED_CHARACTER_REFERENCE(1280, /* l a */ 't' _ 'e' _ ';', 3, 0, 0x2aad _ 0)
NAMED_CHARACTER_REFERENCE(1281,
                          /* l a */ 't' _ 'e' _ 's' _ ';', 4, 0,
                          0x2aad _ 0xfe00)
NAMED_CHARACTER_REFERENCE(1282,
                          /* l b */ 'a' _ 'r' _ 'r' _ ';', 4, 0, 0x290c _ 0)
NAMED_CHARACTER_REFERENCE(1283,
                          /* l b */ 'b' _ 'r' _ 'k' _ ';', 4, 0, 0x2772 _ 0)
NAMED_CHARACTER_REFERENCE(1284,
                          /* l b */ 'r' _ 'a' _ 'c' _ 'e' _ ';', 5, 0,
                          0x007b _ 0)
NAMED_CHARACTER_REFERENCE(1285,
                          /* l b */ 'r' _ 'a' _ 'c' _ 'k' _ ';', 5, 0,
                          0x005b _ 0)
NAMED_CHARACTER_REFERENCE(1286,
                          /* l b */ 'r' _ 'k' _ 'e' _ ';', 4, 0, 0x298b _ 0)
NAMED_CHARACTER_REFERENCE(1287,
                          /* l b */ 'r' _ 'k' _ 's' _ 'l' _ 'd' _ ';', 6, 0,
                          0x298f _ 0)
NAMED_CHARACTER_REFERENCE(1288,
                          /* l b */ 'r' _ 'k' _ 's' _ 'l' _ 'u' _ ';', 6, 0,
                          0x298d _ 0)
NAMED_CHARACTER_REFERENCE(1289,
                          /* l c */ 'a' _ 'r' _ 'o' _ 'n' _ ';', 5, 0,
                          0x013e _ 0)
NAMED_CHARACTER_REFERENCE(1290,
                          /* l c */ 'e' _ 'd' _ 'i' _ 'l' _ ';', 5, 0,
                          0x013c _ 0)
NAMED_CHARACTER_REFERENCE(1291,
                          /* l c */ 'e' _ 'i' _ 'l' _ ';', 4, 0, 0x2308 _ 0)
NAMED_CHARACTER_REFERENCE(1292, /* l c */ 'u' _ 'b' _ ';', 3, 0, 0x007b _ 0)
NAMED_CHARACTER_REFERENCE(1293, /* l c */ 'y' _ ';', 2, 0, 0x043b _ 0)
NAMED_CHARACTER_REFERENCE(1294, /* l d */ 'c' _ 'a' _ ';', 3, 0, 0x2936 _ 0)
NAMED_CHARACTER_REFERENCE(1295,
                          /* l d */ 'q' _ 'u' _ 'o' _ ';', 4, 0, 0x201c _ 0)
NAMED_CHARACTER_REFERENCE(1296,
                          /* l d */ 'q' _ 'u' _ 'o' _ 'r' _ ';', 5, 0,
                          0x201e _ 0)
NAMED_CHARACTER_REFERENCE(1297,
                          /* l d */ 'r' _ 'd' _ 'h' _ 'a' _ 'r' _ ';', 6, 0,
                          0x2967 _ 0)
NAMED_CHARACTER_REFERENCE(1298,
                          /* l d */ 'r' _ 'u' _ 's' _ 'h' _ 'a' _ 'r' _ ';', 7,
                          0, 0x294b _ 0)
NAMED_CHARACTER_REFERENCE(1299, /* l d */ 's' _ 'h' _ ';', 3, 0, 0x21b2 _ 0)
NAMED_CHARACTER_REFERENCE(1300, /* l e */ ';', 1, 0, 0x2264 _ 0)
NAMED_CHARACTER_REFERENCE(
    1301,
    /* l e */ 'f' _ 't' _ 'a' _ 'r' _ 'r' _ 'o' _ 'w' _ ';', 8, 0, 0x2190 _ 0)
NAMED_CHARACTER_REFERENCE(
    1302,
    /* l e */
    'f' _ 't' _ 'a' _ 'r' _ 'r' _ 'o' _ 'w' _ 't' _ 'a' _ 'i' _ 'l' _ ';', 12,
    0, 0x21a2 _ 0)
NAMED_CHARACTER_REFERENCE(
    1303,
    /* l e */
    'f' _ 't' _ 'h' _ 'a' _ 'r' _ 'p' _ 'o' _ 'o' _ 'n' _ 'd' _ 'o' _ 'w' _ 'n' _ ';',
    14, 0, 0x21bd _ 0)
NAMED_CHARACTER_REFERENCE(
    1304,
    /* l e */
    'f' _ 't' _ 'h' _ 'a' _ 'r' _ 'p' _ 'o' _ 'o' _ 'n' _ 'u' _ 'p' _ ';', 12,
    0, 0x21bc _ 0)
NAMED_CHARACTER_REFERENCE(
    1305,
    /* l e */
    'f' _ 't' _ 'l' _ 'e' _ 'f' _ 't' _ 'a' _ 'r' _ 'r' _ 'o' _ 'w' _ 's' _ ';',
    13, 0, 0x21c7 _ 0)
NAMED_CHARACTER_REFERENCE(
    1306,
    /* l e */
    'f' _ 't' _ 'r' _ 'i' _ 'g' _ 'h' _ 't' _ 'a' _ 'r' _ 'r' _ 'o' _ 'w' _ ';',
    13, 0, 0x2194 _ 0)
NAMED_CHARACTER_REFERENCE(
    1307,
    /* l e */
    'f' _ 't' _ 'r' _ 'i' _ 'g' _ 'h' _ 't' _ 'a' _ 'r' _ 'r' _ 'o' _ 'w' _ 's' _ ';',
    14, 0, 0x21c6 _ 0)
NAMED_CHARACTER_REFERENCE(
    1308,
    /* l e */
    'f' _ 't' _ 'r' _ 'i' _ 'g' _ 'h' _ 't' _ 'h' _ 'a' _ 'r' _ 'p' _ 'o' _ 'o' _ 'n' _ 's' _ ';',
    16, 0, 0x21cb _ 0)
NAMED_CHARACTER_REFERENCE(
    1309,
    /* l e */
    'f' _ 't' _ 'r' _ 'i' _ 'g' _ 'h' _ 't' _ 's' _ 'q' _ 'u' _ 'i' _ 'g' _ 'a' _ 'r' _ 'r' _ 'o' _ 'w' _ ';',
    18, 0, 0x21ad _ 0)
NAMED_CHARACTER_REFERENCE(
    1310,
    /* l e */
    'f' _ 't' _ 't' _ 'h' _ 'r' _ 'e' _ 'e' _ 't' _ 'i' _ 'm' _ 'e' _ 's' _ ';',
    13, 0, 0x22cb _ 0)
NAMED_CHARACTER_REFERENCE(1311, /* l e */ 'g' _ ';', 2, 0, 0x22da _ 0)
NAMED_CHARACTER_REFERENCE(1312, /* l e */ 'q' _ ';', 2, 0, 0x2264 _ 0)
NAMED_CHARACTER_REFERENCE(1313, /* l e */ 'q' _ 'q' _ ';', 3, 0, 0x2266 _ 0)
NAMED_CHARACTER_REFERENCE(1314,
                          /* l e */ 'q' _ 's' _ 'l' _ 'a' _ 'n' _ 't' _ ';', 7,
                          0, 0x2a7d _ 0)
NAMED_CHARACTER_REFERENCE(1315, /* l e */ 's' _ ';', 2, 0, 0x2a7d _ 0)
NAMED_CHARACTER_REFERENCE(1316,
                          /* l e */ 's' _ 'c' _ 'c' _ ';', 4, 0, 0x2aa8 _ 0)
NAMED_CHARACTER_REFERENCE(1317,
                          /* l e */ 's' _ 'd' _ 'o' _ 't' _ ';', 5, 0,
                          0x2a7f _ 0)
NAMED_CHARACTER_REFERENCE(1318,
                          /* l e */ 's' _ 'd' _ 'o' _ 't' _ 'o' _ ';', 6, 0,
                          0x2a81 _ 0)
NAMED_CHARACTER_REFERENCE(1319,
                          /* l e */ 's' _ 'd' _ 'o' _ 't' _ 'o' _ 'r' _ ';', 7,
                          0, 0x2a83 _ 0)
NAMED_CHARACTER_REFERENCE(1320,
                          /* l e */ 's' _ 'g' _ ';', 3, 0, 0x22da _ 0xfe00)
NAMED_CHARACTER_REFERENCE(1321,
                          /* l e */ 's' _ 'g' _ 'e' _ 's' _ ';', 5, 0,
                          0x2a93 _ 0)
NAMED_CHARACTER_REFERENCE(
    1322,
    /* l e */ 's' _ 's' _ 'a' _ 'p' _ 'p' _ 'r' _ 'o' _ 'x' _ ';', 9, 0,
    0x2a85 _ 0)
NAMED_CHARACTER_REFERENCE(1323,
                          /* l e */ 's' _ 's' _ 'd' _ 'o' _ 't' _ ';', 6, 0,
                          0x22d6 _ 0)
NAMED_CHARACTER_REFERENCE(
    1324,
    /* l e */ 's' _ 's' _ 'e' _ 'q' _ 'g' _ 't' _ 'r' _ ';', 8, 0, 0x22da _ 0)
NAMED_CHARACTER_REFERENCE(
    1325,
    /* l e */ 's' _ 's' _ 'e' _ 'q' _ 'q' _ 'g' _ 't' _ 'r' _ ';', 9, 0,
    0x2a8b _ 0)
NAMED_CHARACTER_REFERENCE(1326,
                          /* l e */ 's' _ 's' _ 'g' _ 't' _ 'r' _ ';', 6, 0,
                          0x2276 _ 0)
NAMED_CHARACTER_REFERENCE(1327,
                          /* l e */ 's' _ 's' _ 's' _ 'i' _ 'm' _ ';', 6, 0,
                          0x2272 _ 0)
NAMED_CHARACTER_REFERENCE(1328,
                          /* l f */ 'i' _ 's' _ 'h' _ 't' _ ';', 5, 0,
                          0x297c _ 0)
NAMED_CHARACTER_REFERENCE(1329,
                          /* l f */ 'l' _ 'o' _ 'o' _ 'r' _ ';', 5, 0,
                          0x230a _ 0)
NAMED_CHARACTER_REFERENCE(1330, /* l f */ 'r' _ ';', 2, 0, 0xd835 _ 0xdd29)
NAMED_CHARACTER_REFERENCE(1331, /* l g */ ';', 1, 0, 0x2276 _ 0)
NAMED_CHARACTER_REFERENCE(1332, /* l g */ 'E' _ ';', 2, 0, 0x2a91 _ 0)
NAMED_CHARACTER_REFERENCE(1333,
                          /* l h */ 'a' _ 'r' _ 'd' _ ';', 4, 0, 0x21bd _ 0)
NAMED_CHARACTER_REFERENCE(1334,
                          /* l h */ 'a' _ 'r' _ 'u' _ ';', 4, 0, 0x21bc _ 0)
NAMED_CHARACTER_REFERENCE(1335,
                          /* l h */ 'a' _ 'r' _ 'u' _ 'l' _ ';', 5, 0,
                          0x296a _ 0)
NAMED_CHARACTER_REFERENCE(1336,
                          /* l h */ 'b' _ 'l' _ 'k' _ ';', 4, 0, 0x2584 _ 0)
NAMED_CHARACTER_REFERENCE(1337, /* l j */ 'c' _ 'y' _ ';', 3, 0, 0x0459 _ 0)
NAMED_CHARACTER_REFERENCE(1338, /* l l */ ';', 1, 0, 0x226a _ 0)
NAMED_CHARACTER_REFERENCE(1339,
                          /* l l */ 'a' _ 'r' _ 'r' _ ';', 4, 0, 0x21c7 _ 0)
NAMED_CHARACTER_REFERENCE(1340,
                          /* l l */ 'c' _ 'o' _ 'r' _ 'n' _ 'e' _ 'r' _ ';', 7,
                          0, 0x231e _ 0)
NAMED_CHARACTER_REFERENCE(1341,
                          /* l l */ 'h' _ 'a' _ 'r' _ 'd' _ ';', 5, 0,
                          0x296b _ 0)
NAMED_CHARACTER_REFERENCE(1342,
                          /* l l */ 't' _ 'r' _ 'i' _ ';', 4, 0, 0x25fa _ 0)
NAMED_CHARACTER_REFERENCE(1343,
                          /* l m */ 'i' _ 'd' _ 'o' _ 't' _ ';', 5, 0,
                          0x0140 _ 0)
NAMED_CHARACTER_REFERENCE(1344,
                          /* l m */ 'o' _ 'u' _ 's' _ 't' _ ';', 5, 0,
                          0x23b0 _ 0)
NAMED_CHARACTER_REFERENCE(
    1345,
    /* l m */ 'o' _ 'u' _ 's' _ 't' _ 'a' _ 'c' _ 'h' _ 'e' _ ';', 9, 0,
    0x23b0 _ 0)
NAMED_CHARACTER_REFERENCE(1346, /* l n */ 'E' _ ';', 2, 0, 0x2268 _ 0)
NAMED_CHARACTER_REFERENCE(1347, /* l n */ 'a' _ 'p' _ ';', 3, 0, 0x2a89 _ 0)
NAMED_CHARACTER_REFERENCE(1348,
                          /* l n */ 'a' _ 'p' _ 'p' _ 'r' _ 'o' _ 'x' _ ';', 7,
                          0, 0x2a89 _ 0)
NAMED_CHARACTER_REFERENCE(1349, /* l n */ 'e' _ ';', 2, 0, 0x2a87 _ 0)
NAMED_CHARACTER_REFERENCE(1350, /* l n */ 'e' _ 'q' _ ';', 3, 0, 0x2a87 _ 0)
NAMED_CHARACTER_REFERENCE(1351,
                          /* l n */ 'e' _ 'q' _ 'q' _ ';', 4, 0, 0x2268 _ 0)
NAMED_CHARACTER_REFERENCE(1352,
                          /* l n */ 's' _ 'i' _ 'm' _ ';', 4, 0, 0x22e6 _ 0)
NAMED_CHARACTER_REFERENCE(1353,
                          /* l o */ 'a' _ 'n' _ 'g' _ ';', 4, 0, 0x27ec _ 0)
NAMED_CHARACTER_REFERENCE(1354,
                          /* l o */ 'a' _ 'r' _ 'r' _ ';', 4, 0, 0x21fd _ 0)
NAMED_CHARACTER_REFERENCE(1355,
                          /* l o */ 'b' _ 'r' _ 'k' _ ';', 4, 0, 0x27e6 _ 0)
NAMED_CHARACTER_REFERENCE(
    1356,
    /* l o */
    'n' _ 'g' _ 'l' _ 'e' _ 'f' _ 't' _ 'a' _ 'r' _ 'r' _ 'o' _ 'w' _ ';', 12,
    0, 0x27f5 _ 0)
NAMED_CHARACTER_REFERENCE(
    1357,
    /* l o */
    'n' _ 'g' _ 'l' _ 'e' _ 'f' _ 't' _ 'r' _ 'i' _ 'g' _ 'h' _ 't' _ 'a' _ 'r' _ 'r' _ 'o' _ 'w' _ ';',
    17, 0, 0x27f7 _ 0)
NAMED_CHARACTER_REFERENCE(
    1358,
    /* l o */ 'n' _ 'g' _ 'm' _ 'a' _ 'p' _ 's' _ 't' _ 'o' _ ';', 9, 0,
    0x27fc _ 0)
NAMED_CHARACTER_REFERENCE(
    1359,
    /* l o */
    'n' _ 'g' _ 'r' _ 'i' _ 'g' _ 'h' _ 't' _ 'a' _ 'r' _ 'r' _ 'o' _ 'w' _ ';',
    13, 0, 0x27f6 _ 0)
NAMED_CHARACTER_REFERENCE(
    1360,
    /* l o */
    'o' _ 'p' _ 'a' _ 'r' _ 'r' _ 'o' _ 'w' _ 'l' _ 'e' _ 'f' _ 't' _ ';', 12,
    0, 0x21ab _ 0)
NAMED_CHARACTER_REFERENCE(
    1361,
    /* l o */
    'o' _ 'p' _ 'a' _ 'r' _ 'r' _ 'o' _ 'w' _ 'r' _ 'i' _ 'g' _ 'h' _ 't' _ ';',
    13, 0, 0x21ac _ 0)
NAMED_CHARACTER_REFERENCE(1362,
                          /* l o */ 'p' _ 'a' _ 'r' _ ';', 4, 0, 0x2985 _ 0)
NAMED_CHARACTER_REFERENCE(1363,
                          /* l o */ 'p' _ 'f' _ ';', 3, 0, 0xd835 _ 0xdd5d)
NAMED_CHARACTER_REFERENCE(1364,
                          /* l o */ 'p' _ 'l' _ 'u' _ 's' _ ';', 5, 0,
                          0x2a2d _ 0)
NAMED_CHARACTER_REFERENCE(1365,
                          /* l o */ 't' _ 'i' _ 'm' _ 'e' _ 's' _ ';', 6, 0,
                          0x2a34 _ 0)
NAMED_CHARACTER_REFERENCE(1366,
                          /* l o */ 'w' _ 'a' _ 's' _ 't' _ ';', 5, 0,
                          0x2217 _ 0)
NAMED_CHARACTER_REFERENCE(1367,
                          /* l o */ 'w' _ 'b' _ 'a' _ 'r' _ ';', 5, 0,
                          0x005f _ 0)
NAMED_CHARACTER_REFERENCE(1368, /* l o */ 'z' _ ';', 2, 0, 0x25ca _ 0)
NAMED_CHARACTER_REFERENCE(1369,
                          /* l o */ 'z' _ 'e' _ 'n' _ 'g' _ 'e' _ ';', 6, 0,
                          0x25ca _ 0)
NAMED_CHARACTER_REFERENCE(1370, /* l o */ 'z' _ 'f' _ ';', 3, 0, 0x29eb _ 0)
NAMED_CHARACTER_REFERENCE(1371, /* l p */ 'a' _ 'r' _ ';', 3, 0, 0x0028 _ 0)
NAMED_CHARACTER_REFERENCE(1372,
                          /* l p */ 'a' _ 'r' _ 'l' _ 't' _ ';', 5, 0,
                          0x2993 _ 0)
NAMED_CHARACTER_REFERENCE(1373,
                          /* l r */ 'a' _ 'r' _ 'r' _ ';', 4, 0, 0x21c6 _ 0)
NAMED_CHARACTER_REFERENCE(1374,
                          /* l r */ 'c' _ 'o' _ 'r' _ 'n' _ 'e' _ 'r' _ ';', 7,
                          0, 0x231f _ 0)
NAMED_CHARACTER_REFERENCE(1375,
                          /* l r */ 'h' _ 'a' _ 'r' _ ';', 4, 0, 0x21cb _ 0)
NAMED_CHARACTER_REFERENCE(1376,
                          /* l r */ 'h' _ 'a' _ 'r' _ 'd' _ ';', 5, 0,
                          0x296d _ 0)
NAMED_CHARACTER_REFERENCE(1377, /* l r */ 'm' _ ';', 2, 0, 0x200e _ 0)
NAMED_CHARACTER_REFERENCE(1378,
                          /* l r */ 't' _ 'r' _ 'i' _ ';', 4, 0, 0x22bf _ 0)
NAMED_CHARACTER_REFERENCE(1379,
                          /* l s */ 'a' _ 'q' _ 'u' _ 'o' _ ';', 5, 0,
                          0x2039 _ 0)
NAMED_CHARACTER_REFERENCE(1380,
                          /* l s */ 'c' _ 'r' _ ';', 3, 0, 0xd835 _ 0xdcc1)
NAMED_CHARACTER_REFERENCE(1381, /* l s */ 'h' _ ';', 2, 0, 0x21b0 _ 0)
NAMED_CHARACTER_REFERENCE(1382, /* l s */ 'i' _ 'm' _ ';', 3, 0, 0x2272 _ 0)
NAMED_CHARACTER_REFERENCE(1383,
                          /* l s */ 'i' _ 'm' _ 'e' _ ';', 4, 0, 0x2a8d _ 0)
NAMED_CHARACTER_REFERENCE(1384,
                          /* l s */ 'i' _ 'm' _ 'g' _ ';', 4, 0, 0x2a8f _ 0)
NAMED_CHARACTER_REFERENCE(1385, /* l s */ 'q' _ 'b' _ ';', 3, 0, 0x005b _ 0)
NAMED_CHARACTER_REFERENCE(1386,
                          /* l s */ 'q' _ 'u' _ 'o' _ ';', 4, 0, 0x2018 _ 0)
NAMED_CHARACTER_REFERENCE(1387,
                          /* l s */ 'q' _ 'u' _ 'o' _ 'r' _ ';', 5, 0,
                          0x201a _ 0)
NAMED_CHARACTER_REFERENCE(1388,
                          /* l s */ 't' _ 'r' _ 'o' _ 'k' _ ';', 5, 0,
                          0x0142 _ 0)
NAMED_CHARACTER_REFERENCE(1389, /* l t */ 0, 0, 1, 0x003c _ 0)
NAMED_CHARACTER_REFERENCE(1390, /* l t */ ';', 1, 0, 0x003c _ 0)
NAMED_CHARACTER_REFERENCE(1391, /* l t */ 'c' _ 'c' _ ';', 3, 0, 0x2aa6 _ 0)
NAMED_CHARACTER_REFERENCE(1392,
                          /* l t */ 'c' _ 'i' _ 'r' _ ';', 4, 0, 0x2a79 _ 0)
NAMED_CHARACTER_REFERENCE(1393,
                          /* l t */ 'd' _ 'o' _ 't' _ ';', 4, 0, 0x22d6 _ 0)
NAMED_CHARACTER_REFERENCE(1394,
                          /* l t */ 'h' _ 'r' _ 'e' _ 'e' _ ';', 5, 0,
                          0x22cb _ 0)
NAMED_CHARACTER_REFERENCE(1395,
                          /* l t */ 'i' _ 'm' _ 'e' _ 's' _ ';', 5, 0,
                          0x22c9 _ 0)
NAMED_CHARACTER_REFERENCE(1396,
                          /* l t */ 'l' _ 'a' _ 'r' _ 'r' _ ';', 5, 0,
                          0x2976 _ 0)
NAMED_CHARACTER_REFERENCE(1397,
                          /* l t */ 'q' _ 'u' _ 'e' _ 's' _ 't' _ ';', 6, 0,
                          0x2a7b _ 0)
NAMED_CHARACTER_REFERENCE(1398,
                          /* l t */ 'r' _ 'P' _ 'a' _ 'r' _ ';', 5, 0,
                          0x2996 _ 0)
NAMED_CHARACTER_REFERENCE(1399, /* l t */ 'r' _ 'i' _ ';', 3, 0, 0x25c3 _ 0)
NAMED_CHARACTER_REFERENCE(1400,
                          /* l t */ 'r' _ 'i' _ 'e' _ ';', 4, 0, 0x22b4 _ 0)
NAMED_CHARACTER_REFERENCE(1401,
                          /* l t */ 'r' _ 'i' _ 'f' _ ';', 4, 0, 0x25c2 _ 0)
NAMED_CHARACTER_REFERENCE(1402,
                          /* l u */ 'r' _ 'd' _ 's' _ 'h' _ 'a' _ 'r' _ ';', 7,
                          0, 0x294a _ 0)
NAMED_CHARACTER_REFERENCE(1403,
                          /* l u */ 'r' _ 'u' _ 'h' _ 'a' _ 'r' _ ';', 6, 0,
                          0x2966 _ 0)
NAMED_CHARACTER_REFERENCE(
    1404,
    /* l v */ 'e' _ 'r' _ 't' _ 'n' _ 'e' _ 'q' _ 'q' _ ';', 8, 0,
    0x2268 _ 0xfe00)
NAMED_CHARACTER_REFERENCE(1405,
                          /* l v */ 'n' _ 'E' _ ';', 3, 0, 0x2268 _ 0xfe00)
NAMED_CHARACTER_REFERENCE(1406,
                          /* m D */ 'D' _ 'o' _ 't' _ ';', 4, 0, 0x223a _ 0)
NAMED_CHARACTER_REFERENCE(1407, /* m a */ 'c' _ 'r', 2, 0, 0x00af _ 0)
NAMED_CHARACTER_REFERENCE(1408, /* m a */ 'c' _ 'r' _ ';', 3, 0, 0x00af _ 0)
NAMED_CHARACTER_REFERENCE(1409, /* m a */ 'l' _ 'e' _ ';', 3, 0, 0x2642 _ 0)
NAMED_CHARACTER_REFERENCE(1410, /* m a */ 'l' _ 't' _ ';', 3, 0, 0x2720 _ 0)
NAMED_CHARACTER_REFERENCE(1411,
                          /* m a */ 'l' _ 't' _ 'e' _ 's' _ 'e' _ ';', 6, 0,
                          0x2720 _ 0)
NAMED_CHARACTER_REFERENCE(1412, /* m a */ 'p' _ ';', 2, 0, 0x21a6 _ 0)
NAMED_CHARACTER_REFERENCE(1413,
                          /* m a */ 'p' _ 's' _ 't' _ 'o' _ ';', 5, 0,
                          0x21a6 _ 0)
NAMED_CHARACTER_REFERENCE(
    1414,
    /* m a */ 'p' _ 's' _ 't' _ 'o' _ 'd' _ 'o' _ 'w' _ 'n' _ ';', 9, 0,
    0x21a7 _ 0)
NAMED_CHARACTER_REFERENCE(
    1415,
    /* m a */ 'p' _ 's' _ 't' _ 'o' _ 'l' _ 'e' _ 'f' _ 't' _ ';', 9, 0,
    0x21a4 _ 0)
NAMED_CHARACTER_REFERENCE(1416,
                          /* m a */ 'p' _ 's' _ 't' _ 'o' _ 'u' _ 'p' _ ';', 7,
                          0, 0x21a5 _ 0)
NAMED_CHARACTER_REFERENCE(1417,
                          /* m a */ 'r' _ 'k' _ 'e' _ 'r' _ ';', 5, 0,
                          0x25ae _ 0)
NAMED_CHARACTER_REFERENCE(1418,
                          /* m c */ 'o' _ 'm' _ 'm' _ 'a' _ ';', 5, 0,
                          0x2a29 _ 0)
NAMED_CHARACTER_REFERENCE(1419, /* m c */ 'y' _ ';', 2, 0, 0x043c _ 0)
NAMED_CHARACTER_REFERENCE(1420,
                          /* m d */ 'a' _ 's' _ 'h' _ ';', 4, 0, 0x2014 _ 0)
NAMED_CHARACTER_REFERENCE(
    1421,
    /* m e */
    'a' _ 's' _ 'u' _ 'r' _ 'e' _ 'd' _ 'a' _ 'n' _ 'g' _ 'l' _ 'e' _ ';', 12,
    0, 0x2221 _ 0)
NAMED_CHARACTER_REFERENCE(1422, /* m f */ 'r' _ ';', 2, 0, 0xd835 _ 0xdd2a)
NAMED_CHARACTER_REFERENCE(1423, /* m h */ 'o' _ ';', 2, 0, 0x2127 _ 0)
NAMED_CHARACTER_REFERENCE(1424, /* m i */ 'c' _ 'r' _ 'o', 3, 0, 0x00b5 _ 0)
NAMED_CHARACTER_REFERENCE(1425,
                          /* m i */ 'c' _ 'r' _ 'o' _ ';', 4, 0, 0x00b5 _ 0)
NAMED_CHARACTER_REFERENCE(1426, /* m i */ 'd' _ ';', 2, 0, 0x2223 _ 0)
NAMED_CHARACTER_REFERENCE(1427,
                          /* m i */ 'd' _ 'a' _ 's' _ 't' _ ';', 5, 0,
                          0x002a _ 0)
NAMED_CHARACTER_REFERENCE(1428,
                          /* m i */ 'd' _ 'c' _ 'i' _ 'r' _ ';', 5, 0,
                          0x2af0 _ 0)
NAMED_CHARACTER_REFERENCE(1429,
                          /* m i */ 'd' _ 'd' _ 'o' _ 't', 4, 0, 0x00b7 _ 0)
NAMED_CHARACTER_REFERENCE(1430,
                          /* m i */ 'd' _ 'd' _ 'o' _ 't' _ ';', 5, 0,
                          0x00b7 _ 0)
NAMED_CHARACTER_REFERENCE(1431,
                          /* m i */ 'n' _ 'u' _ 's' _ ';', 4, 0, 0x2212 _ 0)
NAMED_CHARACTER_REFERENCE(1432,
                          /* m i */ 'n' _ 'u' _ 's' _ 'b' _ ';', 5, 0,
                          0x229f _ 0)
NAMED_CHARACTER_REFERENCE(1433,
                          /* m i */ 'n' _ 'u' _ 's' _ 'd' _ ';', 5, 0,
                          0x2238 _ 0)
NAMED_CHARACTER_REFERENCE(1434,
                          /* m i */ 'n' _ 'u' _ 's' _ 'd' _ 'u' _ ';', 6, 0,
                          0x2a2a _ 0)
NAMED_CHARACTER_REFERENCE(1435, /* m l */ 'c' _ 'p' _ ';', 3, 0, 0x2adb _ 0)
NAMED_CHARACTER_REFERENCE(1436, /* m l */ 'd' _ 'r' _ ';', 3, 0, 0x2026 _ 0)
NAMED_CHARACTER_REFERENCE(1437,
                          /* m n */ 'p' _ 'l' _ 'u' _ 's' _ ';', 5, 0,
                          0x2213 _ 0)
NAMED_CHARACTER_REFERENCE(1438,
                          /* m o */ 'd' _ 'e' _ 'l' _ 's' _ ';', 5, 0,
                          0x22a7 _ 0)
NAMED_CHARACTER_REFERENCE(1439,
                          /* m o */ 'p' _ 'f' _ ';', 3, 0, 0xd835 _ 0xdd5e)
NAMED_CHARACTER_REFERENCE(1440, /* m p */ ';', 1, 0, 0x2213 _ 0)
NAMED_CHARACTER_REFERENCE(1441,
                          /* m s */ 'c' _ 'r' _ ';', 3, 0, 0xd835 _ 0xdcc2)
NAMED_CHARACTER_REFERENCE(1442,
                          /* m s */ 't' _ 'p' _ 'o' _ 's' _ ';', 5, 0,
                          0x223e _ 0)
NAMED_CHARACTER_REFERENCE(1443, /* m u */ ';', 1, 0, 0x03bc _ 0)
NAMED_CHARACTER_REFERENCE(1444,
                          /* m u */ 'l' _ 't' _ 'i' _ 'm' _ 'a' _ 'p' _ ';', 7,
                          0, 0x22b8 _ 0)
NAMED_CHARACTER_REFERENCE(1445,
                          /* m u */ 'm' _ 'a' _ 'p' _ ';', 4, 0, 0x22b8 _ 0)
NAMED_CHARACTER_REFERENCE(1446, /* n G */ 'g' _ ';', 2, 0, 0x22d9 _ 0x0338)
NAMED_CHARACTER_REFERENCE(1447, /* n G */ 't' _ ';', 2, 0, 0x226b _ 0x20d2)
NAMED_CHARACTER_REFERENCE(1448,
                          /* n G */ 't' _ 'v' _ ';', 3, 0, 0x226b _ 0x0338)
NAMED_CHARACTER_REFERENCE(
    1449,
    /* n L */ 'e' _ 'f' _ 't' _ 'a' _ 'r' _ 'r' _ 'o' _ 'w' _ ';', 9, 0,
    0x21cd _ 0)
NAMED_CHARACTER_REFERENCE(
    1450,
    /* n L */
    'e' _ 'f' _ 't' _ 'r' _ 'i' _ 'g' _ 'h' _ 't' _ 'a' _ 'r' _ 'r' _ 'o' _ 'w' _ ';',
    14, 0, 0x21ce _ 0)
NAMED_CHARACTER_REFERENCE(1451, /* n L */ 'l' _ ';', 2, 0, 0x22d8 _ 0x0338)
NAMED_CHARACTER_REFERENCE(1452, /* n L */ 't' _ ';', 2, 0, 0x226a _ 0x20d2)
NAMED_CHARACTER_REFERENCE(1453,
                          /* n L */ 't' _ 'v' _ ';', 3, 0, 0x226a _ 0x0338)
NAMED_CHARACTER_REFERENCE(
    1454,
    /* n R */ 'i' _ 'g' _ 'h' _ 't' _ 'a' _ 'r' _ 'r' _ 'o' _ 'w' _ ';', 10, 0,
    0x21cf _ 0)
NAMED_CHARACTER_REFERENCE(1455,
                          /* n V */ 'D' _ 'a' _ 's' _ 'h' _ ';', 5, 0,
                          0x22af _ 0)
NAMED_CHARACTER_REFERENCE(1456,
                          /* n V */ 'd' _ 'a' _ 's' _ 'h' _ ';', 5, 0,
                          0x22ae _ 0)
NAMED_CHARACTER_REFERENCE(1457,
                          /* n a */ 'b' _ 'l' _ 'a' _ ';', 4, 0, 0x2207 _ 0)
NAMED_CHARACTER_REFERENCE(1458,
                          /* n a */ 'c' _ 'u' _ 't' _ 'e' _ ';', 5, 0,
                          0x0144 _ 0)
NAMED_CHARACTER_REFERENCE(1459,
                          /* n a */ 'n' _ 'g' _ ';', 3, 0, 0x2220 _ 0x20d2)
NAMED_CHARACTER_REFERENCE(1460, /* n a */ 'p' _ ';', 2, 0, 0x2249 _ 0)
NAMED_CHARACTER_REFERENCE(1461,
                          /* n a */ 'p' _ 'E' _ ';', 3, 0, 0x2a70 _ 0x0338)
NAMED_CHARACTER_REFERENCE(1462,
                          /* n a */ 'p' _ 'i' _ 'd' _ ';', 4, 0,
                          0x224b _ 0x0338)
NAMED_CHARACTER_REFERENCE(1463,
                          /* n a */ 'p' _ 'o' _ 's' _ ';', 4, 0, 0x0149 _ 0)
NAMED_CHARACTER_REFERENCE(1464,
                          /* n a */ 'p' _ 'p' _ 'r' _ 'o' _ 'x' _ ';', 6, 0,
                          0x2249 _ 0)
NAMED_CHARACTER_REFERENCE(1465,
                          /* n a */ 't' _ 'u' _ 'r' _ ';', 4, 0, 0x266e _ 0)
NAMED_CHARACTER_REFERENCE(1466,
                          /* n a */ 't' _ 'u' _ 'r' _ 'a' _ 'l' _ ';', 6, 0,
                          0x266e _ 0)
NAMED_CHARACTER_REFERENCE(1467,
                          /* n a */ 't' _ 'u' _ 'r' _ 'a' _ 'l' _ 's' _ ';', 7,
                          0, 0x2115 _ 0)
NAMED_CHARACTER_REFERENCE(1468, /* n b */ 's' _ 'p', 2, 0, 0x00a0 _ 0)
NAMED_CHARACTER_REFERENCE(1469, /* n b */ 's' _ 'p' _ ';', 3, 0, 0x00a0 _ 0)
NAMED_CHARACTER_REFERENCE(1470,
                          /* n b */ 'u' _ 'm' _ 'p' _ ';', 4, 0,
                          0x224e _ 0x0338)
NAMED_CHARACTER_REFERENCE(1471,
                          /* n b */ 'u' _ 'm' _ 'p' _ 'e' _ ';', 5, 0,
                          0x224f _ 0x0338)
NAMED_CHARACTER_REFERENCE(1472, /* n c */ 'a' _ 'p' _ ';', 3, 0, 0x2a43 _ 0)
NAMED_CHARACTER_REFERENCE(1473,
                          /* n c */ 'a' _ 'r' _ 'o' _ 'n' _ ';', 5, 0,
                          0x0148 _ 0)
NAMED_CHARACTER_REFERENCE(1474,
                          /* n c */ 'e' _ 'd' _ 'i' _ 'l' _ ';', 5, 0,
                          0x0146 _ 0)
NAMED_CHARACTER_REFERENCE(1475,
                          /* n c */ 'o' _ 'n' _ 'g' _ ';', 4, 0, 0x2247 _ 0)
NAMED_CHARACTER_REFERENCE(1476,
                          /* n c */ 'o' _ 'n' _ 'g' _ 'd' _ 'o' _ 't' _ ';', 7,
                          0, 0x2a6d _ 0x0338)
NAMED_CHARACTER_REFERENCE(1477, /* n c */ 'u' _ 'p' _ ';', 3, 0, 0x2a42 _ 0)
NAMED_CHARACTER_REFERENCE(1478, /* n c */ 'y' _ ';', 2, 0, 0x043d _ 0)
NAMED_CHARACTER_REFERENCE(1479,
                          /* n d */ 'a' _ 's' _ 'h' _ ';', 4, 0, 0x2013 _ 0)
NAMED_CHARACTER_REFERENCE(1480, /* n e */ ';', 1, 0, 0x2260 _ 0)
NAMED_CHARACTER_REFERENCE(1481,
                          /* n e */ 'A' _ 'r' _ 'r' _ ';', 4, 0, 0x21d7 _ 0)
NAMED_CHARACTER_REFERENCE(1482,
                          /* n e */ 'a' _ 'r' _ 'h' _ 'k' _ ';', 5, 0,
                          0x2924 _ 0)
NAMED_CHARACTER_REFERENCE(1483,
                          /* n e */ 'a' _ 'r' _ 'r' _ ';', 4, 0, 0x2197 _ 0)
NAMED_CHARACTER_REFERENCE(1484,
                          /* n e */ 'a' _ 'r' _ 'r' _ 'o' _ 'w' _ ';', 6, 0,
                          0x2197 _ 0)
NAMED_CHARACTER_REFERENCE(1485,
                          /* n e */ 'd' _ 'o' _ 't' _ ';', 4, 0,
                          0x2250 _ 0x0338)
NAMED_CHARACTER_REFERENCE(1486,
                          /* n e */ 'q' _ 'u' _ 'i' _ 'v' _ ';', 5, 0,
                          0x2262 _ 0)
NAMED_CHARACTER_REFERENCE(1487,
                          /* n e */ 's' _ 'e' _ 'a' _ 'r' _ ';', 5, 0,
                          0x2928 _ 0)
NAMED_CHARACTER_REFERENCE(1488,
                          /* n e */ 's' _ 'i' _ 'm' _ ';', 4, 0,
                          0x2242 _ 0x0338)
NAMED_CHARACTER_REFERENCE(1489,
                          /* n e */ 'x' _ 'i' _ 's' _ 't' _ ';', 5, 0,
                          0x2204 _ 0)
NAMED_CHARACTER_REFERENCE(1490,
                          /* n e */ 'x' _ 'i' _ 's' _ 't' _ 's' _ ';', 6, 0,
                          0x2204 _ 0)
NAMED_CHARACTER_REFERENCE(1491, /* n f */ 'r' _ ';', 2, 0, 0xd835 _ 0xdd2b)
NAMED_CHARACTER_REFERENCE(1492, /* n g */ 'E' _ ';', 2, 0, 0x2267 _ 0x0338)
NAMED_CHARACTER_REFERENCE(1493, /* n g */ 'e' _ ';', 2, 0, 0x2271 _ 0)
NAMED_CHARACTER_REFERENCE(1494, /* n g */ 'e' _ 'q' _ ';', 3, 0, 0x2271 _ 0)
NAMED_CHARACTER_REFERENCE(1495,
                          /* n g */ 'e' _ 'q' _ 'q' _ ';', 4, 0,
                          0x2267 _ 0x0338)
NAMED_CHARACTER_REFERENCE(
    1496,
    /* n g */ 'e' _ 'q' _ 's' _ 'l' _ 'a' _ 'n' _ 't' _ ';', 8, 0,
    0x2a7e _ 0x0338)
NAMED_CHARACTER_REFERENCE(1497,
                          /* n g */ 'e' _ 's' _ ';', 3, 0, 0x2a7e _ 0x0338)
NAMED_CHARACTER_REFERENCE(1498,
                          /* n g */ 's' _ 'i' _ 'm' _ ';', 4, 0, 0x2275 _ 0)
NAMED_CHARACTER_REFERENCE(1499, /* n g */ 't' _ ';', 2, 0, 0x226f _ 0)
NAMED_CHARACTER_REFERENCE(1500, /* n g */ 't' _ 'r' _ ';', 3, 0, 0x226f _ 0)
NAMED_CHARACTER_REFERENCE(1501,
                          /* n h */ 'A' _ 'r' _ 'r' _ ';', 4, 0, 0x21ce _ 0)
NAMED_CHARACTER_REFERENCE(1502,
                          /* n h */ 'a' _ 'r' _ 'r' _ ';', 4, 0, 0x21ae _ 0)
NAMED_CHARACTER_REFERENCE(1503,
                          /* n h */ 'p' _ 'a' _ 'r' _ ';', 4, 0, 0x2af2 _ 0)
NAMED_CHARACTER_REFERENCE(1504, /* n i */ ';', 1, 0, 0x220b _ 0)
NAMED_CHARACTER_REFERENCE(1505, /* n i */ 's' _ ';', 2, 0, 0x22fc _ 0)
NAMED_CHARACTER_REFERENCE(1506, /* n i */ 's' _ 'd' _ ';', 3, 0, 0x22fa _ 0)
NAMED_CHARACTER_REFERENCE(1507, /* n i */ 'v' _ ';', 2, 0, 0x220b _ 0)
NAMED_CHARACTER_REFERENCE(1508, /* n j */ 'c' _ 'y' _ ';', 3, 0, 0x045a _ 0)
NAMED_CHARACTER_REFERENCE(1509,
                          /* n l */ 'A' _ 'r' _ 'r' _ ';', 4, 0, 0x21cd _ 0)
NAMED_CHARACTER_REFERENCE(1510, /* n l */ 'E' _ ';', 2, 0, 0x2266 _ 0x0338)
NAMED_CHARACTER_REFERENCE(1511,
                          /* n l */ 'a' _ 'r' _ 'r' _ ';', 4, 0, 0x219a _ 0)
NAMED_CHARACTER_REFERENCE(1512, /* n l */ 'd' _ 'r' _ ';', 3, 0, 0x2025 _ 0)
NAMED_CHARACTER_REFERENCE(1513, /* n l */ 'e' _ ';', 2, 0, 0x2270 _ 0)
NAMED_CHARACTER_REFERENCE(
    1514,
    /* n l */ 'e' _ 'f' _ 't' _ 'a' _ 'r' _ 'r' _ 'o' _ 'w' _ ';', 9, 0,
    0x219a _ 0)
NAMED_CHARACTER_REFERENCE(
    1515,
    /* n l */
    'e' _ 'f' _ 't' _ 'r' _ 'i' _ 'g' _ 'h' _ 't' _ 'a' _ 'r' _ 'r' _ 'o' _ 'w' _ ';',
    14, 0, 0x21ae _ 0)
NAMED_CHARACTER_REFERENCE(1516, /* n l */ 'e' _ 'q' _ ';', 3, 0, 0x2270 _ 0)
NAMED_CHARACTER_REFERENCE(1517,
                          /* n l */ 'e' _ 'q' _ 'q' _ ';', 4, 0,
                          0x2266 _ 0x0338)
NAMED_CHARACTER_REFERENCE(
    1518,
    /* n l */ 'e' _ 'q' _ 's' _ 'l' _ 'a' _ 'n' _ 't' _ ';', 8, 0,
    0x2a7d _ 0x0338)
NAMED_CHARACTER_REFERENCE(1519,
                          /* n l */ 'e' _ 's' _ ';', 3, 0, 0x2a7d _ 0x0338)
NAMED_CHARACTER_REFERENCE(1520,
                          /* n l */ 'e' _ 's' _ 's' _ ';', 4, 0, 0x226e _ 0)
NAMED_CHARACTER_REFERENCE(1521,
                          /* n l */ 's' _ 'i' _ 'm' _ ';', 4, 0, 0x2274 _ 0)
NAMED_CHARACTER_REFERENCE(1522, /* n l */ 't' _ ';', 2, 0, 0x226e _ 0)
NAMED_CHARACTER_REFERENCE(1523,
                          /* n l */ 't' _ 'r' _ 'i' _ ';', 4, 0, 0x22ea _ 0)
NAMED_CHARACTER_REFERENCE(1524,
                          /* n l */ 't' _ 'r' _ 'i' _ 'e' _ ';', 5, 0,
                          0x22ec _ 0)
NAMED_CHARACTER_REFERENCE(1525, /* n m */ 'i' _ 'd' _ ';', 3, 0, 0x2224 _ 0)
NAMED_CHARACTER_REFERENCE(1526,
                          /* n o */ 'p' _ 'f' _ ';', 3, 0, 0xd835 _ 0xdd5f)
NAMED_CHARACTER_REFERENCE(1527, /* n o */ 't', 1, 0, 0x00ac _ 0)
NAMED_CHARACTER_REFERENCE(1528, /* n o */ 't' _ ';', 2, 0, 0x00ac _ 0)
NAMED_CHARACTER_REFERENCE(1529,
                          /* n o */ 't' _ 'i' _ 'n' _ ';', 4, 0, 0x2209 _ 0)
NAMED_CHARACTER_REFERENCE(1530,
                          /* n o */ 't' _ 'i' _ 'n' _ 'E' _ ';', 5, 0,
                          0x22f9 _ 0x0338)
NAMED_CHARACTER_REFERENCE(1531,
                          /* n o */ 't' _ 'i' _ 'n' _ 'd' _ 'o' _ 't' _ ';', 7,
                          0, 0x22f5 _ 0x0338)
NAMED_CHARACTER_REFERENCE(1532,
                          /* n o */ 't' _ 'i' _ 'n' _ 'v' _ 'a' _ ';', 6, 0,
                          0x2209 _ 0)
NAMED_CHARACTER_REFERENCE(1533,
                          /* n o */ 't' _ 'i' _ 'n' _ 'v' _ 'b' _ ';', 6, 0,
                          0x22f7 _ 0)
NAMED_CHARACTER_REFERENCE(1534,
                          /* n o */ 't' _ 'i' _ 'n' _ 'v' _ 'c' _ ';', 6, 0,
                          0x22f6 _ 0)
NAMED_CHARACTER_REFERENCE(1535,
                          /* n o */ 't' _ 'n' _ 'i' _ ';', 4, 0, 0x220c _ 0)
NAMED_CHARACTER_REFERENCE(1536,
                          /* n o */ 't' _ 'n' _ 'i' _ 'v' _ 'a' _ ';', 6, 0,
                          0x220c _ 0)
NAMED_CHARACTER_REFERENCE(1537,
                          /* n o */ 't' _ 'n' _ 'i' _ 'v' _ 'b' _ ';', 6, 0,
                          0x22fe _ 0)
NAMED_CHARACTER_REFERENCE(1538,
                          /* n o */ 't' _ 'n' _ 'i' _ 'v' _ 'c' _ ';', 6, 0,
                          0x22fd _ 0)
NAMED_CHARACTER_REFERENCE(1539, /* n p */ 'a' _ 'r' _ ';', 3, 0, 0x2226 _ 0)
NAMED_CHARACTER_REFERENCE(
    1540,
    /* n p */ 'a' _ 'r' _ 'a' _ 'l' _ 'l' _ 'e' _ 'l' _ ';', 8, 0, 0x2226 _ 0)
NAMED_CHARACTER_REFERENCE(1541,
                          /* n p */ 'a' _ 'r' _ 's' _ 'l' _ ';', 5, 0,
                          0x2afd _ 0x20e5)
NAMED_CHARACTER_REFERENCE(1542,
                          /* n p */ 'a' _ 'r' _ 't' _ ';', 4, 0,
                          0x2202 _ 0x0338)
NAMED_CHARACTER_REFERENCE(1543,
                          /* n p */ 'o' _ 'l' _ 'i' _ 'n' _ 't' _ ';', 6, 0,
                          0x2a14 _ 0)
NAMED_CHARACTER_REFERENCE(1544, /* n p */ 'r' _ ';', 2, 0, 0x2280 _ 0)
NAMED_CHARACTER_REFERENCE(1545,
                          /* n p */ 'r' _ 'c' _ 'u' _ 'e' _ ';', 5, 0,
                          0x22e0 _ 0)
NAMED_CHARACTER_REFERENCE(1546,
                          /* n p */ 'r' _ 'e' _ ';', 3, 0, 0x2aaf _ 0x0338)
NAMED_CHARACTER_REFERENCE(1547,
                          /* n p */ 'r' _ 'e' _ 'c' _ ';', 4, 0, 0x2280 _ 0)
NAMED_CHARACTER_REFERENCE(1548,
                          /* n p */ 'r' _ 'e' _ 'c' _ 'e' _ 'q' _ ';', 6, 0,
                          0x2aaf _ 0x0338)
NAMED_CHARACTER_REFERENCE(1549,
                          /* n r */ 'A' _ 'r' _ 'r' _ ';', 4, 0, 0x21cf _ 0)
NAMED_CHARACTER_REFERENCE(1550,
                          /* n r */ 'a' _ 'r' _ 'r' _ ';', 4, 0, 0x219b _ 0)
NAMED_CHARACTER_REFERENCE(1551,
                          /* n r */ 'a' _ 'r' _ 'r' _ 'c' _ ';', 5, 0,
                          0x2933 _ 0x0338)
NAMED_CHARACTER_REFERENCE(1552,
                          /* n r */ 'a' _ 'r' _ 'r' _ 'w' _ ';', 5, 0,
                          0x219d _ 0x0338)
NAMED_CHARACTER_REFERENCE(
    1553,
    /* n r */ 'i' _ 'g' _ 'h' _ 't' _ 'a' _ 'r' _ 'r' _ 'o' _ 'w' _ ';', 10, 0,
    0x219b _ 0)
NAMED_CHARACTER_REFERENCE(1554,
                          /* n r */ 't' _ 'r' _ 'i' _ ';', 4, 0, 0x22eb _ 0)
NAMED_CHARACTER_REFERENCE(1555,
                          /* n r */ 't' _ 'r' _ 'i' _ 'e' _ ';', 5, 0,
                          0x22ed _ 0)
NAMED_CHARACTER_REFERENCE(1556, /* n s */ 'c' _ ';', 2, 0, 0x2281 _ 0)
NAMED_CHARACTER_REFERENCE(1557,
                          /* n s */ 'c' _ 'c' _ 'u' _ 'e' _ ';', 5, 0,
                          0x22e1 _ 0)
NAMED_CHARACTER_REFERENCE(1558,
                          /* n s */ 'c' _ 'e' _ ';', 3, 0, 0x2ab0 _ 0x0338)
NAMED_CHARACTER_REFERENCE(1559,
                          /* n s */ 'c' _ 'r' _ ';', 3, 0, 0xd835 _ 0xdcc3)
NAMED_CHARACTER_REFERENCE(
    1560,
    /* n s */ 'h' _ 'o' _ 'r' _ 't' _ 'm' _ 'i' _ 'd' _ ';', 8, 0, 0x2224 _ 0)
NAMED_CHARACTER_REFERENCE(
    1561,
    /* n s */
    'h' _ 'o' _ 'r' _ 't' _ 'p' _ 'a' _ 'r' _ 'a' _ 'l' _ 'l' _ 'e' _ 'l' _ ';',
    13, 0, 0x2226 _ 0)
NAMED_CHARACTER_REFERENCE(1562, /* n s */ 'i' _ 'm' _ ';', 3, 0, 0x2241 _ 0)
NAMED_CHARACTER_REFERENCE(1563,
                          /* n s */ 'i' _ 'm' _ 'e' _ ';', 4, 0, 0x2244 _ 0)
NAMED_CHARACTER_REFERENCE(1564,
                          /* n s */ 'i' _ 'm' _ 'e' _ 'q' _ ';', 5, 0,
                          0x2244 _ 0)
NAMED_CHARACTER_REFERENCE(1565,
                          /* n s */ 'm' _ 'i' _ 'd' _ ';', 4, 0, 0x2224 _ 0)
NAMED_CHARACTER_REFERENCE(1566,
                          /* n s */ 'p' _ 'a' _ 'r' _ ';', 4, 0, 0x2226 _ 0)
NAMED_CHARACTER_REFERENCE(1567,
                          /* n s */ 'q' _ 's' _ 'u' _ 'b' _ 'e' _ ';', 6, 0,
                          0x22e2 _ 0)
NAMED_CHARACTER_REFERENCE(1568,
                          /* n s */ 'q' _ 's' _ 'u' _ 'p' _ 'e' _ ';', 6, 0,
                          0x22e3 _ 0)
NAMED_CHARACTER_REFERENCE(1569, /* n s */ 'u' _ 'b' _ ';', 3, 0, 0x2284 _ 0)
NAMED_CHARACTER_REFERENCE(1570,
                          /* n s */ 'u' _ 'b' _ 'E' _ ';', 4, 0,
                          0x2ac5 _ 0x0338)
NAMED_CHARACTER_REFERENCE(1571,
                          /* n s */ 'u' _ 'b' _ 'e' _ ';', 4, 0, 0x2288 _ 0)
NAMED_CHARACTER_REFERENCE(1572,
                          /* n s */ 'u' _ 'b' _ 's' _ 'e' _ 't' _ ';', 6, 0,
                          0x2282 _ 0x20d2)
NAMED_CHARACTER_REFERENCE(
    1573,
    /* n s */ 'u' _ 'b' _ 's' _ 'e' _ 't' _ 'e' _ 'q' _ ';', 8, 0, 0x2288 _ 0)
NAMED_CHARACTER_REFERENCE(
    1574,
    /* n s */ 'u' _ 'b' _ 's' _ 'e' _ 't' _ 'e' _ 'q' _ 'q' _ ';', 9, 0,
    0x2ac5 _ 0x0338)
NAMED_CHARACTER_REFERENCE(1575,
                          /* n s */ 'u' _ 'c' _ 'c' _ ';', 4, 0, 0x2281 _ 0)
NAMED_CHARACTER_REFERENCE(1576,
                          /* n s */ 'u' _ 'c' _ 'c' _ 'e' _ 'q' _ ';', 6, 0,
                          0x2ab0 _ 0x0338)
NAMED_CHARACTER_REFERENCE(1577, /* n s */ 'u' _ 'p' _ ';', 3, 0, 0x2285 _ 0)
NAMED_CHARACTER_REFERENCE(1578,
                          /* n s */ 'u' _ 'p' _ 'E' _ ';', 4, 0,
                          0x2ac6 _ 0x0338)
NAMED_CHARACTER_REFERENCE(1579,
                          /* n s */ 'u' _ 'p' _ 'e' _ ';', 4, 0, 0x2289 _ 0)
NAMED_CHARACTER_REFERENCE(1580,
                          /* n s */ 'u' _ 'p' _ 's' _ 'e' _ 't' _ ';', 6, 0,
                          0x2283 _ 0x20d2)
NAMED_CHARACTER_REFERENCE(
    1581,
    /* n s */ 'u' _ 'p' _ 's' _ 'e' _ 't' _ 'e' _ 'q' _ ';', 8, 0, 0x2289 _ 0)
NAMED_CHARACTER_REFERENCE(
    1582,
    /* n s */ 'u' _ 'p' _ 's' _ 'e' _ 't' _ 'e' _ 'q' _ 'q' _ ';', 9, 0,
    0x2ac6 _ 0x0338)
NAMED_CHARACTER_REFERENCE(1583, /* n t */ 'g' _ 'l' _ ';', 3, 0, 0x2279 _ 0)
NAMED_CHARACTER_REFERENCE(1584,
                          /* n t */ 'i' _ 'l' _ 'd' _ 'e', 4, 0, 0x00f1 _ 0)
NAMED_CHARACTER_REFERENCE(1585,
                          /* n t */ 'i' _ 'l' _ 'd' _ 'e' _ ';', 5, 0,
                          0x00f1 _ 0)
NAMED_CHARACTER_REFERENCE(1586, /* n t */ 'l' _ 'g' _ ';', 3, 0, 0x2278 _ 0)
NAMED_CHARACTER_REFERENCE(
    1587,
    /* n t */
    'r' _ 'i' _ 'a' _ 'n' _ 'g' _ 'l' _ 'e' _ 'l' _ 'e' _ 'f' _ 't' _ ';', 12,
    0, 0x22ea _ 0)
NAMED_CHARACTER_REFERENCE(
    1588,
    /* n t */
    'r' _ 'i' _ 'a' _ 'n' _ 'g' _ 'l' _ 'e' _ 'l' _ 'e' _ 'f' _ 't' _ 'e' _ 'q' _ ';',
    14, 0, 0x22ec _ 0)
NAMED_CHARACTER_REFERENCE(
    1589,
    /* n t */
    'r' _ 'i' _ 'a' _ 'n' _ 'g' _ 'l' _ 'e' _ 'r' _ 'i' _ 'g' _ 'h' _ 't' _ ';',
    13, 0, 0x22eb _ 0)
NAMED_CHARACTER_REFERENCE(
    1590,
    /* n t */
    'r' _ 'i' _ 'a' _ 'n' _ 'g' _ 'l' _ 'e' _ 'r' _ 'i' _ 'g' _ 'h' _ 't' _ 'e' _ 'q' _ ';',
    15, 0, 0x22ed _ 0)
NAMED_CHARACTER_REFERENCE(1591, /* n u */ ';', 1, 0, 0x03bd _ 0)
NAMED_CHARACTER_REFERENCE(1592, /* n u */ 'm' _ ';', 2, 0, 0x0023 _ 0)
NAMED_CHARACTER_REFERENCE(1593,
                          /* n u */ 'm' _ 'e' _ 'r' _ 'o' _ ';', 5, 0,
                          0x2116 _ 0)
NAMED_CHARACTER_REFERENCE(1594,
                          /* n u */ 'm' _ 's' _ 'p' _ ';', 4, 0, 0x2007 _ 0)
NAMED_CHARACTER_REFERENCE(1595,
                          /* n v */ 'D' _ 'a' _ 's' _ 'h' _ ';', 5, 0,
                          0x22ad _ 0)
NAMED_CHARACTER_REFERENCE(1596,
                          /* n v */ 'H' _ 'a' _ 'r' _ 'r' _ ';', 5, 0,
                          0x2904 _ 0)
NAMED_CHARACTER_REFERENCE(1597,
                          /* n v */ 'a' _ 'p' _ ';', 3, 0, 0x224d _ 0x20d2)
NAMED_CHARACTER_REFERENCE(1598,
                          /* n v */ 'd' _ 'a' _ 's' _ 'h' _ ';', 5, 0,
                          0x22ac _ 0)
NAMED_CHARACTER_REFERENCE(1599,
                          /* n v */ 'g' _ 'e' _ ';', 3, 0, 0x2265 _ 0x20d2)
NAMED_CHARACTER_REFERENCE(1600,
                          /* n v */ 'g' _ 't' _ ';', 3, 0, 0x003e _ 0x20d2)
NAMED_CHARACTER_REFERENCE(1601,
                          /* n v */ 'i' _ 'n' _ 'f' _ 'i' _ 'n' _ ';', 6, 0,
                          0x29de _ 0)
NAMED_CHARACTER_REFERENCE(1602,
                          /* n v */ 'l' _ 'A' _ 'r' _ 'r' _ ';', 5, 0,
                          0x2902 _ 0)
NAMED_CHARACTER_REFERENCE(1603,
                          /* n v */ 'l' _ 'e' _ ';', 3, 0, 0x2264 _ 0x20d2)
NAMED_CHARACTER_REFERENCE(1604,
                          /* n v */ 'l' _ 't' _ ';', 3, 0, 0x003c _ 0x20d2)
NAMED_CHARACTER_REFERENCE(1605,
                          /* n v */ 'l' _ 't' _ 'r' _ 'i' _ 'e' _ ';', 6, 0,
                          0x22b4 _ 0x20d2)
NAMED_CHARACTER_REFERENCE(1606,
                          /* n v */ 'r' _ 'A' _ 'r' _ 'r' _ ';', 5, 0,
                          0x2903 _ 0)
NAMED_CHARACTER_REFERENCE(1607,
                          /* n v */ 'r' _ 't' _ 'r' _ 'i' _ 'e' _ ';', 6, 0,
                          0x22b5 _ 0x20d2)
NAMED_CHARACTER_REFERENCE(1608,
                          /* n v */ 's' _ 'i' _ 'm' _ ';', 4, 0,
                          0x223c _ 0x20d2)
NAMED_CHARACTER_REFERENCE(1609,
                          /* n w */ 'A' _ 'r' _ 'r' _ ';', 4, 0, 0x21d6 _ 0)
NAMED_CHARACTER_REFERENCE(1610,
                          /* n w */ 'a' _ 'r' _ 'h' _ 'k' _ ';', 5, 0,
                          0x2923 _ 0)
NAMED_CHARACTER_REFERENCE(1611,
                          /* n w */ 'a' _ 'r' _ 'r' _ ';', 4, 0, 0x2196 _ 0)
NAMED_CHARACTER_REFERENCE(1612,
                          /* n w */ 'a' _ 'r' _ 'r' _ 'o' _ 'w' _ ';', 6, 0,
                          0x2196 _ 0)
NAMED_CHARACTER_REFERENCE(1613,
                          /* n w */ 'n' _ 'e' _ 'a' _ 'r' _ ';', 5, 0,
                          0x2927 _ 0)
NAMED_CHARACTER_REFERENCE(1614, /* o S */ ';', 1, 0, 0x24c8 _ 0)
NAMED_CHARACTER_REFERENCE(1615,
                          /* o a */ 'c' _ 'u' _ 't' _ 'e', 4, 0, 0x00f3 _ 0)
NAMED_CHARACTER_REFERENCE(1616,
                          /* o a */ 'c' _ 'u' _ 't' _ 'e' _ ';', 5, 0,
                          0x00f3 _ 0)
NAMED_CHARACTER_REFERENCE(1617, /* o a */ 's' _ 't' _ ';', 3, 0, 0x229b _ 0)
NAMED_CHARACTER_REFERENCE(1618, /* o c */ 'i' _ 'r' _ ';', 3, 0, 0x229a _ 0)
NAMED_CHARACTER_REFERENCE(1619, /* o c */ 'i' _ 'r' _ 'c', 3, 0, 0x00f4 _ 0)
NAMED_CHARACTER_REFERENCE(1620,
                          /* o c */ 'i' _ 'r' _ 'c' _ ';', 4, 0, 0x00f4 _ 0)
NAMED_CHARACTER_REFERENCE(1621, /* o c */ 'y' _ ';', 2, 0, 0x043e _ 0)
NAMED_CHARACTER_REFERENCE(1622,
                          /* o d */ 'a' _ 's' _ 'h' _ ';', 4, 0, 0x229d _ 0)
NAMED_CHARACTER_REFERENCE(1623,
                          /* o d */ 'b' _ 'l' _ 'a' _ 'c' _ ';', 5, 0,
                          0x0151 _ 0)
NAMED_CHARACTER_REFERENCE(1624, /* o d */ 'i' _ 'v' _ ';', 3, 0, 0x2a38 _ 0)
NAMED_CHARACTER_REFERENCE(1625, /* o d */ 'o' _ 't' _ ';', 3, 0, 0x2299 _ 0)
NAMED_CHARACTER_REFERENCE(1626,
                          /* o d */ 's' _ 'o' _ 'l' _ 'd' _ ';', 5, 0,
                          0x29bc _ 0)
NAMED_CHARACTER_REFERENCE(1627,
                          /* o e */ 'l' _ 'i' _ 'g' _ ';', 4, 0, 0x0153 _ 0)
NAMED_CHARACTER_REFERENCE(1628,
                          /* o f */ 'c' _ 'i' _ 'r' _ ';', 4, 0, 0x29bf _ 0)
NAMED_CHARACTER_REFERENCE(1629, /* o f */ 'r' _ ';', 2, 0, 0xd835 _ 0xdd2c)
NAMED_CHARACTER_REFERENCE(1630, /* o g */ 'o' _ 'n' _ ';', 3, 0, 0x02db _ 0)
NAMED_CHARACTER_REFERENCE(1631,
                          /* o g */ 'r' _ 'a' _ 'v' _ 'e', 4, 0, 0x00f2 _ 0)
NAMED_CHARACTER_REFERENCE(1632,
                          /* o g */ 'r' _ 'a' _ 'v' _ 'e' _ ';', 5, 0,
                          0x00f2 _ 0)
NAMED_CHARACTER_REFERENCE(1633, /* o g */ 't' _ ';', 2, 0, 0x29c1 _ 0)
NAMED_CHARACTER_REFERENCE(1634,
                          /* o h */ 'b' _ 'a' _ 'r' _ ';', 4, 0, 0x29b5 _ 0)
NAMED_CHARACTER_REFERENCE(1635, /* o h */ 'm' _ ';', 2, 0, 0x03a9 _ 0)
NAMED_CHARACTER_REFERENCE(1636, /* o i */ 'n' _ 't' _ ';', 3, 0, 0x222e _ 0)
NAMED_CHARACTER_REFERENCE(1637,
                          /* o l */ 'a' _ 'r' _ 'r' _ ';', 4, 0, 0x21ba _ 0)
NAMED_CHARACTER_REFERENCE(1638,
                          /* o l */ 'c' _ 'i' _ 'r' _ ';', 4, 0, 0x29be _ 0)
NAMED_CHARACTER_REFERENCE(1639,
                          /* o l */ 'c' _ 'r' _ 'o' _ 's' _ 's' _ ';', 6, 0,
                          0x29bb _ 0)
NAMED_CHARACTER_REFERENCE(1640,
                          /* o l */ 'i' _ 'n' _ 'e' _ ';', 4, 0, 0x203e _ 0)
NAMED_CHARACTER_REFERENCE(1641, /* o l */ 't' _ ';', 2, 0, 0x29c0 _ 0)
NAMED_CHARACTER_REFERENCE(1642,
                          /* o m */ 'a' _ 'c' _ 'r' _ ';', 4, 0, 0x014d _ 0)
NAMED_CHARACTER_REFERENCE(1643,
                          /* o m */ 'e' _ 'g' _ 'a' _ ';', 4, 0, 0x03c9 _ 0)
NAMED_CHARACTER_REFERENCE(1644,
                          /* o m */ 'i' _ 'c' _ 'r' _ 'o' _ 'n' _ ';', 6, 0,
                          0x03bf _ 0)
NAMED_CHARACTER_REFERENCE(1645, /* o m */ 'i' _ 'd' _ ';', 3, 0, 0x29b6 _ 0)
NAMED_CHARACTER_REFERENCE(1646,
                          /* o m */ 'i' _ 'n' _ 'u' _ 's' _ ';', 5, 0,
                          0x2296 _ 0)
NAMED_CHARACTER_REFERENCE(1647,
                          /* o o */ 'p' _ 'f' _ ';', 3, 0, 0xd835 _ 0xdd60)
NAMED_CHARACTER_REFERENCE(1648, /* o p */ 'a' _ 'r' _ ';', 3, 0, 0x29b7 _ 0)
NAMED_CHARACTER_REFERENCE(1649,
                          /* o p */ 'e' _ 'r' _ 'p' _ ';', 4, 0, 0x29b9 _ 0)
NAMED_CHARACTER_REFERENCE(1650,
                          /* o p */ 'l' _ 'u' _ 's' _ ';', 4, 0, 0x2295 _ 0)
NAMED_CHARACTER_REFERENCE(1651, /* o r */ ';', 1, 0, 0x2228 _ 0)
NAMED_CHARACTER_REFERENCE(1652,
                          /* o r */ 'a' _ 'r' _ 'r' _ ';', 4, 0, 0x21bb _ 0)
NAMED_CHARACTER_REFERENCE(1653, /* o r */ 'd' _ ';', 2, 0, 0x2a5d _ 0)
NAMED_CHARACTER_REFERENCE(1654,
                          /* o r */ 'd' _ 'e' _ 'r' _ ';', 4, 0, 0x2134 _ 0)
NAMED_CHARACTER_REFERENCE(1655,
                          /* o r */ 'd' _ 'e' _ 'r' _ 'o' _ 'f' _ ';', 6, 0,
                          0x2134 _ 0)
NAMED_CHARACTER_REFERENCE(1656, /* o r */ 'd' _ 'f', 2, 0, 0x00aa _ 0)
NAMED_CHARACTER_REFERENCE(1657, /* o r */ 'd' _ 'f' _ ';', 3, 0, 0x00aa _ 0)
NAMED_CHARACTER_REFERENCE(1658, /* o r */ 'd' _ 'm', 2, 0, 0x00ba _ 0)
NAMED_CHARACTER_REFERENCE(1659, /* o r */ 'd' _ 'm' _ ';', 3, 0, 0x00ba _ 0)
NAMED_CHARACTER_REFERENCE(1660,
                          /* o r */ 'i' _ 'g' _ 'o' _ 'f' _ ';', 5, 0,
                          0x22b6 _ 0)
NAMED_CHARACTER_REFERENCE(1661, /* o r */ 'o' _ 'r' _ ';', 3, 0, 0x2a56 _ 0)
NAMED_CHARACTER_REFERENCE(1662,
                          /* o r */ 's' _ 'l' _ 'o' _ 'p' _ 'e' _ ';', 6, 0,
                          0x2a57 _ 0)
NAMED_CHARACTER_REFERENCE(1663, /* o r */ 'v' _ ';', 2, 0, 0x2a5b _ 0)
NAMED_CHARACTER_REFERENCE(1664, /* o s */ 'c' _ 'r' _ ';', 3, 0, 0x2134 _ 0)
NAMED_CHARACTER_REFERENCE(1665,
                          /* o s */ 'l' _ 'a' _ 's' _ 'h', 4, 0, 0x00f8 _ 0)
NAMED_CHARACTER_REFERENCE(1666,
                          /* o s */ 'l' _ 'a' _ 's' _ 'h' _ ';', 5, 0,
                          0x00f8 _ 0)
NAMED_CHARACTER_REFERENCE(1667, /* o s */ 'o' _ 'l' _ ';', 3, 0, 0x2298 _ 0)
NAMED_CHARACTER_REFERENCE(1668,
                          /* o t */ 'i' _ 'l' _ 'd' _ 'e', 4, 0, 0x00f5 _ 0)
NAMED_CHARACTER_REFERENCE(1669,
                          /* o t */ 'i' _ 'l' _ 'd' _ 'e' _ ';', 5, 0,
                          0x00f5 _ 0)
NAMED_CHARACTER_REFERENCE(1670,
                          /* o t */ 'i' _ 'm' _ 'e' _ 's' _ ';', 5, 0,
                          0x2297 _ 0)
NAMED_CHARACTER_REFERENCE(1671,
                          /* o t */ 'i' _ 'm' _ 'e' _ 's' _ 'a' _ 's' _ ';', 7,
                          0, 0x2a36 _ 0)
NAMED_CHARACTER_REFERENCE(1672, /* o u */ 'm' _ 'l', 2, 0, 0x00f6 _ 0)
NAMED_CHARACTER_REFERENCE(1673, /* o u */ 'm' _ 'l' _ ';', 3, 0, 0x00f6 _ 0)
NAMED_CHARACTER_REFERENCE(1674,
                          /* o v */ 'b' _ 'a' _ 'r' _ ';', 4, 0, 0x233d _ 0)
NAMED_CHARACTER_REFERENCE(1675, /* p a */ 'r' _ ';', 2, 0, 0x2225 _ 0)
NAMED_CHARACTER_REFERENCE(1676, /* p a */ 'r' _ 'a', 2, 0, 0x00b6 _ 0)
NAMED_CHARACTER_REFERENCE(1677, /* p a */ 'r' _ 'a' _ ';', 3, 0, 0x00b6 _ 0)
NAMED_CHARACTER_REFERENCE(1678,
                          /* p a */ 'r' _ 'a' _ 'l' _ 'l' _ 'e' _ 'l' _ ';', 7,
                          0, 0x2225 _ 0)
NAMED_CHARACTER_REFERENCE(1679,
                          /* p a */ 'r' _ 's' _ 'i' _ 'm' _ ';', 5, 0,
                          0x2af3 _ 0)
NAMED_CHARACTER_REFERENCE(1680,
                          /* p a */ 'r' _ 's' _ 'l' _ ';', 4, 0, 0x2afd _ 0)
NAMED_CHARACTER_REFERENCE(1681, /* p a */ 'r' _ 't' _ ';', 3, 0, 0x2202 _ 0)
NAMED_CHARACTER_REFERENCE(1682, /* p c */ 'y' _ ';', 2, 0, 0x043f _ 0)
NAMED_CHARACTER_REFERENCE(1683,
                          /* p e */ 'r' _ 'c' _ 'n' _ 't' _ ';', 5, 0,
                          0x0025 _ 0)
NAMED_CHARACTER_REFERENCE(1684,
                          /* p e */ 'r' _ 'i' _ 'o' _ 'd' _ ';', 5, 0,
                          0x002e _ 0)
NAMED_CHARACTER_REFERENCE(1685,
                          /* p e */ 'r' _ 'm' _ 'i' _ 'l' _ ';', 5, 0,
                          0x2030 _ 0)
NAMED_CHARACTER_REFERENCE(1686, /* p e */ 'r' _ 'p' _ ';', 3, 0, 0x22a5 _ 0)
NAMED_CHARACTER_REFERENCE(1687,
                          /* p e */ 'r' _ 't' _ 'e' _ 'n' _ 'k' _ ';', 6, 0,
                          0x2031 _ 0)
NAMED_CHARACTER_REFERENCE(1688, /* p f */ 'r' _ ';', 2, 0, 0xd835 _ 0xdd2d)
NAMED_CHARACTER_REFERENCE(1689, /* p h */ 'i' _ ';', 2, 0, 0x03c6 _ 0)
NAMED_CHARACTER_REFERENCE(1690, /* p h */ 'i' _ 'v' _ ';', 3, 0, 0x03d5 _ 0)
NAMED_CHARACTER_REFERENCE(1691,
                          /* p h */ 'm' _ 'm' _ 'a' _ 't' _ ';', 5, 0,
                          0x2133 _ 0)
NAMED_CHARACTER_REFERENCE(1692,
                          /* p h */ 'o' _ 'n' _ 'e' _ ';', 4, 0, 0x260e _ 0)
NAMED_CHARACTER_REFERENCE(1693, /* p i */ ';', 1, 0, 0x03c0 _ 0)
NAMED_CHARACTER_REFERENCE(
    1694,
    /* p i */ 't' _ 'c' _ 'h' _ 'f' _ 'o' _ 'r' _ 'k' _ ';', 8, 0, 0x22d4 _ 0)
NAMED_CHARACTER_REFERENCE(1695, /* p i */ 'v' _ ';', 2, 0, 0x03d6 _ 0)
NAMED_CHARACTER_REFERENCE(1696,
                          /* p l */ 'a' _ 'n' _ 'c' _ 'k' _ ';', 5, 0,
                          0x210f _ 0)
NAMED_CHARACTER_REFERENCE(1697,
                          /* p l */ 'a' _ 'n' _ 'c' _ 'k' _ 'h' _ ';', 6, 0,
                          0x210e _ 0)
NAMED_CHARACTER_REFERENCE(1698,
                          /* p l */ 'a' _ 'n' _ 'k' _ 'v' _ ';', 5, 0,
                          0x210f _ 0)
NAMED_CHARACTER_REFERENCE(1699, /* p l */ 'u' _ 's' _ ';', 3, 0, 0x002b _ 0)
NAMED_CHARACTER_REFERENCE(1700,
                          /* p l */ 'u' _ 's' _ 'a' _ 'c' _ 'i' _ 'r' _ ';', 7,
                          0, 0x2a23 _ 0)
NAMED_CHARACTER_REFERENCE(1701,
                          /* p l */ 'u' _ 's' _ 'b' _ ';', 4, 0, 0x229e _ 0)
NAMED_CHARACTER_REFERENCE(1702,
                          /* p l */ 'u' _ 's' _ 'c' _ 'i' _ 'r' _ ';', 6, 0,
                          0x2a22 _ 0)
NAMED_CHARACTER_REFERENCE(1703,
                          /* p l */ 'u' _ 's' _ 'd' _ 'o' _ ';', 5, 0,
                          0x2214 _ 0)
NAMED_CHARACTER_REFERENCE(1704,
                          /* p l */ 'u' _ 's' _ 'd' _ 'u' _ ';', 5, 0,
                          0x2a25 _ 0)
NAMED_CHARACTER_REFERENCE(1705,
                          /* p l */ 'u' _ 's' _ 'e' _ ';', 4, 0, 0x2a72 _ 0)
NAMED_CHARACTER_REFERENCE(1706,
                          /* p l */ 'u' _ 's' _ 'm' _ 'n', 4, 0, 0x00b1 _ 0)
NAMED_CHARACTER_REFERENCE(1707,
                          /* p l */ 'u' _ 's' _ 'm' _ 'n' _ ';', 5, 0,
                          0x00b1 _ 0)
NAMED_CHARACTER_REFERENCE(1708,
                          /* p l */ 'u' _ 's' _ 's' _ 'i' _ 'm' _ ';', 6, 0,
                          0x2a26 _ 0)
NAMED_CHARACTER_REFERENCE(1709,
                          /* p l */ 'u' _ 's' _ 't' _ 'w' _ 'o' _ ';', 6, 0,
                          0x2a27 _ 0)
NAMED_CHARACTER_REFERENCE(1710, /* p m */ ';', 1, 0, 0x00b1 _ 0)
NAMED_CHARACTER_REFERENCE(1711,
                          /* p o */ 'i' _ 'n' _ 't' _ 'i' _ 'n' _ 't' _ ';', 7,
                          0, 0x2a15 _ 0)
NAMED_CHARACTER_REFERENCE(1712,
                          /* p o */ 'p' _ 'f' _ ';', 3, 0, 0xd835 _ 0xdd61)
NAMED_CHARACTER_REFERENCE(1713, /* p o */ 'u' _ 'n' _ 'd', 3, 0, 0x00a3 _ 0)
NAMED_CHARACTER_REFERENCE(1714,
                          /* p o */ 'u' _ 'n' _ 'd' _ ';', 4, 0, 0x00a3 _ 0)
NAMED_CHARACTER_REFERENCE(1715, /* p r */ ';', 1, 0, 0x227a _ 0)
NAMED_CHARACTER_REFERENCE(1716, /* p r */ 'E' _ ';', 2, 0, 0x2ab3 _ 0)
NAMED_CHARACTER_REFERENCE(1717, /* p r */ 'a' _ 'p' _ ';', 3, 0, 0x2ab7 _ 0)
NAMED_CHARACTER_REFERENCE(1718,
                          /* p r */ 'c' _ 'u' _ 'e' _ ';', 4, 0, 0x227c _ 0)
NAMED_CHARACTER_REFERENCE(1719, /* p r */ 'e' _ ';', 2, 0, 0x2aaf _ 0)
NAMED_CHARACTER_REFERENCE(1720, /* p r */ 'e' _ 'c' _ ';', 3, 0, 0x227a _ 0)
NAMED_CHARACTER_REFERENCE(
    1721,
    /* p r */ 'e' _ 'c' _ 'a' _ 'p' _ 'p' _ 'r' _ 'o' _ 'x' _ ';', 9, 0,
    0x2ab7 _ 0)
NAMED_CHARACTER_REFERENCE(
    1722,
    /* p r */ 'e' _ 'c' _ 'c' _ 'u' _ 'r' _ 'l' _ 'y' _ 'e' _ 'q' _ ';', 10, 0,
    0x227c _ 0)
NAMED_CHARACTER_REFERENCE(1723,
                          /* p r */ 'e' _ 'c' _ 'e' _ 'q' _ ';', 5, 0,
                          0x2aaf _ 0)
NAMED_CHARACTER_REFERENCE(
    1724,
    /* p r */ 'e' _ 'c' _ 'n' _ 'a' _ 'p' _ 'p' _ 'r' _ 'o' _ 'x' _ ';', 10, 0,
    0x2ab9 _ 0)
NAMED_CHARACTER_REFERENCE(1725,
                          /* p r */ 'e' _ 'c' _ 'n' _ 'e' _ 'q' _ 'q' _ ';', 7,
                          0, 0x2ab5 _ 0)
NAMED_CHARACTER_REFERENCE(1726,
                          /* p r */ 'e' _ 'c' _ 'n' _ 's' _ 'i' _ 'm' _ ';', 7,
                          0, 0x22e8 _ 0)
NAMED_CHARACTER_REFERENCE(1727,
                          /* p r */ 'e' _ 'c' _ 's' _ 'i' _ 'm' _ ';', 6, 0,
                          0x227e _ 0)
NAMED_CHARACTER_REFERENCE(1728,
                          /* p r */ 'i' _ 'm' _ 'e' _ ';', 4, 0, 0x2032 _ 0)
NAMED_CHARACTER_REFERENCE(1729,
                          /* p r */ 'i' _ 'm' _ 'e' _ 's' _ ';', 5, 0,
                          0x2119 _ 0)
NAMED_CHARACTER_REFERENCE(1730, /* p r */ 'n' _ 'E' _ ';', 3, 0, 0x2ab5 _ 0)
NAMED_CHARACTER_REFERENCE(1731,
                          /* p r */ 'n' _ 'a' _ 'p' _ ';', 4, 0, 0x2ab9 _ 0)
NAMED_CHARACTER_REFERENCE(1732,
                          /* p r */ 'n' _ 's' _ 'i' _ 'm' _ ';', 5, 0,
                          0x22e8 _ 0)
NAMED_CHARACTER_REFERENCE(1733, /* p r */ 'o' _ 'd' _ ';', 3, 0, 0x220f _ 0)
NAMED_CHARACTER_REFERENCE(1734,
                          /* p r */ 'o' _ 'f' _ 'a' _ 'l' _ 'a' _ 'r' _ ';', 7,
                          0, 0x232e _ 0)
NAMED_CHARACTER_REFERENCE(1735,
                          /* p r */ 'o' _ 'f' _ 'l' _ 'i' _ 'n' _ 'e' _ ';', 7,
                          0, 0x2312 _ 0)
NAMED_CHARACTER_REFERENCE(1736,
                          /* p r */ 'o' _ 'f' _ 's' _ 'u' _ 'r' _ 'f' _ ';', 7,
                          0, 0x2313 _ 0)
NAMED_CHARACTER_REFERENCE(1737, /* p r */ 'o' _ 'p' _ ';', 3, 0, 0x221d _ 0)
NAMED_CHARACTER_REFERENCE(1738,
                          /* p r */ 'o' _ 'p' _ 't' _ 'o' _ ';', 5, 0,
                          0x221d _ 0)
NAMED_CHARACTER_REFERENCE(1739,
                          /* p r */ 's' _ 'i' _ 'm' _ ';', 4, 0, 0x227e _ 0)
NAMED_CHARACTER_REFERENCE(1740,
                          /* p r */ 'u' _ 'r' _ 'e' _ 'l' _ ';', 5, 0,
                          0x22b0 _ 0)
NAMED_CHARACTER_REFERENCE(1741,
                          /* p s */ 'c' _ 'r' _ ';', 3, 0, 0xd835 _ 0xdcc5)
NAMED_CHARACTER_REFERENCE(1742, /* p s */ 'i' _ ';', 2, 0, 0x03c8 _ 0)
NAMED_CHARACTER_REFERENCE(1743,
                          /* p u */ 'n' _ 'c' _ 's' _ 'p' _ ';', 5, 0,
                          0x2008 _ 0)
NAMED_CHARACTER_REFERENCE(1744, /* q f */ 'r' _ ';', 2, 0, 0xd835 _ 0xdd2e)
NAMED_CHARACTER_REFERENCE(1745, /* q i */ 'n' _ 't' _ ';', 3, 0, 0x2a0c _ 0)
NAMED_CHARACTER_REFERENCE(1746,
                          /* q o */ 'p' _ 'f' _ ';', 3, 0, 0xd835 _ 0xdd62)
NAMED_CHARACTER_REFERENCE(1747,
                          /* q p */ 'r' _ 'i' _ 'm' _ 'e' _ ';', 5, 0,
                          0x2057 _ 0)
NAMED_CHARACTER_REFERENCE(1748,
                          /* q s */ 'c' _ 'r' _ ';', 3, 0, 0xd835 _ 0xdcc6)
NAMED_CHARACTER_REFERENCE(
    1749,
    /* q u */ 'a' _ 't' _ 'e' _ 'r' _ 'n' _ 'i' _ 'o' _ 'n' _ 's' _ ';', 10, 0,
    0x210d _ 0)
NAMED_CHARACTER_REFERENCE(1750,
                          /* q u */ 'a' _ 't' _ 'i' _ 'n' _ 't' _ ';', 6, 0,
                          0x2a16 _ 0)
NAMED_CHARACTER_REFERENCE(1751,
                          /* q u */ 'e' _ 's' _ 't' _ ';', 4, 0, 0x003f _ 0)
NAMED_CHARACTER_REFERENCE(1752,
                          /* q u */ 'e' _ 's' _ 't' _ 'e' _ 'q' _ ';', 6, 0,
                          0x225f _ 0)
NAMED_CHARACTER_REFERENCE(1753, /* q u */ 'o' _ 't', 2, 0, 0x0022 _ 0)
NAMED_CHARACTER_REFERENCE(1754, /* q u */ 'o' _ 't' _ ';', 3, 0, 0x0022 _ 0)
NAMED_CHARACTER_REFERENCE(1755,
                          /* r A */ 'a' _ 'r' _ 'r' _ ';', 4, 0, 0x21db _ 0)
NAMED_CHARACTER_REFERENCE(1756, /* r A */ 'r' _ 'r' _ ';', 3, 0, 0x21d2 _ 0)
NAMED_CHARACTER_REFERENCE(1757,
                          /* r A */ 't' _ 'a' _ 'i' _ 'l' _ ';', 5, 0,
                          0x291c _ 0)
NAMED_CHARACTER_REFERENCE(1758,
                          /* r B */ 'a' _ 'r' _ 'r' _ ';', 4, 0, 0x290f _ 0)
NAMED_CHARACTER_REFERENCE(1759, /* r H */ 'a' _ 'r' _ ';', 3, 0, 0x2964 _ 0)
NAMED_CHARACTER_REFERENCE(1760,
                          /* r a */ 'c' _ 'e' _ ';', 3, 0, 0x223d _ 0x0331)
NAMED_CHARACTER_REFERENCE(1761,
                          /* r a */ 'c' _ 'u' _ 't' _ 'e' _ ';', 5, 0,
                          0x0155 _ 0)
NAMED_CHARACTER_REFERENCE(1762,
                          /* r a */ 'd' _ 'i' _ 'c' _ ';', 4, 0, 0x221a _ 0)
NAMED_CHARACTER_REFERENCE(1763,
                          /* r a */ 'e' _ 'm' _ 'p' _ 't' _ 'y' _ 'v' _ ';', 7,
                          0, 0x29b3 _ 0)
NAMED_CHARACTER_REFERENCE(1764, /* r a */ 'n' _ 'g' _ ';', 3, 0, 0x27e9 _ 0)
NAMED_CHARACTER_REFERENCE(1765,
                          /* r a */ 'n' _ 'g' _ 'd' _ ';', 4, 0, 0x2992 _ 0)
NAMED_CHARACTER_REFERENCE(1766,
                          /* r a */ 'n' _ 'g' _ 'e' _ ';', 4, 0, 0x29a5 _ 0)
NAMED_CHARACTER_REFERENCE(1767,
                          /* r a */ 'n' _ 'g' _ 'l' _ 'e' _ ';', 5, 0,
                          0x27e9 _ 0)
NAMED_CHARACTER_REFERENCE(1768, /* r a */ 'q' _ 'u' _ 'o', 3, 0, 0x00bb _ 0)
NAMED_CHARACTER_REFERENCE(1769,
                          /* r a */ 'q' _ 'u' _ 'o' _ ';', 4, 0, 0x00bb _ 0)
NAMED_CHARACTER_REFERENCE(1770, /* r a */ 'r' _ 'r' _ ';', 3, 0, 0x2192 _ 0)
NAMED_CHARACTER_REFERENCE(1771,
                          /* r a */ 'r' _ 'r' _ 'a' _ 'p' _ ';', 5, 0,
                          0x2975 _ 0)
NAMED_CHARACTER_REFERENCE(1772,
                          /* r a */ 'r' _ 'r' _ 'b' _ ';', 4, 0, 0x21e5 _ 0)
NAMED_CHARACTER_REFERENCE(1773,
                          /* r a */ 'r' _ 'r' _ 'b' _ 'f' _ 's' _ ';', 6, 0,
                          0x2920 _ 0)
NAMED_CHARACTER_REFERENCE(1774,
                          /* r a */ 'r' _ 'r' _ 'c' _ ';', 4, 0, 0x2933 _ 0)
NAMED_CHARACTER_REFERENCE(1775,
                          /* r a */ 'r' _ 'r' _ 'f' _ 's' _ ';', 5, 0,
                          0x291e _ 0)
NAMED_CHARACTER_REFERENCE(1776,
                          /* r a */ 'r' _ 'r' _ 'h' _ 'k' _ ';', 5, 0,
                          0x21aa _ 0)
NAMED_CHARACTER_REFERENCE(1777,
                          /* r a */ 'r' _ 'r' _ 'l' _ 'p' _ ';', 5, 0,
                          0x21ac _ 0)
NAMED_CHARACTER_REFERENCE(1778,
                          /* r a */ 'r' _ 'r' _ 'p' _ 'l' _ ';', 5, 0,
                          0x2945 _ 0)
NAMED_CHARACTER_REFERENCE(1779,
                          /* r a */ 'r' _ 'r' _ 's' _ 'i' _ 'm' _ ';', 6, 0,
                          0x2974 _ 0)
NAMED_CHARACTER_REFERENCE(1780,
                          /* r a */ 'r' _ 'r' _ 't' _ 'l' _ ';', 5, 0,
                          0x21a3 _ 0)
NAMED_CHARACTER_REFERENCE(1781,
                          /* r a */ 'r' _ 'r' _ 'w' _ ';', 4, 0, 0x219d _ 0)
NAMED_CHARACTER_REFERENCE(1782,
                          /* r a */ 't' _ 'a' _ 'i' _ 'l' _ ';', 5, 0,
                          0x291a _ 0)
NAMED_CHARACTER_REFERENCE(1783,
                          /* r a */ 't' _ 'i' _ 'o' _ ';', 4, 0, 0x2236 _ 0)
NAMED_CHARACTER_REFERENCE(
    1784,
    /* r a */ 't' _ 'i' _ 'o' _ 'n' _ 'a' _ 'l' _ 's' _ ';', 8, 0, 0x211a _ 0)
NAMED_CHARACTER_REFERENCE(1785,
                          /* r b */ 'a' _ 'r' _ 'r' _ ';', 4, 0, 0x290d _ 0)
NAMED_CHARACTER_REFERENCE(1786,
                          /* r b */ 'b' _ 'r' _ 'k' _ ';', 4, 0, 0x2773 _ 0)
NAMED_CHARACTER_REFERENCE(1787,
                          /* r b */ 'r' _ 'a' _ 'c' _ 'e' _ ';', 5, 0,
                          0x007d _ 0)
NAMED_CHARACTER_REFERENCE(1788,
                          /* r b */ 'r' _ 'a' _ 'c' _ 'k' _ ';', 5, 0,
                          0x005d _ 0)
NAMED_CHARACTER_REFERENCE(1789,
                          /* r b */ 'r' _ 'k' _ 'e' _ ';', 4, 0, 0x298c _ 0)
NAMED_CHARACTER_REFERENCE(1790,
                          /* r b */ 'r' _ 'k' _ 's' _ 'l' _ 'd' _ ';', 6, 0,
                          0x298e _ 0)
NAMED_CHARACTER_REFERENCE(1791,
                          /* r b */ 'r' _ 'k' _ 's' _ 'l' _ 'u' _ ';', 6, 0,
                          0x2990 _ 0)
NAMED_CHARACTER_REFERENCE(1792,
                          /* r c */ 'a' _ 'r' _ 'o' _ 'n' _ ';', 5, 0,
                          0x0159 _ 0)
NAMED_CHARACTER_REFERENCE(1793,
                          /* r c */ 'e' _ 'd' _ 'i' _ 'l' _ ';', 5, 0,
                          0x0157 _ 0)
NAMED_CHARACTER_REFERENCE(1794,
                          /* r c */ 'e' _ 'i' _ 'l' _ ';', 4, 0, 0x2309 _ 0)
NAMED_CHARACTER_REFERENCE(1795, /* r c */ 'u' _ 'b' _ ';', 3, 0, 0x007d _ 0)
NAMED_CHARACTER_REFERENCE(1796, /* r c */ 'y' _ ';', 2, 0, 0x0440 _ 0)
NAMED_CHARACTER_REFERENCE(1797, /* r d */ 'c' _ 'a' _ ';', 3, 0, 0x2937 _ 0)
NAMED_CHARACTER_REFERENCE(1798,
                          /* r d */ 'l' _ 'd' _ 'h' _ 'a' _ 'r' _ ';', 6, 0,
                          0x2969 _ 0)
NAMED_CHARACTER_REFERENCE(1799,
                          /* r d */ 'q' _ 'u' _ 'o' _ ';', 4, 0, 0x201d _ 0)
NAMED_CHARACTER_REFERENCE(1800,
                          /* r d */ 'q' _ 'u' _ 'o' _ 'r' _ ';', 5, 0,
                          0x201d _ 0)
NAMED_CHARACTER_REFERENCE(1801, /* r d */ 's' _ 'h' _ ';', 3, 0, 0x21b3 _ 0)
NAMED_CHARACTER_REFERENCE(1802, /* r e */ 'a' _ 'l' _ ';', 3, 0, 0x211c _ 0)
NAMED_CHARACTER_REFERENCE(1803,
                          /* r e */ 'a' _ 'l' _ 'i' _ 'n' _ 'e' _ ';', 6, 0,
                          0x211b _ 0)
NAMED_CHARACTER_REFERENCE(1804,
                          /* r e */ 'a' _ 'l' _ 'p' _ 'a' _ 'r' _ 't' _ ';', 7,
                          0, 0x211c _ 0)
NAMED_CHARACTER_REFERENCE(1805,
                          /* r e */ 'a' _ 'l' _ 's' _ ';', 4, 0, 0x211d _ 0)
NAMED_CHARACTER_REFERENCE(1806, /* r e */ 'c' _ 't' _ ';', 3, 0, 0x25ad _ 0)
NAMED_CHARACTER_REFERENCE(1807, /* r e */ 'g', 1, 0, 0x00ae _ 0)
NAMED_CHARACTER_REFERENCE(1808, /* r e */ 'g' _ ';', 2, 0, 0x00ae _ 0)
NAMED_CHARACTER_REFERENCE(1809,
                          /* r f */ 'i' _ 's' _ 'h' _ 't' _ ';', 5, 0,
                          0x297d _ 0)
NAMED_CHARACTER_REFERENCE(1810,
                          /* r f */ 'l' _ 'o' _ 'o' _ 'r' _ ';', 5, 0,
                          0x230b _ 0)
NAMED_CHARACTER_REFERENCE(1811, /* r f */ 'r' _ ';', 2, 0, 0xd835 _ 0xdd2f)
NAMED_CHARACTER_REFERENCE(1812,
                          /* r h */ 'a' _ 'r' _ 'd' _ ';', 4, 0, 0x21c1 _ 0)
NAMED_CHARACTER_REFERENCE(1813,
                          /* r h */ 'a' _ 'r' _ 'u' _ ';', 4, 0, 0x21c0 _ 0)
NAMED_CHARACTER_REFERENCE(1814,
                          /* r h */ 'a' _ 'r' _ 'u' _ 'l' _ ';', 5, 0,
                          0x296c _ 0)
NAMED_CHARACTER_REFERENCE(1815, /* r h */ 'o' _ ';', 2, 0, 0x03c1 _ 0)
NAMED_CHARACTER_REFERENCE(1816, /* r h */ 'o' _ 'v' _ ';', 3, 0, 0x03f1 _ 0)
NAMED_CHARACTER_REFERENCE(
    1817,
    /* r i */ 'g' _ 'h' _ 't' _ 'a' _ 'r' _ 'r' _ 'o' _ 'w' _ ';', 9, 0,
    0x2192 _ 0)
NAMED_CHARACTER_REFERENCE(
    1818,
    /* r i */
    'g' _ 'h' _ 't' _ 'a' _ 'r' _ 'r' _ 'o' _ 'w' _ 't' _ 'a' _ 'i' _ 'l' _ ';',
    13, 0, 0x21a3 _ 0)
NAMED_CHARACTER_REFERENCE(
    1819,
    /* r i */
    'g' _ 'h' _ 't' _ 'h' _ 'a' _ 'r' _ 'p' _ 'o' _ 'o' _ 'n' _ 'd' _ 'o' _ 'w' _ 'n' _ ';',
    15, 0, 0x21c1 _ 0)
NAMED_CHARACTER_REFERENCE(
    1820,
    /* r i */
    'g' _ 'h' _ 't' _ 'h' _ 'a' _ 'r' _ 'p' _ 'o' _ 'o' _ 'n' _ 'u' _ 'p' _ ';',
    13, 0, 0x21c0 _ 0)
NAMED_CHARACTER_REFERENCE(
    1821,
    /* r i */
    'g' _ 'h' _ 't' _ 'l' _ 'e' _ 'f' _ 't' _ 'a' _ 'r' _ 'r' _ 'o' _ 'w' _ 's' _ ';',
    14, 0, 0x21c4 _ 0)
NAMED_CHARACTER_REFERENCE(
    1822,
    /* r i */
    'g' _ 'h' _ 't' _ 'l' _ 'e' _ 'f' _ 't' _ 'h' _ 'a' _ 'r' _ 'p' _ 'o' _ 'o' _ 'n' _ 's' _ ';',
    16, 0, 0x21cc _ 0)
NAMED_CHARACTER_REFERENCE(
    1823,
    /* r i */
    'g' _ 'h' _ 't' _ 'r' _ 'i' _ 'g' _ 'h' _ 't' _ 'a' _ 'r' _ 'r' _ 'o' _ 'w' _ 's' _ ';',
    15, 0, 0x21c9 _ 0)
NAMED_CHARACTER_REFERENCE(
    1824,
    /* r i */
    'g' _ 'h' _ 't' _ 's' _ 'q' _ 'u' _ 'i' _ 'g' _ 'a' _ 'r' _ 'r' _ 'o' _ 'w' _ ';',
    14, 0, 0x219d _ 0)
NAMED_CHARACTER_REFERENCE(
    1825,
    /* r i */
    'g' _ 'h' _ 't' _ 't' _ 'h' _ 'r' _ 'e' _ 'e' _ 't' _ 'i' _ 'm' _ 'e' _ 's' _ ';',
    14, 0, 0x22cc _ 0)
NAMED_CHARACTER_REFERENCE(1826, /* r i */ 'n' _ 'g' _ ';', 3, 0, 0x02da _ 0)
NAMED_CHARACTER_REFERENCE(
    1827,
    /* r i */ 's' _ 'i' _ 'n' _ 'g' _ 'd' _ 'o' _ 't' _ 's' _ 'e' _ 'q' _ ';',
    11, 0, 0x2253 _ 0)
NAMED_CHARACTER_REFERENCE(1828,
                          /* r l */ 'a' _ 'r' _ 'r' _ ';', 4, 0, 0x21c4 _ 0)
NAMED_CHARACTER_REFERENCE(1829,
                          /* r l */ 'h' _ 'a' _ 'r' _ ';', 4, 0, 0x21cc _ 0)
NAMED_CHARACTER_REFERENCE(1830, /* r l */ 'm' _ ';', 2, 0, 0x200f _ 0)
NAMED_CHARACTER_REFERENCE(1831,
                          /* r m */ 'o' _ 'u' _ 's' _ 't' _ ';', 5, 0,
                          0x23b1 _ 0)
NAMED_CHARACTER_REFERENCE(
    1832,
    /* r m */ 'o' _ 'u' _ 's' _ 't' _ 'a' _ 'c' _ 'h' _ 'e' _ ';', 9, 0,
    0x23b1 _ 0)
NAMED_CHARACTER_REFERENCE(1833,
                          /* r n */ 'm' _ 'i' _ 'd' _ ';', 4, 0, 0x2aee _ 0)
NAMED_CHARACTER_REFERENCE(1834,
                          /* r o */ 'a' _ 'n' _ 'g' _ ';', 4, 0, 0x27ed _ 0)
NAMED_CHARACTER_REFERENCE(1835,
                          /* r o */ 'a' _ 'r' _ 'r' _ ';', 4, 0, 0x21fe _ 0)
NAMED_CHARACTER_REFERENCE(1836,
                          /* r o */ 'b' _ 'r' _ 'k' _ ';', 4, 0, 0x27e7 _ 0)
NAMED_CHARACTER_REFERENCE(1837,
                          /* r o */ 'p' _ 'a' _ 'r' _ ';', 4, 0, 0x2986 _ 0)
NAMED_CHARACTER_REFERENCE(1838,
                          /* r o */ 'p' _ 'f' _ ';', 3, 0, 0xd835 _ 0xdd63)
NAMED_CHARACTER_REFERENCE(1839,
                          /* r o */ 'p' _ 'l' _ 'u' _ 's' _ ';', 5, 0,
                          0x2a2e _ 0)
NAMED_CHARACTER_REFERENCE(1840,
                          /* r o */ 't' _ 'i' _ 'm' _ 'e' _ 's' _ ';', 6, 0,
                          0x2a35 _ 0)
NAMED_CHARACTER_REFERENCE(1841, /* r p */ 'a' _ 'r' _ ';', 3, 0, 0x0029 _ 0)
NAMED_CHARACTER_REFERENCE(1842,
                          /* r p */ 'a' _ 'r' _ 'g' _ 't' _ ';', 5, 0,
                          0x2994 _ 0)
NAMED_CHARACTER_REFERENCE(1843,
                          /* r p */ 'p' _ 'o' _ 'l' _ 'i' _ 'n' _ 't' _ ';', 7,
                          0, 0x2a12 _ 0)
NAMED_CHARACTER_REFERENCE(1844,
                          /* r r */ 'a' _ 'r' _ 'r' _ ';', 4, 0, 0x21c9 _ 0)
NAMED_CHARACTER_REFERENCE(1845,
                          /* r s */ 'a' _ 'q' _ 'u' _ 'o' _ ';', 5, 0,
                          0x203a _ 0)
NAMED_CHARACTER_REFERENCE(1846,
                          /* r s */ 'c' _ 'r' _ ';', 3, 0, 0xd835 _ 0xdcc7)
NAMED_CHARACTER_REFERENCE(1847, /* r s */ 'h' _ ';', 2, 0, 0x21b1 _ 0)
NAMED_CHARACTER_REFERENCE(1848, /* r s */ 'q' _ 'b' _ ';', 3, 0, 0x005d _ 0)
NAMED_CHARACTER_REFERENCE(1849,
                          /* r s */ 'q' _ 'u' _ 'o' _ ';', 4, 0, 0x2019 _ 0)
NAMED_CHARACTER_REFERENCE(1850,
                          /* r s */ 'q' _ 'u' _ 'o' _ 'r' _ ';', 5, 0,
                          0x2019 _ 0)
NAMED_CHARACTER_REFERENCE(1851,
                          /* r t */ 'h' _ 'r' _ 'e' _ 'e' _ ';', 5, 0,
                          0x22cc _ 0)
NAMED_CHARACTER_REFERENCE(1852,
                          /* r t */ 'i' _ 'm' _ 'e' _ 's' _ ';', 5, 0,
                          0x22ca _ 0)
NAMED_CHARACTER_REFERENCE(1853, /* r t */ 'r' _ 'i' _ ';', 3, 0, 0x25b9 _ 0)
NAMED_CHARACTER_REFERENCE(1854,
                          /* r t */ 'r' _ 'i' _ 'e' _ ';', 4, 0, 0x22b5 _ 0)
NAMED_CHARACTER_REFERENCE(1855,
                          /* r t */ 'r' _ 'i' _ 'f' _ ';', 4, 0, 0x25b8 _ 0)
NAMED_CHARACTER_REFERENCE(1856,
                          /* r t */ 'r' _ 'i' _ 'l' _ 't' _ 'r' _ 'i' _ ';', 7,
                          0, 0x29ce _ 0)
NAMED_CHARACTER_REFERENCE(1857,
                          /* r u */ 'l' _ 'u' _ 'h' _ 'a' _ 'r' _ ';', 6, 0,
                          0x2968 _ 0)
NAMED_CHARACTER_REFERENCE(1858, /* r x */ ';', 1, 0, 0x211e _ 0)
NAMED_CHARACTER_REFERENCE(1859,
                          /* s a */ 'c' _ 'u' _ 't' _ 'e' _ ';', 5, 0,
                          0x015b _ 0)
NAMED_CHARACTER_REFERENCE(1860,
                          /* s b */ 'q' _ 'u' _ 'o' _ ';', 4, 0, 0x201a _ 0)
NAMED_CHARACTER_REFERENCE(1861, /* s c */ ';', 1, 0, 0x227b _ 0)
NAMED_CHARACTER_REFERENCE(1862, /* s c */ 'E' _ ';', 2, 0, 0x2ab4 _ 0)
NAMED_CHARACTER_REFERENCE(1863, /* s c */ 'a' _ 'p' _ ';', 3, 0, 0x2ab8 _ 0)
NAMED_CHARACTER_REFERENCE(1864,
                          /* s c */ 'a' _ 'r' _ 'o' _ 'n' _ ';', 5, 0,
                          0x0161 _ 0)
NAMED_CHARACTER_REFERENCE(1865,
                          /* s c */ 'c' _ 'u' _ 'e' _ ';', 4, 0, 0x227d _ 0)
NAMED_CHARACTER_REFERENCE(1866, /* s c */ 'e' _ ';', 2, 0, 0x2ab0 _ 0)
NAMED_CHARACTER_REFERENCE(1867,
                          /* s c */ 'e' _ 'd' _ 'i' _ 'l' _ ';', 5, 0,
                          0x015f _ 0)
NAMED_CHARACTER_REFERENCE(1868,
                          /* s c */ 'i' _ 'r' _ 'c' _ ';', 4, 0, 0x015d _ 0)
NAMED_CHARACTER_REFERENCE(1869, /* s c */ 'n' _ 'E' _ ';', 3, 0, 0x2ab6 _ 0)
NAMED_CHARACTER_REFERENCE(1870,
                          /* s c */ 'n' _ 'a' _ 'p' _ ';', 4, 0, 0x2aba _ 0)
NAMED_CHARACTER_REFERENCE(1871,
                          /* s c */ 'n' _ 's' _ 'i' _ 'm' _ ';', 5, 0,
                          0x22e9 _ 0)
NAMED_CHARACTER_REFERENCE(1872,
                          /* s c */ 'p' _ 'o' _ 'l' _ 'i' _ 'n' _ 't' _ ';', 7,
                          0, 0x2a13 _ 0)
NAMED_CHARACTER_REFERENCE(1873,
                          /* s c */ 's' _ 'i' _ 'm' _ ';', 4, 0, 0x227f _ 0)
NAMED_CHARACTER_REFERENCE(1874, /* s c */ 'y' _ ';', 2, 0, 0x0441 _ 0)
NAMED_CHARACTER_REFERENCE(1875, /* s d */ 'o' _ 't' _ ';', 3, 0, 0x22c5 _ 0)
NAMED_CHARACTER_REFERENCE(1876,
                          /* s d */ 'o' _ 't' _ 'b' _ ';', 4, 0, 0x22a1 _ 0)
NAMED_CHARACTER_REFERENCE(1877,
                          /* s d */ 'o' _ 't' _ 'e' _ ';', 4, 0, 0x2a66 _ 0)
NAMED_CHARACTER_REFERENCE(1878,
                          /* s e */ 'A' _ 'r' _ 'r' _ ';', 4, 0, 0x21d8 _ 0)
NAMED_CHARACTER_REFERENCE(1879,
                          /* s e */ 'a' _ 'r' _ 'h' _ 'k' _ ';', 5, 0,
                          0x2925 _ 0)
NAMED_CHARACTER_REFERENCE(1880,
                          /* s e */ 'a' _ 'r' _ 'r' _ ';', 4, 0, 0x2198 _ 0)
NAMED_CHARACTER_REFERENCE(1881,
                          /* s e */ 'a' _ 'r' _ 'r' _ 'o' _ 'w' _ ';', 6, 0,
                          0x2198 _ 0)
NAMED_CHARACTER_REFERENCE(1882, /* s e */ 'c' _ 't', 2, 0, 0x00a7 _ 0)
NAMED_CHARACTER_REFERENCE(1883, /* s e */ 'c' _ 't' _ ';', 3, 0, 0x00a7 _ 0)
NAMED_CHARACTER_REFERENCE(1884, /* s e */ 'm' _ 'i' _ ';', 3, 0, 0x003b _ 0)
NAMED_CHARACTER_REFERENCE(1885,
                          /* s e */ 's' _ 'w' _ 'a' _ 'r' _ ';', 5, 0,
                          0x2929 _ 0)
NAMED_CHARACTER_REFERENCE(1886,
                          /* s e */ 't' _ 'm' _ 'i' _ 'n' _ 'u' _ 's' _ ';', 7,
                          0, 0x2216 _ 0)
NAMED_CHARACTER_REFERENCE(1887,
                          /* s e */ 't' _ 'm' _ 'n' _ ';', 4, 0, 0x2216 _ 0)
NAMED_CHARACTER_REFERENCE(1888, /* s e */ 'x' _ 't' _ ';', 3, 0, 0x2736 _ 0)
NAMED_CHARACTER_REFERENCE(1889, /* s f */ 'r' _ ';', 2, 0, 0xd835 _ 0xdd30)
NAMED_CHARACTER_REFERENCE(1890,
                          /* s f */ 'r' _ 'o' _ 'w' _ 'n' _ ';', 5, 0,
                          0x2322 _ 0)
NAMED_CHARACTER_REFERENCE(1891,
                          /* s h */ 'a' _ 'r' _ 'p' _ ';', 4, 0, 0x266f _ 0)
NAMED_CHARACTER_REFERENCE(1892,
                          /* s h */ 'c' _ 'h' _ 'c' _ 'y' _ ';', 5, 0,
                          0x0449 _ 0)
NAMED_CHARACTER_REFERENCE(1893, /* s h */ 'c' _ 'y' _ ';', 3, 0, 0x0448 _ 0)
NAMED_CHARACTER_REFERENCE(1894,
                          /* s h */ 'o' _ 'r' _ 't' _ 'm' _ 'i' _ 'd' _ ';', 7,
                          0, 0x2223 _ 0)
NAMED_CHARACTER_REFERENCE(
    1895,
    /* s h */
    'o' _ 'r' _ 't' _ 'p' _ 'a' _ 'r' _ 'a' _ 'l' _ 'l' _ 'e' _ 'l' _ ';', 12,
    0, 0x2225 _ 0)
NAMED_CHARACTER_REFERENCE(1896, /* s h */ 'y', 1, 0, 0x00ad _ 0)
NAMED_CHARACTER_REFERENCE(1897, /* s h */ 'y' _ ';', 2, 0, 0x00ad _ 0)
NAMED_CHARACTER_REFERENCE(1898,
                          /* s i */ 'g' _ 'm' _ 'a' _ ';', 4, 0, 0x03c3 _ 0)
NAMED_CHARACTER_REFERENCE(1899,
                          /* s i */ 'g' _ 'm' _ 'a' _ 'f' _ ';', 5, 0,
                          0x03c2 _ 0)
NAMED_CHARACTER_REFERENCE(1900,
                          /* s i */ 'g' _ 'm' _ 'a' _ 'v' _ ';', 5, 0,
                          0x03c2 _ 0)
NAMED_CHARACTER_REFERENCE(1901, /* s i */ 'm' _ ';', 2, 0, 0x223c _ 0)
NAMED_CHARACTER_REFERENCE(1902,
                          /* s i */ 'm' _ 'd' _ 'o' _ 't' _ ';', 5, 0,
                          0x2a6a _ 0)
NAMED_CHARACTER_REFERENCE(1903, /* s i */ 'm' _ 'e' _ ';', 3, 0, 0x2243 _ 0)
NAMED_CHARACTER_REFERENCE(1904,
                          /* s i */ 'm' _ 'e' _ 'q' _ ';', 4, 0, 0x2243 _ 0)
NAMED_CHARACTER_REFERENCE(1905, /* s i */ 'm' _ 'g' _ ';', 3, 0, 0x2a9e _ 0)
NAMED_CHARACTER_REFERENCE(1906,
                          /* s i */ 'm' _ 'g' _ 'E' _ ';', 4, 0, 0x2aa0 _ 0)
NAMED_CHARACTER_REFERENCE(1907, /* s i */ 'm' _ 'l' _ ';', 3, 0, 0x2a9d _ 0)
NAMED_CHARACTER_REFERENCE(1908,
                          /* s i */ 'm' _ 'l' _ 'E' _ ';', 4, 0, 0x2a9f _ 0)
NAMED_CHARACTER_REFERENCE(1909,
                          /* s i */ 'm' _ 'n' _ 'e' _ ';', 4, 0, 0x2246 _ 0)
NAMED_CHARACTER_REFERENCE(1910,
                          /* s i */ 'm' _ 'p' _ 'l' _ 'u' _ 's' _ ';', 6, 0,
                          0x2a24 _ 0)
NAMED_CHARACTER_REFERENCE(1911,
                          /* s i */ 'm' _ 'r' _ 'a' _ 'r' _ 'r' _ ';', 6, 0,
                          0x2972 _ 0)
NAMED_CHARACTER_REFERENCE(1912,
                          /* s l */ 'a' _ 'r' _ 'r' _ ';', 4, 0, 0x2190 _ 0)
NAMED_CHARACTER_REFERENCE(
    1913,
    /* s m */
    'a' _ 'l' _ 'l' _ 's' _ 'e' _ 't' _ 'm' _ 'i' _ 'n' _ 'u' _ 's' _ ';', 12,
    0, 0x2216 _ 0)
NAMED_CHARACTER_REFERENCE(1914,
                          /* s m */ 'a' _ 's' _ 'h' _ 'p' _ ';', 5, 0,
                          0x2a33 _ 0)
NAMED_CHARACTER_REFERENCE(1915,
                          /* s m */ 'e' _ 'p' _ 'a' _ 'r' _ 's' _ 'l' _ ';', 7,
                          0, 0x29e4 _ 0)
NAMED_CHARACTER_REFERENCE(1916, /* s m */ 'i' _ 'd' _ ';', 3, 0, 0x2223 _ 0)
NAMED_CHARACTER_REFERENCE(1917,
                          /* s m */ 'i' _ 'l' _ 'e' _ ';', 4, 0, 0x2323 _ 0)
NAMED_CHARACTER_REFERENCE(1918, /* s m */ 't' _ ';', 2, 0, 0x2aaa _ 0)
NAMED_CHARACTER_REFERENCE(1919, /* s m */ 't' _ 'e' _ ';', 3, 0, 0x2aac _ 0)
NAMED_CHARACTER_REFERENCE(1920,
                          /* s m */ 't' _ 'e' _ 's' _ ';', 4, 0,
                          0x2aac _ 0xfe00)
NAMED_CHARACTER_REFERENCE(1921,
                          /* s o */ 'f' _ 't' _ 'c' _ 'y' _ ';', 5, 0,
                          0x044c _ 0)
NAMED_CHARACTER_REFERENCE(1922, /* s o */ 'l' _ ';', 2, 0, 0x002f _ 0)
NAMED_CHARACTER_REFERENCE(1923, /* s o */ 'l' _ 'b' _ ';', 3, 0, 0x29c4 _ 0)
NAMED_CHARACTER_REFERENCE(1924,
                          /* s o */ 'l' _ 'b' _ 'a' _ 'r' _ ';', 5, 0,
                          0x233f _ 0)
NAMED_CHARACTER_REFERENCE(1925,
                          /* s o */ 'p' _ 'f' _ ';', 3, 0, 0xd835 _ 0xdd64)
NAMED_CHARACTER_REFERENCE(1926,
                          /* s p */ 'a' _ 'd' _ 'e' _ 's' _ ';', 5, 0,
                          0x2660 _ 0)
NAMED_CHARACTER_REFERENCE(
    1927,
    /* s p */ 'a' _ 'd' _ 'e' _ 's' _ 'u' _ 'i' _ 't' _ ';', 8, 0, 0x2660 _ 0)
NAMED_CHARACTER_REFERENCE(1928, /* s p */ 'a' _ 'r' _ ';', 3, 0, 0x2225 _ 0)
NAMED_CHARACTER_REFERENCE(1929,
                          /* s q */ 'c' _ 'a' _ 'p' _ ';', 4, 0, 0x2293 _ 0)
NAMED_CHARACTER_REFERENCE(1930,
                          /* s q */ 'c' _ 'a' _ 'p' _ 's' _ ';', 5, 0,
                          0x2293 _ 0xfe00)
NAMED_CHARACTER_REFERENCE(1931,
                          /* s q */ 'c' _ 'u' _ 'p' _ ';', 4, 0, 0x2294 _ 0)
NAMED_CHARACTER_REFERENCE(1932,
                          /* s q */ 'c' _ 'u' _ 'p' _ 's' _ ';', 5, 0,
                          0x2294 _ 0xfe00)
NAMED_CHARACTER_REFERENCE(1933,
                          /* s q */ 's' _ 'u' _ 'b' _ ';', 4, 0, 0x228f _ 0)
NAMED_CHARACTER_REFERENCE(1934,
                          /* s q */ 's' _ 'u' _ 'b' _ 'e' _ ';', 5, 0,
                          0x2291 _ 0)
NAMED_CHARACTER_REFERENCE(1935,
                          /* s q */ 's' _ 'u' _ 'b' _ 's' _ 'e' _ 't' _ ';', 7,
                          0, 0x228f _ 0)
NAMED_CHARACTER_REFERENCE(
    1936,
    /* s q */ 's' _ 'u' _ 'b' _ 's' _ 'e' _ 't' _ 'e' _ 'q' _ ';', 9, 0,
    0x2291 _ 0)
NAMED_CHARACTER_REFERENCE(1937,
                          /* s q */ 's' _ 'u' _ 'p' _ ';', 4, 0, 0x2290 _ 0)
NAMED_CHARACTER_REFERENCE(1938,
                          /* s q */ 's' _ 'u' _ 'p' _ 'e' _ ';', 5, 0,
                          0x2292 _ 0)
NAMED_CHARACTER_REFERENCE(1939,
                          /* s q */ 's' _ 'u' _ 'p' _ 's' _ 'e' _ 't' _ ';', 7,
                          0, 0x2290 _ 0)
NAMED_CHARACTER_REFERENCE(
    1940,
    /* s q */ 's' _ 'u' _ 'p' _ 's' _ 'e' _ 't' _ 'e' _ 'q' _ ';', 9, 0,
    0x2292 _ 0)
NAMED_CHARACTER_REFERENCE(1941, /* s q */ 'u' _ ';', 2, 0, 0x25a1 _ 0)
NAMED_CHARACTER_REFERENCE(1942,
                          /* s q */ 'u' _ 'a' _ 'r' _ 'e' _ ';', 5, 0,
                          0x25a1 _ 0)
NAMED_CHARACTER_REFERENCE(1943,
                          /* s q */ 'u' _ 'a' _ 'r' _ 'f' _ ';', 5, 0,
                          0x25aa _ 0)
NAMED_CHARACTER_REFERENCE(1944, /* s q */ 'u' _ 'f' _ ';', 3, 0, 0x25aa _ 0)
NAMED_CHARACTER_REFERENCE(1945,
                          /* s r */ 'a' _ 'r' _ 'r' _ ';', 4, 0, 0x2192 _ 0)
NAMED_CHARACTER_REFERENCE(1946,
                          /* s s */ 'c' _ 'r' _ ';', 3, 0, 0xd835 _ 0xdcc8)
NAMED_CHARACTER_REFERENCE(1947,
                          /* s s */ 'e' _ 't' _ 'm' _ 'n' _ ';', 5, 0,
                          0x2216 _ 0)
NAMED_CHARACTER_REFERENCE(1948,
                          /* s s */ 'm' _ 'i' _ 'l' _ 'e' _ ';', 5, 0,
                          0x2323 _ 0)
NAMED_CHARACTER_REFERENCE(1949,
                          /* s s */ 't' _ 'a' _ 'r' _ 'f' _ ';', 5, 0,
                          0x22c6 _ 0)
NAMED_CHARACTER_REFERENCE(1950, /* s t */ 'a' _ 'r' _ ';', 3, 0, 0x2606 _ 0)
NAMED_CHARACTER_REFERENCE(1951,
                          /* s t */ 'a' _ 'r' _ 'f' _ ';', 4, 0, 0x2605 _ 0)
NAMED_CHARACTER_REFERENCE(
    1952,
    /* s t */
    'r' _ 'a' _ 'i' _ 'g' _ 'h' _ 't' _ 'e' _ 'p' _ 's' _ 'i' _ 'l' _ 'o' _ 'n' _ ';',
    14, 0, 0x03f5 _ 0)
NAMED_CHARACTER_REFERENCE(
    1953,
    /* s t */ 'r' _ 'a' _ 'i' _ 'g' _ 'h' _ 't' _ 'p' _ 'h' _ 'i' _ ';', 10, 0,
    0x03d5 _ 0)
NAMED_CHARACTER_REFERENCE(1954,
                          /* s t */ 'r' _ 'n' _ 's' _ ';', 4, 0, 0x00af _ 0)
NAMED_CHARACTER_REFERENCE(1955, /* s u */ 'b' _ ';', 2, 0, 0x2282 _ 0)
NAMED_CHARACTER_REFERENCE(1956, /* s u */ 'b' _ 'E' _ ';', 3, 0, 0x2ac5 _ 0)
NAMED_CHARACTER_REFERENCE(1957,
                          /* s u */ 'b' _ 'd' _ 'o' _ 't' _ ';', 5, 0,
                          0x2abd _ 0)
NAMED_CHARACTER_REFERENCE(1958, /* s u */ 'b' _ 'e' _ ';', 3, 0, 0x2286 _ 0)
NAMED_CHARACTER_REFERENCE(1959,
                          /* s u */ 'b' _ 'e' _ 'd' _ 'o' _ 't' _ ';', 6, 0,
                          0x2ac3 _ 0)
NAMED_CHARACTER_REFERENCE(1960,
                          /* s u */ 'b' _ 'm' _ 'u' _ 'l' _ 't' _ ';', 6, 0,
                          0x2ac1 _ 0)
NAMED_CHARACTER_REFERENCE(1961,
                          /* s u */ 'b' _ 'n' _ 'E' _ ';', 4, 0, 0x2acb _ 0)
NAMED_CHARACTER_REFERENCE(1962,
                          /* s u */ 'b' _ 'n' _ 'e' _ ';', 4, 0, 0x228a _ 0)
NAMED_CHARACTER_REFERENCE(1963,
                          /* s u */ 'b' _ 'p' _ 'l' _ 'u' _ 's' _ ';', 6, 0,
                          0x2abf _ 0)
NAMED_CHARACTER_REFERENCE(1964,
                          /* s u */ 'b' _ 'r' _ 'a' _ 'r' _ 'r' _ ';', 6, 0,
                          0x2979 _ 0)
NAMED_CHARACTER_REFERENCE(1965,
                          /* s u */ 'b' _ 's' _ 'e' _ 't' _ ';', 5, 0,
                          0x2282 _ 0)
NAMED_CHARACTER_REFERENCE(1966,
                          /* s u */ 'b' _ 's' _ 'e' _ 't' _ 'e' _ 'q' _ ';', 7,
                          0, 0x2286 _ 0)
NAMED_CHARACTER_REFERENCE(
    1967,
    /* s u */ 'b' _ 's' _ 'e' _ 't' _ 'e' _ 'q' _ 'q' _ ';', 8, 0, 0x2ac5 _ 0)
NAMED_CHARACTER_REFERENCE(
    1968,
    /* s u */ 'b' _ 's' _ 'e' _ 't' _ 'n' _ 'e' _ 'q' _ ';', 8, 0, 0x228a _ 0)
NAMED_CHARACTER_REFERENCE(
    1969,
    /* s u */ 'b' _ 's' _ 'e' _ 't' _ 'n' _ 'e' _ 'q' _ 'q' _ ';', 9, 0,
    0x2acb _ 0)
NAMED_CHARACTER_REFERENCE(1970,
                          /* s u */ 'b' _ 's' _ 'i' _ 'm' _ ';', 5, 0,
                          0x2ac7 _ 0)
NAMED_CHARACTER_REFERENCE(1971,
                          /* s u */ 'b' _ 's' _ 'u' _ 'b' _ ';', 5, 0,
                          0x2ad5 _ 0)
NAMED_CHARACTER_REFERENCE(1972,
                          /* s u */ 'b' _ 's' _ 'u' _ 'p' _ ';', 5, 0,
                          0x2ad3 _ 0)
NAMED_CHARACTER_REFERENCE(1973, /* s u */ 'c' _ 'c' _ ';', 3, 0, 0x227b _ 0)
NAMED_CHARACTER_REFERENCE(
    1974,
    /* s u */ 'c' _ 'c' _ 'a' _ 'p' _ 'p' _ 'r' _ 'o' _ 'x' _ ';', 9, 0,
    0x2ab8 _ 0)
NAMED_CHARACTER_REFERENCE(
    1975,
    /* s u */ 'c' _ 'c' _ 'c' _ 'u' _ 'r' _ 'l' _ 'y' _ 'e' _ 'q' _ ';', 10, 0,
    0x227d _ 0)
NAMED_CHARACTER_REFERENCE(1976,
                          /* s u */ 'c' _ 'c' _ 'e' _ 'q' _ ';', 5, 0,
                          0x2ab0 _ 0)
NAMED_CHARACTER_REFERENCE(
    1977,
    /* s u */ 'c' _ 'c' _ 'n' _ 'a' _ 'p' _ 'p' _ 'r' _ 'o' _ 'x' _ ';', 10, 0,
    0x2aba _ 0)
NAMED_CHARACTER_REFERENCE(1978,
                          /* s u */ 'c' _ 'c' _ 'n' _ 'e' _ 'q' _ 'q' _ ';', 7,
                          0, 0x2ab6 _ 0)
NAMED_CHARACTER_REFERENCE(1979,
                          /* s u */ 'c' _ 'c' _ 'n' _ 's' _ 'i' _ 'm' _ ';', 7,
                          0, 0x22e9 _ 0)
NAMED_CHARACTER_REFERENCE(1980,
                          /* s u */ 'c' _ 'c' _ 's' _ 'i' _ 'm' _ ';', 6, 0,
                          0x227f _ 0)
NAMED_CHARACTER_REFERENCE(1981, /* s u */ 'm' _ ';', 2, 0, 0x2211 _ 0)
NAMED_CHARACTER_REFERENCE(1982, /* s u */ 'n' _ 'g' _ ';', 3, 0, 0x266a _ 0)
NAMED_CHARACTER_REFERENCE(1983, /* s u */ 'p' _ '1', 2, 0, 0x00b9 _ 0)
NAMED_CHARACTER_REFERENCE(1984, /* s u */ 'p' _ '1' _ ';', 3, 0, 0x00b9 _ 0)
NAMED_CHARACTER_REFERENCE(1985, /* s u */ 'p' _ '2', 2, 0, 0x00b2 _ 0)
NAMED_CHARACTER_REFERENCE(1986, /* s u */ 'p' _ '2' _ ';', 3, 0, 0x00b2 _ 0)
NAMED_CHARACTER_REFERENCE(1987, /* s u */ 'p' _ '3', 2, 0, 0x00b3 _ 0)
NAMED_CHARACTER_REFERENCE(1988, /* s u */ 'p' _ '3' _ ';', 3, 0, 0x00b3 _ 0)
NAMED_CHARACTER_REFERENCE(1989, /* s u */ 'p' _ ';', 2, 0, 0x2283 _ 0)
NAMED_CHARACTER_REFERENCE(1990, /* s u */ 'p' _ 'E' _ ';', 3, 0, 0x2ac6 _ 0)
NAMED_CHARACTER_REFERENCE(1991,
                          /* s u */ 'p' _ 'd' _ 'o' _ 't' _ ';', 5, 0,
                          0x2abe _ 0)
NAMED_CHARACTER_REFERENCE(1992,
                          /* s u */ 'p' _ 'd' _ 's' _ 'u' _ 'b' _ ';', 6, 0,
                          0x2ad8 _ 0)
NAMED_CHARACTER_REFERENCE(1993, /* s u */ 'p' _ 'e' _ ';', 3, 0, 0x2287 _ 0)
NAMED_CHARACTER_REFERENCE(1994,
                          /* s u */ 'p' _ 'e' _ 'd' _ 'o' _ 't' _ ';', 6, 0,
                          0x2ac4 _ 0)
NAMED_CHARACTER_REFERENCE(1995,
                          /* s u */ 'p' _ 'h' _ 's' _ 'o' _ 'l' _ ';', 6, 0,
                          0x27c9 _ 0)
NAMED_CHARACTER_REFERENCE(1996,
                          /* s u */ 'p' _ 'h' _ 's' _ 'u' _ 'b' _ ';', 6, 0,
                          0x2ad7 _ 0)
NAMED_CHARACTER_REFERENCE(1997,
                          /* s u */ 'p' _ 'l' _ 'a' _ 'r' _ 'r' _ ';', 6, 0,
                          0x297b _ 0)
NAMED_CHARACTER_REFERENCE(1998,
                          /* s u */ 'p' _ 'm' _ 'u' _ 'l' _ 't' _ ';', 6, 0,
                          0x2ac2 _ 0)
NAMED_CHARACTER_REFERENCE(1999,
                          /* s u */ 'p' _ 'n' _ 'E' _ ';', 4, 0, 0x2acc _ 0)
NAMED_CHARACTER_REFERENCE(2000,
                          /* s u */ 'p' _ 'n' _ 'e' _ ';', 4, 0, 0x228b _ 0)
NAMED_CHARACTER_REFERENCE(2001,
                          /* s u */ 'p' _ 'p' _ 'l' _ 'u' _ 's' _ ';', 6, 0,
                          0x2ac0 _ 0)
NAMED_CHARACTER_REFERENCE(2002,
                          /* s u */ 'p' _ 's' _ 'e' _ 't' _ ';', 5, 0,
                          0x2283 _ 0)
NAMED_CHARACTER_REFERENCE(2003,
                          /* s u */ 'p' _ 's' _ 'e' _ 't' _ 'e' _ 'q' _ ';', 7,
                          0, 0x2287 _ 0)
NAMED_CHARACTER_REFERENCE(
    2004,
    /* s u */ 'p' _ 's' _ 'e' _ 't' _ 'e' _ 'q' _ 'q' _ ';', 8, 0, 0x2ac6 _ 0)
NAMED_CHARACTER_REFERENCE(
    2005,
    /* s u */ 'p' _ 's' _ 'e' _ 't' _ 'n' _ 'e' _ 'q' _ ';', 8, 0, 0x228b _ 0)
NAMED_CHARACTER_REFERENCE(
    2006,
    /* s u */ 'p' _ 's' _ 'e' _ 't' _ 'n' _ 'e' _ 'q' _ 'q' _ ';', 9, 0,
    0x2acc _ 0)
NAMED_CHARACTER_REFERENCE(2007,
                          /* s u */ 'p' _ 's' _ 'i' _ 'm' _ ';', 5, 0,
                          0x2ac8 _ 0)
NAMED_CHARACTER_REFERENCE(2008,
                          /* s u */ 'p' _ 's' _ 'u' _ 'b' _ ';', 5, 0,
                          0x2ad4 _ 0)
NAMED_CHARACTER_REFERENCE(2009,
                          /* s u */ 'p' _ 's' _ 'u' _ 'p' _ ';', 5, 0,
                          0x2ad6 _ 0)
NAMED_CHARACTER_REFERENCE(2010,
                          /* s w */ 'A' _ 'r' _ 'r' _ ';', 4, 0, 0x21d9 _ 0)
NAMED_CHARACTER_REFERENCE(2011,
                          /* s w */ 'a' _ 'r' _ 'h' _ 'k' _ ';', 5, 0,
                          0x2926 _ 0)
NAMED_CHARACTER_REFERENCE(2012,
                          /* s w */ 'a' _ 'r' _ 'r' _ ';', 4, 0, 0x2199 _ 0)
NAMED_CHARACTER_REFERENCE(2013,
                          /* s w */ 'a' _ 'r' _ 'r' _ 'o' _ 'w' _ ';', 6, 0,
                          0x2199 _ 0)
NAMED_CHARACTER_REFERENCE(2014,
                          /* s w */ 'n' _ 'w' _ 'a' _ 'r' _ ';', 5, 0,
                          0x292a _ 0)
NAMED_CHARACTER_REFERENCE(2015, /* s z */ 'l' _ 'i' _ 'g', 3, 0, 0x00df _ 0)
NAMED_CHARACTER_REFERENCE(2016,
                          /* s z */ 'l' _ 'i' _ 'g' _ ';', 4, 0, 0x00df _ 0)
NAMED_CHARACTER_REFERENCE(2017,
                          /* t a */ 'r' _ 'g' _ 'e' _ 't' _ ';', 5, 0,
                          0x2316 _ 0)
NAMED_CHARACTER_REFERENCE(2018, /* t a */ 'u' _ ';', 2, 0, 0x03c4 _ 0)
NAMED_CHARACTER_REFERENCE(2019, /* t b */ 'r' _ 'k' _ ';', 3, 0, 0x23b4 _ 0)
NAMED_CHARACTER_REFERENCE(2020,
                          /* t c */ 'a' _ 'r' _ 'o' _ 'n' _ ';', 5, 0,
                          0x0165 _ 0)
NAMED_CHARACTER_REFERENCE(2021,
                          /* t c */ 'e' _ 'd' _ 'i' _ 'l' _ ';', 5, 0,
                          0x0163 _ 0)
NAMED_CHARACTER_REFERENCE(2022, /* t c */ 'y' _ ';', 2, 0, 0x0442 _ 0)
NAMED_CHARACTER_REFERENCE(2023, /* t d */ 'o' _ 't' _ ';', 3, 0, 0x20db _ 0)
NAMED_CHARACTER_REFERENCE(2024,
                          /* t e */ 'l' _ 'r' _ 'e' _ 'c' _ ';', 5, 0,
                          0x2315 _ 0)
NAMED_CHARACTER_REFERENCE(2025, /* t f */ 'r' _ ';', 2, 0, 0xd835 _ 0xdd31)
NAMED_CHARACTER_REFERENCE(2026,
                          /* t h */ 'e' _ 'r' _ 'e' _ '4' _ ';', 5, 0,
                          0x2234 _ 0)
NAMED_CHARACTER_REFERENCE(
    2027,
    /* t h */ 'e' _ 'r' _ 'e' _ 'f' _ 'o' _ 'r' _ 'e' _ ';', 8, 0, 0x2234 _ 0)
NAMED_CHARACTER_REFERENCE(2028,
                          /* t h */ 'e' _ 't' _ 'a' _ ';', 4, 0, 0x03b8 _ 0)
NAMED_CHARACTER_REFERENCE(2029,
                          /* t h */ 'e' _ 't' _ 'a' _ 's' _ 'y' _ 'm' _ ';', 7,
                          0, 0x03d1 _ 0)
NAMED_CHARACTER_REFERENCE(2030,
                          /* t h */ 'e' _ 't' _ 'a' _ 'v' _ ';', 5, 0,
                          0x03d1 _ 0)
NAMED_CHARACTER_REFERENCE(
    2031,
    /* t h */ 'i' _ 'c' _ 'k' _ 'a' _ 'p' _ 'p' _ 'r' _ 'o' _ 'x' _ ';', 10, 0,
    0x2248 _ 0)
NAMED_CHARACTER_REFERENCE(2032,
                          /* t h */ 'i' _ 'c' _ 'k' _ 's' _ 'i' _ 'm' _ ';', 7,
                          0, 0x223c _ 0)
NAMED_CHARACTER_REFERENCE(2033,
                          /* t h */ 'i' _ 'n' _ 's' _ 'p' _ ';', 5, 0,
                          0x2009 _ 0)
NAMED_CHARACTER_REFERENCE(2034,
                          /* t h */ 'k' _ 'a' _ 'p' _ ';', 4, 0, 0x2248 _ 0)
NAMED_CHARACTER_REFERENCE(2035,
                          /* t h */ 'k' _ 's' _ 'i' _ 'm' _ ';', 5, 0,
                          0x223c _ 0)
NAMED_CHARACTER_REFERENCE(2036, /* t h */ 'o' _ 'r' _ 'n', 3, 0, 0x00fe _ 0)
NAMED_CHARACTER_REFERENCE(2037,
                          /* t h */ 'o' _ 'r' _ 'n' _ ';', 4, 0, 0x00fe _ 0)
NAMED_CHARACTER_REFERENCE(2038,
                          /* t i */ 'l' _ 'd' _ 'e' _ ';', 4, 0, 0x02dc _ 0)
NAMED_CHARACTER_REFERENCE(2039, /* t i */ 'm' _ 'e' _ 's', 3, 0, 0x00d7 _ 0)
NAMED_CHARACTER_REFERENCE(2040,
                          /* t i */ 'm' _ 'e' _ 's' _ ';', 4, 0, 0x00d7 _ 0)
NAMED_CHARACTER_REFERENCE(2041,
                          /* t i */ 'm' _ 'e' _ 's' _ 'b' _ ';', 5, 0,
                          0x22a0 _ 0)
NAMED_CHARACTER_REFERENCE(2042,
                          /* t i */ 'm' _ 'e' _ 's' _ 'b' _ 'a' _ 'r' _ ';', 7,
                          0, 0x2a31 _ 0)
NAMED_CHARACTER_REFERENCE(2043,
                          /* t i */ 'm' _ 'e' _ 's' _ 'd' _ ';', 5, 0,
                          0x2a30 _ 0)
NAMED_CHARACTER_REFERENCE(2044, /* t i */ 'n' _ 't' _ ';', 3, 0, 0x222d _ 0)
NAMED_CHARACTER_REFERENCE(2045, /* t o */ 'e' _ 'a' _ ';', 3, 0, 0x2928 _ 0)
NAMED_CHARACTER_REFERENCE(2046, /* t o */ 'p' _ ';', 2, 0, 0x22a4 _ 0)
NAMED_CHARACTER_REFERENCE(2047,
                          /* t o */ 'p' _ 'b' _ 'o' _ 't' _ ';', 5, 0,
                          0x2336 _ 0)
NAMED_CHARACTER_REFERENCE(2048,
                          /* t o */ 'p' _ 'c' _ 'i' _ 'r' _ ';', 5, 0,
                          0x2af1 _ 0)
NAMED_CHARACTER_REFERENCE(2049,
                          /* t o */ 'p' _ 'f' _ ';', 3, 0, 0xd835 _ 0xdd65)
NAMED_CHARACTER_REFERENCE(2050,
                          /* t o */ 'p' _ 'f' _ 'o' _ 'r' _ 'k' _ ';', 6, 0,
                          0x2ada _ 0)
NAMED_CHARACTER_REFERENCE(2051, /* t o */ 's' _ 'a' _ ';', 3, 0, 0x2929 _ 0)
NAMED_CHARACTER_REFERENCE(2052,
                          /* t p */ 'r' _ 'i' _ 'm' _ 'e' _ ';', 5, 0,
                          0x2034 _ 0)
NAMED_CHARACTER_REFERENCE(2053,
                          /* t r */ 'a' _ 'd' _ 'e' _ ';', 4, 0, 0x2122 _ 0)
NAMED_CHARACTER_REFERENCE(2054,
                          /* t r */ 'i' _ 'a' _ 'n' _ 'g' _ 'l' _ 'e' _ ';', 7,
                          0, 0x25b5 _ 0)
NAMED_CHARACTER_REFERENCE(
    2055,
    /* t r */ 'i' _ 'a' _ 'n' _ 'g' _ 'l' _ 'e' _ 'd' _ 'o' _ 'w' _ 'n' _ ';',
    11, 0, 0x25bf _ 0)
NAMED_CHARACTER_REFERENCE(
    2056,
    /* t r */ 'i' _ 'a' _ 'n' _ 'g' _ 'l' _ 'e' _ 'l' _ 'e' _ 'f' _ 't' _ ';',
    11, 0, 0x25c3 _ 0)
NAMED_CHARACTER_REFERENCE(
    2057,
    /* t r */
    'i' _ 'a' _ 'n' _ 'g' _ 'l' _ 'e' _ 'l' _ 'e' _ 'f' _ 't' _ 'e' _ 'q' _ ';',
    13, 0, 0x22b4 _ 0)
NAMED_CHARACTER_REFERENCE(
    2058,
    /* t r */ 'i' _ 'a' _ 'n' _ 'g' _ 'l' _ 'e' _ 'q' _ ';', 8, 0, 0x225c _ 0)
NAMED_CHARACTER_REFERENCE(
    2059,
    /* t r */
    'i' _ 'a' _ 'n' _ 'g' _ 'l' _ 'e' _ 'r' _ 'i' _ 'g' _ 'h' _ 't' _ ';', 12,
    0, 0x25b9 _ 0)
NAMED_CHARACTER_REFERENCE(
    2060,
    /* t r */
    'i' _ 'a' _ 'n' _ 'g' _ 'l' _ 'e' _ 'r' _ 'i' _ 'g' _ 'h' _ 't' _ 'e' _ 'q' _ ';',
    14, 0, 0x22b5 _ 0)
NAMED_CHARACTER_REFERENCE(2061,
                          /* t r */ 'i' _ 'd' _ 'o' _ 't' _ ';', 5, 0,
                          0x25ec _ 0)
NAMED_CHARACTER_REFERENCE(2062, /* t r */ 'i' _ 'e' _ ';', 3, 0, 0x225c _ 0)
NAMED_CHARACTER_REFERENCE(2063,
                          /* t r */ 'i' _ 'm' _ 'i' _ 'n' _ 'u' _ 's' _ ';', 7,
                          0, 0x2a3a _ 0)
NAMED_CHARACTER_REFERENCE(2064,
                          /* t r */ 'i' _ 'p' _ 'l' _ 'u' _ 's' _ ';', 6, 0,
                          0x2a39 _ 0)
NAMED_CHARACTER_REFERENCE(2065,
                          /* t r */ 'i' _ 's' _ 'b' _ ';', 4, 0, 0x29cd _ 0)
NAMED_CHARACTER_REFERENCE(2066,
                          /* t r */ 'i' _ 't' _ 'i' _ 'm' _ 'e' _ ';', 6, 0,
                          0x2a3b _ 0)
NAMED_CHARACTER_REFERENCE(2067,
                          /* t r */ 'p' _ 'e' _ 'z' _ 'i' _ 'u' _ 'm' _ ';', 7,
                          0, 0x23e2 _ 0)
NAMED_CHARACTER_REFERENCE(2068,
                          /* t s */ 'c' _ 'r' _ ';', 3, 0, 0xd835 _ 0xdcc9)
NAMED_CHARACTER_REFERENCE(2069, /* t s */ 'c' _ 'y' _ ';', 3, 0, 0x0446 _ 0)
NAMED_CHARACTER_REFERENCE(2070,
                          /* t s */ 'h' _ 'c' _ 'y' _ ';', 4, 0, 0x045b _ 0)
NAMED_CHARACTER_REFERENCE(2071,
                          /* t s */ 't' _ 'r' _ 'o' _ 'k' _ ';', 5, 0,
                          0x0167 _ 0)
NAMED_CHARACTER_REFERENCE(2072,
                          /* t w */ 'i' _ 'x' _ 't' _ ';', 4, 0, 0x226c _ 0)
NAMED_CHARACTER_REFERENCE(
    2073,
    /* t w */
    'o' _ 'h' _ 'e' _ 'a' _ 'd' _ 'l' _ 'e' _ 'f' _ 't' _ 'a' _ 'r' _ 'r' _ 'o' _ 'w' _ ';',
    15, 0, 0x219e _ 0)
NAMED_CHARACTER_REFERENCE(
    2074,
    /* t w */
    'o' _ 'h' _ 'e' _ 'a' _ 'd' _ 'r' _ 'i' _ 'g' _ 'h' _ 't' _ 'a' _ 'r' _ 'r' _ 'o' _ 'w' _ ';',
    16, 0, 0x21a0 _ 0)
NAMED_CHARACTER_REFERENCE(2075, /* u A */ 'r' _ 'r' _ ';', 3, 0, 0x21d1 _ 0)
NAMED_CHARACTER_REFERENCE(2076, /* u H */ 'a' _ 'r' _ ';', 3, 0, 0x2963 _ 0)
NAMED_CHARACTER_REFERENCE(2077,
                          /* u a */ 'c' _ 'u' _ 't' _ 'e', 4, 0, 0x00fa _ 0)
NAMED_CHARACTER_REFERENCE(2078,
                          /* u a */ 'c' _ 'u' _ 't' _ 'e' _ ';', 5, 0,
                          0x00fa _ 0)
NAMED_CHARACTER_REFERENCE(2079, /* u a */ 'r' _ 'r' _ ';', 3, 0, 0x2191 _ 0)
NAMED_CHARACTER_REFERENCE(2080,
                          /* u b */ 'r' _ 'c' _ 'y' _ ';', 4, 0, 0x045e _ 0)
NAMED_CHARACTER_REFERENCE(2081,
                          /* u b */ 'r' _ 'e' _ 'v' _ 'e' _ ';', 5, 0,
                          0x016d _ 0)
NAMED_CHARACTER_REFERENCE(2082, /* u c */ 'i' _ 'r' _ 'c', 3, 0, 0x00fb _ 0)
NAMED_CHARACTER_REFERENCE(2083,
                          /* u c */ 'i' _ 'r' _ 'c' _ ';', 4, 0, 0x00fb _ 0)
NAMED_CHARACTER_REFERENCE(2084, /* u c */ 'y' _ ';', 2, 0, 0x0443 _ 0)
NAMED_CHARACTER_REFERENCE(2085,
                          /* u d */ 'a' _ 'r' _ 'r' _ ';', 4, 0, 0x21c5 _ 0)
NAMED_CHARACTER_REFERENCE(2086,
                          /* u d */ 'b' _ 'l' _ 'a' _ 'c' _ ';', 5, 0,
                          0x0171 _ 0)
NAMED_CHARACTER_REFERENCE(2087,
                          /* u d */ 'h' _ 'a' _ 'r' _ ';', 4, 0, 0x296e _ 0)
NAMED_CHARACTER_REFERENCE(2088,
                          /* u f */ 'i' _ 's' _ 'h' _ 't' _ ';', 5, 0,
                          0x297e _ 0)
NAMED_CHARACTER_REFERENCE(2089, /* u f */ 'r' _ ';', 2, 0, 0xd835 _ 0xdd32)
NAMED_CHARACTER_REFERENCE(2090,
                          /* u g */ 'r' _ 'a' _ 'v' _ 'e', 4, 0, 0x00f9 _ 0)
NAMED_CHARACTER_REFERENCE(2091,
                          /* u g */ 'r' _ 'a' _ 'v' _ 'e' _ ';', 5, 0,
                          0x00f9 _ 0)
NAMED_CHARACTER_REFERENCE(2092,
                          /* u h */ 'a' _ 'r' _ 'l' _ ';', 4, 0, 0x21bf _ 0)
NAMED_CHARACTER_REFERENCE(2093,
                          /* u h */ 'a' _ 'r' _ 'r' _ ';', 4, 0, 0x21be _ 0)
NAMED_CHARACTER_REFERENCE(2094,
                          /* u h */ 'b' _ 'l' _ 'k' _ ';', 4, 0, 0x2580 _ 0)
NAMED_CHARACTER_REFERENCE(2095,
                          /* u l */ 'c' _ 'o' _ 'r' _ 'n' _ ';', 5, 0,
                          0x231c _ 0)
NAMED_CHARACTER_REFERENCE(2096,
                          /* u l */ 'c' _ 'o' _ 'r' _ 'n' _ 'e' _ 'r' _ ';', 7,
                          0, 0x231c _ 0)
NAMED_CHARACTER_REFERENCE(2097,
                          /* u l */ 'c' _ 'r' _ 'o' _ 'p' _ ';', 5, 0,
                          0x230f _ 0)
NAMED_CHARACTER_REFERENCE(2098,
                          /* u l */ 't' _ 'r' _ 'i' _ ';', 4, 0, 0x25f8 _ 0)
NAMED_CHARACTER_REFERENCE(2099,
                          /* u m */ 'a' _ 'c' _ 'r' _ ';', 4, 0, 0x016b _ 0)
NAMED_CHARACTER_REFERENCE(2100, /* u m */ 'l', 1, 0, 0x00a8 _ 0)
NAMED_CHARACTER_REFERENCE(2101, /* u m */ 'l' _ ';', 2, 0, 0x00a8 _ 0)
NAMED_CHARACTER_REFERENCE(2102,
                          /* u o */ 'g' _ 'o' _ 'n' _ ';', 4, 0, 0x0173 _ 0)
NAMED_CHARACTER_REFERENCE(2103,
                          /* u o */ 'p' _ 'f' _ ';', 3, 0, 0xd835 _ 0xdd66)
NAMED_CHARACTER_REFERENCE(2104,
                          /* u p */ 'a' _ 'r' _ 'r' _ 'o' _ 'w' _ ';', 6, 0,
                          0x2191 _ 0)
NAMED_CHARACTER_REFERENCE(
    2105,
    /* u p */ 'd' _ 'o' _ 'w' _ 'n' _ 'a' _ 'r' _ 'r' _ 'o' _ 'w' _ ';', 10, 0,
    0x2195 _ 0)
NAMED_CHARACTER_REFERENCE(
    2106,
    /* u p */
    'h' _ 'a' _ 'r' _ 'p' _ 'o' _ 'o' _ 'n' _ 'l' _ 'e' _ 'f' _ 't' _ ';', 12,
    0, 0x21bf _ 0)
NAMED_CHARACTER_REFERENCE(
    2107,
    /* u p */
    'h' _ 'a' _ 'r' _ 'p' _ 'o' _ 'o' _ 'n' _ 'r' _ 'i' _ 'g' _ 'h' _ 't' _ ';',
    13, 0, 0x21be _ 0)
NAMED_CHARACTER_REFERENCE(2108,
                          /* u p */ 'l' _ 'u' _ 's' _ ';', 4, 0, 0x228e _ 0)
NAMED_CHARACTER_REFERENCE(2109, /* u p */ 's' _ 'i' _ ';', 3, 0, 0x03c5 _ 0)
NAMED_CHARACTER_REFERENCE(2110,
                          /* u p */ 's' _ 'i' _ 'h' _ ';', 4, 0, 0x03d2 _ 0)
NAMED_CHARACTER_REFERENCE(2111,
                          /* u p */ 's' _ 'i' _ 'l' _ 'o' _ 'n' _ ';', 6, 0,
                          0x03c5 _ 0)
NAMED_CHARACTER_REFERENCE(
    2112,
    /* u p */ 'u' _ 'p' _ 'a' _ 'r' _ 'r' _ 'o' _ 'w' _ 's' _ ';', 9, 0,
    0x21c8 _ 0)
NAMED_CHARACTER_REFERENCE(2113,
                          /* u r */ 'c' _ 'o' _ 'r' _ 'n' _ ';', 5, 0,
                          0x231d _ 0)
NAMED_CHARACTER_REFERENCE(2114,
                          /* u r */ 'c' _ 'o' _ 'r' _ 'n' _ 'e' _ 'r' _ ';', 7,
                          0, 0x231d _ 0)
NAMED_CHARACTER_REFERENCE(2115,
                          /* u r */ 'c' _ 'r' _ 'o' _ 'p' _ ';', 5, 0,
                          0x230e _ 0)
NAMED_CHARACTER_REFERENCE(2116,
                          /* u r */ 'i' _ 'n' _ 'g' _ ';', 4, 0, 0x016f _ 0)
NAMED_CHARACTER_REFERENCE(2117,
                          /* u r */ 't' _ 'r' _ 'i' _ ';', 4, 0, 0x25f9 _ 0)
NAMED_CHARACTER_REFERENCE(2118,
                          /* u s */ 'c' _ 'r' _ ';', 3, 0, 0xd835 _ 0xdcca)
NAMED_CHARACTER_REFERENCE(2119,
                          /* u t */ 'd' _ 'o' _ 't' _ ';', 4, 0, 0x22f0 _ 0)
NAMED_CHARACTER_REFERENCE(2120,
                          /* u t */ 'i' _ 'l' _ 'd' _ 'e' _ ';', 5, 0,
                          0x0169 _ 0)
NAMED_CHARACTER_REFERENCE(2121, /* u t */ 'r' _ 'i' _ ';', 3, 0, 0x25b5 _ 0)
NAMED_CHARACTER_REFERENCE(2122,
                          /* u t */ 'r' _ 'i' _ 'f' _ ';', 4, 0, 0x25b4 _ 0)
NAMED_CHARACTER_REFERENCE(2123,
                          /* u u */ 'a' _ 'r' _ 'r' _ ';', 4, 0, 0x21c8 _ 0)
NAMED_CHARACTER_REFERENCE(2124, /* u u */ 'm' _ 'l', 2, 0, 0x00fc _ 0)
NAMED_CHARACTER_REFERENCE(2125, /* u u */ 'm' _ 'l' _ ';', 3, 0, 0x00fc _ 0)
NAMED_CHARACTER_REFERENCE(2126,
                          /* u w */ 'a' _ 'n' _ 'g' _ 'l' _ 'e' _ ';', 6, 0,
                          0x29a7 _ 0)
NAMED_CHARACTER_REFERENCE(2127, /* v A */ 'r' _ 'r' _ ';', 3, 0, 0x21d5 _ 0)
NAMED_CHARACTER_REFERENCE(2128, /* v B */ 'a' _ 'r' _ ';', 3, 0, 0x2ae8 _ 0)
NAMED_CHARACTER_REFERENCE(2129,
                          /* v B */ 'a' _ 'r' _ 'v' _ ';', 4, 0, 0x2ae9 _ 0)
NAMED_CHARACTER_REFERENCE(2130,
                          /* v D */ 'a' _ 's' _ 'h' _ ';', 4, 0, 0x22a8 _ 0)
NAMED_CHARACTER_REFERENCE(2131,
                          /* v a */ 'n' _ 'g' _ 'r' _ 't' _ ';', 5, 0,
                          0x299c _ 0)
NAMED_CHARACTER_REFERENCE(
    2132,
    /* v a */ 'r' _ 'e' _ 'p' _ 's' _ 'i' _ 'l' _ 'o' _ 'n' _ ';', 9, 0,
    0x03f5 _ 0)
NAMED_CHARACTER_REFERENCE(2133,
                          /* v a */ 'r' _ 'k' _ 'a' _ 'p' _ 'p' _ 'a' _ ';', 7,
                          0, 0x03f0 _ 0)
NAMED_CHARACTER_REFERENCE(
    2134,
    /* v a */ 'r' _ 'n' _ 'o' _ 't' _ 'h' _ 'i' _ 'n' _ 'g' _ ';', 9, 0,
    0x2205 _ 0)
NAMED_CHARACTER_REFERENCE(2135,
                          /* v a */ 'r' _ 'p' _ 'h' _ 'i' _ ';', 5, 0,
                          0x03d5 _ 0)
NAMED_CHARACTER_REFERENCE(2136,
                          /* v a */ 'r' _ 'p' _ 'i' _ ';', 4, 0, 0x03d6 _ 0)
NAMED_CHARACTER_REFERENCE(
    2137,
    /* v a */ 'r' _ 'p' _ 'r' _ 'o' _ 'p' _ 't' _ 'o' _ ';', 8, 0, 0x221d _ 0)
NAMED_CHARACTER_REFERENCE(2138, /* v a */ 'r' _ 'r' _ ';', 3, 0, 0x2195 _ 0)
NAMED_CHARACTER_REFERENCE(2139,
                          /* v a */ 'r' _ 'r' _ 'h' _ 'o' _ ';', 5, 0,
                          0x03f1 _ 0)
NAMED_CHARACTER_REFERENCE(2140,
                          /* v a */ 'r' _ 's' _ 'i' _ 'g' _ 'm' _ 'a' _ ';', 7,
                          0, 0x03c2 _ 0)
NAMED_CHARACTER_REFERENCE(
    2141,
    /* v a */ 'r' _ 's' _ 'u' _ 'b' _ 's' _ 'e' _ 't' _ 'n' _ 'e' _ 'q' _ ';',
    11, 0, 0x228a _ 0xfe00)
NAMED_CHARACTER_REFERENCE(
    2142,
    /* v a */
    'r' _ 's' _ 'u' _ 'b' _ 's' _ 'e' _ 't' _ 'n' _ 'e' _ 'q' _ 'q' _ ';', 12,
    0, 0x2acb _ 0xfe00)
NAMED_CHARACTER_REFERENCE(
    2143,
    /* v a */ 'r' _ 's' _ 'u' _ 'p' _ 's' _ 'e' _ 't' _ 'n' _ 'e' _ 'q' _ ';',
    11, 0, 0x228b _ 0xfe00)
NAMED_CHARACTER_REFERENCE(
    2144,
    /* v a */
    'r' _ 's' _ 'u' _ 'p' _ 's' _ 'e' _ 't' _ 'n' _ 'e' _ 'q' _ 'q' _ ';', 12,
    0, 0x2acc _ 0xfe00)
NAMED_CHARACTER_REFERENCE(2145,
                          /* v a */ 'r' _ 't' _ 'h' _ 'e' _ 't' _ 'a' _ ';', 7,
                          0, 0x03d1 _ 0)
NAMED_CHARACTER_REFERENCE(
    2146,
    /* v a */
    'r' _ 't' _ 'r' _ 'i' _ 'a' _ 'n' _ 'g' _ 'l' _ 'e' _ 'l' _ 'e' _ 'f' _ 't' _ ';',
    14, 0, 0x22b2 _ 0)
NAMED_CHARACTER_REFERENCE(
    2147,
    /* v a */
    'r' _ 't' _ 'r' _ 'i' _ 'a' _ 'n' _ 'g' _ 'l' _ 'e' _ 'r' _ 'i' _ 'g' _ 'h' _ 't' _ ';',
    15, 0, 0x22b3 _ 0)
NAMED_CHARACTER_REFERENCE(2148, /* v c */ 'y' _ ';', 2, 0, 0x0432 _ 0)
NAMED_CHARACTER_REFERENCE(2149,
                          /* v d */ 'a' _ 's' _ 'h' _ ';', 4, 0, 0x22a2 _ 0)
NAMED_CHARACTER_REFERENCE(2150, /* v e */ 'e' _ ';', 2, 0, 0x2228 _ 0)
NAMED_CHARACTER_REFERENCE(2151,
                          /* v e */ 'e' _ 'b' _ 'a' _ 'r' _ ';', 5, 0,
                          0x22bb _ 0)
NAMED_CHARACTER_REFERENCE(2152,
                          /* v e */ 'e' _ 'e' _ 'q' _ ';', 4, 0, 0x225a _ 0)
NAMED_CHARACTER_REFERENCE(2153,
                          /* v e */ 'l' _ 'l' _ 'i' _ 'p' _ ';', 5, 0,
                          0x22ee _ 0)
NAMED_CHARACTER_REFERENCE(2154,
                          /* v e */ 'r' _ 'b' _ 'a' _ 'r' _ ';', 5, 0,
                          0x007c _ 0)
NAMED_CHARACTER_REFERENCE(2155, /* v e */ 'r' _ 't' _ ';', 3, 0, 0x007c _ 0)
NAMED_CHARACTER_REFERENCE(2156, /* v f */ 'r' _ ';', 2, 0, 0xd835 _ 0xdd33)
NAMED_CHARACTER_REFERENCE(2157,
                          /* v l */ 't' _ 'r' _ 'i' _ ';', 4, 0, 0x22b2 _ 0)
NAMED_CHARACTER_REFERENCE(2158,
                          /* v n */ 's' _ 'u' _ 'b' _ ';', 4, 0,
                          0x2282 _ 0x20d2)
NAMED_CHARACTER_REFERENCE(2159,
                          /* v n */ 's' _ 'u' _ 'p' _ ';', 4, 0,
                          0x2283 _ 0x20d2)
NAMED_CHARACTER_REFERENCE(2160,
                          /* v o */ 'p' _ 'f' _ ';', 3, 0, 0xd835 _ 0xdd67)
NAMED_CHARACTER_REFERENCE(2161,
                          /* v p */ 'r' _ 'o' _ 'p' _ ';', 4, 0, 0x221d _ 0)
NAMED_CHARACTER_REFERENCE(2162,
                          /* v r */ 't' _ 'r' _ 'i' _ ';', 4, 0, 0x22b3 _ 0)
NAMED_CHARACTER_REFERENCE(2163,
                          /* v s */ 'c' _ 'r' _ ';', 3, 0, 0xd835 _ 0xdccb)
NAMED_CHARACTER_REFERENCE(2164,
                          /* v s */ 'u' _ 'b' _ 'n' _ 'E' _ ';', 5, 0,
                          0x2acb _ 0xfe00)
NAMED_CHARACTER_REFERENCE(2165,
                          /* v s */ 'u' _ 'b' _ 'n' _ 'e' _ ';', 5, 0,
                          0x228a _ 0xfe00)
NAMED_CHARACTER_REFERENCE(2166,
                          /* v s */ 'u' _ 'p' _ 'n' _ 'E' _ ';', 5, 0,
                          0x2acc _ 0xfe00)
NAMED_CHARACTER_REFERENCE(2167,
                          /* v s */ 'u' _ 'p' _ 'n' _ 'e' _ ';', 5, 0,
                          0x228b _ 0xfe00)
NAMED_CHARACTER_REFERENCE(2168,
                          /* v z */ 'i' _ 'g' _ 'z' _ 'a' _ 'g' _ ';', 6, 0,
                          0x299a _ 0)
NAMED_CHARACTER_REFERENCE(2169,
                          /* w c */ 'i' _ 'r' _ 'c' _ ';', 4, 0, 0x0175 _ 0)
NAMED_CHARACTER_REFERENCE(2170,
                          /* w e */ 'd' _ 'b' _ 'a' _ 'r' _ ';', 5, 0,
                          0x2a5f _ 0)
NAMED_CHARACTER_REFERENCE(2171,
                          /* w e */ 'd' _ 'g' _ 'e' _ ';', 4, 0, 0x2227 _ 0)
NAMED_CHARACTER_REFERENCE(2172,
                          /* w e */ 'd' _ 'g' _ 'e' _ 'q' _ ';', 5, 0,
                          0x2259 _ 0)
NAMED_CHARACTER_REFERENCE(2173,
                          /* w e */ 'i' _ 'e' _ 'r' _ 'p' _ ';', 5, 0,
                          0x2118 _ 0)
NAMED_CHARACTER_REFERENCE(2174, /* w f */ 'r' _ ';', 2, 0, 0xd835 _ 0xdd34)
NAMED_CHARACTER_REFERENCE(2175,
                          /* w o */ 'p' _ 'f' _ ';', 3, 0, 0xd835 _ 0xdd68)
NAMED_CHARACTER_REFERENCE(2176, /* w p */ ';', 1, 0, 0x2118 _ 0)
NAMED_CHARACTER_REFERENCE(2177, /* w r */ ';', 1, 0, 0x2240 _ 0)
NAMED_CHARACTER_REFERENCE(2178,
                          /* w r */ 'e' _ 'a' _ 't' _ 'h' _ ';', 5, 0,
                          0x2240 _ 0)
NAMED_CHARACTER_REFERENCE(2179,
                          /* w s */ 'c' _ 'r' _ ';', 3, 0, 0xd835 _ 0xdccc)
NAMED_CHARACTER_REFERENCE(2180, /* x c */ 'a' _ 'p' _ ';', 3, 0, 0x22c2 _ 0)
NAMED_CHARACTER_REFERENCE(2181,
                          /* x c */ 'i' _ 'r' _ 'c' _ ';', 4, 0, 0x25ef _ 0)
NAMED_CHARACTER_REFERENCE(2182, /* x c */ 'u' _ 'p' _ ';', 3, 0, 0x22c3 _ 0)
NAMED_CHARACTER_REFERENCE(2183,
                          /* x d */ 't' _ 'r' _ 'i' _ ';', 4, 0, 0x25bd _ 0)
NAMED_CHARACTER_REFERENCE(2184, /* x f */ 'r' _ ';', 2, 0, 0xd835 _ 0xdd35)
NAMED_CHARACTER_REFERENCE(2185,
                          /* x h */ 'A' _ 'r' _ 'r' _ ';', 4, 0, 0x27fa _ 0)
NAMED_CHARACTER_REFERENCE(2186,
                          /* x h */ 'a' _ 'r' _ 'r' _ ';', 4, 0, 0x27f7 _ 0)
NAMED_CHARACTER_REFERENCE(2187, /* x i */ ';', 1, 0, 0x03be _ 0)
NAMED_CHARACTER_REFERENCE(2188,
                          /* x l */ 'A' _ 'r' _ 'r' _ ';', 4, 0, 0x27f8 _ 0)
NAMED_CHARACTER_REFERENCE(2189,
                          /* x l */ 'a' _ 'r' _ 'r' _ ';', 4, 0, 0x27f5 _ 0)
NAMED_CHARACTER_REFERENCE(2190, /* x m */ 'a' _ 'p' _ ';', 3, 0, 0x27fc _ 0)
NAMED_CHARACTER_REFERENCE(2191, /* x n */ 'i' _ 's' _ ';', 3, 0, 0x22fb _ 0)
NAMED_CHARACTER_REFERENCE(2192,
                          /* x o */ 'd' _ 'o' _ 't' _ ';', 4, 0, 0x2a00 _ 0)
NAMED_CHARACTER_REFERENCE(2193,
                          /* x o */ 'p' _ 'f' _ ';', 3, 0, 0xd835 _ 0xdd69)
NAMED_CHARACTER_REFERENCE(2194,
                          /* x o */ 'p' _ 'l' _ 'u' _ 's' _ ';', 5, 0,
                          0x2a01 _ 0)
NAMED_CHARACTER_REFERENCE(2195,
                          /* x o */ 't' _ 'i' _ 'm' _ 'e' _ ';', 5, 0,
                          0x2a02 _ 0)
NAMED_CHARACTER_REFERENCE(2196,
                          /* x r */ 'A' _ 'r' _ 'r' _ ';', 4, 0, 0x27f9 _ 0)
NAMED_CHARACTER_REFERENCE(2197,
                          /* x r */ 'a' _ 'r' _ 'r' _ ';', 4, 0, 0x27f6 _ 0)
NAMED_CHARACTER_REFERENCE(2198,
                          /* x s */ 'c' _ 'r' _ ';', 3, 0, 0xd835 _ 0xdccd)
NAMED_CHARACTER_REFERENCE(2199,
                          /* x s */ 'q' _ 'c' _ 'u' _ 'p' _ ';', 5, 0,
                          0x2a06 _ 0)
NAMED_CHARACTER_REFERENCE(2200,
                          /* x u */ 'p' _ 'l' _ 'u' _ 's' _ ';', 5, 0,
                          0x2a04 _ 0)
NAMED_CHARACTER_REFERENCE(2201,
                          /* x u */ 't' _ 'r' _ 'i' _ ';', 4, 0, 0x25b3 _ 0)
NAMED_CHARACTER_REFERENCE(2202, /* x v */ 'e' _ 'e' _ ';', 3, 0, 0x22c1 _ 0)
NAMED_CHARACTER_REFERENCE(2203,
                          /* x w */ 'e' _ 'd' _ 'g' _ 'e' _ ';', 5, 0,
                          0x22c0 _ 0)
NAMED_CHARACTER_REFERENCE(2204,
                          /* y a */ 'c' _ 'u' _ 't' _ 'e', 4, 0, 0x00fd _ 0)
NAMED_CHARACTER_REFERENCE(2205,
                          /* y a */ 'c' _ 'u' _ 't' _ 'e' _ ';', 5, 0,
                          0x00fd _ 0)
NAMED_CHARACTER_REFERENCE(2206, /* y a */ 'c' _ 'y' _ ';', 3, 0, 0x044f _ 0)
NAMED_CHARACTER_REFERENCE(2207,
                          /* y c */ 'i' _ 'r' _ 'c' _ ';', 4, 0, 0x0177 _ 0)
NAMED_CHARACTER_REFERENCE(2208, /* y c */ 'y' _ ';', 2, 0, 0x044b _ 0)
NAMED_CHARACTER_REFERENCE(2209, /* y e */ 'n', 1, 0, 0x00a5 _ 0)
NAMED_CHARACTER_REFERENCE(2210, /* y e */ 'n' _ ';', 2, 0, 0x00a5 _ 0)
NAMED_CHARACTER_REFERENCE(2211, /* y f */ 'r' _ ';', 2, 0, 0xd835 _ 0xdd36)
NAMED_CHARACTER_REFERENCE(2212, /* y i */ 'c' _ 'y' _ ';', 3, 0, 0x0457 _ 0)
NAMED_CHARACTER_REFERENCE(2213,
                          /* y o */ 'p' _ 'f' _ ';', 3, 0, 0xd835 _ 0xdd6a)
NAMED_CHARACTER_REFERENCE(2214,
                          /* y s */ 'c' _ 'r' _ ';', 3, 0, 0xd835 _ 0xdcce)
NAMED_CHARACTER_REFERENCE(2215, /* y u */ 'c' _ 'y' _ ';', 3, 0, 0x044e _ 0)
NAMED_CHARACTER_REFERENCE(2216, /* y u */ 'm' _ 'l', 2, 0, 0x00ff _ 0)
NAMED_CHARACTER_REFERENCE(2217, /* y u */ 'm' _ 'l' _ ';', 3, 0, 0x00ff _ 0)
NAMED_CHARACTER_REFERENCE(2218,
                          /* z a */ 'c' _ 'u' _ 't' _ 'e' _ ';', 5, 0,
                          0x017a _ 0)
NAMED_CHARACTER_REFERENCE(2219,
                          /* z c */ 'a' _ 'r' _ 'o' _ 'n' _ ';', 5, 0,
                          0x017e _ 0)
NAMED_CHARACTER_REFERENCE(2220, /* z c */ 'y' _ ';', 2, 0, 0x0437 _ 0)
NAMED_CHARACTER_REFERENCE(2221, /* z d */ 'o' _ 't' _ ';', 3, 0, 0x017c _ 0)
NAMED_CHARACTER_REFERENCE(2222,
                          /* z e */ 'e' _ 't' _ 'r' _ 'f' _ ';', 5, 0,
                          0x2128 _ 0)
NAMED_CHARACTER_REFERENCE(2223, /* z e */ 't' _ 'a' _ ';', 3, 0, 0x03b6 _ 0)
NAMED_CHARACTER_REFERENCE(2224, /* z f */ 'r' _ ';', 2, 0, 0xd835 _ 0xdd37)
NAMED_CHARACTER_REFERENCE(2225, /* z h */ 'c' _ 'y' _ ';', 3, 0, 0x0436 _ 0)
NAMED_CHARACTER_REFERENCE(2226,
                          /* z i */ 'g' _ 'r' _ 'a' _ 'r' _ 'r' _ ';', 6, 0,
                          0x21dd _ 0)
NAMED_CHARACTER_REFERENCE(2227,
                          /* z o */ 'p' _ 'f' _ ';', 3, 0, 0xd835 _ 0xdd6b)
NAMED_CHARACTER_REFERENCE(2228,
                          /* z s */ 'c' _ 'r' _ ';', 3, 0, 0xd835 _ 0xdccf)
NAMED_CHARACTER_REFERENCE(2229, /* z w */ 'j' _ ';', 2, 0, 0x200d _ 0)
NAMED_CHARACTER_REFERENCE(2230, /* z w */ 'n' _ 'j' _ ';', 3, 0, 0x200c _ 0)

#undef _
