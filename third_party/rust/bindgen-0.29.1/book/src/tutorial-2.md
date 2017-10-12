# Create a `wrapper.h` Header

The `wrapper.h` file will include all the various headers containing
declarations of structs and functions we would like bindings for. In the
particular case of `bzip2`, this is pretty easy since the entire public API is
contained in a single header. For a project like [SpiderMonkey][spidermonkey],
where the public API is split across multiple header files and grouped by
functionality, we'd want to include all those headers we want to bind to in this
single `wrapper.h` entry point for `bindgen`.

Here is our `wrapper.h`:

```c
#include <bzlib.h>
```

This is also where we would add any [replacement types](./replacing-types.html),
if we were using some.

[spidermonkey]: https://developer.mozilla.org/en-US/docs/Mozilla/Projects/SpiderMonkey/How_to_embed_the_JavaScript_engine
