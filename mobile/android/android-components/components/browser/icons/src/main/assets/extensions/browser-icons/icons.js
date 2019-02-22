/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

 /*
  * This web extension looks for known icon tags, collects URLs and available
  * meta data (e.g. sizes) and passes that to the app code.
  */

function collect_link_icons(icons, rel) {
    document.querySelectorAll('link[rel="' + rel + '"]').forEach(
        function(currentValue, currentIndex, listObj) {
            icons.push({
                'type': rel,
                'href': currentValue.href,
                'sizes': currentValue.sizes
            });
    })
}

function collect_meta_property_icons(icons, property) {
    document.querySelectorAll('meta[property="' + property + '"]').forEach(
        function(currentValue, currentIndex, listObj) {
            icons.push({
                'type': property,
                'href': currentValue.content
            })
        }
    )
}

function collect_meta_name_icons(icons, name) {
    document.querySelectorAll('meta[name="' + name + '"]').forEach(
        function(currentValue, currentIndex, listObj) {
            icons.push({
                'type': name,
                'href': currentValue.content
            })
        }
    )
}

let icons = [];

collect_link_icons(icons, 'icon');
collect_link_icons(icons, 'shortcut icon');
collect_link_icons(icons, 'fluid-icon')
collect_link_icons(icons, 'apple-touch-icon')
collect_link_icons(icons, 'image_src')
collect_link_icons(icons, 'apple-touch-icon image_src')
collect_link_icons(icons, 'apple-touch-icon-precomposed')

collect_meta_property_icons(icons, 'og:image')
collect_meta_property_icons(icons, 'og:image:url')
collect_meta_property_icons(icons, 'og:image:secure_url')

collect_meta_name_icons(icons, 'twitter:image')
collect_meta_name_icons(icons, 'msapplication-TileImage')

// TODO: For now we are just logging the icons we found here. We need the Messaging API
//       to actually be able to pass them to the Android world.
// https://github.com/mozilla-mobile/android-components/issues/2243
// https://bugzilla.mozilla.org/show_bug.cgi?id=1518843
console.log("browser-icons: (" + icons.length + ")", document.location.href, icons)
