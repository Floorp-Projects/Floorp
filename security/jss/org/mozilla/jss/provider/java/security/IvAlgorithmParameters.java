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
import javax.crypto.spec.IvParameterSpec;
import java.io.IOException;
import org.mozilla.jss.util.Assert;

/**
 * This class is only intended to be used to implement
 * CipherSpi.getAlgorithmParameters().
 */
public class IvAlgorithmParameters extends AlgorithmParametersSpi {

    private IvParameterSpec ivParamSpec;

    public void engineInit(AlgorithmParameterSpec paramSpec) {
        ivParamSpec = (IvParameterSpec) paramSpec;
    }

    public AlgorithmParameterSpec engineGetParameterSpec(Class clazz)
            throws InvalidParameterSpecException
    {
        if( clazz != null && !(clazz.isInstance(ivParamSpec)) ) {
            Class paramSpecClass = ivParamSpec.getClass();
            throw new InvalidParameterSpecException(
                "Parameter spec has class " + paramSpecClass.getName());
        }
        return ivParamSpec;
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
        Assert.notReached("encoding IvAlgorithmParameters not supported");
        throw new IOException("encoding IvAlgorithmParameters not supported");
    }

    public byte[] engineGetEncoded(String format) throws IOException {
        Assert.notReached("encoding IvAlgorithmParameters not supported");
        throw new IOException("encoding IvAlgorithmParameters not supported");
    }

    public String engineToString() {
        Assert.notReached("engineToString() not supported");
        return getClass().getName();
    }
}
