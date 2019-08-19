# Pings

## Events

<pre>
<table style="width: 100%">
    <tr>
        <th>key</th>
        <th>type</th>
        <th>description</th>
        <th>data review</th>
        <th>extras</th>
        <th>expires</th>
    </tr>
    <tr>
        <td>app_opened</td>
        <td>event</td>
        <td>A user opened the app</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/1067#issuecomment-474598673">link</a></td>
        <td>
            <table>
                <tr><td>source</td><td>The method used to open Fenix. Possible values are: `app_icon`, `custom_tab` or `link`</td></tr>
            </table>
        </td>
        <td>2020-03-01</td>
    </tr>
    <tr>
        <td>search_bar_tapped</td>
        <td>event</td>
        <td>A user tapped the search bar</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/1067#issuecomment-474598673">link</a></td>
        <td>
            <table>
                <tr><td>source</td><td>The view the user was on when they initiated the search (For example: `Home` or `Browser`)</td></tr>
            </table>
        </td>
        <td>2020-03-01</td>
    </tr>
    <tr>
        <td>entered_url</td>
        <td>event</td>
        <td>A user entered a url</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/1067#issuecomment-474598673">link</a></td>
        <td>
            <table>
                <tr><td>autocomplete</td><td>A boolean that tells us whether the URL was autofilled by an Autocomplete suggestion</td></tr>
            </table>
        </td>
        <td>2020-03-01</td>
    </tr>
    <tr>
        <td>performed_search</td>
        <td>event</td>
        <td>A user performed a search</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/1677">link</a></td>
        <td>
            <table>
                <tr>
                    <td>description</td>
                    <td>
                        <p>A string that tells us how the user performed the search. Possible values are:</p>
                        <ul>                    
                            <li>default.action</li>
                            <li>default.suggestion</li>
                            <li>shortcut.action</li>
                            <li>shortcut.suggestion</li>
                        </ul>
                    </td>
                </tr>
            </table>
        </td>
        <td>2020-03-01</td>
    </tr>
    <tr>
        <td>browser_menu_action</td>
        <td>event</td>
        <td>A browser menu item was tapped</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/1214#issue-264756708">link</a></td>
        <td>
            <table>
                <tr><td>item</td><td>A string containing the name of the item the user tapped. These items include:
Settings, Library, Help, Desktop Site toggle on/off, Find in Page, New Tab,
Private Tab, Share, Report Site Issue, Back/Forward button, Reload Button</td></tr>
            </table>
        </td>
        <td>2020-03-01</td>
    </tr>
    <tr>
        <td>total_uri_count</td>
        <td>counter</td>
        <td>A counter of URIs visited by the user in the current session, including page reloads. This does not include background page requests and URIs from embedded pages or private browsing.</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/1785">link</a></td>
        <td></td>
        <td>2020-03-01</td>
    </tr>
    <tr>
        <td>preference_toggled</td>
        <td>event</td>
        <td>A user toggled a preference switch in settings</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/1896">link</a></td>
        <td>
            <table>
                <tr><td>preference_key</td><td>The preference key for the switch preference the user toggled. We currently track: leakcanary,
make_default_browser, show_search_suggestions, show_visited_sites_bookmarks, remote_debugging, telemetry,
tracking_protection</td>
                </tr>
                <tr><td>enabled</td><td>Whether or not the preference is <b>now</b> enabled</td></tr>
            </table>
        </td>
        <td>2020-03-01</td>
    </tr>
</table>
</pre>

## search_shortcuts
<pre>
<table style="width: 100%">
    <tr>
        <td>opened</td>
        <td>event</td>
        <td>A user opened the search shortcut menu in the search view by pressing the shortcuts button</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/1202#issuecomment-476870449">link</a></td>
        <td></td>
        <td>2020-03-01</td>
    </tr>
    <tr>
        <td>closed</td>
        <td>event</td>
        <td> A user closed the search shortcut menu in the search view by pressing the shortcuts button</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/1202#issuecomment-476870449">link</a></td>
        <td></td>
        <td>2020-03-01</td>
    </tr>
    <tr>
        <td>selected</td>
        <td>event</td>
        <td>A user selected a search shortcut engine to use</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/1202#issuecomment-476870449">link</a></td>
        <td>
            <table>
                <tr><td>engine</td><td>The name of the built-in search engine the user selected as a string</td></tr>
            </table>
        </td>
        <td>2020-03-01</td>
    </tr>
