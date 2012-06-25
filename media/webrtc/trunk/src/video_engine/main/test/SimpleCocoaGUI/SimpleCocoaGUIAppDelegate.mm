//
//  SimpleCocoaGUIAppDelegate.m
//

#import "SimpleCocoaGUIAppDelegate.h"

@implementation SimpleCocoaGUIAppDelegate

@synthesize window = _window;

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {

//	[self initializeVariables];
	[self createUI];
//	[self initViECocoaTest];
//	[self NSLogVideoCodecs];
//	[self startViECocoaTest];
	
//	[self startLoopback];
	
	[self ioLooback];
}

-(void)createUI{
	
	NSRect outWindow1Frame = NSMakeRect(200, 200, 200, 200);
	NSWindow* outWindow1 = [[NSWindow alloc] initWithContentRect:outWindow1Frame styleMask:NSTitledWindowMask backing:NSBackingStoreBuffered defer:NO];
	[outWindow1 orderOut:nil];
	NSRect vieAutotestCocoaRenderView1Frame = NSMakeRect(0, 0, 200, 200);
	_vieCocoaRenderView1 = [[ViECocoaRenderView alloc] initWithFrame:vieAutotestCocoaRenderView1Frame];
	[[outWindow1 contentView] addSubview:_vieCocoaRenderView1];
	[outWindow1 setTitle:[NSString stringWithFormat:@"window1"]];
	[outWindow1 makeKeyAndOrderFront:NSApp];	

	
	NSRect outWindow2Frame = NSMakeRect(400, 200, 200, 200);
	NSWindow* outWindow2 = [[NSWindow alloc] initWithContentRect:outWindow2Frame styleMask:NSTitledWindowMask backing:NSBackingStoreBuffered defer:NO];
	[outWindow2 orderOut:nil];
	NSRect vieAutotestCocoaRenderView2Frame = NSMakeRect(0, 0, 200, 200);
	_vieCocoaRenderView2 = [[ViECocoaRenderView alloc] initWithFrame:vieAutotestCocoaRenderView2Frame];
	[[outWindow2 contentView] addSubview:_vieCocoaRenderView2];
	[outWindow2 setTitle:[NSString stringWithFormat:@"window2"]];
	[outWindow2 makeKeyAndOrderFront:NSApp];	
	
	
	




}

-(void)initViECocoaTest{
	
	int _error = 0;
    _ptrViE = VideoEngine::Create();
	_ptrViEBase = ViEBase::GetInterface(_ptrViE);
	_error = _ptrViEBase->Init();
		
	_ptrViECapture = ViECapture::GetInterface(_ptrViE);
	_ptrViERender = ViERender::GetInterface(_ptrViE);
	_ptrViECodec = ViECodec::GetInterface(_ptrViE);	
	_ptrViENetwork = ViENetwork::GetInterface(_ptrViE);
	

	_error = _ptrViE->SetTraceFile("ViEBaseStandardTest.txt");
    _error = _ptrViE->SetEncryptedTraceFile("ViEBaseStandardTestEncrypted.txt");

	
}


-(void)initializeVariables{
	_fullScreen = YES;
}

