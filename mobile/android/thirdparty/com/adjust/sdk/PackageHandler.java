//
//  PackageHandler.java
//  Adjust
//
//  Created by Christian Wellenbrock on 2013-06-25.
//  Copyright (c) 2013 adjust GmbH. All rights reserved.
//  See the file MIT-LICENSE for copying permission.
//

package com.adjust.sdk;

import android.content.Context;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Message;

import org.json.JSONObject;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.NotSerializableException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.OptionalDataException;
import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

// persistent
public class PackageHandler extends HandlerThread implements IPackageHandler {
    private static final String PACKAGE_QUEUE_FILENAME = "AdjustIoPackageQueue";

    private final InternalHandler internalHandler;
    private IRequestHandler requestHandler;
    private IActivityHandler activityHandler;
    private List<ActivityPackage> packageQueue;
    private AtomicBoolean isSending;
    private boolean paused;
    private Context context;
    private ILogger logger;

    public PackageHandler(IActivityHandler activityHandler,
                          Context context,
                          boolean startPaused) {
        super(Constants.LOGTAG, MIN_PRIORITY);
        setDaemon(true);
        start();
        this.internalHandler = new InternalHandler(getLooper(), this);
        this.logger = AdjustFactory.getLogger();

        init(activityHandler, context, startPaused);

        Message message = Message.obtain();
        message.arg1 = InternalHandler.INIT;
        internalHandler.sendMessage(message);
    }

    @Override
    public void init(IActivityHandler activityHandler, Context context, boolean startPaused) {
        this.activityHandler = activityHandler;
        this.context = context;
        this.paused = startPaused;
    }

    // add a package to the queue
    @Override
    public void addPackage(ActivityPackage pack) {
        Message message = Message.obtain();
        message.arg1 = InternalHandler.ADD;
        message.obj = pack;
        internalHandler.sendMessage(message);
    }

    // try to send the oldest package
    @Override
    public void sendFirstPackage() {
        Message message = Message.obtain();
        message.arg1 = InternalHandler.SEND_FIRST;
        internalHandler.sendMessage(message);
    }

    // remove oldest package and try to send the next one
    // (after success or possibly permanent failure)
    @Override
    public void sendNextPackage() {
        Message message = Message.obtain();
        message.arg1 = InternalHandler.SEND_NEXT;
        internalHandler.sendMessage(message);
    }

    // close the package to retry in the future (after temporary failure)
    @Override
    public void closeFirstPackage() {
        isSending.set(false);
    }

    // interrupt the sending loop after the current request has finished
    @Override
    public void pauseSending() {
        paused = true;
    }

    // allow sending requests again
    @Override
    public void resumeSending() {
        paused = false;
    }

    // short info about how failing packages are handled
    @Override
    public String getFailureMessage() {
        return "Will retry later.";
    }

    @Override
    public void finishedTrackingActivity(JSONObject jsonResponse) {
        activityHandler.finishedTrackingActivity(jsonResponse);
    }

    @Override
    public void sendClickPackage(ActivityPackage clickPackage) {
        logger.debug("Sending click package (%s)", clickPackage);
        logger.verbose("%s", clickPackage.getExtendedString());
        requestHandler.sendClickPackage(clickPackage);
    }

    private static final class InternalHandler extends Handler {
        private static final int INIT = 1;
        private static final int ADD = 2;
        private static final int SEND_NEXT = 3;
        private static final int SEND_FIRST = 4;

        private final WeakReference<PackageHandler> packageHandlerReference;

        protected InternalHandler(Looper looper, PackageHandler packageHandler) {
            super(looper);
            this.packageHandlerReference = new WeakReference<PackageHandler>(packageHandler);
        }

        @Override
        public void handleMessage(Message message) {
            super.handleMessage(message);

            PackageHandler packageHandler = packageHandlerReference.get();
            if (null == packageHandler) {
                return;
            }

            switch (message.arg1) {
                case INIT:
                    packageHandler.initInternal();
                    break;
                case ADD:
                    ActivityPackage activityPackage = (ActivityPackage) message.obj;
                    packageHandler.addInternal(activityPackage);
                    break;
                case SEND_FIRST:
                    packageHandler.sendFirstInternal();
                    break;
                case SEND_NEXT:
                    packageHandler.sendNextInternal();
                    break;
            }
        }
    }

    // internal methods run in dedicated queue thread

    private void initInternal() {
        requestHandler = AdjustFactory.getRequestHandler(this);

        isSending = new AtomicBoolean();

        readPackageQueue();
    }

    private void addInternal(ActivityPackage newPackage) {
        packageQueue.add(newPackage);
        logger.debug("Added package %d (%s)", packageQueue.size(), newPackage);
        logger.verbose("%s", newPackage.getExtendedString());

        writePackageQueue();
    }

    private void sendFirstInternal() {
        if (packageQueue.isEmpty()) {
            return;
        }

        if (paused) {
            logger.debug("Package handler is paused");
            return;
        }
        if (isSending.getAndSet(true)) {
            logger.verbose("Package handler is already sending");
            return;
        }

        ActivityPackage firstPackage = packageQueue.get(0);
        requestHandler.sendPackage(firstPackage);
    }

    private void sendNextInternal() {
        packageQueue.remove(0);
        writePackageQueue();
        isSending.set(false);
        sendFirstInternal();
    }

    private void readPackageQueue() {
        try {
            FileInputStream inputStream = context.openFileInput(PACKAGE_QUEUE_FILENAME);
            BufferedInputStream bufferedStream = new BufferedInputStream(inputStream);
            ObjectInputStream objectStream = new ObjectInputStream(bufferedStream);

            try {
                Object object = objectStream.readObject();
                @SuppressWarnings("unchecked")
                List<ActivityPackage> packageQueue = (List<ActivityPackage>) object;
                logger.debug("Package handler read %d packages", packageQueue.size());
                this.packageQueue = packageQueue;
                return;
            } catch (ClassNotFoundException e) {
                logger.error("Failed to find package queue class");
            } catch (OptionalDataException e) {
                /* no-op */
            } catch (IOException e) {
                logger.error("Failed to read package queue object");
            } catch (ClassCastException e) {
                logger.error("Failed to cast package queue object");
            } finally {
                objectStream.close();
            }
        } catch (FileNotFoundException e) {
            logger.verbose("Package queue file not found");
        } catch (Exception e) {
            logger.error("Failed to read package queue file");
        }

        // start with a fresh package queue in case of any exception
        packageQueue = new ArrayList<ActivityPackage>();
    }

    public static Boolean deletePackageQueue(Context context) {
        return context.deleteFile(PACKAGE_QUEUE_FILENAME);
    }


    private void writePackageQueue() {
        try {
            FileOutputStream outputStream = context.openFileOutput(PACKAGE_QUEUE_FILENAME, Context.MODE_PRIVATE);
            BufferedOutputStream bufferedStream = new BufferedOutputStream(outputStream);
            ObjectOutputStream objectStream = new ObjectOutputStream(bufferedStream);

            try {
                objectStream.writeObject(packageQueue);
                logger.debug("Package handler wrote %d packages", packageQueue.size());
            } catch (NotSerializableException e) {
                logger.error("Failed to serialize packages");
            } finally {
                objectStream.close();
            }
        } catch (Exception e) {
            logger.error("Failed to write packages (%s)", e.getLocalizedMessage());
            e.printStackTrace();
        }
    }
}
