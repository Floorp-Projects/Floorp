# Chrome power API

Converted extensions declaring `power` receive the Chrome `Level` enum and
synchronous `requestKeepAwake("system" | "display")` / `releaseKeepAwake()`
methods. Chrome 155 return values and argument validation are recorded by
`test-power-reference.mjs`.

Each extension owns one effective request across its documents and background
worker. Repeating a level does not accumulate locks. A new level acquires its
replacement before releasing the former locks. Closing the issuing document or
suspending its background does not release the extension's request. Permission
revocation and extension shutdown release all owned native locks.

`chrome-extension-power-native.patch` is required with the JavaScript patch. The
existing Gecko CPU topic has no desktop OS listener, and ordinary windowless
screen locks are considered hidden. The native patch adds CPU sleep inhibition
and a privileged windowless screen lock that remains effective independently of
document visibility. Ordinary document wake-lock visibility rules stay intact.

The OS backends use GTK suspend/idle inhibitors, macOS idle-sleep/display-sleep
assertions, and Windows system/display power requests. A display request holds
both CPU and screen locks; a system request permits the display to sleep.

`ChromePowerNative.test.mts` requires the compiled backend and checks the real
PowerManagerService state, owner isolation, level replacement, repeated calls,
background suspension, release from another view, permission removal, uninstall,
and namespace isolation. It cannot pass on the older script-only test artifact.