-(void)NSLogVideoCodecs{
	NSLog(@"Searching for video codecs.....");

	VideoCodec videoCodec;
    memset(&videoCodec, 0, sizeof(VideoCodec));
    for(int index = 0; index < _ptrViECodec->NumberOfCodecs(); index++)
    {
        ViE_TEST(_ptrViECodec->GetCodec(index, videoCodec));
		NSLog(@"Video codec found: %s", videoCodec.plName);
    }	
	
}
-(void)startViECocoaTest{




    int error=0;

    char deviceName[128];
    char deviceUniqueName[512];
    int captureId = 0;
    int dummy = 0;

	//ViE_TEST(_ptrViEBase->CreateChannel(_videoChannel));
    //ViE_TEST(_ptrViECapture->GetCaptureDevice(0,deviceName,sizeof(deviceName),deviceUniqueName,sizeof(deviceUniqueName)));
    //ViE_TEST(_ptrViECapture->AllocateCaptureDevice(deviceUniqueName,sizeof(deviceUniqueName),captureId));
    //ViE_TEST(_ptrViECapture->AllocateCaptureDevice("dummydevicethatdoesnotexist",sizeof(deviceUniqueName),dummy));

    char	captureDeviceName[V_DEVICE_NAME_LENGTH] = "";
    char	captureDeviceUniqueId[V_DEVICE_NAME_LENGTH] = "";
	int		captureDeviceId = 0;
	
	
	
	ViE_TEST(_ptrViE->SetTraceFilter(webrtc::TR_ALL));
    ViE_TEST(_ptrViE->SetTraceFile("ViECocoaTrace.txt"));
    ViE_TEST(_ptrViE->SetEncryptedTraceFile("ViECocoaEncryptedTrace.txt"));

	
	
	
	// base
    ViE_TEST(_ptrViEBase->CreateChannel(_videoChannel));
    
	// capture device
    ViE_TEST(_ptrViECapture->GetCaptureDevice(V_CAPTURE_DEVICE_INDEX, captureDeviceName, V_DEVICE_NAME_LENGTH, captureDeviceUniqueId, V_DEVICE_NAME_LENGTH));    
	ViE_TEST(_ptrViECapture->AllocateCaptureDevice(captureDeviceUniqueId, V_DEVICE_NAME_LENGTH, captureDeviceId));
    ViE_TEST(_ptrViECapture->ConnectCaptureDevice(captureDeviceId, _videoChannel));
    ViE_TEST(_ptrViECapture->StartCapture(captureDeviceId));
	
	// renderer
    ViE_TEST(_ptrViERender->AddRenderer(captureDeviceId,  (void*)_vieCocoaRenderView1, 0, 0.0, 0.0, 1.0, 1.0));
    ViE_TEST(_ptrViERender->StartRender(captureDeviceId));
//	usleep(3 * 1000);
//	ViE_TEST(_ptrViERender->RemoveRenderer(captureDeviceId));
	//exit(0);

	
//	// codec
//	[self NSLogVideoCodecs];
//	VideoCodec videoCodec;
//    memset(&videoCodec, 0, sizeof(VideoCodec));
//	ViE_TEST(_ptrViECodec->GetCodec(V_CODEC_INDEX, videoCodec));
//	ViE_TEST(_ptrViECodec->SetReceiveCodec(_videoChannel, videoCodec));
//	ViE_TEST(_ptrViECodec->SetSendCodec(_videoChannel, videoCodec));
//	
//	// network + base
//	ViE_TEST(_ptrViENetwork->SetLocalReceiver(_videoChannel, V_RTP_PORT)); 
//	ViE_TEST(_ptrViEBase->StartReceive(_videoChannel));
//    ViE_TEST(_ptrViENetwork->SetSendDestination(_videoChannel, V_IP_ADDRESS, V_RTP_PORT));	
//    ViE_TEST(_ptrViEBase->StartSend(_videoChannel));
//	ViE_TEST(_ptrViERender->MirrorRenderStream(captureDeviceId, true, false, true));
	
	
}

