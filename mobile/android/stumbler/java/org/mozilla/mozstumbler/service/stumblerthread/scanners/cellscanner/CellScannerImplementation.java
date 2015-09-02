/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.mozstumbler.service.stumblerthread.scanners.cellscanner;

import android.annotation.TargetApi;
import android.content.Context;
import android.os.Build;
import android.telephony.CellIdentityCdma;
import android.telephony.CellIdentityGsm;
import android.telephony.CellIdentityLte;
import android.telephony.CellIdentityWcdma;
import android.telephony.CellInfoCdma;
import android.telephony.CellInfoGsm;
import android.telephony.CellInfoLte;
import android.telephony.CellInfoWcdma;
import android.telephony.CellLocation;
import android.telephony.CellSignalStrengthCdma;
import android.telephony.CellSignalStrengthGsm;
import android.telephony.CellSignalStrengthLte;
import android.telephony.CellSignalStrengthWcdma;
import android.telephony.NeighboringCellInfo;
import android.telephony.PhoneStateListener;
import android.telephony.SignalStrength;
import android.telephony.TelephonyManager;
import android.util.Log;

import org.mozilla.mozstumbler.service.AppGlobals;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.List;

public class CellScannerImplementation implements CellScanner.CellScannerImpl {

    protected static String LOG_TAG = AppGlobals.makeLogTag(CellScannerImplementation.class.getSimpleName());
    protected GetAllCellInfoScannerImpl mGetAllInfoCellScanner;
    protected TelephonyManager mTelephonyManager;
    protected boolean mIsStarted;
    protected int mPhoneType;
    protected final Context mContext;

    protected volatile int mSignalStrength = CellInfo.UNKNOWN_SIGNAL;
    protected volatile int mCdmaDbm = CellInfo.UNKNOWN_SIGNAL;

    private PhoneStateListener mPhoneStateListener;

    private static class GetAllCellInfoScannerDummy implements GetAllCellInfoScannerImpl {
        @Override
        public List<CellInfo> getAllCellInfo(TelephonyManager tm) {
            return Collections.emptyList();
        }
    }

    interface GetAllCellInfoScannerImpl {
        List<CellInfo> getAllCellInfo(TelephonyManager tm);
    }

    public CellScannerImplementation(Context context) {
        mContext = context;
    }

    public boolean isSupportedOnThisDevice() {
        TelephonyManager telephonyManager = mTelephonyManager;
        if (telephonyManager == null) {
            telephonyManager = (TelephonyManager) mContext.getSystemService(Context.TELEPHONY_SERVICE);
        }
        return telephonyManager != null &&
                (telephonyManager.getPhoneType() == TelephonyManager.PHONE_TYPE_CDMA ||
                 telephonyManager.getPhoneType() == TelephonyManager.PHONE_TYPE_GSM);
    }

    @Override
    public synchronized boolean isStarted() {
        return mIsStarted;
    }

    @Override
    public synchronized void start() {
        if (mIsStarted || !isSupportedOnThisDevice()) {
            return;
        }
        mIsStarted = true;

        if (mTelephonyManager == null) {
            if (Build.VERSION.SDK_INT >= 18 /*Build.VERSION_CODES.JELLY_BEAN_MR2 */) {
                mGetAllInfoCellScanner = new GetAllCellInfoScannerMr2();
            } else {
                mGetAllInfoCellScanner = new GetAllCellInfoScannerDummy();
            }

            mTelephonyManager = (TelephonyManager) mContext.getSystemService(Context.TELEPHONY_SERVICE);
        }

        mPhoneStateListener = new PhoneStateListener() {
            @Override
            public void onSignalStrengthsChanged(SignalStrength ss) {
                if (ss.isGsm()) {
                    mSignalStrength = ss.getGsmSignalStrength();
                } else {
                    mCdmaDbm = ss.getCdmaDbm();
                }
            }
        };
        mTelephonyManager.listen(mPhoneStateListener, PhoneStateListener.LISTEN_SIGNAL_STRENGTHS);
    }

    @Override
    public synchronized void stop() {
        mIsStarted = false;
        if (mTelephonyManager != null && mPhoneStateListener != null) {
            mTelephonyManager.listen(mPhoneStateListener, PhoneStateListener.LISTEN_NONE);
        }
        mSignalStrength = CellInfo.UNKNOWN_SIGNAL;
        mCdmaDbm = CellInfo.UNKNOWN_SIGNAL;
    }

    @Override
    public synchronized List<CellInfo> getCellInfo() {
        List<CellInfo> records = new ArrayList<CellInfo>();

        List<CellInfo> allCells = mGetAllInfoCellScanner.getAllCellInfo(mTelephonyManager);
        if (allCells.isEmpty()) {
            CellInfo currentCell = getCurrentCellInfo();
            if (currentCell == null) {
                return records;
            }
            records.add(currentCell);
        }else {
            records.addAll(allCells);
        }

        // getNeighboringCells() sometimes contains more information than that is already
        // in getAllCellInfo(). Use the results of both of them.
        records.addAll(getNeighboringCells());
        return records;
    }

    private String getNetworkOperator() {
        String networkOperator = mTelephonyManager.getNetworkOperator();
        // getNetworkOperator() may be unreliable on CDMA networks
        if (networkOperator == null || networkOperator.length() <= 3) {
            networkOperator = mTelephonyManager.getSimOperator();
        }
        return networkOperator;
    }

