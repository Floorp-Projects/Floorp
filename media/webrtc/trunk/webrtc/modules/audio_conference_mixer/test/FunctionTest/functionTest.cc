/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>
#include <iostream>
#include <time.h>

#include "functionTest.h"
#include "event_wrapper.h"
#include "trace.h"
#include "thread_wrapper.h"
#include "webrtc_vad.h"

#if (defined(WEBRTC_LINUX) || defined(WEBRTC_MAC))
   #include <sys/stat.h>
   #define MY_PERMISSION_MASK S_IRWXU | S_IRWXG | S_IRWXO
   #define MKDIR(directory) mkdir(directory,MY_PERMISSION_MASK)
#else // defined(WINDOWS)
   #include <direct.h>
   #define MKDIR(directory) mkdir(directory)
#endif

int main(int /*argc*/, char* /*argv[]*/)
{
    // Initialize random number generator
    //unsigned int seed = 1220716312; // just a seed that can be used
    unsigned int seed = (unsigned)time( NULL );
    srand(seed);
    std::cout << "Starting function test. Seed = " << seed << std::endl;
    std::cout << "Press enter to continue" << std::endl;
    getchar();
    MixerWrapper* testInstance1 = MixerWrapper::CreateMixerWrapper();
    MixerWrapper* testInstance2 = MixerWrapper::CreateMixerWrapper();
    if((testInstance1 == NULL) ||
       (testInstance2 == NULL))
    {
        assert(false);
        return 0;
    }

    char versionString[256] = "";
    WebRtc_UWord32 remainingBufferInBytes = 256;
    WebRtc_UWord32 position = 0;
    AudioConferenceMixer::GetVersion(versionString,remainingBufferInBytes,position);

    int read = 1;
    while(read != 0)
    {
        std::cout << versionString << std::endl;
        std::cout << "--------Menu-----------" << std::endl;
        std::cout << std::endl;
        std::cout << "0. Quit" << std::endl;
        std::cout << "2. StartMixing" << std::endl;
        std::cout << "3. StopMixing" << std::endl;
        std::cout << "4. Create participant(s)" << std::endl;
        std::cout << "5. Delete participant(s)" << std::endl;
        std::cout << "6. List participants " << std::endl;
        std::cout << "7. Print mix status " << std::endl;
        std::cout << "8. Run identical scenario:" << std::endl;
        std::cout << "   a. 1 VIP,       3 regular, amount of mixed = 3"  << std::endl;
        std::cout << "   b. 1 anonymous, 3 regular, amount of mixed = 2"  << std::endl;
        scanf("%i",&read);
        getchar();
        MixerParticipant::ParticipantType participantType;
        int option = 0;
        WebRtc_UWord32 id = 0;
        ListItem* item = NULL;
        ListWrapper participants;
        if(read == 0)
        {
            // donothing
        }
        else if(read == 1)
        {
        }
        else if(read == 2)
        {
            testInstance1->StartMixing();
        }
        else if(read == 3)
        {
            testInstance1->StopMixing();
        }
        else if(read == 4)
        {
            while(true)
            {
                std::cout << "VIP(music)       = " << MixerParticipant::VIP << std::endl;
                std::cout << "Regular(speech)  = " << MixerParticipant::REGULAR << std::endl;
                std::cout << "Anonymous(music) = " << MixerParticipant::MIXED_ANONYMOUS << std::endl;
                std::cout << "Select type of participant: ";
                scanf("%i",&option);
                if(option == MixerParticipant::VIP ||
                   option == MixerParticipant::REGULAR ||
                   option == MixerParticipant::MIXED_ANONYMOUS)
                {
                    break;
                }
            }
            participantType = (MixerParticipant::ParticipantType)option;
            testInstance1->CreateParticipant(participantType);
        }
        else if(read == 5)
        {
            std::cout << "Select participant to delete: ";
            scanf("%i",&option);
            id = option;
            testInstance1->DeleteParticipant(id);
            break;
        }
        else if(read == 6)
        {
            testInstance1->GetParticipantList(participants);
            item = participants.First();
            std::cout << "The following participants have been created: " << std::endl;
            while(item)
            {
                WebRtc_UWord32 id = item->GetUnsignedItem();
                std::cout << id;
                item = participants.Next(item);
                if(item != NULL)
                {
                    std::cout << ", ";
                }
                else
                {
                    std::cout << std::endl;
                }
            }
        }
        else if(read == 7)
        {
            std::cout << "-------------Mixer Status-------------" << std::endl;
            testInstance1->PrintStatus();
            testInstance2->PrintStatus();
            std::cout << "Press enter to continue";
            getchar();
            std::cout << std::endl;
            std::cout << std::endl;
        }
        else if(read == 8)
        {
            const WebRtc_Word32 amountOfParticipants = 4;
            MixerParticipant::ParticipantType instance1Participants[] =
                                                {MixerParticipant::VIP,
                                                 MixerParticipant::REGULAR,
                                                 MixerParticipant::REGULAR,
                                                 MixerParticipant::REGULAR};
            MixerParticipant::ParticipantType instance2Participants[] =
                                               {MixerParticipant::MIXED_ANONYMOUS,
                                                MixerParticipant::REGULAR,
                                                MixerParticipant::REGULAR,
                                                MixerParticipant::REGULAR};
            for(WebRtc_Word32 i = 0; i < amountOfParticipants; i++)
            {
                WebRtc_Word32 startPosition = 0;
                GenerateRandomPosition(startPosition);
                testInstance1->CreateParticipant(instance1Participants[i],startPosition);
                testInstance2->CreateParticipant(instance2Participants[i],startPosition);
            }
            bool success = true;
            success = testInstance1->StartMixing();
            assert(success);
            success = testInstance2->StartMixing(2);
            assert(success);
        }
    }

    std::cout << "Press enter to stop" << std::endl;
    getchar();
    delete testInstance1;
    delete testInstance2;
    return 0;
}

