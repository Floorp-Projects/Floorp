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
 * The Original Code is Dreftool.
 *
 * The Initial Developer of the Original Code is
 * Rick Gessner <rick@gessner.com>.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Kenneth Herron <kjh-5727@comcast.net>
 *   Bernd Mielke <bmlk@gmx.de>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include <iostream.h>
#include <fstream.h>
#include "nsString.h"
#include "prio.h"
#include "plstr.h"
#include "CScanner.h"
#include "CCPPTokenizer.h"
#include "nsReadableUtils.h"
#include "tokens.h"
#include "CToken.h"
#include "stdlib.h"

//*******************************************************************************
// Global variables...
//*******************************************************************************
bool  gVerbose=false;
bool  gShowIncludeErrors=false;
bool  gEmitHTML=false;
bool  gSloppy=false;
const char *gRootDir;


class IPattern {
public:
  virtual CToken* scan(CCPPTokenizer& aTokenizer,int anIndex,ostream& anOutput)=0;
};


class CPattern {
public:
  CPattern(const nsAFlatCString& aFilename)
  : mNames(0),
    mSafeNames(0),
    mIndex(0),
    mLineNumber(1),
    mErrCount(0),
    mFilename(aFilename)
  {
  }

  CToken* matchID(nsDeque& aDeque,nsAFlatCString& aName){
    int theMax=aDeque.GetSize();
    for(int theIndex=0;theIndex<theMax;theIndex++){
      CToken* theToken=(CToken*)aDeque.ObjectAt(theIndex);
      if(theToken){
        nsAFlatCString& theName=theToken->getStringValue();
        if(theName==aName) {
          return theToken;
        }
      }
    }
    return 0;
  }

  /***************************************************************************
    This gets called when we know we've seen an if statement (at anIndex).
    Look into the parm list (...) for an ID. If you find one, then you know
    it's a conditioned identifier which can be safely ignored during other
    error tests.
   ***************************************************************************/
  CToken* findSafeIDInIfStatement(CCPPTokenizer& aTokenizer) {
    CToken* result=0;

    enum eState {eSkipLparen,eFindID};
    eState  theState=eSkipLparen;
    CToken* theToken=0;
    while((theToken=aTokenizer.getTokenAt(++mIndex))) {
      int   theType=theToken->getTokenType();
      nsAFlatCString& theString=theToken->getStringValue();
      char theChar=theString.First();

      switch(theState) {
        case eSkipLparen:
            //first scan for the lparen...
          switch(theType) {
            case eToken_comment:
            case eToken_whitespace:
            case eToken_newline:
              mLineNumber+=theString.CountChar('\n');
              break;

            case eToken_operator:
              switch(theChar) {
                case '(':
                  theState=eFindID;
                  break;
                case ')':
                  return 0;
                default:
                  break;
              } //switch
              break;
            default:
              break;
          } //switch
          break;

          //now scan for the identifier...
        case eFindID:
          switch(theType) {
            case eToken_comment:
            case eToken_whitespace:
            case eToken_newline:
              mLineNumber+=theString.CountChar('\n'); break;

            case eToken_identifier:
              if(matchID(mNames,theString)) {
                return theToken;
              }
              break;
            case eToken_operator:
              switch(theChar) {
                case ')':
                case ';':
                case '{':
                case '(': 
                  return result;
                default:
                  break;
              } //switch
              break;
            default:
              break;
          } //switch
          break;

        default:
          switch(theType) {
            case eToken_comment:
            case eToken_whitespace:
            case eToken_newline:
              mLineNumber+=theString.CountChar('\n'); 
              break;
            default:
              break;
          }//switch
          break;
      } //switch
    }
    return result;
  }


