# Pings

## Events

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
        <td>ss_menu_opened</td>
        <td>event</td>
        <td>A user opened the search shortcut menu in the search view by pressing the shortcuts button</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/1202#issuecomment-476870449">link</a></td>
        <td></td>
        <td>2020-03-01</td>
    </tr>
    <tr>
        <td>ss_menu_closed</td>
        <td>event</td>
        <td> A user closed the search shortcut menu in the search view by pressing the shortcuts button</td>
        <td><a href="https://github.com/mozilla-mobile/fenix/pull/1202#issuecomment-476870449">link</a></td>
        <td></td>
        <td>2020-03-01</td>
    </tr>
    <tr>
        <td>ss_selected</td>
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

## Metrics
