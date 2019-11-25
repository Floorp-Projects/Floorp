Usage
=====

geckodriver is an implementation of WebDriver, and WebDriver can
be used for widely different purposes.  How you invoke geckodriver
largely depends on your use case.


Selenium
--------

If you are using geckodriver through [Selenium], you must ensure that
you have version 3.11 or greater.  Because geckodriver implements the
[W3C WebDriver standard][WebDriver] and not the same Selenium wire
protocol older drivers are using, you may experience incompatibilities
and migration problems when making the switch from FirefoxDriver to
geckodriver.

Generally speaking, Selenium 3 enabled geckodriver as the default
WebDriver implementation for Firefox.  With the release of Firefox 47,
FirefoxDriver had to be discontinued for its lack of support for the
[new multi-processing architecture in Gecko][e10s].

Selenium client bindings will pick up the _geckodriver_ binary executable
from your [system’s `PATH` environmental variable][PATH] unless you
override it by setting the `webdriver.gecko.driver` [Java VM system
property]:

	System.setProperty("webdriver.gecko.driver", "/home/user/bin");

Or by passing it as a flag to the [java(1)] launcher:

	% java -Dwebdriver.gecko.driver=/home/user/bin YourApplication

Your mileage with this approach may vary based on which programming
language bindings you are using.  It is in any case generally the case
that geckodriver will be picked up if it is available on the system path.
In a bash compatible shell, you can make other programs aware of its
location by exporting or setting the `PATH` variable:

	% export PATH=$PATH:/home/user/bin
	% whereis geckodriver
	geckodriver: /home/user/bin/geckodriver

On Window systems you can change the system path by right-clicking **My
Computer** and choosing **Properties**.  In the dialogue that appears,
navigate **Advanced** → **Environmental Variables** → **Path**.

Or in the Windows console window:

	$ set PATH=%PATH%;C:\bin\geckodriver


Standalone
----------

Since geckodriver is a separate HTTP server that is a complete remote end
implementation of [WebDriver], it is possible to avoid using the Selenium
remote server if you have no requirements to distribute processes across
a matrix of systems.

Given a W3C WebDriver conforming client library (or _local end_) you
may interact with the geckodriver HTTP server as if you were speaking
to any Selenium server.

Using [curl(1)]:

	% geckodriver &
	[1] 16010
	% 1491834109194   geckodriver     INFO    Listening on 127.0.0.1:4444
	% curl -d '{"capabilities": {"alwaysMatch": {"acceptInsecureCerts": true}}}' http://localhost:4444/session
	{"sessionId":"d4605710-5a4e-4d64-a52a-778bb0c31e00","value":{"XULappId":"{ec8030f7-c20a-464f-9b0e-13a3a9e97384}","acceptSslCerts":false,"appBuildId":"20160913030425","browserName":"firefox","browserVersion":"51.0a1","command_id":1,"platform":"LINUX","platformName":"linux","platformVersion":"4.9.0-1-amd64","processId":17474,"proxy":{},"raisesAccessibilityExceptions":false,"rotatable":false,"specificationLevel":0,"takesElementScreenshot":true,"takesScreenshot":true,"version":"51.0a1"}}
	% curl -d '{"url": "https://mozilla.org"}' http://localhost:4444/session/d4605710-5a4e-4d64-a52a-778bb0c31e00/url
	{}
	% curl http://localhost:4444/session/d4605710-5a4e-4d64-a52a-778bb0c31e00/url
	{"value":"https://www.mozilla.org/en-US/"
	% curl -X DELETE http://localhost:4444/session/d4605710-5a4e-4d64-a52a-778bb0c31e00
	{}
	% fg
	geckodriver
	^C
	%

Using the Python [wdclient] library:

	import webdriver

	with webdriver.Session("127.0.0.1", 4444) as session:
	    session.url = "https://mozilla.org"
	    print "The current URL is %s" % session.url

And to run:

	% geckodriver &
	[1] 16054
	% python example.py
	1491835308354   geckodriver     INFO    Listening on 127.0.0.1:4444
	The current URL is https://www.mozilla.org/en-US/
	% fg
	geckodriver
	^C
	%

[Selenium]: http://seleniumhq.org/
[e10s]: https://developer.mozilla.org/en-US/Firefox/Multiprocess_Firefox
[PATH]: https://en.wikipedia.org/wiki/PATH_(variable)
[Java VM system property]: http://docs.oracle.com/javase/tutorial/essential/environment/sysprop.html
[java(1)]: http://www.manpagez.com/man/1/java/
[WebDriver]: https://w3c.github.io/webdriver/
[curl(1)]: http://www.manpagez.com/man/1/curl/
[wdclient]: https://github.com/web-platform-tests/wpt/tree/master/tools/webdriver