  /***************************************************************************
    This gets called when we know we've seen an NS_ENSURE_TRUE statement (at anIndex).
    Look into the parm list (...) for an ID. If you find one, then you know
    it's a conditioned identifier which can be safely ignored during other
    error tests.
   ***************************************************************************/
  CToken* findSafeIDInEnsureTrue(CCPPTokenizer& aTokenizer) {

    CToken* result=0;
    enum eState {eSkipLparen,eFindID};
    eState  theState=eSkipLparen;
    CToken* theToken=0;
    while((theToken=aTokenizer.getTokenAt(++mIndex))) {
      int   theType=theToken->getTokenType();
      nsAFlatCString& theString=theToken->getStringValue();
      char theChar=theString.First();

      switch(theState) {
        case eSkipLparen:
            //first scan for the lparen...
          switch(theType) {
            case eToken_comment:
            case eToken_whitespace:
            case eToken_newline:
              mLineNumber+=theString.CountChar('\n');
              break;

            case eToken_operator:
              switch(theChar) {
                case '(':
                  theState=eFindID;
                  break;
                case ')':
                  return 0;
                default:
                  break;
              } //switch
              break;
            default:
              break;
          } //switch
          break;

          //now scan for the identifier...
        case eFindID:
          switch(theType) {
            case eToken_comment:
            case eToken_whitespace:
            case eToken_newline:
              mLineNumber+=theString.CountChar('\n'); break;

            case eToken_identifier:
              if(matchID(mNames,theString)) {
                return theToken;
              }
              break;
            case eToken_operator:
              switch(theChar) {
                case ')':
                case ';':
                case '{':
                case '(': 
                  return result;
                default:
                  break;
              } //switch
              break;
            default:
              break;
          } //switch
          break;

        default:
          switch(theType) {
            case eToken_comment:
            case eToken_whitespace:
            case eToken_newline:
              mLineNumber+=theString.CountChar('\n'); 
              break;
            default:
              break;
          }//switch
          break;
      } //switch
    }
    return result;
  }
   /***************************************************************************
    We're looking for the variable that is new created by a new 
   ***************************************************************************/
  CToken* findNewVariable(CCPPTokenizer& aTokenizer, int anIndex){
    CToken* theToken=0;
    while((theToken=aTokenizer.getTokenAt(--anIndex))){
      if(theToken) {
        int theType=theToken->getTokenType();
        nsAFlatCString& theString=theToken->getStringValue();
        switch(theType){
        case eToken_semicolon:
          return 0;
        case eToken_operator:
          if (!StringBeginsWith(theString,NS_LITERAL_CSTRING("="))){
            return 0;
          }
          break;
        case eToken_identifier:
          if (StringBeginsWith(theString,NS_LITERAL_CSTRING("return"))){
            return 0;
          }
          return theToken;
        default:
          break;
        }
      } //if
    } //while
    return 0;
  }
  void check_unsafe_ID(CToken* theToken){
  }
  void check_save_ID(CToken* theToken){
  }
  /***************************************************************************
    We're looking for the pattern foo-> without the preceeding (conditional) if():
    Assume that the given id (aName) exists in the tokenizer at anIndex.

    if(foo)
      foo->
   ***************************************************************************/
  CToken* findPtrDecl(CCPPTokenizer& aTokenizer,int anIndex,PRUnichar& anOpChar,bool allocRequired) {

    enum eState {eFindNew,eFindType,eFindPunct,eFindStar};
    CToken* theToken=0;
    CToken* theID=0;
    eState  theState=eFindStar;
    
    while((theToken=aTokenizer.getTokenAt(++anIndex))){
      if(theToken) {
        nsAFlatCString& theString=theToken->getStringValue();
        int     theType=theToken->getTokenType();
        
        anOpChar=theString.First();
        switch(theType){

          case eToken_semicolon:
          case eToken_operator:
            switch(theState) {
              case eFindStar:
                if('*'!=anOpChar)
                  return 0;
                theState=eFindType;
                break;
              case eFindPunct:
                switch(anOpChar) {
                  case '=':
                    if(!allocRequired)
                      return theID;
                    theState=eFindNew;
                    break;
                  case ';':
                    return (allocRequired) ? 0 : theID;
                  default:
                    break;
                }
                break;
              default:
                return 0;
            }
            break;

          case eToken_identifier:
            switch(theState){
              case eFindType:
                theID=theToken;
                theState=eFindPunct;
                break;
              case eFindNew:
                if(theString.Equals(NS_LITERAL_CSTRING("new")) ||
                   theString.Equals(NS_LITERAL_CSTRING("malloc")) ||
                   theString.Equals(NS_LITERAL_CSTRING("calloc")) ||
                   theString.Equals(NS_LITERAL_CSTRING("PR_Malloc")) ||
                   theString.Equals(NS_LITERAL_CSTRING("PR_Calloc")) ||
                   theString.Equals(NS_LITERAL_CSTRING("ToNewCString")) ||
                   theString.Equals(NS_LITERAL_CSTRING("ToNewUTF8String")) ||
                   theString.Equals(NS_LITERAL_CSTRING("ToNewUnicode")) ||
                   theString.Equals(NS_LITERAL_CSTRING("UTF8ToNewUnicode")))
                  return theID;
                return 0;
              default:
                return 0;
          }
          break;

          default:
            break;
        }
      }//if
    }//while
    return 0;
  }

