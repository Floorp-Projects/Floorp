/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.animation;

import android.view.View;

public final class ViewHelper {
	private ViewHelper() {
	}

	public static float getTranslationX(View view) {
		AnimatorProxy proxy = AnimatorProxy.create(view);
		return proxy.getTranslationX();
	}

	public static void setTranslationX(View view, float translationX) {
		final AnimatorProxy proxy = AnimatorProxy.create(view);
		proxy.setTranslationX(translationX);
	}

	public static float getTranslationY(View view) {
		final AnimatorProxy proxy = AnimatorProxy.create(view);
		return proxy.getTranslationY();
	}

	public static void setTranslationY(View view, float translationY) {
		final AnimatorProxy proxy = AnimatorProxy.create(view);
		proxy.setTranslationY(translationY);
	}

	public static float getAlpha(View view) {
		final AnimatorProxy proxy = AnimatorProxy.create(view);
		return proxy.getAlpha();
	}

	public static void setAlpha(View view, float alpha) {
		final AnimatorProxy proxy = AnimatorProxy.create(view);
		proxy.setAlpha(alpha);
	}

	public static int getWidth(View view) {
		final AnimatorProxy proxy = AnimatorProxy.create(view);
		return proxy.getWidth();
	}

	public static void setWidth(View view, int width) {
		final AnimatorProxy proxy = AnimatorProxy.create(view);
		proxy.setWidth(width);
	}

	public static int getHeight(View view) {
		final AnimatorProxy proxy = AnimatorProxy.create(view);
		return proxy.getHeight();
	}

	public static void setHeight(View view, int height) {
		final AnimatorProxy proxy = AnimatorProxy.create(view);
		proxy.setHeight(height);
	}
}
