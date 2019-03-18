/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.media;

import android.media.MediaFormat;
import android.os.Bundle;
import android.os.Parcel;
import android.os.Parcelable;

import java.nio.ByteBuffer;

/** A wrapper to make {@link MediaFormat} parcelable.
 *  Supports following keys:
 *  <ul>
 *  <li>{@link MediaFormat#KEY_MIME}</li>
 *  <li>{@link MediaFormat#KEY_WIDTH}</li>
 *  <li>{@link MediaFormat#KEY_HEIGHT}</li>
 *  <li>{@link MediaFormat#KEY_CHANNEL_COUNT}</li>
 *  <li>{@link MediaFormat#KEY_SAMPLE_RATE}</li>
 *  <li>{@link MediaFormat#KEY_BIT_RATE}</li>
 *  <li>{@link MediaFormat#KEY_BITRATE_MODE}</li>
 *  <li>{@link MediaFormat#KEY_COLOR_FORMAT}</li>
 *  <li>{@link MediaFormat#KEY_FRAME_RATE}</li>
 *  <li>{@link MediaFormat#KEY_I_FRAME_INTERVAL}</li>
 *  <li>"csd-0"</li>
 *  <li>"csd-1"</li>
 *  </ul>
 */
public final class FormatParam implements Parcelable {
    // Keys for codec specific config bits not exposed in {@link MediaFormat}.
    private static final String KEY_CONFIG_0 = "csd-0";
    private static final String KEY_CONFIG_1 = "csd-1";

    private MediaFormat mFormat;

    public MediaFormat asFormat() {
        return mFormat;
    }

    public FormatParam(final MediaFormat format) {
        mFormat = format;
    }

    protected FormatParam(final Parcel in) {
        mFormat = new MediaFormat();
        readFromParcel(in);
    }

    public static final Creator<FormatParam> CREATOR = new Creator<FormatParam>() {
        @Override
        public FormatParam createFromParcel(final Parcel in) {
            return new FormatParam(in);
        }

        @Override
        public FormatParam[] newArray(final int size) {
            return new FormatParam[size];
        }
    };

    @Override
    public int describeContents() {
        return 0;
    }

    public void readFromParcel(final Parcel in) {
        Bundle bundle = in.readBundle();
        fromBundle(bundle);
    }

