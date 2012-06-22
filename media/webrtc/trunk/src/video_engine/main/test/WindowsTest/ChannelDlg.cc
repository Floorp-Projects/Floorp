/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "ChannelDlg.h"
#include "VideoSize.h"
#include "CaptureDevicePool.h"
#include "ChannelPool.h"

#include <Mmsystem.h>
#include <dbt.h>


#include "assert.h"


#include <process.h> // threads.

#if defined _WIN32
    #define SLEEP_10_SEC ::Sleep(10000)
    #define GET_TIME_IN_MS timeGetTime
#endif

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

/////////////////////////////////////////////////////////////////////////////
// CDXChannelDlg dialog

CDXChannelDlg::CDXChannelDlg(VideoEngine* videoEngine,
                             CaptureDevicePool& captureDevicePool,
                             ChannelPool& channelPool,
    void* voiceEngine
    ,CWnd* pParent,CDXChannelDlgObserver* observer,
    int parentChannel/*=-1*/)
  : CDialog(CDXChannelDlg::IDD, pParent),
    _canAddLog(true),
    _dialogObserver(observer),
    _videoEngine(videoEngine),
    _captureDevicePool(captureDevicePool),
    _channelPool(channelPool),
    _parentChannel(parentChannel),
#ifndef NO_VOICE_ENGINE
    _voiceEngine((VoiceEngine*) voiceEngine),
#endif
    _callbackEvent(::CreateEvent( NULL, FALSE, FALSE, NULL)),
    _externalTransport(NULL)
{
    strcpy(_logMsg,"");
    _channelId = -1;
    _audioChannel=-1;
    _captureId=-1;

    //{{AFX_DATA_INIT(CDXChannelDlg)
    //}}AFX_DATA_INIT
    // Note that LoadIcon does not require a subsequent DestroyIcon in Win32

    InitializeCriticalSection(&_critCallback);
    unsigned int threadID;
    _callbackThread=(HANDLE)_beginthreadex(NULL,1024*1024,CallbackThread,(void*)this,0, &threadID);
}

void CDXChannelDlg::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(CDXChannelDlg)
  DDX_Control(pDX, IDC_DEVICE, m_ctrlDevice);
  DDX_Control(pDX, IDC_CODEC_LIST, m_ctrlCodec);
  DDX_Control(pDX, IDC_CAPTURE, m_ctrlLiveRemoteVideo);
  DDX_Control(pDX, IDC_LIVEVIDEO, m_ctrlLiveVideo);
  DDX_Control(pDX, IDC_LOCAL_PORT1, m_localPort1);
  DDX_Control(pDX, IDC_REMOTE_PORT1, m_remotePort1);
  DDX_Control(pDX, IDC_IPADDRESS1, m_remoteIp1);
  DDX_Control(pDX, IDC_CODEC_SIZE, m_ctrlCodecSize);
  DDX_Control(pDX, IDC_RTCPMODE, m_ctrlRtcpMode);
  DDX_Control(pDX, IDC_PACKETBURST, m_ctrlPacketBurst);
  DDX_Control(pDX, IDC_BITRATE, m_ctrlBitrate);
  DDX_Control(pDX, IDC_MIN_FRAME_RATE, m_ctrlMinFrameRate);
  DDX_Control(pDX, IDC_TMMBR,m_cbTmmbr);
  DDX_Control(pDX, IDC_EXTTRANSPORT,m_cbExternalTransport);
  DDX_Control(pDX, IDC_PACKETLOSS,m_ctrlPacketLoss);
  DDX_Control(pDX, IDC_DELAY,m_ctrlDelay);
  DDX_Control(pDX, IDC_FREEZELOG,m_cbFreezeLog);
  DDX_Control(pDX,IDC_INFORMATION,m_ctrlInfo);
  //}}AFX_DATA_MAP
}

// ON_WM_SYSKEYDOWN      ALT+key

BEGIN_MESSAGE_MAP(CDXChannelDlg, CDialog)
  //{{AFX_MSG_MAP(CDXChannelDlg)
  ON_WM_SYSCOMMAND()
  ON_WM_RBUTTONUP()
  //ON_WM_DEVICECHANGE()
  ON_WM_PAINT()
  ON_WM_QUERYDRAGICON()
  ON_BN_CLICKED(IDC_STARTSEND, OnStartSend)
  ON_BN_CLICKED(IDC_STOPSend, OnStopSend)
  //ON_WM_TIMER()
  ON_WM_DESTROY()
  //}}AFX_MSG_MAP
  ON_CBN_SELCHANGE(IDC_CODEC_LIST, OnCbnSelchangeCodecList)
  ON_CBN_SELCHANGE(IDC_DEVICE, OnCbnSelchangeDevice)
  ON_CBN_SELCHANGE(IDC_CODEC_SIZE, OnCbnSelchangeSize)
  ON_CBN_SELCHANGE(IDC_BITRATE, OnCbnSelchangeBitrate)
  //ON_MESSAGE(WM_DISPLAYCHANGE, OnDisplayChange)
  ON_CBN_SELCHANGE(IDC_MIN_FRAME_RATE, OnCbnSelchangeMinFrameRate)
  ON_BN_CLICKED(IDC_STARTLISTEN, OnBnClickedStartlisten)
  ON_BN_CLICKED(IDC_STOPLISTEN, OnBnClickedStoplisten)
  ON_BN_CLICKED(IDC_TMMBR, &CDXChannelDlg::OnBnClickedTmmbr)
  ON_CBN_SELCHANGE(IDC_RTCPMODE, &CDXChannelDlg::OnCbnSelchangeRtcpmode)
  ON_BN_CLICKED(IDC_PROT_NACK, &CDXChannelDlg::OnBnClickedProtNack)
  ON_BN_CLICKED(IDC_PROT_NONE, &CDXChannelDlg::OnBnClickedProtNone)
  ON_BN_CLICKED(IDC_PROT_FEC, &CDXChannelDlg::OnBnClickedProtFec)
  ON_BN_CLICKED(IDC_FREEZELOG, &CDXChannelDlg::OnBnClickedFreezelog)
  ON_BN_CLICKED(IDC_VERSION, &CDXChannelDlg::OnBnClickedVersion)
  ON_BN_CLICKED(IDC_EXTTRANSPORT, &CDXChannelDlg::OnBnClickedExttransport)
  ON_CBN_SELCHANGE(IDC_PACKETLOSS, &CDXChannelDlg::OnCbnSelchangePacketloss)
  ON_CBN_SELCHANGE(IDC_DELAY, &CDXChannelDlg::OnCbnSelchangeDelay)
  ON_BN_CLICKED(IDC_BTN_RECORD_INCOMING, &CDXChannelDlg::OnBnClickedBtnRecordIncoming)
  ON_BN_CLICKED(IDC_BTN_RECORD_OUTGOING, &CDXChannelDlg::OnBnClickedBtnRecordOutgoing)
  ON_BN_CLICKED(IDC_BTN_CREATE_SLAVE, &CDXChannelDlg::OnBnClickedBtnCreateSlave)
  ON_BN_CLICKED(IDC_PROT_NACKFEC, &CDXChannelDlg::OnBnClickedProtNackFec)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDXChannelDlg message handlers