FileWriter::FileWriter()
    :
    _file(NULL)
{
}

FileWriter::~FileWriter()
{
    if(_file)
    {
        fclose(_file);
    }
}

bool
FileWriter::SetFileName(
    const char* fileName)
{
    if(_file)
    {
        fclose(_file);
    }
    _file = fopen(fileName,"wb");
    return _file != NULL;
}

bool
FileWriter::WriteToFile(
    const AudioFrame& audioFrame)
{
    WebRtc_Word32 written = (WebRtc_Word32)fwrite(audioFrame.data_,sizeof(WebRtc_Word16),audioFrame.samples_per_channel_,_file);
    // Do not flush buffers since that will add (a lot of) delay
    return written == audioFrame.samples_per_channel_;
}

FileReader::FileReader()
    :
    _frequency(kDefaultFrequency),
    _sampleSize((_frequency*kProcessPeriodicityInMs)/1000),
    _timeStamp(0),
    _file(NULL),
    _vadInstr(NULL),
    _automaticVad(false),
    _vad(false)
{
    if(WebRtcVad_Create(&_vadInstr) == 0)
    {
        if(WebRtcVad_Init(_vadInstr) != 0)
        {
            assert(false);
            WebRtcVad_Free(_vadInstr);
            _vadInstr = NULL;
        }
    }
    else
    {
        assert(false);
    }
}

FileReader::~FileReader()
{
    if(_file)
    {
        fclose(_file);
    }
    if(_vadInstr)
    {
        WebRtcVad_Free(_vadInstr);
    }
}

bool
FileReader::SetFileName(
    const char* fileName)
{
    if(_file)
    {
        fclose(_file);
    }
    _file = fopen(fileName,"rb");
    return _file != NULL;
}

bool
FileReader::ReadFromFile(
    AudioFrame& audioFrame)
{

    WebRtc_Word16 buffer[AudioFrame::kMaxDataSizeSamples];
    LoopedFileRead(buffer,AudioFrame::kMaxDataSizeSamples,_sampleSize,_file);

    bool vad = false;
    GetVAD(buffer,_sampleSize,vad);
    AudioFrame::VADActivity activity = vad ? AudioFrame::kVadActive :
                                 AudioFrame::kVadPassive;

    _volumeCalculator.ComputeLevel(buffer,_sampleSize);
    const WebRtc_Word32 level = _volumeCalculator.GetLevel();
    return audioFrame.UpdateFrame(  -1,
                                    _timeStamp,
                                    buffer,
                                    _sampleSize,
                                    _frequency,
                                    AudioFrame::kNormalSpeech,
                                    activity,
                                    0,
                                    level) == 0;

}

