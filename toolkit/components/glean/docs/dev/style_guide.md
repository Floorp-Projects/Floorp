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
Linking to other pieces of documentation in the source docs requires a
link to the source file in the sphinx tree.

Links can be relative e.g. to link to the [preferences] docs:

```md
[preferences](preferences.md)
```

Or they can be absolute e.g. to link to the [Telemetry] docs:
```md
[Telemetry](/toolkit/components/telemetry/index.rst)
```

Sphinx will automagically transform that to an
appropriately-base-url'd url with a `.html` suffix.


## Use of "Main" versus "Parent" for processes

When talking about Glean, it is helpeful to distinguish when we are on the main
process specifically. This is because there is only one main process, and there is only one Glean.
However, "parent" is a relative term, and it may refer to a process
that is the parent of another process, but is not itself the main process.
Prefer use of "main" when it is accurate.

[semantic linefeeds]: https://rhodesmill.org/brandon/2012/one-sentence-per-line/
[reference link]: https://spec.commonmark.org/0.29/#reference-link
[Telemetry]: /toolkit/components/telemetry/index.rst
[#firefox-source-docs:mozilla.org]: https://chat.mozilla.org/#/room/#firefox-source-docs:mozilla.org
[bug 1621950]: https://bugzilla.mozilla.org/show_bug.cgi?id=1621950
