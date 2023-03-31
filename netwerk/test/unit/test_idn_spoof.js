// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// https://source.chromium.org/chromium/chromium/src/+/main:LICENSE

// Tests nsIIDNService
// Imported from https://source.chromium.org/chromium/chromium/src/+/main:components/url_formatter/spoof_checks/idn_spoof_checker_unittest.cc;drc=e544837967287f956ba69af3b228b202e8e7cf1a

"use strict";

const idnService = Cc["@mozilla.org/network/idn-service;1"].getService(
  Ci.nsIIDNService
);

const kSafe = 1;
const kUnsafe = 2;
const kInvalid = 3;

// prettier-ignore
let testCases = [
  // No IDN
  ["www.google.com", "www.google.com", kSafe],
  ["www.google.com.", "www.google.com.", kSafe],
  [".", ".", kSafe],
  ["", "", kSafe],
  // Invalid IDN
  ["xn--example-.com", "xn--example-.com", kInvalid],
  // IDN
  // Hanzi (Traditional Chinese)
  ["xn--1lq90ic7f1rc.cn", "\u5317\u4eac\u5927\u5b78.cn", kSafe],
  // Hanzi ('video' in Simplified Chinese)
  ["xn--cy2a840a.com", "\u89c6\u9891.com", kSafe],
  // Hanzi + '123'
  ["www.xn--123-p18d.com", "www.\u4e00123.com", kSafe],
  // Hanzi + Latin : U+56FD is simplified
  ["www.xn--hello-9n1hm04c.com", "www.hello\u4e2d\u56fd.com", kSafe],
  // Kanji + Kana (Japanese)
  ["xn--l8jvb1ey91xtjb.jp", "\u671d\u65e5\u3042\u3055\u3072.jp", kSafe],
  // Katakana including U+30FC
  ["xn--tckm4i2e.jp", "\u30b3\u30de\u30fc\u30b9.jp", kSafe],
  ["xn--3ck7a7g.jp", "\u30ce\u30f3\u30bd.jp", kSafe],
  // Katakana + Latin (Japanese)
  ["xn--e-efusa1mzf.jp", "e\u30b3\u30de\u30fc\u30b9.jp", kSafe],
  ["xn--3bkxe.jp", "\u30c8\u309a.jp", kSafe],
  // Hangul (Korean)
  ["www.xn--or3b17p6jjc.kr", "www.\uc804\uc790\uc815\ubd80.kr", kSafe],
  // b<u-umlaut>cher (German)
  ["xn--bcher-kva.de", "b\u00fccher.de", kSafe],
  // a with diaeresis
  ["www.xn--frgbolaget-q5a.se", "www.f\u00e4rgbolaget.se", kSafe],
  // c-cedilla (French)
  ["www.xn--alliancefranaise-npb.fr", "www.alliancefran\u00e7aise.fr", kSafe],
  // caf'e with acute accent (French)
  ["xn--caf-dma.fr", "caf\u00e9.fr", kSafe],
  // c-cedillla and a with tilde (Portuguese)
  ["xn--poema-9qae5a.com.br", "p\u00e3oema\u00e7\u00e3.com.br", kSafe],
  // s with caron
  ["xn--achy-f6a.com", "\u0161achy.com", kSafe],
  ["xn--kxae4bafwg.gr", "\u03bf\u03c5\u03c4\u03bf\u03c0\u03af\u03b1.gr", kSafe],
  // Eutopia + 123 (Greek)
  ["xn---123-pldm0haj2bk.gr",     "\u03bf\u03c5\u03c4\u03bf\u03c0\u03af\u03b1-123.gr", kSafe],
  // Cyrillic (Russian)
  ["xn--n1aeec9b.r", "\u0442\u043e\u0440\u0442\u044b.r", kSafe],
  // Cyrillic + 123 (Russian)
  ["xn---123-45dmmc5f.r", "\u0442\u043e\u0440\u0442\u044b-123.r", kSafe],
  // 'president' in Russian. Is a wholescript confusable, but allowed.
  ["xn--d1abbgf6aiiy.xn--p1ai",     "\u043f\u0440\u0435\u0437\u0438\u0434\u0435\u043d\u0442.\u0440\u0444", kSafe],
  // Arabic
  ["xn--mgba1fmg.eg", "\u0627\u0641\u0644\u0627\u0645.eg", kSafe],
  // Hebrew
  ["xn--4dbib.he", "\u05d5\u05d0\u05d4.he", kSafe],
  // Hebrew + Common
  ["xn---123-ptf2c5c6bt.il", "\u05e2\u05d1\u05e8\u05d9\u05ea-123.il", kSafe],
  // Thai
  ["xn--12c2cc4ag3b4ccu.th",     "\u0e2a\u0e32\u0e22\u0e01\u0e32\u0e23\u0e1a\u0e34\u0e19.th", kSafe],
  // Thai + Common
  ["xn---123-9goxcp8c9db2r.th",     "\u0e20\u0e32\u0e29\u0e32\u0e44\u0e17\u0e22-123.th", kSafe],
  // Devangari (Hindi)
  ["www.xn--l1b6a9e1b7c.in", "www.\u0905\u0915\u094b\u0932\u093e.in", kSafe],
  // Devanagari + Common
  ["xn---123-kbjl2j0bl2k.in", "\u0939\u093f\u0928\u094d\u0926\u0940-123.in", kSafe],

  // Block mixed numeric + numeric lookalike (12.com, using U+0577).
  ["xn--1-xcc.com", "1\u0577.com", kUnsafe, "DISABLED"],

  // Block mixed numeric lookalike + numeric (੨0.com, uses U+0A68).
  ["xn--0-6ee.com", "\u0a680.com", kUnsafe],
  // Block fully numeric lookalikes (৪੨.com using U+09EA and U+0A68).
  ["xn--47b6w.com", "\u09ea\u0a68.com", kUnsafe],
  // Block single script digit lookalikes (using three U+0A68 characters).
  ["xn--qccaa.com", "\u0a68\u0a68\u0a68.com", kUnsafe, "DISABLED"],

  // URL test with mostly numbers and one confusable character
  // Georgian 'd' 4000.com
  ["xn--4000-pfr.com", "\u10eb4000.com", kUnsafe, "DISABLED"],

  // What used to be 5 Aspirational scripts in the earlier versions of UAX 31.
  // UAX 31 does not define aspirational scripts any more.
  // See http://www.unicode.org/reports/tr31/#Aspirational_Use_Scripts .
  // Unified Canadian Syllabary
  ["xn--dfe0tte.ca", "\u1456\u14c2\u14ef.ca", kUnsafe],
  // Tifinagh
  ["xn--4ljxa2bb4a6bxb.ma", "\u2d5c\u2d49\u2d3c\u2d49\u2d4f\u2d30\u2d56.ma", kUnsafe],
  // Tifinagh with a disallowed character(U+2D6F)
  ["xn--hmjzaby5d5f.ma", "\u2d5c\u2d49\u2d3c\u2d6f\u2d49\u2d4f.ma", kInvalid],

  // Yi
  ["xn--4o7a6e1x64c.cn", "\ua188\ua320\ua071\ua0b7.cn", kUnsafe],
  // Mongolian - 'ordu' (place, camp)
  ["xn--56ec8bp.cn", "\u1823\u1837\u1833\u1824.cn", kUnsafe],
  // Mongolian with a disallowed character
  ["xn--95e5de3ds.cn", "\u1823\u1837\u1804\u1833\u1824.cn", kUnsafe],
  // Miao/Pollad
  ["xn--2u0fpf0a.cn", "\U00016f04\U00016f62\U00016f59.cn", kUnsafe],

  // Script mixing tests
  // The following script combinations are allowed.
  // HIGHLY_RESTRICTIVE with Latin limited to ASCII-Latin.
  // ASCII-Latin + Japn (Kana + Han)
  // ASCII-Latin + Kore (Hangul + Han)
  // ASCII-Latin + Han + Bopomofo
  // "payp<alpha>l.com"
  ["xn--paypl-g9d.com", "payp\u03b1l.com", kUnsafe],
  // google.gr with Greek omicron and epsilon
  ["xn--ggl-6xc1ca.gr", "g\u03bf\u03bfgl\u03b5.gr", kUnsafe],
  // google.ru with Cyrillic o
  ["xn--ggl-tdd6ba.r", "g\u043e\u043egl\u0435.r", kUnsafe],
  // h<e with acute>llo<China in Han>.cn
  ["xn--hllo-bpa7979ih5m.cn", "h\u00e9llo\u4e2d\u56fd.cn", kUnsafe, "DISABLED"],
  // <Greek rho><Cyrillic a><Cyrillic u>.ru
  ["xn--2xa6t2b.r", "\u03c1\u0430\u0443.r", kUnsafe],
  // Georgian + Latin
  ["xn--abcef-vuu.test", "abc\u10ebef.test", kUnsafe],
  // Hangul + Latin
  ["xn--han-eb9ll88m.kr", "\ud55c\uae00han.kr", kSafe],
  // Hangul + Latin + Han with IDN ccTLD
  ["xn--han-or0kq92gkm3c.xn--3e0b707e", "\ud55c\uae00han\u97d3.\ud55c\uad6d", kSafe],
  // non-ASCII Latin + Hangul
  ["xn--caf-dma9024xvpg.kr", "caf\u00e9\uce74\ud398.kr", kUnsafe, "DISABLED"],
  // Hangul + Hiragana
  ["xn--y9j3b9855e.kr", "\ud55c\u3072\u3089.kr", kUnsafe],
  // <Hiragana>.<Hangul> is allowed because script mixing check is per label.
  ["xn--y9j3b.xn--3e0b707e", "\u3072\u3089.\ud55c\uad6d", kSafe],
  //  Traditional Han + Latin
  ["xn--hanzi-u57ii69i.tw", "\u6f22\u5b57hanzi.tw", kSafe],
  //  Simplified Han + Latin
  ["xn--hanzi-u57i952h.cn", "\u6c49\u5b57hanzi.cn", kSafe],
  // Simplified Han + Traditonal Han
  ["xn--hanzi-if9kt8n.cn", "\u6c49\u6f22hanzi.cn", kSafe],
  //  Han + Hiragana + Katakana + Latin
  ["xn--kanji-ii4dpizfq59yuykqr4b.jp",     "\u632f\u308a\u4eee\u540d\u30ab\u30bfkanji.jp", kSafe],
  // Han + Bopomofo
  ["xn--5ekcde0577e87tc.tw", "\u6ce8\u97f3\u3105\u3106\u3107\u3108.tw", kSafe],
  // Han + Latin + Bopomofo
  ["xn--bopo-ty4cghi8509kk7xd.tw",     "\u6ce8\u97f3bopo\u3105\u3106\u3107\u3108.tw", kSafe],
  // Latin + Bopomofo
  ["xn--bopomofo-hj5gkalm.tw", "bopomofo\u3105\u3106\u3107\u3108.tw", kSafe],
  // Bopomofo + Katakana
  ["xn--lcka3d1bztghi.tw",     "\u3105\u3106\u3107\u3108\u30ab\u30bf\u30ab\u30ca.tw", kUnsafe],
  //  Bopomofo + Hangul
  ["xn--5ekcde4543qbec.tw", "\u3105\u3106\u3107\u3108\uc8fc\uc74c.tw", kUnsafe],
  // Devanagari + Latin
  ["xn--ab-3ofh8fqbj6h.in", "ab\u0939\u093f\u0928\u094d\u0926\u0940.in", kUnsafe],
  // Thai + Latin
  ["xn--ab-jsi9al4bxdb6n.th", "ab\u0e20\u0e32\u0e29\u0e32\u0e44\u0e17\u0e22.th", kUnsafe],
  // Armenian + Latin
  ["xn--bs-red.com", "b\u057ds.com", kUnsafe],
  // Tibetan + Latin
  ["xn--foo-vkm.com", "foo\u0f37.com", kUnsafe],
  // Oriya + Latin
  ["xn--fo-h3g.com", "fo\u0b66.com", kUnsafe],
  // Gujarati + Latin
  ["xn--fo-isg.com", "fo\u0ae6.com", kUnsafe],
  // <vitamin in Katakana>b1.com
  ["xn--b1-xi4a7cvc9f.com", "\u30d3\u30bf\u30df\u30f3b1.com", kSafe],
  // Devanagari + Han
  ["xn--t2bes3ds6749n.com", "\u0930\u094b\u0932\u0947\u76e7\u0938.com", kUnsafe],
  // Devanagari + Bengali
  ["xn--11b0x.in", "\u0915\u0995.in", kUnsafe],
  // Canadian Syllabary + Latin
  ["xn--ab-lym.com", "ab\u14bf.com", kUnsafe],
  ["xn--ab1-p6q.com", "ab1\u14bf.com", kUnsafe],
  ["xn--1ab-m6qd.com", "\u14bf1ab\u14bf.com", kUnsafe],
  ["xn--ab-jymc.com", "\u14bfab\u14bf.com", kUnsafe],
  // Tifinagh + Latin
  ["xn--liy-bq1b.com", "li\u2d4fy.com", kUnsafe],
  ["xn--rol-cq1b.com", "rol\u2d4f.com", kUnsafe],
  ["xn--ily-8p1b.com", "\u2d4fily.com", kUnsafe],
  ["xn--1ly-8p1b.com", "\u2d4f1ly.com", kUnsafe],

  // Invisibility check
  // Thai tone mark malek(U+0E48) repeated
  ["xn--03c0b3ca.th", "\u0e23\u0e35\u0e48\u0e48.th", kUnsafe],
  // Accute accent repeated
  ["xn--a-xbba.com", "a\u0301\u0301.com", kInvalid],
  // 'a' with acuted accent + another acute accent
  ["xn--1ca20i.com", "\u00e1\u0301.com", kUnsafe, "DISABLED"],
  // Combining mark at the beginning
  ["xn--abc-fdc.jp", "\u0300abc.jp", kInvalid],

  // The following three are detected by |dangerous_pattern| regex, but
  // can be regarded as an extension of blocking repeated diacritic marks.
  // i followed by U+0307 (combining dot above)
  ["xn--pixel-8fd.com", "pi\u0307xel.com", kUnsafe],
  // U+0131 (dotless i) followed by U+0307
  ["xn--pxel-lza43z.com", "p\u0131\u0307xel.com", kUnsafe],
  // j followed by U+0307 (combining dot above)
  ["xn--jack-qwc.com", "j\u0307ack.com", kUnsafe],
  // l followed by U+0307
  ["xn--lace-qwc.com", "l\u0307ace.com", kUnsafe],

  // Do not allow a combining mark after dotless i/j.
  ["xn--pxel-lza29y.com", "p\u0131\u0300xel.com", kUnsafe],
  ["xn--ack-gpb42h.com", "\u0237\u0301ack.com", kUnsafe],

  // Mixed script confusable
  // google with Armenian Small Letter Oh(U+0585)
  ["xn--gogle-lkg.com", "g\u0585ogle.com", kUnsafe],
  ["xn--range-kkg.com", "\u0585range.com", kUnsafe],
  ["xn--cucko-pkg.com", "cucko\u0585.com", kUnsafe],
  // Latin 'o' in Armenian.
  ["xn--o-ybcg0cu0cq.com", "o\u0580\u0574\u0578\u0582\u0566\u0568.com", kUnsafe],
  // Hiragana HE(U+3078) mixed with Katakana
  ["xn--49jxi3as0d0fpc.com", "\u30e2\u30d2\u30fc\u30c8\u3078\u30d6\u30f3.com", kUnsafe, "DISABLED"],

  // U+30FC should be preceded by a Hiragana/Katakana.
  // Katakana + U+30FC + Han
  ["xn--lck0ip02qw5ya.jp", "\u30ab\u30fc\u91ce\u7403.jp", kSafe],
  // Hiragana + U+30FC + Han
  ["xn--u8j5tr47nw5ya.jp", "\u304b\u30fc\u91ce\u7403.jp", kSafe],
  // U+30FC + Han
  ["xn--weka801xo02a.com", "\u30fc\u52d5\u753b\u30fc.com", kUnsafe],
  // Han + U+30FC + Han
  ["xn--wekz60nb2ay85atj0b.jp", "\u65e5\u672c\u30fc\u91ce\u7403.jp", kUnsafe],
  // U+30FC at the beginning
  ["xn--wek060nb2a.jp", "\u30fc\u65e5\u672c.jp", kUnsafe],
  // Latin + U+30FC + Latin
  ["xn--abcdef-r64e.jp", "abc\u30fcdef.jp", kUnsafe],

  // U+30FB (・) is not allowed next to Latin, but allowed otherwise.
  // U+30FB + Han
  ["xn--vekt920a.jp", "\u30fb\u91ce.jp", kSafe],
  // Han + U+30FB + Han
  ["xn--vek160nb2ay85atj0b.jp", "\u65e5\u672c\u30fb\u91ce\u7403.jp", kSafe],
  // Latin + U+30FB + Latin
  ["xn--abcdef-k64e.jp", "abc\u30fbdef.jp", kUnsafe, "DISABLED"],
  // U+30FB + Latin
  ["xn--abc-os4b.jp", "\u30fbabc.jp", kUnsafe, "DISABLED"],

  // U+30FD (ヽ) is allowed only after Katakana.
  // Katakana + U+30FD
  ["xn--lck2i.jp", "\u30ab\u30fd.jp", kSafe],
  // Hiragana + U+30FD
  ["xn--u8j7t.jp", "\u304b\u30fd.jp", kUnsafe, "DISABLED"],
  // Han + U+30FD
  ["xn--xek368f.jp", "\u4e00\u30fd.jp", kUnsafe, "DISABLED"],
  ["xn--a-mju.jp", "a\u30fd.jp", kUnsafe, "DISABLED"],
  ["xn--a1-bo4a.jp", "a1\u30fd.jp", kUnsafe, "DISABLED"],

  // U+30FE (ヾ) is allowed only after Katakana.
  // Katakana + U+30FE
  ["xn--lck4i.jp", "\u30ab\u30fe.jp", kSafe],
  // Hiragana + U+30FE
  ["xn--u8j9t.jp", "\u304b\u30fe.jp", kUnsafe, "DISABLED"],
  // Han + U+30FE
  ["xn--yek168f.jp", "\u4e00\u30fe.jp", kUnsafe, "DISABLED"],
  ["xn--a-oju.jp", "a\u30fe.jp", kUnsafe, "DISABLED"],
  ["xn--a1-eo4a.jp", "a1\u30fe.jp", kUnsafe, "DISABLED"],

  // Cyrillic labels made of Latin-look-alike Cyrillic letters.
  // 1) ѕсоре.com with ѕсоре in Cyrillic.
  ["xn--e1argc3h.com", "\u0455\u0441\u043e\u0440\u0435.com", kUnsafe, "DISABLED"],
  // 2) ѕсоре123.com with ѕсоре in Cyrillic.
  ["xn--123-qdd8bmf3n.com", "\u0455\u0441\u043e\u0440\u0435123.com", kUnsafe, "DISABLED"],
  // 3) ѕсоре-рау.com with ѕсоре and рау in Cyrillic.
  ["xn----8sbn9akccw8m.com",     "\u0455\u0441\u043e\u0440\u0435-\u0440\u0430\u0443.com", kUnsafe, "DISABLED"],
  // 4) ѕсоре1рау.com with scope and pay in Cyrillic and a non-letter between
  // them.
  ["xn--1-8sbn9akccw8m.com",     "\u0455\u0441\u043e\u0440\u0435\u0031\u0440\u0430\u0443.com", kUnsafe, "DISABLED"],

  // The same as above three, but in IDN TLD (рф).
  // 1) ѕсоре.рф with ѕсоре in Cyrillic.
  ["xn--e1argc3h.xn--p1ai", "\u0455\u0441\u043e\u0440\u0435.\u0440\u0444", kSafe],
  // 2) ѕсоре123.рф with ѕсоре in Cyrillic.
  ["xn--123-qdd8bmf3n.xn--p1ai",     "\u0455\u0441\u043e\u0440\u0435123.\u0440\u0444", kSafe],
  // 3) ѕсоре-рау.рф with ѕсоре and рау in Cyrillic.
  ["xn----8sbn9akccw8m.xn--p1ai",     "\u0455\u0441\u043e\u0440\u0435-\u0440\u0430\u0443.\u0440\u0444", kSafe],
  // 4) ѕсоре1рау.com with scope and pay in Cyrillic and a non-letter between
  // them.
  ["xn--1-8sbn9akccw8m.xn--p1ai",     "\u0455\u0441\u043e\u0440\u0435\u0031\u0440\u0430\u0443.\u0440\u0444", kSafe],

  // Same as above three, but in .ru TLD.
  // 1) ѕсоре.ru  with ѕсоре in Cyrillic.
  ["xn--e1argc3h.r", "\u0455\u0441\u043e\u0440\u0435.r", kSafe],
  // 2) ѕсоре123.ru with ѕсоре in Cyrillic.
  ["xn--123-qdd8bmf3n.r", "\u0455\u0441\u043e\u0440\u0435123.r", kSafe],
  // 3) ѕсоре-рау.ru with ѕсоре and рау in Cyrillic.
  ["xn----8sbn9akccw8m.r",     "\u0455\u0441\u043e\u0440\u0435-\u0440\u0430\u0443.r", kSafe],
  // 4) ѕсоре1рау.com with scope and pay in Cyrillic and a non-letter between
  // them.
  ["xn--1-8sbn9akccw8m.r",     "\u0455\u0441\u043e\u0440\u0435\u0031\u0440\u0430\u0443.r", kSafe],

  // ѕсоре-рау.한국 with ѕсоре and рау in Cyrillic. The label will remain
  // punycode while the TLD will be decoded.
  ["xn----8sbn9akccw8m.xn--3e0b707e", "xn----8sbn9akccw8m.\ud55c\uad6d", kSafe, "DISABLED"],

  // музей (museum in Russian) has characters without a Latin-look-alike.
  ["xn--e1adhj9a.com", "\u043c\u0443\u0437\u0435\u0439.com", kSafe],

  // ѕсоԗе.com is Cyrillic with Latin lookalikes.
  ["xn--e1ari3f61c.com", "\u0455\u0441\u043e\u0517\u0435.com", kUnsafe, "DISABLED"],

  // ыоԍ.com is Cyrillic with Latin lookalikes.
  ["xn--n1az74c.com", "\u044b\u043e\u050d.com", kUnsafe],

  // сю.com is Cyrillic with Latin lookalikes.
  ["xn--q1a0a.com", "\u0441\u044e.com", kUnsafe, "DISABLED"],

  // Regression test for lowercase letters in whole script confusable
  // lookalike character lists.
  ["xn--80a8a6a.com", "\u0430\u044c\u0441.com", kUnsafe, "DISABLED"],

  // googlе.한국 where е is Cyrillic. This tests the generic case when one
  // label is not allowed but  other labels in the domain name are still
  // decoded. Here, googlе is left in punycode but the TLD is decoded.
  ["xn--googl-3we.xn--3e0b707e", "xn--googl-3we.\ud55c\uad6d", kSafe],

  // Combining Diacritic marks after a script other than Latin-Greek-Cyrillic
  ["xn--rsa2568fvxya.com", "\ud55c\u0307\uae00.com", kUnsafe, "DISABLED"],  // 한́글.com
  ["xn--rsa0336bjom.com", "\u6f22\u0307\u5b57.com", kUnsafe, "DISABLED"],  // 漢̇字.com
  // नागरी́.com
  ["xn--lsa922apb7a6do.com", "\u0928\u093e\u0917\u0930\u0940\u0301.com", kUnsafe, "DISABLED"],

  // Similarity checks against the list of top domains. "digklmo68.com" and
  // 'digklmo68.co.uk" are listed for unittest in the top domain list.
  // đigklmo68.com:
  ["xn--igklmo68-kcb.com", "\u0111igklmo68.com", kUnsafe, "DISABLED"],
  // www.đigklmo68.com:
  ["www.xn--igklmo68-kcb.com", "www.\u0111igklmo68.com", kUnsafe, "DISABLED"],
  // foo.bar.đigklmo68.com:
  ["foo.bar.xn--igklmo68-kcb.com", "foo.bar.\u0111igklmo68.com", kUnsafe, "DISABLED"],
  // đigklmo68.co.uk:
  ["xn--igklmo68-kcb.co.uk", "\u0111igklmo68.co.uk", kUnsafe, "DISABLED"],
  // mail.đigklmo68.co.uk:
  ["mail.xn--igklmo68-kcb.co.uk", "mail.\u0111igklmo68.co.uk", kUnsafe, "DISABLED"],
  // di̇gklmo68.com:
  ["xn--digklmo68-6jf.com", "di\u0307gklmo68.com", kUnsafe],
  // dig̱klmo68.com:
  ["xn--digklmo68-7vf.com", "dig\u0331klmo68.com", kUnsafe, "DISABLED"],
  // digĸlmo68.com:
  ["xn--diglmo68-omb.com", "dig\u0138lmo68.com", kUnsafe],
  // digkłmo68.com:
  ["xn--digkmo68-9ob.com", "digk\u0142mo68.com", kUnsafe, "DISABLED"],
  // digklṃo68.com:
  ["xn--digklo68-l89c.com", "digkl\u1e43o68.com", kUnsafe, "DISABLED"],
  // digklmø68.com:
  ["xn--digklm68-b5a.com", "digklm\u00f868.com", kUnsafe, "DISABLED"],
  // digklmoб8.com:
  ["xn--digklmo8-h7g.com", "digklmo\u04318.com", kUnsafe],
  // digklmo6৪.com:
  ["xn--digklmo6-7yr.com", "digklmo6\u09ea.com", kUnsafe],

  // 'islkpx123.com' is in the test domain list.
  // 'іѕӏкрх123' can look like 'islkpx123' in some fonts.
  ["xn--123-bed4a4a6hh40i.com",     "\u0456\u0455\u04cf\u043a\u0440\u0445123.com", kUnsafe, "DISABLED"],

  // 'o2.com', '28.com', '39.com', '43.com', '89.com', 'oo.com' and 'qq.com'
  // are all explicitly added to the test domain list to aid testing of
  // Latin-lookalikes that are numerics in other character sets and similar
  // edge cases.
  //
  // Bengali:
  ["xn--07be.com", "\u09e6\u09e8.com", kUnsafe, "DISABLED"],
  ["xn--27be.com", "\u09e8\u09ea.com", kUnsafe, "DISABLED"],
  ["xn--77ba.com", "\u09ed\u09ed.com", kUnsafe, "DISABLED"],
  // Gurmukhi:
  ["xn--qcce.com", "\u0a68\u0a6a.com", kUnsafe, "DISABLED"],
  ["xn--occe.com", "\u0a66\u0a68.com", kUnsafe, "DISABLED"],
  ["xn--rccd.com", "\u0a6b\u0a69.com", kUnsafe, "DISABLED"],
  ["xn--pcca.com", "\u0a67\u0a67.com", kUnsafe, "DISABLED"],
  // Telugu:
  ["xn--drcb.com", "\u0c69\u0c68.com", kUnsafe, "DISABLED"],
  // Devanagari:
  ["xn--d4be.com", "\u0966\u0968.com", kUnsafe, "DISABLED"],
  // Kannada:
  ["xn--yucg.com", "\u0ce6\u0ce9.com", kUnsafe, "DISABLED"],
  ["xn--yuco.com", "\u0ce6\u0ced.com", kUnsafe, "DISABLED"],
  // Oriya:
  ["xn--1jcf.com", "\u0b6b\u0b68.com", kUnsafe, "DISABLED"],
  ["xn--zjca.com", "\u0b66\u0b66.com", kUnsafe, "DISABLED"],
  // Gujarati:
  ["xn--cgce.com", "\u0ae6\u0ae8.com", kUnsafe, "DISABLED"],
  ["xn--fgci.com", "\u0ae9\u0aed.com", kUnsafe, "DISABLED"],
  ["xn--dgca.com", "\u0ae7\u0ae7.com", kUnsafe, "DISABLED"],

  // wmhtb.com
  ["xn--l1acpvx.com", "\u0448\u043c\u043d\u0442\u044c.com", kUnsafe, "DISABLED"],
  // щмнть.com
  ["xn--l1acpzs.com", "\u0449\u043c\u043d\u0442\u044c.com", kUnsafe, "DISABLED"],
  // шмнтв.com
  ["xn--b1atdu1a.com", "\u0448\u043c\u043d\u0442\u0432.com", kUnsafe, "DISABLED"],
  // шмԋтв.com
  ["xn--b1atsw09g.com", "\u0448\u043c\u050b\u0442\u0432.com", kUnsafe],
  // шмԧтв.com
  ["xn--b1atsw03i.com", "\u0448\u043c\u0527\u0442\u0432.com", kUnsafe, "DISABLED"],
  // шмԋԏв.com
  ["xn--b1at9a12dua.com", "\u0448\u043c\u050b\u050f\u0432.com", kUnsafe],
  // ഠട345.com
  ["xn--345-jtke.com", "\u0d20\u0d1f345.com", kUnsafe, "DISABLED"],

  // Test additional confusable LGC characters (most of them without
  // decomposition into base + diacritc mark). The corresponding ASCII
  // domain names are in the test top domain list.
  // ϼκαωχ.com
  ["xn--mxar4bh6w.com", "\u03fc\u03ba\u03b1\u03c9\u03c7.com", kUnsafe, "DISABLED"],
  // þħĸŧƅ.com
  ["xn--vda6f3b2kpf.com", "\u00fe\u0127\u0138\u0167\u0185.com", kUnsafe],
  // þhktb.com
  ["xn--hktb-9ra.com", "\u00fehktb.com", kUnsafe, "DISABLED"],
  // pħktb.com
  ["xn--pktb-5xa.com", "p\u0127ktb.com", kUnsafe, "DISABLED"],
  // phĸtb.com
  ["xn--phtb-m0a.com", "ph\u0138tb.com", kUnsafe],
  // phkŧb.com
  ["xn--phkb-d7a.com", "phk\u0167b.com", kUnsafe, "DISABLED"],
  // phktƅ.com
  ["xn--phkt-ocb.com", "phkt\u0185.com", kUnsafe],
  // ҏнкть.com
  ["xn--j1afq4bxw.com", "\u048f\u043d\u043a\u0442\u044c.com", kUnsafe, "DISABLED"],
  // ҏћкть.com
  ["xn--j1aq4a7cvo.com", "\u048f\u045b\u043a\u0442\u044c.com", kUnsafe, "DISABLED"],
  // ҏңкть.com
  ["xn--j1aq4azund.com", "\u048f\u04a3\u043a\u0442\u044c.com", kUnsafe, "DISABLED"],
  // ҏҥкть.com
  ["xn--j1aq4azuxd.com", "\u048f\u04a5\u043a\u0442\u044c.com", kUnsafe, "DISABLED"],
  // ҏӈкть.com
  ["xn--j1aq4azuyj.com", "\u048f\u04c8\u043a\u0442\u044c.com", kUnsafe, "DISABLED"],
  // ҏԧкть.com
  ["xn--j1aq4azu9z.com", "\u048f\u0527\u043a\u0442\u044c.com", kUnsafe, "DISABLED"],
  // ҏԩкть.com
  ["xn--j1aq4azuq0a.com", "\u048f\u0529\u043a\u0442\u044c.com", kUnsafe, "DISABLED"],
  // ҏнқть.com
  ["xn--m1ak4azu6b.com", "\u048f\u043d\u049b\u0442\u044c.com", kUnsafe, "DISABLED"],
  // ҏнҝть.com
  ["xn--m1ak4azunc.com", "\u048f\u043d\u049d\u0442\u044c.com", kUnsafe, "DISABLED"],
  // ҏнҟть.com
  ["xn--m1ak4azuxc.com", "\u048f\u043d\u049f\u0442\u044c.com", kUnsafe, "DISABLED"],
  // ҏнҡть.com
  ["xn--m1ak4azu7c.com", "\u048f\u043d\u04a1\u0442\u044c.com", kUnsafe, "DISABLED"],
  // ҏнӄть.com
  ["xn--m1ak4azu8i.com", "\u048f\u043d\u04c4\u0442\u044c.com", kUnsafe, "DISABLED"],
  // ҏнԟть.com
  ["xn--m1ak4azuzy.com", "\u048f\u043d\u051f\u0442\u044c.com", kUnsafe, "DISABLED"],
  // ҏнԟҭь.com
  ["xn--m1a4a4nnery.com", "\u048f\u043d\u051f\u04ad\u044c.com", kUnsafe, "DISABLED"],
  // ҏнԟҭҍ.com
  ["xn--m1a4ne5jry.com", "\u048f\u043d\u051f\u04ad\u048d.com", kUnsafe, "DISABLED"],
  // ҏнԟҭв.com
  ["xn--b1av9v8dry.com", "\u048f\u043d\u051f\u04ad\u0432.com", kUnsafe, "DISABLED"],
  // ҏӊԟҭв.com
  ["xn--b1a9p8c1e8r.com", "\u048f\u04ca\u051f\u04ad\u0432.com", kUnsafe, "DISABLED"],
  // wmŋr.com
  ["xn--wmr-jxa.com", "wm\u014br.com", kUnsafe, "DISABLED"],
  // шмпґ.com
  ["xn--l1agz80a.com", "\u0448\u043c\u043f\u0491.com", kUnsafe, "DISABLED"],
  // щмпґ.com
  ["xn--l1ag2a0y.com", "\u0449\u043c\u043f\u0491.com", kUnsafe, "DISABLED"],
  // щӎпґ.com
  ["xn--o1at1tsi.com", "\u0449\u04ce\u043f\u0491.com", kUnsafe, "DISABLED"],
  // ґғ.com
  ["xn--03ae.com", "\u0491\u0493.com", kUnsafe, "DISABLED"],
  // ґӻ.com
  ["xn--03a6s.com", "\u0491\u04fb.com", kUnsafe, "DISABLED"],
  // ҫұҳҽ.com
  ["xn--r4amg4b.com", "\u04ab\u04b1\u04b3\u04bd.com", kUnsafe, "DISABLED"],
  // ҫұӽҽ.com
  ["xn--r4am0b8r.com", "\u04ab\u04b1\u04fd\u04bd.com", kUnsafe, "DISABLED"],
  // ҫұӿҽ.com
  ["xn--r4am0b3s.com", "\u04ab\u04b1\u04ff\u04bd.com", kUnsafe, "DISABLED"],
  // ҫұӿҿ.com
  ["xn--r4am6b4p.com", "\u04ab\u04b1\u04ff\u04bf.com", kUnsafe, "DISABLED"],
  // ҫұӿє.com
  ["xn--91a7osa62a.com", "\u04ab\u04b1\u04ff\u0454.com", kUnsafe, "DISABLED"],
  // ӏԃԍ.com
  ["xn--s5a8h4a.com", "\u04cf\u0503\u050d.com", kUnsafe],

  // U+04CF(ӏ) is mapped to multiple characters, lowercase L(l) and
  // lowercase I(i). Lowercase L is also regarded as similar to digit 1.
  // The test domain list has {ig, ld, 1gd}.com for Cyrillic.
  // ӏԍ.com
  ["xn--s5a8j.com", "\u04cf\u050d.com", kUnsafe],
  // ӏԃ.com
  ["xn--s5a8h.com", "\u04cf\u0503.com", kUnsafe],
  // ӏԍԃ.com
  ["xn--s5a8h3a.com", "\u04cf\u050d\u0503.com", kUnsafe],

  // 1շ34567890.com
  ["xn--134567890-gnk.com", "1\u057734567890.com", kUnsafe, "DISABLED"],
  // ꓲ2345б7890.com
  ["xn--23457890-e7g93622b.com", "\ua4f22345\u04317890.com", kUnsafe],
  // 1ᒿ345б7890.com
  ["xn--13457890-e7g0943b.com", "1\u14bf345\u04317890.com", kUnsafe],
  // 12з4567890.com
  ["xn--124567890-10h.com", "12\u04374567890.com", kUnsafe, "DISABLED"],
  // 12ҙ4567890.com
  ["xn--124567890-1ti.com", "12\u04994567890.com", kUnsafe, "DISABLED"],
  // 12ӡ4567890.com
  ["xn--124567890-mfj.com", "12\u04e14567890.com", kUnsafe, "DISABLED"],
  // 12उ4567890.com
  ["xn--124567890-m3r.com", "12\u09094567890.com", kUnsafe, "DISABLED"],
  // 12ও4567890.com
  ["xn--124567890-17s.com", "12\u09934567890.com", kUnsafe, "DISABLED"],
  // 12ਤ4567890.com
  ["xn--124567890-hfu.com", "12\u0a244567890.com", kUnsafe, "DISABLED"],
  // 12ဒ4567890.com
  ["xn--124567890-6s6a.com", "12\u10124567890.com", kUnsafe, "DISABLED"],
  // 12ვ4567890.com
  ["xn--124567890-we8a.com", "12\u10D54567890.com", kUnsafe, "DISABLED"],
  // 12პ4567890.com
  ["xn--124567890-hh8a.com", "12\u10DE4567890.com", kUnsafe, "DISABLED"],
  // 123ㄐ567890.com
  ["xn--123567890-dr5h.com", "123ㄐ567890.com", kUnsafe, "DISABLED"],
  // 123Ꮞ567890.com
  ["xn--123567890-dm4b.com", "123\u13ce567890.com", kUnsafe],
  // 12345б7890.com
  ["xn--123457890-fzh.com", "12345\u04317890.com", kUnsafe, "DISABLED"],
  // 12345ճ7890.com
  ["xn--123457890-fmk.com", "12345ճ7890.com", kUnsafe, "DISABLED"],
  // 1234567ȣ90.com
  ["xn--123456790-6od.com", "1234567\u022390.com", kUnsafe],
  // 12345678୨0.com
  ["xn--123456780-71w.com", "12345678\u0b680.com", kUnsafe],
  // 123456789ଠ.com
  ["xn--123456789-ohw.com", "123456789\u0b20.com", kUnsafe, "DISABLED"],
  // 123456789ꓳ.com
  ["xn--123456789-tx75a.com", "123456789\ua4f3.com", kUnsafe],

  // aeœ.com
  ["xn--ae-fsa.com", "ae\u0153.com", kUnsafe, "DISABLED"],
  // æce.com
  ["xn--ce-0ia.com", "\u00e6ce.com", kUnsafe, "DISABLED"],
  // æœ.com
  ["xn--6ca2t.com", "\u00e6\u0153.com", kUnsafe, "DISABLED"],
  // ӕԥ.com
  ["xn--y5a4n.com", "\u04d5\u0525.com", kUnsafe, "DISABLED"],

  // ငၔဌ၂ဝ.com (entirely made of Myanmar characters)
  ["xn--ridq5c9hnd.com", "\u1004\u1054\u100c\u1042\u101d.com", kUnsafe, "DISABLED"],

  // ฟรฟร.com (made of two Thai characters. similar to wsws.com in
  // some fonts)
  ["xn--w3calb.com", "\u0e1f\u0e23\u0e1f\u0e23.com", kUnsafe, "DISABLED"],
  // พรบ.com
  ["xn--r3chp.com", "\u0e1e\u0e23\u0e1a.com", kUnsafe, "DISABLED"],
  // ฟรบ.com
  ["xn--r3cjm.com", "\u0e1f\u0e23\u0e1a.com", kUnsafe, "DISABLED"],

  // Lao characters that look like w, s, o, and u.
  // ພຣບ.com
  ["xn--f7chp.com", "\u0e9e\u0ea3\u0e9a.com", kUnsafe, "DISABLED"],
  // ຟຣບ.com
  ["xn--f7cjm.com", "\u0e9f\u0ea3\u0e9a.com", kUnsafe, "DISABLED"],
  // ຟຮບ.com
  ["xn--f7cj9b.com", "\u0e9f\u0eae\u0e9a.com", kUnsafe, "DISABLED"],
  // ຟຮ໐ບ.com
  ["xn--f7cj9b5h.com", "\u0e9f\u0eae\u0ed0\u0e9a.com", kUnsafe, "DISABLED"],

  // Lao character that looks like n.
  // ก11.com
  ["xn--11-lqi.com", "\u0e0111.com", kUnsafe, "DISABLED"],

  // At one point the skeleton of 'w' was 'vv', ensure that
  // that it's treated as 'w'.
  ["xn--wder-qqa.com", "w\u00f3der.com", kUnsafe, "DISABLED"],

  // Mixed digits: the first two will also fail mixed script test
  // Latin + ASCII digit + Deva digit
  ["xn--asc1deva-j0q.co.in", "asc1deva\u0967.co.in", kUnsafe],
  // Latin + Deva digit + Beng digit
  ["xn--devabeng-f0qu3f.co.in", "deva\u0967beng\u09e7.co.in", kUnsafe],
  // ASCII digit + Deva digit
  ["xn--79-v5f.co.in", "7\u09ea9.co.in", kUnsafe],
  //  Deva digit + Beng digit
  ["xn--e4b0x.co.in", "\u0967\u09e7.co.in", kUnsafe],
  // U+4E00 (CJK Ideograph One) is not a digit, but it's not allowed next to
  // non-Kana scripts including numbers.
  ["xn--d12-s18d.cn", "d12\u4e00.cn", kUnsafe, "DISABLED"],
  // One that's really long that will force a buffer realloc
  ["aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", kSafe],

  // Not allowed; characters outside [:Identifier_Status=Allowed:]
  // Limited Use Scripts: UTS 31 Table 7.
  // Vai
  ["xn--sn8a.com", "\ua50b.com", kUnsafe],
  // 'CARD' look-alike in Cherokee
  ["xn--58db0a9q.com", "\u13df\u13aa\u13a1\u13a0.com", kUnsafe],
  // Scripts excluded from Identifiers: UTS 31 Table 4
  // Coptic
  ["xn--5ya.com", "\u03e7.com", kUnsafe],
  // Old Italic
  ["xn--097cc.com", "\U00010300\U00010301.com", kUnsafe],

  // U+115F (Hangul Filler)
  ["xn--osd3820f24c.kr", "\uac00\ub098\u115f.kr", kInvalid],
  ["www.xn--google-ho0coa.com", "www.\u2039google\u203a.com", kUnsafe],
  // Latin small capital w: hardᴡare.com
  ["xn--hardare-l41c.com", "hard\u1d21are.com", kUnsafe],
  // Minus Sign(U+2212)
  ["xn--t9g238xc2a.jp", "\u65e5\u2212\u672c.jp", kUnsafe],
  // Latin Small Letter Script G: ɡɡ.com
  ["xn--0naa.com", "\u0261\u0261.com", kUnsafe],
  // Hangul Jamo(U+11xx)
  ["xn--0pdc3b.com", "\u1102\u1103\u1110.com", kUnsafe],
  // degree sign: 36°c.com
  ["xn--36c-tfa.com", "36\u00b0c.com", kUnsafe],
  // Pound sign
  ["xn--5free-fga.com", "5free\u00a3.com", kUnsafe],
  // Hebrew points (U+05B0, U+05B6)
  ["xn--7cbl2kc2a.com", "\u05e1\u05b6\u05e7\u05b0\u05e1.com", kUnsafe],
  // Danda(U+0964)
  ["xn--81bp1b6ch8s.com", "\u0924\u093f\u091c\u0964\u0930\u0940.com", kUnsafe],
  // Small letter script G(U+0261)
  ["xn--oogle-qmc.com", "\u0261oogle.com", kUnsafe],
  // Small Katakana Extension(U+31F1)
  ["xn--wlk.com", "\u31f1.com", kUnsafe],
  // Heart symbol: ♥
  ["xn--ab-u0x.com", "ab\u2665.com", kUnsafe],
  // Emoji
  ["xn--vi8hiv.xyz", "\U0001f355\U0001f4a9.xyz", kUnsafe],
  // Registered trade mark
  ["xn--egistered-fna.com", "\u00aeegistered.com", kUnsafe],
  // Latin Letter Retroflex Click
  ["xn--registered-25c.com", "registered\u01c3.com", kUnsafe],
  // ASCII '!' not allowed in IDN
  ["xn--!-257eu42c.kr", "\uc548\ub155!.kr", kUnsafe],
  // 'GOOGLE' in IPA extension: ɢᴏᴏɢʟᴇ
  ["xn--1naa7pn51hcbaa.com", "\u0262\u1d0f\u1d0f\u0262\u029f\u1d07.com", kUnsafe],
  // Padlock icon spoof.
  ["xn--google-hj64e.com", "\U0001f512google.com", kUnsafe],

  // Custom block list
  // Combining Long Solidus Overlay
  ["google.xn--comabc-k8d", "google.com\u0338abc", kUnsafe],
  // Hyphenation Point instead of Katakana Middle dot
  ["xn--svgy16dha.jp", "\u30a1\u2027\u30a3.jp", kUnsafe],
  // Gershayim with other Hebrew characters is allowed.
  ["xn--5db6bh9b.il", "\u05e9\u05d1\u05f4\u05e6.il", kSafe, "DISABLED"],
  // Hebrew Gershayim with Latin is invalid according to Python's idna
  // package.
  ["xn--ab-yod.com", "a\u05f4b.com", kInvalid],
  // Hebrew Gershayim with Arabic is disallowed.
  ["xn--5eb7h.eg", "\u0628\u05f4.eg", kUnsafe],
// #if BUILDFLAG(IS_APPLE)
  // These characters are blocked due to a font issue on Mac.
  // Tibetan transliteration characters.
  ["xn--com-lum.test.pl", "com\u0f8c.test.pl", kUnsafe],
  // Arabic letter KASHMIRI YEH
  ["xn--fgb.com", "\u0620.com", kUnsafe, "DISABLED"],
// #endif

  // Hyphens (http://unicode.org/cldr/utility/confusables.jsp?a=-)
  // Hyphen-Minus (the only hyphen allowed)
  // abc-def
  ["abc-def.com", "abc-def.com", kSafe],
  // Modifier Letter Minus Sign
  ["xn--abcdef-5od.com", "abc\u02d7def.com", kUnsafe],
  // Hyphen
  ["xn--abcdef-dg0c.com", "abc\u2010def.com", kUnsafe],
  // Non-Breaking Hyphen
  // This is actually an invalid IDNA domain (U+2011 normalizes to U+2010),
  // but it is included to ensure that we do not inadvertently allow this
  // character to be displayed as Unicode.
  ["xn--abcdef-kg0c.com", "abc\u2011def.com", kInvalid],
  // Figure Dash.
  // Python's idna package refuses to decode the minus signs and dashes. ICU
  // decodes them but treats them as unsafe in spoof checks, so these test
  // cases are marked as unsafe instead of invalid.
  ["xn--abcdef-rg0c.com", "abc\u2012def.com", kUnsafe],
  // En Dash
  ["xn--abcdef-yg0c.com", "abc\u2013def.com", kUnsafe],
  // Hyphen Bullet
  ["xn--abcdef-kq0c.com", "abc\u2043def.com", kUnsafe],
  // Minus Sign
  ["xn--abcdef-5d3c.com", "abc\u2212def.com", kUnsafe],
  // Heavy Minus Sign
  ["xn--abcdef-kg1d.com", "abc\u2796def.com", kUnsafe],
  // Em Dash
  // Small Em Dash (U+FE58) is normalized to Em Dash.
  ["xn--abcdef-5g0c.com", "abc\u2014def.com", kUnsafe],
  // Coptic Small Letter Dialect-P Ni. Looks like dash.
  // Coptic Capital Letter Dialect-P Ni is normalized to small letter.
  ["xn--abcdef-yy8d.com", "abc\u2cbbdef.com", kUnsafe],

  // Block NV8 (Not valid in IDN 2008) characters.
  // U+058A (֊)
  ["xn--ab-vfd.com", "a\u058ab.com", kUnsafe],
  ["xn--y9ac3j.com", "\u0561\u058a\u0562.com", kUnsafe],
  // U+2019 (’)
  ["xn--ab-n2t.com", "a\u2019b.com", kUnsafe],
  // U+2027 (‧)
  ["xn--ab-u3t.com", "a\u2027b.com", kUnsafe],
  // U+30A0 (゠)
  ["xn--ab-bg4a.com", "a\u30a0b.com", kUnsafe],
  ["xn--9bk3828aea.com", "\uac00\u30a0\uac01.com", kUnsafe],
  ["xn--9bk279fba.com", "\u4e00\u30a0\u4e00.com", kUnsafe],
  ["xn--n8jl2x.com", "\u304a\u30a0\u3044.com", kUnsafe],
  ["xn--fbke7f.com", "\u3082\u30a0\u3084.com", kUnsafe],

  // Block single/double-quote-like characters.
  // U+02BB (ʻ)
  ["xn--ab-8nb.com", "a\u02bbb.com", kUnsafe, "DISABLED"],
  // U+02BC (ʼ)
  ["xn--ab-cob.com", "a\u02bcb.com", kUnsafe, "DISABLED"],
  // U+144A: Not allowed to mix with scripts other than Canadian Syllabics.
  ["xn--ab-jom.com", "a\u144ab.com", kUnsafe],
  ["xn--xcec9s.com", "\u1401\u144a\u1402.com", kUnsafe],

  // Custom dangerous patterns
  // Two Katakana-Hiragana combining mark in a row
  ["google.xn--com-oh4ba.evil.jp", "google.com\u309a\u309a.evil.jp", kUnsafe],
  // Katakana Letter No not enclosed by {Han,Hiragana,Katakana}.
  ["google.xn--comevil-v04f.jp", "google.com\u30ceevil.jp", kUnsafe, "DISABLED"],
  // TODO(jshin): Review the danger of allowing the following two.
  // Hiragana 'No' by itself is allowed.
  ["xn--ldk.jp", "\u30ce.jp", kSafe],
  // Hebrew Gershayim used by itself is allowed.
  ["xn--5eb.il", "\u05f4.il", kSafe, "DISABLED"],

  // Block RTL nonspacing marks (NSM) after unrelated scripts.
  ["xn--foog-ycg.com", "foog\u0650.com", kUnsafe],    // Latin + Arabic N]M
  ["xn--foog-jdg.com", "foog\u0654.com", kUnsafe],    // Latin + Arabic N]M
  ["xn--foog-jhg.com", "foog\u0670.com", kUnsafe],    // Latin + Arbic N]M
  ["xn--foog-opf.com", "foog\u05b4.com", kUnsafe],    // Latin + Hebrew N]M
  ["xn--shb5495f.com", "\uac00\u0650.com", kUnsafe],  // Hang + Arabic N]M

  // 4 Deviation characters between IDNA 2003 and IDNA 2008
  // When entered in Unicode, the first two are mapped to 'ss' and Greek sigma
  // and the latter two are mapped away. However, the punycode form should
  // remain in punycode.
  // U+00DF(sharp-s)
  ["xn--fu-hia.de", "fu\u00df.de", kUnsafe, "DISABLED"],
  // U+03C2(final-sigma)
  ["xn--mxac2c.gr", "\u03b1\u03b2\u03c2.gr", kUnsafe, "DISABLED"],
  // U+200C(ZWNJ)
  ["xn--h2by8byc123p.in", "\u0924\u094d\u200c\u0930\u093f.in", kUnsafe],
  // U+200C(ZWJ)
  ["xn--11b6iy14e.in", "\u0915\u094d\u200d.in", kUnsafe],

  // Math Monospace Small A. When entered in Unicode, it's canonicalized to
  // 'a'. The punycode form should remain in punycode.
  ["xn--bc-9x80a.xyz", "\U0001d68abc.xyz", kInvalid],
  // Math Sans Bold Capital Alpha
  ["xn--bc-rg90a.xyz", "\U0001d756bc.xyz", kInvalid],
  // U+3000 is canonicalized to a space(U+0020), but the punycode form
  // should remain in punycode.
  ["xn--p6j412gn7f.cn", "\u4e2d\u56fd\u3000", kInvalid],
  // U+3002 is canonicalized to ASCII fullstop(U+002E), but the punycode form
  // should remain in punycode.
  ["xn--r6j012gn7f.cn", "\u4e2d\u56fd\u3002", kInvalid],
  // Invalid punycode
  // Has a codepoint beyond U+10FFFF.
  ["xn--krank-kg706554a", "", kInvalid],
  // '?' in punycode.
  ["xn--hello?world.com", "", kInvalid],

  // Not allowed in UTS46/IDNA 2008
  // Georgian Capital Letter(U+10BD)
  ["xn--1nd.com", "\u10bd.com", kInvalid],
  // 3rd and 4th characters are '-'.
  ["xn-----8kci4dhsd", "\u0440\u0443--\u0430\u0432\u0442\u043e", kInvalid],
  // Leading combining mark
  ["xn--72b.com", "\u093e.com", kInvalid],
  // BiDi check per IDNA 2008/UTS 46
  // Cannot starts with AN(Arabic-Indic Number)
  ["xn--8hbae.eg", "\u0662\u0660\u0660.eg", kInvalid],
  // Cannot start with a RTL character and ends with a LTR
  ["xn--x-ymcov.eg", "\u062c\u0627\u0631x.eg", kInvalid],
  // Can start with a RTL character and ends with EN(European Number)
  ["xn--2-ymcov.eg", "\u062c\u0627\u06312.eg", kSafe],
  // Can start with a RTL and end with AN
  ["xn--mgbjq0r.eg", "\u062c\u0627\u0631\u0662.eg", kSafe],

  // Extremely rare Latin letters
  // Latin Ext B - Pinyin: ǔnion.com
  ["xn--nion-unb.com", "\u01d4nion.com", kUnsafe, "DISABLED"],
  // Latin Ext C: ⱴase.com
  ["xn--ase-7z0b.com", "\u2c74ase.com", kUnsafe],
  // Latin Ext D: ꝴode.com
  ["xn--ode-ut3l.com", "\ua774ode.com", kUnsafe],
  // Latin Ext Additional: ḷily.com
  ["xn--ily-n3y.com", "\u1e37ily.com", kUnsafe, "DISABLED"],
  // Latin Ext E: ꬺove.com
  ["xn--ove-8y6l.com", "\uab3aove.com", kUnsafe],
  // Greek Ext: ᾳβγ.com
  ["xn--nxac616s.com", "\u1fb3\u03b2\u03b3.com", kInvalid],
  // Cyrillic Ext A (label cannot begin with an illegal combining character).
  ["xn--lrj.com", "\u2def.com", kInvalid],
  // Cyrillic Ext B: ꙡ.com
  ["xn--kx8a.com", "\ua661.com", kUnsafe],
  // Cyrillic Ext C: ᲂ.com (Narrow o)
  ["xn--43f.com", "\u1c82.com", kInvalid],

  // The skeleton of Extended Arabic-Indic Digit Zero (۰) is a dot. Check that
  // this is handled correctly (crbug/877045).
  ["xn--dmb", "\u06f0", kSafe],

  // Test that top domains whose skeletons are the same as the domain name are
  // handled properly. In this case, tést.net should match test.net top
  // domain and not be converted to unicode.
  ["xn--tst-bma.net", "t\u00e9st.net", kUnsafe, "DISABLED"],
  // Variations of the above, for testing crbug.com/925199.
  // some.tést.net should match test.net.
  ["some.xn--tst-bma.net", "some.t\u00e9st.net", kUnsafe, "DISABLED"],
  // The following should not match test.net, so should be converted to
  // unicode.
  // ést.net (a suffix of tést.net).
  ["xn--st-9ia.net", "\u00e9st.net", kSafe],
  // some.ést.net
  ["some.xn--st-9ia.net", "some.\u00e9st.net", kSafe],
  // atést.net (tést.net is a suffix of atést.net)
  ["xn--atst-cpa.net", "at\u00e9st.net", kSafe],
  // some.atést.net
  ["some.xn--atst-cpa.net", "some.at\u00e9st.net", kSafe],

  // Modifier-letter-voicing should be blocked (wwwˬtest.com).
  ["xn--wwwtest-2be.com", "www\u02ectest.com", kUnsafe, "DISABLED"],

  // oĸ.com: Not a top domain, should be blocked because of Kra.
  ["xn--o-tka.com", "o\u0138.com", kUnsafe],

  // U+4E00 and U+3127 should be blocked when next to non-CJK.
  ["xn--ipaddress-w75n.com", "ip\u4e00address.com", kUnsafe, "DISABLED"],
  ["xn--ipaddress-wx5h.com", "ip\u3127address.com", kUnsafe, "DISABLED"],
  // U+4E00 and U+3127 at the beginning and end of a string.
  ["xn--google-gg5e.com", "google\u3127.com", kUnsafe, "DISABLED"],
  ["xn--google-9f5e.com", "\u3127google.com", kUnsafe, "DISABLED"],
  ["xn--google-gn7i.com", "google\u4e00.com", kUnsafe, "DISABLED"],
  ["xn--google-9m7i.com", "\u4e00google.com", kUnsafe, "DISABLED"],
  // These are allowed because U+4E00 and U+3127 are not immediately next to
  // non-CJK.
  ["xn--gamer-fg1hz05u.com", "\u4e00\u751fgamer.com", kSafe],
  ["xn--gamer-kg1hy05u.com", "gamer\u751f\u4e00.com", kSafe],
  ["xn--gamer-f94d4426b.com", "\u3127\u751fgamer.com", kSafe],
  ["xn--gamer-k94d3426b.com", "gamer\u751f\u3127.com", kSafe],
  ["xn--4gqz91g.com", "\u4e00\u732b.com", kSafe],
  ["xn--4fkv10r.com", "\u3127\u732b.com", kSafe],
  // U+4E00 with another ideograph.
  ["xn--4gqc.com", "\u4e00\u4e01.com", kSafe],

  // CJK ideographs looking like slashes should be blocked when next to
  // non-CJK.
  ["example.xn--comtest-k63k", "example.com\u4e36test", kUnsafe, "DISABLED"],
  ["example.xn--comtest-u83k", "example.com\u4e40test", kUnsafe, "DISABLED"],
  ["example.xn--comtest-283k", "example.com\u4e41test", kUnsafe, "DISABLED"],
  ["example.xn--comtest-m83k", "example.com\u4e3ftest", kUnsafe, "DISABLED"],
  // This is allowed because the ideographs are not immediately next to
  // non-CJK.
  ["xn--oiqsace.com", "\u4e36\u4e40\u4e41\u4e3f.com", kSafe],

  // Kana voiced sound marks are not allowed.
  ["xn--google-1m4e.com", "google\u3099.com", kUnsafe],
  ["xn--google-8m4e.com", "google\u309A.com", kUnsafe],

  // Small letter theta looks like a zero.
  ["xn--123456789-yzg.com", "123456789\u03b8.com", kUnsafe, "DISABLED"],

  ["xn--est-118d.net", "\u4e03est.net", kUnsafe, "DISABLED"],
  ["xn--est-918d.net", "\u4e05est.net", kUnsafe, "DISABLED"],
  ["xn--est-e28d.net", "\u4e06est.net", kUnsafe, "DISABLED"],
  ["xn--est-t18d.net", "\u4e01est.net", kUnsafe, "DISABLED"],
  ["xn--3-cq6a.com", "\u4e293.com", kUnsafe, "DISABLED"],
  ["xn--cxe-n68d.com", "c\u4e2bxe.com", kUnsafe, "DISABLED"],
  ["xn--cye-b98d.com", "cy\u4e42e.com", kUnsafe, "DISABLED"],

  // U+05D7 can look like Latin n in many fonts.
  ["xn--ceba.com", "\u05d7\u05d7.com", kUnsafe, "DISABLED"],

  // U+00FE (þ) and U+00F0 (ð) are only allowed under the .is TLD.
  ["xn--acdef-wva.com", "a\u00fecdef.com", kUnsafe, "DISABLED"],
  ["xn--mnpqr-jta.com", "mn\u00f0pqr.com", kUnsafe, "DISABLED"],
  ["xn--acdef-wva.is", "a\u00fecdef.is", kSafe],
  ["xn--mnpqr-jta.is", "mn\u00f0pqr.is", kSafe],

  // U+0259 (ə) is only allowed under the .az TLD.
  ["xn--xample-vyc.com", "\u0259xample.com", kUnsafe, "DISABLED"],
  ["xn--xample-vyc.az", "\u0259xample.az", kSafe],

  // U+00B7 is only allowed on Catalan domains between two l's.
  ["xn--googlecom-5pa.com", "google\u00b7com.com", kUnsafe, "DISABLED"],
  ["xn--ll-0ea.com", "l\u00b7l.com", kUnsafe, "DISABLED"],
  ["xn--ll-0ea.cat", "l\u00b7l.cat", kSafe],
  ["xn--al-0ea.cat", "a\u00b7l.cat", kUnsafe, "DISABLED"],
  ["xn--la-0ea.cat", "l\u00b7a.cat", kUnsafe, "DISABLED"],
  ["xn--l-fda.cat", "\u00b7l.cat", kUnsafe, "DISABLED"],
  ["xn--l-gda.cat", "l\u00b7.cat", kUnsafe, "DISABLED"],

  ["xn--googlecom-gk6n.com", "google\u4e28com.com", kUnsafe, "DISABLED"],
  ["xn--googlecom-0y6n.com", "google\u4e5bcom.com", kUnsafe, "DISABLED"],
  ["xn--googlecom-v85n.com", "google\u4e03com.com", kUnsafe, "DISABLED"],
  ["xn--googlecom-g95n.com", "google\u4e05com.com", kUnsafe, "DISABLED"],
  ["xn--googlecom-go6n.com", "google\u4e36com.com", kUnsafe, "DISABLED"],
  ["xn--googlecom-b76o.com", "google\u5341com.com", kUnsafe, "DISABLED"],
  ["xn--googlecom-ql3h.com", "google\u3007com.com", kUnsafe, "DISABLED"],
  ["xn--googlecom-0r5h.com", "google\u3112com.com", kUnsafe, "DISABLED"],
  ["xn--googlecom-bu5h.com", "google\u311acom.com", kUnsafe, "DISABLED"],
  ["xn--googlecom-qv5h.com", "google\u311fcom.com", kUnsafe, "DISABLED"],
  ["xn--googlecom-0x5h.com", "google\u3127com.com", kUnsafe, "DISABLED"],
  ["xn--googlecom-by5h.com", "google\u3128com.com", kUnsafe, "DISABLED"],
  ["xn--googlecom-ly5h.com", "google\u3129com.com", kUnsafe, "DISABLED"],
  ["xn--googlecom-5o5h.com", "google\u3108com.com", kUnsafe, "DISABLED"],
  ["xn--googlecom-075n.com", "google\u4e00com.com", kUnsafe, "DISABLED"],
  ["xn--googlecom-046h.com", "google\u31bacom.com", kUnsafe, "DISABLED"],
  ["xn--googlecom-026h.com", "google\u31b3com.com", kUnsafe, "DISABLED"],
  ["xn--googlecom-lg9q.com", "google\u5de5com.com", kUnsafe, "DISABLED"],
  ["xn--googlecom-g040a.com", "google\u8ba0com.com", kUnsafe, "DISABLED"],
  ["xn--googlecom-b85n.com", "google\u4e01com.com", kUnsafe, "DISABLED"],

  // Whole-script-confusables. Cyrillic is sufficiently handled in cases above
  // so it's not included here.
  // Armenian:
  ["xn--mbbkpm.com", "\u0578\u057d\u0582\u0585.com", kUnsafe, "DISABLED"],
  ["xn--mbbkpm.am", "\u0578\u057d\u0582\u0585.am", kSafe],
  ["xn--mbbkpm.xn--y9a3aq", "\u0578\u057d\u0582\u0585.\u0570\u0561\u0575", kSafe],
  // Ethiopic:
  ["xn--6xd66aa62c.com", "\u1220\u12d0\u12d0\u1350.com", kUnsafe, "DISABLED"],
  ["xn--6xd66aa62c.et", "\u1220\u12d0\u12d0\u1350.et", kSafe],
  ["xn--6xd66aa62c.xn--m0d3gwjla96a",     "\u1220\u12d0\u12d0\u1350.\u12a2\u1275\u12ee\u1335\u12eb", kSafe],
  // Greek:
  ["xn--mxapd.com", "\u03b9\u03ba\u03b1.com", kUnsafe, "DISABLED"],
  ["xn--mxapd.gr", "\u03b9\u03ba\u03b1.gr", kSafe],
  ["xn--mxapd.xn--qxam", "\u03b9\u03ba\u03b1.\u03b5\u03bb", kSafe],
  // Georgian:
  ["xn--gpd3ag.com", "\u10fd\u10ff\u10ee.com", kUnsafe, "DISABLED"],
  ["xn--gpd3ag.ge", "\u10fd\u10ff\u10ee.ge", kSafe],
  ["xn--gpd3ag.xn--node", "\u10fd\u10ff\u10ee.\u10d2\u10d4", kSafe],
  // Hebrew:
  ["xn--7dbh4a.com", "\u05d7\u05e1\u05d3.com", kUnsafe, "DISABLED"],
  ["xn--7dbh4a.il", "\u05d7\u05e1\u05d3.il", kSafe],
  ["xn--9dbq2a.xn--7dbh4a", "\u05e7\u05d5\u05dd.\u05d7\u05e1\u05d3", kSafe],
  // Myanmar:
  ["xn--oidbbf41a.com", "\u1004\u1040\u1002\u1001\u1002.com", kUnsafe, "DISABLED"],
  ["xn--oidbbf41a.mm", "\u1004\u1040\u1002\u1001\u1002.mm", kSafe],
  ["xn--oidbbf41a.xn--7idjb0f4ck",     "\u1004\u1040\u1002\u1001\u1002.\u1019\u103c\u1014\u103a\u1019\u102c", kSafe],
  // Myanmar Shan digits:
  ["xn--rmdcmef.com", "\u1090\u1091\u1095\u1096\u1097.com", kUnsafe, "DISABLED"],
  ["xn--rmdcmef.mm", "\u1090\u1091\u1095\u1096\u1097.mm", kSafe],
  ["xn--rmdcmef.xn--7idjb0f4ck",     "\u1090\u1091\u1095\u1096\u1097.\u1019\u103c\u1014\u103a\u1019\u102c", kSafe],
// Thai:
// #if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  ["xn--o3cedqz2c.com", "\u0e17\u0e19\u0e1a\u0e1e\u0e23\u0e2b.com", kUnsafe, "DISABLED"],
  ["xn--o3cedqz2c.th", "\u0e17\u0e19\u0e1a\u0e1e\u0e23\u0e2b.th", kSafe],
  ["xn--o3cedqz2c.xn--o3cw4h",     "\u0e17\u0e19\u0e1a\u0e1e\u0e23\u0e2b.\u0e44\u0e17\u0e22", kSafe],
// #else
  ["xn--r3ch7hsc.com", "\u0e1e\u0e1a\u0e40\u0e50.com", kUnsafe, "DISABLED"],
  ["xn--r3ch7hsc.th", "\u0e1e\u0e1a\u0e40\u0e50.th", kSafe],
  ["xn--r3ch7hsc.xn--o3cw4h", "\u0e1e\u0e1a\u0e40\u0e50.\u0e44\u0e17\u0e22", kSafe],
// #endif

  // Indic scripts:
  // Bengali:
  ["xn--07baub.com", "\u09e6\u09ed\u09e6\u09ed.com", kUnsafe, "DISABLED"],
  // Devanagari:
  ["xn--62ba6j.com", "\u093d\u0966\u093d.com", kUnsafe, "DISABLED"],
  // Gujarati:
  ["xn--becd.com", "\u0aa1\u0a9f.com", kUnsafe, "DISABLED"],
  // Gurmukhi:
  ["xn--occacb.com", "\u0a66\u0a67\u0a66\u0a67.com", kUnsafe, "DISABLED"],
  // Kannada:
  ["xn--stca6jf.com", "\u0cbd\u0ce6\u0cbd\u0ce7.com", kUnsafe, "DISABLED"],
  // Malayalam:
  ["xn--lwccv.com", "\u0d1f\u0d20\u0d27.com", kUnsafe, "DISABLED"],
  // Oriya:
  ["xn--zhca6ub.com", "\u0b6e\u0b20\u0b6e\u0b20.com", kUnsafe, "DISABLED"],
  // Tamil:
  ["xn--mlca6ab.com", "\u0b9f\u0baa\u0b9f\u0baa.com", kUnsafe, "DISABLED"],
  // Telugu:
  ["xn--brcaabbb.com", "\u0c67\u0c66\u0c67\u0c66\u0c67\u0c66.com", kUnsafe, "DISABLED"],

  // IDN domain matching an IDN top-domain (f\u00f3\u00f3.com)
  ["xn--fo-5ja.com", "f\u00f3o.com", kUnsafe, "DISABLED"],

  // crbug.com/769547: Subdomains of top domains should be allowed.
  ["xn--xample-9ua.test.net", "\u00e9xample.test.net", kSafe],
  // Skeleton of the eTLD+1 matches a top domain, but the eTLD+1 itself is
  // not a top domain. Should not be decoded to unicode.
  ["xn--xample-9ua.test.xn--nt-bja", "\u00e9xample.test.n\u00e9t", kUnsafe, "DISABLED"],

  // Digit lookalike check of 16კ.com with character “კ” (U+10D9)
  // Test case for https://crbug.com/1156531
  ["xn--16-1ik.com", "16\u10d9.com", kUnsafe, "DISABLED"],

  // Skeleton generator check of officeკ65.com with character “კ” (U+10D9)
  // Test case for https://crbug.com/1156531
  ["xn--office65-l04a.com", "office\u10d965.com", kUnsafe],

  // Digit lookalike check of 16ੜ.com with character “ੜ” (U+0A5C)
  // Test case for https://crbug.com/1156531 (missed skeleton map)
  ["xn--16-ogg.com", "16\u0a5c.com", kUnsafe, "DISABLED"],

  // Skeleton generator check of officeੜ65.com with character “ੜ” (U+0A5C)
  // Test case for https://crbug.com/1156531 (missed skeleton map)
  ["xn--office65-hts.com", "office\u0a5c65.com", kUnsafe],

  // New test cases go ↑↑ above.

  // /!\ WARNING: You MUST use tools/security/idn_test_case_generator.py to
  // generate new test cases, as specified by the comment at the top of this
  // test list. Why must you use that python script?
  // 1. It is easy to get things wrong. There were several hand-crafted
  //    incorrect test cases committed that was later fixed.
  // 2. This test _also_ is a test of Chromium's IDN encoder/decoder, so using
  //    Chromium's IDN encoder/decoder to generate test files loses an
  //    advantage of having Python's IDN encode/decode the tests.
];

function checkEquals(a, b, message, expectedFail) {
  if (!expectedFail) {
    Assert.equal(a, b, message);
  } else {
    Assert.notEqual(a, b, `EXPECTED-FAIL: ${message}`);
  }
}

add_task(async function test_chrome_spoofs() {
  for (let test of testCases) {
    let isAscii = {};
    let result = idnService.convertToDisplayIDN(test[0], isAscii);
    let expectedFail = test.length == 4 && test[3] == "DISABLED";
    if (test[2] == kSafe) {
      checkEquals(
        result,
        test[1],
        `kSafe label ${test[0]} should convert to ${test[1]}`,
        expectedFail
      );
    } else if (test[2] == kUnsafe) {
      checkEquals(
        result,
        test[0],
        `kUnsafe label ${test[0]} should not convert to ${test[1]}`,
        expectedFail
      );
    } else if (test[2] == kInvalid) {
      checkEquals(
        result,
        test[0],
        `kInvalid label ${test[0]} should stay the same`,
        expectedFail
      );
    }
  }
});