BOOL CDXChannelDlg::OnInitDialog()
{
  CDialog::OnInitDialog();

  // Set the icon for this dialog.  The framework does this automatically
  //  when the application's main window is not a dialog
  SetIcon(m_hIcon, TRUE);      // Set big icon
  SetIcon(m_hIcon, FALSE);    // Set small icon


  ::SendMessage(m_ctrlMinFrameRate.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("5"));
  ::SendMessage(m_ctrlMinFrameRate.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("6"));
  ::SendMessage(m_ctrlMinFrameRate.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("7"));
  ::SendMessage(m_ctrlMinFrameRate.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("8"));
  ::SendMessage(m_ctrlMinFrameRate.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("9"));
  ::SendMessage(m_ctrlMinFrameRate.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("10"));
  ::SendMessage(m_ctrlMinFrameRate.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("11"));
  ::SendMessage(m_ctrlMinFrameRate.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("12"));
  ::SendMessage(m_ctrlMinFrameRate.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("13"));
  ::SendMessage(m_ctrlMinFrameRate.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("14"));
  ::SendMessage(m_ctrlMinFrameRate.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("15"));
  ::SendMessage(m_ctrlMinFrameRate.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("16"));
  ::SendMessage(m_ctrlMinFrameRate.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("17"));
  ::SendMessage(m_ctrlMinFrameRate.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("18"));
  ::SendMessage(m_ctrlMinFrameRate.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("19"));
  ::SendMessage(m_ctrlMinFrameRate.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("20"));
  ::SendMessage(m_ctrlMinFrameRate.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("21"));
  ::SendMessage(m_ctrlMinFrameRate.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("22"));
  ::SendMessage(m_ctrlMinFrameRate.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("23"));
  ::SendMessage(m_ctrlMinFrameRate.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("24"));
  ::SendMessage(m_ctrlMinFrameRate.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("25"));
  ::SendMessage(m_ctrlMinFrameRate.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("26"));
  ::SendMessage(m_ctrlMinFrameRate.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("27"));
  ::SendMessage(m_ctrlMinFrameRate.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("28"));
  ::SendMessage(m_ctrlMinFrameRate.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("29"));
  ::SendMessage(m_ctrlMinFrameRate.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("30"));
  m_ctrlMinFrameRate.SetCurSel(25);

  // Codec sizes
  for(VideoSize i=UNDEFINED;i<NUMBER_OF_VIDEO_SIZE;i=VideoSize(i+1))
  {
    char sizeStr[64];
    int width=0;
    int height=0;
    GetWidthHeight(i,width,height);
    sprintf(sizeStr,"%d x %d",width,height);
    ::SendMessage(m_ctrlCodecSize.m_hWnd, CB_ADDSTRING, 0,(LPARAM) CharToTchar(sizeStr,-1));
  }
  m_ctrlCodecSize.SetCurSel(8);

  // RTCP mode
  /*
  kRtcpNone     = 0,
  kRtcpCompound_RFC4585     = 1,
  kRtcpNonCompound_RFC5506 = 2 */
  ::SendMessage(m_ctrlRtcpMode.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("RTCP_NONE"));
  ::SendMessage(m_ctrlRtcpMode.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("RTCP_COMPOUND_RFC4585"));
  ::SendMessage(m_ctrlRtcpMode.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("RTCP_NON_COMPOUND_RFC5506"));
  m_ctrlRtcpMode.SetCurSel(2);


  //Packet Burst
  ::SendMessage(m_ctrlPacketBurst.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("0"));
  ::SendMessage(m_ctrlPacketBurst.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("10"));
  ::SendMessage(m_ctrlPacketBurst.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("20"));
  ::SendMessage(m_ctrlPacketBurst.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("30"));
  m_ctrlPacketBurst.SetCurSel(0);


  //Send Bitrate
  ::SendMessage(m_ctrlBitrate.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("50"));
  ::SendMessage(m_ctrlBitrate.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("100"));
  ::SendMessage(m_ctrlBitrate.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("200"));
  ::SendMessage(m_ctrlBitrate.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("300"));
  ::SendMessage(m_ctrlBitrate.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("500"));
  ::SendMessage(m_ctrlBitrate.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("1000"));
  ::SendMessage(m_ctrlBitrate.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("2000"));
  ::SendMessage(m_ctrlBitrate.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("3000"));
  ::SendMessage(m_ctrlBitrate.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("4000"));
  ::SendMessage(m_ctrlBitrate.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("5000"));
  ::SendMessage(m_ctrlBitrate.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("6000"));
  ::SendMessage(m_ctrlBitrate.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("7000"));

  m_ctrlBitrate.SetCurSel(3);

  // External transport packet loss
  ::SendMessage(m_ctrlPacketLoss.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("0"));
  ::SendMessage(m_ctrlPacketLoss.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("2"));
  ::SendMessage(m_ctrlPacketLoss.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("4"));
  ::SendMessage(m_ctrlPacketLoss.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("6"));
  ::SendMessage(m_ctrlPacketLoss.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("8"));
  ::SendMessage(m_ctrlPacketLoss.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("10"));
  ::SendMessage(m_ctrlPacketLoss.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("12"));
  ::SendMessage(m_ctrlPacketLoss.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("14"));
  ::SendMessage(m_ctrlPacketLoss.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("16"));
  ::SendMessage(m_ctrlPacketLoss.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("18"));
  ::SendMessage(m_ctrlPacketLoss.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("20"));
  m_ctrlPacketLoss.SetCurSel(0);

  // External transport delay
  ::SendMessage(m_ctrlDelay.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("0"));
  ::SendMessage(m_ctrlDelay.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("30"));
  ::SendMessage(m_ctrlDelay.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("60"));
  ::SendMessage(m_ctrlDelay.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("90"));
  ::SendMessage(m_ctrlDelay.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("120"));
  ::SendMessage(m_ctrlDelay.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("150"));
  ::SendMessage(m_ctrlDelay.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("180"));
  ::SendMessage(m_ctrlDelay.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("210"));
  m_ctrlDelay.SetCurSel(0);

  _vieBase=ViEBase::GetInterface(_videoEngine);
  TEST_MUSTPASS(_vieBase==0,-5);

  _vieCapture=ViECapture::GetInterface(_videoEngine);
  TEST_MUSTPASS(_vieCapture==0,-5);

  _vieRTPRTCP=ViERTP_RTCP::GetInterface(_videoEngine);
  TEST_MUSTPASS(_vieRTPRTCP==0,-5);

  _vieRender=ViERender::GetInterface(_videoEngine);
  TEST_MUSTPASS(_vieRender==0,-5);

  _vieCodec=ViECodec::GetInterface(_videoEngine);
  TEST_MUSTPASS(_vieCodec==0,-5);
  _vieNetwork=ViENetwork::GetInterface(_videoEngine);
  TEST_MUSTPASS(_vieNetwork==0,-5);

  _vieFile=ViEFile::GetInterface(_videoEngine);
  TEST_MUSTPASS(_vieFile==0,-5);

#ifndef NO_VOICE_ENGINE

  _veBase = VoEBase::GetInterface(_voiceEngine);
  _veNetwork = VoENetwork::GetInterface(_voiceEngine);
  _veCodec = VoECodec::GetInterface(_voiceEngine);
  _veRTCP = VoERTP_RTCP::GetInterface(_voiceEngine);
  TEST_MUSTPASS(_vieBase->SetVoiceEngine(_voiceEngine),-5);
#endif

  char str[64];
  bool found = false;

  int captureIdx = 0;
  while (-1 !=_vieCapture->GetCaptureDevice(captureIdx,str,sizeof(str),NULL,0))
  {
    char* tmp = strstr(str,"(VFW)");
    if (!tmp)
    {
      ::SendMessage(m_ctrlDevice.m_hWnd, CB_ADDSTRING, 0,(LPARAM)CharToTchar(str,-1));
      found = true;
    }
    captureIdx++;
    memset(str, 0, 64);
  }
  WIN32_FIND_DATA FindFileData;
  HANDLE hFind;
  //char fileSearch[256];
  //strcpy(fileSearch,_T("*.avi"));
  hFind = FindFirstFile(_T("*.avi"), &FindFileData);
  if (hFind != INVALID_HANDLE_VALUE)
  {
    ::SendMessage(m_ctrlDevice.m_hWnd, CB_ADDSTRING, 0,(LPARAM)(FindFileData.cFileName));
    while(FindNextFile(hFind,&FindFileData))
    {
      ::SendMessage(m_ctrlDevice.m_hWnd, CB_ADDSTRING, 0,(LPARAM)(FindFileData.cFileName));
    }
    FindClose(hFind);
  }

  ::SendMessage(m_ctrlDevice.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("Conference"));
  ::SendMessage(m_ctrlDevice.m_hWnd, CB_ADDSTRING, 0,(LPARAM)_T("None"));

  if (!found)
  {
    strncpy(str,"N/A",64);
    ::SendMessage(m_ctrlDevice.m_hWnd, CB_ADDSTRING, 0,(LPARAM)CharToTchar(str,-1));
  }
  m_ctrlDevice.SetCurSel(0);

  //Codecs
  int numOfCodecs = _vieCodec->NumberOfCodecs();
  for(int i=0; i<numOfCodecs;++i)
  {
    VideoCodec codec;
    if(-1 !=_vieCodec->GetCodec(i,codec))
    {
      ::SendMessage(m_ctrlCodec.m_hWnd, CB_ADDSTRING, 0,(LPARAM)CharToTchar(codec.plName,-1));
    }
  }
  m_ctrlCodec.SetCurSel(0);

#ifndef NO_VOICE_ENGINE
  CodecInst voiceCodec;
  int numOfVeCodecs = _veCodec->NumOfCodecs();
  for(int i=0; i<numOfVeCodecs;++i)
  {
    if(_veCodec->GetCodec(i,voiceCodec)!=-1)
    {
      if(strncmp(voiceCodec.plname,"ISAC",4)==0)
        break;
    }
  }

  _audioChannel = _veBase->CreateChannel();

  TEST_MUSTPASS(_veRTCP->SetRTCPStatus(_audioChannel, true),-5);
  TEST_MUSTPASS(_veCodec->SetSendCodec(_audioChannel, voiceCodec),-5);
  TEST_MUSTPASS(_veBase->StartPlayout(_audioChannel),-5);
#endif  //NO_VOICE_ENGINE

  if(_parentChannel==-1)
  {
    TEST_MUSTPASS(_vieBase->CreateChannel(_channelId),-5);
  }
  else // This is a slave channel
  {
    TEST_MUSTPASS(_vieBase->CreateChannel(_channelId,_parentChannel),-5);
  }
#ifndef NO_VOICE_ENGINE
  TEST_MUSTPASS(_vieBase->ConnectAudioChannel(_channelId,_audioChannel),-5);
#endif

  _channelPool.AddChannel(_channelId);

  //Set Receive codec
  {
    VideoCodec codec;
    int numOfCodecs = _vieCodec->NumberOfCodecs();;
    for(int i=0; i<numOfCodecs;++i)
    {
      if(-1 !=_vieCodec->GetCodec(i,codec))
      {
        if(codec.codecType == webrtc::kVideoCodecVP8)
        {
          codec.codecSpecific.VP8.feedbackModeOn = true;
          codec.codecSpecific.VP8.pictureLossIndicationOn = true;
        }
        TEST_MUSTPASS(_vieCodec->SetReceiveCodec(_channelId,codec),-5);
      }
    }
  }

  //TMMBR
  m_cbTmmbr.SetCheck(BST_CHECKED);
  OnBnClickedTmmbr();

  //Packet Burst
  m_ctrlPacketBurst.SetCurSel(0);


  //Protection method none
  CButton *opProtection = (CButton *) GetDlgItem(IDC_PROT_NONE);
  opProtection->SetCheck(BST_CHECKED);
  OnBnClickedProtNone();


  // Configure the renderer
  ConfigureRender();

  TEST_MUSTPASS(_vieCodec->RegisterEncoderObserver(_channelId,*this),kViECodecObserverAlreadyRegistered);
  TEST_MUSTPASS(_vieCodec->RegisterDecoderObserver(_channelId,*this),-5);

  TEST_MUSTPASS(_vieBase->RegisterObserver(*this),kViEBaseObserverAlreadyRegistered);




  //Set captions based on channel id
  m_remoteIp1.SetAddress(127,0,0,1);
  CString port;
  port.AppendFormat(_T("%d"),11111+_channelId*4);
  m_remotePort1.SetWindowText(port);
  m_localPort1.SetWindowText(port);

  CString title;
  this->GetWindowText(title);
  if(_parentChannel==-1)
  {
    title.AppendFormat(_T("%s - channel %d"),title,_channelId);
  }
  else
  {
    title.AppendFormat(_T("%s - slave channel %d - parent %d"),title,_channelId,_parentChannel);
  }
  this->SetWindowText(title);

  if(_parentChannel!=-1)
    m_ctrlDevice.EnableWindow(FALSE); //Prevent from changing capture device

  return TRUE;  // return TRUE  unless you set the focus to a control
}



