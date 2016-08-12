package org.mozilla.gecko.home.activitystream;

import android.content.Context;
import android.support.annotation.Nullable;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.util.AttributeSet;
import android.widget.LinearLayout;

import org.mozilla.gecko.R;

public class MainRecyclerLayout extends LinearLayout {
    public MainRecyclerLayout(Context context) {
        super(context);
        RecyclerView rv = (RecyclerView) findViewById(R.id.activity_stream_main_recyclerview);
        rv.setAdapter(new MainRecyclerAdapter(context));
        rv.setLayoutManager(new LinearLayoutManager(context));
    }

    public MainRecyclerLayout(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    public MainRecyclerLayout(Context context, @Nullable AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
    }
}