</table>
</pre>

## crash_reporter

<pre>
<table style="width: 100%">
    <tr>
        <th>key</th>
        <th>type</th>
        <th>description</th>
        <th>data deview</th>
        <th>extras</th>
        <th>expires</th>
    </tr>
    <tr>
        <td>opened</td>
        <td>event</td>
        <td>The crash reporter was displayed</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/1214#issue-264756708">link</a></td>
        <td></td>
        <td>2020-03-01</td>
    </tr>
    <tr>
        <td>closed</td>
        <td>event</td>
        <td>The crash reporter was closed</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/1067#issuecomment-474598673">link</a></td>
        <td>
            <table>
                <tr><td>crash_submitted</td><td>A boolean that tells us whether or not the user submitted a crash report</td></tr>
            </table>
        </td>
        <td>2020-03-01</td>
    </tr>
</table>
</pre>

## context_menu

<pre>
<table style="width: 100%">
    <tr>
        <th>key</th>
        <th>type</th>
        <th>description</th>
        <th>data deview</th>
        <th>extras</th>
        <th>expires</th>
    </tr>
    <tr>
        <td>item_tapped</td>
        <td>event</td>
        <td>A user tapped an item in the browsers context menu</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/1344#issuecomment-479285010">link</a></td>
        <td>
            <table>
                <tr><td>named</td><td> The name of the item that was tapped. Available items are
          ```
          open_in_new_tab, open_in_private_tab, open_image_in_new_tab,
          save_image, share_link, copy_link, copy_image_location
          ```</td></tr>
            </table>
        </td>
        <td>2020-03-01</td>
    </tr>
</table>
</pre>

## find_in_page

<pre>
<table style="width: 100%">
    <tr>
        <th>key</th>
        <th>type</th>
        <th>description</th>
        <th>data deview</th>
        <th>extras</th>
        <th>expires</th>
    </tr>
    <tr>
        <td>opened</td>
        <td>event</td>
        <td>A user opened the find in page UI</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/1344#issuecomment-479285010">link</a></td>
        <td>
        </td>
        <td>2020-03-01</td>
    </tr>
    <tr>
        <td>closed</td>
        <td>event</td>
        <td>A user closed the find in page UI</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/1344#issuecomment-479285010">link</a></td>
        <td></td>
        <td>2020-03-01</td>
    </tr>
    <tr>
        <td>next_result</td>
        <td>event</td>
        <td>A user clicked the "next result" button</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/1344#issuecomment-479285010">link</a></td>
        <td></td>
        <td>2020-03-01</td>
    </tr>
    <tr>
        <td>previous_result</td>
        <td>event</td>
        <td>A user clicked the "previous result" button</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/1344#issuecomment-479285010">link</a></td>
        <td></td>
        <td>2020-03-01</td>
    </tr>
    <tr>
        <td>searched_page</td>
        <td>event</td>
        <td>A user searched the page</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/1344#issuecomment-479285010">link</a></td>
        <td></td>
        <td>2020-03-01</td>
    </tr>
</table>
</pre>

## quick_action_sheet

<pre>
<table style="width: 100%">
    <tr>
        <th>key</th>
        <th>type</th>
        <th>description</th>
        <th>data deview</th>
        <th>extras</th>
        <th>expires</th>
    </tr>
    <tr>
        <td>opened</td>
        <td>event</td>
        <td>A user opened the quick action sheet UI</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/1362#issuecomment-479668466">link</a></td>
        <td></td>
        <td>2020-03-01</td>
    </tr>
    <tr>
        <td>closed</td>
        <td>event</td>
        <td>A user closed the quick action sheet UI</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/1362#issuecomment-479668466">link</a></td>
        <td></td>
        <td>2020-03-01</td>
    </tr>
    <tr>
        <td>share_tapped</td>
        <td>event</td>
        <td>A user tapped the share button</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/1362#issuecomment-479668466">link</a></td>
        <td></td>
        <td>2020-03-01</td>
    </tr>
    <tr>
        <td>bookmark_tapped</td>
        <td>event</td>
        <td>A user tapped the bookmark button</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/1362#issuecomment-479668466">link</a></td>
        <td></td>
        <td>2020-03-01</td>
    </tr>
    <tr>
        <td>download_tapped</td>
        <td>event</td>
        <td>A user tapped the download button</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/1362#issuecomment-479668466">link</a></td>
        <td></td>
        <td>2020-03-01</td>
    </tr>
    <tr>
        <td>open_app_tapped</td>
        <td>event</td>
        <td>A user tapped the open in app button</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/4629">link</a></td>
        <td></td>
        <td>2020-03-01</td>
    </tr>
