/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */


//Testing stream crap...

#include <direct.h>
#include <stdlib.h>
#include <io.h>

#include "nsISupports.h"
#include "nsTokenizer.h"
#include "nsHTMLDelegate.h"
#include "nsIParser.h"
#include "nsHTMLContentSink.h"

ofstream filelist("filelist.out");

PRBool compareFiles(const char* file1,const char* file2,int& failpos) {
  PRBool result=PR_TRUE;
  PRBool done=PR_FALSE;
  char ch1,ch2;
  int  eof1,eof2;

  ifstream  input1(file1,ios::in && ios::binary,filebuf::openprot);
  ifstream  input2(file2,ios::in && ios::binary,filebuf::openprot);
  input1.setmode(filebuf::binary);
  input2.setmode(filebuf::binary);
  failpos=-1;

  while(!done) {

    while(!(eof1=input1.eof())) {
      input1.read(&ch1,1);
      if(failpos>4225) {
        int x=failpos;
      }
      failpos++;
      char* p=strchr(" \t\r\n\b",ch1);
      if(!p)
        break;
    }

    while(!(eof2=input2.eof())) {
      input2.read(&ch2,1);
      char* p=strchr(" \t\r\n\b",ch2);
      if(!p)
        break;
    }
    
    if(eof1==eof2) {
      if(eof1) 
        done=PR_TRUE;
      else if(ch1!=ch2) {
        done=PR_TRUE;
        result=PR_FALSE;
        
      }
    }
    else done=PR_TRUE;
  }
  return result;
}


/**-------------------------------------------------------
 * LAST MODS:  gess
 *  
 * @param  
 * @return 
 *------------------------------------------------------*/
void parseFile (const char* aFilename,int size)
{
  //debug
  //aFilename="s:\\ns\\raptor\\parser\\tests\\html\\home01.html";
  //aFilename="c:\\temp\\sun\\test00000.html";
  //aFilename="c:\\windows\\temp\\test.html";
  aFilename="s:\\readHTML-VC\\test.html";
  //aFilename="c:\\temp\\sun\\commentbug.html";


  char filename[_MAX_PATH];
  strcpy(filename,aFilename);
  strcat(filename,".tokens");
  {
    nsIParser* parser;
    nsresult rv = NS_NewParser(&parser);
    nsresult r=NS_NewParser(&parser);
    CHTMLContentSink theSink;
    parser->setContentSink(&theSink);
    parser->parse(aFilename);
    parser->Release();
  }

  int failpos=0;

  if(!compareFiles(aFilename,filename,failpos)) {
    filelist << "FAILED: " << aFilename << "[" << failpos << "]" << endl;
  }
}


/**-------------------------------------------------------
 * LAST MODS:  gess
 *  
 * @param  
 * @return 
 *------------------------------------------------------*/
int walkDirectoryTree(char* aPath) {
  int     result=0;
  char    fullPath[_MAX_PATH];
  struct  _finddata_t c_file;    
  long    hFile;

  strcpy(fullPath,aPath);
  strcat(fullPath,"\\*.*");
  /* Find first .c file in current directory */
  if((hFile = _findfirst( fullPath, &c_file )) == -1L )
     printf( "No matching files in current directory!\n" );   
  else {

    PRBool done=PR_FALSE;
    while(!done) {
      if(c_file.attrib & _A_SUBDIR) {
        if(strlen(c_file.name)>2){
          char newPath[_MAX_PATH];
          strcpy(newPath,aPath);
          strcat(newPath,"\\");
          strcat(newPath,c_file.name);
          walkDirectoryTree(newPath);
        }
      }
      else {
        int len=strlen(c_file.name);
        if(len>5) {
          if(0==strnicmp(&c_file.name[len-5],".HTML",5)) {
            char filepath[_MAX_PATH];            
            strcpy(filepath,aPath);
            strcat(filepath,"\\");
            strcat(filepath,c_file.name);
            parseFile(filepath,c_file.size);
          }
        }
      }
      if(_findnext( hFile, &c_file )!=0) 
        done=PR_TRUE;
    }       
    _findclose( hFile );   
  }
  return 0;
}


/**-------------------------------------------------------
 * LAST MODS:  gess
 *  
 * @param  
 * @return 
 *------------------------------------------------------*/
int main(int argc, char* argv [])
{
  int   result=0;
  char   buffer[_MAX_PATH];

  if(argc==2) 
    strcpy(buffer,argv[1]);
  else _getcwd(buffer,_MAX_PATH);
  walkDirectoryTree(buffer);
  return 0;
}





