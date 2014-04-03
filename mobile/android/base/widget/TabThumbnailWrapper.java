package org.mozilla.gecko.widget;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.FrameLayout;
import org.mozilla.gecko.R;


public class TabThumbnailWrapper extends FrameLayout {
    private boolean mRecording = false;
    private static final int[] STATE_RECORDING = { R.attr.state_recording };

    public TabThumbnailWrapper(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
    }

    public TabThumbnailWrapper(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public TabThumbnailWrapper(Context context) {
        super(context);
    }

    @Override
    public int[] onCreateDrawableState(int extraSpace) {
        final int[] drawableState = super.onCreateDrawableState(extraSpace + 1);

        if (mRecording) {
            mergeDrawableStates(drawableState, STATE_RECORDING);
	}
        return drawableState;
    }

    public void setRecording(boolean recording) {
        if (mRecording != recording) {
            mRecording = recording;
            refreshDrawableState();
        }
    }

}
