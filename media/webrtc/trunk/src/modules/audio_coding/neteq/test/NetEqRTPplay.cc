/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

//TODO(hlundin): Reformat file to meet style guide.

#include <assert.h>
#include <stdio.h>

/* header includes */
#include "typedefs.h"
#include "webrtc_neteq.h"
#include "webrtc_neteq_internal.h"
#include "webrtc_neteq_help_macros.h"
#include "neteq_error_codes.h" // for the API test

#include "NETEQTEST_RTPpacket.h"
#include "NETEQTEST_DummyRTPpacket.h"
#include "NETEQTEST_NetEQClass.h"
#include "NETEQTEST_CodecClass.h"

#ifdef WEBRTC_ANDROID
#include <ctype.h> // isalpha
#endif
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <map>
#include <vector>

#ifdef WIN32
#include <cassert>
#include <windows.h>
#endif

#ifdef WEBRTC_LINUX
#include <netinet/in.h>
#include <libgen.h>
#include <cassert>
#endif

//#include "vld.h"

//#define NETEQ_DELAY_LOGGING
//#define DUMMY_SLAVE_CHANNEL

#ifdef NETEQ_DELAY_LOGGING
#include "delay_logging.h"
#define DUMMY_SLAVE_CHANNEL // do not use a slave channel, only generate zeros instead
#endif


/************************/
/* Define payload types */
/************************/

// Payload types are defined in the textfile ptypes.txt, and can be changed after compilation.



/*********************/
/* Misc. definitions */
/*********************/

#define TIME_STEP 1
#define FIRSTLINELEN 40
#define MAX_NETEQ_BUFFERSIZE    170000 //100000
#define CHECK_ZERO(a) {int errCode = a; char tempErrName[WEBRTC_NETEQ_MAX_ERROR_NAME]; if((errCode)!=0){errCode = WebRtcNetEQ_GetErrorCode(inst); WebRtcNetEQ_GetErrorName(errCode, tempErrName, WEBRTC_NETEQ_MAX_ERROR_NAME); printf("\n %s \n line: %d \n error at %s\n Error Code = %d\n",__FILE__,__LINE__,#a, errCode); exit(0);}}
#define CHECK_NOT_NULL(a) if((a)==NULL){printf("\n %s \n line: %d \nerror at %s\n",__FILE__,__LINE__,#a );return(-1);}
//#define PLAY_CLEAN // ignore arrival times and let the packets arrive according to RTP timestamps
#define HDR_SIZE 8 // rtpplay packet header size in bytes
//#define JUNK_DATA   // scramble the payloads to test error resilience
//#define ZERO_TS_START

#ifdef JUNK_DATA
    #define SEED_FILE "randseed.txt"
#endif

#ifdef WIN32
#define MY_MAX_DRIVE _MAX_DRIVE
#define MY_MAX_PATH _MAX_PATH
#define MY_MAX_FNAME _MAX_FNAME
#define MY_MAX_EXT _MAX_EXT

#elif defined(WEBRTC_LINUX)
#include <linux/limits.h>
#define MY_MAX_PATH PATH_MAX

#elif defined(WEBRTC_MAC)
#include <sys/syslimits.h>
#define MY_MAX_PATH PATH_MAX
#endif // WEBRTC_MAC

/************/
/* Typedefs */
/************/

typedef struct {
    enum WebRtcNetEQDecoder  codec;
    enum stereoModes    stereo;
    NETEQTEST_Decoder * decoder[2];
    int            fs;
} decoderStruct;


/*************************/
/* Function declarations */
/*************************/

void stereoInterleave(WebRtc_Word16 *data, WebRtc_Word16 totalLen);
int getNextRecoutTime(FILE *fp, WebRtc_UWord32 *nextTime);
void getNextExtraDelay(FILE *fp, WebRtc_UWord32 *t, int *d);
bool splitStereo(NETEQTEST_RTPpacket* rtp, NETEQTEST_RTPpacket* rtpSlave,
                 const WebRtc_Word16 *stereoPtype, const enum stereoModes *stereoMode, int noOfStereoCodecs,
                 const WebRtc_Word16 *cngPtype, int noOfCngCodecs,
                 bool *isStereo);
void parsePtypeFile(FILE *ptypeFile, std::map<WebRtc_UWord8, decoderStruct>* decoders);
int populateUsedCodec(std::map<WebRtc_UWord8, decoderStruct>* decoders, enum WebRtcNetEQDecoder *usedCodec);
void createAndInsertDecoders (NETEQTEST_NetEQClass *neteq, std::map<WebRtc_UWord8, decoderStruct>* decoders, int channelNumber);
void free_coders(std::map<WebRtc_UWord8, decoderStruct> & decoders);
int doAPItest();
bool changeStereoMode(NETEQTEST_RTPpacket & rtp, std::map<WebRtc_UWord8, decoderStruct> & decoders, enum stereoModes *stereoMode);



/********************/
/* Global variables */
/********************/

WebRtc_Word16 NetEqPacketBuffer[MAX_NETEQ_BUFFERSIZE>>1];
WebRtc_Word16 NetEqPacketBufferSlave[MAX_NETEQ_BUFFERSIZE>>1];

#ifdef NETEQ_DELAY_LOGGING
extern "C" {
    FILE *delay_fid2;   /* file pointer */
    WebRtc_UWord32 tot_received_packets=0;
}
#endif

#ifdef DEF_BUILD_DATE
extern char BUILD_DATE;
#endif

WebRtc_UWord32 writtenSamples = 0;
WebRtc_UWord32 simClock=0;

