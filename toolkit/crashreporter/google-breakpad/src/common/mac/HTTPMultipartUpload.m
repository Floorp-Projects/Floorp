// Copyright (c) 2006, Google Inc.
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

#import "HTTPMultipartUpload.h"
#import "GTMDefines.h"

// As -[NSString stringByAddingPercentEscapesUsingEncoding:] has been
// deprecated with iOS 9.0 / OS X 10.11 SDKs, this function re-implements it
// using -[NSString stringByAddingPercentEncodingWithAllowedCharacters:] when
// using those SDKs.
static NSString *PercentEncodeNSString(NSString *key) {
#if (defined(__IPHONE_OS_VERSION_MIN_REQUIRED) && defined(__IPHONE_9_0) &&     \
     __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_9_0) ||                      \
    (defined(MAC_OS_X_VERSION_MIN_REQUIRED) &&                                 \
     defined(MAC_OS_X_VERSION_10_11) &&                                        \
     MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_11)
  return [key stringByAddingPercentEncodingWithAllowedCharacters:
                  [NSCharacterSet URLQueryAllowedCharacterSet]];
#else
  return [key stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding];
#endif
}

// As -[NSURLConnection sendSynchronousRequest:returningResponse:error:] has
// been deprecated with iOS 9.0 / OS X 10.11 SDKs, this function re-implements
// it using -[NSURLSession dataTaskWithRequest:completionHandler:] which is
// available on iOS 7+.
static NSData *SendSynchronousNSURLRequest(NSURLRequest *req,
                                           NSURLResponse **out_response,
                                           NSError **out_error) {
#if (defined(__IPHONE_OS_VERSION_MIN_REQUIRED) && defined(__IPHONE_7_0) &&     \
     __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_7_0) ||                      \
    (defined(MAC_OS_X_VERSION_MIN_REQUIRED) &&                                 \
     defined(MAC_OS_X_VERSION_10_11) &&                                        \
     MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_11)
  __block NSData* result = nil;
  __block NSError* error = nil;
  __block NSURLResponse* response = nil;
  dispatch_semaphore_t wait_semaphone = dispatch_semaphore_create(0);
  [[[NSURLSession sharedSession]
      dataTaskWithRequest:req
        completionHandler:^(NSData *data,
                            NSURLResponse *resp,
                            NSError *err) {
            if (out_error)
              error = [err retain];
            if (out_response)
              response = [resp retain];
            if (err == nil)
              result = [data retain];
            dispatch_semaphore_signal(wait_semaphone);
  }] resume];
  dispatch_semaphore_wait(wait_semaphone, DISPATCH_TIME_FOREVER);
  dispatch_release(wait_semaphone);
  if (out_error)
    *out_error = [error autorelease];
  if (out_response)
    *out_response = [response autorelease];
  return [result autorelease];
#else
  return [NSURLConnection sendSynchronousRequest:req
                               returningResponse:out_response
                                           error:out_error];
#endif
}
@interface HTTPMultipartUpload(PrivateMethods)
- (NSString *)multipartBoundary;
// Each of the following methods will append the starting multipart boundary,
// but not the ending one.
- (NSData *)formDataForJSON:(NSString *)json;
- (NSData *)formDataForFileContents:(NSData *)contents name:(NSString *)name;
- (NSData *)formDataForFile:(NSString *)file name:(NSString *)name;
@end

@implementation HTTPMultipartUpload
//=============================================================================
#pragma mark -
#pragma mark || Private ||
//=============================================================================
- (NSString *)multipartBoundary {
  // The boundary has 27 '-' characters followed by 16 hex digits
  return [NSString stringWithFormat:@"---------------------------%08X%08X",
    rand(), rand()];
}

//=============================================================================
- (NSData *)formDataForJSON:(NSString *)json {
  NSMutableData *data = [NSMutableData data];
  NSString *fmt = @"--%@\r\nContent-Disposition: form-data; name=\"extra\"; "
                   "filename=\"extra.json\"\r\nContent-Type: application/json\r\n\r\n";
  NSString *form = [NSString stringWithFormat:fmt, boundary_];

  [data appendData:[form dataUsingEncoding:NSUTF8StringEncoding]];
  [data appendData:[json dataUsingEncoding:NSUTF8StringEncoding]];

  return data;
}

