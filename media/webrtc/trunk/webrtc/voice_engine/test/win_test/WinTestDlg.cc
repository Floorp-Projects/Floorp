/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <math.h>
#include "stdafx.h"
#include "WinTest.h"
#include "WinTestDlg.h"
#include "testsupport/fileutils.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

using namespace webrtc;

unsigned char key[30] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

// Hack to convert char to TCHAR, using two buffers to be able to
// call twice in the same statement
TCHAR convertTemp1[256] = {0};
TCHAR convertTemp2[256] = {0};
bool convertBufferSwitch(false);
TCHAR* CharToTchar(const char* str, int len)
{
#ifdef _UNICODE
  TCHAR* temp = convertBufferSwitch ? convertTemp1 : convertTemp2;
  convertBufferSwitch = !convertBufferSwitch;
  memset(temp, 0, sizeof(convertTemp1));
  MultiByteToWideChar(CP_UTF8, 0, str, len, temp, 256);
  return temp;
#else
  return str;
#endif
}

// Hack to convert TCHAR to char
char convertTemp3[256] = {0};
char* TcharToChar(TCHAR* str, int len)
{
#ifdef _UNICODE
  memset(convertTemp3, 0, sizeof(convertTemp3));
  WideCharToMultiByte(CP_UTF8, 0, str, len, convertTemp3, 256, 0, 0);
  return convertTemp3;
#else
  return str;
#endif
}

// ----------------------------------------------------------------------------
//    VoEConnectionObserver
// ----------------------------------------------------------------------------

class ConnectionObserver : public  VoEConnectionObserver
{
public:
    ConnectionObserver();
    virtual void OnPeriodicDeadOrAlive(const int channel, const bool alive);
};

ConnectionObserver::ConnectionObserver()
{
}

void ConnectionObserver::OnPeriodicDeadOrAlive(const int channel, const bool alive)
{
    CString str;
    str.Format(_T("OnPeriodicDeadOrAlive(channel=%d) => alive=%d"), channel, alive);
    OutputDebugString(str);
}

// ----------------------------------------------------------------------------
//    VoiceEngineObserver
// ----------------------------------------------------------------------------

void CWinTestDlg::CallbackOnError(const int channel, const int errCode)
{
    _nErrorCallbacks++;

    CString str;
    str.Format(_T("[#%d] CallbackOnError(channel=%d) => errCode = %d"), _nErrorCallbacks, channel, errCode);
    if (errCode == VE_RECEIVE_PACKET_TIMEOUT)
    {
        str += _T(" <=> VE_RECEIVE_PACKET_TIMEOUT");
    }
    else if (errCode == VE_PACKET_RECEIPT_RESTARTED)
    {
        str += _T(" <=> VE_PACKET_RECEIPT_RESTARTED");
    }
    else if (errCode == VE_RUNTIME_PLAY_WARNING)
    {
        str += _T(" <=> VE_RUNTIME_PLAY_WARNING");
    }
    else if (errCode == VE_RUNTIME_REC_WARNING)
    {
        str += _T(" <=> VE_RUNTIME_REC_WARNING");
    }
    else if (errCode == VE_RUNTIME_PLAY_ERROR)
    {
        str += _T(" <=> VE_RUNTIME_PLAY_ERROR");
    }
    else if (errCode == VE_RUNTIME_REC_ERROR)
    {
        str += _T(" <=> VE_RUNTIME_REC_ERROR");
    }
    else if (errCode == VE_SATURATION_WARNING)
    {
        str += _T(" <=> VE_SATURATION_WARNING");
    }
    else if (errCode == VE_TYPING_NOISE_WARNING)
    {
        str += _T(" <=> VE_TYPING_NOISE_WARNING");
    }
    else if (errCode == VE_REC_DEVICE_REMOVED)
    {
        str += _T(" <=> VE_REC_DEVICE_REMOVED");
    }
    // AfxMessageBox((LPCTSTR)str, MB_OK);
    SetDlgItemText(IDC_EDIT_ERROR_CALLBACK, (LPCTSTR)str);
}

// ----------------------------------------------------------------------------
//    VoERTPObserver
// ----------------------------------------------------------------------------

void CWinTestDlg::OnIncomingCSRCChanged(const int channel, const unsigned int CSRC, const bool added)
{
    CString str;
    str.Format(_T("OnIncomingCSRCChanged(channel=%d) => CSRC=%u, added=%d"), channel, CSRC, added);
    SetDlgItemText(IDC_EDIT_ERROR_CALLBACK, (LPCTSTR)str);
}

void CWinTestDlg::OnIncomingSSRCChanged(const int channel, const unsigned int SSRC)
{
    CString str;
    str.Format(_T("OnIncomingSSRCChanged(channel=%d) => SSRC=%u"), channel, SSRC);
    SetDlgItemText(IDC_EDIT_ERROR_CALLBACK, (LPCTSTR)str);
}

// ----------------------------------------------------------------------------
//    Transport
// ----------------------------------------------------------------------------

class MyTransport : public Transport
{
public:
    MyTransport(VoENetwork* veNetwork);
    virtual int SendPacket(int channel, const void *data, int len);
    virtual int SendRTCPPacket(int channel, const void *data, int len);
private:
    VoENetwork* _veNetworkPtr;
};

MyTransport::MyTransport(VoENetwork* veNetwork) :
    _veNetworkPtr(veNetwork)
{
}

int
MyTransport::SendPacket(int channel, const void *data, int len)
{
    _veNetworkPtr->ReceivedRTPPacket(channel, data, len);
    return len;
}

int
MyTransport::SendRTCPPacket(int channel, const void *data, int len)
{
    _veNetworkPtr->ReceivedRTCPPacket(channel, data, len);
    return len;
}

// ----------------------------------------------------------------------------
//    VoEMediaProcess
// ----------------------------------------------------------------------------

class MediaProcessImpl : public VoEMediaProcess
{
public:
    MediaProcessImpl();
    virtual void Process(const int channel,
                         const ProcessingTypes type,
                         int16_t audio_10ms[],
                         const int length,
                         const int samplingFreqHz,
                         const bool stereo);
};

MediaProcessImpl::MediaProcessImpl()
{
}

void MediaProcessImpl::Process(const int channel,
                               const ProcessingTypes type,
                               int16_t audio_10ms[],
                               const int length,
                               const int samplingFreqHz,
                               const bool stereo)
{
    int x = rand() % 100;

    for (int i = 0; i < length; i++)
    {
        if (channel == -1)
        {
            if (type == kPlaybackAllChannelsMixed)
            {
                // playout: scale up
                if (!stereo)
                {
                    audio_10ms[i] = (audio_10ms[i] << 2);
                }
                else
                {
                    audio_10ms[2*i] = (audio_10ms[2*i] << 2);
                    audio_10ms[2*i+1] = (audio_10ms[2*i+1] << 2);
                }
            }
            else
            {
                // recording: emulate packet loss by "dropping" 10% of the packets
                if (x >= 0 && x < 10)
                {
                    if (!stereo)
                    {
                        audio_10ms[i] = 0;
                    }
                    else
                    {
                        audio_10ms[2*i] = 0;
                        audio_10ms[2*i+1] = 0;
                    }
                }
            }
        }
        else
        {
            if (type == kPlaybackPerChannel)
            {
                // playout: mute
                if (!stereo)
                {
                    audio_10ms[i] = 0;
                }
                else
                {
                    audio_10ms[2*i] = 0;
                    audio_10ms[2*i+1] = 0;
                }
            }
            else
            {
                // recording: emulate packet loss by "dropping" 50% of the packets
                if (x >= 0 && x < 50)
                {
                    if (!stereo)
                    {
                        audio_10ms[i] = 0;
                    }
                    else
                    {
                        audio_10ms[2*i] = 0;
                        audio_10ms[2*i+1] = 0;
                    }
                }
            }
        }
    }
}

// ----------------------------------------------------------------------------
//    Encryptionen
// ----------------------------------------------------------------------------

class MyEncryption : public Encryption
{
public:
    void encrypt(int channel_no, unsigned char * in_data, unsigned char * out_data, int bytes_in, int* bytes_out);
    void decrypt(int channel_no, unsigned char * in_data, unsigned char * out_data, int bytes_in, int* bytes_out);
    void encrypt_rtcp(int channel_no, unsigned char * in_data, unsigned char * out_data, int bytes_in, int* bytes_out);
    void decrypt_rtcp(int channel_no, unsigned char * in_data, unsigned char * out_data, int bytes_in, int* bytes_out);
};

void MyEncryption::encrypt(int channel_no, unsigned char * in_data, unsigned char * out_data, int bytes_in, int* bytes_out)
{
    // --- Stereo emulation (sample based, 2 bytes per sample)

    const int nBytesPayload = bytes_in-12;

    // RTP header (first 12 bytes)
    memcpy(out_data, in_data, 12);

    // skip RTP header
    short* ptrIn = (short*) &in_data[12];
    short* ptrOut = (short*) &out_data[12];

    // network byte order
    for (int i = 0; i < nBytesPayload/2; i++)
    {
        // produce two output samples for each input sample
        *ptrOut++ = *ptrIn; // left sample
        *ptrOut++ = *ptrIn; // right sample
        ptrIn++;
    }

    *bytes_out = 12 + 2*nBytesPayload;

    /*
    for(int i = 0; i < bytes_in; i++)
        out_data[i] =~ in_data[i];
    *bytes_out = bytes_in;
    */
}

void MyEncryption::decrypt(int channel_no, unsigned char * in_data, unsigned char * out_data, int bytes_in, int* bytes_out)
{
    // Do nothing (<=> memcpy)
    for(int i = 0; i < bytes_in; i++)
        out_data[i] = in_data[i];
    *bytes_out = bytes_in;
}

void MyEncryption::encrypt_rtcp(int channel_no, unsigned char * in_data, unsigned char * out_data, int bytes_in, int* bytes_out)
{
    for(int i = 0; i < bytes_in; i++)
        out_data[i] =~ in_data[i];
    *bytes_out = bytes_in;
}

void MyEncryption::decrypt_rtcp(int channel_no, unsigned char * in_data, unsigned char * out_data, int bytes_in, int* bytes_out)
{
    for(int i = 0; i < bytes_in; i++)
        out_data[i] =~ in_data[i];
    *bytes_out = bytes_in;
}

// ----------------------------------------------------------------------------
//    TelephoneEventObserver
// ----------------------------------------------------------------------------

class TelephoneEventObserver: public VoETelephoneEventObserver
{
public:
    TelephoneEventObserver(CWnd* editControlOut, CWnd* editControlIn);
    virtual void OnReceivedTelephoneEventInband(int channel, int eventCode,
                                                bool endOfEvent);
    virtual void OnReceivedTelephoneEventOutOfBand(int channel, int eventCode,
                                                   bool endOfEvent);
private:
    CWnd* _editControlOutPtr;
    CWnd* _editControlInPtr;
};

TelephoneEventObserver::TelephoneEventObserver(CWnd* editControlOut, CWnd* editControlIn) :
    _editControlOutPtr(editControlOut),
    _editControlInPtr(editControlIn)
{
}

void TelephoneEventObserver::OnReceivedTelephoneEventInband(int channel,
                                                            int eventCode,
                                                            bool endOfEvent)
{
    CString msg;
    if (endOfEvent)
    {
        msg.AppendFormat(_T("%d [END]"), eventCode);
        _editControlInPtr->SetWindowText((LPCTSTR)msg);
    }
    else
    {
        msg.AppendFormat(_T("%d [START]"), eventCode);
        _editControlInPtr->SetWindowText((LPCTSTR)msg);
    }
}

void TelephoneEventObserver::OnReceivedTelephoneEventOutOfBand(int channel,
                                                               int eventCode,
                                                               bool endOfEvent)
{
    CString msg;
    if (endOfEvent)
    {
        msg.AppendFormat(_T("%d [END]"), eventCode);
        _editControlOutPtr->SetWindowText((LPCTSTR)msg);
    }
    else
    {
        msg.AppendFormat(_T("%d [START]"), eventCode);
        _editControlOutPtr->SetWindowText((LPCTSTR)msg);
    }
}

// ----------------------------------------------------------------------------
//    RxVadCallback
// ----------------------------------------------------------------------------

class RxCallback : public VoERxVadCallback
{
public:
    RxCallback() : vad_decision(-1) {};

    virtual void OnRxVad(int , int vadDecision)
    {
        vad_decision = vadDecision;
    }

    int vad_decision;
};

// ----------------------------------------------------------------------------
//                                 CAboutDlg dialog
// ----------------------------------------------------------------------------

class CAboutDlg : public CDialog
{
public:
    CAboutDlg();

// Dialog Data
    enum { IDD = IDD_ABOUTBOX };

    protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
protected:
    DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
END_MESSAGE_MAP()

// ----------------------------------------------------------------------------
//                               CTelephonyEvent dialog
// ----------------------------------------------------------------------------

class CTelephonyEvent : public CDialog
{
    DECLARE_DYNAMIC(CTelephonyEvent)

public:
    CTelephonyEvent(VoiceEngine* voiceEngine, int channel, CDialog* pParentDialog, CWnd* pParent = NULL);   // standard constructor
    virtual ~CTelephonyEvent();

// Dialog Data
    enum { IDD = IDD_DTMF_DIALOG };

protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
    virtual BOOL OnInitDialog();

    DECLARE_MESSAGE_MAP()
public:
    afx_msg void OnBnClickedButton1();
    afx_msg void OnBnClickedButton2();
    afx_msg void OnBnClickedButton3();
    afx_msg void OnBnClickedButton4();
    afx_msg void OnBnClickedButton5();
    afx_msg void OnBnClickedButton6();
    afx_msg void OnBnClickedButton7();
    afx_msg void OnBnClickedButton8();
    afx_msg void OnBnClickedButton9();
    afx_msg void OnBnClickedButton10();
    afx_msg void OnBnClickedButton11();
    afx_msg void OnBnClickedButton12();
    afx_msg void OnBnClickedButtonA();
    afx_msg void OnBnClickedButtonB();
    afx_msg void OnBnClickedButtonC();
    afx_msg void OnBnClickedButtonD();
    afx_msg void OnBnClickedCheckDtmfPlayoutRx();
    afx_msg void OnBnClickedCheckDtmfPlayTone();
    afx_msg void OnBnClickedCheckStartStopMode();
    afx_msg void OnBnClickedCheckEventInband();
    afx_msg void OnBnClickedCheckDtmfFeedback();
    afx_msg void OnBnClickedCheckDirectFeedback();
    afx_msg void OnBnClickedRadioSingle();
    afx_msg void OnBnClickedRadioMulti();
    afx_msg void OnBnClickedRadioStartStop();
    afx_msg void OnBnClickedButtonSetRxTelephonePt();
    afx_msg void OnBnClickedButtonSetTxTelephonePt();
    afx_msg void OnBnClickedButtonSendTelephoneEvent();
    afx_msg void OnBnClickedCheckDetectInband();
    afx_msg void OnBnClickedCheckDetectOutOfBand();
    afx_msg void OnBnClickedCheckEventDetection();

private:
    void SendTelephoneEvent(unsigned char eventCode);

private:
    VoiceEngine*                _vePtr;
    VoEBase*                    _veBasePtr;
    VoEDtmf*                    _veDTMFPtr;
    VoECodec*                   _veCodecPtr;
    int                         _channel;
    CString                     _strMsg;
    CDialog*                    _parentDialogPtr;
    TelephoneEventObserver*     _telephoneEventObserverPtr;
    bool                        _PlayDtmfToneLocally;
    bool                        _modeStartStop;
    bool                        _modeSingle;
    bool                        _modeSequence;
    bool                        _playingDTMFTone;
    bool                        _outOfBandEventDetection;
    bool                        _inbandEventDetection;
};

IMPLEMENT_DYNAMIC(CTelephonyEvent, CDialog)

CTelephonyEvent::CTelephonyEvent(VoiceEngine* voiceEngine,
                                 int channel,
                                 CDialog* pParentDialog,
                                 CWnd* pParent /*=NULL*/)
    : _vePtr(voiceEngine),
      _channel(channel),
      _PlayDtmfToneLocally(false),
      _modeStartStop(false),
      _modeSingle(true),
      _modeSequence(false),
      _playingDTMFTone(false),
      _outOfBandEventDetection(true),
      _inbandEventDetection(false),
      _parentDialogPtr(pParentDialog),
      _telephoneEventObserverPtr(NULL),
      CDialog(CTelephonyEvent::IDD, pParent)
{
    _veBasePtr = VoEBase::GetInterface(_vePtr);
    _veDTMFPtr = VoEDtmf::GetInterface(_vePtr);
    _veCodecPtr = VoECodec::GetInterface(_vePtr);
}

CTelephonyEvent::~CTelephonyEvent()
{
    _veDTMFPtr->Release();
    _veCodecPtr->Release();
    _veBasePtr->Release();

    if (_telephoneEventObserverPtr)
    {
        _veDTMFPtr->DeRegisterTelephoneEventDetection(_channel);
        delete _telephoneEventObserverPtr;
        _telephoneEventObserverPtr = NULL;
    }
}

void CTelephonyEvent::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CTelephonyEvent, CDialog)
    ON_BN_CLICKED(IDC_BUTTON_1, &CTelephonyEvent::OnBnClickedButton1)
    ON_BN_CLICKED(IDC_BUTTON_2, &CTelephonyEvent::OnBnClickedButton2)
    ON_BN_CLICKED(IDC_BUTTON_3, &CTelephonyEvent::OnBnClickedButton3)
    ON_BN_CLICKED(IDC_BUTTON_4, &CTelephonyEvent::OnBnClickedButton4)
    ON_BN_CLICKED(IDC_BUTTON_5, &CTelephonyEvent::OnBnClickedButton5)
    ON_BN_CLICKED(IDC_BUTTON_6, &CTelephonyEvent::OnBnClickedButton6)
    ON_BN_CLICKED(IDC_BUTTON_7, &CTelephonyEvent::OnBnClickedButton7)
    ON_BN_CLICKED(IDC_BUTTON_8, &CTelephonyEvent::OnBnClickedButton8)
    ON_BN_CLICKED(IDC_BUTTON_9, &CTelephonyEvent::OnBnClickedButton9)
    ON_BN_CLICKED(IDC_BUTTON_10, &CTelephonyEvent::OnBnClickedButton10)
    ON_BN_CLICKED(IDC_BUTTON_11, &CTelephonyEvent::OnBnClickedButton11)
    ON_BN_CLICKED(IDC_BUTTON_12, &CTelephonyEvent::OnBnClickedButton12)
    ON_BN_CLICKED(IDC_BUTTON_13, &CTelephonyEvent::OnBnClickedButtonA)
    ON_BN_CLICKED(IDC_BUTTON_14, &CTelephonyEvent::OnBnClickedButtonB)
    ON_BN_CLICKED(IDC_BUTTON_15, &CTelephonyEvent::OnBnClickedButtonC)
    ON_BN_CLICKED(IDC_BUTTON_16, &CTelephonyEvent::OnBnClickedButtonD)
    ON_BN_CLICKED(IDC_CHECK_DTMF_PLAYOUT_RX, &CTelephonyEvent::OnBnClickedCheckDtmfPlayoutRx)
    ON_BN_CLICKED(IDC_CHECK_DTMF_PLAY_TONE, &CTelephonyEvent::OnBnClickedCheckDtmfPlayTone)
    ON_BN_CLICKED(IDC_CHECK_EVENT_INBAND, &CTelephonyEvent::OnBnClickedCheckEventInband)
    ON_BN_CLICKED(IDC_CHECK_DTMF_FEEDBACK, &CTelephonyEvent::OnBnClickedCheckDtmfFeedback)
    ON_BN_CLICKED(IDC_CHECK_DIRECT_FEEDBACK, &CTelephonyEvent::OnBnClickedCheckDirectFeedback)
    ON_BN_CLICKED(IDC_RADIO_SINGLE, &CTelephonyEvent::OnBnClickedRadioSingle)
    ON_BN_CLICKED(IDC_RADIO_MULTI, &CTelephonyEvent::OnBnClickedRadioMulti)
    ON_BN_CLICKED(IDC_RADIO_START_STOP, &CTelephonyEvent::OnBnClickedRadioStartStop)
    ON_BN_CLICKED(IDC_BUTTON_SET_RX_TELEPHONE_PT, &CTelephonyEvent::OnBnClickedButtonSetRxTelephonePt)
    ON_BN_CLICKED(IDC_BUTTON_SET_TX_TELEPHONE_PT, &CTelephonyEvent::OnBnClickedButtonSetTxTelephonePt)
    ON_BN_CLICKED(IDC_BUTTON_SEND_TELEPHONE_EVENT, &CTelephonyEvent::OnBnClickedButtonSendTelephoneEvent)
    ON_BN_CLICKED(IDC_CHECK_DETECT_INBAND, &CTelephonyEvent::OnBnClickedCheckDetectInband)
    ON_BN_CLICKED(IDC_CHECK_DETECT_OUT_OF_BAND, &CTelephonyEvent::OnBnClickedCheckDetectOutOfBand)
    ON_BN_CLICKED(IDC_CHECK_EVENT_DETECTION, &CTelephonyEvent::OnBnClickedCheckEventDetection)