int main(int argc, char* argv[])
{
    std::vector<NETEQTEST_NetEQClass *> NetEQvector;
    char   version[20];

    enum WebRtcNetEQDecoder usedCodec[kDecoderReservedEnd-1];
    int noOfCodecs;
    int ok;
    WebRtc_Word16 out_data[640*2];
    WebRtc_Word16 outLen, writeLen;
    int fs = 8000;
    WebRtcNetEQ_RTCPStat RTCPstat;
#ifdef WIN32
    char outdrive[MY_MAX_DRIVE];
    char outpath[MY_MAX_PATH];
    char outfile[MY_MAX_FNAME];
    char outext[MY_MAX_EXT];
#endif
    char outfilename[MY_MAX_PATH];
#ifdef NETEQ_DELAY_LOGGING
    float clock_float;
    int temp_var;
#endif
#ifdef JUNK_DATA
    FILE *seedfile;
#endif
    FILE *recoutTimes = NULL;
    FILE *extraDelays = NULL;
    WebRtcNetEQPlayoutMode streamingMode = kPlayoutOn;
    bool preParseRTP = false;
    bool rtpOnly = false;
    int packetLen = 0;
    int packetCount = 0;
    std::map<WebRtc_UWord8, decoderStruct> decoders;
    bool dummyRtp = false;
    bool noDecode = false;

    /* get the version string */
    WebRtcNetEQ_GetVersion(version);
    printf("\n\nNetEq version: %s\n", version);
#ifdef DEF_BUILD_DATE
    printf("Build time: %s\n", __BUILD_DATE);
#endif

    /* check number of parameters */
    if ((argc < 3)
#ifdef WIN32 // implicit output file name possible for windows
        && (argc < 2)
#endif
        ) {
        /* print help text and exit */
        printf("Test program for NetEQ.\n");
        printf("The program reads an RTP stream from file and inserts it into NetEQ.\n");
        printf("The format of the RTP stream file should be the same as for rtpplay,\n");
        printf("and can be obtained e.g., from Ethereal by using\n");
        printf("Statistics -> RTP -> Show All Streams -> [select a stream] -> Save As\n\n");
        printf("Usage:\n\n");
#ifdef WIN32
        printf("%s RTPfile [outfile] [-options]\n", argv[0]);
#else
        printf("%s RTPfile outfile [-options]\n", argv[0]);
#endif
        printf("where:\n");

        printf("RTPfile      : RTP stream input file\n\n");

        printf("outfile      : PCM speech output file\n");
        printf("               Output file name is derived from RTP file name if omitted\n\n");

        printf("-options are optional switches:\n");
        printf("\t-recout datfile        : supply recout times\n");
        printf("\t-extradelay datfile    : supply extra delay settings and timing\n");
        printf("\t-streaming             : engage streaming mode\n");
        printf("\t-fax                   : engage fax mode\n");
        printf("\t-preparsertp           : use RecIn with pre-parsed RTP\n");
        printf("\t-rtponly packLenBytes  : input file consists of constant size RTP packets without RTPplay headers\n");
        printf("\t-dummyrtp              : input file contains only RTP headers\n");
        printf("\t-nodecode              : no decoding will be done\n");
        //printf("\t-switchms              : switch from mono to stereo (copy channel) after 10 seconds\n");
        //printf("\t-duplicate             : use two instances with identical input (2-channel mono)\n");

        return(0);
    }

    if (strcmp(argv[1], "-apitest")==0) {
        // do API test and then return
        ok=doAPItest();

        if (ok==0)
            printf("API test successful!\n");
        else
            printf("API test failed!\n");

        return(ok);
    }

    FILE* in_file=fopen(argv[1],"rb");
    CHECK_NOT_NULL(in_file);
    printf("Input file: %s\n",argv[1]);

    int argIx = 2; // index of next argument from command line

    if ( argc >= 3 && argv[2][0] != '-' ) { // output name given on command line
        strcpy(outfilename, argv[2]);
        argIx++;
    } else { // derive output name from input name
#ifdef WIN32
        _splitpath(argv[1],outdrive,outpath,outfile,outext);
        _makepath(outfilename,outdrive,outpath,outfile,"pcm");
#else
        fprintf(stderr,"Output file name must be specified.\n");
        return(-1);
#endif
    }
    FILE* out_file=fopen(outfilename,"wb");
    if (out_file==NULL) {
        fprintf(stderr,"Could not open file %s for writing\n", outfilename);
        return(-1);
    }
    printf("Output file: %s\n",outfilename);

    // Parse for more arguments, all beginning with '-'
    while( argIx < argc ) {
        if (argv[argIx][0] != '-') {
            fprintf(stderr,"Unknown input argument %s\n", argv[argIx]);
            return(-1);
        }

        if( strcmp(argv[argIx], "-recout") == 0 ) {
            argIx++;
            recoutTimes = fopen(argv[argIx], "rb");
            CHECK_NOT_NULL(recoutTimes);
            argIx++;
        }
        else if( strcmp(argv[argIx], "-extradelay") == 0 ) {
            argIx++;
            extraDelays = fopen(argv[argIx], "rb");
            CHECK_NOT_NULL(extraDelays);
            argIx++;
        }
        else if( strcmp(argv[argIx], "-streaming") == 0 ) {
            argIx++;
            streamingMode = kPlayoutStreaming;
        }
        else if( strcmp(argv[argIx], "-fax") == 0 ) {
            argIx++;
            streamingMode = kPlayoutFax;
        }
        else if( strcmp(argv[argIx], "-preparsertp") == 0 ) {
            argIx++;
            preParseRTP = true;
        }
        else if( strcmp(argv[argIx], "-rtponly") == 0 ) {
            argIx++;
            rtpOnly = true;
            packetLen = atoi(argv[argIx]);
            argIx++;
            if (packetLen <= 0)
            {
                printf("Wrong packet size used with argument -rtponly.\n");
                exit(1);
            }
        }
        else if (strcmp(argv[argIx], "-dummyrtp") == 0
            || strcmp(argv[argIx], "-dummy") == 0)
        {
            argIx++;
            dummyRtp = true;
            noDecode = true; // force noDecode since there are no payloads
        }
        else if (strcmp(argv[argIx], "-nodecode") == 0)
        {
            argIx++;
            noDecode = true;
        }
        //else if( strcmp(argv[argIx], "-switchms") == 0 ) {
        //    argIx++;
        //    switchMS = true;
        //}
        //else if( strcmp(argv[argIx], "-duplicate") == 0 ) {
        //    argIx++;
        //    duplicatePayload = true;
        //}
        else {
            fprintf(stderr,"Unknown input argument %s\n", argv[argIx]);
            return(-1);
        }
    }



#ifdef NETEQ_DELAY_LOGGING
    char delayfile[MY_MAX_PATH];
#ifdef WIN32
    _splitpath(outfilename,outdrive,outpath,outfile,outext);
    _makepath(delayfile,outdrive,outpath,outfile,"d");
#else
    sprintf(delayfile, "%s.d", outfilename);
#endif
    delay_fid2 = fopen(delayfile,"wb");
    fprintf(delay_fid2, "#!NetEQ_Delay_Logging%s\n", NETEQ_DELAY_LOGGING_VERSION_STRING);
#endif

    char ptypesfile[MY_MAX_PATH];
#ifdef WIN32
    _splitpath(argv[0],outdrive,outpath,outfile,outext);
    _makepath(ptypesfile,outdrive,outpath,"ptypes","txt");
#elif defined(WEBRTC_ANDROID)
  strcpy(ptypesfile, "/sdcard/ptypes.txt");
#else
    // TODO(hlundin): Include path to ptypes, as for WIN32 above.
  strcpy(ptypesfile, "ptypes.txt");
#endif
    FILE *ptypeFile = fopen(ptypesfile,"rt");
    if (!ptypeFile) {
        // Check if we can find the file at the usual place in the trunk.
        if (strstr(argv[0], "out/Debug/")) {
            int path_len = strstr(argv[0], "out/Debug/") - argv[0];
            strncpy(ptypesfile, argv[0], path_len);
            ptypesfile[path_len] = '\0';
            strcat(ptypesfile,
                   "src/modules/audio_coding/NetEQ/main/test/ptypes.txt");
            ptypeFile = fopen(ptypesfile,"rt");
        }
    }
    CHECK_NOT_NULL(ptypeFile);
    printf("Ptypes file: %s\n\n", ptypesfile);

    parsePtypeFile(ptypeFile, &decoders);
    fclose(ptypeFile);

    noOfCodecs = populateUsedCodec(&decoders, usedCodec);


    /* read RTP file header */
    if (!rtpOnly)
    {
        if (NETEQTEST_RTPpacket::skipFileHeader(in_file) != 0)
        {
            fprintf(stderr, "Wrong format in RTP file.\n");
            return -1;
        }
    }

    /* check payload type for first speech packet */
    long tempFilePos = ftell(in_file);
    enum stereoModes stereoMode = stereoModeMono;

    NETEQTEST_RTPpacket *rtp;
    NETEQTEST_RTPpacket *slaveRtp;
    if (!dummyRtp)
    {
        rtp = new NETEQTEST_RTPpacket();
        slaveRtp = new NETEQTEST_RTPpacket();
    }
    else
    {
        rtp = new NETEQTEST_DummyRTPpacket();
        slaveRtp = new NETEQTEST_DummyRTPpacket();
    }

    if (!rtpOnly)
    {
        while (rtp->readFromFile(in_file) >= 0)
        {
            if (decoders.count(rtp->payloadType()) > 0
                && decoders[rtp->payloadType()].codec != kDecoderRED
                && decoders[rtp->payloadType()].codec != kDecoderAVT
                && decoders[rtp->payloadType()].codec != kDecoderCNG )
            {
                stereoMode = decoders[rtp->payloadType()].stereo;
                fs = decoders[rtp->payloadType()].fs;
                break;
            }
        }
    }
    else
    {
        while (rtp->readFixedFromFile(in_file, packetLen) >= 0)
        {
            if (decoders.count(rtp->payloadType()) > 0
                && decoders[rtp->payloadType()].codec != kDecoderRED
                && decoders[rtp->payloadType()].codec != kDecoderAVT
                && decoders[rtp->payloadType()].codec != kDecoderCNG )
            {
                stereoMode = decoders[rtp->payloadType()].stereo;
                fs = decoders[rtp->payloadType()].fs;
                break;
            }
        }
    }

    fseek(in_file, tempFilePos, SEEK_SET /* from beginning */);


    /* block some payload types */
    //rtp->blockPT(72);
    //rtp->blockPT(23);

    /* read first packet */
    if (!rtpOnly)
    {
        rtp->readFromFile(in_file);
    }
    else
    {
        rtp->readFixedFromFile(in_file, packetLen);
        rtp->setTime((1000 * rtp->timeStamp()) / fs);
    }
    if (!rtp)
    {
        printf("\nWarning: RTP file is empty\n\n");
    }


    /* Initialize NetEQ instances */
    int numInst = 1;
    if (stereoMode > stereoModeMono)
    {
        numInst = 2;
    }

    for (int i = 0; i < numInst; i++)
    {
        // create memory, allocate, initialize, and allocate packet buffer memory
        NetEQvector.push_back (new NETEQTEST_NetEQClass(usedCodec, noOfCodecs, static_cast<WebRtc_UWord16>(fs), kTCPLargeJitter));

        createAndInsertDecoders (NetEQvector[i], &decoders, i /* channel */);

        WebRtcNetEQ_SetAVTPlayout(NetEQvector[i]->instance(),1); // enable DTMF playout

        WebRtcNetEQ_SetPlayoutMode(NetEQvector[i]->instance(), streamingMode);

        NetEQvector[i]->usePreparseRTP(preParseRTP);

        NetEQvector[i]->setNoDecode(noDecode);

        if (numInst > 1)
        {
            // we are using master/slave mode
            if (i == 0)
            {
                // first instance is master
                NetEQvector[i]->setMaster();
            }
            else
            {
                // all other are slaves
                NetEQvector[i]->setSlave();
            }
        }
    }


#ifdef ZERO_TS_START
    WebRtc_UWord32 firstTS = rtp->timeStamp();
    rtp->setTimeStamp(0);
#else
    WebRtc_UWord32 firstTS = 0;
#endif

    // check stereo mode
    if (stereoMode > stereoModeMono)
    {
        if(rtp->splitStereo(slaveRtp, stereoMode))
        {
            printf("Error in splitStereo\n");
        }
    }

#ifdef PLAY_CLEAN
    WebRtc_UWord32 prevTS = rtp->timeStamp();
    WebRtc_UWord32 currTS, prev_time;
#endif

#ifdef JUNK_DATA
    unsigned int random_seed = (unsigned int) /*1196764538; */time(NULL);
    srand(random_seed);

    if ( (seedfile = fopen(SEED_FILE, "a+t") ) == NULL ) {
        fprintf(stderr, "Error: Could not open file %s\n", SEED_FILE);
    }
    else {
        fprintf(seedfile, "%u\n", random_seed);
        fclose(seedfile);
    }
#endif

    WebRtc_UWord32 nextRecoutTime;
    int lastRecout = getNextRecoutTime(recoutTimes, &nextRecoutTime); // does nothing if recoutTimes == NULL

    if (recoutTimes)
        simClock = (rtp->time() < nextRecoutTime ? rtp->time(): nextRecoutTime);
    else
        simClock = rtp->time(); // start immediately with first packet

    WebRtc_UWord32 start_clock = simClock;

    WebRtc_UWord32 nextExtraDelayTime;
    int extraDelay = -1;
    getNextExtraDelay(extraDelays, &nextExtraDelayTime, &extraDelay);

    void *msInfo;
    msInfo = malloc(WebRtcNetEQ_GetMasterSlaveInfoSize());
    if(msInfo == NULL)
        return(-1);

    while(rtp->dataLen() >= 0 || (recoutTimes && !lastRecout)) {
//        printf("simClock = %Lu\n", simClock);

#ifdef NETEQ_DELAY_LOGGING
        temp_var = NETEQ_DELAY_LOGGING_SIGNAL_CLOCK;
        clock_float = (float) simClock;
        if (fwrite(&temp_var, sizeof(int), 1, delay_fid2) != 1) {
          return -1;
        }
        if (fwrite(&clock_float, sizeof(float), 1, delay_fid2) != 1) {
          return -1;
        }
#endif
        /* time to set extra delay */
        if (extraDelay > -1 && simClock >= nextExtraDelayTime) {
            // set extra delay for all instances
            for (int i = 0; i < numInst; i++)
            {
                WebRtcNetEQ_SetExtraDelay(NetEQvector[i]->instance(), extraDelay);
            }
            getNextExtraDelay(extraDelays, &nextExtraDelayTime, &extraDelay);
        }

        /* check if time to receive */
        while (simClock >= rtp->time() && rtp->dataLen() >= 0)
        {
            if (rtp->dataLen() > 0)
            {

                // insert main packet
                NetEQvector[0]->recIn(*rtp);

                if (stereoMode > stereoModeMono
                    && slaveRtp->dataLen() > 0)
                {
                    // insert slave packet
                    NetEQvector[1]->recIn(*slaveRtp);
                }

            }

            /* get next packet */
#ifdef PLAY_CLEAN
            prev_time = rtp->time();
#endif
            if (!rtpOnly)
            {
                rtp->readFromFile(in_file);
            }
            else
            {
                rtp->readFixedFromFile(in_file, packetLen);
                rtp->setTime((1000 * rtp->timeStamp()) / fs);
            }

            if (rtp->dataLen() >= 0)
            {
                rtp->setTimeStamp(rtp->timeStamp() - firstTS);
            }

            packetCount++;

            if (changeStereoMode(*rtp, decoders, &stereoMode))
            {
                printf("Warning: stereo mode changed\n");
            }

            if (stereoMode > stereoModeMono)
            {
                if(rtp->splitStereo(slaveRtp, stereoMode))
                {
                    printf("Error in splitStereo\n");
                }
            }

#ifdef PLAY_CLEAN
            currTS = rtp->timeStamp();
            rtp->setTime(prev_time + (currTS-prevTS)/(fs/1000));
            prevTS = currTS;
#endif
        }

        /* check if time to RecOut */
        if ( (!recoutTimes && (simClock%10)==0) // recout times not given from file
        || ( recoutTimes && (simClock >= nextRecoutTime) ) ) // recout times given from file
        {
            if (stereoMode > stereoModeMono)
            {
                // stereo
                WebRtc_Word16 tempLen;
                tempLen = NetEQvector[0]->recOut( out_data, msInfo ); // master
                outLen = NetEQvector[1]->recOut( &out_data[tempLen], msInfo ); // slave

                assert(tempLen == outLen);

                writeLen = outLen * 2;
                stereoInterleave(out_data, writeLen);
            }
            else
            {
                // mono
                outLen = NetEQvector[0]->recOut( out_data );
                writeLen = outLen;
            }

            // write to file
            if (fwrite(out_data, writeLen, 2, out_file) != 2) {
              return -1;
            }
            writtenSamples += writeLen;


            lastRecout = getNextRecoutTime(recoutTimes, &nextRecoutTime); // does nothing if recoutTimes == NULL

            /* ask for statistics */
            WebRtcNetEQ_NetworkStatistics inCallStats;
            WebRtcNetEQ_GetNetworkStatistics(NetEQvector[0]->instance(), &inCallStats);

        }

        /* increase time */
        simClock+=TIME_STEP;
    }

    fclose(in_file);
    fclose(out_file);

#ifdef NETEQ_DELAY_LOGGING
    temp_var = NETEQ_DELAY_LOGGING_SIGNAL_EOF;
    if (fwrite(&temp_var, sizeof(int), 1, delay_fid2) != 1) {
      return -1;
    }
    if (fwrite(&tot_received_packets, sizeof(WebRtc_UWord32),
               1, delay_fid2) != 1) {
      return -1;
    }
    fprintf(delay_fid2,"End of file\n");
    fclose(delay_fid2);
#endif

    WebRtcNetEQ_GetRTCPStats(NetEQvector[0]->instance(), &RTCPstat);
    printf("RTCP statistics:\n");
    printf("    cum_lost        : %d\n", (int) RTCPstat.cum_lost);
    printf("    ext_max         : %d\n", (int) RTCPstat.ext_max);
    printf("    fraction_lost   : %d (%f%%)\n", RTCPstat.fraction_lost, (float)(100.0*RTCPstat.fraction_lost/256.0));
    printf("    jitter          : %d\n", (int) RTCPstat.jitter);

    printf("\n    Call duration ms    : %u\n", simClock-start_clock);

    printf("\nComplexity estimates (including sub-components):\n");
    printf("    RecIn complexity    : %.2f MCPS\n", NetEQvector[0]->getRecInTime() / ((float) 1000*(simClock-start_clock)));
    printf("    RecOut complexity   : %.2f MCPS\n", NetEQvector[0]->getRecOutTime() / ((float) 1000*(simClock-start_clock)));

    free_coders(decoders);
    //free_coders(0 /* first channel */);
 //   if (stereoMode > stereoModeMono) {
 //       free_coders(1 /* second channel */);
 //   }
    free(msInfo);

    for (std::vector<NETEQTEST_NetEQClass *>::iterator it = NetEQvector.begin();
        it < NetEQvector.end(); delete *it++) {
    }

    printf("\nSimulation done!\n");

#ifdef JUNK_DATA
    if ( (seedfile = fopen(SEED_FILE, "a+t") ) == NULL ) {
        fprintf(stderr, "Error: Could not open file %s\n", SEED_FILE);
    }
    else {
        fprintf(seedfile, "ok\n\n");
        fclose(seedfile);
    }
#endif


    // Log complexity to file
/*    FILE *statfile;
    statfile = fopen("complexity.txt","at");
    fprintf(statfile,"%.4f, %.4f\n", (float) totTime_RecIn.QuadPart / ((float) 1000*(simClock-start_clock)), (float) totTime_RecOut.QuadPart / ((float) 1000*(simClock-start_clock)));
    fclose(statfile);*/

    return(0);

}