bool
FileReader::FastForwardFile(
    const WebRtc_Word32 samples)
{
    WebRtc_Word16* tempBuffer = new WebRtc_Word16[samples];
    bool success = LoopedFileRead(tempBuffer,samples,samples,_file);
    delete[] tempBuffer;
    return success;
}

bool
FileReader::EnableAutomaticVAD(
    bool enable,
    int mode)
{
    if(!_automaticVad &&
       enable)
    {
        if(WebRtcVad_Init(_vadInstr) == -1)
        {
            return false;
        }
    }
    WebRtcVad_set_mode(_vadInstr,mode);
    _automaticVad = enable;
    return true;
}

bool
FileReader::SetVAD(
    bool vad)
{
    if(_automaticVad)
    {
        return false;
    }
    _vad = vad;
    return true;
}

bool
FileReader::GetVAD(
    WebRtc_Word16* buffer,
    WebRtc_UWord8 bufferLengthInSamples,
    bool& vad)
{
    if(_automaticVad)
    {
        WebRtc_Word16 result = WebRtcVad_Process(_vadInstr,_frequency,buffer,bufferLengthInSamples);
        if(result == -1)
        {
            assert(false);
            return false;
        }
        _vad = vad = (result == 1);
    }
    vad = _vad;
    return true;
}

MixerParticipant*
MixerParticipant::CreateParticipant(
    const WebRtc_UWord32 id,
    ParticipantType participantType,
    const WebRtc_Word32 startPosition,
    char* outputPath)
{
    if(participantType == RANDOM)
    {
        participantType = (ParticipantType)(rand() % 3);
    }
    MixerParticipant* participant = new MixerParticipant(id,participantType);
    // Randomize the start position so we only need one input file
    // assume file is smaller than 1 minute wideband = 60 * 16000
    // Always start at a multiple of 10ms wideband
    if(!participant->InitializeFileReader(startPosition) ||
       !participant->InitializeFileWriter(outputPath))
    {
        delete participant;
        return NULL;
    }
    return participant;
}

MixerParticipant::MixerParticipant(
    const WebRtc_UWord32 id,
    ParticipantType participantType)
    :
    _id(id),
    _participantType(participantType),
    _fileReader(),
    _fileWriter()
{
}

MixerParticipant::~MixerParticipant()
{
}

WebRtc_Word32
MixerParticipant::GetAudioFrame(
    const WebRtc_Word32 /*id*/,
    AudioFrame& audioFrame)
{
    if(!_fileReader.ReadFromFile(audioFrame))
    {
        return -1;
    }
    audioFrame._id = _id;
    return 0;
}

WebRtc_Word32
MixerParticipant::MixedAudioFrame(
    const AudioFrame& audioFrame)
{
    return _fileWriter.WriteToFile(audioFrame);
}

WebRtc_Word32
MixerParticipant::GetParticipantType(
    ParticipantType& participantType)
{
    participantType = _participantType;
    return 0;
}

bool
MixerParticipant::InitializeFileReader(
    const WebRtc_Word32 startPositionInSamples)
{
    char fileName[128] = "";
    if(_participantType == REGULAR)
    {
        sprintf(fileName,"convFile.pcm");
    }
    else
    {
        sprintf(fileName,"musicFile.pcm");
    }
    if(!_fileReader.SetFileName(fileName))
    {
        return false;
    }
    if(!_fileReader.EnableAutomaticVAD(true,2))
    {
        assert(false);
    }
    return _fileReader.FastForwardFile(startPositionInSamples);
}

