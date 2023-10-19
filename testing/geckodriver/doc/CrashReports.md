# Analyzing crash data of Firefox

It's not uncommon that under some special platform configurations and while
running automated tests via Selenium and geckodriver Firefox could crash. In
those cases it is very helpful to retrieve the generated crash data aka
minidump files, and report these to us.

## Retrieve the crash information

Because geckodriver creates a temporary user profile for Firefox, it also
automatically removes all its folders once the tests have been finished. That
also means that if Firefox or just a tab crashed the created minidump files
cannot be retrieved. To prevent that the `MINIDUMP_SAVE_PATH` environment
variable can be used. It needs to be forwarded to geckodriver and has to point
to an existing folder on the local machine. Then, whenever a crash occurs the
related crash information will then be written to the `<uuid>.dmp` and
`<uuid>.extra` files within the given folder.

```bash
MINIDUMP_SAVE_PATH="/home/test/crashes" pytest path/to/test.py
```

By running this command Firefox will now write minidump files to that folder:

```bash
$ ls /home/test/crashes
4ad24258-ec0f-87bd-fd78-496d9170bd35.dmp
4ad24258-ec0f-87bd-fd78-496d9170bd35.extra
```

Note that both of those files are needed when you want to file an issue for
geckodriver. If more files are present grab them all.

Attach the files as best archived as zip file to the created [geckodriver issue]
on Github.

[geckodriver issue]: https://github.com/mozilla/geckodriver/issues/new

## Getting details of the crash

More advanced users can upload the generated minidump files themselves and
receive details information about the crash. Therefore find the [crash reporter]
folder and copy all the generated minidump files into the `pending` sub directory.
Make sure that both the `.dmp` and `.extra` files are present.

Once done you can also [view the crash reports].

If you submitted a crash please do not forget to also add the link of the
crash report to the geckodriver issue.

[crash reporter]: https://support.mozilla.org/kb/mozillacrashreporter#w_viewing-reports-outside-of-firefox
[view the crash reports]: https://support.mozilla.orgkb/mozillacrashreporter#w_viewing-crash-reports