/****************/
/* Subfunctions */
/****************/

bool splitStereo(NETEQTEST_RTPpacket* rtp, NETEQTEST_RTPpacket* rtpSlave,
                 const WebRtc_Word16 *stereoPtype, const enum stereoModes *stereoMode, int noOfStereoCodecs,
                 const WebRtc_Word16 *cngPtype, int noOfCngCodecs,
                 bool *isStereo)
{

    // init
    //bool isStereo = false;
    enum stereoModes tempStereoMode = stereoModeMono;
    bool isCng = false;

    // check payload length
    if (rtp->dataLen() <= 0) {
        //*isStereo = false; // don't change
        return(*isStereo);
    }

    // check payload type
    WebRtc_Word16 ptype = rtp->payloadType();

    // is this a cng payload?
    for (int k = 0; k < noOfCngCodecs; k++) {
        if (ptype == cngPtype[k]) {
            // do not change stereo state
            isCng = true;
            tempStereoMode = stereoModeFrame;
        }
    }

    if (!isCng)
    {
        *isStereo = false;

        // is this payload type a stereo codec? which type?
        for (int k = 0; k < noOfStereoCodecs; k++) {
            if (ptype == stereoPtype[k]) {
                tempStereoMode = stereoMode[k];
                *isStereo = true;
                break; // exit for loop
            }
        }
    }

    if (*isStereo)
    {
        // split the payload if stereo

        if(rtp->splitStereo(rtpSlave, tempStereoMode))
        {
            printf("Error in splitStereo\n");
        }

    }

    return(*isStereo);

}

