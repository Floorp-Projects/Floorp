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
 * The Original Code is Network Security Services for Java.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

package org.mozilla.jss.provider.java.security;

import java.security.*;
import java.security.spec.AlgorithmParameterSpec;
import java.security.spec.InvalidParameterSpecException;
import javax.crypto.spec.RC2ParameterSpec;
import java.io.IOException;
import org.mozilla.jss.util.Assert;

/**
 * This class is only intended to be used to implement
 * CipherSpi.getAlgorithmParameters().
 */
public class RC2AlgorithmParameters extends AlgorithmParametersSpi {

    private RC2ParameterSpec RC2ParamSpec;

    public void engineInit(AlgorithmParameterSpec paramSpec) {
        RC2ParamSpec = (RC2ParameterSpec) paramSpec;
    }

    public AlgorithmParameterSpec engineGetParameterSpec(Class clazz)
            throws InvalidParameterSpecException
    {
        if( clazz != null && !clazz.isInstance(RC2ParamSpec) ) {
            Class paramSpecClass = RC2ParamSpec.getClass();
            throw new InvalidParameterSpecException(
                "RC2 getParameterSpec has class " + paramSpecClass.getName());
        }
        return RC2ParamSpec;
    }

    public void engineInit(byte[] params) throws IOException {
        Assert.notReached("engineInit(byte[]) not supported");
        throw new IOException("engineInit(byte[]) not supported");
    }

    public void engineInit(byte[] params, String format) throws IOException {
        Assert.notReached("engineInit(byte[],String) not supported");
        throw new IOException("engineInit(byte[],String) not supported");
    }

    public byte[] engineGetEncoded() throws IOException {
        Assert.notReached("encoding RC2AlgorithmParameters not supported");
        throw new IOException("encoding RC2AlgorithmParameters not supported");
    }

    public byte[] engineGetEncoded(String format) throws IOException {
        Assert.notReached("encoding RC2AlgorithmParameters not supported");
        throw new IOException("encoding RC2AlgorithmParameters not supported");
    }

    public String engineToString() {
        String str = new String("Mozilla-JSS RC2AlgorithmParameters " +  getClass().getName());
        return str;
    }
}
