/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_TOOLS_SIMPLE_COMMAND_LINE_PARSER_H_
#define WEBRTC_TOOLS_SIMPLE_COMMAND_LINE_PARSER_H_

#include <string>
#include <map>
#include <vector>

// This is a very basic command line parsing class. We pass the command line
// arguments and their number and the class forms a vector out of these. Than we
// should set up the flags - we provide a name and a string value and map these.

namespace webrtc {
namespace test {

class CommandLineParser {
 public:
  CommandLineParser() {}
  ~CommandLineParser() {}

  void Init(int argc, char** argv);

  // Prints the entered flags and their values (without --help).
  void PrintEnteredFlags();

  // Processes the vector of command line arguments and puts the value of each
  // flag in the corresponding map entry for this flag's name. We don't process
  // flags which haven't been defined in the map.
  void ProcessFlags();

  // Sets the usage message to be shown if we pass --help.
  void SetUsageMessage(std::string usage_message);

  // prints the usage message.
  void PrintUsageMessage();

  // Set a flag into the map of flag names/values.
  void SetFlag(std::string flag_name, std::string flag_value);

  // Gets a flag when provided a flag name. Returns "" if the flag is unknown.
  std::string GetFlag(std::string flag_name);

 private:
  // The vector of passed command line arguments.
  std::vector<std::string> args_;
  // The map of the flag names/values.
  std::map<std::string, std::string> flags_;
  // The usage message.
  std::string usage_message_;

  // Returns whether the passed flag is standalone or not. By standalone we
  // understand e.g. --standalone (in contrast to --non_standalone=1).
  bool IsStandaloneFlag(std::string flag);

  // Checks weather the flag is in the format --flag_name=flag_value.
  bool IsFlagWellFormed(std::string flag);

  // Extracts the flag name from the flag.
  std::string GetCommandLineFlagName(std::string flag);

  // Extracts the falg value from the flag.
  std::string GetCommandLineFlagValue(std::string flag);
};

}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_TOOLS_SIMPLE_COMMAND_LINE_PARSER_H_
