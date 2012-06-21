/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#pragma once

#if (_MSC_VER >= 1400)
#define PRINT_GET_RESULT(...) \
    { \
        _strMsg.Format(__VA_ARGS__); \
        SetDlgItemText(IDC_EDIT_GET_OUTPUT, _strMsg); \
    } \

#define TEST(x, ...) \
    if (!(x)) \
    { \
        _strMsg.Format(__VA_ARGS__); \
        SetDlgItemText(IDC_EDIT_MESSAGE, _strMsg); \
        _strErr.Format(_T("FAILED (error=%d)"), _veBasePtr->LastError()); \
        SetDlgItemText(IDC_EDIT_RESULT, _strErr); \
        _failCount++; \
        SetDlgItemInt(IDC_EDIT_N_FAILS, _failCount); \
        SetDlgItemInt(IDC_EDIT_LAST_ERROR, _veBasePtr->LastError()); \
    } \
    else \
    { \
        _strMsg.Format(__VA_ARGS__); \
        SetDlgItemText(IDC_EDIT_MESSAGE, _strMsg); \
        SetDlgItemText(IDC_EDIT_RESULT, _T("OK")); \
    } \

#define TEST2(x, ...) \
    if (!(x)) \
    { \
        _strMsg.Format(__VA_ARGS__); \
        ((CWinTestDlg*)_parentDialogPtr)->UpdateTest(true, _strMsg); \
    } \
    else \
    { \
        _strMsg.Format(__VA_ARGS__); \
        ((CWinTestDlg*)_parentDialogPtr)->UpdateTest(false, _strMsg); \
    }
#else
#define TEST(x, exp) \
    if (!(x)) \
    { \
        _strMsg.Format(exp); \
        SetDlgItemText(IDC_EDIT_MESSAGE, _strMsg); \
        _strErr.Format("FAILED (error=%d)", _veBasePtr->LastError()); \
        SetDlgItemText(IDC_EDIT_RESULT, _strErr); \
        _failCount++; \
        SetDlgItemInt(IDC_EDIT_N_FAILS, _failCount); \
        SetDlgItemInt(IDC_EDIT_LAST_ERROR, _veBasePtr->LastError()); \
    } \
    else \
    { \
        _strMsg.Format(exp); \
        SetDlgItemText(IDC_EDIT_MESSAGE, _strMsg); \
        SetDlgItemText(IDC_EDIT_RESULT, _T("OK")); \
    } \

#define TEST2(x, exp) \
    if (!(x)) \
    { \
        _strMsg.Format(exp); \
        ((CWinTestDlg*)_parentDialogPtr)->UpdateTest(true, _strMsg); \
    } \
    else \
    { \
        _strMsg.Format(exp); \
        ((CWinTestDlg*)_parentDialogPtr)->UpdateTest(false, _strMsg); \
    }
#endif

#include "voe_base.h"
#include "voe_rtp_rtcp.h"
#include "voe_codec.h"
#include "voe_dtmf.h"
#include "voe_encryption.h"
#include "voe_external_media.h"
#include "voe_file.h"
#include "voe_hardware.h"
#include "voe_network.h"
#include "voe_video_sync.h"
#include "voe_volume_control.h"

#include "voe_audio_processing.h"
#include "voe_rtp_rtcp.h"
#include "voe_errors.h"

class MediaProcessImpl;
class ConnectionObserver;
class MyEncryption;
class RxCallback;
class MyTransport;

using namespace webrtc;

#define MAX_NUM_OF_CHANNELS    10