bool
MixerParticipant::InitializeFileWriter(
    char* outputPath)
{
    const WebRtc_Word32 stringsize = 128;
    char fileName[stringsize] = "";
    strncpy(fileName,outputPath,stringsize);
    fileName[stringsize-1] = '\0';

    char tempName[stringsize];
    tempName[0] = '\0';
    sprintf(tempName,"outputFile%d.pcm",(int)_id);
    strncat(fileName,tempName,(stringsize - strlen(fileName)));
    fileName[stringsize-1] = '\0';

    return _fileWriter.SetFileName(fileName);
}

StatusReceiver::StatusReceiver(
    const WebRtc_Word32 id)
    :
    _id(id),
    _mixedParticipants(NULL),
    _mixedParticipantsAmount(0),
    _mixedParticipantsSize(0),
    _vadPositiveParticipants(NULL),
    _vadPositiveParticipantsAmount(0),
    _vadPositiveParticipantsSize(0),
    _mixedAudioLevel(0)
{
}

StatusReceiver::~StatusReceiver()
{
    delete[] _mixedParticipants;
    delete[] _vadPositiveParticipants;
}

void
StatusReceiver::MixedParticipants(
    const WebRtc_Word32 id,
    const ParticipantStatistics* participantStatistics,
    const WebRtc_UWord32 size)
{
    if(id != _id)
    {
        assert(false);
    }
    if(_mixedParticipantsSize < size)
    {
        delete[] _mixedParticipants;
        _mixedParticipantsSize = size;
        _mixedParticipants = new ParticipantStatistics[size];
    }
    _mixedParticipantsAmount = size;
    memcpy(_mixedParticipants,participantStatistics,sizeof(ParticipantStatistics)*size);
}

void
StatusReceiver::VADPositiveParticipants(
    const WebRtc_Word32 id,
    const ParticipantStatistics* participantStatistics,
    const WebRtc_UWord32 size)
{
    if(id != _id)
    {
        assert(false);
    }

    if(_vadPositiveParticipantsSize < size)
    {
        delete[] _vadPositiveParticipants;
        _vadPositiveParticipantsSize = size;
        _vadPositiveParticipants = new ParticipantStatistics[size];
    }
    _vadPositiveParticipantsAmount = size;
    memcpy(_vadPositiveParticipants,participantStatistics,sizeof(ParticipantStatistics)*size);
}

void
StatusReceiver::MixedAudioLevel(
    const WebRtc_Word32  id,
    const WebRtc_UWord32 level)
{
    if(id != _id)
    {
        assert(false);
    }
    _mixedAudioLevel = level;
}

void
StatusReceiver::PrintMixedParticipants()
{
    std::cout << "Mixed participants" << std::endl;
    if(_mixedParticipantsAmount == 0)
    {
        std::cout << "N/A" << std::endl;
    }
    for(WebRtc_UWord16 i = 0; i < _mixedParticipantsAmount; i++)
    {
        std::cout << i + 1 << ". Participant " << _mixedParticipants[i].participant << ": level = " << _mixedParticipants[i].level << std::endl;
    }
}

void
StatusReceiver::PrintVadPositiveParticipants()
{
    std::cout << "VAD positive participants" << std::endl;
    if(_mixedParticipantsAmount == 0)
    {
        std::cout << "N/A"  << std::endl;
    }
    for(WebRtc_UWord16 i = 0; i < _mixedParticipantsAmount; i++)
    {
        std::cout << i + 1 << ". Participant " << _mixedParticipants[i].participant << ": level = " << _mixedParticipants[i].level << std::endl;
    }
}

void
StatusReceiver::PrintMixedAudioLevel()
{
    std::cout << "Mixed audio level = " << _mixedAudioLevel << std::endl;
}

WebRtc_Word32 MixerWrapper::_mixerWrapperIdCounter = 0;

