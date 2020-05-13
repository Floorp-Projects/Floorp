<p align="center">
    <a href="https://sentry.io" target="_blank" align="center">
        <img src="https://sentry-brand.storage.googleapis.com/sentry-logo-black.png" width="280">
    </a>
</p>

# sentry-python - Sentry SDK for Python

[![Build Status](https://travis-ci.com/getsentry/sentry-python.svg?branch=master)](https://travis-ci.com/getsentry/sentry-python)
[![PyPi page link -- version](https://img.shields.io/pypi/v/sentry-sdk.svg)](https://pypi.python.org/pypi/sentry-sdk)
[![Discord](https://img.shields.io/discord/621778831602221064)](https://discord.gg/cWnMQeA)

This is the next line of the Python SDK for [Sentry](http://sentry.io/), intended to replace the `raven` package on PyPI.

```python
from sentry_sdk import init, capture_message

init("https://mydsn@sentry.io/123")

capture_message("Hello World")  # Will create an event.

raise ValueError()  # Will also create an event.
```

To learn more about how to use the SDK:

- [Getting started with the new SDK](https://docs.sentry.io/quickstart/?platform=python)
- [Configuration options](https://docs.sentry.io/error-reporting/configuration/?platform=python)
- [Setting context (tags, user, extra information)](https://docs.sentry.io/enriching-error-data/context/?platform=python)
- [Integrations](https://docs.sentry.io/platforms/python/)

Are you coming from raven-python?

- [Cheatsheet: Migrating to the new SDK from Raven](https://forum.sentry.io/t/switching-to-sentry-python/4733)

To learn about internals:

- [API Reference](https://getsentry.github.io/sentry-python/)

# License

Licensed under the BSD license, see `LICENSE`