  /***************************************************************************
    We're looking for the pattern *foo.
   ***************************************************************************/
  bool hasSimpleDeref(CCPPTokenizer& aTokenizer,int anIndex) {
    bool result=false;

    int theMin=anIndex-3;
    for(int theIndex=anIndex-1;theIndex>theMin;theIndex--){
      CToken* theToken=aTokenizer.getTokenAt(theIndex);
      if(theToken) {
        nsAFlatCString& theString=theToken->getStringValue();
        int     theType=theToken->getTokenType();
        char   theChar=theString.First();
        if(eToken_operator==theType) {
          if('*'==theChar){
            result=true;
          }
        }
      }
    }

    if(result) {
      int theMax=anIndex+3;
      for(int theIndex=anIndex;theIndex<theMax;theIndex++){
        CToken* theToken=aTokenizer.getTokenAt(theIndex);
        if(theToken) {
          nsAFlatCString& theString=theToken->getStringValue();
          int     theType=theToken->getTokenType();
          char   theChar=theString.First();
          if(eToken_operator==theType) {
            if((','==theChar) || (')'==theChar)){
              result=false;
            }
          }
        }
      }
    }

    return result;
  }

  /***************************************************************************
    We're looking for the pattern foo-> without the preceeding (conditional) if():
    Assume that the given id (aName) exists in the tokenizer at anIndex.

    if(foo)
      foo->
   ***************************************************************************/
  bool hasUnconditionedDeref(nsAFlatCString& aName,CCPPTokenizer& aTokenizer,int anIndex) {
    bool result=false;

    enum eState {eFailure,eFindIf,eFindDeref};
    eState  theState=eFindDeref;
    int     theIndex=anIndex+1;
    
    while(eFailure!=theState){
      CToken* theToken=aTokenizer.getTokenAt(theIndex);
      if(theToken) {
        nsAFlatCString& theString=theToken->getStringValue();
        int     theType=theToken->getTokenType();
        char   theChar=theString.First();
        switch(theState){
          case eFindDeref:
              switch(theType) {
                case eToken_whitespace:
                case eToken_comment:
                case eToken_newline:
                  theIndex++;
                  break;
                default:
                  if (StringBeginsWith(theString, NS_LITERAL_CSTRING("->"))) {
                    theIndex=anIndex-1; //now start looking backwards...
                    theState=eFindIf;
                  }
                  else theState=eFailure;
              }//switch
              break;

          case eFindIf:
            switch(theType) {
              case eToken_whitespace:
              case eToken_comment:
              case eToken_newline:
                theIndex--;
                break;
              case eToken_semicolon:
                return true;
                break;

              case eToken_operator:
                switch(theChar){
                  case '{':
                  case '+':
                  case '-':
                  case '=':
                  case '*':
                  case '%':
                    return true;
                  default:
                    break;
                }
                theIndex-=1;
                break;
              case eToken_identifier:
                if (StringBeginsWith(theString, NS_LITERAL_CSTRING("if"))) {
                  return false;
                }
                if (theString.Equals(aName)) {
                  return false;
                }
                if (StringBeginsWith(theString, NS_LITERAL_CSTRING("while"))) {
                  return false;
                }
                return true;
              default:
                theIndex--;
            } //switch
            break;
          case eFailure:
            break;
        }//switch
      }//if
      else theState=eFailure;
    }//while

    return result;
  }

