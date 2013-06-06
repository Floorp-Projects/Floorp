/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.gfx.Layer;
import org.mozilla.gecko.gfx.LayerView;
import org.mozilla.gecko.util.EventDispatcher;
import org.mozilla.gecko.util.FloatUtils;
import org.mozilla.gecko.util.GeckoEventListener;
import org.mozilla.gecko.util.ThreadUtils;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import android.util.Log;
import android.view.View;

class TextSelection extends Layer implements GeckoEventListener {
    private static final String LOGTAG = "GeckoTextSelection";

    private final TextSelectionHandle mStartHandle;
    private final TextSelectionHandle mMiddleHandle;
    private final TextSelectionHandle mEndHandle;
    private final EventDispatcher mEventDispatcher;

    private float mViewLeft;
    private float mViewTop;
    private float mViewZoom;

    TextSelection(TextSelectionHandle startHandle,
                  TextSelectionHandle middleHandle,
                  TextSelectionHandle endHandle,
                  EventDispatcher eventDispatcher,
                  GeckoApp activity) {
        mStartHandle = startHandle;
        mMiddleHandle = middleHandle;
        mEndHandle = endHandle;
        mEventDispatcher = eventDispatcher;

        // Only register listeners if we have valid start/middle/end handles
        if (mStartHandle == null || mMiddleHandle == null || mEndHandle == null) {
            Log.e(LOGTAG, "Failed to initialize text selection because at least one handle is null");
        } else {
            registerEventListener("TextSelection:ShowHandles");
            registerEventListener("TextSelection:HideHandles");
            registerEventListener("TextSelection:PositionHandles");
        }
    }

    void destroy() {
        unregisterEventListener("TextSelection:ShowHandles");
        unregisterEventListener("TextSelection:HideHandles");
        unregisterEventListener("TextSelection:PositionHandles");
    }

    private TextSelectionHandle getHandle(String name) {
        if (name.equals("START")) {
            return mStartHandle;
        } else if (name.equals("MIDDLE")) {
            return mMiddleHandle;
        } else {
            return mEndHandle;
        }
    }

    @Override
    public void handleMessage(final String event, final JSONObject message) {
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                try {
                    if (event.equals("TextSelection:ShowHandles")) {
                        final JSONArray handles = message.getJSONArray("handles");
                        for (int i=0; i < handles.length(); i++) {
                            String handle = handles.getString(i);
                            getHandle(handle).setVisibility(View.VISIBLE);
                        }

                        mViewLeft = 0.0f;
                        mViewTop = 0.0f;
                        mViewZoom = 0.0f;
                        LayerView layerView = GeckoAppShell.getLayerView();
                        if (layerView != null) {
                            layerView.addLayer(TextSelection.this);
                        }
                    } else if (event.equals("TextSelection:HideHandles")) {
                        LayerView layerView = GeckoAppShell.getLayerView();
                        if (layerView != null) {
                            layerView.removeLayer(TextSelection.this);
                        }

                        mStartHandle.setVisibility(View.GONE);
                        mMiddleHandle.setVisibility(View.GONE);
                        mEndHandle.setVisibility(View.GONE);
                    } else if (event.equals("TextSelection:PositionHandles")) {
                        final boolean rtl = message.getBoolean("rtl");
                        final JSONArray positions = message.getJSONArray("positions");
                        for (int i=0; i < positions.length(); i++) {
                            JSONObject position = positions.getJSONObject(i);
                            int left = position.getInt("left");
                            int top = position.getInt("top");

                            TextSelectionHandle handle = getHandle(position.getString("handle"));
                            handle.setVisibility(position.getBoolean("hidden") ? View.GONE : View.VISIBLE);
                            handle.positionFromGecko(left, top, rtl);
                        }
                    }
                } catch (JSONException e) {
                    Log.e(LOGTAG, "JSON exception", e);
                }
            }
        });
    }

    @Override
    public void draw(final RenderContext context) {
        // cache the relevant values from the context and bail out if they are the same. we do this
        // because this draw function gets called a lot (once per compositor frame) and we want to
        // avoid doing a lot of extra work in cases where it's not needed.
        final float viewLeft = context.viewport.left - context.offset.x;
        final float viewTop = context.viewport.top - context.offset.y;
        final float viewZoom = context.zoomFactor;

        if (FloatUtils.fuzzyEquals(mViewLeft, viewLeft)
                && FloatUtils.fuzzyEquals(mViewTop, viewTop)
                && FloatUtils.fuzzyEquals(mViewZoom, viewZoom)) {
            return;
        }
        mViewLeft = viewLeft;
        mViewTop = viewTop;
        mViewZoom = viewZoom;

        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                mStartHandle.repositionWithViewport(viewLeft, viewTop, viewZoom);
                mMiddleHandle.repositionWithViewport(viewLeft, viewTop, viewZoom);
                mEndHandle.repositionWithViewport(viewLeft, viewTop, viewZoom);
            }
        });
    }

    private void registerEventListener(String event) {
        mEventDispatcher.registerEventListener(event, this);
    }

    private void unregisterEventListener(String event) {
        mEventDispatcher.unregisterEventListener(event, this);
    }
}
