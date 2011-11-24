package org.mozilla.gecko;

import android.graphics.Canvas;
import java.nio.Buffer;

public class SurfaceLockInfo {
    public int dirtyTop;
    public int dirtyLeft;
    public int dirtyRight;
    public int dirtyBottom;

    public int bpr;
    public int format;
    public int width;
    public int height;
    public Buffer buffer;
    public Canvas canvas;
}
