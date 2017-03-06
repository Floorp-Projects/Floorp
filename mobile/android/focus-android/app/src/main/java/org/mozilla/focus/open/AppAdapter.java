/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.open;

import android.content.Context;
import android.content.pm.ActivityInfo;
import android.graphics.drawable.Drawable;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.mozilla.focus.R;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;

public class AppAdapter extends RecyclerView.Adapter<AppViewHolder> {
    /* package-private */ class App {
        private Context context;
        private ActivityInfo info;
        private String label;

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
    }

    /* package-private */ interface OnAppSelectedListener {
        void onAppSelected(App app);
    }

    private List<App> apps;
    private OnAppSelectedListener listener;

    public AppAdapter(Context context, ActivityInfo[] infoArray) {
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
    }

    @Override
    public AppViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
        final View view = LayoutInflater.from(parent.getContext()).inflate(R.layout.item_app, parent, false);
        return new AppViewHolder(view);
    }

    public void setOnAppSelectedListener(OnAppSelectedListener listener) {
        this.listener = listener;
    }

    @Override
    public void onBindViewHolder(AppViewHolder holder, int position) {
        final App app = apps.get(position);

        holder.bind(app);

        holder.itemView.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if (listener != null) {
                    listener.onAppSelected(app);
                }
            }
        });
    }

    @Override
    public int getItemCount() {
        return apps.size();
    }
}
