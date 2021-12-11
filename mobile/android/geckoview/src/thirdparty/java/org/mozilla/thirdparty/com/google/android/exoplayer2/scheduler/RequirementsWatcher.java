/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package org.mozilla.thirdparty.com.google.android.exoplayer2.scheduler;

import android.annotation.TargetApi;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.os.Handler;
import android.os.Looper;
import android.os.PowerManager;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import org.mozilla.thirdparty.com.google.android.exoplayer2.util.Assertions;
import org.mozilla.thirdparty.com.google.android.exoplayer2.util.Util;

/**
 * Watches whether the {@link Requirements} are met and notifies the {@link Listener} on changes.
 */
public final class RequirementsWatcher {

  /**
   * Notified when RequirementsWatcher instance first created and on changes whether the {@link
   * Requirements} are met.
   */
  public interface Listener {
    /**
     * Called when there is a change on the met requirements.
     *
     * @param requirementsWatcher Calling instance.
     * @param notMetRequirements {@link Requirements.RequirementFlags RequirementFlags} that are not
     *     met, or 0.
     */
    void onRequirementsStateChanged(
        RequirementsWatcher requirementsWatcher,
        @Requirements.RequirementFlags int notMetRequirements);
  }

  private final Context context;
  private final Listener listener;
  private final Requirements requirements;
  private final Handler handler;

  @Nullable private DeviceStatusChangeReceiver receiver;

  @Requirements.RequirementFlags private int notMetRequirements;
  @Nullable private NetworkCallback networkCallback;

  /**
   * @param context Any context.
   * @param listener Notified whether the {@link Requirements} are met.
   * @param requirements The requirements to watch.
   */
  public RequirementsWatcher(Context context, Listener listener, Requirements requirements) {
    this.context = context.getApplicationContext();
    this.listener = listener;
    this.requirements = requirements;
    handler = new Handler(Util.getLooper());
  }

  /**
   * Starts watching for changes. Must be called from a thread that has an associated {@link
   * Looper}. Listener methods are called on the caller thread.
   *
   * @return Initial {@link Requirements.RequirementFlags RequirementFlags} that are not met, or 0.
   */
  @Requirements.RequirementFlags
  public int start() {
    notMetRequirements = requirements.getNotMetRequirements(context);

    IntentFilter filter = new IntentFilter();
    if (requirements.isNetworkRequired()) {
      if (Util.SDK_INT >= 24) {
        registerNetworkCallbackV24();
      } else {
        filter.addAction(ConnectivityManager.CONNECTIVITY_ACTION);
      }
    }
    if (requirements.isChargingRequired()) {
      filter.addAction(Intent.ACTION_POWER_CONNECTED);
      filter.addAction(Intent.ACTION_POWER_DISCONNECTED);
    }
    if (requirements.isIdleRequired()) {
      if (Util.SDK_INT >= 23) {
        filter.addAction(PowerManager.ACTION_DEVICE_IDLE_MODE_CHANGED);
      } else {
        filter.addAction(Intent.ACTION_SCREEN_ON);
        filter.addAction(Intent.ACTION_SCREEN_OFF);
      }
    }
    receiver = new DeviceStatusChangeReceiver();
    context.registerReceiver(receiver, filter, null, handler);
    return notMetRequirements;
  }

  /** Stops watching for changes. */
  public void stop() {
    context.unregisterReceiver(Assertions.checkNotNull(receiver));
    receiver = null;
    if (Util.SDK_INT >= 24 && networkCallback != null) {
      unregisterNetworkCallbackV24();
    }
  }

  /** Returns watched {@link Requirements}. */
  public Requirements getRequirements() {
    return requirements;
  }

  @TargetApi(24)
  private void registerNetworkCallbackV24() {
    ConnectivityManager connectivityManager =
        Assertions.checkNotNull(
            (ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE));
    networkCallback = new NetworkCallback();
    connectivityManager.registerDefaultNetworkCallback(networkCallback);
  }

  @TargetApi(24)
  private void unregisterNetworkCallbackV24() {
    ConnectivityManager connectivityManager =
        (ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE);
    connectivityManager.unregisterNetworkCallback(Assertions.checkNotNull(networkCallback));
    networkCallback = null;
  }

  private void checkRequirements() {
    @Requirements.RequirementFlags
    int notMetRequirements = requirements.getNotMetRequirements(context);
    if (this.notMetRequirements != notMetRequirements) {
      this.notMetRequirements = notMetRequirements;
      listener.onRequirementsStateChanged(this, notMetRequirements);
    }
  }

  private class DeviceStatusChangeReceiver extends BroadcastReceiver {
    @Override
    public void onReceive(Context context, Intent intent) {
      if (!isInitialStickyBroadcast()) {
        checkRequirements();
      }
    }
  }

  @RequiresApi(24)
  private final class NetworkCallback extends ConnectivityManager.NetworkCallback {
    boolean receivedCapabilitiesChange;
    boolean networkValidated;

    @Override
    public void onAvailable(Network network) {
      onNetworkCallback();
    }

    @Override
    public void onLost(Network network) {
      onNetworkCallback();
    }

    @Override
    public void onCapabilitiesChanged(Network network, NetworkCapabilities networkCapabilities) {
      boolean networkValidated =
          networkCapabilities.hasCapability(NetworkCapabilities.NET_CAPABILITY_VALIDATED);
      if (!receivedCapabilitiesChange || this.networkValidated != networkValidated) {
        receivedCapabilitiesChange = true;
        this.networkValidated = networkValidated;
        onNetworkCallback();
      }
    }

    private void onNetworkCallback() {
      handler.post(
          () -> {
            if (networkCallback != null) {
              checkRequirements();
            }
          });
    }
  }
}