void CDXChannelDlg::OnTimer(UINT nIDEvent)
{
  CDialog::OnTimer(nIDEvent);
}

void CDXChannelDlg::SetSendCodec()
{
    // Get the codec stucture
    int codecSel= m_ctrlCodec.GetCurSel();
    VideoCodec codec;
    TEST_MUSTPASS(_vieCodec->GetCodec(codecSel,codec),-5);


    // Set Codec Size
    VideoSize sizeSel=VideoSize(m_ctrlCodecSize.GetCurSel());
    int width, height;
    GetWidthHeight(sizeSel, width, height);
    codec.width=width;
    codec.height=height;

    //Set the codec bitrate
    CString bitrateStr;
  m_ctrlBitrate.GetLBText(m_ctrlBitrate.GetCurSel(), bitrateStr);
    int bitrate = _ttoi(bitrateStr.GetBuffer(0));
    if(codec.codecType!=kVideoCodecI420)
    {
        codec.startBitrate=bitrate;
        codec.maxBitrate=bitrate*4;
    }


    //Set the codec frame rate
    codec.maxFramerate = m_ctrlMinFrameRate.GetCurSel() +5;

    if(strncmp(codec.plName, "VP8", 5) == 0)
    {
    codec.codecSpecific.VP8.feedbackModeOn = true;
    codec.codecSpecific.VP8.pictureLossIndicationOn = true;
        TEST_MUSTPASS(_vieRTPRTCP->SetKeyFrameRequestMethod(_channelId, kViEKeyFrameRequestPliRtcp),-5);
    }else
    {
        TEST_MUSTPASS(_vieRTPRTCP->SetKeyFrameRequestMethod(_channelId, kViEKeyFrameRequestPliRtcp),-5);
    }
    TEST_MUSTPASS(_vieCodec->SetSendCodec(_channelId, codec),-5);

    if (codec.codecType == webrtc::kVideoCodecI420)
    {        // Need to set the receive codec size
        _vieCodec->SetReceiveCodec(_channelId, codec);
    }
}

