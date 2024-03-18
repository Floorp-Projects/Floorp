# Homescreen Tips

## Homescreen Tips Explained 💡

Homescreen tips launched in Firefox Focus 7.0 in October 2018. They allow users to get more information about the product without having to research on their own or dig around in settings. Tips also enable users to _quickly_ access portions of the settings menu they may not know how to navigate otherwise. All tips have either a deep link to settings or a link to their corresponding SUMO article (if available).

<img width="300" src="https://user-images.githubusercontent.com/4400286/43972144-4dbf9508-9c88-11e8-8660-3e16030cdc45.png">

## How Tips are Displayed
Currently, tips are shown based on the following constraints:
1. We will never show a user a tip from a feature they have _already engaged with_
2. Tips are shown in a random order, and all have an equal chance of being shown
3. Only three tips are displayed per session before we revert back to showing "Browse. Erase. Repeat."
4. Tips are displayed on the home screen after launch, and are refreshed after each erase.

Some notes to keep in mind:
* clicking a SUMO link does not count as engaging with the feature
* the "disable tips" tip is shown less frequently and is _guaranteed_ to show on the third launch of the app.
* a "session" is defined as every time the app is loaded into memory (i.e. has not been force quit).

## List of Tips
Below is a list of tips available with their corresponding deep links:

* "Open every link in Firefox Focus. Set Firefox Focus as default browser" -> Settings deep link
* "Turn off tips on the Firefox Focus start screen" -> Settings deep link
* "Turn off tips on the start screen" -> Settings deep link
* "Trusted site? Allowlist disables Tracking Protection for sites you know and trust." -> Settings deep link
* "Site behaving unexpectedly? Try turning off Tracking Protection" -> None
* "Get one-tap access to sites you use most. Menu > Add to Home screen" -> [SUMO article](https://support.mozilla.org/en-US/kb/add-web-page-shortcuts-your-home-screen)
* "Autocomplete URLs for sites you use most. Long-press any URL in the address bar" -> [SUMO article](https://support.mozilla.org/en-US/kb/autocomplete-settings-firefox-focus-address-bar)
* "Open a link in a new tab. Long-press any link on a page" -> [SUMO article](https://support.mozilla.org/en-US/kb/open-new-tab-firefox-focus-android)
* "Rather see the full desktop site? Menu > Request desktop site" -> [SUMO article](https://support.mozilla.org/en-US/kb/switch-desktop-view-firefox-focus-android)

## The Future of Tips
If tips are received well, there are plans to update them in the future. A major change in consideration is hosting them server-side so they can be updated on the fly, rather than having to push an update to the app in order to change them. An android component for this feature is also being investigated.
