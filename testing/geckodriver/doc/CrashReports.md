Analyzing crash data of Firefox
===============================

It's not uncommon that under some special platform configurations and while
running automated tests via Selenium and geckodriver Firefox could crash. In
those cases it is very helpful to retrieve the generated crash data aka
minidump files, and report these to us.

Retrieve the crash data
-----------------------

Because geckodriver creates a temporary user profile for Firefox, it also
automatically removes all its folders once the tests have been finished. That
also means that if Firefox crashed the created minidump files are lost. To
prevent that a custom profile has to be used instead. The following code
shows an example by using the Python Selenium bindings on Mac OS:

    import tempfile

    from selenium import webdriver
    from selenium.webdriver.firefox.options import Options

    # Custom profile folder to keep the minidump files
    profile = tempfile.mkdtemp(".selenium")
    print("*** Using profile: {}".format(profile))

    # Use the above folder as custom profile
    opts = Options()
    opts.add_argument("-profile")
    opts.add_argument(profile)
    opts.binary = "/Applications/Firefox.app/Contents/MacOS/firefox"

    driver = webdriver.Firefox(options=opts,
        # hard-code the Marionette port so geckodriver can connect
        service_args=["--marionette-port", "2828"])

    # Your test code which crashes Firefox

Executing the test with Selenium now, which triggers the crash of Firefox
will leave all the files from the user profile around in the above path.

To retrieve the minidump files navigate to that folder and look for a sub
folder with the name `minidumps`. It should contain at least one series of
files. One file with the `.dmp` extension and another one with `.extra`.
Both of those files are needed. If more crash files are present grab them all.

Attach the files as best archived as zip file to the created [geckodriver issue]
on Github.

[geckodriver issue]: https://github.com/mozilla/geckodriver/issues/new


Getting details of the crash
----------------------------

More advanced users can upload the generated minidump files themselves and
receive details information about the crash. Therefore find the [crash reporter]
folder and copy all the generated minidump files into the `pending` sub directory.
Make sure that both the `.dmp` and `.extra` files are present.

Once done you can also [view the crash reports].

If you submitted a crash please do not forget to also add the link of the
crash report to the geckodriver issue.

[crash reporter]: https://support.mozilla.org/kb/mozillacrashreporter#w_viewing-reports-outside-of-firefox
[view crash reports]: https://support.mozilla.orgkb/mozillacrashreporter#w_viewing-crash-reports




