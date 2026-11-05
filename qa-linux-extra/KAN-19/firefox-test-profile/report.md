# KAN19 Firefox comparison completed with official automation notification setting

2026-11-06 JST. Exact parent report path is `qa-linux-extra/KAN-19/report.md`, not outputs/KAN-19-followup.md. This follow-up qualifies the two previously unavailable Firefox native clipboard comparisons under an explicitly documented official test-profile setting. Original Windows authenticated Proton Mail remains untested; no account, personal profile or consent action was used.

## Official source and configuration boundary

Read both unchanged officialFirefox156.0.1 and159.0.1 packaged `chrome/remote/content/shared/RecommendedPreferences.sys.mjs`. Both contain `termsofuse.bypassNotification=true` under the explicit comment “Do not show TOU new user modal which can interfere with tests”. The file documents remote-control automation use, including manually supplying common prefs when remote.prefs.recommended=false. The packaged TelemetryReportingPolicy branch reads the notification pref and returns without showing the modal, separately from the actual accepted-version/date check. Exact member bytes/hashes/excerpts saved in official-test-setting.json and source/.

Fresh dedicated QA profiles set only this documented notification control in addition to the existing proxy/CA, remote.prefs.recommended=false, marionetteport and authorized app.update.disabledForTesting version pin. The full recommended-pref set was not enabled, preserving other normal browser behavior. No acceptedVersion/acceptedDate was written, Continue clicked, renderer/init called, policy signal fabricated, productHTML changed or modalDOM removed. Existing personal/QA consent was not borrowed. This is consent-free official automation notification configuration, **not a default fresh-profile/TOU acceptance test**.

Startup AND final live state in both profiles: notificationTestPreftrue, acceptedVersion0/date"0", accepted/dateuserfalse; content sandbox6/userfalse; signatures.requiredtrue/userfalse. Native TLS verification remained enabled (MarionetteacceptInsecureCertsfalse); no verifier was lowered. Copied products remained unchanged,51entries each. Actual buildIDs159=20261102144711,156=20260921121718; exact official archives and source-generation distinctions retained from parent. The prior virgin-profile TOU-blocked attempts and screenshots remain unchanged, explicitly unqualified for paste.

## Native clipboard comparison

No Floorp trials/Linux22 recheck or Windows/Protonlogin was repeated. The same owned no-store local fixture and publishedRooster coreplugins were used. It is not the deployed Proton custompaste plugin/fullcomposer. Separate privateXvfb/Openbox/DBus/OSclipboards; nativeClipboardHelper/Transferable + actualXTestCtrlV/CtrlShiftV/CtrlA/CtrlC. All qualified paste events isTrustedtrue; passive logging/DOMsnapshot only, no fakepasteevent or producthandler replacement.

| Firefox profile | Ordinary conditions | Explicit opacityHTML controls | Normal exit |
|---|---:|---:|---|
|159.0.1 officialtestprofile|12|2|0|
|156.0.1 officialtestprofile|12|2|0|

All24ordinary conditions produce visible text and <=1anchor, nozeroopacity. FourexplicitrichHTML controls retain sourceopacity0. Text, anchorcount, opacityresult and realpasteMIME match the corresponding recorded Floorp156/159 results for all28conditions. FinalHTML is also byte-identical in28/28pairs. CtrlShiftV delivers text/plain; input/textarea native recopy are text/plain. SourceURL is the same publicexample fixture, not original sensitive mailcontent. Two settled snapshots are bounded timings, not exhaustive race coverage.

Combined qualified localfixture data are Floorp28 + Firefox28 =56conditions:48ordinary +8deliberateopacity-input controls, across4qualified browser trials. There are also2historical input-blocked Firefox attempts; those remain excluded and are not silently promoted. The6recorded processes all normalQuit/code0, but only4trials supply valid clipboard comparisons.

## Result and remaining conditions

The Firefox control-profile availability gap is fulfilled for this official automation configuration. No unexplained engine difference or localfixture source of opacity0 was found. The original Windows12.18.1 Protoncomposer behavior, nativeWindows clipboardformats, deployedProtonplugin/assets and originalprofile/clipboardorigin remain unresolved. Linux localfixture equality does not resolve the original opacity0/duplicate-link report or establish FirefoxTOU/NewTab causality in other tasks. Authorized Proton testcomposer access is still needed; no account was created/accessed or emaildraft/send performed. No productfix/PR, merge/release/Done. OriginalLinux22/C5 denominator and all earlier QA evidence preserved.
