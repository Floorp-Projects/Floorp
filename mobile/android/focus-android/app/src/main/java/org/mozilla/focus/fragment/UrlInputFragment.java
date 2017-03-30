/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.fragment;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.graphics.Typeface;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentManager;
import android.text.SpannableString;
import android.text.style.StyleSpan;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;
import android.view.animation.DecelerateInterpolator;
import android.widget.TextView;

import org.mozilla.focus.R;
import org.mozilla.focus.autocomplete.UrlAutoCompleteFilter;
import org.mozilla.focus.utils.UrlUtils;
import org.mozilla.focus.utils.ViewUtils;
import org.mozilla.focus.widget.InlineAutocompleteEditText;

/**
 * Fragment for displaying he URL input controls.
 */
public class UrlInputFragment extends Fragment implements View.OnClickListener, InlineAutocompleteEditText.OnCommitListener, InlineAutocompleteEditText.OnFilterListener {
    public static final String FRAGMENT_TAG = "url_input";

    private static final String ARGUMENT_URL = "url";
    private static final String ARGUMENT_ANIMATION = "animation";
    private static final String ARGUMENT_X = "x";
    private static final String ARGUMENT_Y = "y";
    private static final String ARGUMENT_WIDTH = "width";
    private static final String ARGUMENT_HEIGHT = "height";

    private static final String ANIMATION_HOME_SCREEN = "home_screen";

    /**
     * Create a new UrlInputFragment and animate the url input view from the position/size of the
     * fake url bar view.
     */
    public static UrlInputFragment createWithHomeScreenAnimation(View fakeUrlBarView) {
        int[] screenLocation = new int[2];
        fakeUrlBarView.getLocationOnScreen(screenLocation);

        Bundle arguments = new Bundle();
        arguments.putString(ARGUMENT_ANIMATION, ANIMATION_HOME_SCREEN);
        arguments.putInt(ARGUMENT_X, screenLocation[0]);
        arguments.putInt(ARGUMENT_Y, screenLocation[1]);
        arguments.putInt(ARGUMENT_WIDTH, fakeUrlBarView.getWidth());
        arguments.putInt(ARGUMENT_HEIGHT, fakeUrlBarView.getHeight());

        UrlInputFragment fragment = new UrlInputFragment();
        fragment.setArguments(arguments);

        return fragment;
    }

    public static UrlInputFragment create(String url) {
        final UrlInputFragment fragment = new UrlInputFragment();

        final Bundle arguments = new Bundle();
        arguments.putString(ARGUMENT_URL, url);
        fragment.setArguments(arguments);

        return fragment;
    }

    private InlineAutocompleteEditText urlView;
    private View clearView;
    private View searchViewContainer;
    private TextView searchView;

    private UrlAutoCompleteFilter urlAutoCompleteFilter;
    private View dismissView;
    private View urlBackgroundView;
    private View toolbarBackgroundView;

    @Override
    public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        final View view = inflater.inflate(R.layout.fragment_urlinput, container, false);

        final ViewGroup backgroundView = (ViewGroup) view.findViewById(R.id.background);
        backgroundView.setOnClickListener(this);

        dismissView = view.findViewById(R.id.dismiss);
        dismissView.setOnClickListener(this);

        clearView = view.findViewById(R.id.clear);
        clearView.setOnClickListener(this);

        searchViewContainer = view.findViewById(R.id.search_hint_container);

        searchView =  (TextView)view.findViewById(R.id.search_hint);
        searchView.setOnClickListener(this);

        urlAutoCompleteFilter = new UrlAutoCompleteFilter(getContext());

        urlView = (InlineAutocompleteEditText) view.findViewById(R.id.url_edit);
        urlView.setOnFilterListener(this);
        urlView.setOnFocusChangeListener(new View.OnFocusChangeListener() {
            @Override
            public void onFocusChange(View v, boolean hasFocus) {
                if (hasFocus) {
                    ViewUtils.showKeyboard(urlView);
                }
            }
        });

        urlBackgroundView = view.findViewById(R.id.url_background);

        toolbarBackgroundView = view.findViewById(R.id.toolbar_background);

        urlBackgroundView.getViewTreeObserver().addOnPreDrawListener(new ViewTreeObserver.OnPreDrawListener() {
            @Override
            public boolean onPreDraw() {
                urlBackgroundView.getViewTreeObserver().removeOnPreDrawListener(this);

                animateFirstDraw();

                return true;
            }
        });

        urlView.setOnCommitListener(this);

        if (getArguments().containsKey(ARGUMENT_URL)) {
            urlView.setText(getArguments().getString(ARGUMENT_URL));
            clearView.setVisibility(View.VISIBLE);
        }

