Blah-blah = Uppercase letters in identifiers are not permitted.
blah-blah = This is a legal identifier.
blah_blah = Underscores in identifiers are not permitted.

bad-apostrophe-1 = The bee's knees
bad-apostrophe-end-1 = The bees' knees
bad-apostrophe-2 = The bee‘s knees
bad-single-quote = 'The bee’s knees'
ok-apostrophe = The bee’s knees
ok-single-quote = ‘The bee’s knees’
bad-double-quote = "The bee’s knees"
good-double-quote = “The bee’s knees”
bad-ellipsis = The bee’s knees...
good-ellipsis = The bee’s knees…

embedded-tag = Read more about <a data-l10n-name="privacy-policy"> our privacy policy </a>.
bad-embedded-tag = Read more about <a data-l10n-name="privacy-policy"> our 'privacy' policy </a>.

Invalid_Id = This identifier is in the exclusions file and will not cause an error.

good-has-attributes =
    .accessKey = Attribute identifiers are not checked.

bad-has-attributes =
    .accessKey = Attribute 'values' are checked.

good-function-call = Last modified: { DATETIME($timeChanged, day: "numeric", month: "long", year: "numeric") }

#   $engineName (String) - The engine name that will currently be used for the private window.
good-variable-identifier = { $engineName } is your default search engine in Private Windows

short-id = I am too short

identifiers-in-selectors-should-be-ignored =
  .label = { $tabCount ->
      [1] Send Tab to Device
      [UPPERCASE] Send Tab to Device
     *[other] Send { $tabCount } Tabs to Device
  }

this-message-reference-is-ignored =
    .label = { menu-quit.label }

ok-message-with-html-and-var = This is a <a href="{ $url }">link</a>
