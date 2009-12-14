// Copyright (c) 2007, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Utility that can inspect another process and write a crash dump

#include <cstdio>
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <string>
#include <sys/time.h>

#import "client/mac/crash_generation/Inspector.h"

#import "client/mac/Framework/Breakpad.h"
#import "client/mac/handler/minidump_generator.h"

#import "common/mac/SimpleStringDictionary.h"
#import "common/mac/MachIPC.h"

#import <Foundation/Foundation.h>

#if VERBOSE
  bool gDebugLog = true;
#else
  bool gDebugLog = false;
#endif

namespace google_breakpad {

//=============================================================================
static BOOL EnsureDirectoryPathExists(NSString *dirPath) {
  NSFileManager *mgr = [NSFileManager defaultManager];

  // If we got a relative path, prepend the current directory
  if (![dirPath isAbsolutePath])
    dirPath = [[mgr currentDirectoryPath] stringByAppendingPathComponent:dirPath];

  NSString *path = dirPath;

  // Ensure that no file exists within the path which would block creation
  while (1) {
    BOOL isDir;
    if ([mgr fileExistsAtPath:path isDirectory:&isDir]) {
      if (isDir)
        break;

      return NO;
    }

    path = [path stringByDeletingLastPathComponent];
  }

  // Path now contains the first valid directory (or is empty)
  if (![path length])
    return NO;

  NSString *common =
    [dirPath commonPrefixWithString:path options:NSLiteralSearch];

  // If everything is good
  if ([common isEqualToString:dirPath])
    return YES;

  // Break up the difference into components
  NSString *diff = [dirPath substringFromIndex:[common length] + 1];
  NSArray *components = [diff pathComponents];
  unsigned count = [components count];

  // Rebuild the path one component at a time
  NSDictionary *attrs =
    [NSDictionary dictionaryWithObject:[NSNumber numberWithUnsignedLong:0750]
                                forKey:NSFilePosixPermissions];
  path = common;
  for (unsigned i = 0; i < count; ++i) {
    path = [path stringByAppendingPathComponent:[components objectAtIndex:i]];

    if (![mgr createDirectoryAtPath:path attributes:attrs])
      return NO;
  }

  return YES;
}

//=============================================================================
BOOL ConfigFile::WriteData(const void *data, size_t length) {
  size_t result = write(config_file_, data, length);

  return result == length;
}

//=============================================================================
BOOL ConfigFile::AppendConfigData(const char *key,
                                  const void *data, size_t length) {
  assert(config_file_ != -1);

  if (!key) {
    DEBUGLOG(stderr, "Breakpad: Missing Key\n");
    return NO;
  }

  if (!data) {
    DEBUGLOG(stderr, "Breakpad: Missing data for key: %s\n", key ? key :
            "<Unknown Key>");
    return NO;
  }

  // Write the key, \n, length of data (ascii integer), \n, data
  char buffer[16];
  char nl = '\n';
  BOOL result = WriteData(key, strlen(key));

  snprintf(buffer, sizeof(buffer) - 1, "\n%lu\n", length);
  result &= WriteData(buffer, strlen(buffer));
  result &= WriteData(data, length);
  result &= WriteData(&nl, 1);
  return result;
}

//=============================================================================
BOOL ConfigFile::AppendConfigString(const char *key,
                                    const char *value) {
  return AppendConfigData(key, value, strlen(value));
}

//=============================================================================
void ConfigFile::WriteFile(const SimpleStringDictionary *configurationParameters,
                           const char *dump_dir,
                           const char *minidump_id) {

  assert(config_file_ == -1);

  // Open and write out configuration file preamble
  strlcpy(config_file_path_, "/tmp/Config-XXXXXX",
          sizeof(config_file_path_));
  config_file_ = mkstemp(config_file_path_);

  if (config_file_ == -1) {
    DEBUGLOG(stderr,
             "mkstemp(config_file_path_) == -1 (%s)\n",
             strerror(errno));
    return;
  }
  else {
    DEBUGLOG(stderr, "Writing config file to (%s)\n", config_file_path_);
  }

  has_created_file_ = true;

  // Add the minidump dir
  AppendConfigString(kReporterMinidumpDirectoryKey, dump_dir);
  AppendConfigString(kReporterMinidumpIDKey, minidump_id);

  // Write out the configuration parameters
  BOOL result = YES;
  const SimpleStringDictionary &dictionary = *configurationParameters;

  const KeyValueEntry *entry = NULL;
  SimpleStringDictionaryIterator iter(dictionary);

  while ((entry = iter.Next())) {
    DEBUGLOG(stderr,
             "config: (%s) -> (%s)\n",
             entry->GetKey(),
             entry->GetValue());
    result = AppendConfigString(entry->GetKey(), entry->GetValue());

    if (!result)
      break;
  }

  close(config_file_);
  config_file_ = -1;
}

//=============================================================================
void Inspector::Inspect(const char *receive_port_name) {
  kern_return_t result = ServiceCheckIn(receive_port_name);

  if (result == KERN_SUCCESS) {
    result = ReadMessages();

    if (result == KERN_SUCCESS) {
      // Inspect the task and write a minidump file.
      bool wrote_minidump = InspectTask();

      // Send acknowledgement to the crashed process that the inspection
      // has finished.  It will then be able to cleanly exit.
      // The return value is ignored because failure isn't fatal. If the process
      // didn't get the message there's nothing we can do, and we still want to
      // send the report.
      SendAcknowledgement();

      if (wrote_minidump) {
        // Ask the user if he wants to upload the crash report to a server,
        // and do so if he agrees.
        LaunchReporter(config_file_.GetFilePath());
      } else {
        fprintf(stderr, "Inspection of crashed process failed\n");
      }

      // Now that we're done reading messages, cleanup the service, but only
      // if there was an actual exception
      // Otherwise, it means the dump was generated on demand and the process
      // lives on, and we might be needed again in the future.
      if (exception_code_) {
        ServiceCheckOut(receive_port_name);
      }
    } else {
        PRINT_MACH_RESULT(result, "Inspector: WaitForMessage()");
    }
  }
}

//=============================================================================
kern_return_t Inspector::ServiceCheckIn(const char *receive_port_name) {
  // We need to get the mach port representing this service, so we can
  // get information from the crashed process.
  kern_return_t kr = bootstrap_check_in(bootstrap_port,
                                        (char*)receive_port_name,
                                        &service_rcv_port_);

  if (kr != KERN_SUCCESS) {
#if VERBOSE
    PRINT_MACH_RESULT(kr, "Inspector: bootstrap_check_in()");
#endif
  }

  return kr;
}

//=============================================================================
kern_return_t Inspector::ServiceCheckOut(const char *receive_port_name) {
  // We're done receiving mach messages from the crashed process,
  // so clean up a bit.
  kern_return_t kr;

  // DO NOT use mach_port_deallocate() here -- it will fail and the
  // following bootstrap_register() will also fail leaving our service
  // name hanging around forever (until reboot)
  kr = mach_port_destroy(mach_task_self(), service_rcv_port_);

  if (kr != KERN_SUCCESS) {
    PRINT_MACH_RESULT(kr,
      "Inspector: UNREGISTERING: service_rcv_port mach_port_deallocate()");
    return kr;
  }

  // Unregister the service associated with the receive port.
  kr = bootstrap_register(bootstrap_port,
                          (char*)receive_port_name,
                          MACH_PORT_NULL);

  if (kr != KERN_SUCCESS) {
    PRINT_MACH_RESULT(kr, "Inspector: UNREGISTERING: bootstrap_register()");
  }

  return kr;
}

//=============================================================================
kern_return_t Inspector::ReadMessages() {
  // Wait for an initial message from the crashed process containing basic
  // information about the crash.
  ReceivePort receive_port(service_rcv_port_);

  MachReceiveMessage message;
  kern_return_t result = receive_port.WaitForMessage(&message, 1000);

  if (result == KERN_SUCCESS) {
    InspectorInfo &info = (InspectorInfo &)*message.GetData();
    exception_type_ = info.exception_type;
    exception_code_ = info.exception_code;
    exception_subcode_ = info.exception_subcode;

#if VERBOSE
    printf("message ID = %d\n", message.GetMessageID());
#endif

    remote_task_ = message.GetTranslatedPort(0);
    crashing_thread_ = message.GetTranslatedPort(1);
    handler_thread_ = message.GetTranslatedPort(2);
    ack_port_ = message.GetTranslatedPort(3);

#if VERBOSE
    printf("exception_type = %d\n", exception_type_);
    printf("exception_code = %d\n", exception_code_);
    printf("exception_subcode = %d\n", exception_subcode_);
    printf("remote_task = %d\n", remote_task_);
    printf("crashing_thread = %d\n", crashing_thread_);
    printf("handler_thread = %d\n", handler_thread_);
    printf("ack_port_ = %d\n", ack_port_);
    printf("parameter count = %d\n", info.parameter_count);
#endif

    // In certain situations where multiple crash requests come
    // through quickly, we can end up with the mach IPC messages not
    // coming through correctly.  Since we don't know what parameters
    // we've missed, we can't do much besides abort the crash dump
    // situation in this case.
    unsigned int parameters_read = 0;
    // The initial message contains the number of key value pairs that
    // we are expected to read.
    // Read each key/value pair, one mach message per key/value pair.
    for (unsigned int i = 0; i < info.parameter_count; ++i) {
      MachReceiveMessage message;
      result = receive_port.WaitForMessage(&message, 1000);

      if(result == KERN_SUCCESS) {
        KeyValueMessageData &key_value_data =
          (KeyValueMessageData&)*message.GetData();
        // If we get a blank key, make sure we don't increment the
        // parameter count; in some cases (notably on-demand generation
        // many times in a short period of time) caused the Mach IPC
        // messages to not come through correctly.
        if (strlen(key_value_data.key) == 0) {
          continue;
        }
        parameters_read++;

        config_params_.SetKeyValue(key_value_data.key, key_value_data.value);
      } else {
        PRINT_MACH_RESULT(result, "Inspector: key/value message");
        break;
      }
    }
    if (parameters_read != info.parameter_count) {
      DEBUGLOG(stderr, "Only read %d parameters instead of %d, aborting crash "
               "dump generation.", parameters_read, info.parameter_count);
      return KERN_FAILURE;
    }
  }

  return result;
}

//=============================================================================
// Sets keys in the parameters dictionary that are specific to process uptime.
// The two we set are process up time, and process crash time.
void Inspector::SetCrashTimeParameters() {
  // Set process uptime parameter
  struct timeval tv;
  gettimeofday(&tv, NULL);

  char processUptimeString[32], processCrashtimeString[32];
  const char *processStartTimeString =
    config_params_.GetValueForKey(BREAKPAD_PROCESS_START_TIME);

  // Set up time if we've received the start time.
  if (processStartTimeString) {
    time_t processStartTime = strtol(processStartTimeString, NULL, 10);
    time_t processUptime = tv.tv_sec - processStartTime;
    sprintf(processUptimeString, "%d", processUptime);
    config_params_.SetKeyValue(BREAKPAD_PROCESS_UP_TIME, processUptimeString);
  }

  sprintf(processCrashtimeString, "%d", tv.tv_sec);
  config_params_.SetKeyValue(BREAKPAD_PROCESS_CRASH_TIME,
                             processCrashtimeString);
}

bool Inspector::InspectTask() {
  // keep the task quiet while we're looking at it
  task_suspend(remote_task_);
  DEBUGLOG(stderr, "Suspended Remote task\n");

  NSString *minidumpDir;

  const char *minidumpDirectory =
    config_params_.GetValueForKey(BREAKPAD_DUMP_DIRECTORY);

  SetCrashTimeParameters();
  // If the client app has not specified a minidump directory,
  // use a default of Library/<kDefaultLibrarySubdirectory>/<Product Name>
  if (!minidumpDirectory || 0 == strlen(minidumpDirectory)) {
    NSArray *libraryDirectories =
      NSSearchPathForDirectoriesInDomains(NSLibraryDirectory,
                                          NSUserDomainMask,
                                          YES);

    NSString *applicationSupportDirectory =
        [libraryDirectories objectAtIndex:0];
    NSString *library_subdirectory = [NSString 
        stringWithUTF8String:kDefaultLibrarySubdirectory];
    NSString *breakpad_product = [NSString 
        stringWithUTF8String:config_params_.GetValueForKey(BREAKPAD_PRODUCT)];
        
    NSArray *path_components = [NSArray
        arrayWithObjects:applicationSupportDirectory,
                         library_subdirectory,
                         breakpad_product,
                         nil];

    minidumpDir = [NSString pathWithComponents:path_components];
  } else {
    minidumpDir = [[NSString stringWithUTF8String:minidumpDirectory]
                    stringByExpandingTildeInPath];
  }
  DEBUGLOG(stderr, 
           "Writing minidump to directory (%s)\n",
           [minidumpDir UTF8String]);

  MinidumpLocation minidumpLocation(minidumpDir);

  // Obscure bug alert:
  // Don't use [NSString stringWithFormat] to build up the path here since it
  // assumes system encoding and in RTL locales will prepend an LTR override
  // character for paths beginning with '/' which fileSystemRepresentation does
  // not remove. Filed as rdar://6889706 .
  NSString *path_ns = [NSString
      stringWithUTF8String:minidumpLocation.GetPath()];
  NSString *pathid_ns = [NSString
      stringWithUTF8String:minidumpLocation.GetID()];
  NSString *minidumpPath = [path_ns stringByAppendingPathComponent:pathid_ns];
  minidumpPath = [minidumpPath 
      stringByAppendingPathExtension:@"dmp"];
  
  DEBUGLOG(stderr, 
           "minidump path (%s)\n",
           [minidumpPath UTF8String]);


  config_file_.WriteFile( &config_params_,
                          minidumpLocation.GetPath(),
                          minidumpLocation.GetID());


  MinidumpGenerator generator(remote_task_, handler_thread_);

  if (exception_type_ && exception_code_) {
    generator.SetExceptionInformation(exception_type_,
                                      exception_code_,
                                      exception_subcode_,
                                      crashing_thread_);
  }


  bool result = generator.Write([minidumpPath fileSystemRepresentation]);

  if (result) {
    DEBUGLOG(stderr, "Wrote minidump - OK\n");
  } else {
    DEBUGLOG(stderr, "Error writing minidump - errno=%s\n",  strerror(errno));
  }

  // let the task continue
  task_resume(remote_task_);
  DEBUGLOG(stderr, "Resumed remote task\n");

  return result;
}

//=============================================================================
// The crashed task needs to be told that the inspection has finished.
// It will wait on a mach port (with timeout) until we send acknowledgement.
kern_return_t Inspector::SendAcknowledgement() {
  if (ack_port_ != MACH_PORT_DEAD) {
    MachPortSender sender(ack_port_);
    MachSendMessage ack_message(kMsgType_InspectorAcknowledgement);

    DEBUGLOG(stderr, "Inspector: trying to send acknowledgement to port %d\n",
      ack_port_);

    kern_return_t result = sender.SendMessage(ack_message, 2000);

#if VERBOSE
    PRINT_MACH_RESULT(result, "Inspector: sent acknowledgement");
#endif

    return result;
  }

  DEBUGLOG(stderr, "Inspector: port translation failure!\n");
  return KERN_INVALID_NAME;
}

//=============================================================================
void Inspector::LaunchReporter(const char *inConfigFilePath) {
  // Extract the path to the reporter executable.
  const char *reporterExecutablePath =
          config_params_.GetValueForKey(BREAKPAD_REPORTER_EXE_LOCATION);
  DEBUGLOG(stderr, "reporter path = %s\n", reporterExecutablePath);

  // Setup and launch the crash dump sender.
  const char *argv[3];
  argv[0] = reporterExecutablePath;
  argv[1] = inConfigFilePath;
  argv[2] = NULL;

  // Launch the reporter
  pid_t pid = fork();

  // If we're in the child, load in our new executable and run.
  // The parent will not wait for the child to complete.
  if (pid == 0) {
    execv(argv[0], (char * const *)argv);
    config_file_.Unlink();  // launch failed - get rid of config file
    DEBUGLOG(stderr, "Inspector: unable to launch reporter app\n");
    _exit(1);
  }

  // Wait until the Reporter child process exits.
  //

  // We'll use a timeout of one minute.
  int timeoutCount = 60;   // 60 seconds

  while (timeoutCount-- > 0) {
    int status;
    pid_t result = waitpid(pid, &status, WNOHANG);

    if (result == 0) {
      // The child has not yet finished.
      sleep(1);
    } else if (result == -1) {
      DEBUGLOG(stderr, "Inspector: waitpid error (%d) waiting for reporter app\n",
        errno);
      break;
    } else {
      // child has finished
      break;
    }
  }
}

} // namespace google_breakpad