</table>
</pre>

## bookmarks_management

<pre>
<table style="width: 100%">
    <tr>
        <th>key</th>
        <th>type</th>
        <th>description</th>
        <th>data deview</th>
        <th>extras</th>
        <th>expires</th>
    </tr>
    <tr>
        <td>open_in_new_tab</td>
        <td>event</td>
        <td>A user opened a bookmark in a new tab.</td>
        <td> <a href="https://github.com/mozilla-mobile/fenix/pull/1708">link</a></td>
        <td></td>
        <td>2020-03-01</td>
    </tr>

    <tr>
        <td>open_in_new_tabs</td>
        <td>event</td>
        <td>A user opened multiple bookmarks at once in new tabs.</td>
        <td> <a href="https://github.com/mozilla-mobile/fenix/pull/1708">link</a></td>
        <td></td>
        <td>2020-03-01</td>
    </tr>

    <tr>
        <td>open_in_private_tab</td>
        <td>event</td>
        <td>A user opened a bookmark in a new private tab.</td>
        <td> <a href="https://github.com/mozilla-mobile/fenix/pull/1708">link</a></td>
        <td></td>
        <td>2020-03-01</td>
    </tr>

    <tr>
        <td>open_in_private_tabs</td>
        <td>event</td>
        <td>A user opened multiple bookmarks at once in new private tabs.</td>
        <td> <a href="https://github.com/mozilla-mobile/fenix/pull/1708">link</a></td>
        <td></td>
        <td>2020-03-01</td>
    </tr>

    <tr>
        <td>edited</td>
        <td>event</td>
        <td>A user edited the title and/or URL of an existing bookmark.</td>
        <td> <a href="https://github.com/mozilla-mobile/fenix/pull/1708">link</a></td>
        <td></td>
        <td>2020-03-01</td>
    </tr>

    <tr>
        <td>moved</td>
        <td>event</td>
        <td>A user moved an existing bookmark or folder to another folder.</td>
        <td> <a href="https://github.com/mozilla-mobile/fenix/pull/1708">link</a></td>
        <td></td>
        <td>2020-03-01</td>
    </tr>

    <tr>
        <td>removed</td>
        <td>event</td>
        <td>A user removed a bookmark item.</td>
        <td> <a href="https://github.com/mozilla-mobile/fenix/pull/1708">link</a></td>
        <td></td>
        <td>2020-03-01</td>
    </tr>

    <tr>
        <td>multi_removed</td>
        <td>event</td>
        <td>A user removed multiple bookmarks at once.</td>
        <td> <a href="https://github.com/mozilla-mobile/fenix/pull/1708">link</a></td>
        <td></td>
        <td>2020-03-01</td>
    </tr>

    <tr>
        <td>shared</td>
        <td>event</td>
        <td>A user shared a bookmark.</td>
        <td> <a href="https://github.com/mozilla-mobile/fenix/pull/1708">link</a></td>
        <td></td>
        <td>2020-03-01</td>
    </tr>

    <tr>
        <td>copied</td>
        <td>event</td>
        <td>A user copied a bookmark.</td>
        <td> <a href="https://github.com/mozilla-mobile/fenix/pull/1708">link</a></td>
        <td></td>
        <td>2020-03-01</td>
    </tr>

    <tr>
        <td>folder_add</td>
        <td>event</td>
        <td>A user added a new bookmark folder.</td>
        <td> <a href="https://github.com/mozilla-mobile/fenix/pull/1708">link</a></td>
        <td></td>
        <td>2020-03-01</td>
    </tr>
    
    <tr>
        <td>folder_remove</td>
        <td>event</td>
        <td>A user removed a bookmark folder.</td>
        <td> <a href="https://github.com/mozilla-mobile/fenix/pull/3724">link</a></td>
        <td></td>
        <td>2020-03-01</td>
    </tr>
</table>
</pre>

## custom_tab

