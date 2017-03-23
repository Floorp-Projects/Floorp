/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.fragment;

import android.graphics.Typeface;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.v4.app.Fragment;
import android.text.SpannableString;
import android.text.style.StyleSpan;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.mozilla.focus.R;
import org.mozilla.focus.autocomplete.UrlAutoCompleteFilter;
import org.mozilla.focus.utils.UrlUtils;
import org.mozilla.focus.utils.ViewUtils;
import org.mozilla.focus.widget.InlineAutocompleteEditText;

/**
 * Fragment for displaying he URL input controls.
 */
public class UrlInputFragment extends Fragment implements View.OnClickListener, InlineAutocompleteEditText.OnTextChangeListener, InlineAutocompleteEditText.OnCommitListener {
    public static final String ARGUMENT_URL = "url";

    public static UrlInputFragment create() {
        final UrlInputFragment fragment = new UrlInputFragment();
        fragment.setArguments(new Bundle());

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

    @Override
    public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        final View view = inflater.inflate(R.layout.fragment_urlinput, container, false);

        final ViewGroup backgroundView = (ViewGroup) view.findViewById(R.id.background);
        backgroundView.setOnClickListener(this);

        view.findViewById(R.id.dismiss).setOnClickListener(this);

        clearView = view.findViewById(R.id.clear);
        clearView.setOnClickListener(this);

        searchViewContainer = view.findViewById(R.id.search_hint_container);

        searchView =  (TextView)view.findViewById(R.id.search_hint);
        searchView.setOnClickListener(this);

        urlView = (InlineAutocompleteEditText) view.findViewById(R.id.url_edit);
        urlView.setOnFilterListener(new UrlAutoCompleteFilter(getContext()));
        urlView.setOnFocusChangeListener(new View.OnFocusChangeListener() {
            @Override
            public void onFocusChange(View v, boolean hasFocus) {
                if (hasFocus) {
                    ViewUtils.showKeyboard(urlView);
                }
            }
        });

        urlView.setOnCommitListener(this);

        urlView.setOnTextChangeListener(this);

        if (getArguments().containsKey(ARGUMENT_URL)) {
            urlView.setText(getArguments().getString(ARGUMENT_URL));
            clearView.setVisibility(View.VISIBLE);
        }

        return view;
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
                getActivity().getSupportFragmentManager()
                        .beginTransaction()
                        .remove(this)
                        .commit();
                break;
        }
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
        // Since we've completed URL entry, we no longer want the URL entry fragment in our backstack
        // (exiting browser activity should return you to the main activity), so we pop that entry (which
        // essentially removes the fragment which was add()ed elsewhere)
        getActivity().getSupportFragmentManager().popBackStack();

        // Replace all fragments with a fresh browser fragment. This means we either remove the
        // HomeFragment with an UrlInputFragment on top or an old BrowserFragment with an
        // UrlInputFragment.
        final BrowserFragment browserFragment = (BrowserFragment) getActivity().getSupportFragmentManager()
                .findFragmentByTag(BrowserFragment.FRAGMENT_TAG);

        if (browserFragment != null && browserFragment.isVisible()) {
            // Reuse existing visible fragment - in this case we know the user is already browsing.
            // The fragment might exist if we "erased" a browsing session, hence we need to check
            // for visibility in addition to existence.
            browserFragment.loadURL(url);
        } else {
            getActivity().getSupportFragmentManager()
                    .beginTransaction()
                    .replace(R.id.container, BrowserFragment.create(url), BrowserFragment.FRAGMENT_TAG)
                    .addToBackStack("browser")
                    .commit();
        }
    }

    @Override
    public void onTextChange(String originalText, String autocompleteText) {
        if (originalText.length() == 0) {
            clearView.setVisibility(View.GONE);
            searchViewContainer.setVisibility(View.GONE);
        } else {
            clearView.setVisibility(View.VISIBLE);

            final String hint = getString(R.string.search_hint, originalText);

            final SpannableString content = new SpannableString(hint);
            content.setSpan(new StyleSpan(Typeface.BOLD), hint.length() - originalText.length(), hint.length(), 0);

            searchView.setText(content);
            searchViewContainer.setVisibility(View.VISIBLE);
        }
    }
}
