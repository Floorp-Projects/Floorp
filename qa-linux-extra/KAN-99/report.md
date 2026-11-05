# KAN-99 / GitHub2514 signed extension integration

**Partial actual integration pass; original YouTube case remains untested because the Cloud proxy rejects CONNECT with403.** No demonstrated product defect warrants a new PR. The agent subtask initially saved local evidence; the root subsequently published approved QA evidence and updated Jira as recorded below. No merge, production release or Done transition.

## Context read

Read Jira KAN99 latest body and comments10146/10168, GitHub2514 original report/comment5345635551, and merged PR2699. Original12.15.2@152.0.2 Ubuntu24.04.4 report is specifically YouTube sponsored cosmetic ads in panel; video network ads still blocked and other panel sites worked. Comment separately reports uBO picker and TWP no visible response. PR2699 maps panel content to a real tab; its presence alone does not prove original symptoms fixed.

Current main inspection SHA29a3eb23145da90706d8a94441373278521cc737. Executed published12.21.1 archive is release target984509d2053be7bae9027133bda7259cd706b208, not a build of latest main. Actual process reports Gecko159.0.1/build20261103153232; do not infer effective version from unused outer INI. Debian13/Xvfb is not original Ubuntu environment.

## Actual signed extension trial

Downloaded AMO uBO1.75.4, verified downloaded SHA256 against official AMO API e2df503cce7cf5069130618c349695f62bdddb9eb8c189156d027269ae4ae267. Installed unmodified XPI through real AddonManager: signedState2, active true, appDisabled false. Content sandbox measured6; TLS validation retained. Packaged xpinstall.signatures.required default was false, then set true for these trials (strengthened). No sandbox disabling environment sourced.

Used actual uBO My filters UI (CodeMirror API to enter text, real Apply changes button) to configure:

```text
127.0.0.1###qa-sponsored
||127.0.0.1:18799/network-ad.svg$image
```

The local HTTP fixture contains sponsored target, visible control and image. Both normal tab and panel report sponsored display:none, control display:block, image naturalWidth0; server received tab/panel HTML but zero network-ad requests. uBO actual popup reported two blocked requests for each target context. No fixture DOM was hidden by test code. Panel has real gBrowser tab marked BMS-webpanel-tab=true and webIsolated remote type. Local custom filters are a controlled extension integration test, not YouTube's real ad layout/filter-list test.

For picker, created real uBO popup-fenix UI with actual ExtensionParent tabTracker IDs (normal2/panel3), then clicked its real gotoPick button. Both reported enabled/canPick and created full-size overlay iframes (normal612x827, panel600x789). panel-picker.png visually shows both genuine uBO picker overlays simultaneously. This establishes picker launch in both contexts; element selection, filter creation/save, and TWP translation were not completed and are not claimed.

Harness startup observation had tab-selection and extension-view frame-script mistakes, retained as result error JSON; corrected tab handle selection produced the stated UI results. No product defect inferred from harness errors. Owned browser was restarted once retaining its isolated profile; default signature preference raised again before resumed trials.

## Real external attempt and limits

Navigated both actual browsers to https://www.youtube.com/: both reached about:neterror proxyConnectFailure. Independent urllib through inherited approved proxy returned CONNECT403 for YouTube, while example.com returned200. This is network infrastructure evidence, not an ad-blocker product failure or a claim of non-reproduction. No authentication, CAPTCHA, alternate route or TLS bypass attempted.

Remaining: reachable live YouTube sponsored ads with representative lists/version; original Ubuntu condition; full picker select/save; separate TWP integration. Keep needs-external-check/needs-repro, narrow any broader extension-env label to needs-live-youtube-sponsored-ads and these specific conditions. No duplicate PR2699; no new fix without reproduced defect. Parent owns Jira integration; this task issued no new Jira comment ID.

QA publication: evidence commit2834b1aca2dcde0f383b79bed9e066e2cac39f30; Jira KAN-99 comment10364. Labels narrowed to the remaining live-site/filter conditions; issue remains open/未着手.
