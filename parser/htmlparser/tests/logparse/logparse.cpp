/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsXPCOM.h"
#include "nsIComponentManager.h"
#include "nsParserCIID.h"
#include "nsIAtom.h"
#include "nsIParser.h"
#include "nsILoggingSink.h"
#include "nsIIOService.h"
#include "nsNetCID.h"
#include "nsIURI.h"
#include "CNavDTD.h"
#include <fstream.h>

// Class IID's
static NS_DEFINE_CID(kParserCID, NS_PARSER_CID);
static NS_DEFINE_IID(kLoggingSinkCID, NS_LOGGING_SINK_CID);

// Interface IID's

//----------------------------------------------------------------------

static const char* kWorkingDir = "./";

nsresult GenerateBaselineFile(const char* aSourceFilename,const char* aBaselineFilename)
{
  if (!aSourceFilename || !aBaselineFilename)
     return NS_ERROR_INVALID_ARG;

  nsresult rv;

  // Create a parser
  nsCOMPtr<nsIParser> parser(do_CreateInstance(kParserCID, &rv));
  if (NS_FAILED(rv)) {
    cout << "Unable to create a parser (" << rv << ")" <<endl;
    return rv;
  }

  // Create a sink
  nsCOMPtr<nsILoggingSink> sink(do_CreateInstance(kLoggingSinkCID, &rv));
  if (NS_FAILED(rv)) {
    cout << "Unable to create a sink (" << rv << ")" <<endl;
    return rv;
  }

  nsCOMPtr<nsILocalFile> localfile (do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv));
  if (NS_FAILED(rv))
    return rv;

  localfile->InitWithNativePath(nsDependentCString(aSourceFilename));
  nsCOMPtr<nsIURI> inputURI;
  {
    nsCOMPtr<nsIIOService> ioService(do_GetService(NS_IOSERVICE_CONTRACTID, &rv));
    if (NS_FAILED(rv))
      return rv;
    rv = ioService->NewFileURI(localfile, getter_AddRefs(inputURI));
    if (NS_FAILED(rv))
      return rv;
  }
  localfile->InitWithNativePath(nsDependentCString(aBaselineFilename));
  PRFileDesc *outputfile;
  localfile->OpenNSPRFileDesc(0660, PR_WRONLY | PR_CREATE_FILE, &outputfile);
  sink->SetOutputStream(outputfile);

  // Parse the document, having the sink write the data to fp
  parser->SetContentSink(sink);

  rv = parser->Parse(inputURI, 0, PR_FALSE, eDTDMode_unknown);

  return rv;
}

//----------------------------------------------------------------------

bool CompareFiles(const char* aFilename1, const char* aFilename2) {
  bool result=true;

  fstream theFirstStream(aFilename1,ios::in | ios::nocreate);
  fstream theSecondStream(aFilename2,ios::in | ios::nocreate);

  bool done=false;
  char   ch1,ch2;

  while(!done) {
    theFirstStream >> ch1;
    theSecondStream >> ch2;
    if(ch1!=ch2) {
      result=PR_FALSE;
      break;
    }
    done=bool((theFirstStream.ipfx(1)==0) || (theSecondStream.ipfx(1)==0));
  }
  return result;
}

//----------------------------------------------------------------------

void ComputeTempFilename(const char* anIndexFilename, char* aTempFilename) {
  if(anIndexFilename) {
    strcpy(aTempFilename,anIndexFilename);
    char* pos=strrchr(aTempFilename,'\\');
    if(!pos)
      pos=strrchr(aTempFilename,'/');
    if(pos) {
      (*pos)=0;
      strcat(aTempFilename,"/temp.blx");
      return;
    }
  }
  //fall back to our last resort...
  strcpy(aTempFilename,"c:/windows/temp/temp.blx");
}

//----------------------------------------------------------------------

static const char* kAppName = "logparse ";
static const char* kOption1 = "Compare baseline file-set";
static const char* kOption2 = "Generate baseline ";
static const char* kResultMsg[2] = {" failed!"," ok."};

void ValidateBaselineFiles(const char* anIndexFilename) {

  fstream theIndexFile(anIndexFilename,ios::in | ios::nocreate);
  char    theFilename[500];
  char    theBaselineFilename[500];
  char    theTempFilename[500];
  bool    done=false;

  ComputeTempFilename(anIndexFilename,theTempFilename);

  while(!done) {
    theIndexFile >> theFilename;
    theIndexFile >> theBaselineFilename;
    if(theFilename[0] && theBaselineFilename[0]) {
      if(NS_SUCCEEDED(GenerateBaselineFile(theFilename,theTempFilename))) {
        bool matches=CompareFiles(theTempFilename,theBaselineFilename);
        cout << theFilename << kResultMsg[matches] << endl;
      }
    }
    theFilename[0]=0;
    theBaselineFilename[0]=0;
    done=bool(theIndexFile.ipfx(1)==0);
  }


  // Now it's time to compare our output to the baseline...
//  if(!CompareFiles(aBaselineFilename,aBaselineFilename)){
//    cout << "File: \"" << aSourceFilename << "\" does not match baseline." << endl;
//  }

}


//----------------------------------------------------------------------

int main(int argc, char** argv)
{
  if (argc < 2) {
    cout << "Usage: " << kAppName << " [options] [filename]" << endl;
    cout << "     -c [filelist]   " << kOption1 << endl;
    cout << "     -g [in] [out]   " << kOption2 << endl;
    return -1;
  }

  int result=0;

  nsresult rv = NS_InitXPCOM2(nsnull, nsnull, nsnull);
  if (NS_FAILED(rv)) {
    printf("NS_InitXPCOM2 failed\n");
    return 1;
  }

  if(0==strcmp("-c",argv[1])) {

    if(argc>2) {
      cout << kOption1 << "..." << endl;

      //Open the master filelist, and read the filenames.
      //Each line contains a source filename and a baseline filename, separated by a space.
      ValidateBaselineFiles(argv[2]);
    }
    else {
      cout << kAppName << ": Filelist missing for -c option -- nothing to do." << endl;
    }

  }
  else if(0==strcmp("-g",argv[1])) {
    if(argc>3) {
      cout << kOption2 << argv[3] << " from " << argv[2] << "..." << endl;
      GenerateBaselineFile(argv[2],argv[3]);
    }
    else {
      cout << kAppName << ": Filename(s) missing for -g option -- nothing to do." << endl;
    }
  }
  else {
    cout << kAppName << ": Unknown options -- nothing to do." << endl;
  }
  return result;
}