    private void fromBundle(final Bundle bundle) {
        if (bundle.containsKey(MediaFormat.KEY_MIME)) {
            mFormat.setString(MediaFormat.KEY_MIME,
                    bundle.getString(MediaFormat.KEY_MIME));
        }
        if (bundle.containsKey(MediaFormat.KEY_WIDTH)) {
            mFormat.setInteger(MediaFormat.KEY_WIDTH,
                    bundle.getInt(MediaFormat.KEY_WIDTH));
        }
        if (bundle.containsKey(MediaFormat.KEY_HEIGHT)) {
            mFormat.setInteger(MediaFormat.KEY_HEIGHT,
                    bundle.getInt(MediaFormat.KEY_HEIGHT));
        }
        if (bundle.containsKey(MediaFormat.KEY_CHANNEL_COUNT)) {
            mFormat.setInteger(MediaFormat.KEY_CHANNEL_COUNT,
                    bundle.getInt(MediaFormat.KEY_CHANNEL_COUNT));
        }
        if (bundle.containsKey(MediaFormat.KEY_SAMPLE_RATE)) {
            mFormat.setInteger(MediaFormat.KEY_SAMPLE_RATE,
                    bundle.getInt(MediaFormat.KEY_SAMPLE_RATE));
        }
        if (bundle.containsKey(KEY_CONFIG_0)) {
            mFormat.setByteBuffer(KEY_CONFIG_0,
                    ByteBuffer.wrap(bundle.getByteArray(KEY_CONFIG_0)));
        }
        if (bundle.containsKey(KEY_CONFIG_1)) {
            mFormat.setByteBuffer(KEY_CONFIG_1,
                    ByteBuffer.wrap(bundle.getByteArray((KEY_CONFIG_1))));
        }
        if (bundle.containsKey(MediaFormat.KEY_BIT_RATE)) {
            mFormat.setInteger(MediaFormat.KEY_BIT_RATE,
                    bundle.getInt(MediaFormat.KEY_BIT_RATE));
        }
        if (bundle.containsKey(MediaFormat.KEY_BITRATE_MODE)) {
            mFormat.setInteger(MediaFormat.KEY_BITRATE_MODE,
                    bundle.getInt(MediaFormat.KEY_BITRATE_MODE));
        }
        if (bundle.containsKey(MediaFormat.KEY_COLOR_FORMAT)) {
            mFormat.setInteger(MediaFormat.KEY_COLOR_FORMAT,
                    bundle.getInt(MediaFormat.KEY_COLOR_FORMAT));
        }
        if (bundle.containsKey(MediaFormat.KEY_FRAME_RATE)) {
            mFormat.setInteger(MediaFormat.KEY_FRAME_RATE,
                    bundle.getInt(MediaFormat.KEY_FRAME_RATE));
        }
        if (bundle.containsKey(MediaFormat.KEY_I_FRAME_INTERVAL)) {
            mFormat.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL,
                    bundle.getInt(MediaFormat.KEY_I_FRAME_INTERVAL));
        }
    }

    @Override
    public void writeToParcel(final Parcel dest, final int flags) {
        dest.writeBundle(toBundle());
    }

    private Bundle toBundle() {
        Bundle bundle = new Bundle();
        if (mFormat.containsKey(MediaFormat.KEY_MIME)) {
            bundle.putString(MediaFormat.KEY_MIME, mFormat.getString(MediaFormat.KEY_MIME));
        }
        if (mFormat.containsKey(MediaFormat.KEY_WIDTH)) {
            bundle.putInt(MediaFormat.KEY_WIDTH, mFormat.getInteger(MediaFormat.KEY_WIDTH));
        }
        if (mFormat.containsKey(MediaFormat.KEY_HEIGHT)) {
            bundle.putInt(MediaFormat.KEY_HEIGHT, mFormat.getInteger(MediaFormat.KEY_HEIGHT));
        }
        if (mFormat.containsKey(MediaFormat.KEY_CHANNEL_COUNT)) {
            bundle.putInt(MediaFormat.KEY_CHANNEL_COUNT, mFormat.getInteger(MediaFormat.KEY_CHANNEL_COUNT));
        }
        if (mFormat.containsKey(MediaFormat.KEY_SAMPLE_RATE)) {
            bundle.putInt(MediaFormat.KEY_SAMPLE_RATE, mFormat.getInteger(MediaFormat.KEY_SAMPLE_RATE));
        }
        if (mFormat.containsKey(KEY_CONFIG_0)) {
            ByteBuffer bytes = mFormat.getByteBuffer(KEY_CONFIG_0);
            bundle.putByteArray(KEY_CONFIG_0,
                Sample.byteArrayFromBuffer(bytes, 0, bytes.capacity()));
        }
        if (mFormat.containsKey(KEY_CONFIG_1)) {
            ByteBuffer bytes = mFormat.getByteBuffer(KEY_CONFIG_1);
            bundle.putByteArray(KEY_CONFIG_1,
                Sample.byteArrayFromBuffer(bytes, 0, bytes.capacity()));
        }
        if (mFormat.containsKey(MediaFormat.KEY_BIT_RATE)) {
            bundle.putInt(MediaFormat.KEY_BIT_RATE, mFormat.getInteger(MediaFormat.KEY_BIT_RATE));
        }
        if (mFormat.containsKey(MediaFormat.KEY_BITRATE_MODE)) {
            bundle.putInt(MediaFormat.KEY_BITRATE_MODE, mFormat.getInteger(MediaFormat.KEY_BITRATE_MODE));
        }
        if (mFormat.containsKey(MediaFormat.KEY_COLOR_FORMAT)) {
            bundle.putInt(MediaFormat.KEY_COLOR_FORMAT, mFormat.getInteger(MediaFormat.KEY_COLOR_FORMAT));
        }
        if (mFormat.containsKey(MediaFormat.KEY_FRAME_RATE)) {
            bundle.putInt(MediaFormat.KEY_FRAME_RATE, mFormat.getInteger(MediaFormat.KEY_FRAME_RATE));
        }
        if (mFormat.containsKey(MediaFormat.KEY_I_FRAME_INTERVAL)) {
            bundle.putInt(MediaFormat.KEY_I_FRAME_INTERVAL, mFormat.getInteger(MediaFormat.KEY_I_FRAME_INTERVAL));
        }
        return bundle;
    }
}