MixerWrapper::MixerWrapper()
    :
    _processThread(NULL),
    _threadId(0),
    _firstProcessCall(true),
    _previousTime(),
    _periodicityInTicks(TickTime::MillisecondsToTicks(FileReader::kProcessPeriodicityInMs)),
    _synchronizationEvent(EventWrapper::Create()),
    _freeItemIds(),
    _itemIdCounter(0),
    _mixerParticipants(),
    _mixerWrappererId(_mixerWrapperIdCounter++),
    _instanceOutputPath(),
    _trace(NULL),
    _statusReceiver(_mixerWrappererId),
    _generalAudioWriter()
{
    sprintf(_instanceOutputPath,"instance%d/",(int)_mixerWrappererId);
    MKDIR(_instanceOutputPath);
    _mixer = AudioConferenceMixer::CreateAudioConferenceMixer(
                                                    _mixerWrappererId);
    if(_mixer != NULL)
    {
        bool success = true;

        success = _mixer->RegisterMixedStreamCallback(*this) == 0;
        assert(success);
        success = _mixer->RegisterMixedStreamCallback(*this) == -1;
        assert(success);
        success = _mixer->UnRegisterMixedStreamCallback() == 0;
        assert(success);
        success = _mixer->UnRegisterMixedStreamCallback() == -1;
        assert(success);
        success = _mixer->RegisterMixedStreamCallback(*this) == 0;
        assert(success);

        success = _mixer->RegisterMixerStatusCallback(_statusReceiver,2) == 0;
        assert(success);
        success = _mixer->RegisterMixerStatusCallback(_statusReceiver,1) == -1;
        assert(success);
        success = _mixer->UnRegisterMixerStatusCallback() == 0;
        assert(success);
        success = _mixer->UnRegisterMixerStatusCallback() == -1;
        assert(success);
        success = _mixer->RegisterMixerStatusCallback(_statusReceiver,1) == 0;
        assert(success);
    }
    else
    {
        assert(false);
        std::cout << "Failed to create mixer instance";
    }
}

MixerWrapper*
MixerWrapper::CreateMixerWrapper()
{
    MixerWrapper* mixerWrapper = new MixerWrapper();
    if(!mixerWrapper->InitializeFileWriter())
    {
        delete mixerWrapper;
        return NULL;
    }
    return mixerWrapper;
}

MixerWrapper::~MixerWrapper()
{
    StopMixing();
    ClearAllItemIds();
    _synchronizationEvent->StopTimer();
    delete _synchronizationEvent;
    delete _mixer;
}

bool
MixerWrapper::CreateParticipant(
    MixerParticipant::ParticipantType participantType)
{
    WebRtc_Word32 startPosition = 0;
    GenerateRandomPosition(startPosition);
    return CreateParticipant(participantType,startPosition);
}

bool
MixerWrapper::CreateParticipant(
    MixerParticipant::ParticipantType participantType,
    const WebRtc_Word32 startPosition)
{
    WebRtc_UWord32 id;
    if(!GetFreeItemIds(id))
    {
        return false;
    }

    MixerParticipant* participant = MixerParticipant::CreateParticipant(id,participantType,startPosition,_instanceOutputPath);
    if(!participant)
    {
        return false;
    }
    if(_mixerParticipants.Insert(id,static_cast<void*>(participant)) != 0)
    {
        delete participant;
        return false;
    }
    if(!StartMixingParticipant(id))
    {
        DeleteParticipant(id);
        return false;
    }
    return true;
}

bool
MixerWrapper::DeleteParticipant(
    const WebRtc_UWord32 id)
{
    bool success = StopMixingParticipant(id);
    if(!success)
    {
        assert(false);
        return false;
    }
    MapItem* item = _mixerParticipants.Find(id);
    if(item == NULL)
    {
        return false;
    }
    MixerParticipant* participant = static_cast<MixerParticipant*>(item->GetItem());
    delete participant;
    _mixerParticipants.Erase(item);
    AddFreeItemIds(id);
    return true;
}

bool
MixerWrapper::StartMixing(
    const WebRtc_UWord32 mixedParticipants)
{
    if(_processThread)
    {
        return false;
    }
    if(_mixer->SetAmountOfMixedParticipants(mixedParticipants) != 0)
    {
        assert(false);
    }
    WebRtc_UWord32 mixedParticipantsTest = 0;
    _mixer->AmountOfMixedParticipants(mixedParticipantsTest);
    assert(mixedParticipantsTest == mixedParticipants);

    if(!_synchronizationEvent->StartTimer(true,10))
    {
        assert(false);
        return false;
    }
    _processThread = ThreadWrapper::CreateThread(Process, this, kLowPriority);
    if(!_processThread->Start(_threadId))
    {
        delete _processThread;
        _processThread = NULL;
        assert(false);
        return false;
    }

    return true;
}

