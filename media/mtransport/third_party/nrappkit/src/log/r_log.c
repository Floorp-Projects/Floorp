/**
   r_log.c


   Copyright (C) 2001, RTFM, Inc.
   Copyright (C) 2006, Network Resonance, Inc.
   All Rights Reserved

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
   3. Neither the name of Network Resonance, Inc. nor the name of any
      contributors to this software may be used to endorse or promote
      products derived from this software without specific prior written
      permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
   POSSIBILITY OF SUCH DAMAGE.


   ekr@rtfm.com  Mon Dec  3 15:24:38 2001
 */


static char *RCSSTRING __UNUSED__ ="$Id: r_log.c,v 1.10 2008/11/25 22:25:18 adamcain Exp $";


#ifdef LINUX
#define _BSD_SOURCE
#define _DEFAULT_SOURCE
#endif

#include "r_log.h"
#include "hex.h"

#include <string.h>
#include <errno.h>
#ifndef _MSC_VER
#include <strings.h>
#include <syslog.h>
#endif
#include <registry.h>
#include <time.h>


#include "nr_common.h"
#include "nr_reg_keys.h"


#define LOGGING_DEFAULT_LEVEL  5

int NR_LOG_LOGGING = 0;

static char *log_level_strings[]={
     "EMERG",
     "ALERT",
     "CRIT",
     "ERR",
     "WARNING",
     "NOTICE",
     "INFO",
     "DEBUG"
};

static char *log_level_reg_strings[]={
     "emergency",
     "alert",
     "critical",
     "error",
     "warning",
     "notice",
     "info",
     "debug"
};

#define LOGGING_REG_PREFIX "logging"

#define MAX_ERROR_STRING_SIZE   512

#define R_LOG_INITTED1   1
#define R_LOG_INITTED2   2

#define LOG_NUM_DESTINATIONS 3

typedef struct log_type_ {
     char *facility_name;
     int level[LOG_NUM_DESTINATIONS];
     NR_registry dest_facility_key[LOG_NUM_DESTINATIONS];
} log_type;

#define MAX_LOG_TYPES 16

static log_type log_types[MAX_LOG_TYPES];
static int log_type_ct;


typedef struct log_destination_ {
     char *dest_name;
     int enabled;
     int default_level;
     r_dest_vlog *dest_vlog;
} log_destination;


#define LOG_LEVEL_UNDEFINED        -1
#define LOG_LEVEL_NONE             -2
#define LOG_LEVEL_USE_DEST_DEFAULT -3

static int stderr_vlog(int facility,int level,const char *format,va_list ap);
static int syslog_vlog(int facility,int level,const char *format,va_list ap);
static int noop_vlog(int facility,int level,const char *format,va_list ap);

static log_destination log_destinations[LOG_NUM_DESTINATIONS]={
     {
          "stderr",
          0,
          LOGGING_DEFAULT_LEVEL,
          stderr_vlog,
     },
     {
          "syslog",
#ifndef WIN32
          1,
#else
          0,
#endif
          LOGGING_DEFAULT_LEVEL,
          syslog_vlog,
     },
     {
          "extra",
          0,
          LOGGING_DEFAULT_LEVEL,
          noop_vlog,
     },
};

static int r_log_level=LOGGING_DEFAULT_LEVEL;
static int r_log_level_environment=0;
static int r_log_initted=0;
static int r_log_env_verbose=0;

static void r_log_facility_change_cb(void *cb_arg, char action, NR_registry name);
static void r_log_facility_delete_cb(void *cb_arg, char action, NR_registry name);
static void r_log_destination_change_cb(void *cb_arg, char action, NR_registry name);
static void r_log_default_level_change_cb(void *cb_arg, char action, NR_registry name);
static int r_log_get_default_level(void);
static int r_log_get_destinations(int usereg);
static int r_logging_dest(int dest_index, int facility, int level);
static int _r_log_init(int usereg);
static int r_log_get_reg_level(NR_registry name, int *level);