void stereoInterleave(WebRtc_Word16 *data, WebRtc_Word16 totalLen)
{
    int k;

    for(k = totalLen/2; k < totalLen; k++) {
        WebRtc_Word16 temp = data[k];
        memmove(&data[2*k - totalLen + 2], &data[2*k - totalLen + 1], (totalLen - k -1) *  sizeof(WebRtc_Word16));
        data[2*k - totalLen + 1] = temp;
    }
}


int getNextRecoutTime(FILE *fp, WebRtc_UWord32 *nextTime) {

    float tempTime;

    if (!fp) {
        return -1;
    }

    if (fread(&tempTime, sizeof(float), 1, fp) != 0) {
        // not end of file
        *nextTime = (WebRtc_UWord32) tempTime;
        return 0;
    }

    *nextTime = 0;
    fclose(fp);

    return 1;
}

void getNextExtraDelay(FILE *fp, WebRtc_UWord32 *t, int *d) {

    float temp[2];

    if(!fp) {
        *d = -1;
        return;
    }

    if (fread(&temp, sizeof(float), 2, fp) != 0) {
        // not end of file
        *t = (WebRtc_UWord32) temp[0];
        *d = (int) temp[1];
        return;
    }

    *d = -1;
    fclose(fp);

    return;
}


void parsePtypeFile(FILE *ptypeFile, std::map<WebRtc_UWord8, decoderStruct>* decoders)
{
    int n, pt;
    char codec[100];
    decoderStruct tempDecoder;

    // read first line
    n = fscanf(ptypeFile, "%s %i\n", codec, &pt);

    while (n==2)
    {
        memset(&tempDecoder, 0, sizeof(decoderStruct));
        tempDecoder.stereo = stereoModeMono;

        if( pt >= 0  // < 0 disables this codec
            && isalpha(codec[0]) ) // and is a letter
        {

            /* check for stereo */
            int L = strlen(codec);
            bool isStereo = false;

            if (codec[L-1] == '*') {
                // stereo codec
                isStereo = true;

                // remove '*'
                codec[L-1] = '\0';
            }

#ifdef CODEC_G711
            if(strcmp(codec, "pcmu") == 0) {
                tempDecoder.codec = kDecoderPCMu;
                tempDecoder.fs = 8000;
            }
            else if(strcmp(codec, "pcma") == 0) {
                tempDecoder.codec = kDecoderPCMa;
                tempDecoder.fs = 8000;
            }
#endif
#ifdef CODEC_IPCMU
            else if(strcmp(codec, "eg711u") == 0) {
                tempDecoder.codec = kDecoderEG711u;
                tempDecoder.fs = 8000;
            }
#endif
#ifdef CODEC_IPCMA
            else if(strcmp(codec, "eg711a") == 0) {
                tempDecoder.codec = kDecoderEG711a;
                tempDecoder.fs = 8000;
            }
#endif
#ifdef CODEC_ILBC
            else if(strcmp(codec, "ilbc") == 0) {
                tempDecoder.codec = kDecoderILBC;
                tempDecoder.fs = 8000;
            }
#endif
#ifdef CODEC_ISAC
            else if(strcmp(codec, "isac") == 0) {
                tempDecoder.codec = kDecoderISAC;
                tempDecoder.fs = 16000;
            }
#endif
#ifdef CODEC_ISACLC
            else if(strcmp(codec, "isaclc") == 0) {
                tempDecoder.codec = NETEQ_CODEC_ISACLC;
                tempDecoder.fs = 16000;
            }
#endif
#ifdef CODEC_ISAC_SWB
            else if(strcmp(codec, "isacswb") == 0) {
                tempDecoder.codec = kDecoderISACswb;
                tempDecoder.fs = 32000;
            }
#endif
#ifdef CODEC_IPCMWB
            else if(strcmp(codec, "ipcmwb") == 0) {
                tempDecoder.codec = kDecoderIPCMwb;
                tempDecoder.fs = 16000;
            }
#endif
#ifdef CODEC_G722
            else if(strcmp(codec, "g722") == 0) {
                tempDecoder.codec = kDecoderG722;
                tempDecoder.fs = 16000;
            }
#endif
#ifdef CODEC_G722_1_16
            else if(strcmp(codec, "g722_1_16") == 0) {
                tempDecoder.codec = kDecoderG722_1_16;
                tempDecoder.fs = 16000;
            }
#endif
#ifdef CODEC_G722_1_24
            else if(strcmp(codec, "g722_1_24") == 0) {
                tempDecoder.codec = kDecoderG722_1_24;
                tempDecoder.fs = 16000;
            }
#endif
#ifdef CODEC_G722_1_32
            else if(strcmp(codec, "g722_1_32") == 0) {
                tempDecoder.codec = kDecoderG722_1_32;
                tempDecoder.fs = 16000;
            }
#endif
#ifdef CODEC_G722_1C_24
            else if(strcmp(codec, "g722_1c_24") == 0) {
                tempDecoder.codec = kDecoderG722_1C_24;
                tempDecoder.fs = 32000;
            }
#endif
#ifdef CODEC_G722_1C_32
            else if(strcmp(codec, "g722_1c_32") == 0) {
                tempDecoder.codec = kDecoderG722_1C_32;
                tempDecoder.fs = 32000;
            }
#endif
#ifdef CODEC_G722_1C_48
            else if(strcmp(codec, "g722_1c_48") == 0) {
                tempDecoder.codec = kDecoderG722_1C_48;
                tempDecoder.fs = 32000;
            }
#endif
#ifdef CODEC_G723
            else if(strcmp(codec, "g723") == 0) {
                tempDecoder.codec = NETEQ_CODEC_G723;
                tempDecoder.fs = 8000;
            }
#endif
#ifdef CODEC_G726
            else if(strcmp(codec, "g726_16") == 0) {
                tempDecoder.codec = kDecoderG726_16;
                tempDecoder.fs = 8000;
            }
            else if(strcmp(codec, "g726_24") == 0) {
                tempDecoder.codec = kDecoderG726_24;
                tempDecoder.fs = 8000;
            }
            else if(strcmp(codec, "g726_32") == 0) {
                tempDecoder.codec = kDecoderG726_32;
                tempDecoder.fs = 8000;
            }
            else if(strcmp(codec, "g726_40") == 0) {
                tempDecoder.codec = kDecoderG726_40;
                tempDecoder.fs = 8000;
            }
#endif
#ifdef CODEC_G729
            else if(strcmp(codec, "g729") == 0) {
                tempDecoder.codec = kDecoderG729;
                tempDecoder.fs = 8000;
            }
#endif
#ifdef CODEC_G729D
            else if(strcmp(codec, "g729d") == 0) {
                tempDecoder.codec = NETEQ_CODEC_G729D;
                tempDecoder.fs = 8000;
            }
#endif
#ifdef CODEC_G729_1
            else if(strcmp(codec, "g729_1") == 0) {
                tempDecoder.codec = kDecoderG729_1;
                tempDecoder.fs = 16000;
            }
#endif
#ifdef CODEC_GSMFR
            else if(strcmp(codec, "gsmfr") == 0) {
                tempDecoder.codec = kDecoderGSMFR;
                tempDecoder.fs = 8000;
            }
#endif
#ifdef CODEC_GSMEFR
            else if(strcmp(codec, "gsmefr") == 0) {
                tempDecoder.codec = NETEQ_CODEC_GSMEFR;
                tempDecoder.fs = 8000;
            }
#endif
#ifdef CODEC_AMR
            else if(strcmp(codec, "amr") == 0) {
                tempDecoder.codec = kDecoderAMR;
                tempDecoder.fs = 8000;
            }
#endif
#ifdef CODEC_AMRWB
            else if(strcmp(codec, "amrwb") == 0) {
                tempDecoder.codec = kDecoderAMRWB;
                tempDecoder.fs = 16000;
            }
#endif
#ifdef CODEC_DVI4
            else if(strcmp(codec, "dvi4") == 0) {
                tempDecoder.codec = NETEQ_CODEC_DVI4;
                tempDecoder.fs = 8000;
            }
#endif
#ifdef CODEC_SPEEX_8
            else if(strcmp(codec, "speex8") == 0) {
                tempDecoder.codec = kDecoderSPEEX_8;
                tempDecoder.fs = 8000;
            }
#endif
#ifdef CODEC_SPEEX_16
            else if(strcmp(codec, "speex16") == 0) {
                tempDecoder.codec = kDecoderSPEEX_16;
                tempDecoder.fs = 16000;
            }
#endif
#ifdef CODEC_CELT_32
            else if(strcmp(codec, "celt32") == 0) {
                tempDecoder.codec = kDecoderCELT_32;
                tempDecoder.fs = 32000;
            }
#endif
#ifdef CODEC_SILK_NB
            else if(strcmp(codec, "silk8") == 0) {
                tempDecoder.codec = NETEQ_CODEC_SILK_8;
                tempDecoder.fs = 8000;
            }
#endif
#ifdef CODEC_SILK_WB
            else if(strcmp(codec, "silk12") == 0) {
                tempDecoder.codec = NETEQ_CODEC_SILK_12;
                tempDecoder.fs = 16000;
            }
            else if(strcmp(codec, "silk16") == 0) {
                tempDecoder.codec = NETEQ_CODEC_SILK_16;
                tempDecoder.fs = 16000;
            }
#endif
#ifdef CODEC_SILK_SWB
            else if(strcmp(codec, "silk24") == 0) {
                tempDecoder.codec = NETEQ_CODEC_SILK_24;
                tempDecoder.fs = 32000;
            }
#endif
#ifdef CODEC_MELPE
            else if(strcmp(codec, "melpe") == 0) {
                tempDecoder.codec = NETEQ_CODEC_MELPE;
                tempDecoder.fs = 8000;
            }
#endif
#ifdef CODEC_PCM16B
            else if(strcmp(codec, "pcm16b") == 0) {
                tempDecoder.codec = kDecoderPCM16B;
                tempDecoder.fs = 8000;
            }
#endif
#ifdef CODEC_PCM16B_WB
            else if(strcmp(codec, "pcm16b_wb") == 0) {
                tempDecoder.codec = kDecoderPCM16Bwb;
                tempDecoder.fs = 16000;
            }
#endif
#ifdef CODEC_PCM16B_32KHZ
            else if(strcmp(codec, "pcm16b_swb32khz") == 0) {
                tempDecoder.codec = kDecoderPCM16Bswb32kHz;
                tempDecoder.fs = 32000;
            }
#endif
#ifdef CODEC_PCM16B_48KHZ
            else if(strcmp(codec, "pcm16b_swb48khz") == 0) {
                tempDecoder.codec = kDecoderPCM16Bswb48kHz;
                tempDecoder.fs = 48000;
            }
#endif
#ifdef CODEC_CNGCODEC8
            else if(strcmp(codec, "cn") == 0) {
                tempDecoder.codec = kDecoderCNG;
                tempDecoder.fs = 8000;
            }
#endif
#ifdef CODEC_CNGCODEC16
            else if(strcmp(codec, "cn_wb") == 0) {
                tempDecoder.codec = kDecoderCNG;
                tempDecoder.fs = 16000;
            }
#endif
#ifdef CODEC_CNGCODEC32
            else if(strcmp(codec, "cn_swb32") == 0) {
                tempDecoder.codec = kDecoderCNG;
                tempDecoder.fs = 32000;
            }
#endif
#ifdef CODEC_CNGCODEC48
            else if(strcmp(codec, "cn_swb48") == 0) {
                tempDecoder.codec = kDecoderCNG;
                tempDecoder.fs = 48000;
            }
#endif
#ifdef CODEC_ATEVENT_DECODE
            else if(strcmp(codec, "avt") == 0) {
                tempDecoder.codec = kDecoderAVT;
                tempDecoder.fs = 8000;
            }
#endif
#ifdef CODEC_RED
            else if(strcmp(codec, "red") == 0) {
                tempDecoder.codec = kDecoderRED;
                tempDecoder.fs = 8000;
            }
#endif
            else if(isalpha(codec[0])) {
                printf("Unsupported codec %s\n", codec);
                // read next line and continue while loop
                n = fscanf(ptypeFile, "%s %i\n", codec, &pt);
                continue;
            }
            else {
                // name is not recognized, and does not start with a letter
                // hence, it is commented out
                // read next line and continue while loop
                n = fscanf(ptypeFile, "%s %i\n", codec, &pt);
                continue;
            }

            // handle stereo
            if (tempDecoder.codec == kDecoderCNG)
            {
                // always set stereo mode for CNG, even if it is not marked at stereo
                tempDecoder.stereo = stereoModeFrame;
            }
            else if(isStereo)
            {
                switch(tempDecoder.codec) {
                    // sample based codecs
                    case kDecoderPCMu:
                    case kDecoderPCMa:
                    case kDecoderG722:
                        {
                            // 1 octet per sample
                            tempDecoder.stereo = stereoModeSample1;
                            break;
                        }
                    case kDecoderPCM16B:
                    case kDecoderPCM16Bwb:
                    case kDecoderPCM16Bswb32kHz:
                    case kDecoderPCM16Bswb48kHz:
                        {
                            // 2 octets per sample
                            tempDecoder.stereo = stereoModeSample2;
                            break;
                        }

                    case kDecoderCELT_32:
                    {
                      tempDecoder.stereo = stereoModeDuplicate;
                      break;
                    }
                        // fixed-rate frame codecs
//                    case kDecoderG729:
//                    case NETEQ_CODEC_G729D:
//                    case NETEQ_CODEC_G729E:
//                    case kDecoderG722_1_16:
//                    case kDecoderG722_1_24:
//                    case kDecoderG722_1_32:
//                    case kDecoderG722_1C_24:
//                    case kDecoderG722_1C_32:
//                    case kDecoderG722_1C_48:
//                    case NETEQ_CODEC_MELPE:
//                        {
//                            tempDecoder.stereo = stereoModeFrame;
//                            break;
//                        }
                    default:
                        {
                            printf("Cannot use codec %s as stereo codec\n", codec);
                            exit(0);
                        }
                }
            }

            if (pt > 127)
            {
                printf("Payload type must be less than 128\n");
                exit(0);
            }

            // insert into codecs map
            (*decoders)[static_cast<WebRtc_UWord8>(pt)] = tempDecoder;

        }

        n = fscanf(ptypeFile, "%s %i\n", codec, &pt);
    } // end while

}


