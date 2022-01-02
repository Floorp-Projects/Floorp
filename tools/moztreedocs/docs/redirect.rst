Redirects
=========

We now have the ability to define redirects in-tree! This will allow us to
refactor and move docs around to our hearts content without needing to worry
about stale external URLs. To set up a redirect simply add a line to this file under ``redirects`` key:

https://searchfox.org/mozilla-central/source/docs/config.yml

Any request starting with the prefix on the left, will be rewritten to the prefix on the right by the server. So for example a request to
``/testing/marionette/marionette/index.html`` will be re-written to ``/testing/marionette/index.html``. Amazon's API only supports prefix redirects, so anything more complex isn't supported.