END_MESSAGE_MAP()


// CTelephonyEvent message handlers

BOOL CTelephonyEvent::OnInitDialog()
{
    CDialog::OnInitDialog();

    CString str;
    GetWindowText(str);
    str.AppendFormat(_T(" [channel = %d]"), _channel);
    SetWindowText(str);

    // Update dialog with latest playout state
    bool enabled(false);
    _veDTMFPtr->GetDtmfPlayoutStatus(_channel, enabled);
    CButton* button = (CButton*)GetDlgItem(IDC_CHECK_DTMF_PLAYOUT_RX);
    button->SetCheck(enabled ? BST_CHECKED : BST_UNCHECKED);

    // Update dialog with latest feedback state
    bool directFeedback(false);
    _veDTMFPtr->GetDtmfFeedbackStatus(enabled, directFeedback);
    button = (CButton*)GetDlgItem(IDC_CHECK_DTMF_FEEDBACK);
    button->SetCheck(enabled ? BST_CHECKED : BST_UNCHECKED);
    button = (CButton*)GetDlgItem(IDC_CHECK_DIRECT_FEEDBACK);
    button->SetCheck(directFeedback ? BST_CHECKED : BST_UNCHECKED);

    // Default event length is 160 ms
    SetDlgItemInt(IDC_EDIT_EVENT_LENGTH, 160);

    // Default event attenuation is 10 (<-> -10dBm0)
    SetDlgItemInt(IDC_EDIT_EVENT_ATTENUATION, 10);

    // Current event-detection status
    TelephoneEventDetectionMethods detectionMethod(kOutOfBand);
    if (_veDTMFPtr->GetTelephoneEventDetectionStatus(_channel, enabled, detectionMethod) == 0)
    {
        // DTMF detection is supported
        if (enabled)
        {
            button = (CButton*)GetDlgItem(IDC_CHECK_EVENT_DETECTION);
            button->SetCheck(BST_CHECKED);
        }
        if (detectionMethod == kOutOfBand || detectionMethod == kInAndOutOfBand)
        {
            button = (CButton*)GetDlgItem(IDC_CHECK_DETECT_OUT_OF_BAND);
            button->SetCheck(BST_CHECKED);
        }
        if (detectionMethod == kInBand || detectionMethod == kInAndOutOfBand)
        {
            button = (CButton*)GetDlgItem(IDC_CHECK_DETECT_INBAND);
            button->SetCheck(BST_CHECKED);
        }
    }
    else
    {
        // DTMF detection is not supported
        GetDlgItem(IDC_CHECK_EVENT_DETECTION)->EnableWindow(FALSE);
        GetDlgItem(IDC_CHECK_DETECT_OUT_OF_BAND)->EnableWindow(FALSE);
        GetDlgItem(IDC_CHECK_DETECT_INBAND)->EnableWindow(FALSE);
        GetDlgItem(IDC_EDIT_ON_EVENT_INBAND)->EnableWindow(FALSE);
        GetDlgItem(IDC_EDIT_ON_EVENT_OUT_OF_BAND)->EnableWindow(FALSE);
    }

    // Telephone-event PTs
    unsigned char pt(0);
    _veDTMFPtr->GetSendTelephoneEventPayloadType(_channel, pt);
    SetDlgItemInt(IDC_EDIT_EVENT_TX_PT, pt);

    CodecInst codec;
    strcpy_s(codec.plname, 32, "telephone-event"); codec.channels = 1; codec.plfreq = 8000;
    _veCodecPtr->GetRecPayloadType(_channel, codec);
    SetDlgItemInt(IDC_EDIT_EVENT_RX_PT, codec.pltype);

    if (_modeSingle)
    {
        ((CButton*)GetDlgItem(IDC_RADIO_SINGLE))->SetCheck(BST_CHECKED);
    }
    else if (_modeStartStop)
    {
        ((CButton*)GetDlgItem(IDC_RADIO_START_STOP))->SetCheck(BST_CHECKED);
    }
    else if (_modeSequence)
    {
        ((CButton*)GetDlgItem(IDC_RADIO_MULTI))->SetCheck(BST_CHECKED);
    }

    return TRUE;  // return TRUE  unless you set the focus to a control
}
void CTelephonyEvent::SendTelephoneEvent(unsigned char eventCode)
{
    BOOL ret;
    int lengthMs(0);
    int attenuationDb(0);
    bool outBand(false);
    int res(0);

    // tone length
    if (!_modeStartStop)
    {
        lengthMs = GetDlgItemInt(IDC_EDIT_EVENT_LENGTH, &ret);
        if (ret == FALSE)
        {
            // use default length if edit field is empty
            lengthMs = 160;
        }
    }

    // attenuation
    attenuationDb = GetDlgItemInt(IDC_EDIT_EVENT_ATTENUATION, &ret);
    if (ret == FALSE)
    {
        // use default length if edit field is empty
        attenuationDb = 10;
    }

    // out-band or in-band
    CButton* button = (CButton*)GetDlgItem(IDC_CHECK_EVENT_INBAND);
    int check = button->GetCheck();
    outBand = (check == BST_UNCHECKED);

    if (eventCode < 16)
        SetDlgItemInt(IDC_EDIT_DTMF_EVENT, eventCode);

    if (_PlayDtmfToneLocally)
    {
        // --- PlayDtmfTone

        if (_modeSingle)
        {
            TEST2(_veDTMFPtr->PlayDtmfTone(eventCode, lengthMs, attenuationDb) == 0,
                _T("PlayDtmfTone(eventCode=%u, lengthMs=%d, attenuationDb=%d)"), eventCode, lengthMs, attenuationDb);
        }
        else if (_modeStartStop)
        {
            if (!_playingDTMFTone)
            {
                TEST2((res = _veDTMFPtr->StartPlayingDtmfTone(eventCode, attenuationDb)) == 0,
                    _T("StartPlayingDtmfTone(eventCode=%u, attenuationDb=%d)"), eventCode, attenuationDb);
            }
            else
            {
                TEST2((res = _veDTMFPtr->StopPlayingDtmfTone()) == 0,
                    _T("StopPlayingDTMFTone()"));
            }
            if (res == 0)
                _playingDTMFTone = !_playingDTMFTone;
        }
        else if (_modeSequence)
        {
            int nTones(1);
            int sleepMs(0);
            int lenMult(1);
            if (eventCode == 1)
            {
                nTones = 2;
                sleepMs = lengthMs;
                lenMult = 1;
            }
            else if (eventCode == 2)
            {
                nTones = 2;
                sleepMs = lengthMs/2;
                lenMult = 2;
            }
            else if (eventCode == 3)
            {
                nTones = 3;
                sleepMs = 0;
                lenMult = 1;
            }
            for (int i = 0; i < nTones; i++)
            {
                TEST2(_veDTMFPtr->PlayDtmfTone(eventCode, lengthMs, attenuationDb) == 0,
                    _T("PlayDtmfTone(eventCode=%u, outBand=%d, lengthMs=%d, attenuationDb=%d)"), eventCode, lengthMs, attenuationDb);
                Sleep(sleepMs);
                lengthMs = lenMult*lengthMs;
                eventCode++;
            }
        }
    }
    else
    {
        // --- SendTelephoneEvent

        if (_modeSingle)
        {
            TEST2(_veDTMFPtr->SendTelephoneEvent(_channel, eventCode, outBand, lengthMs, attenuationDb) == 0,
                _T("SendTelephoneEvent(channel=%d, eventCode=%u, outBand=%d, lengthMs=%d, attenuationDb=%d)"), _channel, eventCode, outBand, lengthMs, attenuationDb);
        }
        else if (_modeStartStop)
        {
            TEST2(false, _T("*** NOT IMPLEMENTED ***"));
        }
        else if (_modeSequence)
        {
            int nTones(1);
            int sleepMs(0);
            int lenMult(1);
            if (eventCode == 1)
            {
                nTones = 2;
                sleepMs = lengthMs;
                lenMult = 1;
            }
            else if (eventCode == 2)
            {
                eventCode = 1;
                nTones = 2;
                sleepMs = lengthMs/2;
                lenMult = 2;
            }
            else if (eventCode == 3)
            {
                eventCode = 1;
                nTones = 3;
                sleepMs = 0;
                lenMult = 1;
            }
            for (int i = 0; i < nTones; i++)
            {
                TEST2(_veDTMFPtr->SendTelephoneEvent(_channel, eventCode, outBand, lengthMs, attenuationDb) == 0,
                    _T("SendTelephoneEvent(channel=%d, eventCode=%u, outBand=%d, lengthMs=%d, attenuationDb=%d)"), _channel, eventCode, outBand, lengthMs, attenuationDb);
                Sleep(sleepMs);
                lengthMs = lenMult*lengthMs;
                eventCode++;
            }
        }
    }
}

void CTelephonyEvent::OnBnClickedButtonSendTelephoneEvent()
{
    BOOL ret;
    unsigned char eventCode(0);

    eventCode = (unsigned char)GetDlgItemInt(IDC_EDIT_EVENT_CODE, &ret);
    if (ret == FALSE)
    {
        return;
    }
    SendTelephoneEvent(eventCode);
}

void CTelephonyEvent::OnBnClickedButton1()
{
    SendTelephoneEvent(1);
}

void CTelephonyEvent::OnBnClickedButton2()
{
    SendTelephoneEvent(2);
}

void CTelephonyEvent::OnBnClickedButton3()
{
    SendTelephoneEvent(3);
}

void CTelephonyEvent::OnBnClickedButton4()
{
    SendTelephoneEvent(4);
}

void CTelephonyEvent::OnBnClickedButton5()
{
    SendTelephoneEvent(5);
}

void CTelephonyEvent::OnBnClickedButton6()
{
    SendTelephoneEvent(6);
}

void CTelephonyEvent::OnBnClickedButton7()
{
    SendTelephoneEvent(7);
}

void CTelephonyEvent::OnBnClickedButton8()
{
    SendTelephoneEvent(8);
}

void CTelephonyEvent::OnBnClickedButton9()
{
    SendTelephoneEvent(9);
}

void CTelephonyEvent::OnBnClickedButton10()
{
    // *
    SendTelephoneEvent(10);
}

void CTelephonyEvent::OnBnClickedButton11()
{
    SendTelephoneEvent(0);
}

void CTelephonyEvent::OnBnClickedButton12()
{
    // #
    SendTelephoneEvent(11);
}

void CTelephonyEvent::OnBnClickedButtonA()
{
    SendTelephoneEvent(12);
}

void CTelephonyEvent::OnBnClickedButtonB()
{
    SendTelephoneEvent(13);
}

void CTelephonyEvent::OnBnClickedButtonC()
{
    SendTelephoneEvent(14);
}

void CTelephonyEvent::OnBnClickedButtonD()
{
    SendTelephoneEvent(15);
}

void CTelephonyEvent::OnBnClickedCheckDtmfPlayoutRx()
{
    CButton* button = (CButton*)GetDlgItem(IDC_CHECK_DTMF_PLAYOUT_RX);
    int check = button->GetCheck();
    const bool enable = (check == BST_CHECKED);
    TEST2(_veDTMFPtr->SetDtmfPlayoutStatus(_channel, enable) == 0, _T("SetDtmfPlayoutStatus(channel=%d, enable=%d)"), _channel, enable);
}

void CTelephonyEvent::OnBnClickedCheckDtmfPlayTone()
{
    CButton* button = (CButton*)GetDlgItem(IDC_CHECK_DTMF_PLAY_TONE);
    int check = button->GetCheck();
    _PlayDtmfToneLocally = (check == BST_CHECKED);
}

void CTelephonyEvent::OnBnClickedRadioSingle()
{
    _modeStartStop = false;
    _modeSingle = true;
    _modeSequence = false;
}

void CTelephonyEvent::OnBnClickedRadioMulti()
{
    _modeStartStop = false;
    _modeSingle = false;
    _modeSequence = true;
}

void CTelephonyEvent::OnBnClickedRadioStartStop()
{
    // CButton* button = (CButton*)GetDlgItem(IDC_RADIO_START_STOP);
    // int check = button->GetCheck();
    _modeStartStop = true;
    _modeSingle = false;
    _modeSequence = false;
    // GetDlgItem(IDC_EDIT_EVENT_LENGTH)->EnableWindow();
}

void CTelephonyEvent::OnBnClickedCheckEventInband()
{
    CButton* button = (CButton*)GetDlgItem(IDC_CHECK_EVENT_INBAND);
    int check = button->GetCheck();
    GetDlgItem(IDC_EDIT_EVENT_CODE)->EnableWindow(check?FALSE:TRUE);
    GetDlgItem(IDC_BUTTON_SEND_TELEPHONE_EVENT)->EnableWindow(check?FALSE:TRUE);
}

void CTelephonyEvent::OnBnClickedCheckDtmfFeedback()
{
    CButton* button(NULL);

    // Retrieve feedback state
    button = (CButton*)GetDlgItem(IDC_CHECK_DTMF_FEEDBACK);
    int check = button->GetCheck();
    const bool enable = (check == BST_CHECKED);

    // Retrieve direct-feedback setting
    button = (CButton*)GetDlgItem(IDC_CHECK_DIRECT_FEEDBACK);
    check = button->GetCheck();
    const bool directFeedback = (check == BST_CHECKED);

    // GetDlgItem(IDC_CHECK_DIRECT_FEEDBACK)->EnableWindow(enable ? TRUE : FALSE);

    TEST2(_veDTMFPtr->SetDtmfFeedbackStatus(enable, directFeedback) == 0,
        _T("SetDtmfFeedbackStatus(enable=%d, directFeedback=%d)"), enable, directFeedback);
}

void CTelephonyEvent::OnBnClickedCheckDirectFeedback()
{
    CButton* button(NULL);

    // Retrieve feedback state
    button = (CButton*)GetDlgItem(IDC_CHECK_DTMF_FEEDBACK);
    int check = button->GetCheck();
    const bool enable = (check == BST_CHECKED);

    // Retrieve new direct-feedback setting
    button = (CButton*)GetDlgItem(IDC_CHECK_DIRECT_FEEDBACK);
    check = button->GetCheck();
    const bool directFeedback = (check == BST_CHECKED);

    TEST2(_veDTMFPtr->SetDtmfFeedbackStatus(enable, directFeedback) == 0,
        _T("SetDtmfFeedbackStatus(enable=%d, directFeedback=%d)"), enable, directFeedback);
}

void CTelephonyEvent::OnBnClickedButtonSetRxTelephonePt()
{
    BOOL ret;
    int pt = GetDlgItemInt(IDC_EDIT_EVENT_RX_PT, &ret);
    if (ret == FALSE)
        return;
    CodecInst codec;
    strcpy_s(codec.plname, 32, "telephone-event");
    codec.pltype = pt; codec.channels = 1; codec.plfreq = 8000;
    TEST2(_veCodecPtr->SetRecPayloadType(_channel, codec) == 0,
        _T("SetSendTelephoneEventPayloadType(channel=%d, codec.pltype=%u)"), _channel, codec.pltype);
}

void CTelephonyEvent::OnBnClickedButtonSetTxTelephonePt()
{
    BOOL ret;
    int pt = GetDlgItemInt(IDC_EDIT_EVENT_TX_PT, &ret);
    if (ret == FALSE)
        return;
    TEST2(_veDTMFPtr->SetSendTelephoneEventPayloadType(_channel, pt) == 0,
        _T("SetSendTelephoneEventPayloadType(channel=%d, type=%u)"), _channel, pt);
}

void CTelephonyEvent::OnBnClickedCheckDetectInband()
{
    CButton* button = (CButton*)GetDlgItem(IDC_CHECK_DETECT_INBAND);
    int check = button->GetCheck();
    _inbandEventDetection = (check == BST_CHECKED);

    bool enabled(false);
    TelephoneEventDetectionMethods detectionMethod;
    _veDTMFPtr->GetTelephoneEventDetectionStatus(_channel, enabled, detectionMethod);
    if (enabled)
    {
        // deregister
        _veDTMFPtr->DeRegisterTelephoneEventDetection(_channel);
        delete _telephoneEventObserverPtr;
        _telephoneEventObserverPtr = NULL;
        SetDlgItemText(IDC_EDIT_ON_EVENT_INBAND,_T(""));
        SetDlgItemText(IDC_EDIT_ON_EVENT_OUT_OF_BAND,_T(""));
    }
    OnBnClickedCheckEventDetection();
}

void CTelephonyEvent::OnBnClickedCheckDetectOutOfBand()
{
    CButton* button = (CButton*)GetDlgItem(IDC_CHECK_DETECT_OUT_OF_BAND);
    int check = button->GetCheck();
    _outOfBandEventDetection = (check == BST_CHECKED);

    bool enabled(false);
    TelephoneEventDetectionMethods detectionMethod;
    _veDTMFPtr->GetTelephoneEventDetectionStatus(_channel, enabled, detectionMethod);
    if (enabled)
    {
        // deregister
        _veDTMFPtr->DeRegisterTelephoneEventDetection(_channel);
        delete _telephoneEventObserverPtr;
        _telephoneEventObserverPtr = NULL;
        SetDlgItemText(IDC_EDIT_ON_EVENT_INBAND,_T(""));
        SetDlgItemText(IDC_EDIT_ON_EVENT_OUT_OF_BAND,_T(""));
    }
    OnBnClickedCheckEventDetection();
}

void CTelephonyEvent::OnBnClickedCheckEventDetection()
{
    CButton* button = (CButton*)GetDlgItem(IDC_CHECK_EVENT_DETECTION);
    int check = button->GetCheck();
    const bool enable = (check == BST_CHECKED);

    if (enable)
    {
        TelephoneEventDetectionMethods method(kInBand);
        if (_inbandEventDetection && !_outOfBandEventDetection)
            method = kInBand;
        else if (!_inbandEventDetection && _outOfBandEventDetection)
            method = kOutOfBand;
        else if (_inbandEventDetection && _outOfBandEventDetection)
            method = kInAndOutOfBand;

        CWnd* wndOut = GetDlgItem(IDC_EDIT_ON_EVENT_OUT_OF_BAND);
        CWnd* wndIn = GetDlgItem(IDC_EDIT_ON_EVENT_INBAND);
        _telephoneEventObserverPtr = new TelephoneEventObserver(wndOut, wndIn);

        TEST2(_veDTMFPtr->RegisterTelephoneEventDetection(_channel, method, *_telephoneEventObserverPtr) == 0,
            _T("RegisterTelephoneEventDetection(channel=%d, detectionMethod=%d)"), _channel, method);
    }
    else
    {
        TEST2(_veDTMFPtr->DeRegisterTelephoneEventDetection(_channel) == 0,
            _T("DeRegisterTelephoneEventDetection(channel=%d)"), _channel);
        delete _telephoneEventObserverPtr;
        _telephoneEventObserverPtr = NULL;
        SetDlgItemText(IDC_EDIT_ON_EVENT_INBAND,_T(""));
        SetDlgItemText(IDC_EDIT_ON_EVENT_OUT_OF_BAND,_T(""));
    }
}

