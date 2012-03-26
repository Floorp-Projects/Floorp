/* -*- Mode: Java; tab-width: 20; indent-tabs-mode: nil; -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Android code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Gian-Carlo Pascutto <gpascutto@mozilla.com>
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

package org.mozilla.gecko;

import android.app.Dialog;
import android.content.Context;
import android.graphics.drawable.AnimationDrawable;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.os.Handler;
import android.util.Log;
import android.widget.ImageView;
import java.util.Timer;
import java.util.TimerTask;

import org.mozilla.gecko.R;

public class SetupScreen extends Dialog
{
    private static final String LOGTAG = "SetupScreen";
    // Default delay before showing dialog, in milliseconds
    private static final int DEFAULT_DELAY = 100;
    private AnimationDrawable mProgressSpinner;
    private Context mContext;
    private Timer mTimer;
    private TimerTask mShowTask;

    public class ShowTask extends TimerTask {
        private Handler mHandler;

        public ShowTask(Handler aHandler) {
            mHandler = aHandler;
        }

        @Override
        public void run() {
            mHandler.post(new Runnable() {
                    public void run() {
                        SetupScreen.this.show();
                    }
            });

        }

        @Override
        public boolean cancel() {
            boolean stillInQueue = super.cancel();
            if (!stillInQueue) {
                mHandler.post(new Runnable() {
                        public void run() {
                            // SetupScreen.dismiss calls us,
                            // we need to call the real Dialog.dismiss.
                            SetupScreen.super.dismiss();
                        }
                });
            }
            return stillInQueue;
        }
    }

    public SetupScreen(Context aContext) {
        super(aContext, android.R.style.Theme_NoTitleBar_Fullscreen);
        mContext = aContext;
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.setup_screen);
        setCancelable(false);

        setTitle(R.string.splash_settingup);

        ImageView spinnerImage = (ImageView)findViewById(R.id.spinner_image);
        mProgressSpinner = (AnimationDrawable)spinnerImage.getBackground();
        spinnerImage.setImageDrawable(mProgressSpinner);
    }

    @Override
    public void onWindowFocusChanged (boolean hasFocus) {
        mProgressSpinner.start();
    }

    public void showDelayed(Handler aHandler) {
        showDelayed(aHandler, DEFAULT_DELAY);
    }

    // Delay showing the dialog until at least aDelay ms have
    // passed. We need to know the handler to (eventually) post the UI
    // actions on.
    public void showDelayed(Handler aHandler, int aDelay) {
        mTimer = new Timer("SetupScreen");
        mShowTask = new ShowTask(aHandler);
        mTimer.schedule(mShowTask, aDelay);
    }

    @Override
    public void dismiss() {
        // Cancel the timers/tasks if we were using showDelayed,
        // and post to the correct handler.
        if (mShowTask != null) {
            mShowTask.cancel();
            mShowTask = null;
        } else {
            // If not using showDelayed, just dismiss here.
            super.dismiss();
        }
        if (mTimer != null) {
            mTimer.cancel();
            mTimer = null;
        }
    }
}
