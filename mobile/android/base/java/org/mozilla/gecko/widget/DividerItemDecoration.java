package org.mozilla.gecko.widget;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.support.v4.content.ContextCompat;
import android.support.v7.widget.RecyclerView;
import android.view.View;
import org.mozilla.gecko.R;

public class DividerItemDecoration extends RecyclerView.ItemDecoration {
    private final int mDividerHeight;
    private final Paint mDividerPaint;

    public DividerItemDecoration(Context context) {
        mDividerHeight = (int) context.getResources().getDimension(R.dimen.page_row_divider_height);

        mDividerPaint = new Paint();
        mDividerPaint.setColor(ContextCompat.getColor(context, R.color.toolbar_divider_grey));
        mDividerPaint.setStyle(Paint.Style.FILL_AND_STROKE);
    }

    @Override
    public void getItemOffsets(Rect outRect, View view, RecyclerView parent, RecyclerView.State state) {
        outRect.set(0, 0, 0, mDividerHeight);
    }

    @Override
    public void onDraw(Canvas c, RecyclerView parent, RecyclerView.State state) {
        if (parent.getChildCount() == 0) {
            return;
        }
        for (int i = 0; i < parent.getChildCount(); i++) {
            final View child = parent.getChildAt(i);
            c.drawRect(0, child.getBottom(), parent.getWidth(), child.getBottom() + mDividerHeight, mDividerPaint);
        }
    }
}