    protected CellInfo getCurrentCellInfo() {
        final CellLocation currentCell = mTelephonyManager.getCellLocation();
        if (currentCell == null) {
            return null;
        }

        try {
            final CellInfo info = new CellInfo(mPhoneType);
            final int signalStrength = mSignalStrength;
            final int cdmaDbm = mCdmaDbm;
            info.setCellLocation(currentCell,
                    mTelephonyManager.getNetworkType(),
                    getNetworkOperator(),
                    signalStrength == CellInfo.UNKNOWN_SIGNAL ? null : signalStrength,
                    cdmaDbm == CellInfo.UNKNOWN_SIGNAL ? null : cdmaDbm);
            return info;
        } catch (IllegalArgumentException iae) {
            Log.e(LOG_TAG, "Skip invalid or incomplete CellLocation: " + currentCell, iae);
        }
        return null;
    }

    private List<CellInfo> getNeighboringCells() {
        // For max fennec compatibility, avoid VERSION_CODES
        if (Build.VERSION.SDK_INT >= 22 /* Build.VERSION_CODES.LOLLIPOP_MR1 */) {
            return Collections.emptyList();
        }

        @SuppressWarnings("deprecation")
        Collection<NeighboringCellInfo> cells = mTelephonyManager.getNeighboringCellInfo();
        if (cells == null || cells.isEmpty()) {
            return Collections.emptyList();
        }

        String networkOperator = getNetworkOperator();
        List<CellInfo> records = new ArrayList<CellInfo>(cells.size());
        for (NeighboringCellInfo nci : cells) {
            try {
                final CellInfo record = new CellInfo(mPhoneType);
                record.setNeighboringCellInfo(nci, networkOperator);
                if (record.isCellRadioValid()) {
                    records.add(record);
                }
            } catch (IllegalArgumentException iae) {
                Log.e(LOG_TAG, "Skip invalid or incomplete NeighboringCellInfo: " + nci, iae);
            }
        }
        return records;
    }


    @TargetApi(18)
    protected boolean addWCDMACellToList(List<CellInfo> cells,
                                         android.telephony.CellInfo observedCell,
                                         TelephonyManager tm) {
        boolean added = false;
        if (Build.VERSION.SDK_INT >= 18 &&
                observedCell instanceof CellInfoWcdma) {
            CellIdentityWcdma ident = ((CellInfoWcdma) observedCell).getCellIdentity();
            if (ident.getMnc() != Integer.MAX_VALUE && ident.getMcc() != Integer.MAX_VALUE) {
                CellInfo cell = new CellInfo(tm.getPhoneType());
                CellSignalStrengthWcdma strength = ((CellInfoWcdma) observedCell).getCellSignalStrength();
                cell.setWcmdaCellInfo(ident.getMcc(),
                        ident.getMnc(),
                        ident.getLac(),
                        ident.getCid(),
                        ident.getPsc(),
                        strength.getAsuLevel());
                cells.add(cell);
                added = true;
            }
        }
        return added;
    }

    @TargetApi(18)
    protected boolean addCellToList(List<CellInfo> cells,
                                    android.telephony.CellInfo observedCell,
                                    TelephonyManager tm) {
        if (tm.getPhoneType() == 0) {
            return false;
        }

        boolean added = false;
        if (observedCell instanceof CellInfoGsm) {
            CellIdentityGsm ident = ((CellInfoGsm) observedCell).getCellIdentity();
            if (ident.getMcc() != Integer.MAX_VALUE && ident.getMnc() != Integer.MAX_VALUE) {
                CellSignalStrengthGsm strength = ((CellInfoGsm) observedCell).getCellSignalStrength();
                CellInfo cell = new CellInfo(tm.getPhoneType());
                cell.setGsmCellInfo(ident.getMcc(),
                        ident.getMnc(),
                        ident.getLac(),
                        ident.getCid(),
                        strength.getAsuLevel());
                cells.add(cell);
                added = true;
            }
        } else if (observedCell instanceof CellInfoCdma) {
            CellInfo cell = new CellInfo(tm.getPhoneType());
            CellIdentityCdma ident = ((CellInfoCdma) observedCell).getCellIdentity();
            CellSignalStrengthCdma strength = ((CellInfoCdma) observedCell).getCellSignalStrength();
            cell.setCdmaCellInfo(ident.getBasestationId(),
                    ident.getNetworkId(),
                    ident.getSystemId(),
                    strength.getDbm());
            cells.add(cell);
            added = true;
        } else if (observedCell instanceof CellInfoLte) {
            CellIdentityLte ident = ((CellInfoLte) observedCell).getCellIdentity();
            if (ident.getMnc() != Integer.MAX_VALUE && ident.getMcc() != Integer.MAX_VALUE) {
                CellInfo cell = new CellInfo(tm.getPhoneType());
                CellSignalStrengthLte strength = ((CellInfoLte) observedCell).getCellSignalStrength();
                cell.setLteCellInfo(ident.getMcc(),
                        ident.getMnc(),
                        ident.getCi(),
                        ident.getPci(),
                        ident.getTac(),
                        strength.getAsuLevel(),
                        strength.getTimingAdvance());
                cells.add(cell);
                added = true;
            }
        }

        if (!added && Build.VERSION.SDK_INT >= 18) {
            added = addWCDMACellToList(cells, observedCell, tm);
        }

        return added;
    }

    @TargetApi(18)
    private class GetAllCellInfoScannerMr2 implements GetAllCellInfoScannerImpl {
        @Override
        public List<CellInfo> getAllCellInfo(TelephonyManager tm) {
            final List<android.telephony.CellInfo> observed = tm.getAllCellInfo();
            if (observed == null || observed.isEmpty()) {
                return Collections.emptyList();
            }

            List<CellInfo> cells = new ArrayList<CellInfo>(observed.size());
            for (android.telephony.CellInfo observedCell : observed) {
                if (!addCellToList(cells, observedCell, tm)) {
                    //Log.i(LOG_TAG, "Skipped CellInfo of unknown class: " + observedCell.toString());
                }
            }
            return cells;
        }
    }
}