void CDXChannelDlg::SetSendDestination()
{
    if(_externalTransport)
        return;

    BYTE part1, part2, part3, part4;
  char sendIP1[16];
  m_remoteIp1.GetAddress(part1, part2, part3, part4);
  sprintf(sendIP1,"%d.%d.%d.%d",part1,part2,part3,part4);

    CString strPort;
    m_remotePort1.GetWindowText(strPort);
  int remotePort1 = _ttoi(strPort.GetString());

  TEST_MUSTPASS(_vieNetwork->SetSendDestination(_channelId,sendIP1,remotePort1),kViENetworkAlreadySending);

#ifndef NO_VOICE_ENGINE
  m_localPort1.GetWindowText(strPort);
  int localPort1 = _ttoi(strPort.GetString());
  _veBase->SetLocalReceiver(_audioChannel,localPort1+2);
  TEST_MUSTPASS(_veBase->SetSendDestination(_audioChannel, remotePort1+2, sendIP1),-5)
#endif
}

void CDXChannelDlg::SetLocalReceiver()
{
    if(_externalTransport)
        return;

    CString strPort;
  m_localPort1.GetWindowText(strPort);
  int localPort1 = _ttoi(strPort.GetString());



    // May fail because we are sending
    TEST_MUSTPASS(_vieNetwork->SetLocalReceiver(_channelId, localPort1),-5);

#ifndef NO_VOICE_ENGINE
    _veBase->SetLocalReceiver(_audioChannel,localPort1+2);
#endif
}

