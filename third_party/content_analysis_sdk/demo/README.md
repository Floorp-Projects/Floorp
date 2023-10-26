# Google Chrome Content Analysis Connector Agent SDK Demo

This directory holds the Google Chrome Content Analysis Connector Agent SDK Demo.
It contains an example of how to use the SDK.

Build instructions are available in the main project `README.md`.

## Demo agent permissions
On Microsoft Windows, if the demo agent is run without the `--user` command line
argument it must have Administrator privileges in order to properly create the
pipe used to communicate with the browser.  The demo browser must also be run
without the `--user` command line argument.

Otherwise the agent may run as any user, with or without Administrator
privileges.  The demo browser must also be run with the `--user` command line
argument and run as the same user.