-(int)initLoopback
{
	
}
-(int)startLoopback
{
	//********************************************************
    //  Begin create/initialize  Video Engine for testing
    //********************************************************	
	
    int error = 0;
    bool succeeded = true;
    int numberOfErrors = 0;
    std::string str;
    
	//
    // Create a  VideoEngine instance
    //
//    VideoEngine* ptrViE = NULL;
    ptrViE = VideoEngine::Create();
    if (ptrViE == NULL)
    {
        printf("ERROR in VideoEngine::Create\n");
        return -1;
    }
	
	error = ptrViE->SetTraceFilter(webrtc::TR_ALL);
	if (error == -1)
    {
        printf("ERROR in VideoEngine::SetTraceLevel\n");
        return -1;
    }
	
	
    error = ptrViE->SetTraceFile("ViETrace.txt");
    if (error == -1)
    {
        printf("ERROR in VideoEngine::SetTraceFile\n");
        return -1;
    }
	
    error = ptrViE->SetEncryptedTraceFile("ViEEncryptedTrace.txt");
    if (error == -1)
    {
        printf("ERROR in VideoEngine::SetEncryptedTraceFile\n");
        return -1;
    }
	
    //
    // Init  VideoEngine and create a channel
    //
    ptrViEBase = ViEBase::GetInterface(ptrViE);
    if (ptrViEBase == NULL)
    {
        printf("ERROR in ViEBase::GetInterface\n");
        return -1;
    }
	
    error = ptrViEBase->Init();
    if (error == -1)
    {
        printf("ERROR in ViEBase::Init\n");
        return -1;
    }
	
    int videoChannel = -1;
    error = ptrViEBase->CreateChannel(_videoChannel);
    if (error == -1)
    {
        printf("ERROR in ViEBase::CreateChannel\n");
        return -1;
    }
	
    //
    // List available capture devices, allocate and connect.
    //
	ptrViECapture = ViECapture::GetInterface(ptrViE);
    if (ptrViEBase == NULL)
    {
        printf("ERROR in ViECapture::GetInterface\n");
        return -1;
    }
	
    const unsigned int KMaxDeviceNameLength = 128;
    const unsigned int KMaxUniqueIdLength = 256;
    char deviceName[KMaxDeviceNameLength];
    memset(deviceName, 0, KMaxDeviceNameLength);
    char uniqueId[KMaxUniqueIdLength];
    memset(uniqueId, 0, KMaxUniqueIdLength);

    std::cout << std::endl;
    std::cout << "Available capture devices:" << std::endl;
    unsigned int captureIdx = 0;
    for (captureIdx = 0;
         captureIdx < ptrViECapture->NumberOfCaptureDevices();
         captureIdx++)
    {
        memset(deviceName, 0, KMaxDeviceNameLength);
        memset(uniqueId, 0, KMaxUniqueIdLength);
		
        error = ptrViECapture->GetCaptureDevice(captureIdx,
														deviceName, KMaxDeviceNameLength, uniqueId, KMaxUniqueIdLength);
        if (error == -1)
        {
            printf("ERROR in ViECapture::GetCaptureDevice\n");
            return -1;
        }
        std::cout << "   " << captureIdx+1 << ". " << deviceName
		<< std::endl;
    }
    std::cout << std::endl;
    std::cout << "Choose capture devices: ";
//    std::getline(std::cin, str);
//    captureIdx = atoi(str.c_str()) - 1;
	captureIdx = 0;
    error = ptrViECapture->GetCaptureDevice(captureIdx, deviceName,
													KMaxDeviceNameLength, uniqueId, KMaxUniqueIdLength);
    if (error == -1)
    {
        printf("ERROR in ViECapture::GetCaptureDevice\n");
        return -1;
    }
	
    _captureId = 0;
    error = ptrViECapture->AllocateCaptureDevice(uniqueId,
														 KMaxUniqueIdLength, _captureId);
    if (error == -1)
    {
        printf("ERROR in ViECapture::AllocateCaptureDevice\n");
        return -1;
    }
	
    error = ptrViECapture->ConnectCaptureDevice(_captureId,
														_videoChannel);
    if (error == -1)
    {
        printf("ERROR in ViECapture::ConnectCaptureDevice\n");
        return -1;
    }
	
    error = ptrViECapture->StartCapture(_captureId);
    if (error == -1)
    {
        printf("ERROR in ViECapture::StartCapture\n");
        return -1;
    }
	
    //
    // RTP/RTCP settings
    //
	ptrViERtpRtcp = ViERTP_RTCP::GetInterface(ptrViE);
    if (ptrViERtpRtcp == NULL)
    {
        printf("ERROR in ViERTP_RTCP::GetInterface\n");
        return -1;
    }
	
    error = ptrViERtpRtcp->SetRTCPStatus(_videoChannel,
												 kRtcpCompound_RFC4585);
    if (error == -1)
    {
        printf("ERROR in ViERTP_RTCP::SetRTCPStatus\n");
        return -1;
    }
	
    error = ptrViERtpRtcp->SetKeyFrameRequestMethod(_videoChannel,
															kViEKeyFrameRequestPliRtcp);
    if (error == -1)
    {
        printf("ERROR in ViERTP_RTCP::SetKeyFrameRequestMethod\n");
        return -1;
    }
	
    error = ptrViERtpRtcp->SetTMMBRStatus(_videoChannel, true);
    if (error == -1)
    {
        printf("ERROR in ViERTP_RTCP::SetTMMBRStatus\n");
        return -1;
    }
	
    //
    // Set up rendering
    //
    ptrViERender = ViERender::GetInterface(ptrViE);
	if (ptrViERender == NULL)
    {
        printf("ERROR in ViERender::GetInterface\n");
        return -1;
    }
	
    error = ptrViERender->AddRenderer(_captureId, _vieCocoaRenderView1,
											  0, 0.0, 0.0, 1.0, 1.0);
    if (error == -1)
    {
        printf("ERROR in ViERender::AddRenderer\n");
        return -1;
    }
	
    error = ptrViERender->StartRender(_captureId);
    if (error == -1)
    {
        printf("ERROR in ViERender::StartRender\n");
        return -1;
    }
	
    error = ptrViERender->AddRenderer(_videoChannel, _vieCocoaRenderView2,
											  1, 0.0, 0.0, 1.0, 1.0);
    if (error == -1)
    {
        printf("ERROR in ViERender::AddRenderer\n");
        return -1;
    }
	
    error = ptrViERender->StartRender(_videoChannel);
    if (error == -1)
    {
        printf("ERROR in ViERender::StartRender\n");
        return -1;
    }
	
    //
    // Setup codecs
    //
    ptrViECodec = ViECodec::GetInterface(ptrViE);
    if (ptrViECodec == NULL)
    {
        printf("ERROR in ViECodec::GetInterface\n");
        return -1;
    }    
	
    std::cout << std::endl;
    std::cout << "Available codecs:" << std::endl;
	
    // Check available codecs and prepare receive codecs
    VideoCodec videoCodec;
    memset(&videoCodec, 0, sizeof(VideoCodec));
    unsigned int codecIdx = 0;
    for (codecIdx = 0;
         codecIdx < ptrViECodec->NumberOfCodecs();
         codecIdx++)
    {
        error = ptrViECodec->GetCodec(codecIdx, videoCodec);
        if (error == -1)
        {
            printf("ERROR in ViECodec::GetCodec\n");
            return -1;
        }
		
        error = ptrViECodec->SetReceiveCodec(_videoChannel,
													 videoCodec);
        if (error == -1)
        {
            printf("ERROR in ViECodec::SetReceiveCodec\n");
            return -1;
        }
        if (videoCodec.codecType != kVideoCodecRED &&
            videoCodec.codecType != kVideoCodecULPFEC)
        {
            std::cout << "   " << codecIdx+1 << ". " << videoCodec.plName
			<< std::endl;
        }
    }
//    std::cout << std::endl;
//    std::cout << "Choose codec: ";
//    std::getline(std::cin, str);
//    codecIdx = atoi(str.c_str()) - 1;
	codecIdx = 0;
	
    error = ptrViECodec->GetCodec(codecIdx, videoCodec);
    if (error == -1)
    {
        printf("ERROR in ViECodec::GetCodec\n");
        return -1;
    }
	
    error = ptrViECodec->SetSendCodec(_videoChannel, videoCodec);
    if (error == -1)
    {
        printf("ERROR in ViECodec::SetSendCodec\n");
        return -1;
    }
	
    //
    // Address settings
    //
    ptrViENetwork = ViENetwork::GetInterface(ptrViE);
    if (ptrViENetwork == NULL)
    {
        printf("ERROR in ViENetwork::GetInterface\n");
        return -1;
    }
	
    const char* ipAddress = "127.0.0.1";
    const unsigned short rtpPort = 6000;
    error = ptrViENetwork->SetLocalReceiver(_videoChannel, rtpPort);
    if (error == -1)
    {
        printf("ERROR in ViENetwork::SetLocalReceiver\n");
        return -1;
    }
    
    error = ptrViEBase->StartReceive(_videoChannel);
    if (error == -1)
    {
        printf("ERROR in ViENetwork::StartReceive\n");
        return -1;
    }
	
    error = ptrViENetwork->SetSendDestination(_videoChannel,
													  ipAddress, rtpPort);
    if (error == -1)
    {
        printf("ERROR in ViENetwork::SetSendDestination\n");
        return -1;
    }
	
    error = ptrViEBase->StartSend(_videoChannel);
    if (error == -1)
    {
        printf("ERROR in ViENetwork::StartSend\n");
        return -1;
    }
	
	
    //********************************************************
    //  Engine started
    //********************************************************
	
	
    // Call started
    std::cout << std::endl;
    std::cout << "Loopback call started" << std::endl;
//    std::cout << std::endl << std::endl;
//    std::cout << "Press enter to stop...";
//    std::getline(std::cin, str);
}

