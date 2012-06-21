/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

//#include "isac_codec.h"
//#include "isac_structs.h"
#include "isacfix.h"


#define NUM_CODECS 1

int main(int argc, char* argv[])
{
    FILE *inFileList;
    FILE *audioFile;
    FILE *outFile;
    char audioFileName[501];
    short audioBuff[960];
    short encoded[600];
    short startAudio;
    short encodedLen;
    ISACFIX_MainStruct *isac_struct;
    unsigned long int hist[601];

    // reset the histogram
    for(short n=0; n < 601; n++)
    {
        hist[n] = 0;
    }
    
    
    inFileList = fopen(argv[1], "r");
    if(inFileList == NULL)
    {
        printf("Could not open the input file.\n");
        getchar();
        exit(-1);
    }
    outFile = fopen(argv[2], "w");
    if(outFile == NULL)
    {
        printf("Could not open the histogram file.\n");
        getchar();
        exit(-1);
    }

    short frameSizeMsec = 30;
    if(argc > 3)
    {
        frameSizeMsec = atoi(argv[3]);
    }
    
    short audioOffset = 0;
    if(argc > 4)
    {
        audioOffset = atoi(argv[4]);
    }
    int ok;
    ok = WebRtcIsacfix_Create(&isac_struct);
    // instantaneous mode
    ok |= WebRtcIsacfix_EncoderInit(isac_struct, 1);
    // is not used but initialize
    ok |= WebRtcIsacfix_DecoderInit(isac_struct);    
    ok |= WebRtcIsacfix_Control(isac_struct, 32000, frameSizeMsec);

    if(ok != 0)
    {
        printf("\nProblem in seting up iSAC\n");
        exit(-1);
    } 

    while( fgets(audioFileName, 500, inFileList) != NULL )
    {
        // remove trailing white-spaces and any Cntrl character
        if(strlen(audioFileName) == 0)
        {
            continue;
        }
        short n = strlen(audioFileName) - 1;
        while(isspace(audioFileName[n]) || iscntrl(audioFileName[n]))
        {
            audioFileName[n] = '\0';
            n--;
            if(n < 0)
            {
                break;
            }
        }

        // remove leading spaces
        if(strlen(audioFileName) == 0)
        {
            continue;
        }
        n = 0;
        while((isspace(audioFileName[n]) || iscntrl(audioFileName[n])) && 
            (audioFileName[n] != '\0'))
        {
            n++;
        }
        memmove(audioFileName, &audioFileName[n], 500 - n);
        if(strlen(audioFileName) == 0)
        {
            continue;
        }
        audioFile = fopen(audioFileName, "rb");
        if(audioFile == NULL)
        {
            printf("\nCannot open %s!!!!!\n", audioFileName);
            exit(0);
        }

        if(audioOffset > 0)
        {
            fseek(audioFile, (audioOffset<<1), SEEK_SET);
        }
        
        while(fread(audioBuff, sizeof(short), (480*frameSizeMsec/30), audioFile) >= (480*frameSizeMsec/30))
        {            
            startAudio = 0;
            do
            {
                encodedLen = WebRtcIsacfix_Encode(isac_struct, 
                                                  &audioBuff[startAudio], encoded);
                startAudio += 160;            
            } while(encodedLen == 0);

            if(encodedLen < 0)
            {
                printf("\nEncoding Error!!!\n");
                exit(0);
            }
            hist[encodedLen]++;
        }
        fclose(audioFile);
    }
    fclose(inFileList);
    unsigned long totalFrames = 0;
    for(short n=0; n < 601; n++)
    {
        totalFrames += hist[n];
        fprintf(outFile, "%10lu\n", hist[n]);
    }
    fclose(outFile);
    
    short topTenCntr = 0;
    printf("\nTotal number of Frames %lu\n\n", totalFrames);
    printf("Payload Len    # occurences\n");
    printf("===========    ============\n");

    for(short n = 600; (n >= 0) && (topTenCntr < 10); n--)
    {
        if(hist[n] > 0)
        {
            topTenCntr++;
            printf("    %3d            %3d\n", n, hist[n]);
        }
    }
    WebRtcIsacfix_Free(isac_struct);    
    return 0;
}
    
