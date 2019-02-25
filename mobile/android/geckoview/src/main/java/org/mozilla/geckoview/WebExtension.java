package org.mozilla.geckoview;

import android.support.annotation.NonNull;

import java.util.UUID;

/**
 * Represents a WebExtension that may be used by GeckoView.
 */
public class WebExtension {
    /**
     * <code>file:</code> or <code>resource:</code> URI that points to the
     * install location of this WebExtension. When the WebExtension is included
     * with the APK the file can be specified using the
     * <code>resource://android</code> alias. E.g.
     *
     * <pre><code>
     *      resource://android/assets/web_extensions/my_webextension/
     * </code></pre>
     *
     * Will point to folder
     * <code>/assets/web_extensions/my_webextension/</code> in the APK.
     */
    public final @NonNull String location;
    /**
     * Unique identifier for this WebExtension
     */
    public final @NonNull String id;

    /**
     * Builds a WebExtension instance that can be loaded in GeckoView using
     * {@link GeckoRuntime#registerWebExtension}
     *
     * @param location The WebExtension install location. It must be either a
     *                 <code>resource:</code> URI to a folder inside the APK or
     *                 a <code>file:</code> URL to a <code>.xpi</code> file.
     * @param id Unique identifier for this WebExtension. This identifier must
     *           either be a GUID or a string formatted like an email address.
     *           E.g. <pre><code>
     *              "extensionname@example.org"
     *              "{daf44bf7-a45e-4450-979c-91cf07434c3d}"
     *           </code></pre>
     *
     *           See also: <ul>
     *           <li><a href="https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/manifest.json/browser_specific_settings">
     *                  WebExtensions/manifest.json/browser_specific_settings
     *               </a>
     *           <li><a href="https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/WebExtensions_and_the_Add-on_ID#When_do_you_need_an_add-on_ID">
     *                  WebExtensions/WebExtensions_and_the_Add-on_ID
     *               </a>
     *           </ul>
     */
    public WebExtension(final @NonNull String location, final @NonNull String id) {
        this.location = location;
        this.id = id;
    }

    /**
     * Builds a WebExtension instance that can be loaded in GeckoView using
     * {@link GeckoRuntime#registerWebExtension}
     * The <code>id</code> for this web extension will be automatically
     * generated.
     *
     * @param location The WebExtension install location. It must be either a
     *                 <code>resource:</code> URI to a folder inside the APK or
     *                 a <code>file:</code> URL to a <code>.xpi</code> file.
     */
    public WebExtension(final @NonNull String location) {
        this.location = location;
        this.id = "{" + UUID.randomUUID().toString() + "}";
    }

    // TODO (Bug 1518843) add messaging support
}
