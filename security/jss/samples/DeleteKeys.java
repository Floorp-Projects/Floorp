/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape Security Services for Java.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */


/** This class will delete all the private keys from your
 *  Key3.db - please make sure you know what you are doing
 *  before you run this.
 */

import com.netscape.jss.CryptoManager;
import com.netscape.jss.util.ConsolePasswordCallback;
import com.netscape.jss.util.Debug;
import com.netscape.jss.crypto.*;
import java.math.BigInteger;

public class DeleteKeys {

    public static void main(String args[]) {
        int i;
        if(args.length!=1) {
            System.out.println("Usage: java DeleteKeys <dbdir>");
            return;
        }

        try {

        CryptoManager.InitializationValues vals = 
            new CryptoManager.InitializationValues(
                                  args[0]+"/secmodule.db",
                                  args[0]+"/key3.db",
                                  args[0]+"/cert7.db");
        CryptoManager.initialize(vals);
        CryptoManager manager = CryptoManager.getInstance();
        CryptoStore store;

        CryptoToken token = manager.getInternalKeyStorageToken();

        // Generate a few keys
        KeyPairGenerator gen =
            token.getKeyPairGenerator(KeyPairAlgorithm.RSA);
        gen.initialize(512);
        gen.genKeyPair();
        gen.genKeyPair();


        store = token.getCryptoStore();
        PrivateKey[] keys = store.getPrivateKeys();
        System.out.println(keys.length+" Keys:");
        for(i=0; i < keys.length; i++) {
			PrivateKey key = (PrivateKey)keys[i];
            byte[] keyID = key.getUniqueID();
            if(keyID.length!=20) {
                System.out.println("ERROR: keyID length != 20");
            }
            BigInteger bigint = new BigInteger(keyID);
            System.out.println("UniqueID: "+ bigint);
        }

        System.out.println("Now we shall delete all the keys!"+
            " Hope you backed up...");
        keys = store.getPrivateKeys();
        for(i=0; i < keys.length; i ++) {
            PrivateKey key = (PrivateKey)keys[i];
            store.deletePrivateKey(key);
        }

        System.out.println("Now here's all the keys:");
        keys = store.getPrivateKeys();
        for(i=0; i < keys.length; i ++) {
			PrivateKey key = (PrivateKey)keys[i];
            byte[] keyID = key.getUniqueID();
            if(keyID.length!=20) {
                System.out.println("ERROR: keyID length != 20");
            }
            BigInteger bigint = new BigInteger(keyID);
            System.out.println("UniqueID: "+ bigint);
        }

        System.out.println("done.");

        } catch( Exception e) {
            e.printStackTrace();
        }
    }
}