<pre>
<table style="width: 100%">
    <tr>
        <th>key</th>
        <th>type</th>
        <th>description</th>
        <th>data deview</th>
        <th>extras</th>
        <th>expires</th>
    </tr>
    <tr>
        <td>closed</td>
        <td>event</td>
        <td>A user closed the custom tab</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/1697">link</a></td>
        <td></td>
        <td>2020-03-01</td>
    </tr>
    <tr>
        <td>action_button</td>
        <td>event</td>
        <td>A user pressed the action button provided by the launching app</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/1697">link</a></td>
        <td></td>
        <td>2020-03-01</td>
    </tr>
    <tr>
        <td>menu</td>
        <td>event</td>
        <td>A user opened the custom tabs menu</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/1697">link</a></td>
        <td></td>
        <td>2020-03-01</td>
    </tr>
</table>
</pre>

## qr_scanner

<pre>
<table style="width: 100%">
    <tr>
        <th>key</th>
        <th>type</th>
        <th>description</th>
        <th>data deview</th>
        <th>extras</th>
        <th>expires</th>
    </tr>
    <tr>
        <td>opened</td>
        <td>event</td>
        <td>A user opened the QR scanner</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/2524#issuecomment-492739967">link</a></td>
        <td></td>
        <td>2020-03-01</td>
    </tr>
    <tr>
        <td>prompt_displayed</td>
        <td>event</td>
        <td>A user scanned a QR code, causing a confirmation prompt to display asking if they want to navigate to the page</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/2524#issuecomment-492739967">link</a></td>
        <td></td>
        <td>2020-03-01</td>
    </tr>
    <tr>
        <td>navigation_allowed</td>
        <td>event</td>
        <td>A user tapped "allow" on the prompt, directing the user to the website scanned</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/2524#issuecomment-492739967">link</a></td>
        <td></td>
        <td>2020-03-01</td>
    </tr>
    <tr>
        <td>navigation_denied</td>
        <td>event</td>
        <td>A user tapped "deny" on the prompt, putting the user back to the scanning view</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/2524#issuecomment-492739967">link</a></td>
        <td></td>
        <td>2020-03-01</td>
    </tr>
</table>
</pre>

## library

<pre>
<table style="width: 100%">
    <tr>
        <th>key</th>
        <th>type</th>
        <th>description</th>
        <th>data deview</th>
        <th>extras</th>
        <th>expires</th>
    </tr>
    <tr>
        <td>opened</td>
        <td>event</td>
        <td>A user opened the library</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/2538#issuecomment-492830242">link</a></td>
        <td></td>
        <td>2020-03-01</td>
    </tr>
    <tr>
        <td>closed</td>
        <td>event</td>
        <td>A user closed the library</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/2538#issuecomment-492830242">link</a></td>
        <td></td>
        <td>2020-03-01</td>
    </tr>
    <tr>
        <td>selected_item</td>
        <td>event</td>
        <td>A user selected a library item</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/2538#issuecomment-492830242">link</a></td>
        <td>
            <table>
                <tr><td>item</td><td>The library item the user selected</td></tr>
            </table>
        </td>
        <td>2020-03-01</td>
    </tr>
</table>
</pre>

## error_page

<pre>
<table style="width: 100%">
    <tr>
        <th>key</th>
        <th>type</th>
        <th>description</th>
        <th>data deview</th>
        <th>extras</th>
        <th>expires</th>
    </tr>
    <tr>
        <td>visited_error</td>
        <td>event</td>
        <td>A user encountered an error page</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/2491#issuecomment-492414486">link</a></td>
        <td>
            <table>
                <tr><td>error_type</td><td>The error type of the error page encountered</td></tr>
            </table>
        </td>
        <td>2020-03-01</td>
    </tr>
</table>
</pre>

## sync_auth

