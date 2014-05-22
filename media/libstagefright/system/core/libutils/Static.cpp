/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// All static variables go here, to control initialization and
// destruction order in the library.

namespace stagefright {

// For String8.cpp
extern void initialize_string8();
extern void terminate_string8();

// For String16.cpp
extern void initialize_string16();
extern void terminate_string16();

class LibUtilsFirstStatics
{
public:
    LibUtilsFirstStatics()
    {
        initialize_string8();
        initialize_string16();
    }
    
    ~LibUtilsFirstStatics()
    {
        terminate_string16();
        terminate_string8();
    }
};

static LibUtilsFirstStatics gFirstStatics;
int gDarwinCantLoadAllObjects = 1;

}   // namespace stagefright