int r_log_register(char *facility_name,int *log_facility)
  {
    int i,j;
    int level;
    int r,_status;
    char *buf=0;
    NR_registry dest_prefix, dest_facility_prefix;

    for(i=0;i<log_type_ct;i++){
      if(!strcmp(facility_name,log_types[i].facility_name)){
        *log_facility=i;
        return(0);
      }
    }

    if(log_type_ct==MAX_LOG_TYPES){
      ABORT(R_INTERNAL);
    }

    i=log_type_ct;

    /* Initial registration completed, increment log_type_ct */
    log_types[i].facility_name=r_strdup(facility_name);
    *log_facility=log_type_ct;
    log_type_ct++;

    for(j=0; j<LOG_NUM_DESTINATIONS; j++){
      log_types[i].level[j]=LOG_LEVEL_UNDEFINED;

      if(NR_reg_initted()){

        if((size_t)snprintf(dest_prefix,sizeof(NR_registry),
          "logging.%s.facility",log_destinations[j].dest_name)>=sizeof(NR_registry))
          ABORT(R_INTERNAL);

        if (r=NR_reg_make_registry(dest_prefix,facility_name,dest_facility_prefix))
          ABORT(r);

        if((size_t)snprintf(log_types[i].dest_facility_key[j],sizeof(NR_registry),
          "%s.level",dest_facility_prefix)>=sizeof(NR_registry))
          ABORT(R_INTERNAL);

        if(!r_log_get_reg_level(log_types[i].dest_facility_key[j],&level)){
          log_types[i].level[j]=level;
        }

        /* Set a callback for the facility's level */
        if(r=NR_reg_register_callback(log_types[i].dest_facility_key[j],
          NR_REG_CB_ACTION_ADD|NR_REG_CB_ACTION_CHANGE,
          r_log_facility_change_cb,(void *)&(log_types[i].level[j])))
          ABORT(r);
        if(r=NR_reg_register_callback(log_types[i].dest_facility_key[j],
          NR_REG_CB_ACTION_DELETE,
          r_log_facility_delete_cb,(void *)&(log_types[i].level[j])))
          ABORT(r);

      }
    }

    _status=0;
  abort:
    if(_status)
      RFREE(buf);
    return(_status);
  }

int r_log_facility(int facility,char **typename)
  {
    if(facility >= 0 && facility < log_type_ct){
      *typename=log_types[facility].facility_name;
      return(0);
    }
    return(R_NOT_FOUND);
  }

static int r_log_get_reg_level(NR_registry name, int *out)
  {
    char level[32];
    int r,_status;
    int i;

    if(r=NR_reg_get_string(name,level,sizeof(level)))
      ABORT(r);

    if(!strcasecmp(level,"none")){
      *out=LOG_LEVEL_NONE;
      return(0);
    }

    for(i=0;i<=LOG_DEBUG;i++){
      if(!strcasecmp(level,log_level_reg_strings[i])){
        *out=(int)i;
        return(0);
      }
    }

    if(i>LOG_DEBUG){
      *out=LOG_LEVEL_UNDEFINED;
    }

    _status=0;
  abort:
    return(_status);
  }

/* Handle the case where a value changes */
static void r_log_facility_change_cb(void *cb_arg, char action, NR_registry name)
  {
    int *lt_level=(int *)cb_arg;
    int level;
    int r,_status;

    if(r=r_log_get_reg_level(name,&level))
      ABORT(r);

    *lt_level=level;

    _status=0;
  abort:
    (void)_status; // to avoid unused variable warning and still conform to
                   // pattern of using ABORT
    return;
  }

/* Handle the case where a value is deleted */
static void r_log_facility_delete_cb(void *cb_arg, char action, NR_registry name)
  {
    int *lt_level=(int *)cb_arg;

    *lt_level=LOG_LEVEL_UNDEFINED;
  }

int r_log(int facility,int level,const char *format,...)
  {
    va_list ap;

    va_start(ap,format);

    r_vlog(facility,level,format,ap);
    va_end(ap);

    return(0);
  }

int r_dump(int facility,int level,char *name,char *data,int len)
  {
    char *hex = 0;
    size_t unused;

    if(!r_logging(facility,level))
      return(0);

    hex=RMALLOC((len*2)+1);
    if (!hex)
      return(R_FAILED);

    if (nr_nbin2hex((UCHAR*)data, len, hex, len*2+1, &unused))
      strcpy(hex, "?");

    if(name)
      r_log(facility,level,"%s[%d]=%s",name,len,hex);
    else
      r_log(facility,level,"%s",hex);

    RFREE(hex);
    return(0);
  }

// Some platforms (notably WIN32) do not have this
#ifndef va_copy
  #ifdef WIN32
    #define va_copy(dest, src) ( (dest) = (src) )
  #else  // WIN32
    #error va_copy undefined, and semantics of assignment on va_list unknown
  #endif //WIN32
#endif //va_copy

