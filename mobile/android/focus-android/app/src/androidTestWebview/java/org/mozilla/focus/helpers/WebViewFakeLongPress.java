package org.mozilla.focus.helpers;

import android.support.test.espresso.UiController;
import android.support.test.espresso.ViewAction;
import android.support.test.espresso.matcher.ViewMatchers;
import android.view.View;

import org.hamcrest.Matcher;
import org.mozilla.focus.R;
import org.mozilla.focus.web.IWebView;
import org.mozilla.focus.webview.SystemWebView;

public class WebViewFakeLongPress implements ViewAction {
    public static ViewAction injectHitTarget(IWebView.HitTarget hitTarget) {
        return new WebViewFakeLongPress(hitTarget);
    }

    private IWebView.HitTarget hitTarget;

    private WebViewFakeLongPress(IWebView.HitTarget hitTarget) {
        this.hitTarget = hitTarget;
    }

    @Override
    public Matcher<View> getConstraints() {
        return ViewMatchers.withId(R.id.webview);
    }

    @Override
    public String getDescription() {
        return String.format("Long pressing webview");
    }

    @Override
    public void perform(UiController uiController, View view) {
        final SystemWebView webView = (SystemWebView) view;

        webView.getCallback()
                .onLongPress(hitTarget);
    }
}
