/*This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.overlays.service.sharemethods;

import android.content.Context;
import android.os.Parcel;
import android.os.Parcelable;
import org.mozilla.gecko.overlays.service.ShareData;

/**
 * Represents a method of sharing a URL/title. Add a bookmark? Send to a device? Add to reading list?
 */
public abstract class ShareMethod {
    protected final Context context;

    public ShareMethod(Context aContext) {
        context = aContext;
    }

    /**
     * Perform a share for the given title/URL combination. Called on the background thread by the
     * handler service when a request is made. The "extra" parameter is provided should a ShareMethod
     * desire to handle the share differently based on some additional parameters.
     *
     * @param title The page title for the page being shared. May be null if none can be found.
     * @param url The URL of the page to be shared. Never null.
     * @param extra A Parcelable of ShareMethod-specific parameters that may be provided by the
     *              caller. Generally null, but this field may be used to provide extra input to
     *              the ShareMethod (such as the device to share to in the case of SendTab).
     * @return true if the attempt to share was a success. False in the event of an error.
     */
    public abstract Result handle(ShareData shareData);

    public abstract String getSuccessMessage();
    public abstract String getFailureMessage();

    /**
     * Enum representing the possible results of performing a share.
     */
    public static enum Result {
        // Victory!
        SUCCESS,

        // Failure, but retrying the same action again might lead to success.
        TRANSIENT_FAILURE,

        // Failure, and you're not going to succeed until you reinitialise the ShareMethod (ie.
        // until you repeat the entire share action). Examples include broken Sync accounts, or
        // Sync accounts with no valid target devices (so the only way to fix this is to add some
        // and try again: pushing a retry button isn't sane).
        PERMANENT_FAILURE
    }

    /**
     * Enum representing types of ShareMethod. Parcelable so it may be efficiently used in Intents.
     */
    public static enum Type implements Parcelable {
        ADD_BOOKMARK,
        ADD_TO_READING_LIST,
        SEND_TAB;

        @Override
        public int describeContents() {
            return 0;
        }

        @Override
        public void writeToParcel(final Parcel dest, final int flags) {
            dest.writeInt(ordinal());
        }

        public static final Creator<Type> CREATOR = new Creator<Type>() {
            @Override
            public Type createFromParcel(final Parcel source) {
                return Type.values()[source.readInt()];
            }

            @Override
            public Type[] newArray(final int size) {
                return new Type[size];
            }
        };
    }
}
