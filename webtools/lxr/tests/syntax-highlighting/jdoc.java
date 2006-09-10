/* ***** BEGIN LICENSE BLOCK *****
 * ***** END LICENSE BLOCK ***** */

package org.bugzilla.landfill.mxr;

import java.io.*;


/**
 * <p>
 * This interface is similar to <code>nsISupports</code>.
 * </p>
 *
 * @see Mozilla#initEmbedding
 * @see Mozilla#initXPCOM
 * @see <a href=
 *     "http://lxr.mozilla.org/mozilla/source/xpcom/io/nsISupports.idl">
 *      nsISupports </a>
 */
public interface IMXRTestSupports {

  /**
   * Blah
   *
   * @param interface_id the interface
   * @param out_result this is the result
   *
   * The following is gibberish for testing mxr's cross reference
   *                    <ul>
   *                      <li><code>true</code> - ...
   *                      </li>
   *                      <li><code>false</code> - ... </li>
   *                    </ul>
   */
  void QueryInterface(nsid interface_id, [out,QIResult] out_result);
}