int r_vlog(int facility,int level,const char *format,va_list ap)
  {
    char log_fmt_buf[MAX_ERROR_STRING_SIZE];
    char *level_str="unknown";
    char *facility_str="unknown";
    char *fmt_str=(char *)format;
    int i;

    if(r_log_env_verbose){
      if((level>=LOG_EMERG) && (level<=LOG_DEBUG))
        level_str=log_level_strings[level];

      if(facility >= 0 && facility < log_type_ct)
        facility_str=log_types[facility].facility_name;

      snprintf(log_fmt_buf, MAX_ERROR_STRING_SIZE, "(%s/%s) %s",
        facility_str,level_str,format);

      log_fmt_buf[MAX_ERROR_STRING_SIZE-1]=0;
      fmt_str=log_fmt_buf;
    }

    for(i=0; i<LOG_NUM_DESTINATIONS; i++){
      if(r_logging_dest(i,facility,level)){
        // Some platforms do not react well when you use a va_list more than
        // once
        va_list copy;
        va_copy(copy, ap);
        log_destinations[i].dest_vlog(facility,level,fmt_str,copy);
        va_end(copy);
      }
    }
    return(0);
  }

int stderr_vlog(int facility,int level,const char *format,va_list ap)
  {
#if 0 /* remove time stamping, for now */
    char cbuf[30];
    time_t tt;

    tt=time(0);

    ctime_r(&tt,cbuf);
    cbuf[strlen(cbuf)-1]=0;

    fprintf(stderr,"%s: ",cbuf);
#endif

    vfprintf(stderr,format,ap);
      fprintf(stderr,"\n");
    return(0);
    }

int syslog_vlog(int facility,int level,const char *format,va_list ap)
  {
#ifndef WIN32
    vsyslog(level|LOG_LOCAL0,format,ap);
#endif
    return(0);
  }

int noop_vlog(int facility,int level,const char *format,va_list ap)
  {
    return(0);
  }

int r_log_e(int facility,int level,const char *format,...)
  {
    va_list ap;

    va_start(ap,format);
    r_vlog_e(facility,level,format,ap);
    va_end(ap);

    return(0);
  }

int r_vlog_e(int facility,int level,const char *format,va_list ap)
  {
    char log_fmt_buf[MAX_ERROR_STRING_SIZE];
    if(r_logging(facility,level)) {
      int formatlen = strlen(format);

      if(formatlen+2 > MAX_ERROR_STRING_SIZE)
        return(1);

      strncpy(log_fmt_buf, format, formatlen);
      strcpy(&log_fmt_buf[formatlen], ": ");
      snprintf(&log_fmt_buf[formatlen+2], MAX_ERROR_STRING_SIZE - formatlen - 2, "%s",
#ifdef WIN32
               strerror(WSAGetLastError()));
#else
               strerror(errno));
#endif
      log_fmt_buf[MAX_ERROR_STRING_SIZE-1]=0;

      r_vlog(facility,level,log_fmt_buf,ap);
    }
    return(0);
  }

int r_log_nr(int facility,int level,int r,const char *format,...)
  {
    va_list ap;

    va_start(ap,format);
    r_vlog_nr(facility,level,r,format,ap);
    va_end(ap);

    return(0);
  }

int r_vlog_nr(int facility,int level,int r,const char *format,va_list ap)
  {
    char log_fmt_buf[MAX_ERROR_STRING_SIZE];
    if(r_logging(facility,level)) {
      int formatlen = strlen(format);

      if(formatlen+2 > MAX_ERROR_STRING_SIZE)
        return(1);
      strncpy(log_fmt_buf, format, formatlen);
      strcpy(&log_fmt_buf[formatlen], ": ");
      snprintf(&log_fmt_buf[formatlen+2], MAX_ERROR_STRING_SIZE - formatlen - 2, "%s",
               nr_strerror(r));

      log_fmt_buf[MAX_ERROR_STRING_SIZE-1]=0;

      r_vlog(facility,level,log_fmt_buf,ap);
    }
    return(0);
  }

static int r_logging_dest(int dest_index, int facility, int level)
  {
    int thresh;

    _r_log_init(0);

    if(!log_destinations[dest_index].enabled)
      return(0);

    if(level <= r_log_level_environment)
      return(1);

    if(r_log_initted<R_LOG_INITTED2)
      return(level<=r_log_level);

    if(facility < 0 || facility > log_type_ct)
      thresh=r_log_level;
    else{
      if(log_types[facility].level[dest_index]==LOG_LEVEL_NONE)
        return(0);

      if(log_types[facility].level[dest_index]>=0)
        thresh=log_types[facility].level[dest_index];
      else if(log_destinations[dest_index].default_level!=LOG_LEVEL_UNDEFINED)
        thresh=log_destinations[dest_index].default_level;
      else
        thresh=r_log_level;
    }

    if(level<=thresh)
      return(1);

    return(0);
  }

