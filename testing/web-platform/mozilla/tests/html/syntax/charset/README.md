The tests in this directory intentionally differ from WebKit and Blink.

These are case where using the real tree builder (with `noscript`) parsing
as in the scripting enabled mode and with CDATA sections parsing with
awareness of foreign content differs from WebKit's and Blink's behavior
that works as if there was a pre-foreign content, pre-template tree builder
running in the scripting disabled mode.