// ============================================================================
//                                 CWinTestDlg dialog
// ============================================================================

CWinTestDlg::CWinTestDlg(CWnd* pParent /*=NULL*/)
    : CDialog(CWinTestDlg::IDD, pParent),
    _failCount(0),
    _vePtr(NULL),
    _veBasePtr(NULL),
    _veCodecPtr(NULL),
    _veNetworkPtr(NULL),
    _veFilePtr(NULL),
    _veHardwarePtr(NULL),
    _veExternalMediaPtr(NULL),
    _veApmPtr(NULL),
    _veEncryptionPtr(NULL),
    _veRtpRtcpPtr(NULL),
    _transportPtr(NULL),
    _encryptionPtr(NULL),
    _externalMediaPtr(NULL),
    _externalTransport(false),
    _externalTransportBuild(false),
    _checkPlayFileIn(0),
    _checkPlayFileIn1(0),
    _checkPlayFileIn2(0),
    _checkPlayFileOut1(0),
    _checkPlayFileOut2(0),
    _checkAGC(0),
    _checkAGC1(0),
    _checkNS(0),
    _checkNS1(0),
    _checkEC(0),
    _checkVAD1(0),
    _checkVAD2(0),
    _checkSrtpTx1(0),
    _checkSrtpTx2(0),
    _checkSrtpRx1(0),
    _checkSrtpRx2(0),
    _checkConference1(0),
    _checkConference2(0),
    _checkOnHold1(0),
    _checkOnHold2(0),
    _strComboIp1(_T("")),
    _strComboIp2(_T("")),
    _delayEstimate1(false),
    _delayEstimate2(false),
    _rxVad(false),
    _nErrorCallbacks(0),
    _timerTicks(0)
{
    m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);

    _vePtr = VoiceEngine::Create();

    VoiceEngine::SetTraceFilter(kTraceNone);
    // VoiceEngine::SetTraceFilter(kTraceAll);
    // VoiceEngine::SetTraceFilter(kTraceStream | kTraceStateInfo | kTraceWarning | kTraceError | kTraceCritical | kTraceApiCall | kTraceModuleCall | kTraceMemory | kTraceDebug | kTraceInfo);
    // VoiceEngine::SetTraceFilter(kTraceStateInfo | kTraceWarning | kTraceError | kTraceCritical | kTraceApiCall | kTraceModuleCall | kTraceMemory | kTraceInfo);

    VoiceEngine::SetTraceFile("ve_win_test.txt");
    VoiceEngine::SetTraceCallback(NULL);

    if (_vePtr)
    {
        _veExternalMediaPtr = VoEExternalMedia::GetInterface(_vePtr);
        _veVolumeControlPtr = VoEVolumeControl::GetInterface(_vePtr);
        _veEncryptionPtr = VoEEncryption::GetInterface(_vePtr);
        _veVideoSyncPtr = VoEVideoSync::GetInterface(_vePtr);
        _veNetworkPtr = VoENetwork::GetInterface(_vePtr);
        _veFilePtr = VoEFile::GetInterface(_vePtr);
        _veApmPtr = VoEAudioProcessing::GetInterface(_vePtr);

        _veBasePtr = VoEBase::GetInterface(_vePtr);
        _veCodecPtr = VoECodec::GetInterface(_vePtr);
        _veHardwarePtr = VoEHardware::GetInterface(_vePtr);
        _veRtpRtcpPtr = VoERTP_RTCP::GetInterface(_vePtr);
        _transportPtr = new MyTransport(_veNetworkPtr);
        _encryptionPtr = new MyEncryption();
        _externalMediaPtr = new MediaProcessImpl();
        _connectionObserverPtr = new ConnectionObserver();
        _rxVadObserverPtr = new RxCallback();
    }

    _veBasePtr->RegisterVoiceEngineObserver(*this);

    std::string resource_path = webrtc::test::ProjectRootPath();
    if (resource_path == webrtc::test::kCannotFindProjectRootDir) {
        _long_audio_file_path = "./";
    } else {
        _long_audio_file_path = resource_path + "data\\voice_engine\\";
    }
}

CWinTestDlg::~CWinTestDlg()
{
    if (_connectionObserverPtr) delete _connectionObserverPtr;
    if (_externalMediaPtr) delete _externalMediaPtr;
    if (_transportPtr) delete _transportPtr;
    if (_encryptionPtr) delete _encryptionPtr;
    if (_rxVadObserverPtr) delete _rxVadObserverPtr;

    if (_veExternalMediaPtr) _veExternalMediaPtr->Release();
    if (_veEncryptionPtr) _veEncryptionPtr->Release();
    if (_veVideoSyncPtr) _veVideoSyncPtr->Release();
    if (_veVolumeControlPtr) _veVolumeControlPtr->Release();

    if (_veBasePtr) _veBasePtr->Terminate();
    if (_veBasePtr) _veBasePtr->Release();

    if (_veCodecPtr) _veCodecPtr->Release();
    if (_veNetworkPtr) _veNetworkPtr->Release();
    if (_veFilePtr) _veFilePtr->Release();
    if (_veHardwarePtr) _veHardwarePtr->Release();
    if (_veApmPtr) _veApmPtr->Release();
    if (_veRtpRtcpPtr) _veRtpRtcpPtr->Release();
    if (_vePtr)
    {
        VoiceEngine::Delete(_vePtr);
    }
    VoiceEngine::SetTraceFilter(kTraceNone);
}

void CWinTestDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_CBString(pDX, IDC_COMBO_IP_1, _strComboIp1);
    DDX_CBString(pDX, IDC_COMBO_IP_2, _strComboIp2);
}

BEGIN_MESSAGE_MAP(CWinTestDlg, CDialog)
    ON_WM_SYSCOMMAND()
    ON_WM_PAINT()
    ON_WM_QUERYDRAGICON()
    ON_WM_TIMER()
    //}}AFX_MSG_MAP
    ON_BN_CLICKED(IDC_BUTTON_CREATE_1, &CWinTestDlg::OnBnClickedButtonCreate1)
    ON_BN_CLICKED(IDC_BUTTON_DELETE_1, &CWinTestDlg::OnBnClickedButtonDelete1)
    ON_BN_CLICKED(IDC_BUTTON_CREATE_2, &CWinTestDlg::OnBnClickedButtonCreate2)
    ON_BN_CLICKED(IDC_BUTTON_DELETE_2, &CWinTestDlg::OnBnClickedButtonDelete2)
    ON_CBN_SELCHANGE(IDC_COMBO_CODEC_1, &CWinTestDlg::OnCbnSelchangeComboCodec1)
    ON_BN_CLICKED(IDC_BUTTON_START_LISTEN_1, &CWinTestDlg::OnBnClickedButtonStartListen1)
    ON_BN_CLICKED(IDC_BUTTON_STOP_LISTEN_1, &CWinTestDlg::OnBnClickedButtonStopListen1)
    ON_BN_CLICKED(IDC_BUTTON_START_PLAYOUT_1, &CWinTestDlg::OnBnClickedButtonStartPlayout1)
    ON_BN_CLICKED(IDC_BUTTON_STOP_PLAYOUT_1, &CWinTestDlg::OnBnClickedButtonStopPlayout1)
    ON_BN_CLICKED(IDC_BUTTON_START_SEND_1, &CWinTestDlg::OnBnClickedButtonStartSend1)
    ON_BN_CLICKED(IDC_BUTTON_STOP_SEND_1, &CWinTestDlg::OnBnClickedButtonStopSend1)
    ON_CBN_SELCHANGE(IDC_COMBO_IP_2, &CWinTestDlg::OnCbnSelchangeComboIp2)
    ON_CBN_SELCHANGE(IDC_COMBO_IP_1, &CWinTestDlg::OnCbnSelchangeComboIp1)
    ON_CBN_SELCHANGE(IDC_COMBO_CODEC_2, &CWinTestDlg::OnCbnSelchangeComboCodec2)
    ON_BN_CLICKED(IDC_BUTTON_START_LISTEN_2, &CWinTestDlg::OnBnClickedButtonStartListen2)
    ON_BN_CLICKED(IDC_BUTTON_STOP_LISTEN_2, &CWinTestDlg::OnBnClickedButtonStopListen2)
    ON_BN_CLICKED(IDC_BUTTON_START_PLAYOUT_2, &CWinTestDlg::OnBnClickedButtonStartPlayout2)
    ON_BN_CLICKED(IDC_BUTTON_STOP_PLAYOUT_2, &CWinTestDlg::OnBnClickedButtonStopPlayout2)
    ON_BN_CLICKED(IDC_BUTTON_START_SEND_2, &CWinTestDlg::OnBnClickedButtonStartSend2)
    ON_BN_CLICKED(IDC_BUTTON_STOP_SEND_2, &CWinTestDlg::OnBnClickedButtonStopSend2)
    ON_BN_CLICKED(IDC_CHECK_EXT_TRANS_1, &CWinTestDlg::OnBnClickedCheckExtTrans1)
    ON_BN_CLICKED(IDC_CHECK_PLAY_FILE_IN_1, &CWinTestDlg::OnBnClickedCheckPlayFileIn1)
    ON_BN_CLICKED(IDC_CHECK_PLAY_FILE_OUT_1, &CWinTestDlg::OnBnClickedCheckPlayFileOut1)
    ON_BN_CLICKED(IDC_CHECK_EXT_TRANS_2, &CWinTestDlg::OnBnClickedCheckExtTrans2)
    ON_BN_CLICKED(IDC_CHECK_PLAY_FILE_IN_2, &CWinTestDlg::OnBnClickedCheckPlayFileIn2)
    ON_BN_CLICKED(IDC_CHECK_PLAY_FILE_OUT_2, &CWinTestDlg::OnBnClickedCheckPlayFileOut2)
    ON_BN_CLICKED(IDC_CHECK_PLAY_FILE_IN, &CWinTestDlg::OnBnClickedCheckPlayFileIn)
    ON_CBN_SELCHANGE(IDC_COMBO_REC_DEVICE, &CWinTestDlg::OnCbnSelchangeComboRecDevice)
    ON_CBN_SELCHANGE(IDC_COMBO_PLAY_DEVICE, &CWinTestDlg::OnCbnSelchangeComboPlayDevice)
    ON_BN_CLICKED(IDC_CHECK_EXT_MEDIA_IN_1, &CWinTestDlg::OnBnClickedCheckExtMediaIn1)
    ON_BN_CLICKED(IDC_CHECK_EXT_MEDIA_OUT_1, &CWinTestDlg::OnBnClickedCheckExtMediaOut1)
    ON_NOTIFY(NM_RELEASEDCAPTURE, IDC_SLIDER_INPUT_VOLUME, &CWinTestDlg::OnNMReleasedcaptureSliderInputVolume)
    ON_NOTIFY(NM_RELEASEDCAPTURE, IDC_SLIDER_OUTPUT_VOLUME, &CWinTestDlg::OnNMReleasedcaptureSliderOutputVolume)
    ON_BN_CLICKED(IDC_CHECK_AGC, &CWinTestDlg::OnBnClickedCheckAgc)
    ON_BN_CLICKED(IDC_CHECK_NS, &CWinTestDlg::OnBnClickedCheckNs)
    ON_BN_CLICKED(IDC_CHECK_EC, &CWinTestDlg::OnBnClickedCheckEc)
    ON_BN_CLICKED(IDC_CHECK_VAD_1, &CWinTestDlg::OnBnClickedCheckVad1)
    ON_BN_CLICKED(IDC_CHECK_VAD_3, &CWinTestDlg::OnBnClickedCheckVad2)
    ON_BN_CLICKED(IDC_CHECK_EXT_MEDIA_IN_2, &CWinTestDlg::OnBnClickedCheckExtMediaIn2)
    ON_BN_CLICKED(IDC_CHECK_EXT_MEDIA_OUT_2, &CWinTestDlg::OnBnClickedCheckExtMediaOut2)
    ON_BN_CLICKED(IDC_CHECK_MUTE_IN, &CWinTestDlg::OnBnClickedCheckMuteIn)
    ON_BN_CLICKED(IDC_CHECK_MUTE_IN_1, &CWinTestDlg::OnBnClickedCheckMuteIn1)
    ON_BN_CLICKED(IDC_CHECK_MUTE_IN_2, &CWinTestDlg::OnBnClickedCheckMuteIn2)
    ON_BN_CLICKED(IDC_CHECK_SRTP_TX_1, &CWinTestDlg::OnBnClickedCheckSrtpTx1)
    ON_BN_CLICKED(IDC_CHECK_SRTP_RX_1, &CWinTestDlg::OnBnClickedCheckSrtpRx1)
    ON_BN_CLICKED(IDC_CHECK_SRTP_TX_2, &CWinTestDlg::OnBnClickedCheckSrtpTx2)
    ON_BN_CLICKED(IDC_CHECK_SRTP_RX_2, &CWinTestDlg::OnBnClickedCheckSrtpRx2)
    ON_BN_CLICKED(IDC_CHECK_EXT_ENCRYPTION_1, &CWinTestDlg::OnBnClickedCheckExtEncryption1)
    ON_BN_CLICKED(IDC_CHECK_EXT_ENCRYPTION_2, &CWinTestDlg::OnBnClickedCheckExtEncryption2)
    ON_BN_CLICKED(IDC_BUTTON_DTMF_1, &CWinTestDlg::OnBnClickedButtonDtmf1)
    ON_BN_CLICKED(IDC_CHECK_REC_MIC, &CWinTestDlg::OnBnClickedCheckRecMic)
    ON_BN_CLICKED(IDC_BUTTON_DTMF_2, &CWinTestDlg::OnBnClickedButtonDtmf2)
    ON_BN_CLICKED(IDC_BUTTON_TEST_1, &CWinTestDlg::OnBnClickedButtonTest1)
    ON_BN_CLICKED(IDC_CHECK_CONFERENCE_1, &CWinTestDlg::OnBnClickedCheckConference1)
    ON_BN_CLICKED(IDC_CHECK_CONFERENCE_2, &CWinTestDlg::OnBnClickedCheckConference2)
    ON_BN_CLICKED(IDC_CHECK_ON_HOLD_1, &CWinTestDlg::OnBnClickedCheckOnHold1)
    ON_BN_CLICKED(IDC_CHECK_ON_HOLD_2, &CWinTestDlg::OnBnClickedCheckOnHold2)
    ON_BN_CLICKED(IDC_CHECK_EXT_MEDIA_IN, &CWinTestDlg::OnBnClickedCheckExtMediaIn)
    ON_BN_CLICKED(IDC_CHECK_EXT_MEDIA_OUT, &CWinTestDlg::OnBnClickedCheckExtMediaOut)
    ON_LBN_SELCHANGE(IDC_LIST_CODEC_1, &CWinTestDlg::OnLbnSelchangeListCodec1)
    ON_NOTIFY(NM_RELEASEDCAPTURE, IDC_SLIDER_PAN_LEFT, &CWinTestDlg::OnNMReleasedcaptureSliderPanLeft)
    ON_NOTIFY(NM_RELEASEDCAPTURE, IDC_SLIDER_PAN_RIGHT, &CWinTestDlg::OnNMReleasedcaptureSliderPanRight)
    ON_BN_CLICKED(IDC_BUTTON_VERSION, &CWinTestDlg::OnBnClickedButtonVersion)
    ON_BN_CLICKED(IDC_CHECK_DELAY_ESTIMATE_1, &CWinTestDlg::OnBnClickedCheckDelayEstimate1)
    ON_BN_CLICKED(IDC_CHECK_RXVAD, &CWinTestDlg::OnBnClickedCheckRxvad)
    ON_BN_CLICKED(IDC_CHECK_AGC_1, &CWinTestDlg::OnBnClickedCheckAgc1)
    ON_BN_CLICKED(IDC_CHECK_NS_1, &CWinTestDlg::OnBnClickedCheckNs1)
    ON_BN_CLICKED(IDC_CHECK_REC_CALL, &CWinTestDlg::OnBnClickedCheckRecCall)
    ON_BN_CLICKED(IDC_CHECK_TYPING_DETECTION, &CWinTestDlg::OnBnClickedCheckTypingDetection)
    ON_BN_CLICKED(IDC_CHECK_FEC, &CWinTestDlg::OnBnClickedCheckFEC)
    ON_BN_CLICKED(IDC_BUTTON_CLEAR_ERROR_CALLBACK, &CWinTestDlg::OnBnClickedButtonClearErrorCallback)
END_MESSAGE_MAP()

BOOL CWinTestDlg::UpdateTest(bool failed, const CString& strMsg)
{
    if (failed)
    {
        SetDlgItemText(IDC_EDIT_MESSAGE, strMsg);
        _strErr.Format(_T("FAILED (error=%d)"), _veBasePtr->LastError());
        SetDlgItemText(IDC_EDIT_RESULT, _strErr);
        _failCount++;
        SetDlgItemInt(IDC_EDIT_N_FAILS, _failCount);
        SetDlgItemInt(IDC_EDIT_LAST_ERROR, _veBasePtr->LastError());
    }
    else
    {
        SetDlgItemText(IDC_EDIT_MESSAGE, strMsg);
        SetDlgItemText(IDC_EDIT_RESULT, _T("OK"));
    }
    return TRUE;
}


// CWinTestDlg message handlers

