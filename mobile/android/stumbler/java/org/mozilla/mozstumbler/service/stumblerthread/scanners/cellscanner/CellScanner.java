/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.mozstumbler.service.stumblerthread.scanners.cellscanner;

import android.content.Context;
import android.content.Intent;
import android.support.v4.content.LocalBroadcastManager;
import android.util.Log;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.Timer;
import java.util.TimerTask;

import org.mozilla.mozstumbler.service.AppGlobals;
import org.mozilla.mozstumbler.service.AppGlobals.ActiveOrPassiveStumbling;


public class CellScanner {
    public static final String ACTION_BASE = AppGlobals.ACTION_NAMESPACE + ".CellScanner.";
    public static final String ACTION_CELLS_SCANNED = ACTION_BASE + "CELLS_SCANNED";
    public static final String ACTION_CELLS_SCANNED_ARG_CELLS = "cells";
    public static final String ACTION_CELLS_SCANNED_ARG_TIME = AppGlobals.ACTION_ARG_TIME;

    private static final String LOG_TAG = AppGlobals.makeLogTag(CellScanner.class.getSimpleName());
    private static final long CELL_MIN_UPDATE_TIME = 1000; // milliseconds

    private final Context mContext;
    private static CellScannerImpl sImpl;
    private Timer mCellScanTimer;
    private final Set<String> mCells = new HashSet<String>();
    private int mCurrentCellInfoCount;

    public ArrayList<CellInfo> sTestingModeCellInfoArray;

    public interface CellScannerImpl {
        public void start();

        public void stop();

        public List<CellInfo> getCellInfo();
    }

    public CellScanner(Context context) {
        mContext = context;
    }

    private static synchronized CellScannerImpl getImplementation() {
        return sImpl;
    }

    public static synchronized boolean isCellScannerImplSet() {
        return sImpl != null;
    }

    /* Fennec doesn't support the apis needed for full scanning, we have different implementations.*/
    public static synchronized void setCellScannerImpl(CellScannerImpl cellScanner) {
        sImpl = cellScanner;
    }

    public void start(final ActiveOrPassiveStumbling stumblingMode) {
        if (getImplementation() == null) {
            return;
        }

        try {
            getImplementation().start();
        } catch (UnsupportedOperationException uoe) {
            Log.e(LOG_TAG, "Cell scanner probe failed", uoe);
            return;
        }

        if (mCellScanTimer != null) {
            return;
        }

        mCellScanTimer = new Timer();

        mCellScanTimer.schedule(new TimerTask() {
            int mPassiveScanCount;
            @Override
            public void run() {
                if (getImplementation() == null) {
                    return;
                }

                if (stumblingMode == ActiveOrPassiveStumbling.PASSIVE_STUMBLING &&
                        mPassiveScanCount++ > AppGlobals.PASSIVE_MODE_MAX_SCANS_PER_GPS)
                {
                    mPassiveScanCount = 0;
                    stop();
                    return;
                }
                //if (SharedConstants.isDebug) Log.d(LOG_TAG, "Cell Scanning Timer fired");
                final long curTime = System.currentTimeMillis();

                ArrayList<CellInfo> cells = (sTestingModeCellInfoArray != null)? sTestingModeCellInfoArray :
                        new ArrayList<CellInfo>(getImplementation().getCellInfo());

                mCurrentCellInfoCount = cells.size();
                if (cells.isEmpty()) {
                    return;
                }
                for (CellInfo cell: cells) mCells.add(cell.getCellIdentity());

                Intent intent = new Intent(ACTION_CELLS_SCANNED);
                intent.putParcelableArrayListExtra(ACTION_CELLS_SCANNED_ARG_CELLS, cells);
                intent.putExtra(ACTION_CELLS_SCANNED_ARG_TIME, curTime);
                LocalBroadcastManager.getInstance(mContext).sendBroadcastSync(intent);
            }
        }, 0, CELL_MIN_UPDATE_TIME);
    }

    public void stop() {
        if (mCellScanTimer != null) {
            mCellScanTimer.cancel();
            mCellScanTimer = null;
        }
        if (getImplementation() != null) {
            getImplementation().stop();
        }
    }

    public int getCellInfoCount() {
        return mCells.size();
    }

    public int getCurrentCellInfoCount() {
        return mCurrentCellInfoCount;
    }
}
