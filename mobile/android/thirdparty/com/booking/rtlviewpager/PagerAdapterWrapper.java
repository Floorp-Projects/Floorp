/*
 * Copyright 2015 Diego Gómez Olvera
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.booking.rtlviewpager;

import android.database.DataSetObservable;
import android.database.DataSetObserver;
import android.os.Parcelable;
import android.support.annotation.NonNull;
import android.support.v4.view.PagerAdapter;
import android.view.View;
import android.view.ViewGroup;

/**
 * PagerAdapter decorator.
 */
class PagerAdapterWrapper extends PagerAdapter {

    @NonNull
    private final PagerAdapter adapter;
    private final DataSetObservable dataSetObservable = new DataSetObservable();


    protected PagerAdapterWrapper(@NonNull PagerAdapter adapter) {
        this.adapter = adapter;
        this.adapter.registerDataSetObserver(new DataSetObserver() {
            @Override
            public void onChanged() {
                PagerAdapterWrapper.super.notifyDataSetChanged();
                dataSetObservable.notifyChanged();
            }

            @Override
            public void onInvalidated() {
                dataSetObservable.notifyInvalidated();
            }
        });
    }

    @NonNull
    public PagerAdapter getInnerAdapter() {
        return adapter;
    }

    @Override
    public int getCount() {
        return adapter.getCount();
    }

    @Override
    public boolean isViewFromObject(View view, Object object) {
        return adapter.isViewFromObject(view, object);
    }

    @Override
    public CharSequence getPageTitle(int position) {
        return adapter.getPageTitle(position);
    }

    @Override
    public float getPageWidth(int position) {
        return adapter.getPageWidth(position);
    }

    @Override
    public int getItemPosition(Object object) {
        return adapter.getItemPosition(object);
    }

    @Override
    public Object instantiateItem(ViewGroup container, int position) {
        return adapter.instantiateItem(container, position);
    }

    @Override
    public void destroyItem(ViewGroup container, int position, Object object) {
        adapter.destroyItem(container, position, object);
    }

    @Override
    public void setPrimaryItem(ViewGroup container, int position, Object object) {
        adapter.setPrimaryItem(container, position, object);
    }

    @Override
    public void notifyDataSetChanged() {
        adapter.notifyDataSetChanged();
    }

    @Override
    public void registerDataSetObserver(DataSetObserver observer) {
        dataSetObservable.registerObserver(observer);
    }

    @Override
    public void unregisterDataSetObserver(DataSetObserver observer) {
        dataSetObservable.unregisterObserver(observer);
    }

    @Override
    public Parcelable saveState() {
        return adapter.saveState();
    }

    @Override
    public void restoreState(Parcelable state, ClassLoader loader) {
        adapter.restoreState(state, loader);
    }

    @Override
    public void startUpdate(ViewGroup container) {
        adapter.startUpdate(container);
    }

    @Override
    public void finishUpdate(ViewGroup container) {
        adapter.finishUpdate(container);
    }
}
