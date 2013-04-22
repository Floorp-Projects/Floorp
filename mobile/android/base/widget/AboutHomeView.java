package org.mozilla.gecko.widget;

import org.mozilla.gecko.LightweightTheme;
import org.mozilla.gecko.LightweightThemeDrawable;
import org.mozilla.gecko.R;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.ScrollView;

public class AboutHomeView extends ScrollView implements LightweightTheme.OnChangeListener {
    private LightweightTheme mLightweightTheme;

    public AboutHomeView(Context context) {
        super(context);
    }

    public AboutHomeView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public AboutHomeView(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
    }

    public void setLightweightTheme(LightweightTheme theme) {
        mLightweightTheme = theme;
    }

    @Override
    public void onLightweightThemeChanged() {
        if (mLightweightTheme == null) {
            throw new IllegalStateException("setLightweightTheme() must be called before this view can listen to theme changes.");
        }

        LightweightThemeDrawable drawable = mLightweightTheme.getColorDrawable(this);
        if (drawable == null) {
            return;
        }

        drawable.setAlpha(255, 0);
        setBackgroundDrawable(drawable);
    }

    @Override
    public void onLightweightThemeReset() {
        setBackgroundColor(getResources().getColor(R.color.background_normal));
    }

    @Override
    protected void onLayout(boolean changed, int l, int t, int r, int b) {
        super.onLayout(changed, l, t, r, b);
        onLightweightThemeChanged();
    }
}
