/* ----- BEGIN LICENSE BLOCK -----
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
 * The Original Code is the MRJ Carbon OJI Plugin.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):  Patrick C. Beard <beard@netscape.com>
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
 * ----- END LICENSE BLOCK ----- */

/*
	AWTUtils.java
 */

package netscape.oji;


import java.awt.Container;
import java.awt.Graphics;
import java.awt.Point;

import com.apple.mrj.internal.awt.PrintingPort;

public class AWTUtils {
	/**
	 * Prints the components of a specified Container.
	 */
	public static void printContainer(Container container, int printingPort, int originX, int originY, Object notifier) {
		try {
			// obtain a graphics object to draw with.
			PrintingPort printer = new PrintingPort(printingPort, originX, originY);
			Graphics graphics = printer.getGraphics(container);

			// print the specified container.
			container.printAll(graphics);
			
			graphics.dispose();
			printer.dispose();
		} finally {
			// if caller is waiting for this to complete, then notify.
			if (notifier != null) {
				synchronized(notifier) {
					notifier.notifyAll();
				}
			}
		}
	}
}
