/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.fragment;

import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.v4.app.Fragment;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.mozilla.focus.R;
import org.mozilla.focus.autocomplete.UrlAutoCompleteFilter;
import org.mozilla.focus.utils.UrlUtils;
import org.mozilla.focus.utils.ViewUtils;
import org.mozilla.focus.widget.InlineAutocompleteEditText;

/**
 * Fragment for displaying he URL input controls.
 */
public class UrlInputFragment extends Fragment implements View.OnClickListener, TextWatcher {
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

    @Override
    public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        final View view = inflater.inflate(R.layout.fragment_urlinput, container, false);

        view.findViewById(R.id.background).setOnClickListener(this);

        clearView = view.findViewById(R.id.clear);
        clearView.setOnClickListener(this);

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

        urlView.setOnCommitListener(new InlineAutocompleteEditText.OnCommitListener() {
            @Override
            public void onCommit() {
                onUrlEntered();
            }
        });

        urlView.addTextChangedListener(this);

        if (getArguments().containsKey(ARGUMENT_URL)) {
            urlView.setText(getArguments().getString(ARGUMENT_URL));
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
        }
    }

    private void onUrlEntered() {
        ViewUtils.hideKeyboard(urlView);

        final String rawUrl = urlView.getText().toString();

        final String url = UrlUtils.isUrl(rawUrl)
                ? UrlUtils.normalize(rawUrl)
                : UrlUtils.createSearchUrl(rawUrl);

        // Replace all fragments with a fresh browser fragment. This means we either remove the
        // HomeFragment with an UrlInputFragmnet on top or an old BrowserFragment with an
        // UrlInputFragment.
        getActivity().getSupportFragmentManager()
                .beginTransaction()
                .replace(R.id.container, BrowserFragment.create(url), BrowserFragment.FRAGMENT_TAG)
                .commit();
    }

    @Override
    public void beforeTextChanged(CharSequence text, int start, int count, int after) {
        // TextWatcher - Unused
    }

    @Override
    public void onTextChanged(CharSequence text, int start, int before, int count) {
        // TextWatcher - Unused
    }

    @Override
    public void afterTextChanged(Editable editable) {
        if (editable.length() == 0) {
            clearView.setVisibility(View.GONE);
        } else {
            clearView.setVisibility(View.VISIBLE);
        }
    }
}
