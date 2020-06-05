Vendoring Puppeteer
===================

As mentioned in the chapter on [Testing], we run the full [Puppeteer
test suite] on try.  These tests are vendored in central under
_remote/test/puppeteer/_ and we have a script to pull in the most
recent changes.

To update the vendored copy of Puppeteer under _remote/test/puppeteer/_:

	% ./mach remote vendor-puppeteer

[Testing]: ./Testing.html
[Puppeteer test suite]: https://github.com/GoogleChrome/puppeteer/tree/master/test
