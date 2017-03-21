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


import android.content.Context;
import android.database.DataSetObserver;
import android.os.Parcel;
import android.os.Parcelable;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v4.text.TextUtilsCompat;
import android.support.v4.util.ArrayMap;
import android.support.v4.view.PagerAdapter;
import android.support.v4.view.ViewCompat;
import android.support.v4.view.ViewPager;
import android.util.AttributeSet;
import android.view.ViewGroup;

import java.util.Map;

/**
 * ViewPager that reverses the items order in RTL locales.
 */
public class RtlViewPager extends ViewPager {

    @NonNull
    private final Map<OnPageChangeListener, ReverseOnPageChangeListener> reverseOnPageChangeListeners;

    @Nullable
    private DataSetObserver dataSetObserver;

    private boolean suppressOnPageChangeListeners;

    public RtlViewPager(Context context) {
        super(context);
        reverseOnPageChangeListeners = new ArrayMap<>(1);
    }

    public RtlViewPager(Context context, AttributeSet attrs) {
        super(context, attrs);
        reverseOnPageChangeListeners = new ArrayMap<>(1);
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        registerRtlDataSetObserver(super.getAdapter());
    }

    @Override
    protected void onDetachedFromWindow() {
        unregisterRtlDataSetObserver();
        super.onDetachedFromWindow();
    }

    private void registerRtlDataSetObserver(PagerAdapter adapter) {
        if (adapter instanceof ReverseAdapter && dataSetObserver == null) {
            dataSetObserver = new RevalidateIndicesOnContentChange((ReverseAdapter) adapter);
            adapter.registerDataSetObserver(dataSetObserver);
            ((ReverseAdapter) adapter).revalidateIndices();
        }
    }

    private void unregisterRtlDataSetObserver() {
        final PagerAdapter adapter = super.getAdapter();

        if (adapter instanceof ReverseAdapter && dataSetObserver != null) {
            adapter.unregisterDataSetObserver(dataSetObserver);
            dataSetObserver = null;
        }
    }

    @Override
    public void setCurrentItem(int item, boolean smoothScroll) {
        super.setCurrentItem(convert(item), smoothScroll);
    }

    @Override
    public void setCurrentItem(int item) {
        super.setCurrentItem(convert(item));
    }

    @Override
    public int getCurrentItem() {
        return convert(super.getCurrentItem());
    }

    private int convert(int position) {
        if (position >= 0 && isRtl()) {
            return getAdapter() == null ? 0 : getAdapter().getCount() - position - 1;
        } else {
            return position;
        }
    }

    @Nullable
    @Override
    public PagerAdapter getAdapter() {
        final PagerAdapter adapter = super.getAdapter();
        return adapter instanceof ReverseAdapter ? ((ReverseAdapter) adapter).getInnerAdapter() : adapter;
    }

    @Override
    public void fakeDragBy(float xOffset) {
        super.fakeDragBy(isRtl() ? xOffset : -xOffset);
    }

    @Override
    public void setAdapter(@Nullable PagerAdapter adapter) {
        unregisterRtlDataSetObserver();

        final boolean rtlReady = adapter != null && isRtl();
        if (rtlReady) {
            adapter = new ReverseAdapter(adapter);
            registerRtlDataSetObserver(adapter);
        }
        super.setAdapter(adapter);
        if (rtlReady) {
            setCurrentItemWithoutNotification(0);
        }
    }

    private void setCurrentItemWithoutNotification(int index) {
        suppressOnPageChangeListeners = true;
        setCurrentItem(index, false);
        suppressOnPageChangeListeners = false;
    }

    protected boolean isRtl() {
        return TextUtilsCompat.getLayoutDirectionFromLocale(
                getContext().getResources().getConfiguration().locale) == ViewCompat.LAYOUT_DIRECTION_RTL;
    }

    @Override
    public void addOnPageChangeListener(@NonNull OnPageChangeListener listener) {
        if (isRtl()) {
            final ReverseOnPageChangeListener reverseListener = new ReverseOnPageChangeListener(listener);
            reverseOnPageChangeListeners.put(listener, reverseListener);
            listener = reverseListener;
        }
        super.addOnPageChangeListener(listener);
    }

    @Override
    public void removeOnPageChangeListener(@NonNull OnPageChangeListener listener) {
        if (isRtl()) {
            listener = reverseOnPageChangeListeners.remove(listener);
        }
        super.removeOnPageChangeListener(listener);
    }

    @Override
    public Parcelable onSaveInstanceState() {
        return new SavedState(super.onSaveInstanceState(), getCurrentItem(), isRtl());
    }

