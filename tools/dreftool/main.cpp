#include <iostream.h>
#include <fstream.h>
#include "nsString.h"
#include "IDirectory.h"
#include "CScanner.h"
#include "CCPPTokenizer.h"
#include "tokens.h"
#include "CToken.h"
#include <direct.h>

//*******************************************************************************
// Global variables...
//*******************************************************************************
bool  gVerbose=false;
bool  gShowIncludeErrors=false;
bool  gEmitHTML=false;
char  gRootDir[1024];


class IPattern {
public:
  virtual CToken* scan(CCPPTokenizer& aTokenizer,int anIndex,ostream& anOutput)=0;
};


class CPattern {
public:
  CPattern(const nsCString& aFilename) : mNames(0), mFilename(aFilename), mSafeNames(0) {
    mIndex=0;
    mLineNumber=1;
    mErrCount=0;
  }

  CToken* matchID(nsDeque& aDeque,nsCString& aName){
    int theMax=aDeque.GetSize();
    for(int theIndex=0;theIndex<theMax;theIndex++){
      CToken* theToken=(CToken*)aDeque.ObjectAt(theIndex);
      if(theToken){
        nsCString& theName=theToken->getStringValue();
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
  CToken* findSafeIDInIfStatement(CCPPTokenizer& aTokenizer,nsDeque& aNameDeque) {
    CToken* result=0;

    enum eState {eSkipLparen,eFindID};
    eState  theState=eSkipLparen;
    CToken* theToken=0;
    while(theToken=aTokenizer.getTokenAt(++mIndex)) {
      int   theType=theToken->getTokenType();
      nsCString& theString=theToken->getStringValue();
      PRUnichar theChar=theString[0];

      switch(theState) {
        case eSkipLparen:
            //first scan for the lparen...
          switch(theType) {
            case eToken_comment:
            case eToken_whitespace:
            case eToken_newline:
              mLineNumber+=theString.CountChar('\n'); break;

            case eToken_operator:
              switch(theChar) {
                case '(': theState=eFindID; break;
                case ')': return 0; break;
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
                  break;
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
    
    while(theToken=aTokenizer.getTokenAt(++anIndex)){
      if(theToken) {
        nsCString& theString=theToken->getStringValue();
        int     theType=theToken->getTokenType();
        
        anOpChar=theString[0];
        switch(theType){

          case eToken_semicolon:
          case eToken_operator:
            switch(theState) {
              case eFindStar:
                if('*'==anOpChar)
                  theState=eFindType;
                else return 0;
                break;
              case eFindPunct:
                switch(anOpChar) {
                  case '=':
                    if(!allocRequired)
                      return theID;
                    else theState=eFindNew;
                    break;
                  case ';':
                    return (allocRequired) ? 0 : theID;
                    break;
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
                if(theString.Equals("new") || theString.Equals("PR_Malloc"))
                  return theID;
                else return 0;
                break;
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
  bool hasSimpleDeref(nsCString& aName,CCPPTokenizer& aTokenizer,int anIndex) {
    bool result=false;

    int theMin=anIndex-3;
    for(int theIndex=anIndex-1;theIndex>theMin;theIndex--){
      CToken* theToken=aTokenizer.getTokenAt(theIndex);
      if(theToken) {
        nsCString& theString=theToken->getStringValue();
        int     theType=theToken->getTokenType();
        PRUnichar   theChar=theString[0];
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
          nsCString& theString=theToken->getStringValue();
          int     theType=theToken->getTokenType();
          PRUnichar   theChar=theString[0];
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
  bool hasUnconditionedDeref(nsCString& aName,CCPPTokenizer& aTokenizer,int anIndex) {
    bool result=false;

    enum eState {eFailure,eFindIf,eFindDeref};
    CToken* theToken=0;
    eState  theState=eFindDeref;
    int     theIndex=anIndex+1;
    
    while(eFailure!=theState){
      CToken* theToken=aTokenizer.getTokenAt(theIndex);
      if(theToken) {
        nsCString& theString=theToken->getStringValue();
        int     theType=theToken->getTokenType();
        PRUnichar   theChar=theString[0];
        switch(theState){
          case eFindDeref:
              switch(theType) {
                case eToken_whitespace:
                case eToken_comment:
                case eToken_newline:
                  theIndex++;
                  break;
                default:
                  if(0==theString.Compare("->",true,2)) {
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
                if(0==theString.Compare("if",true,2)){
                  return false;
                }
                else if(0==theString.Compare(aName)){
                  return false;
                }
                else if(0==theString.Compare("while",true,5)){
                  return false;
                }
                return true;
              default:
                theIndex--;
            } //switch
            break;
        }//switch
      }//if
      else theState=eFailure;
    }//while

    return result;
  }

  void skipSemi(CCPPTokenizer& aTokenizer) {
    CToken*   theToken=0;
    while(theToken=aTokenizer.getTokenAt(++mIndex)){
      int     theType=theToken->getTokenType();
      nsCString& theString=theToken->getStringValue();
      PRUnichar   theChar=theString[0];

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
    
    PRInt32 pos=mFilename.Find("mozilla");
    const char* buf=mFilename.GetBuffer();
    if(-1<pos) {
      buf+=pos+8;
  
      if(gEmitHTML) {
        anOutput << "<br><a href=\"" ;
      
        anOutput << "http://lxr.mozilla.org/mozilla/source/";
        anOutput << buf << "#" << mLineNumber;

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
    CToken* theMatch=0;

    while(mIndex<theMax){
      CToken*   theToken=aTokenizer.getTokenAt(mIndex);
      int     theType=theToken->getTokenType();
      nsCString& theString=theToken->getStringValue();
      int     theNLCount=theString.CountChar('\n');

      switch(theType){
        case eToken_operator:
          {
            PRUnichar   theChar=theString[0];
            switch(theChar){
              case '{':
                mIndex++;
                scan(aTokenizer,anOutput);
                break;
              case '}':
                //before returning, let's remove the new identifiers...
                while(mNames.GetSize()>theIDCount){
                  CToken* theToken2=(CToken*)mNames.Pop();
                }
                while(mSafeNames.GetSize()>theSafeIDCount){
                  CToken* theToken2=(CToken*)mSafeNames.Pop();
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
            if(theToken=findPtrDecl(aTokenizer,mIndex,theOpChar,true)) {
              //we found ID* ID; (or) ID* ID=...;
              mNames.Push(theToken);
              mIndex+=2;
              skipSemi(aTokenizer);
            }

            else if(0==theString.Compare("if",true,2)) {
              CToken* theSafeID=findSafeIDInIfStatement(aTokenizer,mNames);
              if(theSafeID) {
                mSafeNames.Push(theSafeID);
                theSafeIDCount++;
              }
            }
            else {
              CToken* theMatch=matchID(mNames,theString);
              if(theMatch){
                if((hasUnconditionedDeref(theString,aTokenizer,mIndex)) || 
                   (hasSimpleDeref(theString,aTokenizer,mIndex))){
                  CToken* theSafeID=matchID(mSafeNames,theString);
                  if(theSafeID) {
                    nsCString& s1=theSafeID->getStringValue();
                    if(s1.Equals(theString))
                      break;
                  }
                  //dump the name out in LXR format

                  OutputMozillaFilename(anOutput);
                  nsCAutoString theCStr(theString);


                  anOutput << "   Deref-error: \"" << (const char*)theCStr << "\"" << endl;
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

void ScanFile(nsCString& aFilename,ostream& anOutput,int& aLineCount,int& anErrCount) {
  nsCAutoString theCStr(aFilename);
 
  fstream input((const char*)theCStr,ios::in);
	CScanner theScanner(input);
	CCPPTokenizer theTokenizer;
  theTokenizer.tokenize(theScanner);

  CPattern thePattern(aFilename);
  thePattern.scan(theTokenizer,anOutput);
  aLineCount+=thePattern.mLineNumber;
  anErrCount+=thePattern.mErrCount;
}


void IterateFiles(IDirectory& aDirectory,ostream& anOutput,int& aFilecount,int& aLineCount,int& anErrCount) {

  nsCAutoString thePath("");
  aDirectory.getPath(thePath);

  int theCount=aDirectory.getSize();
  int theIndex=0;
  for(theIndex=0;theIndex<theCount;theIndex++){
    IDirEntry* theEntry=aDirectory.getEntryAt(theIndex);
    nsCString& theName=theEntry->getName();
    if(!dynamic_cast<IDirectory*>(theEntry)){
      nsCAutoString theCopy;
      theName.Right(theCopy,4);
      if(0==theCopy.Compare(".cpp",true,3)) {
        nsCAutoString path(thePath);
        path.Append('/');
        path.Append(theName);
        aFilecount++;
        ScanFile(path,anOutput,aLineCount,anErrCount);
      }
    } else { 
      //we've got a directory on our hands...
      //let's go walk it...
      nsCAutoString path(thePath);
      path.Append("/");
      path.Append(theName);
      if(!theName.EqualsIgnoreCase("cvs")) {
        IDirectory* theDirectory=IDirectory::openDirectory(path.GetBuffer(),"*.*");
        if(theDirectory) {
          IterateFiles(*theDirectory,anOutput,aFilecount,aLineCount,anErrCount);
          IDirectory::closeDirectory(theDirectory);
        }
      }
    }
  }
}


void ConsumeArguments(int argc, char* argv[]) {
  if(argc<2) {
    cout << "Usage: dreftool -h -d sourcepath" << endl;
    cout << "-D path to root of source tree" << endl;
    cout << "-H emit as HTML" << endl;
    exit(1);
  }

  for(int index=1;index<argc;index++){
    switch(argv[index][0]) {
      case '-':
      case '/':
        switch(toupper(argv[index][1])){
          case 'D': //to specify starting directory
            if(('-'!=argv[index+1][0]) && ('/'!=argv[index+1][0])) {
              strcpy(gRootDir,argv[index+1]);
              index++;
            }
            break;

          case 'H': //user chose to show redundant include errors
            gEmitHTML=true;
            break;

          default:
            break;
        }
        break;
      default:
        break;
    }
  }
}

int main(int argc, char* argv[]){

  getcwd(gRootDir,sizeof(gRootDir)-1);
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

  cout << "Scanning " << gRootDir << "..." << endl;
  IDirectory* theDirectory=IDirectory::openDirectory(gRootDir,"*.*");
  if(theDirectory)
    IterateFiles(*theDirectory,cout,fileCount,lineCount,errCount);
#endif

  if(gEmitHTML) {
    cout << "<PRE>" <<endl;
  }

  cout << endl << endl << "Summary: " << endl;
  cout << "===============================" << endl;
  cout<< "Files: " << fileCount << endl;
  cout<< "Lines: " << lineCount<< endl;
  cout<< "Errors: " << errCount<< endl;

  if(gEmitHTML) {
    cout << "</PRE>" <<endl;
    cout << "</body></html>" << endl;
  }


  return 0;
}
 