void CDXChannelDlg::SetCaptureDevice()
{
    if(_parentChannel!=-1) // don't accept changing input on slave channels.
        return;

    int camSel=-1;
    camSel=m_ctrlDevice.GetCurSel();

  CString captureStr;
    //captureStr.Compare
  m_ctrlDevice.GetLBText(camSel, captureStr);
  if(captureStr!=_T("N/A") != 0)
  {

        TEST_MUSTPASS(_vieFile->StopPlayFile(_captureId),kViEFileNotPlaying);
        TEST_MUSTPASS(_vieCapture->DisconnectCaptureDevice(_channelId),kViECaptureDeviceNotConnected);
        TEST_MUSTPASS(_vieRender->RemoveRenderer(_captureId),kViERenderInvalidRenderId);

        if(_captureId>=0x1001 && _captureId<0x10FF)// ID is a capture device
        {
            TEST_MUSTPASS(_captureDevicePool.ReturnCaptureDevice(_captureId),-5);
        }

        if(captureStr!=_T("None")==0)
        {
            _captureId=-1;
        }
        else if(_tcsstr(captureStr,_T(".avi"))!=NULL ) // Selected an AVI file
        {
            TEST_MUSTPASS(_vieFile->StartPlayFile(TcharToChar(captureStr.GetBuffer(),-1),_captureId,false,webrtc::kFileFormatAviFile),-5);
            TEST_MUSTPASS(_vieRender->AddRenderer(_captureId,m_ctrlLiveVideo.m_hWnd, 0, 0.0f, 0.0f,1.0f,1.0f),-5);
            TEST_MUSTPASS(_vieRender->StartRender(_captureId),-5);
            TEST_MUSTPASS(_vieFile->SendFileOnChannel(_captureId,_channelId),-5);
            TEST_MUSTPASS(_vieFile->StartPlayFileAsMicrophone(_captureId,_channelId,true),-5);
            //TEST_MUSTPASS(_vieFile->StartPlayAudioLocally(_captureId,_channelId),-5);
        }
        else
        {

            char captureName[256];
            char uniqueCaptureName[256];

            TEST_MUSTPASS(_vieCapture->GetCaptureDevice(camSel,captureName,256,uniqueCaptureName,256),-5);

            TEST_MUSTPASS(_captureDevicePool.GetCaptureDevice(_captureId,uniqueCaptureName),-5);
            TEST_MUSTPASS(_vieCapture->StartCapture(_captureId),kViECaptureDeviceAlreadyStarted);
            TEST_MUSTPASS(_vieCapture->RegisterObserver(_captureId,*this),kViECaptureObserverAlreadyRegistered);

            TEST_MUSTPASS(_vieRender->AddRenderer(_captureId,m_ctrlLiveVideo.m_hWnd, 0, 0.0f, 0.0f,1.0f,1.0f),-5);
            TEST_MUSTPASS(_vieCapture->ConnectCaptureDevice(_captureId,_channelId),-5);
            TEST_MUSTPASS(_vieRender->StartRender(_captureId),-5);            
        }
    }

}



void CDXChannelDlg::OnBnClickedStartlisten()
{


    // Configure the local ports
    SetLocalReceiver();

    //Configure the remote destination- needed in order to be able to respond to RTCP messages
    SetSendDestination();


    #ifndef NO_VOICE_ENGINE
        TEST_MUSTPASS(_veBase->StartReceive(_audioChannel),-5);
    #endif
    TEST_MUSTPASS(_vieBase->StartReceive(_channelId),-5);


}

void CDXChannelDlg::OnStartSend()
{

    // Set the send destination
    SetSendDestination();

    // Configure the local ports (Needed to be able to receive RTCP
    //SetLocalReceiver();


    // Set the send codec
    SetSendCodec();

    if(_captureId==-1) // If no capture device has been set.
      SetCaptureDevice(); //Set the capture device



    //TEST_MUSTPASS(_vieRTPRTCP->SetStartSequenceNumber(_channelId,1),-5);

    // Start sending
    TEST_MUSTPASS(_vieBase->StartSend(_channelId),-5);


    #ifndef NO_VOICE_ENGINE
        TEST_MUSTPASS(_veBase->StartSend(_audioChannel),-5);
    #endif


}

void CDXChannelDlg::ConfigureRender()
{
    TEST_MUSTPASS(_vieRender->AddRenderer(_channelId,m_ctrlLiveRemoteVideo.m_hWnd, 0, 0.0f, 0.0f,1.0f,1.0f),-5);

    TEST_MUSTPASS(_vieFile->SetRenderStartImage(_channelId,
                           "./main/test/WindowsTest/renderStartImage.jpg"),-5);
    TEST_MUSTPASS(_vieRender->StartRender(_channelId),-5);
    TEST_MUSTPASS(_vieFile->SetRenderTimeoutImage(_channelId,
                         "./main/test/WindowsTest/renderTimeoutImage.jpg"),-5);


}


void CDXChannelDlg::OnStopSend()
{

    #ifndef NO_VOICE_ENGINE
        TEST_MUSTPASS(_veBase->StopSend(_audioChannel),-5);
    #endif


    TEST_MUSTPASS(_vieBase->StopSend(_channelId),kViEBaseNotSending);   // Accept error Not sending


}
void CDXChannelDlg::OnBnClickedStoplisten()
{


    #ifndef NO_VOICE_ENGINE
        TEST_MUSTPASS(_veBase->StopReceive(_audioChannel),-5);
    #endif
    TEST_MUSTPASS(_vieBase->StopReceive(_channelId),-5);
}