bool
MixerWrapper::StopMixing()
{
    while(_processThread &&
          !_processThread->Stop())
    {}
    _synchronizationEvent->StopTimer();

    delete _processThread;
    _processThread = NULL;
    return true;
}

void
MixerWrapper::NewMixedAudio(
    const WebRtc_Word32 id,
    const AudioFrame& generalAudioFrame,
    const AudioFrame** uniqueAudioFrames,
    const WebRtc_UWord32 size)
{
    if(id < 0)
    {
        assert(false);
    }
    // Store the general audio
    _generalAudioWriter.WriteToFile(generalAudioFrame);

    // Send the unique audio frames to its corresponding participants
    ListWrapper uniqueAudioFrameList;
    for(WebRtc_UWord32 i = 0; i < size; i++)
    {
        WebRtc_UWord32 id = (uniqueAudioFrames[i])->_id;
        MapItem* resultItem = _mixerParticipants.Find(id);
        if(resultItem == NULL)
        {
            assert(false);
            continue;
        }
        MixerParticipant* participant = static_cast<MixerParticipant*>(resultItem->GetItem());
        participant->MixedAudioFrame(*(uniqueAudioFrames[i]));
        uniqueAudioFrameList.PushBack(resultItem->GetItem());
    }

    // Send the general audio frames to the remaining participants
    MapItem* item = _mixerParticipants.First();
    while(item)
    {
        bool isUnique = false;
        ListItem* compareItem = uniqueAudioFrameList.First();
        while(compareItem)
        {
            if(compareItem->GetItem() == item->GetItem())
            {
                isUnique = true;
                break;
            }
            compareItem = uniqueAudioFrameList.Next(compareItem);
        }
        if(!isUnique)
        {
            MixerParticipant* participant = static_cast<MixerParticipant*>(item->GetItem());
            participant->MixedAudioFrame(generalAudioFrame);
        }
        item = _mixerParticipants.Next(item);
    }
}

bool
MixerWrapper::GetParticipantList(
    ListWrapper& participants)
{
    MapItem* item = _mixerParticipants.First();
    while(item)
    {
        participants.PushBack(item->GetId());
        item = _mixerParticipants.Next(item);
    }
    return true;
}

void
MixerWrapper::PrintStatus()
{
    std::cout << "instance " << _mixerWrappererId << std::endl;
    std::cout << std::endl;
    _statusReceiver.PrintMixedParticipants();
    std::cout << std::endl;
    _statusReceiver.PrintVadPositiveParticipants();
    std::cout << std::endl;
    _statusReceiver.PrintMixedAudioLevel();
    std::cout << "---------------------------------------" << std::endl;
}

bool
MixerWrapper::InitializeFileWriter()
{
    const WebRtc_Word32 stringsize = 128;
    char fileName[stringsize] = "";
    strncpy(fileName,_instanceOutputPath,stringsize);
    fileName[stringsize-1] = '\0';

    strncat(fileName,"generalOutputFile.pcm",(stringsize - strlen(fileName)));
    fileName[stringsize-1] = '\0';
    return _generalAudioWriter.SetFileName(fileName);
}

bool
MixerWrapper::Process(
    void* instance)
{
    MixerWrapper* mixerWrapper = static_cast<MixerWrapper*>(instance);
    return mixerWrapper->Process();
}