  void skipSemi(CCPPTokenizer& aTokenizer) {
    CToken*   theToken=0;
    while((theToken=aTokenizer.getTokenAt(++mIndex))){
      int     theType=theToken->getTokenType();
      nsAFlatCString& theString=theToken->getStringValue();
      char   theChar=theString.First();

      switch(theType){
        case eToken_semicolon:
          return;
        case eToken_operator:
          if(('}'==theChar) || (';'==theChar))
            return;
          break;
        case eToken_newline:
        case eToken_comment:
        case eToken_whitespace:
          mLineNumber+=theString.CountChar('\n');
        default:
          break;
      } //switch
    }//while
  }

  void OutputMozillaFilename(ostream& anOutput) {
    mFilename.ReplaceChar('\\','/');
    PRInt32 pos=mFilename.RFind("/") + 1;
    nsCAutoString entries,repository;
    {
      nsCAutoString cvsbase(
        Substring(mFilename, 0, pos) +
        NS_LITERAL_CSTRING("CVS/")
      );
      entries=cvsbase+NS_LITERAL_CSTRING("Entries");
      repository=cvsbase+NS_LITERAL_CSTRING("Repository");
    }
    nsCAutoString filename(
      Substring(mFilename, pos)
    );
    char cvspath[1024], cvsfilename[256], version[10], prefix;
    bool found = false;
    {
      fstream cvsrecord (entries.get(),ios::in);
      while (!cvsrecord.eof()) {
        cvsrecord.get(prefix);
        if (prefix=='/') {
          cvsrecord.getline(cvsfilename,sizeof cvsfilename, '/');
          if (found = !strcmp(filename.get(), cvsfilename))
            break;
        }
        cvsrecord.ignore(256,'\n');
      }
      if (found)
        cvsrecord.getline(version,sizeof version, '/');
    }
    {
      fstream cvsrecord (repository.get(),ios::in);
      cvsrecord.getline(cvspath,sizeof cvspath);
      if (cvspath[0])
        filename =
          NS_LITERAL_CSTRING("/") +
          nsDependentCString(cvspath) + 
          NS_LITERAL_CSTRING("/") + 
          filename;
      else
        filename = mFilename;
    }
    pos=filename.Find("mozilla/");
    const char* buf=filename.get();

    if(-1<pos) {
      buf+=pos+(found?0:8);

      if(gEmitHTML) {
        anOutput << "<br><a href=\"" ;
        if (found)
          anOutput
            << "http://bonsai.mozilla.org/cvsblame.cgi?file="
            << buf
            << "&rev="
            << version
            << "&mark="
            << mLineNumber
            << "#"
            << mLineNumber - 5;
        else
          anOutput
            << "http://lxr.mozilla.org/mozilla/source/"
            << buf
            << "#"
            << mLineNumber;

        cout << "\">";  
      }

      cout << buf << ":" << mLineNumber;

      if(gEmitHTML) {
        cout << "</a>";
      }

    }
    else {
      anOutput << buf;
    }
  }


