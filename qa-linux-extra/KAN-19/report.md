# KAN19 / GH2792 — bounded Linux native clipboard investigation

2026-11-06 JST. **No unseeded opacity0 or duplicate anchor was reproduced in24ordinary local-fixture conditions across two official Floorp versions.** Four intentionally supplied opacity0 HTML controls remain invisible, as expected from that input. These28real native-paste observations are not authenticated Proton Mail or originalWindows acceptance. Both Firefox controls were blocked by actual TOU modal and are not qualified paste comparisons. No product cause/fix or DraftPR is established.

## Prior records and source scope

Read JiraKAN19 fullbody and sole priorcomment10076, originalGH2792 fullreport (no comments), relatedPRsearches and bounded localQA inventory before trials. Prior Jira was static/authentication-blocked, no earlier browser trial identified in these records. Direct2792PRsearch returnednone; this is not exhaustive absence proof. PR2960 addresses webpanel paste/permission context, not ordinary mailwebtab; otherclipboard/OSAPI/WorkspacePRs concern distinct paths. No duplicate productpatch.

Original: Windows Floorp12.18.1 vsFirefox156.0.1; transparent span inlineopacity0/white-spacepre-wrap/user-selectauto, nested/duplicatedlinks; Notepad andCtrlShiftV don't fix, Subjectrecopy does. Browser freshprofile/extensionsdisabled are reporter claims, not independently executed originalWindows conditions here.

Independent read-only worktree qa/kan19-proton-paste at latestmain29a3eb23145da90706d8a94441373278521cc737. Floorp-side audit found no matching normalcopy-staging span/globalProtonpastehook in bounded frontend/patch scan. Defaultshortcuts are F2/CtrlAltZ, noV; actual fresh-profile configuration was recorded and agrees. Globalcontent theme sheets do exist, so no claim allFloorpCSS is chromeonly. Reviewedopacity rules are internalvideo/offscreen contexts, not generic editable spans. No-match source search is not proof of impossibility or a nativeWindows audit.

## Public Proton and upstream evidence

ProtonMail/WebClients immutable publiccommit43ba5f750c48e87e4ea322d7ecd8529f4a3135ec uses RoosterJS ContentEdit/Paste/CutPasteListChain/HyperLink plus EditorCustomPastePlugin. Currentpublic plugin's plain branch requires clipboard types exactlyone text/plain, clears fragment text and appends span + transformLinkify output. transformLinkify generates target_blank/rel noreferrer nofollow noopener, consistent with the report's second anchor pattern but not proof of deployed version or source of opacity. A different selected-text URL branch emits relnoopenernoreferrer and returns early; the two string matches do not prove simultaneous execution or cause of duplicated links. Neither inspected plugin nor transformLinkify adds opacity0. Publiccopy helper uses ClipboardAPI/plaintextarea or offscreen container, not the exact reported span combination. PublishedRooster source scan found no matching opacity0 paste-span constructor. UpstreamFirefoxpaste/formatting issue searches returned distinct historical reports, not an established GH2792 fix; searchresults retained.

PublicProton declares roosterjs8.47.0; its lockfile resolves editor api/core/dom/types8.62.0 and plugins8.62.1. All7fixture Rooster package versions/integrities match that publiclock. This is current public source, **not the authenticated/deployed Proton asset or original-report-time version**. Our fixture runs the real published Rooster core plugins and passive BeforePaste observer, plus a bare contenteditable. It deliberately does not pretend to execute Proton's customplugin/fullapp; no mocked production paste handler is called a reproduction.

## Actual products and native input

| Linux product | Source/retrieval | Actual nativeBuildID |
|---|---|---|
| Floorp12.21.1 / Gecko159.0.1 | existing officialarchive source984509d2053be7bae9027133bda7259cd706b208 | 20261103153232 |
| Floorp12.18.1 / Gecko156.0.1 | officialrelease/ac42feadf1140a3c5f015e46db3c64104e97f60a, archiveSHA2562891d016b03cee91e030231d709d572b63980780612e4ccaae8e7dca76d86edd | 20260922152932 |
| Firefox159.0.1 | existing officialarchive | 20261102144711 |
| Firefox156.0.1 | Mozillaarchive/SHA256SUMS, archiveSHA2567405c0487fa3e517c1e20e5aec0ed14014185dad6357d99a59b53f65acb10ed6 | 20260921121718 |

Same Gecko version is not identical native build/distribution. All4fresh task profiles use private Xvfb/Openbox/DBus with separate X11clipboards; regular proxy/CA, remote.prefs.recommendedfalse and QA-profile-only authorized app.update.disabledForTestingtrue hold versions. Stock sandbox6 inall; Floorp stocksignatureRequiredfalse/Firefoxtrue, allsecurity useroverridesfalse. No extensions/accounts/personaldata added, no Protonlogin/account creation/mailaccess or third-party load test. Native startup/finalstates recordactualversion. Full46/51entry hash snapshots remain unchanged; snapshotcollection was after the firstFloorp suite/during subsequent suites, so it is not claimed a pre-firststartup snapshot. Currentcopies additionally derive from untouched verified officialprefixes; originalversionarchives were independently hashchecked before extraction.

