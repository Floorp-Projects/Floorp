/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*
	PrintingPort.java
	
	Wraps a Quickdraw printing port and provides a way to image on it
	with a java.awt.Graphics object.  
	
	by Patrick C. Beard.
 */

package com.apple.mrj.internal.awt;

import com.apple.mrj.macos.generated.RectStruct;
import com.apple.mrj.macos.generated.QuickdrawFunctions;
import java.awt.*;

public class PrintingPort implements GraphicsHost {
	private int mPrintingPort;
	private int mOriginX;
	private int mOriginY;
	private PortManager mPortManager;
	private QDPipeline mPipeline;
	private Rectangle mClipping;

	public PrintingPort(int printingPort, int originX, int originY) {
		mPrintingPort = printingPort;
		mOriginX = originX;
		mOriginY = originY;
		mPortManager = new PortManager(printingPort, 0, 0, PortManager.kPrinting, 0);
		mPipeline = new QDPipeline();
		mPortManager.setPipeline(mPipeline);
	}
	
	public void dispose() {
		// dispose of port manager flushes the pipeline.
		if (mPortManager != null) {
			mPortManager.dispose();
			mPortManager = null;
		}
		if (mPipeline != null) {
			mPipeline.dispose();
			mPipeline = null;
		}
	}

	private static final short MAXSHORT = 32767, MINSHORT = -32768;
	
	static short pinToShort(int value) {
		if (value > MAXSHORT)
			return MAXSHORT;
		else if (value < MINSHORT)
			return MINSHORT;
		return (short) value;
	}

	final class PrintingGraphics extends QDGraphics {
		/** Creates a new PrintingGraphics. You must call <code>initialize</code> on it next. */
		public PrintingGraphics() {
			super();
		}

	    public synchronized Graphics create() {
			if( !internalMarkHostInUse ( ) )
				throw new AWTError("Using invalid Graphics object");
			try {
	    		return (new PrintingGraphics()).initialize(this,fXoff,fYoff,fClip);
	    	} finally {
				internalDoneUsingHost ( );
			}
	    }

	    public synchronized Graphics create(int x, int y, int width, int height) {
			if( !internalMarkHostInUse() )
				throw new AWTError("Using invalid Graphics object");
			try {
				VToolkit.intersect(sRectangle, fClip, fXoff+x,fYoff+y,width,height);
				return (new PrintingGraphics()).initialize(this,fXoff+x,fYoff+y,sRectangle);
	    	} finally {
				internalDoneUsingHost ( );
			}
		}

		/**
		 * Override QDGraphics.restore(), to reset origin and clipping during drawing.
		 */
		void restore() {
			super.restore();
			
	 		int port = VAWTDirect.FastGetThePort();
			if (port != mPrintingPort)
				QuickdrawFunctions.SetPort(mPrintingPort);
			
			// restore default orgin.
			QuickdrawFunctions.SetOrigin((short)0, (short)0);
			
			// set up correct clipping.
			QDRectStruct clipRect = new QDRectStruct();
			clipRect.Set(pinToShort(mClipping.x), pinToShort(mClipping.y),
						pinToShort(mClipping.x + mClipping.width), pinToShort(mClipping.y + mClipping.width));
			QuickdrawFunctions.ClipRect(clipRect);
			
			if (port != mPrintingPort)
				QuickdrawFunctions.SetPort(port);
		}
	}
	
	/**
	 * Creates a graphics object that wraps the specified printing port.
	 * Assumes that the underlying port's coordinate system is already
	 * set up to have (0, 0) as the upper left corner.
	 */
	public Graphics getGraphics(Component component) {
		mClipping = component.getBounds();
		mClipping.x = mOriginX; mClipping.y = mOriginY;
		QDGraphics graphics = new PrintingGraphics();
		graphics.initialize(mPrintingPort, this, mPortManager,
							mOriginX, mOriginY, mClipping, component.getForeground(),
							component.getFont());
		return graphics;
	}

	/** methods to satisfy GraphicsHost interface. */
	public void graphicsCreated(QDGraphics g) throws OutOfMemoryError {}
	public void graphicsDisposed(QDGraphics g) {}
	public RGBColorValue getBackgroundRGB() { return new RGBColorValue(Color.white); }
	public void repaint(RectStruct r) {}
	public boolean markInUse() { return (mPortManager != null); }
	public void doneUsingIt() {}
}
