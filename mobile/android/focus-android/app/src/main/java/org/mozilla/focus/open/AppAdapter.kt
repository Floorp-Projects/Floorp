/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.open;

import android.content.Context;
import android.content.pm.ActivityInfo;
import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.ViewGroup;

import androidx.recyclerview.widget.RecyclerView;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;

public class AppAdapter extends RecyclerView.Adapter<RecyclerView.ViewHolder> {
    static class App {
        private final Context context;
        private final ActivityInfo info;
        private final String label;

        public App(Context context, ActivityInfo info) {
            this.context = context;
            this.info = info;
            this.label = info.loadLabel(context.getPackageManager()).toString();
        }

        public String getLabel() {
            return label;
        }

        public Drawable loadIcon() {
            return info.loadIcon(context.getPackageManager());
        }

        public String getPackageName() {
            return info.packageName;
        }

        public String getActivityName() {
            return info.name;
        }

    }

    interface OnAppSelectedListener {
        void onAppSelected(App app);
    }

    private final List<App> apps;
    private final App store;
    private OnAppSelectedListener listener;

    public AppAdapter(Context context, ActivityInfo[] infoArray, ActivityInfo store) {
        final List<App> apps = new ArrayList<>(infoArray.length);

        for (ActivityInfo info : infoArray) {
            apps.add(new App(context, info));
        }

        Collections.sort(apps, new Comparator<App>() {
            @Override
            public int compare(App app1, App app2) {
                return app1.getLabel().compareTo(app2.getLabel());
            }
        });

        this.apps = apps;
        this.store = store != null ? new App(context, store) : null;
    }

    @Override
    public RecyclerView.ViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
        final LayoutInflater inflater = LayoutInflater.from(parent.getContext());

        if (viewType == AppViewHolder.LAYOUT_ID) {
            return new AppViewHolder(
                    inflater.inflate(AppViewHolder.LAYOUT_ID, parent, false));
        } else if (viewType == InstallBannerViewHolder.LAYOUT_ID) {
            return new InstallBannerViewHolder(
                    inflater.inflate(InstallBannerViewHolder.LAYOUT_ID, parent, false));
        }

        throw new IllegalStateException("Unknown view type: " + viewType);
    }

    /* package */ void setOnAppSelectedListener(OnAppSelectedListener listener) {
        this.listener = listener;
    }

    @Override
    public int getItemViewType(int position) {
        if (position < apps.size()) {
            return AppViewHolder.LAYOUT_ID;
        } else {
            return InstallBannerViewHolder.LAYOUT_ID;
        }
    }

    @Override
    public void onBindViewHolder(RecyclerView.ViewHolder holder, int position) {
        final int itemViewType = holder.getItemViewType();

        if (itemViewType == AppViewHolder.LAYOUT_ID) {
            bindApp(holder, position);
        } else if (itemViewType == InstallBannerViewHolder.LAYOUT_ID) {
            bindInstallBanner(holder);
        }
    }

    private void bindApp(RecyclerView.ViewHolder holder, int position) {
        final AppViewHolder appViewHolder = (AppViewHolder) holder;
        final App app = apps.get(position);

        appViewHolder.bind(app, listener);
    }

    private void bindInstallBanner(RecyclerView.ViewHolder holder) {
        final InstallBannerViewHolder installViewHolder = (InstallBannerViewHolder) holder;
        installViewHolder.bind(store);
    }

    @Override
    public int getItemCount() {
        return apps.size() + (store != null ? 1 : 0);
    }
}