-(int)stopLooback
{
	int error = 0;
	
	
	
    //********************************************************
    //  Testing finished. Tear down Video Engine
    //********************************************************
	
    error = ptrViEBase->StopReceive(_videoChannel);
    if (error == -1)
    {
        printf("ERROR in ViEBase::StopReceive\n");
        return -1;
    }
	
    error = ptrViEBase->StopSend(_videoChannel);
    if (error == -1)
    {
        printf("ERROR in ViEBase::StopSend\n");
        return -1;
    }
	
    error = ptrViERender->StopRender(_captureId);
    if (error == -1)
    {
        printf("ERROR in ViERender::StopRender\n");
        return -1;
    }
	
    error = ptrViERender->RemoveRenderer(_captureId);
    if (error == -1)
    {
        printf("ERROR in ViERender::RemoveRenderer\n");
        return -1; 
    }
	
    error = ptrViERender->StopRender(_videoChannel);
    if (error == -1)
    {
        printf("ERROR in ViERender::StopRender\n");
        return -1;
    }
	
    error = ptrViERender->RemoveRenderer(_videoChannel);
    if (error == -1)
    {
        printf("ERROR in ViERender::RemoveRenderer\n");
        return -1;
    }
	
    error = ptrViECapture->StopCapture(_captureId);
    if (error == -1)
    {
        printf("ERROR in ViECapture::StopCapture\n");
        return -1; 
    }
	
    error = ptrViECapture->DisconnectCaptureDevice(_videoChannel);
    if (error == -1)
    {
        printf("ERROR in ViECapture::DisconnectCaptureDevice\n");
        return -1;
    }
	
    error = ptrViECapture->ReleaseCaptureDevice(_captureId);
    if (error == -1)
    {
        printf("ERROR in ViECapture::ReleaseCaptureDevice\n");
        return -1;
    }
	
    error = ptrViEBase->DeleteChannel(_videoChannel);
    if (error == -1)
    {
        printf("ERROR in ViEBase::DeleteChannel\n");
        return -1;
    }
	
    int remainingInterfaces = 0;
    remainingInterfaces = ptrViECodec->Release();
    remainingInterfaces += ptrViECapture->Release();
    remainingInterfaces += ptrViERtpRtcp->Release();
    remainingInterfaces += ptrViERender->Release();
    remainingInterfaces += ptrViENetwork->Release();
    remainingInterfaces += ptrViEBase->Release();
    if (remainingInterfaces > 0)
    {
        printf("ERROR: Could not release all interfaces\n");
        return -1;
    }
	
    bool deleted = VideoEngine::Delete(ptrViE);
    if (deleted == false)
    {
        printf("ERROR in VideoEngine::Delete\n");
        return -1;
    }
	
    return 0;
	
	// ===================================================================
    //
    // END:  VideoEngine 3.0 Sample Code
    //
    // ===================================================================
	
	
}

