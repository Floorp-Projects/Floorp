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

	% ./mach vendor puppeteer
	 0:00.40 Removing old checkout from remote/test/puppeteer
	 0:00.56 Checking out firefox from https://github.com/andreastt/puppeteer.git to remote/test/puppeteer
	 0:03.35 Removing unnecessary files
	 0:03.35 Updating remote/test/puppeteer/moz.yaml
	 0:03.35 Registering changes with version control
	 0:03.73 Updated Puppeteer to firefox

This will replace the current checkout with a fresh copy from
the predefined remote, which currently is the `firefox` branch on
https://github.com/andreastt/puppeteer.git.  You can override which
remote and commit to use with the `--remote` and `--committish` flags:

	  -c COMMITISH, --commitish COMMITISH
	                        Repository tag or commit to update to.
	  --remote REMOTE       Remote git repository.
	  --ignore-modified     Ignore modified files in current checkout.

[Testing]: ./Testing.html
[Puppeteer test suite]: https://github.com/GoogleChrome/puppeteer/tree/master/test