        return view;
    }

    public boolean onBackPressed() {
        animateAndDismiss();
        return true;
    }

    @Override
    public void onStart() {
        super.onStart();

        urlView.requestFocus();
    }

    @Override
    public void onClick(View view) {
        switch (view.getId()) {
            case R.id.clear:
                urlView.setText("");
                urlView.requestFocus();
                break;

            case R.id.search_hint:
                onSearch();
                break;

            case R.id.dismiss:
                animateAndDismiss();
                break;
        }
    }

    private void animateFirstDraw() {
        if (ANIMATION_HOME_SCREEN.equals(getArguments().getString(ARGUMENT_ANIMATION))) {
            playHomeScreenAnimation(false);
        }
    }

    private void animateAndDismiss() {
        if (ANIMATION_HOME_SCREEN.equals(getArguments().getString(ARGUMENT_ANIMATION))) {
            playHomeScreenAnimation(true);
        } else {
            dismiss();
        }
    }

    /**
     * Play animation between home screen and the URL input.
     */
    private void playHomeScreenAnimation(final boolean reverse) {
        int[] screenLocation = new int[2];
        urlBackgroundView.getLocationOnScreen(screenLocation);

        int leftDelta = getArguments().getInt(ARGUMENT_X) - screenLocation[0];
        int topDelta = getArguments().getInt(ARGUMENT_Y) - screenLocation[1];

        float widthScale = (float) getArguments().getInt(ARGUMENT_WIDTH) / urlBackgroundView.getWidth();
        float heightScale = (float) getArguments().getInt(ARGUMENT_HEIGHT) / urlBackgroundView.getHeight();

        if (!reverse) {
            // Move all views to the position of the fake URL bar on the home screen. Hide them until
            // the animation starts because we need to switch between fake URL bar and the actual URL
            // bar once the animation starts.
            urlBackgroundView.setAlpha(0);
            urlBackgroundView.setPivotX(0);
            urlBackgroundView.setPivotY(0);
            urlBackgroundView.setScaleX(widthScale);
            urlBackgroundView.setScaleY(heightScale);
            urlBackgroundView.setTranslationX(leftDelta);
            urlBackgroundView.setTranslationY(topDelta);

            toolbarBackgroundView.setAlpha(0);
            toolbarBackgroundView.setTranslationY(-toolbarBackgroundView.getHeight());

            dismissView.setAlpha(0);
        }

        // Move the URL bar from its position on the home screen to the actual position (and scale it).
        urlBackgroundView.animate()
                .setDuration(200)
                .scaleX(reverse ? widthScale : 1)
                .scaleY(reverse ? heightScale : 1)
                .translationX(reverse ? leftDelta : 0)
                .translationY(reverse ? topDelta : 0)
                .setInterpolator(new DecelerateInterpolator())
                .setListener(new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationStart(Animator animation) {
                        ViewUtils.updateAlphaIfViewExists(getActivity(), R.id.fake_urlbar, 0f);

                        urlBackgroundView.setAlpha(1);
                    }

                    @Override
                    public void onAnimationEnd(Animator animation) {
                        if (reverse) {
                            urlBackgroundView.setAlpha(0f);

                            ViewUtils.updateAlphaIfViewExists(getActivity(), R.id.fake_urlbar, 1f);

                            dismiss();
                        }
                    }
                });

        // Let the toolbar background come int from the top
        toolbarBackgroundView.animate()
                .alpha(reverse ? 0 : 1)
                .translationY(reverse ? -toolbarBackgroundView.getHeight() : 0)
                .setDuration(200)
                .setInterpolator(new DecelerateInterpolator());

        // Use an alpha animation on the transparent black background
        dismissView.animate()
                .alpha(reverse ? 0 : 1)
                .setDuration(200);
    }

    private void dismiss() {
        getActivity().getSupportFragmentManager()
                .beginTransaction()
                .remove(this)
                .commit();
    }

    @Override
    public void onCommit() {
        ViewUtils.hideKeyboard(urlView);

        final String rawUrl = urlView.getText().toString();

        final String url = UrlUtils.isUrl(rawUrl)
                ? UrlUtils.normalize(rawUrl)
                : UrlUtils.createSearchUrl(getContext(), rawUrl);

        openUrl(url);
    }

    private void onSearch() {
        final String searchUrl = UrlUtils.createSearchUrl(getContext(), urlView.getOriginalText());

        openUrl(searchUrl);
    }

    private void openUrl(String url) {
        final FragmentManager fragmentManager = getActivity().getSupportFragmentManager();

        // Replace all fragments with a fresh browser fragment. This means we either remove the
        // HomeFragment with an UrlInputFragment on top or an old BrowserFragment with an
        // UrlInputFragment.
        final BrowserFragment browserFragment = (BrowserFragment) fragmentManager
                .findFragmentByTag(BrowserFragment.FRAGMENT_TAG);

        if (browserFragment != null && browserFragment.isVisible()) {
            // Reuse existing visible fragment - in this case we know the user is already browsing.
            // The fragment might exist if we "erased" a browsing session, hence we need to check
            // for visibility in addition to existence.
            browserFragment.loadURL(url);

            // And this fragment can be removed again.
            fragmentManager.beginTransaction()
                    .remove(this)
                    .commit();
        } else {
            fragmentManager
                    .beginTransaction()
                    .replace(R.id.container, BrowserFragment.create(url), BrowserFragment.FRAGMENT_TAG)
                    .commit();
        }
    }

    @Override
    public void onFilter(String searchText, InlineAutocompleteEditText view) {
        urlAutoCompleteFilter.onFilter(searchText, view);

        if (searchText.length() == 0) {
            clearView.setVisibility(View.GONE);
            searchViewContainer.setVisibility(View.GONE);
        } else {
            clearView.setVisibility(View.VISIBLE);

            final String hint = getString(R.string.search_hint, searchText);

            final SpannableString content = new SpannableString(hint);
            content.setSpan(new StyleSpan(Typeface.BOLD), hint.length() - searchText.length(), hint.length(), 0);

            searchView.setText(content);
            searchViewContainer.setVisibility(View.VISIBLE);
        }
    }
}