    @Override
    public void onRestoreInstanceState(Parcelable state) {
        SavedState ss = (SavedState) state;
        super.onRestoreInstanceState(ss.superState);
        if (ss.isRTL != isRtl()) setCurrentItem(ss.position, false);
    }

    private class ReverseAdapter extends PagerAdapterWrapper {

        private int lastCount;

        public ReverseAdapter(@NonNull PagerAdapter adapter) {
            super(adapter);
            lastCount = adapter.getCount();
        }

        @Override
        public CharSequence getPageTitle(int position) {
            return super.getPageTitle(reverse(position));
        }

        @Override
        public float getPageWidth(int position) {
            return super.getPageWidth(reverse(position));
        }

        @Override
        public int getItemPosition(Object object) {
            final int itemPosition = super.getItemPosition(object);
            return itemPosition < 0 ? itemPosition : reverse(itemPosition);
        }

        @Override
        public Object instantiateItem(ViewGroup container, int position) {
            return super.instantiateItem(container, reverse(position));
        }

        @Override
        public void destroyItem(ViewGroup container, int position, Object object) {
            super.destroyItem(container, reverse(position), object);
        }

        @Override
        public void setPrimaryItem(ViewGroup container, int position, Object object) {
            super.setPrimaryItem(container, lastCount - position - 1, object);
        }

        private int reverse(int position) {
            return getCount() - position - 1;
        }

        private void revalidateIndices() {
            final int newCount = getCount();
            if (newCount != lastCount) {
                setCurrentItemWithoutNotification(Math.max(0, lastCount - 1));
                lastCount = newCount;
            }
        }
    }

    private static class RevalidateIndicesOnContentChange extends DataSetObserver {
        @NonNull
        private final ReverseAdapter adapter;

        private RevalidateIndicesOnContentChange(@NonNull ReverseAdapter adapter) {
            this.adapter = adapter;
        }

        @Override
        public void onChanged() {
            super.onChanged();
            adapter.revalidateIndices();
        }
    }

    private class ReverseOnPageChangeListener implements OnPageChangeListener {

        @NonNull
        private final OnPageChangeListener original;

        private int pagerPosition;

        private ReverseOnPageChangeListener(@NonNull OnPageChangeListener original) {
            this.original = original;
            pagerPosition = -1;
        }

        @Override
        public void onPageScrolled(int position, float positionOffset, int positionOffsetPixels) {
            if (!suppressOnPageChangeListeners) {
                if (positionOffset == 0f && positionOffsetPixels == 0) {
                    pagerPosition = reverse(position);
                } else {
                    pagerPosition = reverse(position + 1);
                }
                original.onPageScrolled(pagerPosition, positionOffset > 0 ? 1f - positionOffset : positionOffset, positionOffsetPixels);
            }
        }

        @Override
        public void onPageSelected(int position) {
            if (!suppressOnPageChangeListeners) {
                original.onPageSelected(reverse(position));
            }
        }

        @Override
        public void onPageScrollStateChanged(int state) {
            if (!suppressOnPageChangeListeners) {
                original.onPageScrollStateChanged(state);
            }
        }

        private int reverse(int position) {
            final PagerAdapter adapter = getAdapter();
            return adapter == null ? position : adapter.getCount() - position - 1;
        }
    }

    public static class SavedState implements Parcelable {

        Parcelable superState;
        int position;
        boolean isRTL;

        public SavedState(Parcelable superState, int position, boolean isRTL) {
            super();
            this.superState = superState;
            this.position = position;
            this.isRTL = isRTL;
        }

        SavedState(Parcel in, ClassLoader loader) {
            if (loader == null) {
                loader = getClass().getClassLoader();
            }
            superState = in.readParcelable(loader);
            position = in.readInt();
            isRTL = in.readByte() != 0;
        }

        @Override
        public void writeToParcel(Parcel out, int flags) {
            out.writeParcelable(superState, flags);
            out.writeInt(position);
            out.writeByte(isRTL ? (byte) 1 : (byte) 0);
        }

        @Override
        public int describeContents() {
            return 0;
        }

        public static final ClassLoaderCreator<SavedState> CREATOR = new
                ClassLoaderCreator<SavedState>() {
            @Override
            public SavedState createFromParcel(Parcel source, ClassLoader loader) {
                return new SavedState(source, loader);
            }

            @Override
            public SavedState createFromParcel(Parcel source) {
                return new SavedState(source, null);
            }

            public SavedState[] newArray(int size) {
                return new SavedState[size];
            }
        };
    }
}