bool changeStereoMode(NETEQTEST_RTPpacket & rtp, std::map<WebRtc_UWord8, decoderStruct> & decoders, enum stereoModes *stereoMode)
{
        if (decoders.count(rtp.payloadType()) > 0
            && decoders[rtp.payloadType()].codec != kDecoderRED
            && decoders[rtp.payloadType()].codec != kDecoderAVT
            && decoders[rtp.payloadType()].codec != kDecoderCNG )
        {
            if (decoders[rtp.payloadType()].stereo != *stereoMode)
            {
                *stereoMode = decoders[rtp.payloadType()].stereo;
                return true; // stereo mode did change
            }
        }

        return false; // stereo mode did not change
}


int populateUsedCodec(std::map<WebRtc_UWord8, decoderStruct>* decoders, enum WebRtcNetEQDecoder *usedCodec)
{
    int numCodecs = 0;

    std::map<WebRtc_UWord8, decoderStruct>::iterator it;

    it = decoders->begin();

    for (int i = 0; i < static_cast<int>(decoders->size()); i++, it++)
    {
        usedCodec[numCodecs] = (*it).second.codec;
        numCodecs++;
    }

    return numCodecs;
}


void createAndInsertDecoders (NETEQTEST_NetEQClass *neteq, std::map<WebRtc_UWord8, decoderStruct>* decoders, int channelNumber)
{
    std::map<WebRtc_UWord8, decoderStruct>::iterator it;

    for (it = decoders->begin(); it != decoders->end();  it++)
    {
        if (channelNumber == 0 ||
            ((*it).second.stereo > stereoModeMono ))
        {
            // create decoder instance
            WebRtc_UWord8 pt = static_cast<WebRtc_UWord8>( (*it).first );
            NETEQTEST_Decoder **dec = &((*it).second.decoder[channelNumber]);
            enum WebRtcNetEQDecoder type = (*it).second.codec;

            switch (type)
            {
#ifdef CODEC_G711
            case kDecoderPCMu:
                *dec = new decoder_PCMU( pt );
                break;
            case kDecoderPCMa:
                *dec = new decoder_PCMA( pt );
                break;
#endif
#ifdef CODEC_IPCMU
            case kDecoderEG711u:
                *dec = new decoder_IPCMU( pt );
                break;
#endif
#ifdef CODEC_IPCMA
            case kDecoderEG711a:
                *dec = new decoder_IPCMA( pt );
                break;
#endif
#ifdef CODEC_IPCMWB
            case kDecoderIPCMwb:
                *dec = new decoder_IPCMWB( pt );
                break;
#endif
#ifdef CODEC_ILBC
            case kDecoderILBC:
                *dec = new decoder_ILBC( pt );
                break;
#endif
#ifdef CODEC_ISAC
            case kDecoderISAC:
                *dec = new decoder_iSAC( pt );
                break;
#endif
#ifdef CODEC_ISAC_SWB
            case kDecoderISACswb:
                *dec = new decoder_iSACSWB( pt );
                break;
#endif
#ifdef CODEC_G729
            case kDecoderG729:
                *dec = new decoder_G729( pt );
                break;
            case NETEQ_CODEC_G729D:
                printf("Error: G729D not supported\n");
                break;
#endif
#ifdef CODEC_G729E
            case NETEQ_CODEC_G729E:
                *dec = new decoder_G729E( pt );
                break;
#endif
#ifdef CODEC_G729_1
            case kDecoderG729_1:
                *dec = new decoder_G729_1( pt );
                break;
#endif
#ifdef CODEC_G723
            case NETEQ_CODEC_G723:
                *dec = new decoder_G723( pt );
                break;
#endif
#ifdef CODEC_PCM16B
            case kDecoderPCM16B:
                *dec = new decoder_PCM16B_NB( pt );
                break;
#endif
#ifdef CODEC_PCM16B_WB
            case kDecoderPCM16Bwb:
                *dec = new decoder_PCM16B_WB( pt );
                break;
#endif
#ifdef CODEC_PCM16B_32KHZ
            case kDecoderPCM16Bswb32kHz:
                *dec = new decoder_PCM16B_SWB32( pt );
                break;
#endif
#ifdef CODEC_PCM16B_48KHZ
            case kDecoderPCM16Bswb48kHz:
                *dec = new decoder_PCM16B_SWB48( pt );
                break;
#endif
#ifdef CODEC_DVI4
            case NETEQ_CODEC_DVI4:
                *dec = new decoder_DVI4( pt );
                break;
#endif
#ifdef CODEC_G722
            case kDecoderG722:
                *dec = new decoder_G722( pt );
                break;
#endif
#ifdef CODEC_G722_1_16
            case kDecoderG722_1_16:
                *dec = new decoder_G722_1_16( pt );
                break;
#endif
#ifdef CODEC_G722_1_24
            case kDecoderG722_1_24:
                *dec = new decoder_G722_1_24( pt );
                break;
#endif
#ifdef CODEC_G722_1_32
            case kDecoderG722_1_32:
                *dec = new decoder_G722_1_32( pt );
                break;
#endif
#ifdef CODEC_G722_1C_24
            case kDecoderG722_1C_24:
                *dec = new decoder_G722_1C_24( pt );
                break;
#endif
#ifdef CODEC_G722_1C_32
            case kDecoderG722_1C_32:
                *dec = new decoder_G722_1C_32( pt );
                break;
#endif
#ifdef CODEC_G722_1C_48
            case kDecoderG722_1C_48:
                *dec = new decoder_G722_1C_48( pt );
                break;
#endif
#ifdef CODEC_AMR
            case kDecoderAMR:
                *dec = new decoder_AMR( pt );
                break;
#endif
#ifdef CODEC_AMRWB
            case kDecoderAMRWB:
                *dec = new decoder_AMRWB( pt );
                break;
#endif
#ifdef CODEC_GSMFR
            case kDecoderGSMFR:
                *dec = new decoder_GSMFR( pt );
                break;
#endif
#ifdef CODEC_GSMEFR
            case NETEQ_CODEC_GSMEFR:
                *dec = new decoder_GSMEFR( pt );
                break;
#endif
#ifdef CODEC_G726
            case kDecoderG726_16:
                *dec = new decoder_G726_16( pt );
                break;
            case kDecoderG726_24:
                *dec = new decoder_G726_24( pt );
                break;
            case kDecoderG726_32:
                *dec = new decoder_G726_32( pt );
                break;
            case kDecoderG726_40:
                *dec = new decoder_G726_40( pt );
                break;
#endif
#ifdef CODEC_MELPE
            case NETEQ_CODEC_MELPE:
#if (_MSC_VER >= 1400) && !defined(_WIN64) // only for Visual 2005 or later, and not for x64
                *dec = new decoder_MELPE( pt );
#endif
                break;
#endif
#ifdef CODEC_SPEEX_8
            case kDecoderSPEEX_8:
                *dec = new decoder_SPEEX( pt, 8000 );
                break;
#endif
#ifdef CODEC_SPEEX_16
            case kDecoderSPEEX_16:
                *dec = new decoder_SPEEX( pt, 16000 );
                break;
#endif
#ifdef CODEC_CELT_32
            case kDecoderCELT_32:
              if (channelNumber == 0)
                *dec = new decoder_CELT( pt, 32000 );
              else
                *dec = new decoder_CELTslave( pt, 32000 );
                break;
#endif
#ifdef CODEC_RED
            case kDecoderRED:
                *dec = new decoder_RED( pt );
                break;
#endif
#ifdef CODEC_ATEVENT_DECODE
            case kDecoderAVT:
                *dec = new decoder_AVT( pt );
                break;
#endif
#if (defined(CODEC_CNGCODEC8) || defined(CODEC_CNGCODEC16) || \
    defined(CODEC_CNGCODEC32) || defined(CODEC_CNGCODEC48))
            case kDecoderCNG:
                *dec = new decoder_CNG( pt, static_cast<WebRtc_UWord16>((*it).second.fs) );
                break;
#endif
#ifdef CODEC_ISACLC
            case NETEQ_CODEC_ISACLC:
                *dec = new decoder_iSACLC( pt );
                break;
#endif
#ifdef CODEC_SILK_NB
            case NETEQ_CODEC_SILK_8:
#if (_MSC_VER >= 1400) && !defined(_WIN64) // only for Visual 2005 or later, and not for x64
                *dec = new decoder_SILK8( pt );
#endif
                break;
#endif
#ifdef CODEC_SILK_WB
            case NETEQ_CODEC_SILK_12:
#if (_MSC_VER >= 1400) && !defined(_WIN64) // only for Visual 2005 or later, and not for x64
                *dec = new decoder_SILK12( pt );
#endif
                break;
#endif
#ifdef CODEC_SILK_WB
            case NETEQ_CODEC_SILK_16:
#if (_MSC_VER >= 1400) && !defined(_WIN64) // only for Visual 2005 or later, and not for x64
                *dec = new decoder_SILK16( pt );
#endif
                break;
#endif
#ifdef CODEC_SILK_SWB
            case NETEQ_CODEC_SILK_24:
#if (_MSC_VER >= 1400) && !defined(_WIN64) // only for Visual 2005 or later, and not for x64
                *dec = new decoder_SILK24( pt );
#endif
                break;
#endif

            default:
                printf("Unknown codec type encountered in createAndInsertDecoders\n");
                exit(0);
            }

            // insert into codec DB
            if (*dec)
            {
                (*dec)->loadToNetEQ(*neteq);
            }
        }
    }

}


