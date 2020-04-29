// Note: this file tests only valid syntax (of default pref files, not user
// pref files). See modules/libpref/test/gtest/Parser.cpp for tests of invalid
// syntax.

#
# comment
    # comment £
//
// comment
        // comment £
/**/
   /* comment £ */
/* comment
 * and
some
   # more
   // comment */
// comment /*
# comment /*
/* /* /* /* /* ...no nesting of C-style comments... */

// The following four lines have whitespace: \t, \v, \f, \r
	




// This comment has the same whitespace: 	
# This comment has other stuff: \n \r \t \v \f \r \a \b \? \' \" \\ \@
/* This comment has more stuff: \x61 \u0061 \u1234 \uXYZ */

/*0*/ pref /*1*/ ( /*2*/ "comment1" /*3*/ , /*4*/ true /*5*/ ) /*6*/ ; /*7*/

pref # foo
( // foo
"comment2" /*
foo
*/,/*foo*/
true#foo
)//
; /*7*/

pref
    (
     "spaced-out"
                 ,
                   true
                       )
                        ;

pref("pref", true);
sticky_pref("sticky_pref", true);
user_pref("user_pref", true);
pref("sticky_pref2", true, sticky);
pref("locked_pref", true, locked);
pref("locked_sticky_pref", true, locked, sticky,sticky,
     locked, locked, locked);

pref("bool.true", true);
pref("bool.false", false);

pref("int.0", 0);
pref("int.1", 1);
pref("int.123", 123);
pref("int.+234", +234);
pref("int.+  345", +  345);
// Note that both the prefname and value have tabs in them
pref("int.-0", -0);
pref("int.-1", -1);
pref("int.- /* hmm */	456", - /* hmm */	456);
pref("int.-\n567", -
567);
pref("int.INT_MAX-1",  2147483646);
pref("int.INT_MAX",    2147483647);
pref("int.INT_MIN+2", -2147483646);
pref("int.INT_MIN+1", -2147483647);
pref("int.INT_MIN",   -2147483648);
//pref("int.overflow.max", 2147483648);           // overflows to -2147483648
//pref("int.overflow.min", -2147483649);          // overflows to 2147483647
//pref("int.overflow.other", 4000000000);         // overflows to -294967296
//pref("int.overflow.another", 5000000000000000); // overflows to 937459712

pref("string.empty", "");
pref("string.abc", "abc");
pref("string.long", "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
pref('string.single-quotes', '"abc"');
pref("string.double-quotes", "'abc'");
pref("string.weird-chars", " 	    ");
pref("string.escapes", "\" \' \\ \n \r");
pref("string.x-escapes1", "Mozilla0\x4d\x6F\x7a\x69\x6c\x6C\x610");
pref("string.x-escapes2", "A\x41 A_umlaut\xc4 y_umlaut\xff");
pref("string.u-escapes1", "A\u0041 A_umlaut\u00c4 y_umlaut\u00ff0");
pref("string.u-escapes2", "S_acute\u015a y_grave\u1Ef3");
// CYCLONE is 0x1f300, GRINNING FACE is 0x1f600. We have to represent them via
// surrogate pairs.
pref("string.u-surrogates",
    "cyclone\uD83C\uDF00 grinning_face\uD83D\uDE00");