  /***************************************************************************
    This method iterates the tokens (in tokenizer) looking for statements.
    The statements are then decoded to find pointer variables. If they get a 
    new call, then they're recorded in mNames. If they're in an IF(), then
    they're recording in mSafeNames. We also look for simple dereferences.
    When we find one, we look it up in mSafeNames. If it's not there, we have
    a potential deref error.
   ***************************************************************************/
  void scan(CCPPTokenizer& aTokenizer,ostream& anOutput){
    int     theMax=aTokenizer.getCount();
    int     theIDCount=mNames.GetSize();
    int     theSafeIDCount=mSafeNames.GetSize();

    while(mIndex<theMax){
      CToken*   theToken=aTokenizer.getTokenAt(mIndex);
      int     theType=theToken->getTokenType();
      nsAFlatCString& theString=theToken->getStringValue();
      if (!theString.IsEmpty())
      switch(theType){
        case eToken_operator:
          {
            char   theChar=theString.First();
            switch(theChar){
              case '{':
                mIndex++;
                scan(aTokenizer,anOutput);
                break;
              case '}':
                //before returning, let's remove the new identifiers...
                while(mNames.GetSize()>theIDCount){
                  mNames.Pop();
                }
                while(mSafeNames.GetSize()>theSafeIDCount){
                  mSafeNames.Pop();
                }
                return;
              default:
                break;
            }
          }
          break;

        case eToken_comment:
        case eToken_newline:
        case eToken_whitespace:
          mLineNumber+=theString.CountChar('\n');
          break;

            //If it's an ID, then we're looking for 3 patterns:
            //  1. ID* ID...; 
            //  2. *ID=ID;
            //  3. ID->XXX;
        case eToken_identifier:
          {
            PRUnichar theOpChar=0;
            if((theToken=findPtrDecl(aTokenizer,mIndex,theOpChar,true))) {
              //we found ID* ID; (or) ID* ID=...;
              mNames.Push(theToken);
              mIndex+=2;
              skipSemi(aTokenizer);
            }

            else if (StringBeginsWith(theString, NS_LITERAL_CSTRING("if"))) {
              CToken* theSafeID=findSafeIDInIfStatement(aTokenizer);
              if(theSafeID) {
                mSafeNames.Push(theSafeID);
                theSafeIDCount++;
              }
            }
            else if (StringBeginsWith(theString, NS_LITERAL_CSTRING("NS_ENSURE"))) {
              CToken* theSafeID=findSafeIDInEnsureTrue(aTokenizer);
              if (theSafeID) {
                mSafeNames.Push(theSafeID);
                theSafeIDCount++;
                }
              }
            else if (!gSloppy &&
                     (theString.Equals(NS_LITERAL_CSTRING("new")))) {
              CToken* theToken=findNewVariable(aTokenizer,mIndex);
              if(theToken) {
                mNames.Push(theToken);
                theIDCount++;
                CToken* lastSafe=(CToken*)mSafeNames.PeekFront();
                if(lastSafe){
                  if(theToken==lastSafe){
                    mSafeNames.Pop();
                    theSafeIDCount--;
                  }
                }
              }
            }
            else {
              CToken* theMatch=matchID(mNames,theString);
              if(theMatch){
                if((hasUnconditionedDeref(theString,aTokenizer,mIndex)) || 
                   (hasSimpleDeref(aTokenizer,mIndex))){
                  CToken* theSafeID=matchID(mSafeNames,theString);
                  if(theSafeID) {
                    nsAFlatCString& s1=theSafeID->getStringValue();
                    if(s1.Equals(theString))
                      break;
                  }
                  //dump the name out in LXR format

                  OutputMozillaFilename(anOutput);

                  anOutput << "   Deref-error: \""
                           << PromiseFlatCString(theString).get()
                           << "\"" << endl;
                  mErrCount++;
                }
              }
            }
          }
          break;
        default:
          break;
      }
      mIndex++;
    }
  }
  
