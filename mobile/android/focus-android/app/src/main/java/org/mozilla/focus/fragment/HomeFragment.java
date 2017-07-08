/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.fragment;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentActivity;
import android.support.v4.app.FragmentManager;
import android.support.v7.widget.PopupMenu;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;

import org.mozilla.focus.R;
import org.mozilla.focus.activity.InfoActivity;
import org.mozilla.focus.locale.LocaleAwareAppCompatActivity;
import org.mozilla.focus.locale.LocaleAwareFragment;

/**
 * Fragment displaying the "home" screen when launching the app.
 */
public class HomeFragment
        extends LocaleAwareFragment
        implements View.OnClickListener, PopupMenu.OnMenuItemClickListener, PopupMenu.OnDismissListener {
    public static final String FRAGMENT_TAG = "home";

    public static HomeFragment create() {
        return new HomeFragment();
    }

    private View fakeUrlBarView;

    @Nullable private PopupMenu displayedPopupMenu = null;

    @Override
    public void applyLocale() {
        // We only show static text in this fragment, it's simplest just to reload everything:
        getActivity()
                .getSupportFragmentManager()
                .beginTransaction()
                .replace(R.id.container, HomeFragment.create(), HomeFragment.FRAGMENT_TAG)
                .commit();
    }

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);

        final FragmentActivity activity = getActivity();
        // No safe intent needed here - intent.getAction() is always safe:
        if (activity != null && Intent.ACTION_VIEW.equals(activity.getIntent().getAction())) {
            // If this activity was launched from a ACTION_VIEW intent then pressing back will send
            // the user back to the previous application (finishing this activity). However if the
            // user presses "erase" we will send the user back to the home screen. Pressing back
            // in a new browsing session should send the user back to the home screen instead of
            // the previous app. Therefore we clear the intent once we show the home screen.
            activity.setIntent(new Intent(Intent.ACTION_MAIN));
        }
    }

    @Override
    public void onDetach() {
        super.onDetach();

        // On detach, the PopupMenu is no longer relevant to other content (e.g. BrowserFragment) so dismiss it.
        // Note: if we don't dismiss the PopupMenu, its onMenuItemClick method references the old Fragment, which now
        // has a null Context and will cause crashes.
        if (displayedPopupMenu != null) {
            displayedPopupMenu.dismiss();
        }
    }

    @Override
    public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        final View view = inflater.inflate(R.layout.fragment_home, container, false);

        fakeUrlBarView = view.findViewById(R.id.fake_urlbar);
        fakeUrlBarView.setOnClickListener(this);

        view.findViewById(R.id.menu).setOnClickListener(this);

        return view;
    }

    @Override
    public void onClick(View view) {
        switch (view.getId()) {
            case R.id.fake_urlbar:
                showUrlInput();
                break;

            case R.id.menu:
                final PopupMenu popupMenu = new PopupMenu(view.getContext(), view);
                popupMenu.getMenuInflater().inflate(R.menu.menu_home, popupMenu.getMenu());
                popupMenu.setOnMenuItemClickListener(this);
                popupMenu.setGravity(Gravity.TOP);
                popupMenu.show();
                displayedPopupMenu = popupMenu;
                break;

            default:
                throw new IllegalStateException("Unhandled view ID in onClick()");
        }
    }

    private synchronized void showUrlInput() {
        final FragmentManager fragmentManager = getActivity().getSupportFragmentManager();

        final Fragment existingFragment = fragmentManager.findFragmentByTag(UrlInputFragment.FRAGMENT_TAG);
        if (existingFragment != null && existingFragment.isAdded() && !existingFragment.isRemoving()) {
            // We are already showing an URL input fragment. This might have been a double click on the
            // fake URL bar. Just ignore it.
            return;
        }

        final UrlInputFragment fragment = UrlInputFragment.createWithHomeScreenAnimation(fakeUrlBarView);

        fragmentManager.beginTransaction()
                .add(R.id.container, fragment, UrlInputFragment.FRAGMENT_TAG)
                .commit();
    }

    @Override
    public boolean onMenuItemClick(MenuItem item) {
        final int id = item.getItemId();

        switch (id) {
            case R.id.settings:
                ((LocaleAwareAppCompatActivity) getActivity()).openPreferences();
                return true;

            case R.id.about:
                Intent aboutIntent = InfoActivity.getAboutIntent(getActivity());
                startActivity(aboutIntent);
                return true;

            case R.id.rights:
                Intent rightsIntent = InfoActivity.getRightsIntent(getActivity());
                startActivity(rightsIntent);
                return true;

            case R.id.help:
                Intent helpIntent = InfoActivity.getHelpIntent(getActivity());
                startActivity(helpIntent);
                return true;

            default:
                throw new IllegalStateException("Unhandled view ID in onMenuItemClick()");
        }
    }

    @Override
    public void onDismiss(final PopupMenu menu) {
        displayedPopupMenu = null;
    }
}