// CWinTestDlg dialog
class CWinTestDlg : public CDialog,
                    public VoiceEngineObserver,
                    public VoERTPObserver
{
// Construction
public:
    CWinTestDlg(CWnd* pParent = NULL);    // standard constructor
    virtual ~CWinTestDlg();

// Dialog Data
    enum { IDD = IDD_WINTEST_DIALOG };

    BOOL UpdateTest(bool failed, const CString& strMsg);

protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

protected:  // VoiceEngineObserver
    virtual void CallbackOnError(const int channel, const int errCode);

protected:    // VoERTPObserver
    virtual void OnIncomingCSRCChanged(
        const int channel, const unsigned int CSRC, const bool added);
    virtual void OnIncomingSSRCChanged(
        const int channel, const unsigned int SSRC);

// Implementation
protected:
    HICON m_hIcon;

    // Generated message map functions
    virtual BOOL OnInitDialog();
    afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
    afx_msg void OnPaint();
    afx_msg HCURSOR OnQueryDragIcon();
    DECLARE_MESSAGE_MAP()
public:
    afx_msg void OnBnClickedButtonCreate1();
    afx_msg void OnBnClickedButtonDelete1();

private:
    VoiceEngine*    _vePtr;

    VoECodec*               _veCodecPtr;
    VoEExternalMedia*       _veExternalMediaPtr;
    VoEVolumeControl*       _veVolumeControlPtr;
    VoEEncryption*          _veEncryptionPtr;
    VoEHardware*            _veHardwarePtr;
    VoEVideoSync*           _veVideoSyncPtr;
    VoENetwork*             _veNetworkPtr;
    VoEFile*                _veFilePtr;
    VoEAudioProcessing*     _veApmPtr;
    VoEBase*                _veBasePtr;
    VoERTP_RTCP*            _veRtpRtcpPtr;

    MyTransport*            _transportPtr;
    MediaProcessImpl*       _externalMediaPtr;
    ConnectionObserver*     _connectionObserverPtr;
    MyEncryption*           _encryptionPtr;
    RxCallback*             _rxVadObserverPtr;

private:
    int                     _failCount;
    CString                 _strMsg;
    CString                 _strErr;
    bool                    _externalTransport;
    bool                    _externalTransportBuild;
    int                     _checkPlayFileIn;
    int                     _checkPlayFileIn1;
    int                     _checkPlayFileIn2;
    int                     _checkPlayFileOut1;
    int                     _checkPlayFileOut2;
    int                     _checkAGC;
    int                     _checkAGC1;
    int                     _checkNS;
    int                     _checkNS1;
    int                     _checkEC;
    int                     _checkVAD1;
    int                     _checkVAD2;
    int                     _checkSrtpTx1;
    int                     _checkSrtpTx2;
    int                     _checkSrtpRx1;
    int                     _checkSrtpRx2;
    int                     _checkConference1;
    int                     _checkConference2;
    int                     _checkOnHold1;
    int                     _checkOnHold2;
    bool                    _delayEstimate1;
    bool                    _delayEstimate2;
    bool                    _rxVad;
    int                     _nErrorCallbacks;
    int                     _timerTicks;

public:
    afx_msg void OnBnClickedButtonCreate2();
    afx_msg void OnBnClickedButtonDelete2();
    afx_msg void OnCbnSelchangeComboCodec1();
    afx_msg void OnBnClickedButtonStartListen1();
    afx_msg void OnBnClickedButtonStopListen1();
    afx_msg void OnBnClickedButtonStartPlayout1();
    afx_msg void OnBnClickedButtonStopPlayout1();
    afx_msg void OnBnClickedButtonStartSend1();
    afx_msg void OnBnClickedButtonStopSend1();
    afx_msg void OnCbnSelchangeComboIp2();
    afx_msg void OnCbnSelchangeComboIp1();
    afx_msg void OnCbnSelchangeComboCodec2();
    afx_msg void OnBnClickedButtonStartListen2();
    afx_msg void OnBnClickedButtonStopListen2();
    afx_msg void OnBnClickedButtonStartPlayout2();
    afx_msg void OnBnClickedButtonStopPlayout2();
    afx_msg void OnBnClickedButtonStartSend2();
    afx_msg void OnBnClickedButtonStopSend2();
    afx_msg void OnBnClickedButtonTest11();
    afx_msg void OnBnClickedCheckExtTrans1();
    afx_msg void OnBnClickedCheckPlayFileIn1();
    afx_msg void OnBnClickedCheckPlayFileOut1();
    afx_msg void OnBnClickedCheckExtTrans2();
    afx_msg void OnBnClickedCheckPlayFileIn2();
    afx_msg void OnBnClickedCheckPlayFileOut2();
    afx_msg void OnBnClickedCheckPlayFileIn();
    afx_msg void OnBnClickedCheckPlayFileOut();
    afx_msg void OnCbnSelchangeComboRecDevice();
    afx_msg void OnCbnSelchangeComboPlayDevice();
    afx_msg void OnBnClickedCheckExtMediaIn1();
    afx_msg void OnBnClickedCheckExtMediaOut1();
    afx_msg void OnNMReleasedcaptureSliderInputVolume(NMHDR *pNMHDR, LRESULT *pResult);
    afx_msg void OnNMReleasedcaptureSliderOutputVolume(NMHDR *pNMHDR, LRESULT *pResult);
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnBnClickedCheckAgc();
    CString _strComboIp1;
    CString _strComboIp2;
    afx_msg void OnBnClickedCheckNs();
    afx_msg void OnBnClickedCheckEc();
    afx_msg void OnBnClickedCheckVad1();
    afx_msg void OnBnClickedCheckVad2();
    afx_msg void OnBnClickedCheckExtMediaIn2();
    afx_msg void OnBnClickedCheckExtMediaOut2();
    afx_msg void OnBnClickedCheckMuteIn();
    afx_msg void OnBnClickedCheckMuteIn1();
    afx_msg void OnBnClickedCheckMuteIn2();
    afx_msg void OnBnClickedCheckSrtpTx1();
    afx_msg void OnBnClickedCheckSrtpRx1();
    afx_msg void OnBnClickedCheckSrtpTx2();
    afx_msg void OnBnClickedCheckSrtpRx2();
    afx_msg void OnBnClickedCheckExtEncryption1();
    afx_msg void OnBnClickedCheckExtEncryption2();
    afx_msg void OnBnClickedButtonDtmf1();
    afx_msg void OnBnClickedCheckRecMic();
    afx_msg void OnBnClickedButtonDtmf2();
    afx_msg void OnBnClickedButtonTest1();
    afx_msg void OnBnClickedCheckConference1();
    afx_msg void OnBnClickedCheckConference2();
    afx_msg void OnBnClickedCheckOnHold1();
    afx_msg void OnBnClickedCheckOnHold2();
    afx_msg void OnBnClickedCheckExtMediaIn();
    afx_msg void OnBnClickedCheckExtMediaOut();
    afx_msg void OnLbnSelchangeListCodec1();
    afx_msg void OnNMReleasedcaptureSliderPanLeft(NMHDR *pNMHDR, LRESULT *pResult);
    afx_msg void OnNMReleasedcaptureSliderPanRight(NMHDR *pNMHDR, LRESULT *pResult);
    afx_msg void OnBnClickedButtonVersion();
    afx_msg void OnBnClickedCheckDelayEstimate1();
    afx_msg void OnBnClickedCheckRxvad();
    afx_msg void OnBnClickedCheckAgc1();
    afx_msg void OnBnClickedCheckNs1();
    afx_msg void OnBnClickedCheckRecCall();
    afx_msg void OnBnClickedCheckTypingDetection();
    afx_msg void OnBnClickedCheckFEC();
    afx_msg void OnBnClickedButtonClearErrorCallback();
    afx_msg void OnBnClickedCheckBwe1();
};
#pragma once
