Vendoring Puppeteer
===================

As mentioned in the chapter on [Testing], we run the full [Puppeteer
test suite] on try.  These tests are vendored in central under
_remote/test/puppeteer/_ and we have a script to pull in the most
recent changes.

We currently use [a fork of Puppeteer] with a small number of tweaks
and workaround to make them run against Firefox.  These changes
will be upstreamed to Puppeteer when we feel more comfortable about
compatibility with Chrome.

To update the vendored copy of Puppeteer under _remote/test/puppeteer/_:

	% ./mach remote vendor-puppeteer

This will replace the current checkout with a fresh copy from
the predefined remote, which currently is the `firefox` branch on
https://github.com/andreastt/puppeteer.git.  You can override which
remote and commit to use.

[Testing]: ./Testing.html
[Puppeteer test suite]: https://github.com/GoogleChrome/puppeteer/tree/master/test
