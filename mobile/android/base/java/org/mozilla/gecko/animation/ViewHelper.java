/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.animation;

import android.view.View;
import android.view.ViewGroup;

public final class ViewHelper {
    private ViewHelper() {
    }

    public static float getTranslationX(View view) {
        if (view != null) {
            return view.getTranslationX();
        }

        return 0;
    }

    public static void setTranslationX(View view, float translationX) {
        if (view != null) {
            view.setTranslationX(translationX);
        }
    }

    public static float getTranslationY(View view) {
        if (view != null) {
            return view.getTranslationY();
        }

        return 0;
    }

    public static void setTranslationY(View view, float translationY) {
        if (view != null) {
            view.setTranslationY(translationY);
        }
    }

    public static float getAlpha(View view) {
        if (view != null) {
            return view.getAlpha();
        }

        return 1;
    }

    public static void setAlpha(View view, float alpha) {
        if (view != null) {
            view.setAlpha(alpha);
        }
    }

    public static int getWidth(View view) {
        if (view != null) {
            return view.getWidth();
        }

        return 0;
    }

    public static void setWidth(View view, int width) {
        if (view != null) {
            ViewGroup.LayoutParams lp = view.getLayoutParams();
            lp.width = width;
            view.setLayoutParams(lp);
        }
    }

    public static int getHeight(View view) {
        if (view != null) {
            return view.getHeight();
        }

        return 0;
    }

    public static void setHeight(View view, int height) {
        if (view != null) {
            ViewGroup.LayoutParams lp = view.getLayoutParams();
            lp.height = height;
            view.setLayoutParams(lp);
        }
    }

    public static int getScrollX(View view) {
        if (view != null) {
            return view.getScrollX();
        }

        return 0;
    }

    public static int getScrollY(View view) {
        if (view != null) {
            return view.getScrollY();
        }

        return 0;
    }

    public static void scrollTo(View view, int scrollX, int scrollY) {
        if (view != null) {
            view.scrollTo(scrollX, scrollY);
        }
    }
}