void free_coders(std::map<WebRtc_UWord8, decoderStruct> & decoders)
{
    std::map<WebRtc_UWord8, decoderStruct>::iterator it;

    for (it = decoders.begin(); it != decoders.end();  it++)
    {
        if ((*it).second.decoder[0])
        {
            delete (*it).second.decoder[0];
        }

        if ((*it).second.decoder[1])
        {
            delete (*it).second.decoder[1];
        }
    }
}



#include "pcm16b.h"
#include "g711_interface.h"
#include "isac.h"

int doAPItest() {

    char   version[20];
    void *inst;
    enum WebRtcNetEQDecoder usedCodec;
    int NetEqBufferMaxPackets, BufferSizeInBytes;
    WebRtcNetEQ_CodecDef codecInst;
    WebRtcNetEQ_RTCPStat RTCPstat;
    WebRtc_UWord32 timestamp;
    int memorySize;
    int ok;

    printf("API-test:\n");

    /* get the version string */
    WebRtcNetEQ_GetVersion(version);
    printf("NetEq version: %s\n\n", version);

    /* test that API functions return -1 if instance is NULL */
#define CHECK_MINUS_ONE(x) {int errCode = x; if((errCode)!=-1){printf("\n API test failed at line %d: %s. Function did not return -1 as expected\n",__LINE__,#x); return(-1);}}
//#define RESET_ERROR(x) ((MainInst_t*) x)->ErrorCode = 0;
    inst = NULL;

    CHECK_MINUS_ONE(WebRtcNetEQ_GetErrorCode(inst))
    CHECK_MINUS_ONE(WebRtcNetEQ_Assign(&inst, NULL))
//  printf("WARNING: Test of WebRtcNetEQ_Assign() is disabled due to a bug.\n");
    usedCodec=kDecoderPCMu;
    CHECK_MINUS_ONE(WebRtcNetEQ_GetRecommendedBufferSize(inst, &usedCodec, 1, kTCPLargeJitter,  &NetEqBufferMaxPackets, &BufferSizeInBytes))
    CHECK_MINUS_ONE(WebRtcNetEQ_AssignBuffer(inst, NetEqBufferMaxPackets, NetEqPacketBuffer, BufferSizeInBytes))

    CHECK_MINUS_ONE(WebRtcNetEQ_Init(inst, 8000))
    CHECK_MINUS_ONE(WebRtcNetEQ_SetAVTPlayout(inst, 0))
    CHECK_MINUS_ONE(WebRtcNetEQ_SetExtraDelay(inst, 17))
    CHECK_MINUS_ONE(WebRtcNetEQ_SetPlayoutMode(inst, kPlayoutOn))

    CHECK_MINUS_ONE(WebRtcNetEQ_CodecDbReset(inst))
    CHECK_MINUS_ONE(WebRtcNetEQ_CodecDbAdd(inst, &codecInst))
    CHECK_MINUS_ONE(WebRtcNetEQ_CodecDbRemove(inst, usedCodec))
    WebRtc_Word16 temp1, temp2;
    CHECK_MINUS_ONE(WebRtcNetEQ_CodecDbGetSizeInfo(inst, &temp1, &temp2))
    CHECK_MINUS_ONE(WebRtcNetEQ_CodecDbGetCodecInfo(inst, 0, &usedCodec))

    CHECK_MINUS_ONE(WebRtcNetEQ_RecIn(inst, &temp1, 17, 4711))
    CHECK_MINUS_ONE(WebRtcNetEQ_RecOut(inst, &temp1, &temp2))
    CHECK_MINUS_ONE(WebRtcNetEQ_GetRTCPStats(inst, &RTCPstat)); // error here!!!
    CHECK_MINUS_ONE(WebRtcNetEQ_GetSpeechTimeStamp(inst, &timestamp))
    WebRtcNetEQOutputType temptype;
    CHECK_MINUS_ONE(WebRtcNetEQ_GetSpeechOutputType(inst, &temptype))

    WebRtc_UWord8 tempFlags;
    WebRtc_UWord16 utemp1, utemp2;
    CHECK_MINUS_ONE(WebRtcNetEQ_VQmonRecOutStatistics(inst, &utemp1, &utemp2, &tempFlags))
    CHECK_MINUS_ONE(WebRtcNetEQ_VQmonGetRxStatistics(inst, &utemp1, &utemp2))

    WebRtcNetEQ_AssignSize(&memorySize);
    CHECK_ZERO(WebRtcNetEQ_Assign(&inst, malloc(memorySize)))

    /* init with wrong sample frequency */
    CHECK_MINUS_ONE(WebRtcNetEQ_Init(inst, 17))

    /* init with correct fs */
    CHECK_ZERO(WebRtcNetEQ_Init(inst, 8000))

    /* GetRecommendedBufferSize with wrong codec */
    usedCodec=kDecoderReservedStart;
    ok = WebRtcNetEQ_GetRecommendedBufferSize(inst, &usedCodec, 1, kTCPLargeJitter , &NetEqBufferMaxPackets, &BufferSizeInBytes);
    if((ok!=-1) || ((ok==-1)&&(WebRtcNetEQ_GetErrorCode(inst)!=-CODEC_DB_UNKNOWN_CODEC))){
        printf("WebRtcNetEQ_GetRecommendedBufferSize() did not return proper error code for wrong codec.\n");
        printf("return value = %d; error code = %d\n", ok, WebRtcNetEQ_GetErrorCode(inst));
    }
    //RESET_ERROR(inst)

    /* GetRecommendedBufferSize with wrong network type */
    usedCodec = kDecoderPCMu;
    ok=WebRtcNetEQ_GetRecommendedBufferSize(inst, &usedCodec, 1, (enum WebRtcNetEQNetworkType) 4711 , &NetEqBufferMaxPackets, &BufferSizeInBytes);
    if ((ok!=-1) || ((ok==-1)&&(WebRtcNetEQ_GetErrorCode(inst)!=-FAULTY_NETWORK_TYPE))) {
        printf("WebRtcNetEQ_GetRecommendedBufferSize() did not return proper error code for wrong network type.\n");
        printf("return value = %d; error code = %d\n", ok, WebRtcNetEQ_GetErrorCode(inst));
        //RESET_ERROR(inst)
    }
    CHECK_ZERO(WebRtcNetEQ_GetRecommendedBufferSize(inst, &usedCodec, 1, kTCPLargeJitter , &NetEqBufferMaxPackets, &BufferSizeInBytes))

    /* try to do RecIn before assigning the packet buffer */
/*  makeRTPheader(rtp_data, NETEQ_CODEC_AVT_PT, 17,4711, 1235412312);
    makeDTMFpayload(&rtp_data[12], 1, 1, 10, 100);
    ok = WebRtcNetEQ_RecIn(inst, (short *) rtp_data, 12+4, 4711);
    printf("return value = %d; error code = %d\n", ok, WebRtcNetEQ_GetErrorCode(inst));*/

    /* check all limits of WebRtcNetEQ_AssignBuffer */
    ok=WebRtcNetEQ_AssignBuffer(inst, NetEqBufferMaxPackets, NetEqPacketBuffer, 149<<1);
    if((ok!=-1)||((ok==-1)&&(WebRtcNetEQ_GetErrorCode(inst)!=-PBUFFER_INIT_ERROR))) {
        printf("WebRtcNetEQ_AssignBuffer() did not return proper error code for wrong sizeinbytes\n");
    }
    ok=WebRtcNetEQ_AssignBuffer(inst, NetEqBufferMaxPackets, NULL, BufferSizeInBytes);
    if((ok!=-1)||((ok==-1)&&(WebRtcNetEQ_GetErrorCode(inst)!=-PBUFFER_INIT_ERROR))) {
        printf("WebRtcNetEQ_AssignBuffer() did not return proper error code for NULL memory pointer\n");
    }
    ok=WebRtcNetEQ_AssignBuffer(inst, 1, NetEqPacketBuffer, BufferSizeInBytes);
    if((ok!=-1)||((ok==-1)&&(WebRtcNetEQ_GetErrorCode(inst)!=-PBUFFER_INIT_ERROR))) {
        printf("WebRtcNetEQ_AssignBuffer() did not return proper error code for wrong MaxNoOfPackets\n");
    }
    ok=WebRtcNetEQ_AssignBuffer(inst, 601, NetEqPacketBuffer, BufferSizeInBytes);
    if((ok!=-1)||((ok==-1)&&(WebRtcNetEQ_GetErrorCode(inst)!=-PBUFFER_INIT_ERROR))) {
        printf("WebRtcNetEQ_AssignBuffer() did not return proper error code for wrong MaxNoOfPackets\n");
    }

    /* do correct assignbuffer */
    CHECK_ZERO(WebRtcNetEQ_AssignBuffer(inst, NetEqBufferMaxPackets, NetEqPacketBuffer, BufferSizeInBytes))

    ok=WebRtcNetEQ_SetExtraDelay(inst, -1);
    if((ok!=-1)||((ok==-1)&&(WebRtcNetEQ_GetErrorCode(inst)!=-FAULTY_DELAYVALUE))) {
        printf("WebRtcNetEQ_SetExtraDelay() did not return proper error code for too small delay\n");
    }
    ok=WebRtcNetEQ_SetExtraDelay(inst, 1001);
    if((ok!=-1)||((ok==-1)&&(WebRtcNetEQ_GetErrorCode(inst)!=-FAULTY_DELAYVALUE))) {
        printf("WebRtcNetEQ_SetExtraDelay() did not return proper error code for too large delay\n");
    }

    ok=WebRtcNetEQ_SetPlayoutMode(inst,(enum WebRtcNetEQPlayoutMode) 4711);
    if((ok!=-1)||((ok==-1)&&(WebRtcNetEQ_GetErrorCode(inst)!=-FAULTY_PLAYOUTMODE))) {
        printf("WebRtcNetEQ_SetPlayoutMode() did not return proper error code for wrong mode\n");
    }

    /* number of codecs should return zero before adding any codecs */
    WebRtcNetEQ_CodecDbGetSizeInfo(inst, &temp1, &temp2);
    if(temp1!=0)
        printf("WebRtcNetEQ_CodecDbGetSizeInfo() return non-zero number of codecs in DB before adding any codecs\n");

    /* get info from empty database */
    ok=WebRtcNetEQ_CodecDbGetCodecInfo(inst, 17, &usedCodec);
    if((ok!=-1)||((ok==-1)&&(WebRtcNetEQ_GetErrorCode(inst)!=-CODEC_DB_NOT_EXIST1))) {
        printf("WebRtcNetEQ_CodecDbGetCodecInfo() did not return proper error code for out-of-range entry number\n");
    }

    /* remove codec from empty database */
    ok=WebRtcNetEQ_CodecDbRemove(inst,kDecoderPCMa);
    if((ok!=-1)||((ok==-1)&&(WebRtcNetEQ_GetErrorCode(inst)!=-CODEC_DB_NOT_EXIST4))) {
        printf("WebRtcNetEQ_CodecDbRemove() did not return proper error code when removing codec that has not been added\n");
    }

    /* add codec with unsupported fs */
#ifdef CODEC_PCM16B
#ifndef NETEQ_48KHZ_WIDEBAND
    SET_CODEC_PAR(codecInst,kDecoderPCM16Bswb48kHz,77,NULL,48000);
    SET_PCM16B_SWB48_FUNCTIONS(codecInst);
    ok=WebRtcNetEQ_CodecDbAdd(inst, &codecInst);
    if((ok!=-1)||((ok==-1)&&(WebRtcNetEQ_GetErrorCode(inst)!=-CODEC_DB_UNSUPPORTED_FS))) {
        printf("WebRtcNetEQ_CodecDbAdd() did not return proper error code when adding codec with unsupported sample freq\n");
    }
#else
    printf("Could not test adding codec with unsupported sample frequency since NetEQ is compiled with 48kHz support.\n");
#endif
#else
    printf("Could not test adding codec with unsupported sample frequency since NetEQ is compiled without PCM16B support.\n");
#endif

    /* add two codecs with identical payload types */
    SET_CODEC_PAR(codecInst,kDecoderPCMa,17,NULL,8000);
    SET_PCMA_FUNCTIONS(codecInst);
    CHECK_ZERO(WebRtcNetEQ_CodecDbAdd(inst, &codecInst))

    SET_CODEC_PAR(codecInst,kDecoderPCMu,17,NULL,8000);
    SET_PCMU_FUNCTIONS(codecInst);
    ok=WebRtcNetEQ_CodecDbAdd(inst, &codecInst);
    if((ok!=-1)||((ok==-1)&&(WebRtcNetEQ_GetErrorCode(inst)!=-CODEC_DB_PAYLOAD_TAKEN))) {
        printf("WebRtcNetEQ_CodecDbAdd() did not return proper error code when adding two codecs with identical payload types\n");
    }

    /* try adding several payload types for CNG codecs */
    SET_CODEC_PAR(codecInst,kDecoderCNG,105,NULL,16000);
    SET_CNG_FUNCTIONS(codecInst);
    CHECK_ZERO(WebRtcNetEQ_CodecDbAdd(inst, &codecInst))
    SET_CODEC_PAR(codecInst,kDecoderCNG,13,NULL,8000);
    SET_CNG_FUNCTIONS(codecInst);
    CHECK_ZERO(WebRtcNetEQ_CodecDbAdd(inst, &codecInst))

    /* try adding a speech codec over a CNG codec */
    SET_CODEC_PAR(codecInst,kDecoderISAC,105,NULL,16000); /* same as WB CNG above */
    SET_ISAC_FUNCTIONS(codecInst);
    ok=WebRtcNetEQ_CodecDbAdd(inst, &codecInst);
    if((ok!=-1)||((ok==-1)&&(WebRtcNetEQ_GetErrorCode(inst)!=-CODEC_DB_PAYLOAD_TAKEN))) {
        printf("WebRtcNetEQ_CodecDbAdd() did not return proper error code when adding a speech codec over a CNG codec\n");
    }

    /* try adding a CNG codec over a speech codec */
    SET_CODEC_PAR(codecInst,kDecoderCNG,17,NULL,32000); /* same as PCMU above */
    SET_CNG_FUNCTIONS(codecInst);
    ok=WebRtcNetEQ_CodecDbAdd(inst, &codecInst);
    if((ok!=-1)||((ok==-1)&&(WebRtcNetEQ_GetErrorCode(inst)!=-CODEC_DB_PAYLOAD_TAKEN))) {
        printf("WebRtcNetEQ_CodecDbAdd() did not return proper error code when adding a speech codec over a CNG codec\n");
    }


    /* remove codec out of range */
    ok=WebRtcNetEQ_CodecDbRemove(inst,kDecoderReservedStart);
    if((ok!=-1)||((ok==-1)&&(WebRtcNetEQ_GetErrorCode(inst)!=-CODEC_DB_UNSUPPORTED_CODEC))) {
        printf("WebRtcNetEQ_CodecDbRemove() did not return proper error code when removing codec that is out of range\n");
    }
    ok=WebRtcNetEQ_CodecDbRemove(inst,kDecoderReservedEnd);
    if((ok!=-1)||((ok==-1)&&(WebRtcNetEQ_GetErrorCode(inst)!=-CODEC_DB_UNSUPPORTED_CODEC))) {
        printf("WebRtcNetEQ_CodecDbRemove() did not return proper error code when removing codec that is out of range\n");
    }

    /*SET_CODEC_PAR(codecInst,kDecoderEG711a,NETEQ_CODEC_EG711A_PT,NetEqiPCMAState,8000);
    SET_IPCMA_FUNCTIONS(codecInst);
    CHECK_ZERO(WebRtcNetEQ_CodecDbAdd(inst, &codecInst))
*/
    free(inst);

    return(0);

}
