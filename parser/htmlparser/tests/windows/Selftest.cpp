/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#include <direct.h>
#include <stdlib.h>
#include <stdio.h>
#include <io.h>
#include <time.h>
#include <fstream.h>

#include "nsScanner.h"
#include "nsTokenizer.h"
#include "nsHTMLDelegate.h"


bool compareFiles(const char* file1,const char* file2) {
  bool result=true;
  bool done=false;
  char ch1,ch2;
  int  eof1,eof2;

  ifstream	input1(file1,ios::in && ios::binary,filebuf::openprot);
  ifstream	input2(file2,ios::in && ios::binary,filebuf::openprot);

  while(!done) {

    while(!(eof1=input1.eof())) {
      input1.read(&ch1,1);
      if(('\n'!=ch1) && (' '!=ch1))
        break;
    }

    while(!(eof2=input2.eof())) {
      input2.read(&ch2,1);
      if(('\n'!=ch2) && (' '!=ch2))
        break;
    }
    
    if(eof1==eof2) {
      if(eof1) 
        done=true;
      else if(ch1!=ch2) {
        done=true;
        result=false;
      }
    }
    else done=true;
  }
  return result;
}

void parseFile (const char* aFilename,int size)
{
  //debug
  aFilename="s:\\ns\\raptor\\parser\\tests\\html\\tag001.html";
  //aFilename="c:\\temp\\temp.html";

  char filename[_MAX_PATH];
  strcpy(filename,aFilename);
  strcat(filename,".tokens");
  {
    ofstream out(filename);
    ifstream	input(aFilename);
	  CScanner scanner(input);
  	CHTMLTokenizerDelegate delegate;
	  CTokenizer tokenizer(scanner,delegate);
    tokenizer.tokenize();
    tokenizer.debugDumpSource(out);
  }
  cout << aFilename;
  if(compareFiles(aFilename,filename))
    cout << " PASS " << endl;
  else cout << " FAIL" << endl;
}

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

    bool done=false;
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
        done=true;
    }       
    _findclose( hFile );   
  }
  return 0;
}

int main(int argc, char* argv [])
{
  int   result=0;
  char 	buffer[_MAX_PATH];

  if(argc==2) 
    strcpy(buffer,argv[1]);
  else _getcwd(buffer,_MAX_PATH);
  walkDirectoryTree(buffer);
  return 0;
}