<pre>
<table style="width: 100%">
    <tr>
        <th>key</th>
        <th>type</th>
        <th>description</th>
        <th>data deview</th>
        <th>extras</th>
        <th>expires</th>
    </tr>
    <tr>
        <td>opened</td>
        <td>event</td>
        <td>A user opened the sync authentication page</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/2745#issuecomment-494918532">link</a></td>
        <td></td>
        <td>2020-03-01</td>
    </tr>
    <tr>
        <td>closed</td>
        <td>event</td>
        <td>A user closed the sync page</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/2745#issuecomment-494918532">link</a></td>
        <td></td>
        <td>2020-03-01</td>
    </tr>
    <tr>
        <td>sign_in</td>
        <td>event</td>
        <td>A user pressed the sign in button on the sync authentication page and was successfully signed in to FxA</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/2745#issuecomment-494918532">link</a></td>
        <td></td>
        <td>2020-03-01</td>
    </tr>
    <tr>
        <td>sign_out</td>
        <td>event</td>
        <td>A user pressed the sign out button on the sync account page and was successfully signed out of FxA</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/2745#issuecomment-494918532">link</a></td>
        <td></td>
        <td>2020-03-01</td>
    </tr>
    <tr>
        <td>sign_up</td>
        <td>event</td>
        <td> A user signed up for a new FxA account</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/4816#issuecomment-522643217">link</a></td>
        <td></td>
        <td>2020-03-01</td>
    </tr>
    <tr>
        <td>scan_pairing</td>
        <td>event</td>
        <td>A user pressed the scan pairing button on the sync authentication page</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/2745#issuecomment-494918532">link</a></td>
        <td></td>
        <td>2020-03-01</td>
    </tr>
    <tr>
        <td>create_account</td>
        <td>event</td>
        <td>A user pressed the create account button on the sync authentication page</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/2745#issuecomment-494918532">link</a></td>
        <td></td>
        <td>2020-03-01</td>
    </tr>
</table>
</pre>

## sync_account

<pre>
<table style="width: 100%">
    <tr>
        <th>key</th>
        <th>type</th>
        <th>description</th>
        <th>data deview</th>
        <th>extras</th>
        <th>expires</th>
    </tr>
    <tr>
        <td>opened</td>
        <td>event</td>
        <td>A user opened the sync account page</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/2745#issuecomment-494918532">link</a></td>
        <td></td>
        <td>2020-03-01</td>
    </tr>
    <tr>
        <td>closed</td>
        <td>event</td>
        <td>A user closed the sync account page</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/2745#issuecomment-494918532">link</a></td>
        <td></td>
        <td>2020-03-01</td>
    </tr>
    <tr>
        <td>sync_now</td>
        <td>event</td>
        <td>A user pressed the sync now button on the sync account page</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/2745#issuecomment-494918532">link</a></td>
        <td></td>
        <td>2020-03-01</td>
    </tr>
</table>
</pre>

## history

<pre>
<table style="width: 100%">
    <tr>
        <th>key</th>
        <th>type</th>
        <th>description</th>
        <th>data deview</th>
        <th>extras</th>
        <th>expires</th>
    </tr>
    <tr>
        <td>opened</td>
        <td>event</td>
        <td>A user opened the history screen</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/3940">link</a></td>
        <td></td>
        <td>2020-03-01</td>
    </tr>
    <tr>
        <td>removed</td>
        <td>event</td>
        <td>A user removed a history item</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/3940">link</a></td>
        <td></td>
        <td>2020-03-01</td>
    </tr>
    <tr>
        <td>removed_all</td>
        <td>event</td>
        <td>A user removed all history items</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/3940">link</a></td>
        <td></td>
        <td>2020-03-01</td>
    </tr>
    <tr>
        <td>shared</td>
        <td>event</td>
        <td>A user shared a history item</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/3940">link</a></td>
        <td></td>
        <td>2020-03-01</td>
    </tr>
    <tr>
        <td>opened_item</td>
        <td>event</td>
        <td>A user opened a history item</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/3940">link</a></td>
        <td></td>
        <td>2020-03-01</td>
    </tr>
</table>
</pre>


## reader_mode

<pre>
<table style="width: 100%">
    <tr>
        <th>key</th>
        <th>type</th>
        <th>description</th>
        <th>data deview</th>
        <th>extras</th>
        <th>expires</th>
    </tr>
    <tr>
        <td>available</td>
        <td>event</td>
        <td>Reader mode is available for the current page</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/3941">link</a></td>
        <td></td>
        <td>2020-03-01</td>
    </tr>
    <tr>
        <td>opened</td>
        <td>event</td>
        <td>A user opened reader mode</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/3941">link</a></td>
        <td></td>
        <td>2020-03-01</td>
    </tr>
    <tr>
        <td>opened</td>
        <td>event</td>
        <td>A user closed reader mode</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/4328">link</a></td>
        <td></td>
        <td>2020-03-01</td>
    </tr>
    <tr>
        <td>appearance</td>
        <td>event</td>
        <td>A user tapped the appearance button</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/3941">link</a></td>
        <td></td>
        <td>2020-03-01</td>
    </tr>
</table>
</pre>

