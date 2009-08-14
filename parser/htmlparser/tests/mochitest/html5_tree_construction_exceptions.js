/* these are the tests we don't pass */
var html5TreeConstructionExceptions = {
  "<isindex test=x name=x>": true,
  "<select><keygen>": true,
  "<!DOCTYPE html><body xlink:href=foo><math xlink:href=foo></math>": true,
  "<!DOCTYPE html><body xlink:href=foo xml:lang=en><math><mi xml:lang=en xlink:href=foo></mi></math>": true,
  "<!DOCTYPE html><body xlink:href=foo xml:lang=en><math><mi xml:lang=en xlink:href=foo /></math>": true,
  "<!DOCTYPE html><body xlink:href=foo xml:lang=en><math><mi xml:lang=en xlink:href=foo />bar</math>": true,
  "<!DOCTYPE html><body xlink:href=foo><svg xlink:href=foo></svg>": true,
  "<!DOCTYPE html><body xlink:href=foo xml:lang=en><svg><g xml:lang=en xlink:href=foo></g></svg>": true,
  "<!DOCTYPE html><body xlink:href=foo xml:lang=en><svg><g xml:lang=en xlink:href=foo /></svg>": true,
  "<!DOCTYPE html><body xlink:href=foo xml:lang=en><svg><g xml:lang=en xlink:href=foo />bar</svg>": true,
  "<body><body><base><link><meta><title><p></title><body><p>#</body>": true,
  "<!doctype html><body><title>X</title><meta name=y><link rel=foo><style>\u000ax { content:\"</style\" } </style>": true,
  "<!DOCTYPE html><body><table><tr><td></td><input tYPe=\"\u0009hiDDen\u0009\"><td></td></tr></table></body>": true,
  "<!DOCTYPE html><html><head></head><form><input></form><frameset rows=\"*\"><frame></frameset></html>": true,
  "<!DOCTYPE html><html><head></head><form><input type=\"hidden\"></form><frameset rows=\"*\"><frame></frameset></html>": true,
  "<!DOCTYPE html><html><head></head><form><input tYpE=\"\u0009HIdDen\u0009\"></form><frameset rows=\"*\"><frame></frameset></html>": true,
  "<!DOCTYPE html><html><body><table><link><tr><td>Hi!</td></tr></table></html>": true,
}

var html5TokenizerExceptions = {
  "<xmp> &amp; <!-- &amp; --> &amp; </xmp>": true,
  "a\uFFFFb": true,
  "a\u000cb": true,
  "<!-- foo -- bar -->": true,
}

var html5JSCompareExceptions = {
   "<table><tr></strong></b></em></i></u></strike></s></blink></tt></pre></big></small></font></select></h1></h2></h3></h4></h5></h6></body></br></a></img></title></span></style></script></table></th></td></tr></frame></area></link></param></hr></input></col></base></meta></basefont></bgsound></embed></spacer></p></dd></dt></caption></colgroup></tbody></tfoot></thead></address></blockquote></center></dir></div></dl></fieldset></listing></menu></ol></ul></li></nobr></wbr></form></button></marquee></object></html></frameset></head></iframe></image></isindex></noembed></noframes></noscript></optgroup></option></plaintext></textarea>": true,
  "<noscript><!--</noscript>--></noscript>": true,
  "<!DOCTYPE html><body xlink:href=foo><math xlink:href=foo></math>": true,
  "<!DOCTYPE html><body xlink:href=foo xml:lang=en><math><mi xml:lang=en xlink:href=foo></mi></math>": true,
  "<!DOCTYPE html><body xlink:href=foo xml:lang=en><math><mi xml:lang=en xlink:href=foo /></math>": true,
  "<!DOCTYPE html><body xlink:href=foo xml:lang=en><math><mi xml:lang=en xlink:href=foo />bar</math>": true,
  "<!DOCTYPE html><body xlink:href=foo><svg xlink:href=foo></svg>": true,
  "<!DOCTYPE html><body xlink:href=foo xml:lang=en><svg><g xml:lang=en xlink:href=foo></g></svg>": true,
  "<!DOCTYPE html><body xlink:href=foo xml:lang=en><svg><g xml:lang=en xlink:href=foo /></svg>": true,
  "<!DOCTYPE html><body xlink:href=foo xml:lang=en><svg><g xml:lang=en xlink:href=foo />bar</svg>": true,
  "<!DOCTYPE html><html><body><xyz:abc></xyz:abc>": true,
  "<!DOCTYPE html><html><html abc:def=gh><xyz:abc></xyz:abc>": true,
  "<!DOCTYPE html><html xml:lang=bar><html xml:lang=foo>": true,
}
