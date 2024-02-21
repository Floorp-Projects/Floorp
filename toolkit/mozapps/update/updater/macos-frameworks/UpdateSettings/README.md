# UpdateSettings macOS Framework

## Summary

The UpdateSettings macOS Framework is used to set the accepted MAR download
channels.

## What are MAR update channels and what are they used for?

As the name implies, MAR update channels are the channels where MAR update files
are served from and we want to ensure that the updater only applies MAR files
from accepted channels.

## Why do we need a Framework instead of compiling the accepted MAR update channels directly into the executable?

There are three main use cases that make it necessary for the accepted MAR
update channels to be set by external means, such as a macOS Framework:

  1. Allowing users on the Beta channel to test RC builds

Our beta users test release candidate builds before they are released to the
release population. The MAR files related to release candidates have their MAR
channel set to `release`. We make it possible for beta users to test these
release candidate MAR files by having beta Firefox installs accept MAR files
with their internal update channel set to either `release` or `beta`.

  2. Switching users to another channel, such as ESR

In contrast to the Beta use case outlined above, there are times where we
explicitly WANT to switch users to a different channel. An example of this is
when hardware or a particular macOS version have reached their EOL. In this
case, we usually switch users to our ESR channel for extended support. We switch
users to a different channel by serving a MAR file that forces a change to the
update channels that will be accepted for future updates. In other words, while
users may have previously accepted MAR update files from the `release` channel,
they now only accept MAR files from the `esr` channel.

  3. QA update testing

QA requires a way to temporarily switch the MAR update channel to a test channel
in order to test MAR updates before new releases.

## How does the UpdateSettings macOS Framework address these use cases?

We are able to accommodate all three use cases above by enabling the updater to
ignore certain files on disk if they are already present, but continue to force
update them if so desired.

In the case of a Beta user updating to an RC build, the updater would encounter
an UpdateSettings macOS Framework inside the .app bundle that has the accepted
MAR update channels set to `beta` and `release`. In this case, the updater will
not update the Framework, but update everything else. This beta user is now able
to run the RC build with the update channel still set to `beta` and `release`
and will be able to apply MAR files related to the next beta cycle once the end
of RC builds is reached.

In the case of switching users to the ESR channel, the updater will be set to
forcefully update the UpdateSettings macOS Framework, even if already present on
disk. After the update, the user will now be set to accept MAR updates from the
`esr` channel only.

Before releases, QA replaces the UpdateSettings macOS Framework within the .app
bundle and set the accepted MAR update channels to a test channel in order to
test MAR updates. During testing, the new Framework file would remain in place
for typical update testing, but gets replaced in case QA was testing channel
switching.

## Why is a macOS Framework the best solution to store the accepted MAR update channels?

Apple has started strengthening code signature checks and the requirements on
developers such as ourselves on how their apps are signed. In particular,
most files in the .app bundle are now included in signature verifications.

A macOS Framework is the ideal solution to store the accepted MAR update
channels because Frameworks are the only component within a .app bundle that can
be replaced without invalidating the code signature on the .app bundle, as long
as both the previous and the new Framework are signed.