-(int)ioLooback
{
    //********************************************************
    //  Begin create/initialize  Video Engine for testing
    //********************************************************	
	
    int error = 0;
    bool succeeded = true;
    int numberOfErrors = 0;
    std::string str;
    
	//
    // Create a  VideoEngine instance
    //
    VideoEngine* ptrViE = NULL;
    ptrViE = VideoEngine::Create();
    if (ptrViE == NULL)
    {
        printf("ERROR in VideoEngine::Create\n");
        return -1;
    }
	
	error = ptrViE->SetTraceFilter(webrtc::TR_ALL);
	if (error == -1)
    {
        printf("ERROR in VideoEngine::SetTraceLevel\n");
        return -1;
    }
	
	
    error = ptrViE->SetTraceFile("ViETrace.txt");
    if (error == -1)
    {
        printf("ERROR in VideoEngine::SetTraceFile\n");
        return -1;
    }
	
    error = ptrViE->SetEncryptedTraceFile("ViEEncryptedTrace.txt");
    if (error == -1)
    {
        printf("ERROR in VideoEngine::SetEncryptedTraceFile\n");
        return -1;
    }
	
    //
    // Init  VideoEngine and create a channel
    //
    ViEBase* ptrViEBase = ViEBase::GetInterface(ptrViE);
    if (ptrViEBase == NULL)
    {
        printf("ERROR in ViEBase::GetInterface\n");
        return -1;
    }
	
    error = ptrViEBase->Init();
    if (error == -1)
    {
        printf("ERROR in ViEBase::Init\n");
        return -1;
    }
	
    int videoChannel = -1;
    error = ptrViEBase->CreateChannel(videoChannel);
    if (error == -1)
    {
        printf("ERROR in ViEBase::CreateChannel\n");
        return -1;
    }
	
    //
    // List available capture devices, allocate and connect.
    //
    ViECapture* ptrViECapture = 
	ViECapture::GetInterface(ptrViE);
    if (ptrViEBase == NULL)
    {
        printf("ERROR in ViECapture::GetInterface\n");
        return -1;
    }
	
    const unsigned int KMaxDeviceNameLength = 128;
    const unsigned int KMaxUniqueIdLength = 256;
    char deviceName[KMaxDeviceNameLength];
    memset(deviceName, 0, KMaxDeviceNameLength);
    char uniqueId[KMaxUniqueIdLength];
    memset(uniqueId, 0, KMaxUniqueIdLength);
	
    std::cout << std::endl;
    std::cout << "Available capture devices:" << std::endl;
    unsigned int captureIdx = 0;
    for (captureIdx = 0;
         captureIdx < ptrViECapture->NumberOfCaptureDevices();
         captureIdx++)
    {
        memset(deviceName, 0, KMaxDeviceNameLength);
        memset(uniqueId, 0, KMaxUniqueIdLength);
		
        error = ptrViECapture->GetCaptureDevice(captureIdx,
														deviceName, KMaxDeviceNameLength, uniqueId, KMaxUniqueIdLength);
        if (error == -1)
        {
            printf("ERROR in ViECapture::GetCaptureDevice\n");
            return -1;
        }
        std::cout << "   " << captureIdx+1 << ". " << deviceName
		<< std::endl;
    }
    std::cout << std::endl;
    std::cout << "Choose capture devices: ";
//    std::getline(std::cin, str);
//    captureIdx = atoi(str.c_str()) - 1;
	captureIdx = 0;
    error = ptrViECapture->GetCaptureDevice(captureIdx, deviceName,
													KMaxDeviceNameLength, uniqueId, KMaxUniqueIdLength);
    if (error == -1)
    {
        printf("ERROR in ViECapture::GetCaptureDevice\n");
        return -1;
    }
	
    int captureId = 0;
    error = ptrViECapture->AllocateCaptureDevice(uniqueId,
														 KMaxUniqueIdLength, captureId);
    if (error == -1)
    {
        printf("ERROR in ViECapture::AllocateCaptureDevice\n");
        return -1;
    }
	
    error = ptrViECapture->ConnectCaptureDevice(captureId,
														videoChannel);
    if (error == -1)
    {
        printf("ERROR in ViECapture::ConnectCaptureDevice\n");
        return -1;
    }
	
    error = ptrViECapture->StartCapture(captureId);
    if (error == -1)
    {
        printf("ERROR in ViECapture::StartCapture\n");
        return -1;
    }
	
    //
    // RTP/RTCP settings
    //
    ViERTP_RTCP* ptrViERtpRtcp =
	ViERTP_RTCP::GetInterface(ptrViE);
    if (ptrViERtpRtcp == NULL)
    {
        printf("ERROR in ViERTP_RTCP::GetInterface\n");
        return -1;
    }
	
    error = ptrViERtpRtcp->SetRTCPStatus(videoChannel,
												 kRtcpCompound_RFC4585);
    if (error == -1)
    {
        printf("ERROR in ViERTP_RTCP::SetRTCPStatus\n");
        return -1;
    }
	
    error = ptrViERtpRtcp->SetKeyFrameRequestMethod(videoChannel,
															kViEKeyFrameRequestPliRtcp);
    if (error == -1)
    {
        printf("ERROR in ViERTP_RTCP::SetKeyFrameRequestMethod\n");
        return -1;
    }
	
    error = ptrViERtpRtcp->SetTMMBRStatus(videoChannel, true);
    if (error == -1)
    {
        printf("ERROR in ViERTP_RTCP::SetTMMBRStatus\n");
        return -1;
    }
	
    //
    // Set up rendering
    //
    ViERender* ptrViERender =
	ViERender::GetInterface(ptrViE);
	if (ptrViERender == NULL)
    {
        printf("ERROR in ViERender::GetInterface\n");
        return -1;
    }
	
//    error = ptrViERender->EnableFullScreenRender(_vieCocoaRenderView1);
//    if (error == -1)
//    {
//        printf("ERROR in ViERender::AddRenderer\n");
//        return -1;
//    }	
	
	
    error = ptrViERender->AddRenderer(captureId, _vieCocoaRenderView1,
											0, 0.5, 0.5, 1.0, 1.0);
    if (error == -1)
    {
        printf("ERROR in ViERender::AddRenderer\n");
        return -1;
    }
	
    error = ptrViERender->StartRender(captureId);
    if (error == -1)
    {
        printf("ERROR in ViERender::StartRender\n");
        return -1;
    }
	
    error = ptrViERender->AddRenderer(videoChannel, _vieCocoaRenderView2,
											  1, 0.0, 0.0, 1.0, 1.0);
    if (error == -1)
    {
        printf("ERROR in ViERender::AddRenderer\n");
        return -1;
    }
	
    error = ptrViERender->StartRender(videoChannel);
    if (error == -1)
    {
        printf("ERROR in ViERender::StartRender\n");
        return -1;
    }
	
    //
    // Setup codecs
    //
    ViECodec* ptrViECodec = ViECodec::GetInterface(ptrViE);
    if (ptrViECodec == NULL)
    {
        printf("ERROR in ViECodec::GetInterface\n");
        return -1;
    }    
	
    std::cout << std::endl;
    std::cout << "Available codecs:" << std::endl;
	
    // Check available codecs and prepare receive codecs
    VideoCodec videoCodec;
    memset(&videoCodec, 0, sizeof(VideoCodec));
    unsigned int codecIdx = 0;
    for (codecIdx = 0;
         codecIdx < ptrViECodec->NumberOfCodecs();
         codecIdx++)
    {
        error = ptrViECodec->GetCodec(codecIdx, videoCodec);
        if (error == -1)
        {
            printf("ERROR in ViECodec::GetCodec\n");
            return -1;
        }
		
        error = ptrViECodec->SetReceiveCodec(videoChannel,
													 videoCodec);
        if (error == -1)
        {
            printf("ERROR in ViECodec::SetReceiveCodec\n");
            return -1;
        }
        if (videoCodec.codecType != kVideoCodecRED &&
            videoCodec.codecType != kVideoCodecULPFEC)
        {
            std::cout << "   " << codecIdx+1 << ". " << videoCodec.plName
			<< std::endl;
        }
    }
    std::cout << std::endl;
    std::cout << "Choose codec: ";
//    std::getline(std::cin, str);
//    codecIdx = atoi(str.c_str()) - 1;
	
	
	error = ptrViECapture->ShowCaptureSettingsDialogBox("unique",10, "mytitle");
	codecIdx = 0;
    error = ptrViECodec->GetCodec(codecIdx, videoCodec);
    if (error == -1)
    {
        printf("ERROR in ViECodec::GetCodec\n");
        return -1;
    }
	
    error = ptrViECodec->SetSendCodec(videoChannel, videoCodec);
    if (error == -1)
    {
        printf("ERROR in ViECodec::SetSendCodec\n");
        return -1;
    }
	
    //
    // Address settings
    //
    ViENetwork* ptrViENetwork =
	ViENetwork::GetInterface(ptrViE);
    if (ptrViENetwork == NULL)
    {
        printf("ERROR in ViENetwork::GetInterface\n");
        return -1;
    }
	
    const char* ipAddress = "127.0.0.1";
    const unsigned short rtpPort = 6000;
    error = ptrViENetwork->SetLocalReceiver(videoChannel, rtpPort);
    if (error == -1)
    {
        printf("ERROR in ViENetwork::SetLocalReceiver\n");
        return -1;
    }
    
    error = ptrViEBase->StartReceive(videoChannel);
    if (error == -1)
    {
        printf("ERROR in ViENetwork::StartReceive\n");
        return -1;
    }
	
    error = ptrViENetwork->SetSendDestination(videoChannel,
													  ipAddress, rtpPort);
    if (error == -1)
    {
        printf("ERROR in ViENetwork::SetSendDestination\n");
        return -1;
    }
	
    error = ptrViEBase->StartSend(videoChannel);
    if (error == -1)
    {
        printf("ERROR in ViENetwork::StartSend\n");
        return -1;
    }
	
	
    //********************************************************
    //  Engine started
    //********************************************************
	
	
    // Call started
    std::cout << std::endl;
    std::cout << "Loopback call started" << std::endl;
    std::cout << std::endl << std::endl;
    std::cout << "Press enter to stop...";
//	[[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:1]];
//    std::getline(std::cin, str);
	usleep(5 * 1000 * 1000);
	
	//int i = 0;
//	while(1)
//	{
//		NSLog(@"app iteration %d", i);
//		i++;
//		[[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:1]];
//		std::getline(std::cin, str);
//		if(i > 3)
//		{
//			break;
//		}
//	}
	
    //********************************************************
    //  Testing finished. Tear down Video Engine
    //********************************************************
	
    error = ptrViEBase->StopReceive(videoChannel);
    if (error == -1)
    {
        printf("ERROR in ViEBase::StopReceive\n");
        return -1;
    }
	
    error = ptrViEBase->StopSend(videoChannel);
    if (error == -1)
    {
        printf("ERROR in ViEBase::StopSend\n");
        return -1;
    }
	
    error = ptrViERender->StopRender(captureId);
    if (error == -1)
    {
        printf("ERROR in ViERender::StopRender\n");
        return -1;
    }
	
    error = ptrViERender->RemoveRenderer(captureId);
    if (error == -1)
    {
        printf("ERROR in ViERender::RemoveRenderer\n");
        return -1; 
    }
	
    error = ptrViERender->StopRender(videoChannel);
    if (error == -1)
    {
        printf("ERROR in ViERender::StopRender\n");
        return -1;
    }
	
    error = ptrViERender->RemoveRenderer(videoChannel);
    if (error == -1)
    {
        printf("ERROR in ViERender::RemoveRenderer\n");
        return -1;
    }
	
    error = ptrViECapture->StopCapture(captureId);
    if (error == -1)
    {
        printf("ERROR in ViECapture::StopCapture\n");
        return -1; 
    }
	
    error = ptrViECapture->DisconnectCaptureDevice(videoChannel);
    if (error == -1)
    {
        printf("ERROR in ViECapture::DisconnectCaptureDevice\n");
        return -1;
    }
	
    error = ptrViECapture->ReleaseCaptureDevice(captureId);
    if (error == -1)
    {
        printf("ERROR in ViECapture::ReleaseCaptureDevice\n");
        return -1;
    }
	
    error = ptrViEBase->DeleteChannel(videoChannel);
    if (error == -1)
    {
        printf("ERROR in ViEBase::DeleteChannel\n");
        return -1;
    }
	
    int remainingInterfaces = 0;
    remainingInterfaces = ptrViECodec->Release();
    remainingInterfaces += ptrViECapture->Release();
    remainingInterfaces += ptrViERtpRtcp->Release();
    remainingInterfaces += ptrViERender->Release();
    remainingInterfaces += ptrViENetwork->Release();
    remainingInterfaces += ptrViEBase->Release();
    if (remainingInterfaces > 0)
    {
        printf("ERROR: Could not release all interfaces\n");
        return -1;
    }
	
    bool deleted = VideoEngine::Delete(ptrViE);
    if (deleted == false)
    {
        printf("ERROR in VideoEngine::Delete\n");
        return -1;
    }
	
	NSLog(@"Finished function");
    return 0;
	
    //
    // END:  VideoEngine 3.0 Sample Code
    //
    // ===================================================================
}




-(IBAction)handleRestart:(id)sender
{
//	[self stopLooback];
//	[self startLoopback];
	[self ioLooback];
}
@end
