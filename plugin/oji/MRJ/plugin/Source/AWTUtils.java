/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

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