int r_logging(int facility, int level)
  {
    int i;

    _r_log_init(0);

    /* return 1 if logging is on for any dest */

    for(i=0; i<LOG_NUM_DESTINATIONS; i++){
      if(r_logging_dest(i,facility,level))
        return(1);
    }

    return(0);
  }


static int r_log_get_default_level(void)
  {
    char *log;
    int _status;

    log=getenv("R_LOG_LEVEL");

    if(log){
      r_log_level=atoi(log);
      r_log_level_environment=atoi(log);
    }
    else{
      r_log_level=LOGGING_DEFAULT_LEVEL;
    }

    _status=0;
  //abort:
    return(_status);
  }


static int r_log_get_destinations(int usereg)
  {
    char *log;
    int i;
    int r,_status;

    log=getenv("R_LOG_DESTINATION");
    if(log){
      for(i=0; i<LOG_NUM_DESTINATIONS; i++)
        log_destinations[i].enabled=!strcmp(log,log_destinations[i].dest_name);
    }
    else if(usereg){
      NR_registry reg_key;
      int i;
      int value;
      char c;

      /* Get the data out of the registry */
      for(i=0; i<LOG_NUM_DESTINATIONS; i++){
        /* set callback for default level */
        if((size_t)snprintf(reg_key,sizeof(reg_key),"%s.%s.level",LOGGING_REG_PREFIX,
          log_destinations[i].dest_name)>=sizeof(reg_key))
          ABORT(R_INTERNAL);

        NR_reg_register_callback(reg_key,
          NR_REG_CB_ACTION_ADD|NR_REG_CB_ACTION_CHANGE|NR_REG_CB_ACTION_DELETE,
          r_log_default_level_change_cb,0);

        if(r=r_log_get_reg_level(reg_key,&value)){
          if(r==R_NOT_FOUND)
            log_destinations[i].default_level=LOG_LEVEL_UNDEFINED;
          else
            ABORT(R_INTERNAL);
        }
        else
          log_destinations[i].default_level=value;

        /* set callback for the enabled key for this logging dest */
        if((size_t)snprintf(reg_key,sizeof(reg_key),"%s.%s.enabled",LOGGING_REG_PREFIX,
          log_destinations[i].dest_name)>=sizeof(reg_key))
          ABORT(R_INTERNAL);

        NR_reg_register_callback(reg_key,
          NR_REG_CB_ACTION_ADD|NR_REG_CB_ACTION_CHANGE|NR_REG_CB_ACTION_DELETE,
          r_log_destination_change_cb,0);

        if(r=NR_reg_get_char(reg_key,&c)){
          if(r==R_NOT_FOUND)
            log_destinations[i].enabled=0;
          else
            ABORT(r);
        }
        else
          log_destinations[i].enabled=c;
      }
    }

    _status=0;
  abort:
    return(_status);
  }

static void r_log_destination_change_cb(void *cb_arg, char action, NR_registry name)
  {
    r_log_get_destinations(1);
  }

static void r_log_default_level_change_cb(void *cb_arg, char action, NR_registry name)
  {
    r_log_get_destinations(1);
  }


int r_log_init()
  {
    _r_log_init(1);

    return 0;
  }

int _r_log_init(int use_reg)
  {
#ifndef WIN32
    char *log;
#endif

    if(!use_reg){
      if(r_log_initted<R_LOG_INITTED1){
        r_log_get_default_level();
        r_log_get_destinations(0);

        r_log_initted=R_LOG_INITTED1;
      }
    }
    else{
      if(r_log_initted<R_LOG_INITTED2){
        int facility;

        r_log_get_default_level();
        r_log_get_destinations(1);

        r_log_register("generic",&facility);
        r_log_register("logging",&NR_LOG_LOGGING);

        r_log_initted=R_LOG_INITTED2;
      }
    }

#ifdef WIN32
    r_log_env_verbose=1;
#else
    log=getenv("R_LOG_VERBOSE");
    if(log)
      r_log_env_verbose=atoi(log);
#endif

    return(0);
  }

int r_log_set_extra_destination(int default_level, r_dest_vlog *dest_vlog)
  {
    int i;
    log_destination *dest = 0;

    for(i=0; i<LOG_NUM_DESTINATIONS; i++){
      if(!strcmp("extra",log_destinations[i].dest_name)){
        dest=&log_destinations[i];
        break;
      }
    }

    if(!dest)
      return(R_INTERNAL);

    if (dest_vlog==0){
      dest->enabled=0;
      dest->dest_vlog=noop_vlog;
    }
    else{
      dest->enabled=1;
      dest->default_level=default_level;
      dest->dest_vlog=dest_vlog;
    }

    return(0);
  }