## collections  

<pre>  
<table style="width: 100%">  
    <tr>  
        <th>key</th>  
        <th>type</th>  
        <th>description</th>  
        <th>data deview</th>  
        <th>extras</th>  
        <th>expires</th>  
    </tr>  
    <tr>  
        <td>renamed</td>  
        <td>event</td>  
        <td>A user renamed a collection</td>  
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/3935">link</a></td>  
        <td></td>  
        <td>2020-03-01</td>  
    </tr>  
    <tr>  
        <td>tab_restored</td>  
        <td>event</td>  
        <td>A user restored a tab from collection tab list</td>  
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/3935">link</a></td>  
        <td></td>  
        <td>2020-03-01</td>  
    </tr>  
    <tr>  
        <td>all_tabs_restored</td>  
        <td>event</td>  
        <td>A user tapped "open tabs" from collection menu</td>  
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/3935">link</a></td>  
        <td></td>  
        <td>2020-03-01</td>  
    </tr>  
    <tr>  
        <td>tab_removed</td>  
        <td>event</td>  
        <td>A user tapped remove tab from collection tab list</td>  
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/3935">link</a></td>  
        <td></td>  
        <td>2020-03-01</td>  
    </tr>  
    <tr>  
        <td>shared</td>  
        <td>event</td>  
        <td>A user tapped share collection</td>  
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/3935">link</a></td>  
        <td></td>  
        <td>2020-03-01</td>  
    </tr>  
    <tr>  
        <td>removed</td>  
        <td>event</td>  
        <td>A user tapped delete collection from collection menu</td>  
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/3935">link</a></td>  
        <td></td>  
        <td>2020-03-01</td>  
    </tr>  
    <tr>  
        <td>saved</td>  
        <td>event</td>  
        <td>A user saved a list of tabs to a new collection</td>  
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/3935">link</a></td>  
        <td>tabs_open: The number of tabs open in the current session
tabs_selected: The number of tabs added to the collection
        </td> 
        <td>2020-03-01</td>  
    </tr>  
    <tr>  
        <td>tabs_added</td>  
        <td>event</td>  
        <td>A user saved a list of tabs to an existing collection</td>  
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/3935">link</a></td>  
        <td>tabs_open: The number of tabs open in the current session
tabs_selected: The number of tabs added to the collection
        </td> 
        <td>2020-03-01</td>  
    </tr>  
    <tr>  
        <td>tab_select_opened</td>  
        <td>event</td>  
        <td>A user opened the select tabs screen (the first step of the collection creation flow)</td>  
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/3935">link</a></td>  
        <td></td>  
        <td>2020-03-01</td>  
    </tr>  
    <tr>  
        <td>add_tab_button</td>  
        <td>event</td>  
        <td>A user tapped the "add tab" button in the three dot menu of collections</td>  
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/4358">link</a></td>  
        <td></td>  
        <td>2020-03-01</td>  
    </tr>  
    <tr>  
        <td>long_press</td>  
        <td>event</td>  
        <td>A user long pressed on a tab, triggering the collection creation screen</td>  
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/4358">link</a></td>  
        <td></td>  
        <td>2020-03-01</td>  
    </tr> 
    <tr>  
        <td>save_button</td>  
        <td>event</td>  
        <td>A user pressed the "save to collection" button on either the home or browser screen, triggering the
                  collection creation screen to open (tab_select_opened)</td>  
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/3935">link</a></td>  
        <td>from_screen: A string representing the screen from which the user pressed the save button. Either `browser` or `home`
        </td> 
        <td>2020-03-01</td>  
    </tr>   
    <tr>  
        <td>rename_button</td>  
        <td>event</td>  
        <td>A user pressed the "rename collection" button in the three dot menu</td>  
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/4539">link</a></td>  
        <td></td>  
        <td>2020-03-01</td>  
    </tr> 
</table>  
</pre> 


## Metrics

Items that are added to the metrics ping