bool
MixerWrapper::Process()
{
    switch(_synchronizationEvent->Wait(1000))
    {
    case kEventSignaled:
         // Normal operation, ~10 ms has passed
        break;
    case kEventError:
        // Error occured end the thread and throw an assertion
        assert(false);
        return false;
    case kEventTimeout:
        // One second has passed without a timeout something is wrong
        // end the thread and throw an assertion
        assert(false);
        return false;
    }
    WebRtc_Word32 processOfset = 0;
    const TickTime currentTime = TickTime::Now();
    if(_firstProcessCall)
    {
        _previousTime = TickTime::Now();
        _firstProcessCall = false;
    }
    else
    {
        TickInterval deltaTime = (currentTime - _previousTime);
        _previousTime += _periodicityInTicks;
        processOfset = (WebRtc_Word32) deltaTime.Milliseconds();
        processOfset -= FileReader::kProcessPeriodicityInMs;
    }

    _mixer->Process();
    WebRtc_Word32 timeUntilNextProcess = _mixer->TimeUntilNextProcess();
    if(processOfset > FileReader::kProcessPeriodicityInMs)
    {
        std::cout << "Performance Warning: Process running " << processOfset << " too slow" << std::endl;
        _previousTime = currentTime;
        if(timeUntilNextProcess > 0)
        {
            std::cout << "Performance Warning: test performance and module performance missmatch" << std::endl;
        }
    }
    else if(processOfset < -FileReader::kProcessPeriodicityInMs)
    {
        std::cout << "Performance Warning: Process running " << -processOfset << " too fast" << std::endl;
        _previousTime = currentTime;
        if(timeUntilNextProcess < FileReader::kProcessPeriodicityInMs)
        {
            std::cout << "Performance Warning: test performance and module performance missmatch" << std::endl;
        }
    }
    return true;
}


bool
MixerWrapper::StartMixingParticipant(
    const WebRtc_UWord32 id)
{
    MapItem* item = _mixerParticipants.Find(id);
    if(item == NULL)
    {
        return false;
    }
    MixerParticipant* participant = static_cast<MixerParticipant*>(item->GetItem());
    MixerParticipant::ParticipantType participantType = MixerParticipant::REGULAR;
    participant->GetParticipantType(participantType);
    if(participantType == MixerParticipant::MIXED_ANONYMOUS)
    {
        bool anonymouslyMixed = false;
        bool success = _mixer->SetAnonymousMixabilityStatus(*participant,true) == 0;
        assert(success);
        success = _mixer->AnonymousMixabilityStatus(*participant,anonymouslyMixed) == 0;
        assert(success);
        assert(anonymouslyMixed);
        success = _mixer->SetAnonymousMixabilityStatus(*participant,true) == -1;
        assert(success);
        success = _mixer->SetAnonymousMixabilityStatus(*participant,false) == 0;
        assert(success);
        success = _mixer->AnonymousMixabilityStatus(*participant,anonymouslyMixed) == 0;
        assert(success);
        assert(!anonymouslyMixed);
        success = _mixer->SetAnonymousMixabilityStatus(*participant,false) == -1;
        assert(success);
        success = _mixer->SetAnonymousMixabilityStatus(*participant,true) == 0;
        assert(success);
        success = _mixer->AnonymousMixabilityStatus(*participant,anonymouslyMixed) == 0;
        assert(success);
        assert(anonymouslyMixed);
        return success;
    }
    WebRtc_UWord32 previousAmountOfMixableParticipants = 0;
    bool success = _mixer->AmountOfMixables(previousAmountOfMixableParticipants) == 0;
    assert(success);

    success = _mixer->SetMixabilityStatus(*participant,true) == 0;
    assert(success);
    success = _mixer->SetMixabilityStatus(*participant,true) == -1;
    assert(success);
    success = _mixer->SetMixabilityStatus(*participant,false) == 0;
    assert(success);
    success = _mixer->SetMixabilityStatus(*participant,false) == -1;
    assert(success);
    success = _mixer->SetMixabilityStatus(*participant,true) == 0;
    assert(success);
    if(!success)
    {
        return false;
    }

    WebRtc_UWord32 currentAmountOfMixableParticipants = 0;
    success = _mixer->AmountOfMixables(currentAmountOfMixableParticipants) == 0;
    assert(currentAmountOfMixableParticipants == previousAmountOfMixableParticipants + 1);

    bool mixable = true;
    success = _mixer->MixabilityStatus(*participant,mixable) == 0;
    assert(success);
    assert(mixable);
    if(participantType == MixerParticipant::REGULAR)
    {
        return true;
    }
    bool IsVIP = false;
    success = _mixer->SetVIPStatus(*participant,true) == 0;
    assert(success);
    success = _mixer->VIPStatus(*participant,IsVIP) == 0;
    assert(success);
    assert(IsVIP);
    success = _mixer->SetVIPStatus(*participant,true) == -1;
    assert(success);
    success = _mixer->SetVIPStatus(*participant,false) == 0;
    assert(success);
    success = _mixer->VIPStatus(*participant,IsVIP) == 0;
    assert(success);
    assert(!IsVIP);
    success = _mixer->SetVIPStatus(*participant,false) == -1;
    assert(success);
    success = _mixer->SetVIPStatus(*participant,true) == 0;
    assert(success);
    success = _mixer->VIPStatus(*participant,IsVIP) == 0;
    assert(success);
    assert(IsVIP);
    assert(success);
    return success;
}