//=============================================================================
- (NSData *)formDataForFileContents:(NSData *)contents name:(NSString *)name {
  NSMutableData *data = [NSMutableData data];
  NSString *escaped = PercentEncodeNSString(name);
  NSString *fmt = @"--%@\r\nContent-Disposition: form-data; name=\"%@\"; "
    "filename=\"minidump.dmp\"\r\nContent-Type: application/octet-stream\r\n\r\n";
  NSString *pre = [NSString stringWithFormat:fmt, boundary_, escaped];

  [data appendData:[pre dataUsingEncoding:NSUTF8StringEncoding]];
  [data appendData:contents];

  return data;
}

//=============================================================================
- (NSData *)formDataForFile:(NSString *)file name:(NSString *)name {
  NSData *contents = [NSData dataWithContentsOfFile:file];

  return [self formDataForFileContents:contents name:name];
}

//=============================================================================
#pragma mark -
#pragma mark || Public ||
//=============================================================================
- (id)initWithURL:(NSURL *)url {
  if ((self = [super init])) {
    url_ = [url copy];
    boundary_ = [[self multipartBoundary] retain];
    files_ = [[NSMutableDictionary alloc] init];
  }

  return self;
}

//=============================================================================
- (void)dealloc {
  [url_ release];
  [parameters_ release];
  [files_ release];
  [boundary_ release];
  [response_ release];

  [super dealloc];
}

//=============================================================================
- (NSURL *)URL {
  return url_;
}

//=============================================================================
- (void)setParameters:(NSMutableString *)parameters {
  if (parameters != parameters_) {
    [parameters_ release];
    parameters_ = [parameters mutableCopy];
  }
}

//=============================================================================
- (NSMutableString *)parameters {
  return parameters_;
}

//=============================================================================
- (void)addFileAtPath:(NSString *)path name:(NSString *)name {
  [files_ setObject:path forKey:name];
}

//=============================================================================
- (void)addFileContents:(NSData *)data name:(NSString *)name {
  [files_ setObject:data forKey:name];
}

//=============================================================================
- (NSDictionary *)files {
  return files_;
}

//=============================================================================
- (NSData *)send:(NSError **)error {
  NSMutableURLRequest *req =
    [[NSMutableURLRequest alloc]
          initWithURL:url_ cachePolicy:NSURLRequestUseProtocolCachePolicy
      timeoutInterval:60.0];

  NSMutableData *postBody = [NSMutableData data];

  [req setValue:[NSString stringWithFormat:@"multipart/form-data; boundary=%@",
    boundary_] forHTTPHeaderField:@"Content-type"];

  // Add JSON parameters to the message
  [postBody appendData:[self formDataForJSON:parameters_]];

  // Add any files to the message
  NSArray *fileNames = [files_ allKeys];
  for (NSString *name in fileNames) {
    id fileOrData = [files_ objectForKey:name];
    NSData *fileData;

    // The object can be either the path to a file (NSString) or the contents
    // of the file (NSData).
    if ([fileOrData isKindOfClass:[NSData class]])
      fileData = [self formDataForFileContents:fileOrData name:name];
    else
      fileData = [self formDataForFile:fileOrData name:name];

    [postBody appendData:fileData];
  }

  NSString *epilogue = [NSString stringWithFormat:@"\r\n--%@--\r\n", boundary_];
  [postBody appendData:[epilogue dataUsingEncoding:NSUTF8StringEncoding]];

  [req setHTTPBody:postBody];
  [req setHTTPMethod:@"POST"];

  [response_ release];
  response_ = nil;

  NSData *data = nil;
  if ([[req URL] isFileURL]) {
    [[req HTTPBody] writeToURL:[req URL] options:0 error:error];
  } else {
    NSURLResponse *response = nil;
    data = SendSynchronousNSURLRequest(req, &response, error);
    response_ = (NSHTTPURLResponse *)[response retain];
  }
  [req release];

  return data;
}

//=============================================================================
- (NSHTTPURLResponse *)response {
  return response_;
}

@end
