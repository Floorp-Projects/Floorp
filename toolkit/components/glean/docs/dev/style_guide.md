# FOG Documentation Style Guide

FOG's Documentation is written in Markdown.
You can find its source at `toolkit/components/glean/docs`.

## Line breaks

We will use [semantic linefeeds]:
* Break anywhere before 80-100 characters
* Break after any punctuation when it makes sense
* Break before or after any markdown when it makes sense

**Tip:** To keep lines narrow, use markdown's [reference link]
feature whenever it makes sense (or all the time. Up to you.).

## Linking to other documentation

Linking to other external documentation is [easy][reference link].
Linking to other pieces of documentation in the source docs might not be.

To link to another markdown page in FOG's documentation, you can use
```md
[link text](page_name.md)
```

Sphinx will automagically transform that to an
appropriately-base-url'd url with a `.html` suffix.

Unfortunately, this doesn't work for linking to
`.rst` files like those in use in [Telemetry]'s documentation.
(Follow [bug 1621950] for updates).

In those cases you have to link it like it's html.
For example, to link to [Telemetry] you can use either of
```md
[Telemetry](../telemetry)
[Telemetry](../telemetry/index.html)
```

Both will work. Both will generate warnings.
For example, the first form will generate this:
```console
None:any reference target not found: ../telemetry
```
But it will still work because linking to a directory in html links to its
`index.html` (which is where `index.rst` ends up).

We can suppress this by putting a fake anchor
(like `#https://`) on the end to fool Sphinx into not checking it.
But that seems like a hack more unseemly than the warnings,
so let's not.


[semantic linefeeds]: https://rhodesmill.org/brandon/2012/one-sentence-per-line/
[reference link]: https://spec.commonmark.org/0.29/#reference-link
[Telemetry]: ../telemetry
[#firefox-source-docs:mozilla.org]: https://chat.mozilla.org/#/room/#firefox-source-docs:mozilla.org
[bug 1621950]: https://bugzilla.mozilla.org/show_bug.cgi?id=1621950
