# How to write WebRTC documentation

<?% config.freshness.owner = 'titovartem' %?>
<?% config.freshness.reviewed = '2021-03-01' %?>

## Audience

Engineers and tech writers who wants to contribute to WebRTC documentation

## Conceptual documentation

Conceptual documentation provides overview of APIs or systems. Examples can
be threading model of a particular module or data life cycle. Conceptual
documentation can skip some edge cases in favor of clarity. The main point
is to impart understanding.

Conceptual documentation often cannot be embedded directly within the source
code because it usually describes multiple APIs and entites, so the only
logical place to document such complex behavior is through a separate
conceptual document.

A concept document needs to be useful to both experts and novices. Moreover,
it needs to emphasize clarity, so it often needs to sacrifice completeness
and sometimes strict accuracy. That's not to say a conceptual document should
intentionally be inaccurate. It just means that is should focus more on common
usage and leave rare ones or side effects for class/function level comments.

In the WebRTC repo, conceptual documentation is located in `g3doc` subfolders
of related components. To add a new document for the component `Foo` find a
`g3doc` subfolder for this component and create a `.md` file there with
desired documentation. If there is no `g3doc` subfolder, create a new one
and add `g3doc.lua` file there with following content:

```
config = require('/g3doc/g3doc.lua')
return config
```

If you are a Googler also please specify an owner, who will be responsible for
keeping this documentation updated, by adding the next lines at the beginning
of your `.md` file immediately after page title:

```
<?\% config.freshness.owner = '<user name>' %?>
<?\% config.freshness.reviewed = '<last review date in format yyyy-mm-dd>' %?>
```

After the document is ready you should add it into `/g3doc/sitemap.md`, so it
will be visible for others.

### Documentation format

The documentation is written in g3doc, which is a markup format derived from
markdown. This is processed by multiple tools, so we recommend using only simple
markup, and previewing the documents in multiple viewers if possible.

## Class/function level comments

Documentation of specific classes and function APIs and their usage, inculding
their purpose, is embedded in the .h files defining that API. See
[C++ style guide](https://chromium.googlesource.com/chromium/src/+/master/styleguide/c++/c++.md)
for pointers on how to write API documentatin in .h files.

