// Mozilla has modified this file - see https://hg.mozilla.org/ for details.
/*
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 * 
 *      http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.mozilla.apache.commons.codec;

/**
 * Defines common encoding methods for String encoders.
 *
 * @author Apache Software Foundation
 * @version $Id: StringEncoder.java 1080701 2011-03-11 17:52:27Z ggregory $
 */
public interface StringEncoder extends Encoder {
    
    /**
     * Encodes a String and returns a String.
     * 
     * @param source the String to encode
     * 
     * @return the encoded String
     * 
     * @throws EncoderException thrown if there is
     *  an error conidition during the Encoding process.
     */
    String encode(String source) throws EncoderException;
}  