BOOL CWinTestDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    // Add "About..." menu item to system menu.

    // IDM_ABOUTBOX must be in the system command range.
    ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
    ASSERT(IDM_ABOUTBOX < 0xF000);

    CMenu* pSysMenu = GetSystemMenu(FALSE);
    if (pSysMenu != NULL)
    {
        CString strAboutMenu;
        strAboutMenu.LoadString(IDS_ABOUTBOX);
        if (!strAboutMenu.IsEmpty())
        {
            pSysMenu->AppendMenu(MF_SEPARATOR);
            pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
        }
    }

    // Set the icon for this dialog.  The framework does this automatically
    //  when the application's main window is not a dialog
    SetIcon(m_hIcon, TRUE);            // Set big icon
    SetIcon(m_hIcon, FALSE);        // Set small icon

    // char version[1024];
    // _veBasePtr->GetVersion(version);
    // AfxMessageBox(version, MB_OK);

    if (_veBasePtr->Init() != 0)
    {
         AfxMessageBox(_T("Init() failed "), MB_OKCANCEL);
    }

    int ch = _veBasePtr->CreateChannel();
    if (_veBasePtr->SetSendDestination(ch, 1234, "127.0.0.1") == -1)
    {
        if (_veBasePtr->LastError() == VE_EXTERNAL_TRANSPORT_ENABLED)
        {
            _strMsg.Format(_T("*** External transport build ***"));
            SetDlgItemText(IDC_EDIT_MESSAGE, _strMsg);
            _externalTransportBuild = true;
        }
    }
    _veBasePtr->DeleteChannel(ch);

    // --- Add (preferred) local IPv4 address in title

    if (_veNetworkPtr)
    {
        char localIP[64];
        _veNetworkPtr->GetLocalIP(localIP);
        CString str;
        GetWindowText(str);
        str.AppendFormat(_T("  [Local IPv4 address: %s]"), CharToTchar(localIP, 64));
        SetWindowText(str);
    }

    // --- Volume sliders

    if (_veVolumeControlPtr)
    {
        unsigned int volume(0);
        CSliderCtrl* slider(NULL);

        slider = (CSliderCtrl*)GetDlgItem(IDC_SLIDER_INPUT_VOLUME);
        slider->SetRangeMin(0);
        slider->SetRangeMax(255);
        _veVolumeControlPtr->GetMicVolume(volume);
        slider->SetPos(volume);

        slider = (CSliderCtrl*)GetDlgItem(IDC_SLIDER_OUTPUT_VOLUME);
        slider->SetRangeMin(0);
        slider->SetRangeMax(255);
        _veVolumeControlPtr->GetSpeakerVolume(volume);
        slider->SetPos(volume);
    }

    // --- Panning sliders

    if (_veVolumeControlPtr)
    {
        float lVol(0.0);
        float rVol(0.0);
        int leftVol, rightVol;
        CSliderCtrl* slider(NULL);

        _veVolumeControlPtr->GetOutputVolumePan(-1, lVol, rVol);

        leftVol = (int)(lVol*10.0f);    // [0,10]
        rightVol = (int)(rVol*10.0f);    // [0,10]

        slider = (CSliderCtrl*)GetDlgItem(IDC_SLIDER_PAN_LEFT);
        slider->SetRange(0,10);
        slider->SetPos(10-leftVol);        // pos 0 <=> max pan 1.0 (top of slider)

        slider = (CSliderCtrl*)GetDlgItem(IDC_SLIDER_PAN_RIGHT);
        slider->SetRange(0,10);
        slider->SetPos(10-rightVol);
    }

    // --- APM settings

    bool enable(false);
    CButton* button(NULL);

    AgcModes agcMode(kAgcDefault);
    if (_veApmPtr->GetAgcStatus(enable, agcMode) == 0)
    {
        button = (CButton*)GetDlgItem(IDC_CHECK_AGC);
        enable ? button->SetCheck(BST_CHECKED) : button->SetCheck(BST_UNCHECKED);
    }
    else
    {
        // AGC is not supported
        GetDlgItem(IDC_CHECK_AGC)->EnableWindow(FALSE);
    }

    NsModes nsMode(kNsDefault);
    if (_veApmPtr->GetNsStatus(enable, nsMode) == 0)
    {
        button = (CButton*)GetDlgItem(IDC_CHECK_NS);
        enable ? button->SetCheck(BST_CHECKED) : button->SetCheck(BST_UNCHECKED);
    }
    else
    {
        // NS is not supported
        GetDlgItem(IDC_CHECK_NS)->EnableWindow(FALSE);
    }

    EcModes ecMode(kEcDefault);
    if (_veApmPtr->GetEcStatus(enable, ecMode) == 0)
    {
        button = (CButton*)GetDlgItem(IDC_CHECK_EC);
        enable ? button->SetCheck(BST_CHECKED) : button->SetCheck(BST_UNCHECKED);
    }
    else
    {
        // EC is not supported
        GetDlgItem(IDC_CHECK_EC)->EnableWindow(FALSE);
    }

    // --- First channel section

    GetDlgItem(IDC_COMBO_IP_1)->EnableWindow(FALSE);
    GetDlgItem(IDC_EDIT_TX_PORT_1)->EnableWindow(FALSE);
    GetDlgItem(IDC_EDIT_RX_PORT_1)->EnableWindow(FALSE);
    GetDlgItem(IDC_COMBO_CODEC_1)->EnableWindow(FALSE);
    GetDlgItem(IDC_LIST_CODEC_1)->EnableWindow(FALSE);
    GetDlgItem(IDC_EDIT_CODEC_1)->EnableWindow(FALSE);
    GetDlgItem(IDC_BUTTON_DELETE_1)->EnableWindow(FALSE);
    GetDlgItem(IDC_BUTTON_START_LISTEN_1)->EnableWindow(FALSE);
    GetDlgItem(IDC_BUTTON_STOP_LISTEN_1)->EnableWindow(FALSE);
    GetDlgItem(IDC_BUTTON_START_PLAYOUT_1)->EnableWindow(FALSE);
    GetDlgItem(IDC_BUTTON_STOP_PLAYOUT_1)->EnableWindow(FALSE);
    GetDlgItem(IDC_BUTTON_START_SEND_1)->EnableWindow(FALSE);
    GetDlgItem(IDC_BUTTON_STOP_SEND_1)->EnableWindow(FALSE);
    GetDlgItem(IDC_CHECK_EXT_TRANS_1)->EnableWindow(FALSE);
    GetDlgItem(IDC_CHECK_PLAY_FILE_IN_1)->EnableWindow(FALSE);
    GetDlgItem(IDC_CHECK_PLAY_FILE_OUT_1)->EnableWindow(FALSE);
    GetDlgItem(IDC_CHECK_EXT_MEDIA_IN_1)->EnableWindow(FALSE);
    GetDlgItem(IDC_CHECK_EXT_MEDIA_OUT_1)->EnableWindow(FALSE);
    GetDlgItem(IDC_CHECK_VAD_1)->EnableWindow(FALSE);
    GetDlgItem(IDC_CHECK_MUTE_IN_1)->EnableWindow(FALSE);
    GetDlgItem(IDC_CHECK_SRTP_TX_1)->EnableWindow(FALSE);
    GetDlgItem(IDC_CHECK_SRTP_RX_1)->EnableWindow(FALSE);
    GetDlgItem(IDC_CHECK_EXT_ENCRYPTION_1)->EnableWindow(FALSE);
    GetDlgItem(IDC_BUTTON_DTMF_1)->EnableWindow(FALSE);
    GetDlgItem(IDC_CHECK_CONFERENCE_1)->EnableWindow(FALSE);
    GetDlgItem(IDC_CHECK_ON_HOLD_1)->EnableWindow(FALSE);
    GetDlgItem(IDC_CHECK_DELAY_ESTIMATE_1)->EnableWindow(FALSE);
    GetDlgItem(IDC_CHECK_RXVAD)->EnableWindow(FALSE);
    GetDlgItem(IDC_CHECK_AGC_1)->EnableWindow(FALSE);
    GetDlgItem(IDC_CHECK_NS_1)->EnableWindow(FALSE);
    GetDlgItem(IDC_CHECK_FEC)->EnableWindow(FALSE);

    CComboBox* comboIP(NULL);
    comboIP = (CComboBox*)GetDlgItem(IDC_COMBO_IP_1);
    comboIP->AddString(_T("127.0.0.1"));
    comboIP->SetCurSel(0);

    SetDlgItemInt(IDC_EDIT_TX_PORT_1, 1111);
    SetDlgItemInt(IDC_EDIT_RX_PORT_1, 1111);

    // --- Add supported codecs to the codec combo box

    CComboBox* comboCodec(NULL);
    comboCodec = (CComboBox*)GetDlgItem(IDC_COMBO_CODEC_1);
    comboCodec->ResetContent();

    int numCodecs = _veCodecPtr->NumOfCodecs();
    for (int idx = 0; idx < numCodecs; idx++)
    {
        CodecInst codec;
        _veCodecPtr->GetCodec(idx, codec);
        if ((_stricmp(codec.plname, "CNNB") != 0) &&
            (_stricmp(codec.plname, "CNWB") != 0))
        {
            CString strCodec;
            if (_stricmp(codec.plname, "G7221") == 0)
                strCodec.Format(_T("%s (%d/%d/%d)"), CharToTchar(codec.plname, 32), codec.pltype, codec.plfreq/1000, codec.rate/1000);
            else
                strCodec.Format(_T("%s (%d/%d)"), CharToTchar(codec.plname, 32), codec.pltype, codec.plfreq/1000);
            comboCodec->AddString(strCodec);
        }
        if (idx == 0)
        {
            SetDlgItemInt(IDC_EDIT_CODEC_1, codec.pltype);
        }
    }
    comboCodec->SetCurSel(0);

    CListBox* list = (CListBox*)GetDlgItem(IDC_LIST_CODEC_1);
    list->AddString(_T("pltype"));
    list->AddString(_T("plfreq"));
    list->AddString(_T("pacsize"));
    list->AddString(_T("channels"));
    list->AddString(_T("rate"));
    list->SetCurSel(0);

    // --- Add available audio devices to the combo boxes

    CComboBox* comboRecDevice(NULL);
    CComboBox* comboPlayDevice(NULL);
    comboRecDevice = (CComboBox*)GetDlgItem(IDC_COMBO_REC_DEVICE);
    comboPlayDevice = (CComboBox*)GetDlgItem(IDC_COMBO_PLAY_DEVICE);
    comboRecDevice->ResetContent();
    comboPlayDevice->ResetContent();

    if (_veHardwarePtr)
    {
        int numPlayout(0);
        int numRecording(0);
        char nameStr[128];
        char guidStr[128];
        CString strDevice;
        AudioLayers audioLayer;

        _veHardwarePtr->GetAudioDeviceLayer(audioLayer);
        if (kAudioWindowsWave == audioLayer)
        {
            strDevice.FormatMessage(_T("Audio Layer: Windows Wave API"));
        }
        else if (kAudioWindowsCore == audioLayer)
        {
            strDevice.FormatMessage(_T("Audio Layer: Windows Core API"));
        }
        else
        {
            strDevice.FormatMessage(_T("Audio Layer: ** UNKNOWN **"));
        }
        SetDlgItemText(IDC_EDIT_AUDIO_LAYER, (LPCTSTR)strDevice);

        _veHardwarePtr->GetNumOfRecordingDevices(numRecording);

        for (int idx = 0; idx < numRecording; idx++)
        {
            _veHardwarePtr->GetRecordingDeviceName(idx, nameStr, guidStr);
      strDevice.Format(_T("%s"), CharToTchar(nameStr, 128));
            comboRecDevice->AddString(strDevice);
        }
        // Select default (communication) device in the combo box
        _veHardwarePtr->GetRecordingDeviceName(-1, nameStr, guidStr);
    CString tmp = CString(nameStr);
        int nIndex = comboRecDevice->SelectString(-1, tmp);
        ASSERT(nIndex != CB_ERR);

        _veHardwarePtr->GetNumOfPlayoutDevices(numPlayout);

        for (int idx = 0; idx < numPlayout; idx++)
        {
            _veHardwarePtr->GetPlayoutDeviceName(idx, nameStr, guidStr);
      strDevice.Format(_T("%s"), CharToTchar(nameStr, 128));
            comboPlayDevice->AddString(strDevice);
        }
        // Select default (communication) device in the combo box
        _veHardwarePtr->GetPlayoutDeviceName(-1, nameStr, guidStr);
        nIndex = comboPlayDevice->SelectString(-1, CString(nameStr));
        ASSERT(nIndex != CB_ERR);
    }

    // --- Second channel section

    GetDlgItem(IDC_COMBO_IP_2)->EnableWindow(FALSE);
    GetDlgItem(IDC_EDIT_TX_PORT_2)->EnableWindow(FALSE);
    GetDlgItem(IDC_EDIT_RX_PORT_2)->EnableWindow(FALSE);
    GetDlgItem(IDC_COMBO_CODEC_2)->EnableWindow(FALSE);
    GetDlgItem(IDC_BUTTON_DELETE_2)->EnableWindow(FALSE);
    GetDlgItem(IDC_BUTTON_START_LISTEN_2)->EnableWindow(FALSE);
    GetDlgItem(IDC_BUTTON_STOP_LISTEN_2)->EnableWindow(FALSE);
    GetDlgItem(IDC_BUTTON_START_PLAYOUT_2)->EnableWindow(FALSE);
    GetDlgItem(IDC_BUTTON_STOP_PLAYOUT_2)->EnableWindow(FALSE);
    GetDlgItem(IDC_BUTTON_START_SEND_2)->EnableWindow(FALSE);
    GetDlgItem(IDC_BUTTON_STOP_SEND_2)->EnableWindow(FALSE);
    GetDlgItem(IDC_CHECK_EXT_TRANS_2)->EnableWindow(FALSE);
    GetDlgItem(IDC_CHECK_PLAY_FILE_IN_2)->EnableWindow(FALSE);
    GetDlgItem(IDC_CHECK_PLAY_FILE_OUT_2)->EnableWindow(FALSE);
    GetDlgItem(IDC_CHECK_EXT_MEDIA_IN_2)->EnableWindow(FALSE);
    GetDlgItem(IDC_CHECK_EXT_MEDIA_OUT_2)->EnableWindow(FALSE);
    GetDlgItem(IDC_CHECK_VAD_3)->EnableWindow(FALSE);
    GetDlgItem(IDC_CHECK_MUTE_IN_2)->EnableWindow(FALSE);
    GetDlgItem(IDC_CHECK_SRTP_TX_2)->EnableWindow(FALSE);
    GetDlgItem(IDC_CHECK_SRTP_RX_2)->EnableWindow(FALSE);
    GetDlgItem(IDC_CHECK_EXT_ENCRYPTION_2)->EnableWindow(FALSE);
    GetDlgItem(IDC_BUTTON_DTMF_2)->EnableWindow(FALSE);
    GetDlgItem(IDC_CHECK_CONFERENCE_2)->EnableWindow(FALSE);
    GetDlgItem(IDC_CHECK_ON_HOLD_2)->EnableWindow(FALSE);

    comboIP = (CComboBox*)GetDlgItem(IDC_COMBO_IP_2);
    comboIP->AddString(_T("127.0.0.1"));
    comboIP->SetCurSel(0);

    SetDlgItemInt(IDC_EDIT_TX_PORT_2, 2222);
    SetDlgItemInt(IDC_EDIT_RX_PORT_2, 2222);

    comboCodec = (CComboBox*)GetDlgItem(IDC_COMBO_CODEC_2);
    comboCodec->ResetContent();

    if (_veCodecPtr)
    {
        numCodecs = _veCodecPtr->NumOfCodecs();
        for (int idx = 0; idx < numCodecs; idx++)
        {
            CodecInst codec;
            _veCodecPtr->GetCodec(idx, codec);
            CString strCodec;
            strCodec.Format(_T("%s (%d/%d)"), CharToTchar(codec.plname, 32), codec.pltype, codec.plfreq/1000);
            comboCodec->AddString(strCodec);
        }
        comboCodec->SetCurSel(0);
    }

    // --- Start windows timer

    SetTimer(0, 1000, NULL);

    return TRUE;  // return TRUE  unless you set the focus to a control
}

void CWinTestDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
    if ((nID & 0xFFF0) == IDM_ABOUTBOX)
    {
        CAboutDlg dlgAbout;
        dlgAbout.DoModal();
    }
    else if (nID == SC_CLOSE)
    {
        BOOL ret;
        int channel(0);
        channel = GetDlgItemInt(IDC_EDIT_1, &ret);
        if (ret == TRUE)
        {
            _veBasePtr->DeleteChannel(channel);
        }
        channel = GetDlgItemInt(IDC_EDIT_2, &ret);
        if (ret == TRUE)
        {
            _veBasePtr->DeleteChannel(channel);
        }

        CDialog::OnSysCommand(nID, lParam);
    }
    else
    {
        CDialog::OnSysCommand(nID, lParam);
    }

}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CWinTestDlg::OnPaint()
{
    if (IsIconic())
    {
        CPaintDC dc(this); // device context for painting

        SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

        // Center icon in client rectangle
        int cxIcon = GetSystemMetrics(SM_CXICON);
        int cyIcon = GetSystemMetrics(SM_CYICON);
        CRect rect;
        GetClientRect(&rect);
        int x = (rect.Width() - cxIcon + 1) / 2;
        int y = (rect.Height() - cyIcon + 1) / 2;

        // Draw the icon
        dc.DrawIcon(x, y, m_hIcon);
    }
    else
    {
        CDialog::OnPaint();
    }
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CWinTestDlg::OnQueryDragIcon()
{
    return static_cast<HCURSOR>(m_hIcon);
}


void CWinTestDlg::OnBnClickedButtonCreate1()
{
    int channel(0);
    TEST((channel = _veBasePtr->CreateChannel()) >= 0, _T("CreateChannel(channel=%d)"), channel);
    if (channel >= 0)
    {
        _veRtpRtcpPtr->RegisterRTPObserver(channel, *this);

        SetDlgItemInt(IDC_EDIT_1, channel);
        GetDlgItem(IDC_BUTTON_CREATE_1)->EnableWindow(FALSE);
        GetDlgItem(IDC_BUTTON_DELETE_1)->EnableWindow(TRUE);
        GetDlgItem(IDC_COMBO_IP_1)->EnableWindow(TRUE);
        GetDlgItem(IDC_EDIT_TX_PORT_1)->EnableWindow(TRUE);
        GetDlgItem(IDC_EDIT_RX_PORT_1)->EnableWindow(TRUE);
        GetDlgItem(IDC_COMBO_CODEC_1)->EnableWindow(TRUE);
        GetDlgItem(IDC_LIST_CODEC_1)->EnableWindow(TRUE);
        GetDlgItem(IDC_EDIT_CODEC_1)->EnableWindow(TRUE);
        GetDlgItem(IDC_BUTTON_START_LISTEN_1)->EnableWindow(TRUE);
        GetDlgItem(IDC_BUTTON_START_PLAYOUT_1)->EnableWindow(TRUE);
        GetDlgItem(IDC_BUTTON_START_SEND_1)->EnableWindow(TRUE);
        GetDlgItem(IDC_CHECK_EXT_TRANS_1)->EnableWindow(TRUE);
        GetDlgItem(IDC_CHECK_PLAY_FILE_IN_1)->EnableWindow(TRUE);
        GetDlgItem(IDC_CHECK_PLAY_FILE_OUT_1)->EnableWindow(TRUE);
        GetDlgItem(IDC_CHECK_EXT_MEDIA_IN_1)->EnableWindow(TRUE);
        GetDlgItem(IDC_CHECK_EXT_MEDIA_OUT_1)->EnableWindow(TRUE);
        GetDlgItem(IDC_CHECK_VAD_1)->EnableWindow(TRUE);
        GetDlgItem(IDC_CHECK_MUTE_IN_1)->EnableWindow(TRUE);
        GetDlgItem(IDC_CHECK_SRTP_TX_1)->EnableWindow(TRUE);
        GetDlgItem(IDC_CHECK_SRTP_RX_1)->EnableWindow(TRUE);
        GetDlgItem(IDC_CHECK_EXT_ENCRYPTION_1)->EnableWindow(TRUE);
        GetDlgItem(IDC_BUTTON_DTMF_1)->EnableWindow(TRUE);
        GetDlgItem(IDC_CHECK_ON_HOLD_1)->EnableWindow(TRUE);
        GetDlgItem(IDC_CHECK_DELAY_ESTIMATE_1)->EnableWindow(TRUE);
        GetDlgItem(IDC_CHECK_RXVAD)->EnableWindow(TRUE);
        GetDlgItem(IDC_CHECK_AGC_1)->EnableWindow(TRUE);
        GetDlgItem(IDC_CHECK_NS_1)->EnableWindow(TRUE);
        GetDlgItem(IDC_CHECK_FEC)->EnableWindow(TRUE);

        // Always set send codec to default codec <=> index 0.
        CodecInst codec;
        _veCodecPtr->GetCodec(0, codec);
        _veCodecPtr->SetSendCodec(channel, codec);
    }
}

void CWinTestDlg::OnBnClickedButtonCreate2()
{
    int channel(0);
    TEST((channel = _veBasePtr->CreateChannel()) >=0 , _T("CreateChannel(%d)"), channel);
    if (channel >= 0)
    {
        _veRtpRtcpPtr->RegisterRTPObserver(channel, *this);

        SetDlgItemInt(IDC_EDIT_2, channel);
        GetDlgItem(IDC_BUTTON_CREATE_2)->EnableWindow(FALSE);
        GetDlgItem(IDC_BUTTON_DELETE_2)->EnableWindow(TRUE);
        GetDlgItem(IDC_COMBO_IP_2)->EnableWindow(TRUE);
        GetDlgItem(IDC_EDIT_TX_PORT_2)->EnableWindow(TRUE);
        GetDlgItem(IDC_EDIT_RX_PORT_2)->EnableWindow(TRUE);
        GetDlgItem(IDC_COMBO_CODEC_2)->EnableWindow(TRUE);
        GetDlgItem(IDC_BUTTON_START_LISTEN_2)->EnableWindow(TRUE);
        GetDlgItem(IDC_BUTTON_START_PLAYOUT_2)->EnableWindow(TRUE);
        GetDlgItem(IDC_BUTTON_START_SEND_2)->EnableWindow(TRUE);
        GetDlgItem(IDC_CHECK_EXT_TRANS_2)->EnableWindow(TRUE);
        GetDlgItem(IDC_CHECK_PLAY_FILE_IN_2)->EnableWindow(TRUE);
        GetDlgItem(IDC_CHECK_PLAY_FILE_OUT_2)->EnableWindow(TRUE);
        GetDlgItem(IDC_CHECK_EXT_MEDIA_IN_2)->EnableWindow(TRUE);
        GetDlgItem(IDC_CHECK_EXT_MEDIA_OUT_2)->EnableWindow(TRUE);
        GetDlgItem(IDC_CHECK_VAD_3)->EnableWindow(TRUE);
        GetDlgItem(IDC_CHECK_MUTE_IN_2)->EnableWindow(TRUE);
        GetDlgItem(IDC_CHECK_SRTP_TX_2)->EnableWindow(TRUE);
        GetDlgItem(IDC_CHECK_SRTP_RX_2)->EnableWindow(TRUE);
        GetDlgItem(IDC_CHECK_EXT_ENCRYPTION_2)->EnableWindow(TRUE);
        GetDlgItem(IDC_BUTTON_DTMF_2)->EnableWindow(TRUE);
        GetDlgItem(IDC_CHECK_CONFERENCE_2)->EnableWindow(TRUE);
        GetDlgItem(IDC_CHECK_ON_HOLD_2)->EnableWindow(TRUE);

        // Always set send codec to default codec <=> index 0.
        CodecInst codec;
        _veCodecPtr->GetCodec(0, codec);
        _veCodecPtr->SetSendCodec(channel, codec);
    }
}

void CWinTestDlg::OnBnClickedButtonDelete1()
{
    BOOL ret;
    int channel = GetDlgItemInt(IDC_EDIT_1, &ret);
    if (ret == TRUE)
    {
        _delayEstimate1 = false;
        _rxVad = false;
        _veRtpRtcpPtr->DeRegisterRTPObserver(channel);
        TEST(_veBasePtr->DeleteChannel(channel) == 0, _T("DeleteChannel(channel=%d)"), channel);
        SetDlgItemText(IDC_EDIT_1, _T(""));
        GetDlgItem(IDC_BUTTON_CREATE_1)->EnableWindow(TRUE);
        GetDlgItem(IDC_BUTTON_DELETE_1)->EnableWindow(FALSE);
        GetDlgItem(IDC_COMBO_IP_1)->EnableWindow(FALSE);
        GetDlgItem(IDC_EDIT_TX_PORT_1)->EnableWindow(FALSE);
        GetDlgItem(IDC_EDIT_RX_PORT_1)->EnableWindow(FALSE);
        GetDlgItem(IDC_COMBO_CODEC_1)->EnableWindow(FALSE);
        GetDlgItem(IDC_LIST_CODEC_1)->EnableWindow(FALSE);
        GetDlgItem(IDC_EDIT_CODEC_1)->EnableWindow(FALSE);
        GetDlgItem(IDC_BUTTON_START_LISTEN_1)->EnableWindow(FALSE);
        GetDlgItem(IDC_BUTTON_START_PLAYOUT_1)->EnableWindow(FALSE);
        GetDlgItem(IDC_BUTTON_START_SEND_1)->EnableWindow(FALSE);
        GetDlgItem(IDC_BUTTON_STOP_LISTEN_1)->EnableWindow(FALSE);
        GetDlgItem(IDC_BUTTON_STOP_PLAYOUT_1)->EnableWindow(FALSE);
        GetDlgItem(IDC_BUTTON_STOP_SEND_1)->EnableWindow(FALSE);
        GetDlgItem(IDC_BUTTON_DTMF_1)->EnableWindow(FALSE);
        GetDlgItem(IDC_CHECK_EXT_TRANS_1)->EnableWindow(FALSE);
        GetDlgItem(IDC_CHECK_PLAY_FILE_IN_1)->EnableWindow(FALSE);
        GetDlgItem(IDC_CHECK_PLAY_FILE_OUT_1)->EnableWindow(FALSE);
        GetDlgItem(IDC_CHECK_EXT_MEDIA_IN_1)->EnableWindow(FALSE);
        GetDlgItem(IDC_CHECK_EXT_MEDIA_OUT_1)->EnableWindow(FALSE);
        GetDlgItem(IDC_CHECK_VAD_1)->EnableWindow(FALSE);
        GetDlgItem(IDC_CHECK_MUTE_IN_1)->EnableWindow(FALSE);
        GetDlgItem(IDC_CHECK_SRTP_TX_1)->EnableWindow(FALSE);
        GetDlgItem(IDC_CHECK_SRTP_RX_1)->EnableWindow(FALSE);
        GetDlgItem(IDC_CHECK_EXT_ENCRYPTION_1)->EnableWindow(FALSE);
        GetDlgItem(IDC_CHECK_CONFERENCE_1)->EnableWindow(FALSE);
        GetDlgItem(IDC_CHECK_ON_HOLD_1)->EnableWindow(FALSE);
        GetDlgItem(IDC_CHECK_DELAY_ESTIMATE_1)->EnableWindow(FALSE);
        GetDlgItem(IDC_CHECK_AGC_1)->EnableWindow(FALSE);
        GetDlgItem(IDC_CHECK_NS_1)->EnableWindow(FALSE);
        GetDlgItem(IDC_CHECK_RXVAD)->EnableWindow(FALSE);
        GetDlgItem(IDC_CHECK_FEC)->EnableWindow(FALSE);
        SetDlgItemText(IDC_EDIT_RXVAD, _T(""));
        GetDlgItem(IDC_EDIT_RXVAD)->EnableWindow(FALSE);
        CButton* button = (CButton*)GetDlgItem(IDC_CHECK_EXT_TRANS_1);
        button->SetCheck(BST_UNCHECKED);
        button = (CButton*)GetDlgItem(IDC_CHECK_PLAY_FILE_IN_1);
        button->SetCheck(BST_UNCHECKED);
        button = (CButton*)GetDlgItem(IDC_CHECK_PLAY_FILE_OUT_1);
        button->SetCheck(BST_UNCHECKED);
        button = (CButton*)GetDlgItem(IDC_CHECK_EXT_MEDIA_IN_1);
        button->SetCheck(BST_UNCHECKED);
        button = (CButton*)GetDlgItem(IDC_CHECK_EXT_MEDIA_OUT_1);
        button->SetCheck(BST_UNCHECKED);
        button = (CButton*)GetDlgItem(IDC_CHECK_VAD_1);
        button->SetCheck(BST_UNCHECKED);
        button = (CButton*)GetDlgItem(IDC_CHECK_MUTE_IN_1);
        button->SetCheck(BST_UNCHECKED);
        button = (CButton*)GetDlgItem(IDC_CHECK_SRTP_TX_1);
        button->SetCheck(BST_UNCHECKED);
        button = (CButton*)GetDlgItem(IDC_CHECK_SRTP_RX_1);
        button->SetCheck(BST_UNCHECKED);
        button = (CButton*)GetDlgItem(IDC_CHECK_EXT_ENCRYPTION_1);
        button->SetCheck(BST_UNCHECKED);
        button = (CButton*)GetDlgItem(IDC_CHECK_CONFERENCE_1);
        button->SetCheck(BST_UNCHECKED);
        button = (CButton*)GetDlgItem(IDC_CHECK_ON_HOLD_1);
        button->SetCheck(BST_UNCHECKED);
        button = (CButton*)GetDlgItem(IDC_CHECK_DELAY_ESTIMATE_1);
        button->SetCheck(BST_UNCHECKED);
        button = (CButton*)GetDlgItem(IDC_CHECK_AGC_1);
        button->SetCheck(BST_UNCHECKED);
        button = (CButton*)GetDlgItem(IDC_CHECK_NS_1);
        button->SetCheck(BST_UNCHECKED);
        button = (CButton*)GetDlgItem(IDC_CHECK_RXVAD);
        button->SetCheck(BST_UNCHECKED);
        button = (CButton*)GetDlgItem(IDC_CHECK_FEC);
        button->SetCheck(BST_UNCHECKED);
    }
}

void CWinTestDlg::OnBnClickedButtonDelete2()
{
    BOOL ret;
    int channel = GetDlgItemInt(IDC_EDIT_2, &ret);
    if (ret == TRUE)
    {
        _delayEstimate2 = false;
        _veRtpRtcpPtr->DeRegisterRTPObserver(channel);
        TEST(_veBasePtr->DeleteChannel(channel) == 0, _T("DeleteChannel(%d)"), channel);
        SetDlgItemText(IDC_EDIT_2, _T(""));
        GetDlgItem(IDC_BUTTON_CREATE_2)->EnableWindow(TRUE);
        GetDlgItem(IDC_BUTTON_DELETE_2)->EnableWindow(FALSE);
        GetDlgItem(IDC_COMBO_IP_2)->EnableWindow(FALSE);
        GetDlgItem(IDC_EDIT_TX_PORT_2)->EnableWindow(FALSE);
        GetDlgItem(IDC_EDIT_RX_PORT_2)->EnableWindow(FALSE);
        GetDlgItem(IDC_COMBO_CODEC_2)->EnableWindow(FALSE);
        GetDlgItem(IDC_BUTTON_START_LISTEN_2)->EnableWindow(FALSE);
        GetDlgItem(IDC_BUTTON_START_PLAYOUT_2)->EnableWindow(FALSE);
        GetDlgItem(IDC_BUTTON_START_SEND_2)->EnableWindow(FALSE);
        GetDlgItem(IDC_BUTTON_STOP_LISTEN_2)->EnableWindow(FALSE);
        GetDlgItem(IDC_BUTTON_STOP_PLAYOUT_2)->EnableWindow(FALSE);
        GetDlgItem(IDC_BUTTON_STOP_SEND_2)->EnableWindow(FALSE);
        GetDlgItem(IDC_CHECK_EXT_TRANS_2)->EnableWindow(FALSE);
        GetDlgItem(IDC_CHECK_PLAY_FILE_IN_2)->EnableWindow(FALSE);
        GetDlgItem(IDC_CHECK_PLAY_FILE_OUT_2)->EnableWindow(FALSE);
        GetDlgItem(IDC_CHECK_EXT_MEDIA_IN_2)->EnableWindow(FALSE);
        GetDlgItem(IDC_CHECK_EXT_MEDIA_OUT_2)->EnableWindow(FALSE);
        GetDlgItem(IDC_CHECK_MUTE_IN_2)->EnableWindow(FALSE);
        GetDlgItem(IDC_CHECK_VAD_3)->EnableWindow(FALSE);
        GetDlgItem(IDC_CHECK_SRTP_TX_2)->EnableWindow(FALSE);
        GetDlgItem(IDC_CHECK_SRTP_RX_2)->EnableWindow(FALSE);
        GetDlgItem(IDC_CHECK_EXT_ENCRYPTION_2)->EnableWindow(FALSE);
        GetDlgItem(IDC_CHECK_CONFERENCE_2)->EnableWindow(FALSE);
        GetDlgItem(IDC_BUTTON_DTMF_2)->EnableWindow(FALSE);
        GetDlgItem(IDC_CHECK_ON_HOLD_2)->EnableWindow(FALSE);
        CButton* button = (CButton*)GetDlgItem(IDC_CHECK_EXT_TRANS_2);
        button->SetCheck(BST_UNCHECKED);
        button = (CButton*)GetDlgItem(IDC_CHECK_PLAY_FILE_IN_2);
        button->SetCheck(BST_UNCHECKED);
        button = (CButton*)GetDlgItem(IDC_CHECK_PLAY_FILE_OUT_2);
        button->SetCheck(BST_UNCHECKED);
        button = (CButton*)GetDlgItem(IDC_CHECK_EXT_MEDIA_IN_2);
        button->SetCheck(BST_UNCHECKED);
        button = (CButton*)GetDlgItem(IDC_CHECK_EXT_MEDIA_OUT_2);
        button->SetCheck(BST_UNCHECKED);
        button = (CButton*)GetDlgItem(IDC_CHECK_VAD_3);
        button->SetCheck(BST_UNCHECKED);
        button = (CButton*)GetDlgItem(IDC_CHECK_MUTE_IN_2);
        button->SetCheck(BST_UNCHECKED);
        button = (CButton*)GetDlgItem(IDC_CHECK_SRTP_TX_2);
        button->SetCheck(BST_UNCHECKED);
        button = (CButton*)GetDlgItem(IDC_CHECK_SRTP_RX_2);
        button->SetCheck(BST_UNCHECKED);
        button = (CButton*)GetDlgItem(IDC_CHECK_EXT_ENCRYPTION_2);
        button->SetCheck(BST_UNCHECKED);
        button = (CButton*)GetDlgItem(IDC_CHECK_CONFERENCE_2);
        button->SetCheck(BST_UNCHECKED);
        button = (CButton*)GetDlgItem(IDC_CHECK_ON_HOLD_2);
        button->SetCheck(BST_UNCHECKED);
    }
}

void CWinTestDlg::OnCbnSelchangeComboIp1()
{
    int channel = GetDlgItemInt(IDC_EDIT_1);
    CString str;
    int port = GetDlgItemInt(IDC_EDIT_TX_PORT_1);
    CComboBox* comboIP = (CComboBox*)GetDlgItem(IDC_COMBO_IP_1);
    int n = comboIP->GetLBTextLen(0);
    comboIP->GetLBText(0, str.GetBuffer(n));
    TEST(_veBasePtr->SetSendDestination(channel, port, TcharToChar(str.GetBuffer(n), -1)) == 0,
        _T("SetSendDestination(channel=%d, port=%d, ip=%s)"), channel, port, str.GetBuffer(n));
    str.ReleaseBuffer();
}

void CWinTestDlg::OnCbnSelchangeComboIp2()
{
    int channel = GetDlgItemInt(IDC_EDIT_2);
    CString str;
    int port = GetDlgItemInt(IDC_EDIT_TX_PORT_2);
    CComboBox* comboIP = (CComboBox*)GetDlgItem(IDC_COMBO_IP_2);
    int n = comboIP->GetLBTextLen(0);
    comboIP->GetLBText(0, str.GetBuffer(n));
    TEST(_veBasePtr->SetSendDestination(channel, port, TcharToChar(str.GetBuffer(n), -1)) == 0,
        _T("SetSendDestination(channel=%d, port=%d, ip=%s)"), channel, port, str.GetBuffer(n));
    str.ReleaseBuffer();
}

void CWinTestDlg::OnCbnSelchangeComboCodec1()
{
    int channel = GetDlgItemInt(IDC_EDIT_1);

    CodecInst codec;
    CComboBox* comboCodec(NULL);
    comboCodec = (CComboBox*)GetDlgItem(IDC_COMBO_CODEC_1);
    int index = comboCodec->GetCurSel();
    _veCodecPtr->GetCodec(index, codec);
    if (strncmp(codec.plname, "ISAC", 4) == 0)
    {
        // Set iSAC to adaptive mode by default.
        codec.rate = -1;
    }
    TEST(_veCodecPtr->SetSendCodec(channel, codec) == 0,
        _T("SetSendCodec(channel=%d, plname=%s, pltype=%d, plfreq=%d, rate=%d, pacsize=%d, channels=%d)"),
        channel, CharToTchar(codec.plname, 32), codec.pltype, codec.plfreq, codec.rate, codec.pacsize, codec.channels);

    CListBox* list = (CListBox*)GetDlgItem(IDC_LIST_CODEC_1);
    list->SetCurSel(0);
    SetDlgItemInt(IDC_EDIT_CODEC_1, codec.pltype);
}

void CWinTestDlg::OnLbnSelchangeListCodec1()
{
    int channel = GetDlgItemInt(IDC_EDIT_1);

    CListBox* list = (CListBox*)GetDlgItem(IDC_LIST_CODEC_1);
    int listIdx = list->GetCurSel();
    if (listIdx < 0)
        return;
    CString str;
    list->GetText(listIdx, str);

    CodecInst codec;
    _veCodecPtr->GetSendCodec(channel, codec);

    int value = GetDlgItemInt(IDC_EDIT_CODEC_1);
    if (str == _T("pltype"))
    {
        codec.pltype = value;
    }
    else if (str == _T("plfreq"))
    {
        codec.plfreq = value;
    }
    else if (str == _T("pacsize"))
    {
        codec.pacsize = value;
    }
    else if (str == _T("channels"))
    {
        codec.channels = value;
    }
    else if (str == _T("rate"))
    {
        codec.rate = value;
    }
    TEST(_veCodecPtr->SetSendCodec(channel, codec) == 0,
        _T("SetSendCodec(channel=%d, plname=%s, pltype=%d, plfreq=%d, rate=%d, pacsize=%d, channels=%d)"),
        channel, CharToTchar(codec.plname, 32), codec.pltype, codec.plfreq, codec.rate, codec.pacsize, codec.channels);
}

void CWinTestDlg::OnCbnSelchangeComboCodec2()
{
    int channel = GetDlgItemInt(IDC_EDIT_2);

    CodecInst codec;
    CComboBox* comboCodec(NULL);
    comboCodec = (CComboBox*)GetDlgItem(IDC_COMBO_CODEC_2);
    int index = comboCodec->GetCurSel();
    _veCodecPtr->GetCodec(index, codec);
    TEST(_veCodecPtr->SetSendCodec(channel, codec) == 0,
        _T("SetSendCodec(channel=%d, plname=%s, pltype=%d, plfreq=%d, rate=%d, pacsize=%d, channels=%d)"),
        channel, CharToTchar(codec.plname, 32), codec.pltype, codec.plfreq, codec.rate, codec.pacsize, codec.channels);
}

void CWinTestDlg::OnBnClickedButtonStartListen1()
{
    int ret1(0);
    int ret2(0);
    int channel = GetDlgItemInt(IDC_EDIT_1);
    int port = GetDlgItemInt(IDC_EDIT_RX_PORT_1);
    TEST((ret1 = _veBasePtr->SetLocalReceiver(channel, port)) == 0, _T("SetLocalReceiver(channel=%d, port=%d)"), channel, port);
    TEST((ret2 = _veBasePtr->StartReceive(channel)) == 0, _T("StartReceive(channel=%d)"), channel);
    if (ret1 == 0 && ret2 == 0)
    {
        GetDlgItem(IDC_BUTTON_START_LISTEN_1)->EnableWindow(FALSE);
        GetDlgItem(IDC_BUTTON_STOP_LISTEN_1)->EnableWindow(TRUE);
    }
}

void CWinTestDlg::OnBnClickedButtonStartListen2()
{
    int ret1(0);
    int ret2(0);
    int channel = GetDlgItemInt(IDC_EDIT_2);
    int port = GetDlgItemInt(IDC_EDIT_RX_PORT_2);
    TEST((ret1 = _veBasePtr->SetLocalReceiver(channel, port)) == 0, _T("SetLocalReceiver(channel=%d, port=%d)"), channel, port);
    TEST((ret2 = _veBasePtr->StartReceive(channel)) == 0, _T("StartReceive(channel=%d)"), channel);
    if (ret1 == 0 && ret2 == 0)
    {
        GetDlgItem(IDC_BUTTON_START_LISTEN_2)->EnableWindow(FALSE);
        GetDlgItem(IDC_BUTTON_STOP_LISTEN_2)->EnableWindow(TRUE);
    }
}

void CWinTestDlg::OnBnClickedButtonStopListen1()
{
    int ret(0);
    int channel = GetDlgItemInt(IDC_EDIT_1);
    TEST((ret = _veBasePtr->StopReceive(channel)) == 0, _T("StopListen(channel=%d)"), channel);
    if (ret == 0)
    {
        GetDlgItem(IDC_BUTTON_START_LISTEN_1)->EnableWindow(TRUE);
        GetDlgItem(IDC_BUTTON_STOP_LISTEN_1)->EnableWindow(FALSE);
    }
}

void CWinTestDlg::OnBnClickedButtonStopListen2()
{
    int ret(0);
    int channel = GetDlgItemInt(IDC_EDIT_2);
    TEST((ret = _veBasePtr->StopReceive(channel)) == 0, _T("StopListen(channel=%d)"), channel);
    if (ret == 0)
    {
        GetDlgItem(IDC_BUTTON_START_LISTEN_2)->EnableWindow(TRUE);
        GetDlgItem(IDC_BUTTON_STOP_LISTEN_2)->EnableWindow(FALSE);
    }
}

void CWinTestDlg::OnBnClickedButtonStartPlayout1()
{
    int ret(0);
    int channel = GetDlgItemInt(IDC_EDIT_1);
    TEST((ret = _veBasePtr->StartPlayout(channel)) == 0, _T("StartPlayout(channel=%d)"), channel);
    if (ret == 0)
    {
        GetDlgItem(IDC_BUTTON_START_PLAYOUT_1)->EnableWindow(FALSE);
        GetDlgItem(IDC_BUTTON_STOP_PLAYOUT_1)->EnableWindow(TRUE);
    }
}

void CWinTestDlg::OnBnClickedButtonStartPlayout2()
{
    int ret(0);
    int channel = GetDlgItemInt(IDC_EDIT_2);
    TEST((ret = _veBasePtr->StartPlayout(channel)) == 0, _T("StartPlayout(channel=%d)"), channel);
    if (ret == 0)
    {
        GetDlgItem(IDC_BUTTON_START_PLAYOUT_2)->EnableWindow(FALSE);
        GetDlgItem(IDC_BUTTON_STOP_PLAYOUT_2)->EnableWindow(TRUE);
    }
}

void CWinTestDlg::OnBnClickedButtonStopPlayout1()
{
    int ret(0);
    int channel = GetDlgItemInt(IDC_EDIT_1);
    TEST((ret = _veBasePtr->StopPlayout(channel)) == 0, _T("StopPlayout(channel=%d)"), channel);
    if (ret == 0)
    {
        GetDlgItem(IDC_BUTTON_START_PLAYOUT_1)->EnableWindow(TRUE);
        GetDlgItem(IDC_BUTTON_STOP_PLAYOUT_1)->EnableWindow(FALSE);
    }
}

void CWinTestDlg::OnBnClickedButtonStopPlayout2()
{
    int ret(0);
    int channel = GetDlgItemInt(IDC_EDIT_2);
    TEST((ret = _veBasePtr->StopPlayout(channel)) == 0, _T("StopPlayout(channel=%d)"));
    if (ret == 0)
    {
        GetDlgItem(IDC_BUTTON_START_PLAYOUT_2)->EnableWindow(TRUE);
        GetDlgItem(IDC_BUTTON_STOP_PLAYOUT_2)->EnableWindow(FALSE);
    }
}

void CWinTestDlg::OnBnClickedButtonStartSend1()
{
    UpdateData(TRUE);  // update IP address

    int ret(0);
    int channel = GetDlgItemInt(IDC_EDIT_1);
    if (!_externalTransport)
    {
        CString str;
        int port = GetDlgItemInt(IDC_EDIT_TX_PORT_1);
    TEST(_veBasePtr->SetSendDestination(channel, port, TcharToChar(_strComboIp1.GetBuffer(7), -1)) == 0,
      _T("SetSendDestination(channel=%d, port=%d, ip=%s)"), channel, port, _strComboIp1.GetBuffer(7));
        str.ReleaseBuffer();
    }

	//_veVideoSyncPtr->SetInitTimestamp(0,0);
    // OnCbnSelchangeComboCodec1();

    TEST((ret = _veBasePtr->StartSend(channel)) == 0, _T("StartSend(channel=%d)"), channel);
    if (ret == 0)
    {
        GetDlgItem(IDC_BUTTON_START_SEND_1)->EnableWindow(FALSE);
        GetDlgItem(IDC_BUTTON_STOP_SEND_1)->EnableWindow(TRUE);
    }
}

void CWinTestDlg::OnBnClickedButtonStartSend2()
{
    UpdateData(TRUE);  // update IP address

    int ret(0);
    int channel = GetDlgItemInt(IDC_EDIT_2);
    if (!_externalTransport)
    {
        CString str;
        int port = GetDlgItemInt(IDC_EDIT_TX_PORT_2);
        TEST(_veBasePtr->SetSendDestination(channel, port, TcharToChar(_strComboIp2.GetBuffer(7), -1)) == 0,
            _T("SetSendDestination(channel=%d, port=%d, ip=%s)"), channel, port, _strComboIp2.GetBuffer(7));
        str.ReleaseBuffer();
    }

    // OnCbnSelchangeComboCodec2();

    TEST((ret = _veBasePtr->StartSend(channel)) == 0, _T("StartSend(channel=%d)"), channel);
    if (ret == 0)
    {
        GetDlgItem(IDC_BUTTON_START_SEND_2)->EnableWindow(FALSE);
        GetDlgItem(IDC_BUTTON_STOP_SEND_2)->EnableWindow(TRUE);
    }
}

void CWinTestDlg::OnBnClickedButtonStopSend1()
{
    int ret(0);
    int channel = GetDlgItemInt(IDC_EDIT_1);
    TEST((ret = _veBasePtr->StopSend(channel)) == 0, _T("StopSend(channel=%d)"), channel);
    if (ret == 0)
    {
        GetDlgItem(IDC_BUTTON_START_SEND_1)->EnableWindow(TRUE);
        GetDlgItem(IDC_BUTTON_STOP_SEND_1)->EnableWindow(FALSE);
    }
}

void CWinTestDlg::OnBnClickedButtonStopSend2()
{
    int ret(0);
    int channel = GetDlgItemInt(IDC_EDIT_2);
    TEST((ret = _veBasePtr->StopSend(channel)) == 0, _T("StopSend(channel=%d)"), channel);
    if (ret == 0)
    {
        GetDlgItem(IDC_BUTTON_START_SEND_2)->EnableWindow(TRUE);
        GetDlgItem(IDC_BUTTON_STOP_SEND_2)->EnableWindow(FALSE);
    }
}

void CWinTestDlg::OnBnClickedCheckExtTrans1()
{
    int ret(0);
    int channel = GetDlgItemInt(IDC_EDIT_1);
    CButton* button = (CButton*)GetDlgItem(IDC_CHECK_EXT_TRANS_1);
    int check = button->GetCheck();
    const bool enable = (check == BST_CHECKED);
    if (enable)
    {
        TEST((ret = _veNetworkPtr->RegisterExternalTransport(channel, *_transportPtr)) == 0,
            _T("RegisterExternalTransport(channel=%d, transport=0x%x)"), channel, _transportPtr);
    }
    else
    {
        TEST((ret = _veNetworkPtr->DeRegisterExternalTransport(channel)) == 0,
            _T("DeRegisterExternalTransport(channel=%d)"), channel);
    }
    if (ret == 0)
    {
        _externalTransport = enable;
    }
    else
    {
        // restore inital state since API call failed
        button->SetCheck((check == BST_CHECKED) ? BST_UNCHECKED : BST_CHECKED);
    }
}

void CWinTestDlg::OnBnClickedCheckExtTrans2()
{
    int ret(0);
    int channel = GetDlgItemInt(IDC_EDIT_2);
    CButton* button = (CButton*)GetDlgItem(IDC_CHECK_EXT_TRANS_2);
    int check = button->GetCheck();
    const bool enable = (check == BST_CHECKED);
    if (enable)
    {
        TEST((ret = _veNetworkPtr->RegisterExternalTransport(channel, *_transportPtr)) == 0,
            _T("RegisterExternalTransport(channel=%d, transport=0x%x)"), channel, _transportPtr);
    }
    else
    {
        TEST((ret = _veNetworkPtr->DeRegisterExternalTransport(channel)) == 0,
            _T("DeRegisterExternalTransport(channel=%d)"), channel);
    }
    if (ret == 0)
    {
        _externalTransport = enable;
    }
    else
    {
        // restore inital state since API call failed
        button->SetCheck((check == BST_CHECKED) ? BST_UNCHECKED : BST_CHECKED);
    }
}

void CWinTestDlg::OnBnClickedCheckPlayFileIn1()
{
    std::string micFile = _long_audio_file_path + "audio_short16.pcm";

    int ret(0);
    int channel = GetDlgItemInt(IDC_EDIT_1);
    CButton* button = (CButton*)GetDlgItem(IDC_CHECK_PLAY_FILE_IN_1);
    int check = button->GetCheck();
    const bool enable = (check == BST_CHECKED);
    if (enable)
    {
        bool mix;
        const bool loop(true);
        const FileFormats format = kFileFormatPcm16kHzFile;
        const float scale(1.0);

        (_checkPlayFileIn1 %2 == 0) ? mix = true : mix = false;
        TEST((ret = _veFilePtr->StartPlayingFileAsMicrophone(channel,
            micFile.c_str(), loop, mix, format, scale) == 0),
            _T("StartPlayingFileAsMicrophone(channel=%d, file=%s, loop=%d, ")
            _T("mix=%d, format=%d, scale=%2.1f)"),
            channel, CharToTchar(micFile.c_str(), -1),
            loop, mix, format, scale);
        _checkPlayFileIn1++;
    }
    else
    {
        TEST((ret = _veFilePtr->StopPlayingFileAsMicrophone(channel) == 0),
            _T("StopPlayingFileAsMicrophone(channel=%d)"), channel);
    }
    if (ret == -1)
    {
        // restore inital state since API call failed
        button->SetCheck((check == BST_CHECKED) ? BST_UNCHECKED : BST_CHECKED);
    }
}

void CWinTestDlg::OnBnClickedCheckPlayFileIn2()
{
    std::string micFile = _long_audio_file_path + "audio_long16.pcm";

    int ret(0);
    int channel = GetDlgItemInt(IDC_EDIT_2);
    CButton* button = (CButton*)GetDlgItem(IDC_CHECK_PLAY_FILE_IN_2);
    int check = button->GetCheck();
    const bool enable = (check == BST_CHECKED);
    if (enable)
    {
        bool mix;
        const bool loop(true);
        const FileFormats format = kFileFormatPcm16kHzFile;
        const float scale(1.0);

        (_checkPlayFileIn2 %2 == 0) ? mix = true : mix = false;
        TEST((ret = _veFilePtr->StartPlayingFileAsMicrophone(channel,
            micFile.c_str(), loop, mix, format, scale) == 0),
            _T("StartPlayingFileAsMicrophone(channel=%d, file=%s, loop=%d, ")
            _T("mix=%d, format=%d, scale=%2.1f)"),
            channel, CharToTchar(micFile.c_str(), -1),
            loop, mix, format, scale);
        _checkPlayFileIn2++;
    }
    else
    {
        TEST((ret = _veFilePtr->StopPlayingFileAsMicrophone(channel) == 0),
            _T("StopPlayingFileAsMicrophone(channel=%d)"), channel);
    }
    if (ret == -1)
    {
        // restore inital state since API call failed
        button->SetCheck((check == BST_CHECKED) ? BST_UNCHECKED : BST_CHECKED);
    }
}

void CWinTestDlg::OnBnClickedCheckPlayFileOut1()
{
    const FileFormats formats[8]  = {{kFileFormatPcm16kHzFile},
                                          {kFileFormatWavFile},
                                          {kFileFormatWavFile},
                                          {kFileFormatWavFile},
                                          {kFileFormatWavFile},
                                          {kFileFormatWavFile},
                                          {kFileFormatWavFile},
                                          {kFileFormatWavFile}};
    // File path is relative to the location of 'voice_engine.gyp'.
    const char spkrFiles[8][64] = {{"audio_short16.pcm"},
                                   {"audio_tiny8.wav"},
                                   {"audio_tiny11.wav"},
                                   {"audio_tiny16.wav"},
                                   {"audio_tiny22.wav"},
                                   {"audio_tiny32.wav"},
                                   {"audio_tiny44.wav"},
                                   {"audio_tiny48.wav"}};
    int ret(0);
    int channel = GetDlgItemInt(IDC_EDIT_1);
    CButton* button = (CButton*)GetDlgItem(IDC_CHECK_PLAY_FILE_OUT_1);
    int check = button->GetCheck();
    const bool enable = (check == BST_CHECKED);
    if (enable)
    {
        const bool loop(true);
        const float volumeScaling(1.0);
        const int startPointMs(0);
        const int stopPointMs(0);
        const FileFormats format = formats[_checkPlayFileOut1 % 8];
        std::string spkrFile = _long_audio_file_path +
                               spkrFiles[_checkPlayFileOut1 % 8];

        CString str;
        if (_checkPlayFileOut1 % 8 == 0)
        {
            str = _T("kFileFormatPcm16kHzFile");
        }
        else
        {
            str = _T("kFileFormatWavFile");
        }
        // (_checkPlayFileOut1 %2 == 0) ? mix = true : mix = false;
        TEST((ret = _veFilePtr->StartPlayingFileLocally(channel,
            spkrFile.c_str(), loop, format, volumeScaling,
            startPointMs,stopPointMs) == 0),
            _T("StartPlayingFileLocally(channel=%d, file=%s, loop=%d, ")
            _T("format=%s, scale=%2.1f, start=%d, stop=%d)"),
            channel, CharToTchar(spkrFile.c_str(), -1),
            loop, str, volumeScaling, startPointMs, stopPointMs);
        _checkPlayFileOut1++;
    }
    else
    {
        TEST((ret = _veFilePtr->StopPlayingFileLocally(channel) == 0),
            _T("StopPlayingFileLocally(channel=%d)"), channel);
    }
    if (ret == -1)
    {
        // restore inital state since API call failed
        button->SetCheck((check == BST_CHECKED) ? BST_UNCHECKED : BST_CHECKED);
    }
}

void CWinTestDlg::OnBnClickedCheckPlayFileOut2()
{
    std::string spkrFile = _long_audio_file_path + "audio_long16.pcm";

    int ret(0);
    int channel = GetDlgItemInt(IDC_EDIT_2);
    CButton* button = (CButton*)GetDlgItem(IDC_CHECK_PLAY_FILE_OUT_2);
    int check = button->GetCheck();
    const bool enable = (check == BST_CHECKED);
    if (enable)
    {
        const bool loop(true);
        const FileFormats format = kFileFormatPcm16kHzFile;
        const float volumeScaling(1.0);
        const int startPointMs(0);
        const int stopPointMs(0);

        // (_checkPlayFileOut2 %2 == 0) ? mix = true : mix = false;
        TEST((ret = _veFilePtr->StartPlayingFileLocally(channel,
            spkrFile.c_str(), loop, format, volumeScaling,
            startPointMs,stopPointMs) == 0),
            _T("StartPlayingFileLocally(channel=%d, file=%s, loop=%d, ")
            _T("format=%d, scale=%2.1f, start=%d, stop=%d)"),
            channel, CharToTchar(spkrFile.c_str(), -1),
            loop, format, volumeScaling, startPointMs, stopPointMs);
        // _checkPlayFileIn2++;
    }
    else
    {
        TEST((ret = _veFilePtr->StopPlayingFileLocally(channel) == 0),
            _T("StopPlayingFileLocally(channel=%d)"), channel);
    }
    if (ret == -1)
    {
        // restore inital state since API call failed
        button->SetCheck((check == BST_CHECKED) ? BST_UNCHECKED : BST_CHECKED);
    }
}

void CWinTestDlg::OnBnClickedCheckExtMediaIn1()
{
    int channel = GetDlgItemInt(IDC_EDIT_1);
    CButton* buttonExtTrans = (CButton*)GetDlgItem(IDC_CHECK_EXT_MEDIA_IN_1);
    int check = buttonExtTrans->GetCheck();
    const bool enable = (check == BST_CHECKED);
    if (enable)
    {
        TEST(_veExternalMediaPtr->RegisterExternalMediaProcessing(channel, kRecordingPerChannel, *_externalMediaPtr) == 0,
            _T("RegisterExternalMediaProcessing(channel=%d, kRecordingPerChannel, processObject=0x%x)"), channel, _externalMediaPtr);
    }
    else
    {
        TEST(_veExternalMediaPtr->DeRegisterExternalMediaProcessing(channel, kRecordingPerChannel) == 0,
            _T("DeRegisterExternalMediaProcessing(channel=%d, kRecordingPerChannel)"), channel);
    }
}

void CWinTestDlg::OnBnClickedCheckExtMediaIn2()
{
    int channel = GetDlgItemInt(IDC_EDIT_2);
    CButton* buttonExtTrans = (CButton*)GetDlgItem(IDC_CHECK_EXT_MEDIA_IN_2);
    int check = buttonExtTrans->GetCheck();
    const bool enable = (check == BST_CHECKED);
    if (enable)
    {
        TEST(_veExternalMediaPtr->RegisterExternalMediaProcessing(channel, kRecordingPerChannel, *_externalMediaPtr) == 0,
            _T("RegisterExternalMediaProcessing(channel=%d, kRecordingPerChannel, processObject=0x%x)"), channel, _externalMediaPtr);
    }
    else
    {
        TEST(_veExternalMediaPtr->DeRegisterExternalMediaProcessing(channel, kRecordingPerChannel) == 0,
            _T("DeRegisterExternalMediaProcessing(channel=%d, kRecordingPerChannel)"), channel);
    }
}

void CWinTestDlg::OnBnClickedCheckExtMediaOut1()
{
    int channel = GetDlgItemInt(IDC_EDIT_1);
    CButton* buttonExtTrans = (CButton*)GetDlgItem(IDC_CHECK_EXT_MEDIA_OUT_1);
    int check = buttonExtTrans->GetCheck();
    const bool enable = (check == BST_CHECKED);
    if (enable)
    {
        TEST(_veExternalMediaPtr->RegisterExternalMediaProcessing(channel, kPlaybackPerChannel, *_externalMediaPtr) == 0,
            _T("RegisterExternalMediaProcessing(channel=%d, kPlaybackPerChannel, processObject=0x%x)"), channel, _externalMediaPtr);
    }
    else
    {
        TEST(_veExternalMediaPtr->DeRegisterExternalMediaProcessing(channel, kPlaybackPerChannel) == 0,
            _T("DeRegisterExternalMediaProcessing(channel=%d, kPlaybackPerChannel)"), channel);
    }
}

void CWinTestDlg::OnBnClickedCheckExtMediaOut2()
{
    int channel = GetDlgItemInt(IDC_EDIT_2);
    CButton* buttonExtTrans = (CButton*)GetDlgItem(IDC_CHECK_EXT_MEDIA_OUT_2);
    int check = buttonExtTrans->GetCheck();
    const bool enable = (check == BST_CHECKED);
    if (enable)
    {
        TEST(_veExternalMediaPtr->RegisterExternalMediaProcessing(channel, kPlaybackPerChannel, *_externalMediaPtr) == 0,
            _T("RegisterExternalMediaProcessing(channel=%d, kPlaybackPerChannel, processObject=0x%x)"), channel, _externalMediaPtr);
    }
    else
    {
        TEST(_veExternalMediaPtr->DeRegisterExternalMediaProcessing(channel, kPlaybackPerChannel) == 0,
            _T("DeRegisterExternalMediaProcessing(channel=%d, kPlaybackPerChannel)"), channel);
    }
}

void CWinTestDlg::OnBnClickedCheckVad1()
{
    int ret(0);
    int channel = GetDlgItemInt(IDC_EDIT_1);
    CButton* button = (CButton*)GetDlgItem(IDC_CHECK_VAD_1);
    int check = button->GetCheck();
    const bool enable = (check == BST_CHECKED);
    if (enable)
    {
        CString str;
        VadModes mode(kVadConventional);
        if (_checkVAD1 % 4 == 0)
        {
            mode = kVadConventional;
            str = _T("kVadConventional");
        }
        else if (_checkVAD1 % 4 == 1)
        {
            mode = kVadAggressiveLow;
            str = _T("kVadAggressiveLow");
        }
        else if (_checkVAD1 % 4 == 2)
        {
            mode = kVadAggressiveMid;
            str = _T("kVadAggressiveMid");
        }
        else if (_checkVAD1 % 4 == 3)
        {
            mode = kVadAggressiveHigh;
            str = _T("kVadAggressiveHigh");
        }
        const bool disableDTX(false);
        TEST((ret = _veCodecPtr->SetVADStatus(channel, true, mode, disableDTX) == 0),
            _T("SetVADStatus(channel=%d, enable=%d, mode=%s, disableDTX=%d)"), channel, enable, str, disableDTX);
        _checkVAD1++;
    }
    else
    {
        TEST((ret = _veCodecPtr->SetVADStatus(channel, false)) == 0, _T("SetVADStatus(channel=%d, enable=%d)"), channel, false);
    }
    if (ret == -1)
    {
        // restore inital state since API call failed
        button->SetCheck((check == BST_CHECKED) ? BST_UNCHECKED : BST_CHECKED);
    }
}

void CWinTestDlg::OnBnClickedCheckVad2()
{
    int ret(0);
    int channel = GetDlgItemInt(IDC_EDIT_2);
    CButton* button = (CButton*)GetDlgItem(IDC_CHECK_VAD_2);
    int check = button->GetCheck();
    const bool enable = (check == BST_CHECKED);
    if (enable)
    {
        CString str;
        VadModes mode(kVadConventional);
        if (_checkVAD2 % 4 == 0)
        {
            mode = kVadConventional;
            str = _T("kVadConventional");
        }
        else if (_checkVAD2 % 4 == 1)
        {
            mode = kVadAggressiveLow;
            str = _T("kVadAggressiveLow");
        }
        else if (_checkVAD2 % 4 == 2)
        {
            mode = kVadAggressiveMid;
            str = _T("kVadAggressiveMid");
        }
        else if (_checkVAD2 % 4 == 3)
        {
            mode = kVadAggressiveHigh;
            str = _T("kVadAggressiveHigh");
        }
        const bool disableDTX(false);
        TEST((ret = _veCodecPtr->SetVADStatus(channel, true, mode, disableDTX)) == 0,
            _T("SetVADStatus(channel=%d, enable=%d, mode=%s, disableDTX=%d)"), channel, enable, str, disableDTX);
        _checkVAD2++;
    }
    else
    {
        TEST((ret = _veCodecPtr->SetVADStatus(channel, false) == 0), _T("SetVADStatus(channel=%d, enable=%d)"), channel, false);
    }
    if (ret == -1)
    {
        // restore inital state since API call failed
        button->SetCheck((check == BST_CHECKED) ? BST_UNCHECKED : BST_CHECKED);
    }
}

void CWinTestDlg::OnBnClickedCheckMuteIn1()
{
    int channel = GetDlgItemInt(IDC_EDIT_1);
    CButton* buttonMute = (CButton*)GetDlgItem(IDC_CHECK_MUTE_IN_1);
    int check = buttonMute->GetCheck();
    const bool enable = (check == BST_CHECKED);
    TEST(_veVolumeControlPtr->SetInputMute(channel, enable) == 0,
        _T("SetInputMute(channel=%d, enable=%d)"), channel, enable);
}

void CWinTestDlg::OnBnClickedCheckMuteIn2()
{
    int channel = GetDlgItemInt(IDC_EDIT_2);
    CButton* buttonMute = (CButton*)GetDlgItem(IDC_CHECK_MUTE_IN_2);
    int check = buttonMute->GetCheck();
    const bool enable = (check == BST_CHECKED);
    TEST(_veVolumeControlPtr->SetInputMute(channel, enable) == 0,
        _T("SetInputMute(channel=%d, enable=%d)"), channel, enable);
}

void CWinTestDlg::OnBnClickedCheckSrtpTx1()
{
    int ret(0);
    int channel = GetDlgItemInt(IDC_EDIT_1);
    CButton* button = (CButton*)GetDlgItem(IDC_CHECK_SRTP_TX_1);
    int check = button->GetCheck();
    const bool enable = (check == BST_CHECKED);
    bool useForRTCP = false;
    if (enable)
    {
        (_checkSrtpTx1++ %2 == 0) ? useForRTCP = false : useForRTCP = true;
        // TODO(solenberg): Install SRTP encryption policy.
        TEST(true, "Built-in SRTP support is deprecated. Enable it again by "
            "setting an external encryption policy, i.e.:\n\r"
            "_veEncryptionPtr->RegisterExternalEncryption(channel, myPolicy)");
    }
    else
    {
        // TODO(solenberg): Uninstall SRTP encryption policy, i.e.:
        //   _veEncryptionPtr->DeRegisterExternalEncryption(channel);
    }
    if (ret == -1)
    {
        // restore inital state since API call failed
        button->SetCheck((check == BST_CHECKED) ? BST_UNCHECKED : BST_CHECKED);
    }
}

void CWinTestDlg::OnBnClickedCheckSrtpTx2()
{
    int ret(0);
    int channel = GetDlgItemInt(IDC_EDIT_2);
    CButton* button = (CButton*)GetDlgItem(IDC_CHECK_SRTP_TX_2);
    int check = button->GetCheck();
    const bool enable = (check == BST_CHECKED);
    bool useForRTCP = false;
    if (enable)
    {
        (_checkSrtpTx2++ %2 == 0) ? useForRTCP = false : useForRTCP = true;
        // TODO(solenberg): Install SRTP encryption policy.
        TEST(true, "Built-in SRTP support is deprecated. Enable it again by "
            "setting an external encryption policy, i.e.:\n\r"
            "_veEncryptionPtr->RegisterExternalEncryption(channel, myPolicy)");
    }
    else
    {
        // TODO(solenberg): Uninstall SRTP encryption policy, i.e.:
        //   _veEncryptionPtr->DeRegisterExternalEncryption(channel);
    }
    if (ret == -1)
    {
        // restore inital state since API call failed
        button->SetCheck((check == BST_CHECKED) ? BST_UNCHECKED : BST_CHECKED);
    }
}

void CWinTestDlg::OnBnClickedCheckSrtpRx1()
{
    int ret(0);
    int channel = GetDlgItemInt(IDC_EDIT_1);
    CButton* button = (CButton*)GetDlgItem(IDC_CHECK_SRTP_RX_1);
    int check = button->GetCheck();
    const bool enable = (check == BST_CHECKED);
    bool useForRTCP(false);
    if (enable)
    {
        (_checkSrtpRx1++ %2 == 0) ? useForRTCP = false : useForRTCP = true;
        // TODO(solenberg): Install SRTP encryption policy.
        TEST(true, "Built-in SRTP support is deprecated. Enable it again by "
            "setting an external encryption policy, i.e.:\n\r"
            "_veEncryptionPtr->RegisterExternalEncryption(channel, myPolicy)");
    }
    else
    {
        // TODO(solenberg): Uninstall SRTP encryption policy, i.e.:
        //   _veEncryptionPtr->DeRegisterExternalEncryption(channel);
    }
    if (ret == -1)
    {
        // restore inital state since API call failed
        button->SetCheck((check == BST_CHECKED) ? BST_UNCHECKED : BST_CHECKED);
    }
}

void CWinTestDlg::OnBnClickedCheckSrtpRx2()
{
    int ret(0);
    int channel = GetDlgItemInt(IDC_EDIT_2);
    CButton* button = (CButton*)GetDlgItem(IDC_CHECK_SRTP_RX_2);
    int check = button->GetCheck();
    const bool enable = (check == BST_CHECKED);
    bool useForRTCP(false);
    if (enable)
    {
        (_checkSrtpRx2++ %2 == 0) ? useForRTCP = false : useForRTCP = true;
        // TODO(solenberg): Install SRTP encryption policy.
        TEST(true, "Built-in SRTP support is deprecated. Enable it again by "
            "setting an external encryption policy, i.e.:\n\r"
            "_veEncryptionPtr->RegisterExternalEncryption(channel, myPolicy)");
    }
    else
    {
        // TODO(solenberg): Uninstall SRTP encryption policy, i.e.:
        //   _veEncryptionPtr->DeRegisterExternalEncryption(channel);
    }
    if (ret == -1)
    {
        // restore inital state since API call failed
        button->SetCheck((check == BST_CHECKED) ? BST_UNCHECKED : BST_CHECKED);
    }
}

void CWinTestDlg::OnBnClickedCheckExtEncryption1()
{
    int ret(0);
    int channel = GetDlgItemInt(IDC_EDIT_1);
    CButton* button = (CButton*)GetDlgItem(IDC_CHECK_EXT_ENCRYPTION_1);
    int check = button->GetCheck();
    const bool enable = (check == BST_CHECKED);
    if (enable)
    {
        TEST((ret = _veEncryptionPtr->RegisterExternalEncryption(channel, *_encryptionPtr)) == 0,
            _T("RegisterExternalEncryption(channel=%d, encryption=0x%x)"), channel, _encryptionPtr);
    }
    else
    {
        TEST((ret = _veEncryptionPtr->DeRegisterExternalEncryption(channel)) == 0,
            _T("DeRegisterExternalEncryption(channel=%d)"), channel);
    }
    if (ret == -1)
    {
        // restore inital state since API call failed
        button->SetCheck((check == BST_CHECKED) ? BST_UNCHECKED : BST_CHECKED);
    }
}

void CWinTestDlg::OnBnClickedCheckExtEncryption2()
{
    int ret(0);
    int channel = GetDlgItemInt(IDC_EDIT_2);
    CButton* button = (CButton*)GetDlgItem(IDC_CHECK_EXT_ENCRYPTION_2);
    int check = button->GetCheck();
    const bool enable = (check == BST_CHECKED);
    if (enable)
    {
        TEST((ret = _veEncryptionPtr->RegisterExternalEncryption(channel, *_encryptionPtr)) == 0,
            _T("RegisterExternalEncryption(channel=%d, encryption=0x%x)"), channel, _encryptionPtr);
    }
    else
    {
        TEST((ret = _veEncryptionPtr->DeRegisterExternalEncryption(channel)) == 0,
            _T("DeRegisterExternalEncryption(channel=%d)"), channel);
    }
    if (ret == -1)
    {
        // restore inital state since API call failed
        button->SetCheck((check == BST_CHECKED) ? BST_UNCHECKED : BST_CHECKED);
    }
}

void CWinTestDlg::OnBnClickedButtonDtmf1()
{
    int channel = GetDlgItemInt(IDC_EDIT_1);
    CTelephonyEvent dlgTelephoneEvent(_vePtr, channel, this);
    dlgTelephoneEvent.DoModal();
}

void CWinTestDlg::OnBnClickedButtonDtmf2()
{
    int channel = GetDlgItemInt(IDC_EDIT_2);
    CTelephonyEvent dlgTelephoneEvent(_vePtr, channel, this);
    dlgTelephoneEvent.DoModal();
}

void CWinTestDlg::OnBnClickedCheckConference1()
{
    // Not supported yet
}

void CWinTestDlg::OnBnClickedCheckConference2()
{
   // Not supported yet
}

void CWinTestDlg::OnBnClickedCheckOnHold1()
{
    SHORT shiftKeyIsPressed = ::GetAsyncKeyState(VK_SHIFT);

    CString str;
    int channel = GetDlgItemInt(IDC_EDIT_1);
    CButton* button = (CButton*)GetDlgItem(IDC_CHECK_ON_HOLD_1);
    int check = button->GetCheck();

    if (shiftKeyIsPressed)
    {
        bool enabled(false);
        OnHoldModes mode(kHoldSendAndPlay);
        TEST(_veBasePtr->GetOnHoldStatus(channel, enabled, mode) == 0,
            _T("GetOnHoldStatus(channel=%d, enabled=?, mode=?)"), channel);
        button->SetCheck((check == BST_CHECKED) ? BST_UNCHECKED : BST_CHECKED);

        switch (mode)
        {
        case kHoldSendAndPlay:
            str = _T("kHoldSendAndPlay");
            break;
        case kHoldSendOnly:
            str = _T("kHoldSendOnly");
            break;
        case kHoldPlayOnly:
            str = _T("kHoldPlayOnly");
            break;
        default:
            break;
        }
        PRINT_GET_RESULT(_T("enabled=%d, mode=%s"), enabled, str);
        return;
    }

    int ret(0);
    const bool enable = (check == BST_CHECKED);
    if (enable)
    {
        OnHoldModes mode(kHoldSendAndPlay);
        if (_checkOnHold1 % 3 == 0)
        {
            mode = kHoldSendAndPlay;
            str = _T("kHoldSendAndPlay");
        }
        else if (_checkOnHold1 % 3 == 1)
        {
            mode = kHoldSendOnly;
            str = _T("kHoldSendOnly");
        }
        else if (_checkOnHold1 % 3 == 2)
        {
            mode = kHoldPlayOnly;
            str = _T("kHoldPlayOnly");
        }
        TEST((ret = _veBasePtr->SetOnHoldStatus(channel, enable, mode)) == 0,
            _T("SetOnHoldStatus(channel=%d, enable=%d, mode=%s)"), channel, enable, str);
        _checkOnHold1++;
    }
    else
    {
        TEST((ret = _veBasePtr->SetOnHoldStatus(channel, enable)) == 0,
            _T("SetOnHoldStatus(channel=%d, enable=%d)"), channel, enable);
    }
}

void CWinTestDlg::OnBnClickedCheckOnHold2()
{
    int ret(0);
    int channel = GetDlgItemInt(IDC_EDIT_2);
    CButton* button = (CButton*)GetDlgItem(IDC_CHECK_ON_HOLD_2);
    int check = button->GetCheck();
    const bool enable = (check == BST_CHECKED);
    if (enable)
    {
        CString str;
        OnHoldModes mode(kHoldSendAndPlay);
        if (_checkOnHold1 % 3 == 0)
        {
            mode = kHoldSendAndPlay;
            str = _T("kHoldSendAndPlay");
        }
        else if (_checkOnHold1 % 3 == 1)
        {
            mode = kHoldSendOnly;
            str = _T("kHoldSendOnly");
        }
        else if (_checkOnHold1 % 3 == 2)
        {
            mode = kHoldPlayOnly;
            str = _T("kHoldPlayOnly");
        }
        TEST((ret = _veBasePtr->SetOnHoldStatus(channel, enable, mode)) == 0,
            _T("SetOnHoldStatus(channel=%d, enable=%d, mode=%s)"), channel, enable, str);
        _checkOnHold1++;
    }
    else
    {
        TEST((ret = _veBasePtr->SetOnHoldStatus(channel, enable)) == 0,
            _T("SetOnHoldStatus(channel=%d, enable=%d)"), channel, enable);
    }
}

void CWinTestDlg::OnBnClickedCheckDelayEstimate1()
{
    CButton* button = (CButton*)GetDlgItem(IDC_CHECK_DELAY_ESTIMATE_1);
    int check = button->GetCheck();
    const bool enable = (check == BST_CHECKED);

    if (enable)
    {
        _delayEstimate1 = true;
        SetDlgItemInt(IDC_EDIT_DELAY_ESTIMATE_1, 0);
    }
    else
    {
        _delayEstimate1 = false;
        SetDlgItemText(IDC_EDIT_DELAY_ESTIMATE_1, _T(""));
    }
}

void CWinTestDlg::OnBnClickedCheckRxvad()
{
    int channel = GetDlgItemInt(IDC_EDIT_1);
    CButton* button = (CButton*)GetDlgItem(IDC_CHECK_RXVAD);
    int check = button->GetCheck();
    const bool enable = (check == BST_CHECKED);

    if (enable)
    {
        _rxVad = true;
        _veApmPtr->RegisterRxVadObserver(channel, *_rxVadObserverPtr);
        SetDlgItemInt(IDC_EDIT_RXVAD, 0);
    }
    else
    {
        _rxVad = false;
        _veApmPtr->DeRegisterRxVadObserver(channel);
        SetDlgItemText(IDC_EDIT_RXVAD, _T(""));
    }
}

void CWinTestDlg::OnBnClickedCheckAgc1()
{
    SHORT shiftKeyIsPressed = ::GetAsyncKeyState(VK_SHIFT);

    CString str;
    int channel = GetDlgItemInt(IDC_EDIT_1);
    CButton* button = (CButton*)GetDlgItem(IDC_CHECK_AGC_1);
    int check = button->GetCheck();
    const bool enable = (check == BST_CHECKED);

    if (shiftKeyIsPressed)
    {
        bool enabled(false);
        AgcModes mode(kAgcAdaptiveDigital);
        TEST(_veApmPtr->GetRxAgcStatus(channel, enabled, mode) == 0,
            _T("GetRxAgcStatus(channel=%d, enabled=?, mode=?)"), channel);
        button->SetCheck((check == BST_CHECKED) ? BST_UNCHECKED : BST_CHECKED);

        switch (mode)
        {
        case kAgcAdaptiveAnalog:
            str = _T("kAgcAdaptiveAnalog");
            break;
        case kAgcAdaptiveDigital:
            str = _T("kAgcAdaptiveDigital");
            break;
        case kAgcFixedDigital:
            str = _T("kAgcFixedDigital");
            break;
        default:
            break;
        }
        PRINT_GET_RESULT(_T("enabled=%d, mode=%s"), enabled, str);
        return;
    }

    if (enable)
    {
        CString str;
        AgcModes mode(kAgcDefault);
        if (_checkAGC1 % 3 == 0)
        {
            mode = kAgcDefault;
            str = _T("kAgcDefault");
        }
        else if (_checkAGC1 % 3 == 1)
        {
            mode = kAgcAdaptiveDigital;
            str = _T("kAgcAdaptiveDigital");
        }
        else if (_checkAGC1 % 3 == 2)
        {
            mode = kAgcFixedDigital;
            str = _T("kAgcFixedDigital");
        }
        TEST(_veApmPtr->SetRxAgcStatus(channel, true, mode) == 0, _T("SetRxAgcStatus(channel=%d, enable=%d, %s)"), channel, enable, str);
        _checkAGC1++;
    }
    else
    {
        TEST(_veApmPtr->SetRxAgcStatus(channel, false, kAgcUnchanged) == 0, _T("SetRxAgcStatus(channel=%d, enable=%d)"), channel, enable);
    }
}

void CWinTestDlg::OnBnClickedCheckNs1()
{
    int channel = GetDlgItemInt(IDC_EDIT_1);
    CButton* buttonNS = (CButton*)GetDlgItem(IDC_CHECK_NS_1);
    int check = buttonNS->GetCheck();
    const bool enable = (check == BST_CHECKED);
    if (enable)
    {
        CString str;
        NsModes mode(kNsDefault);
        if (_checkNS1 % 6 == 0)
        {
            mode = kNsDefault;
            str = _T("kNsDefault");
        }
        else if (_checkNS1 % 6 == 1)
        {
            mode = kNsConference;
            str = _T("kNsConference");
        }
        else if (_checkNS1 % 6 == 2)
        {
            mode = kNsLowSuppression;
            str = _T("kNsLowSuppression");
        }
        else if (_checkNS1 % 6 == 3)
        {
            mode = kNsModerateSuppression;
            str = _T("kNsModerateSuppression");
        }
        else if (_checkNS1 % 6 == 4)
        {
            mode = kNsHighSuppression;
            str = _T("kNsHighSuppression");
        }
        else if (_checkNS1 % 6 == 5)
        {
            mode = kNsVeryHighSuppression;
            str = _T("kNsVeryHighSuppression");
        }
        TEST(_veApmPtr->SetRxNsStatus(channel, true, mode) == 0, _T("SetRxNsStatus(channel=%d, enable=%d, %s)"), channel, enable, str);
        _checkNS1++;
    }
    else
    {
        TEST(_veApmPtr->SetRxNsStatus(channel, false, kNsUnchanged) == 0, _T("SetRxNsStatus(channel=%d, enable=%d)"), enable, channel);
    }
}

// ----------------------------------------------------------------------------
//                         Channel-independent Operations
// ----------------------------------------------------------------------------

void CWinTestDlg::OnBnClickedCheckPlayFileIn()
{
    std::string micFile = _long_audio_file_path + "audio_short16.pcm";
    // std::string micFile = _long_audio_file_path + "audio_long16noise.pcm";

    int channel(-1);
    CButton* buttonExtTrans = (CButton*)GetDlgItem(IDC_CHECK_PLAY_FILE_IN);
    int check = buttonExtTrans->GetCheck();
    const bool enable = (check == BST_CHECKED);
    if (enable)
    {
        bool mix;
        const bool loop(true);
        const FileFormats format = kFileFormatPcm16kHzFile;
        const float scale(1.0);

        (_checkPlayFileIn %2 == 0) ? mix = true : mix = false;
        TEST(_veFilePtr->StartPlayingFileAsMicrophone(channel,
            micFile.c_str(), loop, mix, format, scale) == 0,
            _T("StartPlayingFileAsMicrophone(channel=%d, file=%s, ")
            _T("loop=%d, mix=%d, format=%d, scale=%2.1f)"),
            channel, CharToTchar(micFile.c_str(), -1),
            loop, mix, format, scale);
        _checkPlayFileIn++;
    }
    else
    {
        TEST(_veFilePtr->StopPlayingFileAsMicrophone(channel) == 0,
            _T("StopPlayingFileAsMicrophone(channel=%d)"), channel);
    }
}

void CWinTestDlg::OnBnClickedCheckRecMic()
{
    std::string micFile = webrtc::test::OutputPath() +
                          "rec_mic_mono_16kHz.pcm";

    CButton* button = (CButton*)GetDlgItem(IDC_CHECK_REC_MIC);
    int check = button->GetCheck();
    const bool enable = (check == BST_CHECKED);
    if (enable)
    {
        TEST(_veFilePtr->StartRecordingMicrophone(micFile.c_str(), NULL) == 0,
            _T("StartRecordingMicrophone(file=%s)"),
            CharToTchar(micFile.c_str(), -1));
    }
    else
    {
        TEST(_veFilePtr->StopRecordingMicrophone() == 0,
            _T("StopRecordingMicrophone()"));
    }
}

void CWinTestDlg::OnBnClickedCheckAgc()
{
    CButton* buttonAGC = (CButton*)GetDlgItem(IDC_CHECK_AGC);
    int check = buttonAGC->GetCheck();
    const bool enable = (check == BST_CHECKED);
    if (enable)
    {
        CString str;
        AgcModes mode(kAgcDefault);
        if (_checkAGC % 4 == 0)
        {
            mode = kAgcDefault;
            str = _T("kAgcDefault");
        }
        else if (_checkAGC % 4 == 1)
        {
            mode = kAgcAdaptiveAnalog;
            str = _T("kAgcAdaptiveAnalog");
        }
        else if (_checkAGC % 4 == 2)
        {
            mode = kAgcAdaptiveDigital;
            str = _T("kAgcAdaptiveDigital");
        }
        else if (_checkAGC % 4 == 3)
        {
            mode = kAgcFixedDigital;
            str = _T("kAgcFixedDigital");
        }
        TEST(_veApmPtr->SetAgcStatus(true, mode) == 0, _T("SetAgcStatus(enable=%d, %s)"), enable, str);
        _checkAGC++;
    }
    else
    {
        TEST(_veApmPtr->SetAgcStatus(false, kAgcUnchanged) == 0, _T("SetAgcStatus(enable=%d)"), enable);
    }
}

void CWinTestDlg::OnBnClickedCheckNs()
{
    CButton* buttonNS = (CButton*)GetDlgItem(IDC_CHECK_NS);
    int check = buttonNS->GetCheck();
    const bool enable = (check == BST_CHECKED);
    if (enable)
    {
        CString str;
        NsModes mode(kNsDefault);
        if (_checkNS % 6 == 0)
        {
            mode = kNsDefault;
            str = _T("kNsDefault");
        }
        else if (_checkNS % 6 == 1)
        {
            mode = kNsConference;
            str = _T("kNsConference");
        }
        else if (_checkNS % 6 == 2)
        {
            mode = kNsLowSuppression;
            str = _T("kNsLowSuppression");
        }
        else if (_checkNS % 6 == 3)
        {
            mode = kNsModerateSuppression;
            str = _T("kNsModerateSuppression");
        }
        else if (_checkNS % 6 == 4)
        {
            mode = kNsHighSuppression;
            str = _T("kNsHighSuppression");
        }
        else if (_checkNS % 6 == 5)
        {
            mode = kNsVeryHighSuppression;
            str = _T("kNsVeryHighSuppression");
        }
        TEST(_veApmPtr->SetNsStatus(true, mode) == 0, _T("SetNsStatus(enable=%d, %s)"), enable, str);
        _checkNS++;
    }
    else
    {
        TEST(_veApmPtr->SetNsStatus(false, kNsUnchanged) == 0, _T("SetNsStatus(enable=%d)"), enable);
    }
}

void CWinTestDlg::OnBnClickedCheckEc()
{
    CButton* buttonEC = (CButton*)GetDlgItem(IDC_CHECK_EC);
    int check = buttonEC->GetCheck();
    const bool enable = (check == BST_CHECKED);
    if (enable)
    {
        CString str;
        EcModes mode(kEcDefault);
        if (_checkEC % 4 == 0)
        {
            mode = kEcDefault;
            str = _T("kEcDefault");
        }
        else if (_checkEC % 4 == 1)
        {
            mode = kEcConference;
            str = _T("kEcConference");
        }
        else if (_checkEC % 4 == 2)
        {
            mode = kEcAec;
            str = _T("kEcAec");
        }
        else if (_checkEC % 4 == 3)
        {
            mode = kEcAecm;
            str = _T("kEcAecm");
        }
        TEST(_veApmPtr->SetEcStatus(true, mode) == 0, _T("SetEcStatus(enable=%d, %s)"), enable, str);
        _checkEC++;
    }
    else
    {
        TEST(_veApmPtr->SetEcStatus(false, kEcUnchanged) == 0, _T("SetEcStatus(enable=%d)"), enable);
    }
}

void CWinTestDlg::OnBnClickedCheckMuteIn()
{
    CButton* buttonMute = (CButton*)GetDlgItem(IDC_CHECK_MUTE_IN);
    int check = buttonMute->GetCheck();
    const bool enable = (check == BST_CHECKED);
    const int channel(-1);
    TEST(_veVolumeControlPtr->SetInputMute(channel, enable) == 0,
        _T("SetInputMute(channel=%d, enable=%d)"), channel, enable);
}

void CWinTestDlg::OnBnClickedCheckExtMediaIn()
{
    const int channel(-1);
    CButton* button = (CButton*)GetDlgItem(IDC_CHECK_EXT_MEDIA_IN);
    int check = button->GetCheck();
    const bool enable = (check == BST_CHECKED);
    if (enable)
    {
        TEST(_veExternalMediaPtr->RegisterExternalMediaProcessing(channel, kRecordingAllChannelsMixed, *_externalMediaPtr) == 0,
            _T("RegisterExternalMediaProcessing(channel=%d, kRecordingAllChannelsMixed, processObject=0x%x)"), channel, _externalMediaPtr);
    }
    else
    {
        TEST(_veExternalMediaPtr->DeRegisterExternalMediaProcessing(channel, kRecordingAllChannelsMixed) == 0,
            _T("DeRegisterExternalMediaProcessing(channel=%d, kRecordingAllChannelsMixed)"), channel);
    }
}

void CWinTestDlg::OnBnClickedCheckExtMediaOut()
{
    const int channel(-1);
    CButton* button = (CButton*)GetDlgItem(IDC_CHECK_EXT_MEDIA_OUT);
    int check = button->GetCheck();
    const bool enable = (check == BST_CHECKED);
    if (enable)
    {
        TEST(_veExternalMediaPtr->RegisterExternalMediaProcessing(channel, kPlaybackAllChannelsMixed, *_externalMediaPtr) == 0,
            _T("RegisterExternalMediaProcessing(channel=%d, kPlaybackAllChannelsMixed, processObject=0x%x)"), channel, _externalMediaPtr);
    }
    else
    {
        TEST(_veExternalMediaPtr->DeRegisterExternalMediaProcessing(channel, kPlaybackAllChannelsMixed) == 0,
            _T("DeRegisterExternalMediaProcessing(channel=%d, kPlaybackAllChannelsMixed)"), channel);
    }
}

void CWinTestDlg::OnCbnSelchangeComboRecDevice()
{
    CComboBox* comboCodec(NULL);
    comboCodec = (CComboBox*)GetDlgItem(IDC_COMBO_REC_DEVICE);
    int index = comboCodec->GetCurSel();
    TEST(_veHardwarePtr->SetRecordingDevice(index) == 0,
        _T("SetRecordingDevice(index=%d)"), index);
}

void CWinTestDlg::OnCbnSelchangeComboPlayDevice()
{
    CComboBox* comboCodec(NULL);
    comboCodec = (CComboBox*)GetDlgItem(IDC_COMBO_PLAY_DEVICE);
    int index = comboCodec->GetCurSel();
    TEST(_veHardwarePtr->SetPlayoutDevice(index) == 0,
        _T("SetPlayoutDevice(index=%d)"), index);
}

void CWinTestDlg::OnNMReleasedcaptureSliderInputVolume(NMHDR *pNMHDR, LRESULT *pResult)
{
    CSliderCtrl* slider = (CSliderCtrl*)GetDlgItem(IDC_SLIDER_INPUT_VOLUME);
    slider->SetRangeMin(0);
    slider->SetRangeMax(255);
    int pos = slider->GetPos();

    TEST(_veVolumeControlPtr->SetMicVolume(pos) == 0, _T("SetMicVolume(volume=%d)"), pos);

    *pResult = 0;
}

void CWinTestDlg::OnNMReleasedcaptureSliderOutputVolume(NMHDR *pNMHDR, LRESULT *pResult)
{
    CSliderCtrl* slider = (CSliderCtrl*)GetDlgItem(IDC_SLIDER_OUTPUT_VOLUME);
    slider->SetRangeMin(0);
    slider->SetRangeMax(255);
    int pos = slider->GetPos();

    TEST(_veVolumeControlPtr->SetSpeakerVolume(pos) == 0, _T("SetSpeakerVolume(volume=%d)"), pos);

    *pResult = 0;
}

void CWinTestDlg::OnNMReleasedcaptureSliderPanLeft(NMHDR *pNMHDR, LRESULT *pResult)
{
    CSliderCtrl* slider = (CSliderCtrl*)GetDlgItem(IDC_SLIDER_PAN_LEFT);
    slider->SetRange(0,10);
    int pos = 10 - slider->GetPos();    // 0 <=> lower end, 10 <=> upper end

    float left(0.0);
    float right(0.0);
    const int channel(-1);

    // Only left channel will be modified
    _veVolumeControlPtr->GetOutputVolumePan(channel, left, right);

    left = (float)((float)pos/10.0f);

    TEST(_veVolumeControlPtr->SetOutputVolumePan(channel, left, right) == 0,
        _T("SetOutputVolumePan(channel=%d, left=%2.1f, right=%2.1f)"), channel, left, right);

    *pResult = 0;
}

void CWinTestDlg::OnNMReleasedcaptureSliderPanRight(NMHDR *pNMHDR, LRESULT *pResult)
{
    CSliderCtrl* slider = (CSliderCtrl*)GetDlgItem(IDC_SLIDER_PAN_RIGHT);
    slider->SetRange(0,10);
    int pos = 10 - slider->GetPos();    // 0 <=> lower end, 10 <=> upper end

    float left(0.0);
    float right(0.0);
    const int channel(-1);

    // Only right channel will be modified
    _veVolumeControlPtr->GetOutputVolumePan(channel, left, right);

    right = (float)((float)pos/10.0f);

    TEST(_veVolumeControlPtr->SetOutputVolumePan(channel, left, right) == 0,
        _T("SetOutputVolumePan(channel=%d, left=%2.1f, right=%2.1f)"), channel, left, right);

    *pResult = 0;
}

void CWinTestDlg::OnBnClickedButtonVersion()
{
    if (_veBasePtr)
    {
        char version[1024];
        if (_veBasePtr->GetVersion(version) == 0)
        {
            AfxMessageBox(CString(version), MB_OK);
        }
        else
        {
            AfxMessageBox(_T("FAILED!"), MB_OK);
        }
    }
}

void CWinTestDlg::OnBnClickedCheckRecCall()
{
    // Not supported
}

void CWinTestDlg::OnBnClickedCheckTypingDetection()
{
    CButton* button = (CButton*)GetDlgItem(IDC_CHECK_TYPING_DETECTION);
    int check = button->GetCheck();
    const bool enable = (check == BST_CHECKED);
    TEST(_veApmPtr->SetTypingDetectionStatus(enable) == 0, _T("SetTypingDetectionStatus(enable=%d)"), enable);
}

void CWinTestDlg::OnBnClickedCheckFEC()
{
    CButton* button = (CButton*)GetDlgItem(IDC_CHECK_FEC);
    int channel = GetDlgItemInt(IDC_EDIT_1);
    int check = button->GetCheck();
    const bool enable = (check == BST_CHECKED);
    TEST(_veRtpRtcpPtr->SetFECStatus(channel, enable) == 0, _T("SetFECStatus(enable=%d)"), enable);
}

// ----------------------------------------------------------------------------
//                                   Message Handlers
// ----------------------------------------------------------------------------

void CWinTestDlg::OnTimer(UINT_PTR nIDEvent)
{
    CString str;

    unsigned int svol(0);
    unsigned int mvol(0);

    _timerTicks++;

    // Get speaker and microphone volumes
    _veVolumeControlPtr->GetSpeakerVolume(svol);
    _veVolumeControlPtr->GetMicVolume(mvol);

    // Update speaker volume slider
    CSliderCtrl* sliderSpkr = (CSliderCtrl*)GetDlgItem(IDC_SLIDER_OUTPUT_VOLUME);
    sliderSpkr->SetRangeMin(0);
    sliderSpkr->SetRangeMax(255);
    sliderSpkr->SetPos(svol);

    // Update microphone volume slider
    CSliderCtrl* sliderMic = (CSliderCtrl*)GetDlgItem(IDC_SLIDER_INPUT_VOLUME);
    sliderMic->SetRangeMin(0);
    sliderMic->SetRangeMax(255);
    sliderMic->SetPos(mvol);

    unsigned int micLevel;
    unsigned int combinedOutputLevel;

    // Get audio levels
    _veVolumeControlPtr->GetSpeechInputLevel(micLevel);
    _veVolumeControlPtr->GetSpeechOutputLevel(-1, combinedOutputLevel);

    // Update audio level controls
    CProgressCtrl* progressMic = (CProgressCtrl*)GetDlgItem(IDC_PROGRESS_AUDIO_LEVEL_IN);
    progressMic->SetRange(0,9);
    progressMic->SetStep(1);
    progressMic->SetPos(micLevel);
    CProgressCtrl* progressOut = (CProgressCtrl*)GetDlgItem(IDC_PROGRESS_AUDIO_LEVEL_OUT);
    progressOut->SetRange(0,9);
    progressOut->SetStep(1);
    progressOut->SetPos(combinedOutputLevel);

    // Update playout delay (buffer size)
    if (_veVideoSyncPtr)
    {
        int bufferMs(0);
        _veVideoSyncPtr->GetPlayoutBufferSize(bufferMs);
        SetDlgItemInt(IDC_EDIT_PLAYOUT_BUFFER_SIZE, bufferMs);
    }

    if (_delayEstimate1 && _veVideoSyncPtr)
    {
        const int channel = GetDlgItemInt(IDC_EDIT_1);
        int delayMs(0);
        _veVideoSyncPtr->GetDelayEstimate(channel, delayMs);
        SetDlgItemInt(IDC_EDIT_DELAY_ESTIMATE_1, delayMs);
    }

    if (_rxVad && _veApmPtr && _rxVadObserverPtr)
    {
        SetDlgItemInt(IDC_EDIT_RXVAD, _rxVadObserverPtr->vad_decision);
    }

    if (_veHardwarePtr)
    {
        int load1, load2;
        _veHardwarePtr->GetSystemCPULoad(load1);
        _veHardwarePtr->GetCPULoad(load2);
        str.Format(_T("CPU load (system/VoE): %d/%d [%%]"), load1, load2);
        SetDlgItemText(IDC_EDIT_CPU_LOAD, (LPCTSTR)str);
    }

    BOOL ret;
    int channel = GetDlgItemInt(IDC_EDIT_1, &ret);

    if (_veCodecPtr)
    {
        if (ret == TRUE)
        {
            CodecInst codec;
            if (_veCodecPtr->GetRecCodec(channel, codec) == 0)
            {
        str.Format(_T("RX codec: %s, freq=%d, pt=%d, rate=%d, size=%d"), CharToTchar(codec.plname, 32), codec.plfreq, codec.pltype, codec.rate, codec.pacsize);
                SetDlgItemText(IDC_EDIT_RX_CODEC_1, (LPCTSTR)str);
            }
        }
    }

    if (_veRtpRtcpPtr)
    {
        if (ret == TRUE)
        {
            CallStatistics stats;
            if (_veRtpRtcpPtr->GetRTCPStatistics(channel, stats) == 0)
            {
                str.Format(_T("RTCP | RTP: cum=%u, ext=%d, frac=%u, jitter=%u | TX=%d, RX=%d, RTT=%d"),
                    stats.cumulativeLost, stats.extendedMax, stats.fractionLost, stats.jitterSamples, stats.packetsSent, stats.packetsReceived, stats.rttMs);
                SetDlgItemText(IDC_EDIT_RTCP_STAT_1, (LPCTSTR)str);
            }
        }
    }

    SetTimer(0, 1000, NULL);
    CDialog::OnTimer(nIDEvent);
}

void CWinTestDlg::OnBnClickedButtonClearErrorCallback()
{
    _nErrorCallbacks = 0;
    SetDlgItemText(IDC_EDIT_ERROR_CALLBACK, _T(""));
}

// ----------------------------------------------------------------------------
//                                       TEST
// ----------------------------------------------------------------------------

void CWinTestDlg::OnBnClickedButtonTest1()
{
    // add tests here...
}