void CDXChannelDlg::OnDestroy()
{

    OnStopSend();
    OnBnClickedStoplisten();

    if(_vieCapture && _parentChannel==-1)
    {
        _vieCapture->DisconnectCaptureDevice(_channelId);
        _captureDevicePool.ReturnCaptureDevice(_captureId);
    }
    if(_vieFile && _parentChannel!=-1)
    {
        TEST_MUSTPASS(_vieFile->StopPlayFile(_captureId),kViEFileNotPlaying);
    }




    if(_videoEngine)
  {
        if(_parentChannel==-1)
        {
            _vieCodec->DeregisterEncoderObserver(_channelId);
        }
        _vieBase->DeleteChannel(_channelId);
        _channelPool.RemoveChannel(_channelId);
  }

  _videoEngine = NULL;
#ifndef NO_VOICE_ENGINE

    if (_voiceEngine)
  {
        _veBase->DeleteChannel(_audioChannel);
        _veBase->Release();
        _veNetwork->Release();
        _veCodec->Release();
        _veRTCP->Release();
  }
#endif


    strcpy(_logMsg,"");
    SetEvent(_callbackEvent);
    MSG msg; // Wait until the callback thread exits. Need to handle messages since the callback thread can call SendMessage when updating UI
    while(WaitForSingleObject(_callbackThread,10)==WAIT_TIMEOUT)
    {
        DWORD ret = PeekMessage( &msg, NULL, 0, 0,PM_REMOVE );
        if (ret >0)
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    CloseHandle(_callbackThread);
    CloseHandle(_callbackEvent);
    DeleteCriticalSection(&_critCallback);

    TEST_MUSTPASS(_vieCapture->Release()<0,-5);
    TEST_MUSTPASS(_vieRTPRTCP->Release()<0,-5);
    TEST_MUSTPASS(_vieRender->Release()<0,-5);
    TEST_MUSTPASS(_vieCodec->Release()<0,-5);
    TEST_MUSTPASS(_vieNetwork->Release()<0,-5);
    TEST_MUSTPASS(_vieFile->Release()<0,-5);
    TEST_MUSTPASS(_vieBase->Release()<0,-5);



#ifdef TEST_EXTERNAL_TRANSPORT
  if(_transport)
    delete _transport;
  _transport = NULL;
#endif

    delete _externalTransport;

  CDialog::OnDestroy();
    if(_dialogObserver)
    {
        _dialogObserver->ChannelDialogEnded(this);
    }
}

void CDXChannelDlg::OnCancel()
{
    DestroyWindow();
}
// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CDXChannelDlg::OnPaint()
{
    if (IsIconic())
  {
    CPaintDC dc(this); // device context for painting

    SendMessage(WM_ICONERASEBKGND, (WPARAM) dc.GetSafeHdc(), 0);

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

BOOL CDXChannelDlg::OnDeviceChange( UINT nID, DWORD lParam)
{
  if(nID ==  DBT_DEVNODES_CHANGED)
  {
    //  SetCaptureDevice();
  }
  return CDialog::OnDeviceChange(nID, lParam);
}


void CDXChannelDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
  if(SC_MAXIMIZE == nID)
  {}
  CDialog::OnSysCommand(nID, lParam);
}


static bool fullScreen = false;
void CDXChannelDlg::OnRButtonUp( UINT nFlags, CPoint point)
{
  CDialog::OnRButtonUp( nFlags,  point);
}

// The system calls this to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CDXChannelDlg::OnQueryDragIcon()
{
  return (HCURSOR) m_hIcon;
}

void CDXChannelDlg::OnCbnSelchangeCodecList()
{
    SetSendCodec();
}


void CDXChannelDlg::OnCbnSelchangeSize()
{
    SetSendCodec();
}

void CDXChannelDlg::OnCbnSelchangeDevice()
{


    SetCaptureDevice();

}


void CDXChannelDlg::OnCbnSelchangeBitrate()
{

    SetSendCodec();

}

void CDXChannelDlg::OnCbnSelchangeMinFrameRate()
{

    SetSendCodec();

}


void CDXChannelDlg::OnBnClickedTmmbr()
{

    TEST_MUSTPASS(_vieRTPRTCP->SetTMMBRStatus(_channelId,m_cbTmmbr.GetCheck()==BST_CHECKED),-5);

}

void CDXChannelDlg::OnCbnSelchangeRtcpmode()
{

 /*
 kRtcpNone     = 0,
 kRtcpCompound_RFC4585     = 1,
 kRtcpNonCompound_RFC5506 = 2 */
    ViERTCPMode mode=ViERTCPMode(m_ctrlRtcpMode.GetCurSel());
    TEST_MUSTPASS(_vieRTPRTCP->SetRTCPStatus(_channelId,mode),-5);

}

void CDXChannelDlg::OnBnClickedFreezelog()
{
    _canAddLog=m_cbFreezeLog.GetCheck()!=BST_CHECKED;
}

void CDXChannelDlg::OnBnClickedProtNack()
{

    TEST_MUSTPASS(_vieRTPRTCP->SetNACKStatus(_channelId,true),-5);

}

void CDXChannelDlg::OnBnClickedProtNone()
{

    TEST_MUSTPASS(_vieRTPRTCP->SetNACKStatus(_channelId,false),-5);
    TEST_MUSTPASS(_vieRTPRTCP->SetFECStatus(_channelId,false,0,0),-5);
    TEST_MUSTPASS(_vieRTPRTCP->SetHybridNACKFECStatus(_channelId,false,0,0),-5);
}

void CDXChannelDlg::OnBnClickedProtFec()
{
    int noCodec=_vieCodec->NumberOfCodecs();
    int redPayloadType=0;
    int fecPayloadType=0;
    for(unsigned char i=0;i<noCodec;++i)
    {
        VideoCodec codec;
        _vieCodec->GetCodec(i,codec);
        if(codec.codecType==webrtc::kVideoCodecRED)
        {
            redPayloadType=codec.plType;
        }
        if(codec.codecType==webrtc::kVideoCodecULPFEC)
        {
            fecPayloadType=codec.plType;
        }
    }
    TEST_MUSTPASS(_vieRTPRTCP->SetFECStatus(_channelId,true,redPayloadType,fecPayloadType),-5);
}

void CDXChannelDlg::OnBnClickedProtNackFec()
{
    int noCodec=_vieCodec->NumberOfCodecs();
    int redPayloadType=0;
    int fecPayloadType=0;
    for(unsigned char i=0;i<noCodec;++i)
    {
        VideoCodec codec;
        _vieCodec->GetCodec(i,codec);
        if(codec.codecType==webrtc::kVideoCodecRED)
        {
            redPayloadType=codec.plType;
        }
        if(codec.codecType==webrtc::kVideoCodecULPFEC)
        {
            fecPayloadType=codec.plType;
        }
    }
    TEST_MUSTPASS(_vieRTPRTCP->SetHybridNACKFECStatus(_channelId,true,
                                                      redPayloadType,
                                                      fecPayloadType),-5);

}

void CDXChannelDlg::OnBnClickedVersion()
{
    char version[1024];
    _vieBase->GetVersion(version);
    MessageBox(CharToTchar(version,-1));
#ifndef NO_VOICE_ENGINE
    _veBase->GetVersion(version);
    MessageBox(CharToTchar(version,-1));
#endif
}

unsigned int WINAPI CDXChannelDlg::CallbackThread(LPVOID lpParameter)
{
    static_cast<CDXChannelDlg*>(lpParameter)->CallbackThreadProcess();
    return 0;
}

void CDXChannelDlg::CallbackThreadProcess()
{
    while(1)
    {
        if(WAIT_OBJECT_0==WaitForSingleObject(_callbackEvent,INFINITE))
        {
            char smsg[512];
            EnterCriticalSection(&_critCallback);
            strncpy(smsg,_logMsg,strlen(_logMsg)+1);
            strcpy(_logMsg,"");


            LeaveCriticalSection(&_critCallback);
            if(strstr(smsg,"Send")!=NULL)
            {
                unsigned short fractionLost=0;
                unsigned int cumulativeLost=0;
                unsigned int extendedMax=0;
                unsigned int jitter=0;
                int rttMs=0;



                _vieRTPRTCP->GetReceivedRTCPStatistics(_channelId,
                                                  fractionLost,
                                                  cumulativeLost,
                                                  extendedMax,
                                                  jitter,
                                                  rttMs);

                //int bw=0;
                //if(_vieCodec->GetAvailableBandwidth(_channelId,bw)==0)
                //{
                //    sprintf(smsg,"%s, rtt %d, loss %d,bw %d", smsg,rttMs,fractionLost,bw);
                //}
                //else
                //{
                //    _vieBase->LastError(); // Reset last error.
                //}



            }
            if(strlen(smsg))
            {
                m_ctrlInfo.InsertString(0,(LPCTSTR) CharToTchar(smsg,-1));
                while(m_ctrlInfo.GetCount()==151)
                    m_ctrlInfo.DeleteString(150);
            }
            else
            {
                break; // End the callback thread
            }
        }
    }

}
void CDXChannelDlg::AddToInfo(const char* msg)
{
    if(!_canAddLog)
        return;
    EnterCriticalSection(&_critCallback);

    SYSTEMTIME systemTime;
    GetSystemTime(&systemTime);

    if(strlen(_logMsg)==0)
    {
        SetEvent(_callbackEvent); // Notify of new
    }

    sprintf (_logMsg, "(%2u:%2u:%2u:%3u) %s", systemTime.wHour,
                                                           systemTime.wMinute,
                                                           systemTime.wSecond,
                                                           systemTime.wMilliseconds,
                                                           msg
                                                           );



    LeaveCriticalSection(&_critCallback);


}

void CDXChannelDlg::IncomingRate(const int videoChannel,
                              unsigned int framerate,
                              unsigned int bitrate)
{
  char str[64];
  sprintf(str,"Incoming Fr:%d br %d\n", framerate, bitrate);
    AddToInfo(str);
}

void CDXChannelDlg::RequestNewKeyFrame(int channel)
{
    assert(false && "(RequestNewKeyFrame why is it called");
}
void CDXChannelDlg::PerformanceAlarm(unsigned int cpuLoad)
{
    char str[64];
    sprintf(str,"Performance alarm %d",cpuLoad);    
    AddToInfo(str);
}
void CDXChannelDlg::OutgoingRate(const int videoChannel,
                              unsigned int framerate,
                              unsigned int bitrate)
  {
    char str[64];
        sprintf(str,"Send Fr:%d br %d", framerate, bitrate);
        AddToInfo(str);
  }
void CDXChannelDlg::IncomingCodecChanged(const int  videoChannel,
                                      const VideoCodec& videoCodec)
  {
    char str[128];
        sprintf(str,"Incoming codec channel:%d pltype:%d width:%d height:%d\n", videoChannel, videoCodec.plType, videoCodec.width,videoCodec.height);        
        AddToInfo(str);
  }
void CDXChannelDlg::BrightnessAlarm(const int captureId,
                                 const Brightness brightness)
{

    switch(brightness)
    {
    case Normal:        
        AddToInfo("BrightnessAlarm - image ok.\n");
        break;
    case Bright:        
        AddToInfo("BrightnessAlarm - light image.\n");
        break;
    case Dark:        
        AddToInfo("BrightnessAlarm - dark image.\n");
        break;
    }
}

void CDXChannelDlg::CapturedFrameRate(const int captureId,
                                   const unsigned char frameRate)
{
   char str[64];
   sprintf(str,"Local Camera Frame rate:%d \n", frameRate);
   AddToInfo(str);
}

void CDXChannelDlg::NoPictureAlarm(const int captureId,
                                const CaptureAlarm alarm)
{
   char str[64];
   sprintf(str,"No Picture alarm\n");   
   AddToInfo(str);

}


void CDXChannelDlg::OnBnClickedExttransport()
{
    if(m_cbExternalTransport.GetCheck()==BST_CHECKED)
    {
        m_localPort1.EnableWindow(FALSE);
        m_remotePort1.EnableWindow(FALSE);
        m_remoteIp1.EnableWindow(FALSE);
        m_ctrlPacketLoss.EnableWindow(TRUE);
        m_ctrlDelay.EnableWindow(TRUE);
        _externalTransport= new TbExternalTransport(*_vieNetwork);
        _vieNetwork->RegisterSendTransport(_channelId,*_externalTransport);
    }
    else
    {
        _vieNetwork->DeregisterSendTransport(_channelId);

        delete _externalTransport;
        _externalTransport=NULL;
        m_localPort1.EnableWindow(TRUE);
        m_remotePort1.EnableWindow(TRUE);
        m_remoteIp1.EnableWindow(TRUE);
        m_ctrlPacketLoss.EnableWindow(FALSE);
        m_ctrlDelay.EnableWindow(FALSE);
    }
}


void CDXChannelDlg::OnCbnSelchangePacketloss()
{
    if(_externalTransport)
    {
        _externalTransport->SetPacketLoss(m_ctrlPacketLoss.GetCurSel()*2);
    }
}


void CDXChannelDlg::OnCbnSelchangeDelay()
{
    if(_externalTransport)
    {
        _externalTransport->SetNetworkDelay(m_ctrlDelay.GetCurSel()*30);
    }

}

void CDXChannelDlg::OnBnClickedBtnRecordIncoming()
{

    CButton *recordBtn = (CButton *) GetDlgItem(IDC_BTN_RECORD_INCOMING);
    
    CString text;
    recordBtn->GetWindowText(text);
    if(text!=_T("Stop Rec Inc")!=0)
    {
        recordBtn->SetWindowText(_T("Stop Rec Inc"));
        SYSTEMTIME time;
        GetSystemTime(&time);
        sprintf(_fileName,"IncomingChannel%d_%4d%2d%2d%2d%2d.avi",_channelId,time.wYear,time.wMonth,time.wDay,time.wHour,time.wMinute);

        AudioSource audioSource=PLAYOUT;
        webrtc::CodecInst audioCodec;
        strcpy(audioCodec.plname,"L16");
        audioCodec.rate     = 256000;
        audioCodec.plfreq   = 16000;
        audioCodec.pacsize  = 160;

        webrtc::VideoCodec videoCodec;
        memset(&videoCodec,0,sizeof(videoCodec));

        strcpy(videoCodec.plName,"VP8");
        videoCodec.maxBitrate=1000;
        videoCodec.startBitrate=1000;
        videoCodec.width=352;
        videoCodec.height=288;
        videoCodec.codecType=webrtc::kVideoCodecVP8;
        videoCodec.maxFramerate=30;        
        TEST_MUSTPASS(_vieFile->StartRecordIncomingVideo(_channelId,_fileName,audioSource,audioCodec, videoCodec),-5);
    }
    else
    {
        recordBtn->SetWindowText(_T("Record Incoming"));
        TEST_MUSTPASS(_vieFile->StopRecordIncomingVideo(_channelId),-5);
        CString msg;
        msg.AppendFormat(_T("Recorded file %s"),_fileName);
        MessageBox(msg);
    }
}

void CDXChannelDlg::OnBnClickedBtnRecordOutgoing()
{

    CButton *recordBtn = (CButton *) GetDlgItem(IDC_BTN_RECORD_OUTGOING);
    CString text;
    recordBtn->GetWindowText(text);
    if(text!=_T("Stop Rec Out"))
    {
        recordBtn->SetWindowText(_T("Stop Rec Out"));
        SYSTEMTIME time;
        GetSystemTime(&time);
        sprintf(_fileName,"OutgoingChannel%d_%4d%2d%2d%2d%2d.avi",_channelId,time.wYear,time.wMonth,time.wDay,time.wHour,time.wMinute);

        AudioSource audioSource=MICROPHONE;
        webrtc::CodecInst audioCodec;
        strcpy(audioCodec.plname,"L16");
        audioCodec.rate     = 256000;
        audioCodec.plfreq   = 16000;
        audioCodec.pacsize  = 160;

        webrtc::VideoCodec videoCodec;
        memset(&videoCodec,0,sizeof(videoCodec));

        strcpy(videoCodec.plName,"VP8");
        videoCodec.maxBitrate=1000;
        videoCodec.startBitrate=1000;
        videoCodec.width=352;
        videoCodec.height=288;
        videoCodec.codecType=webrtc::kVideoCodecVP8;
        videoCodec.maxFramerate=30;        
        TEST_MUSTPASS(_vieFile->StartRecordOutgoingVideo(_channelId,_fileName,audioSource,audioCodec,videoCodec),-5);
    }
    else
    {
        recordBtn->SetWindowText(_T("Record Outgoing"));
        TEST_MUSTPASS(_vieFile->StopRecordOutgoingVideo(_channelId),-5);
        CString msg;
        msg.AppendFormat(_T("Recorded file %s"),_fileName);
        MessageBox(msg);
    }
}

void CDXChannelDlg::OnBnClickedBtnCreateSlave()
{
    CDXChannelDlg* newSlave =new CDXChannelDlg(_videoEngine,_captureDevicePool,_channelPool,_voiceEngine,NULL,_dialogObserver,_channelId);
    newSlave->Create(CDXChannelDlg::IDD,NULL);
}
