#include <stdio.h>
#include <stream.h>
#include <fstream.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "proto.h"

/************************************************
 * Retrieves the server's process id
 * from the data file.
 ************************************************/

void getProcessId(char *fname, char *s)
{
    char ch;
    short i = 0;

    ifstream f_in(fname);
    if (!f_in) {
       cerr << "Cannot open file %s.\n" << fname;
       exit(1);
    }
    while(1){
       if (f_in.eof()) break;
       f_in >> ch;
       s[i++] = ch;
    }
    s[i-1] = '\0';
    f_in.close();
}

/***********************************************
 * Saves the server's process id
 * in to the data file.
 ***********************************************/

void setProcessId(char *fname)
{
    int pid = getpid();
    cout << "%x\n" << pid;
    ofstream f_out(fname);
    if(!f_out) {
       cerr << "Cannot open file" << fname;
       exit(1);
    }
    f_out << form("%x",pid);
    f_out.close();
}

/***********************************************
 * Writes the string into the result data 
 * file by appending it into the existing data.
 ***********************************************/

void writeResult(char *fname, char *str) {

   char **buf = new char *[500];
   int line = readFromFile(fname, buf);
   int len = strlen(str);
   buf[line] = new char[len];
   strcpy(buf[line], str);
   writeToFile(fname, line + 1, buf);
   for(int i = 0; i < line; i++){
      delete buf[i];
   }
   delete buf;
}

/***********************************************
 * Reads the content of the result data file. 
 *
 ***********************************************/

int readFromFile(char *fname, char **buf) {


   int i = 0;
   ifstream f_in(fname);
   if (!f_in) {
      return i;
   }
   char *tmpbuf = new char[400];
   char ch;
   int j = 0;
   while(!f_in.eof()) {
      f_in.get(ch);
      tmpbuf[j++] = ch;
      if (ch == '\n') {
        tmpbuf[j] = '\0';
        buf[i] = new char[strlen(tmpbuf)];
        strcpy(buf[i++], tmpbuf);
        j = 0;
      }
   }
   delete tmpbuf;
   f_in.close();
   return i;
}

/***********************************************
 * Writes the content of the buffer to the
 * of the result data file. 

 ***********************************************/
void writeToFile(char *fname, int numLine, char **buf) {

   ofstream f_out(fname);
   if (!f_out) {
      cerr << "Error in reading file %s.\n" << fname;
      exit(1);
   }
   int i;
   for(i = 0; i < numLine; i++) {
      f_out << buf[i];
   }
   f_out.close();
}

