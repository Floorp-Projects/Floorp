# ChannelPrefs macOS Framework

## Summary

The ChannelPrefs macOS Framework is used to initialize the `app.update.channel`
pref during startup.

## What is `app.update.channel` and what is it used for?

`app.update.channel` is used to set the download channel for application
updates.

## Why do we need a Framework instead of compiling the download channel directly into the executable?

There are three main use cases that make it necessary for the
`app.update.channel` pref to be set by external means, such as a macOS
Framework:

  1. Allowing users on the Beta channel to test RC builds

Beta users have their update channel set to 'beta'. However, RC builds by
definition have their channel set to `release`, since these are release
candidates that could get released to our release population. If we were to
indiscriminately update our Beta users to an RC build, we would lose our entire
Beta population since everyone would get switched to the `release` channel.

  2. Switching users to another channel, such as ESR

In contrast to the Beta use case outlined above, there are times where we
explicitly WANT to switch users to a different channel. An example of this is
when hardware or a particular macOS version have reached their EOL. In this
case, we usually switch users to our ESR channel for extended support.

  3. QA update testing

QA requires a way to temporarily switch the update channel to a test channel in
order to test updates before new releases.

## How does the ChannelPrefs macOS Framework address these use cases?

We are able to accommodate all three use cases above by enabling the updater to
ignore certain files on disk if they were already present, but continue to force
update them if so desired.

In the case of a Beta user updating to an RC build, the updater would encounter
a ChannelPrefs macOS Framework inside the .app bundle that has an update channel
of `beta`. In this case, the updater will not update the Framework, but update
everything else. This beta user is now able to run the RC build with the update
channel still set to `beta`.

In the case of switching users to the ESR channel, the updater will be set to
forcefully update the ChannelPrefs macOS Framework, even if already present on
disk. After the update, the user will now be set to the `esr` channel and start
receiving updates through this channel.

Before releases, QA replaces the ChannelPrefs macOS Framework within the .app
bundle and point the channel at a test channel in order to test updates. During
testing, the new Framework file would remain in place for typical update
testing, but gets replaced in case QA was testing channel switching.

## Why is a macOS Framework the best solution to store the update channel?

Apple has started strengthening code signature checks and the requirements on
developers such as ourselves on how their apps are signed. In particular,
most files in the .app bundle are now included in signature verifications.

A macOS Framework is the ideal solution to store the update channel because
Frameworks are the only component within a .app bundle that can be replaced
without invalidating the code signature on the .app bundle, as long as both the
previous and the new Framework are signed.
