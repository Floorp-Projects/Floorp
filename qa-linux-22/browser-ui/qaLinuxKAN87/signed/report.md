# KAN-87 normal signed image-theme control

Official Floorp12.21.1 / Gecko159.0.1 build20261103153232, Debian13.6 nested KWin, actual client windowProtocol=wayland. Dedicated qa-kde-zen-signed socket, port29701, isolated profile. Browser protections/signature preferences unchanged. No product code changes.

AMO Galaxy1.0 (file3680282) contains `theme.images.additional_backgrounds`. Normal AddonManager.getInstallForFile→install→enable succeeded: signedState=2, temporarilyInstalled=false, isActive=true. The raw fixture field named `signatureRequired` accidentally recorded AddonManager.checkCompatibility; it is not evidence of signature policy. Script field corrected to checkCompatibility. Actual signedState and normal install are recorded independently.

Synthetic W3C click opened real right bookmarks panel body; Alt+T then Tools > Toggle Zen Mode enabled Zen. Pointer near top-right simultaneously revealed top and right panel. Two screenshots use the same signed image theme with red/cyan content backdrops. No CSS UI override. Temporary addon installation was not used.

Measured changedPixels between red/cyan: tabbar frame0/17500, above-right-panel overlap0/2750, toolbar0/16800, panel body0/116000. Content backdrop250000/250000 confirms backdrop changed. Both screenshots visually inspected: Galaxy image paints in top frame and right panel body. This single-theme control does not prove all transparency configurations or original CachyOS/Niri/Hyprland/MangoWM, GPU and physical-input conditions.

Reproduce after placing official product/Python dependencies as existing qaLinuxKAN87 harness expects:

```sh
curl --fail --location https://addons.mozilla.org/firefox/downloads/file/3680282/galaxy0512145-1.0.xpi -o signed/theme/galaxy.xpi
bash signed/nested.sh 29701 paint
python3 signed/measure-pixels.py 29701
```

Scripts/evidence are scoped to signed/. Preserve screenshots, summary.json, paint/pixels JSON, logs/kwin-29701.log. Theme SHA256 is in summary.json. Do not commit runtime profiles, signed.xpi (unused color theme), search.json or product logs unless specifically needed. No Jira state/Sprint/Done/merge changes.