<pre>
<table style="width: 100%">
    <tr>
        <th>key</th>
        <th>type</th>
        <th>description</th>
        <th>data deview</th>
        <th>extras</th>
        <th>expires</th>
    </tr>
    <tr>
        <td>default_browser</td>
        <td>boolean</td>
        <td>Is Fenix the default browser?</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/1067#issuecomment-474598673">link</a></td>
        <td></td>
        <td>2020-03-01</td>
    </tr>
    <tr>
        <td>search_count</td>
        <td>labeled_counter</td>
        <td> The labels for this counter are `[search-engine-name].[source]`.

      If the search engine is bundled with Fenix `search-engine-name` will be the name of the search engine. If it's a
      custom search engine (defined: https://github.com/mozilla-mobile/fenix/issues/1607) the value will be `custom`.

      `source` will either be `action` or `suggestion`</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/1677">link</a></td>
        <td></td>
        <td>2019-09-01</td>
    </tr>
    <tr>
        <td>mozilla_products</td>
        <td>string_list</td>
        <td>A list of all the Mozilla products installed on device. We currently scan for: Firefox, Firefox Beta,
       Firefox Aurora, Firefox Nightly, Firefox Fdroid, Firefox Lite, Reference Browser, Reference Browser Debug,
       Fenix, Focus, and Lockwise.</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/1953/">link</a></td>
        <td></td>
        <td>2019-09-01</td>
    </tr>
    <tr>
        <td>default_moz_browser</td>
        <td>string</td>
        <td>The name of the default browser on device if and only if it's a Mozilla owned product, otherwise empty string</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/1953/">link</a></td>
        <td></td>
        <td>2019-09-01</td>
    </tr>
    <tr>
        <td>search.default_engine.code</td>
        <td>string</td>
        <td>If the search engine is pre-loaded with Fenix this value
      will be the search engine identifier. If it's a custom search engine
      (defined: https://github.com/mozilla-mobile/fenix/issues/1607) the
      value will be "custom"</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/issues/1607">link</a></td>
        <td></td>
        <td>2019-09-01</td>
    </tr>
    <tr>
        <td>search.default_engine.name</td>
        <td>string</td>
        <td>If the search engine is pre-loaded with Fenix this value
      will be the search engine name. If it's a custom search engine
      (defined: https://github.com/mozilla-mobile/fenix/issues/1607) the
      value will be "custom"</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/1953/">link</a></td>
        <td></td>
        <td>2019-09-01</td>
    </tr>
    <tr>
        <td>search.default_engine.submission_url</td>
        <td>string</td>
        <td>If the search engine is pre-loaded with Fenix this value
      will be he base URL we use to build the search query for the search engine.
      For example: https://mysearchengine.com/?query=%s. If it's a custom search engine
      (defined: https://github.com/mozilla-mobile/fenix/issues/1607) the
      value will be "custom"</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/1953/">link</a></td>
        <td></td>
        <td>2019-09-01</td>
    </tr>
</table>
</pre>


## Activation

Data that is sent in the activation ping

<pre>
<table style="width: 100%">
    <tr>
        <th>key</th>
        <th>type</th>
        <th>description</th>
        <th>data deview</th>
        <th>extras</th>
        <th>expires</th>
    </tr>
    <tr>
        <td>identifier</td>
        <td>string</td>
        <td>An hashed and salted version of the Google Advertising ID from the device.
      This will never be sent in a ping that also contains the client_id.</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/1707#issuecomment-486972209">link</a></td>
        <td></td>
        <td>2019-10-01</td>
    </tr>
    <tr>
        <td>activation_id</td>
        <td>uuid</td>
        <td>An alternate identifier, not correlated with the client_id, generated once
      and only sent with the activation ping.</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/1707#issuecomment-486972209">link</a></td>
        <td></td>
        <td>2019-10-01</td>
    </tr>
    <tr>
        <td>active_experiment</td>
        <td>string</td>
        <td>The branch name of the active experiment, if the client is enrolled in the `fenix-test-2019-08-05` experiment.</td>
        <td><a href="https://bugzilla.mozilla.org/show_bug.cgi?id=1543986#c4">link</a></td>
        <td></td>
        <td>2019-11-01</td>
    </tr>
</table>
</pre>

## search_widget

<pre>
<table style="width: 100%">
    <tr>
        <td>new_tab_button</td>
        <td>event</td>
        <td>A user pressed anywhere from the Firefox logo until the start of the microphone icon, opening a
                  new tab search screen.</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/4714">link</a></td>
        <td></td>
        <td>2020-03-01</td>
    </tr>
    <tr>
        <td>voice_button</td>
        <td>event</td>
        <td>A user pressed the microphone icon, opening a new voice search screen.</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/4714">link</a></td>
        <td></td>
        <td>2020-03-01</td>
    </tr>
</table>
</pre>