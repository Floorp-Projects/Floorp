# Taskcluster URL Building Library

[![License](https://img.shields.io/badge/license-MPL%202.0-orange.svg)](http://mozilla.org/MPL/2.0)

A simple library to generate URLs for various Taskcluster resources across our various deployment methods.

This serves as both a simple shim for projects that use JavaScript but also is the reference implementation for
how we define these paths.

URLs are defined in the 'Taskcluster URL Format' document.

Changelog
---------
View the changelog on the [releases page](https://github.com/taskcluster/taskcluster-lib-urls/releases).

Requirements
------------

This is tested on and should run on any of Node.js `{8, 10}`.

JS Usage
--------
[![Node.js Build Status](https://travis-ci.org/taskcluster/taskcluster-lib-urls.svg?branch=master)](https://travis-ci.org/taskcluster/taskcluster-lib-urls)
[![npm](https://img.shields.io/npm/v/taskcluster-lib-urls.svg?maxAge=2592000)](https://www.npmjs.com/package/taskcluster-lib-urls)

This package exports several methods for generating URLs conditionally based on
a root URL, as well as a few helper classes for generating URLs for a pre-determined
root URL:

* `api(rootUrl, service, version, path)` -> `String`
* `apiReference(rootUrl, service, version)` -> `String`
* `docs(rootUrl, path)` -> `String`
* `exchangeReference(rootUrl, service, version)` -> `String`
* `schema(rootUrl, service, schema)` -> `String`
* `ui(rootUrl, path)` -> `String`
* `servicesManifest(rootUrl)` -> `String`
* `testRootUrl()` -> `String`
* `withRootUrl(rootUrl)` -> `Class` instance for above methods

When the `rootUrl` is `https://taskcluster.net`, the generated URLs will be to the Heroku cluster. Otherwise they will follow the
[spec defined in this project](https://github.com/taskcluster/taskcluster-lib-urls/tree/master/docs/urls-spec.md).

`testRootUrl()` is used to share a common fake `rootUrl` between various Taskcluster mocks in testing.
The URL does not resolve.

```js
// Specifying root URL every time:
const libUrls = require('taskcluster-lib-urls');

libUrls.api(rootUrl, 'auth', 'v1', 'foo/bar');
libUrls.schema(rootUrl, 'auth', 'v1/foo.yml'); // Note that schema names have versions in them
libUrls.apiReference(rootUrl, 'auth', 'v1');
libUrls.exchangeReference(rootUrl, 'auth', 'v1');
libUrls.ui(rootUrl, 'foo/bar');
libUrls.servicesManifest(rootUrl);
libUrls.docs(rootUrl, 'foo/bar');
```

```js
// Specifying root URL in advance:
const libUrls = require('taskcluster-lib-urls');

const urls = libUrls.withRoot(rootUrl);

urls.api('auth', 'v1', 'foo/bar');
urls.schema('auth', 'v1/foo.yml');
urls.apiReference('auth', 'v1');
urls.exchangeReference('auth', 'v1');
urls.ui('foo/bar');
urls.servicesManifest();
urls.docs('foo/bar');
```

If you would like, you can set this up via [taskcluster-lib-loader](https://github.com/taskcluster/taskcluster-lib-loader) as follows:

```js
{
  libUrlss: {
    require: ['cfg'],
    setup: ({cfg}) => withRootUrl(cfg.rootURl),
  },
}
```

Test with:

```
yarn install
yarn test
```


Go Usage
--------

[![GoDoc](https://godoc.org/github.com/taskcluster/taskcluster-lib-urls?status.svg)](https://godoc.org/github.com/taskcluster/taskcluster-lib-urls)

The go package exports the following functions:

```go
func API(rootURL string, service string, version string, path string) string
func APIReference(rootURL string, service string, version string) string
func Docs(rootURL string, path string) string
func ExchangeReference(rootURL string, service string, version string) string
func Schema(rootURL string, service string, name string) string
func UI(rootURL string, path string) string
func ServicesManifest(rootURL string) string
```

Install with:

```
go install ./..
```

Test with:

```
go test -v ./...
```

Python Usage
------------

You can install the python client with `pip install taskcluster-urls`;

```python
import taskcluster_urls

taskcluster_urls.api(root_url, 'auth', 'v1', 'foo/bar')
taskcluster_urls.schema(root_url, 'auth', 'v1/foo.yml') # Note that schema names have versions in them
taskcluster_urls.api_reference(root_url, 'auth', 'v1')
taskcluster_urls.exchange_reference(root_url, 'auth', 'v1')
taskcluster_urls.ui(root_url, 'foo/bar')
taskcluster_urls.servicesManifest(root_url)
taskcluster_urls.docs(root_url, 'foo/bar')

And for testing,
```python
taskcluster_urls.test_root_url()
```

Test with:

```
tox
```

Java Usage
----------

[![JavaDoc](https://img.shields.io/badge/javadoc-reference-blue.svg)](http://taskcluster.github.io/taskcluster-lib-urls/apidocs)

In order to use this library from your maven project, simply include it as a project dependency:

```
<project>
  ...
  <dependencies>
    ...
    <dependency>
      <groupId>org.mozilla.taskcluster</groupId>
      <artifactId>taskcluster-lib-urls</artifactId>
      <version>1.0.0</version>
    </dependency>
  </dependencies>
</project>
```

The taskcluster-lib-urls artifacts are now available from the [maven central repository](http://central.sonatype.org/):

* [Search Results](http://search.maven.org/#search|gav|1|g%3A%22org.mozilla.taskcluster%22%20AND%20a%3A%22taskcluster-lib-urls%22)
* [Directory Listing](https://repo1.maven.org/maven2/org/mozilla/taskcluster/taskcluster-lib-urls/)

To use the library, do as follows:

```java
import org.mozilla.taskcluster.urls.*;

...

    URLProvider urlProvider = URLs.provider("https://mytaskcluster.acme.org");

    String fooBarAPI        = urlProvider.api("auth", "v1", "foo/bar");
    String fooSchema        = urlProvider.schema("auth", "v1/foo.yml"); // Note that schema names have versions in them
    String authAPIRef       = urlProvider.apiReference("auth", "v1");
    String authExchangesRef = urlProvider.exchangeReference("auth", "v1");
    String uiFooBar         = urlProvider.ui("foo/bar");
    String servicesManifest = urlProvider.servicesManifest();
    String docsFooBar       = urlProvider.docs("foo/bar");

...
```

Install with:

```
mvn install
```

Test with:

```
mvn test
```


Releasing
---------

New releases should be tested on Travis and Taskcluster to allow for all supported versions of various languages to be tested. Once satisfied that it works, new versions should be created with
`npm version` rather than by manually editing `package.json` and tags should be pushed to Github. 

Make the Node release first, as Python's version depends on its `package.json`.  This follows the typical tag-and-push-to-publish approach:

```sh
$ npm version minor  # or patch, or major
$ git push upstream
```

Once that's done, build the Python sdists (only possible by the [maintainers on pypi](https://pypi.org/project/taskcluster-urls/#files)):

```sh
rm -rf dist/*
python setup.py sdist bdist_wheel
python3 setup.py bdist_wheel
pip install twine
twine upload dist/*
```

Make sure to update [the changelog](https://github.com/taskcluster/taskcluster-lib-urls/releases)!

License
-------

[Mozilla Public License Version 2.0](https://github.com/taskcluster/taskcluster-lib-urls/blob/master/LICENSE)