bool
MixerWrapper::StopMixingParticipant(
    const WebRtc_UWord32 id)
{
    MapItem* item = _mixerParticipants.Find(id);
    if(item == NULL)
    {
        return false;
    }
    MixerParticipant* participant = static_cast<MixerParticipant*>(item->GetItem());
    bool success = false;
    WebRtc_UWord32 previousAmountOfMixableParticipants = 0;
    success = _mixer->AmountOfMixables(previousAmountOfMixableParticipants) == 0;
    assert(success);
    success = _mixer->SetMixabilityStatus(*participant,false) == 0;
    assert(success);
    WebRtc_UWord32 currentAmountOfMixableParticipants = 0;
    success = _mixer->AmountOfMixables(currentAmountOfMixableParticipants) == 0;
    assert(success);
    assert(success ? currentAmountOfMixableParticipants == previousAmountOfMixableParticipants -1 :
                     currentAmountOfMixableParticipants == previousAmountOfMixableParticipants);
    return success;
}

bool
MixerWrapper::GetFreeItemIds(
    WebRtc_UWord32& itemId)
{
    if(!_freeItemIds.Empty())
    {
        ListItem* item = _freeItemIds.First();
        WebRtc_UWord32* id = static_cast<WebRtc_UWord32*>(item->GetItem());
        itemId = *id;
        delete id;
        return true;
    }
    if(_itemIdCounter == (WebRtc_UWord32) -1)
    {
        return false;
    }
    itemId = _itemIdCounter++;
    return true;
}

void
MixerWrapper::AddFreeItemIds(
    const WebRtc_UWord32 itemId)
{
    WebRtc_UWord32* id = new WebRtc_UWord32;
    *id = itemId;
    _freeItemIds.PushBack(static_cast<void*>(id));
}

void
MixerWrapper::ClearAllItemIds()
{
    ListItem* item = _freeItemIds.First();
    while(item != NULL)
    {
        WebRtc_UWord32* id = static_cast<WebRtc_UWord32*>(item->GetItem());
        delete id;
        _freeItemIds.Erase(item);
        item = _freeItemIds.First();
    }
}

bool
LoopedFileRead(
    WebRtc_Word16* buffer,
    WebRtc_UWord32 bufferSizeInSamples,
    WebRtc_UWord32 samplesToRead,
    FILE* file)
{
    if(bufferSizeInSamples < samplesToRead)
    {
        return false;
    }
    WebRtc_UWord32 gottenSamples = (WebRtc_UWord32)fread(buffer,sizeof(WebRtc_Word16),samplesToRead,file);
    if(gottenSamples != samplesToRead)
    {
        WebRtc_UWord32 missingSamples = samplesToRead - gottenSamples;
        fseek(file,0,0);
        gottenSamples += (WebRtc_UWord32)fread(&buffer[gottenSamples],sizeof(WebRtc_Word16),missingSamples,file);
    }
    if(gottenSamples != samplesToRead)
    {
        return false;
    }
    return true;
}

void
GenerateRandomPosition(
    WebRtc_Word32& startPosition)
{
    startPosition = (rand() % (60*16000/160)) * 160;
}