  nsDeque     mNames;
  nsDeque     mSafeNames;
  int         mIndex;
  int         mLineNumber;
  int         mErrCount;
  nsCString   mFilename;
};

void ScanFile(const nsAFlatCString& aFilename,ostream& anOutput,int& aLineCount,int& anErrCount) {
  nsCAutoString theCStr(aFilename);
 
  fstream input(PromiseFlatCString(theCStr).get(),ios::in);
  CScanner theScanner(input);
  CCPPTokenizer theTokenizer;
  theTokenizer.tokenize(theScanner);

  CPattern thePattern(aFilename);
  thePattern.scan(theTokenizer,anOutput);
  aLineCount+=thePattern.mLineNumber;
  anErrCount+=thePattern.mErrCount;
}


void IteratePath(const char *aPath, ostream& anOutput,
  int& aFilecount,int& aLinecount,int& anErrcount) {
  PRFileInfo eInfo;
  if (PR_SUCCESS == PR_GetFileInfo(aPath, &eInfo)) {
    if (eInfo.type == PR_FILE_DIRECTORY) {
      if (PRDir *aDir = PR_OpenDir(aPath)) {
        while (PRDirEntry *anEntry = PR_ReadDir(aDir, PR_SKIP_BOTH)) {
          nsCAutoString path(
            nsDependentCString(aPath) +
            NS_LITERAL_CSTRING("/") +
            nsDependentCString(anEntry->name)
          );
          if (0 != PL_strcasecmp(anEntry->name, "CVS"))
            IteratePath(path.get(), anOutput, aFilecount, aLinecount, anErrcount);
        }
      }
    }
    else if (eInfo.type == PR_FILE_FILE) {
      const char *ext = PL_strcaserstr(aPath, ".cpp");
      if (ext && (ext[4] == 0)) {
        aFilecount++;
        ScanFile(nsDependentCString(aPath),anOutput,aLinecount,anErrcount);
      }
    }
  }
}
 

void ConsumeArguments(int argc, char* argv[]) {

  for(int index=1;index<argc;index++){
    switch(argv[index][0]) {
      case '-':
      case '/':
        switch(argv[index][1]){
          case 'd':
          case 'D': // Starting directory
            gRootDir = argv[index+1];
            index++;
            break;

          case 'h':
          case 'H': // Output HTML with lxr.mozilla.org links
            gEmitHTML=true;
            break;
          case 's':
          case 'S':
            gSloppy=true; // Dont look for member variables
            break;

          default:
            cout << "Usage: dreftool [-h] [-d sourcepath]" << endl;
            cout << "-d path to root of source tree" << endl;
            cout << "-s sloppy mode no warnings if member variables are used unsafe" <<endl;
            cout << "-h emit as HTML" << endl;
            exit(1);
        }
        break;
      default:
        break;
    }
  }
}

int main(int argc, char* argv[]){

  gRootDir = ".";
  ConsumeArguments(argc,argv);

  int fileCount=0;
  int lineCount=0;
  int errCount=0;

#if 0
  nsCAutoString temp("test3.cpp");
  ScanFile(temp,cout,lineCount,errCount);
  fileCount++;
#endif

#if 1
  if(gEmitHTML) {
    cout << "<html><body>" << endl;
  }

  IteratePath(gRootDir,cout,fileCount,lineCount,errCount);
#endif

  if(gEmitHTML) {
    cout << "<PRE>" <<endl;
  }

  cout << endl << "Summary";
  if (gRootDir[0]!='.' || gRootDir[1])
    cout << " for " << gRootDir;
  cout << ": " << endl;
  cout << "===============================" << endl;
  cout<< "Files:  " << fileCount << endl;
  cout<< "Lines:  " << lineCount<< endl;
  cout<< "Errors: " << errCount<< endl;

  if(gEmitHTML) {
    cout << "</PRE>" <<endl;
    cout << "</body></html>" << endl;
  }


  return 0;
}
