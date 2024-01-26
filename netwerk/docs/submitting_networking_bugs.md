# Submitting actionable networking bugs

So you've found a networking issue with Firefox and decided to file a bug. First of all **Thanks!**. 🎉🎉🎉

## Networking bugs lifecycle

After a bug is filed, it gets triaged by one of the Necko team members.
The engineer will consider the *steps to reproduce* then will do one of the following:
- Assign a priority. An engineer will immediately or eventually start working on the bug.
- Move the bug to another team.
- Request more info from the reporter or someone else.

A necko bug is considered triaged when it has a priority and the `[necko-triaged]` tag has been added to the whiteboard.

As a bug reporter, please do not change the `Priority` or `Severity` flags. Doing so could prevent the bug from showing up in the triage queue.

<div class="note">
<div class="admonition-title">Note</div>

> For bugs to get fixed as quickly as possible engineers should spend their time
on the actual fix, not on figuring out what might be wrong. That's why it's
important to go through the sections below and include as much information as
possible in the bug report.

</div>


## Make sure it's a Firefox bug

Sometimes a website may be misbehaving and you'll initially think it's caused by a bug in Firefox. However, extensions and other customizations could also cause an issue. Here are a few things to check before submitting the bug:
- [Troubleshoot extensions, themes and hardware acceleration issues to solve common Firefox problems](https://support.mozilla.org/en-US/kb/troubleshoot-extensions-themes-to-fix-problems#w_start-firefox-in-troubleshoot-mode)
    - This will confirm if an extension is causing the issue you're seeing. If the bug goes away, with extensions turned off, you might then want to figure out which extension is causing the problem. Turn off each extension and see if it keeps happening. Include this information in the bug report.
- [Try reproducing the bug with a new Firefox profile](https://support.mozilla.org/en-US/kb/profile-manager-create-remove-switch-firefox-profiles#w_creating-a-profile)
    - If a bug stops happening with a new profile, that could be caused by changed prefs, or some bad configuration in your active profile.
    - Make sure to include the contents of `about:support` in your bug report.
- Check if the bug also happens in other browsers

## Make sure the bug has clear steps to reproduce

This is one of the most important requirements of getting the bug fixed. Clear steps to reproduce will help the engineer figure out what the problem is.
If the bug can only be reproduced on a website that requires authentication you may provide a test account to the engineer via private email.
If a certain interaction with a web server is required to reproduce the bug, feel free to attach a small nodejs, python, etc script to the bug.

Sometimes a bug is intermittent (only happens occasionally) or the steps to reproduce it aren't obvious.
It's still important to report these bugs but they should include additional info mentioned below so the engineers have a way to investigate.

### Example 1:
```
  1. Load `http://example.com`
  2. Click on button
  3. See that nothing happens and an exception is present in the web console.
```
### Example 2:
```
  1. Download attached testcase
  2. Run testcase with the following command: `node index.js`
  3. Go to `http://localhost:8888/test` and click the button
```

## Additional questions

- Are you using a proxy? What kind?
-  Are you using DNS-over-HTTPS?
    -  If the `DoH mode` at about:networking#dns is 2 or 3 then the answer is yes.
-  What platform are you using? (Operating system, Linux distribution, etc)
    - It's best to simply copy the output of `about:support`

## MozRegression

If a bug is easy to reproduce and you think it used to work before, consider using MozRegression to track down when/what started causing this issue.

First you need to [install the tool](https://mozilla.github.io/mozregression/install.html). Then just follow [the instructions](https://mozilla.github.io/mozregression/quickstart.html) presented by mozregression. Reproducing the bug a dozen times might be necessary before the tool tracks down the cause.

At the end you will be presented with a regression range that includes the commits that introduced the bug.

## Performance issues

If you're seeing a performance issue (site is very slow to load, etc) you should consider submitting a performance profile.

- Activate the profiler at: [https://profiler.firefox.com/](https://profiler.firefox.com/)
- Use the `Networking` preset and click `Start Recording`.

## Crashes

If something you're doing is causing a crash, having the link to the stack trace is very useful.

- Go to `about:crashes`
- Paste the **Report ID** of the crash in the bug.

## HTTP logs

See the [HTTP Logging](https://firefox-source-docs.mozilla.org/networking/http/logging.html) page for steps to capture HTTP logs.

If the logs are large you can create a zip archive and attach them to the bug. If the archive is still too large to attach, you can upload it to a file storage service such as Google drive or OneDrive and submit the public link.

Logs may include personal information such as cookies. Try using a fresh Firefox profile to capture the logs. If that is not possible, you can also put them in a password protected archive, or send them directly via email to the developer working on the bug.

## Wireshark dump

In some cases it is necessary to see exactly what bytes Firefox is sending and receiving over the network. When that happens, the developer working on the bug might ask you for a wireshark capture.

[Download](https://www.wireshark.org/download.html) it then run it while reproducing the bug.

If the website you're loading to reproduce the bug is over HTTPS, then it might be necessary to [decrypt the capture file](https://wiki.wireshark.org/TLS#Using_the_.28Pre.29-Master-Secret) when recording it.

## Web console and browser console errors

Sometimes a website breaks because its assumptions about executing JavaScript in Firefox are wrong. When that happens the JavaScript engine might throw exceptions which could break the website you're viewing.

When reporting a broken website or a proxy issue, also check the [web console](https://developer.mozilla.org/en-US/docs/Tools/Web_Console) `Press the Ctrl+Shift+K (Command+Option+K on OS X) keyboard shortcut` and [browser console](https://developer.mozilla.org/en-US/docs/Tools/Browser_Console) `keyboard: press Ctrl+Shift+J (or Cmd+Shift+J on a Mac).`

If they contain errors or warnings, it would be good to add them to the bug report (As text is best, but a screenshot is also acceptable).
