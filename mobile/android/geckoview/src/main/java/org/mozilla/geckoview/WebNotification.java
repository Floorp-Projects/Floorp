package org.mozilla.geckoview;

import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.UiThread;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.util.ThreadUtils;

public class WebNotification {

    /**
     * Title is shown at the top of the notification window.
     *
     * @see <a href="https://developer.mozilla.org/en-US/docs/Web/API/Notification/title">Web Notification - title</a>
     */
    public final @Nullable String title;

    /**
     * Tag is the ID of the notification.
     *
     * @see <a href="https://developer.mozilla.org/en-US/docs/Web/API/Notification/tag">Web Notification - tag</a>
     */
    public final @NonNull String tag;

    private final @Nullable String mCookie;

    /**
     * Text represents the body of the notification.
     *
     * @see <a href="https://developer.mozilla.org/en-US/docs/Web/API/Notification/body">Web Notification - text</a>
     */
    public final @Nullable String text;

    /**
     * ImageURL contains the URL of an icon to be displayed as part of the notification.
     *
     * @see <a href="https://developer.mozilla.org/en-US/docs/Web/API/Notification/icon">Web Notification - icon</a>
     */
    public final @Nullable String imageUrl;

    /**
     * TextDirection indicates the direction that the language of the text is displayed.
     * Possible values are:
     * auto: adopts the browser's language setting behaviour (the default.)
     * ltr: left to right.
     * rtl: right to left.
     *
     * @see <a href="https://developer.mozilla.org/en-US/docs/Web/API/Notification/dir">Web Notification - dir</a>
     */
    public final @Nullable String textDirection;

    /**
     * Lang indicates the notification's language, as specified using a DOMString
     * representing a BCP 47 language tag.
     *
     * @see <a href="https://developer.mozilla.org/en-US/docs/Web/API/DOMString">DOM String</a>
     * @see <a href="http://www.rfc-editor.org/rfc/bcp/bcp47.txt">BCP 47</a>
     * @see <a href="https://developer.mozilla.org/en-US/docs/Web/API/Notification/lang">Web Notification - lang</a>
     */
    public final @Nullable String lang;

    /**
     * RequireInteraction indicates whether a notification should remain active until the user
     * clicks or dismisses it, rather than closing automatically.
     *
     * @see <a href="https://developer.mozilla.org/en-US/docs/Web/API/Notification/requireInteraction">Web Notification - requireInteraction</a>
     */
    public final @NonNull boolean requireInteraction;

    @WrapForJNI
    /* package */ WebNotification(@Nullable final String title, @NonNull final String tag,
                        @Nullable final String cookie, @Nullable final String text,
                        @Nullable final String imageUrl, @Nullable final String textDirection,
                        @Nullable final String lang, @NonNull final boolean requireInteraction) {
        this.tag = tag;
        this.mCookie = cookie;
        this.title = title;
        this.text = text;
        this.imageUrl = imageUrl;
        this.textDirection = textDirection;
        this.lang = lang;
        this.requireInteraction = requireInteraction;
    }

    @UiThread
    public void click() {
        ThreadUtils.assertOnUiThread();
        GeckoAppShell.onNotificationClick(tag, mCookie);

    }

    @UiThread
    public void dismiss() {
        ThreadUtils.assertOnUiThread();
        GeckoAppShell.onNotificationClose(tag, mCookie);
    }
}