Fixture is owned localhost HTTP no-store page, public URL https://example.org/kan19/status/123/video/1. Native nsIClipboardHelper/plain or nsITransferable/text+HTML populates that display's realOSclipboard; actual XTestCtrlV/CtrlShiftV pastes into focusededitor. Textarea and Subject-shapedinput recopy use actualCtrlA/CtrlC. No syntheticClipboardEvent/KeyboardEvent or injectedobservernotification. Eventlogging is passive; editor reset is fixtureAPI state setup, not replacement of productcode. Every qualified paste event has isTrustedtrue. DOMcapture at~350ms/~1s includesHTML/MIME, beforeinput/input, passiveeditorBeforePaste andmutationhistory; no persistent arbitrarytiming/longsoak guarantee.

| Input path (each bareCE andRooster; each Floorpversion) | Conditions | Observations |
|---|---:|---|
| plain nativewriter, CtrlV | 4 | text/plain; visible; noopacity0; bare0anchors/Rooster1 |
| plain nativewriter, CtrlShiftV | 4 | text/plain; visible; noopacity0 |
| visible rich HTML, CtrlV | 4 | text/html+text/plain;1visibleanchor |
| visible rich HTML, CtrlShiftV | 4 | pasteDataTransfer text/plain only; visible |
| localtextarea copy thenpaste | 4 | text/plain; visible; not WindowsNotepad test |
| Subject-shapedinput copy thenpaste | 4 | text/plain; visible; no distinctworkaround needed |
| explicit opacity0-richHTML control | 4 | text/html+text/plain; importedopacity0persists |

Ordinary24conditions have <=1anchor and nozeroopacity at bothsettled snapshots. Fourexplicitcontrols intentionally set the report-like inlineopacity in the OSHTMLpayload and prove logger sensitivity/inputstyle preservation; they are not a spontaneous bug reproduction or a productfix. No post-paste opacity/CSS correction was made. Subject/textarea equalMIME onLinux does not refute the reportedWindowsProton workaround.

## Firefox comparison is unqualified

Actual156and159Firefox showed a modal “Welcome to Firefox — By continuing, you agree...” withContinue, screenshot retained. Webpagevisible butdocument.hasFocusfalse; no rootkeydown/paste/beforeinput/input occurred. One attempted firstcondition perFirefox was rejected by the harness's trustedpaste-event assertion; it is not a browserpaste failure or negativeProton result. Passiveprefs: mozilla-official, bypassfalse/no useroverride, acceptedVersion0/date0/minimum4/no acceptanceoverride. NoContinue, consent spoof, policychange, DOMremoval or backgroundforcedpaste to evade thatmodal. This is direct nativeinput-path blockage inthese controls; it does not retroactively prove KAN37's separateNewTab readiness cause. Need an authorized cleanFirefoxQA profile with genuine priorconsent for a usablecontrol, not a forcedrenderer/acceptancepref.

All4productprocesses exited via normalQuit/code0. Only the2Floorp suites are qualified data trials. BothFirefox TOU-limited trials remain explicitly unqualified. Sourcequeries initially tried nonexisting yarn.lock (repo uses pnpm-lock); no productresult derived from that404. Dependencyinstall disabledscripts; publishedlock/integrity verified afterward.

## Classification / next discriminator

Bounded Linux bareCE/Rooster nativepaste works forordinary controlledpayloads inreported/currentFloorp; actualWindows12.18.1 authenticatedProtoncomposer remains untested. Exactopacity0 generator and duplicateanchor creator remain unidentified; no Floorp-specific or upstreamcause assigned. OriginalNotepad/CtrlShiftV failure means an HTML-only explanation must not be assumed. Next evidence needs authorizedProton testcomposer onoriginalWindows, nativeCFHTML/plainflavors beforepaste, trustedpaste/BeforePaste/DOMmutation timeline, source/deployededitorassetversion, actualshortcut/config/theme/extensionprovenance and sameURL.Subjectcopy comparison. No personal mailbox/profile is requested/accessed by this work.

Retain needs-repro/env-blocked/account condition foractualProton access, narrowWindows/clipboard/controlprofile labels and add Linuxfixture-tested only. No merge/release/Done or speculativePR. This is additionalKAN19, outside originalLinux22/C5; KAN37parentclarification alreadypushed74c07f3a, historical KAN37 failure/fulfillment and KAN50/77/86 evidence remain unchanged. KAN102producthold maintained.
