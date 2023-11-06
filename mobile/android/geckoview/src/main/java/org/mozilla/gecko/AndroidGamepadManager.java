/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.Context;
import android.hardware.input.InputManager;
import android.util.SparseArray;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import java.util.ArrayList;
import java.util.List;
import java.util.Timer;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.util.ThreadUtils;

public class AndroidGamepadManager {
  // This is completely arbitrary.
  private static final float TRIGGER_PRESSED_THRESHOLD = 0.25f;
  private static final long POLL_TIMER_PERIOD = 1000; // milliseconds

  private enum Axis {
    X(MotionEvent.AXIS_X),
    Y(MotionEvent.AXIS_Y),
    Z(MotionEvent.AXIS_Z),
    RZ(MotionEvent.AXIS_RZ);

    public final int axis;

    Axis(final int axis) {
      this.axis = axis;
    }
  }

  // A list of gamepad button mappings. Axes are determined at
  // runtime, as they vary by Android version.
  private enum Trigger {
    Left(6),
    Right(7);

    public final int button;

    Trigger(final int button) {
      this.button = button;
    }
  }

  private static final int FIRST_DPAD_BUTTON = 12;

  // A list of axis number, gamepad button mappings for negative, positive.
  // Button mappings are added to FIRST_DPAD_BUTTON.
  private enum DpadAxis {
    UpDown(MotionEvent.AXIS_HAT_Y, 0, 1),
    LeftRight(MotionEvent.AXIS_HAT_X, 2, 3);

    public final int axis;
    public final int negativeButton;
    public final int positiveButton;

    DpadAxis(final int axis, final int negativeButton, final int positiveButton) {
      this.axis = axis;
      this.negativeButton = negativeButton;
      this.positiveButton = positiveButton;
    }
  }

  private enum Button {
    A(KeyEvent.KEYCODE_BUTTON_A),
    B(KeyEvent.KEYCODE_BUTTON_B),
    X(KeyEvent.KEYCODE_BUTTON_X),
    Y(KeyEvent.KEYCODE_BUTTON_Y),
    L1(KeyEvent.KEYCODE_BUTTON_L1),
    R1(KeyEvent.KEYCODE_BUTTON_R1),
    L2(KeyEvent.KEYCODE_BUTTON_L2),
    R2(KeyEvent.KEYCODE_BUTTON_R2),
    SELECT(KeyEvent.KEYCODE_BUTTON_SELECT),
    START(KeyEvent.KEYCODE_BUTTON_START),
    THUMBL(KeyEvent.KEYCODE_BUTTON_THUMBL),
    THUMBR(KeyEvent.KEYCODE_BUTTON_THUMBR),
    DPAD_UP(KeyEvent.KEYCODE_DPAD_UP),
    DPAD_DOWN(KeyEvent.KEYCODE_DPAD_DOWN),
    DPAD_LEFT(KeyEvent.KEYCODE_DPAD_LEFT),
    DPAD_RIGHT(KeyEvent.KEYCODE_DPAD_RIGHT);

    public final int button;

    Button(final int button) {
      this.button = button;
    }
  }

  private static class Gamepad {
    // ID from GamepadService
    public byte[] handle;
    // Retain axis state so we can determine changes.
    public float axes[];
    public boolean dpad[];
    public int triggerAxes[];
    public float triggers[];

    public Gamepad(final byte[] handle, final int deviceId) {
      this.handle = handle;
      axes = new float[Axis.values().length];
      dpad = new boolean[4];
      triggers = new float[2];

      final InputDevice device = InputDevice.getDevice(deviceId);
      if (device != null) {
        // LTRIGGER/RTRIGGER don't seem to be exposed on older
        // versions of Android.
        if (device.getMotionRange(MotionEvent.AXIS_LTRIGGER) != null
            && device.getMotionRange(MotionEvent.AXIS_RTRIGGER) != null) {
          triggerAxes = new int[] {MotionEvent.AXIS_LTRIGGER, MotionEvent.AXIS_RTRIGGER};
        } else if (device.getMotionRange(MotionEvent.AXIS_BRAKE) != null
            && device.getMotionRange(MotionEvent.AXIS_GAS) != null) {
          triggerAxes = new int[] {MotionEvent.AXIS_BRAKE, MotionEvent.AXIS_GAS};
        } else {
          triggerAxes = null;
        }
      }
    }
  }

  @WrapForJNI(calledFrom = "ui")
  private static native byte[] nativeAddGamepad();

  @WrapForJNI(calledFrom = "ui")
  private static native void nativeRemoveGamepad(byte[] aGamepadHandle);

  @WrapForJNI(calledFrom = "ui")
  private static native void onButtonChange(
      byte[] aGamepadHandle, int aButton, boolean aPressed, float aValue);

  @WrapForJNI(calledFrom = "ui")
  private static native void onAxisChange(byte[] aGamepadHandle, boolean[] aValid, float[] aValues);

  private static boolean sStarted;
  private static final SparseArray<Gamepad> sGamepads = new SparseArray<>();
  private static final SparseArray<List<KeyEvent>> sPendingGamepads = new SparseArray<>();
  private static InputManager.InputDeviceListener sListener;
  private static Timer sPollTimer;

  private AndroidGamepadManager() {}

  @WrapForJNI
  private static void start(final Context context) {
    ThreadUtils.runOnUiThread(
        new Runnable() {
          @Override
          public void run() {
            doStart(context);
          }
        });
  }

  /* package */ static void doStart(final Context context) {
    ThreadUtils.assertOnUiThread();
    if (!sStarted) {
      scanForGamepads();
      addDeviceListener(context);
      sStarted = true;
    }
  }

  @WrapForJNI
  private static void stop(final Context context) {
    ThreadUtils.runOnUiThread(
        new Runnable() {
          @Override
          public void run() {
            doStop(context);
          }
        });
  }

  /* package */ static void doStop(final Context context) {
    ThreadUtils.assertOnUiThread();
    if (sStarted) {
      removeDeviceListener(context);
      sPendingGamepads.clear();
      sGamepads.clear();
      sStarted = false;
    }
  }

  /* package */ static void handleGamepadAdded(final int deviceId, final byte[] gamepadHandle) {
    ThreadUtils.assertOnUiThread();
    if (!sStarted) {
      return;
    }

    final List<KeyEvent> pending = sPendingGamepads.get(deviceId);
    if (pending == null) {
      removeGamepad(deviceId);
      return;
    }

    sPendingGamepads.remove(deviceId);
    sGamepads.put(deviceId, new Gamepad(gamepadHandle, deviceId));
    // Handle queued KeyEvents
    for (final KeyEvent ev : pending) {
      handleKeyEvent(ev);
    }
  }

  private static float sDeadZoneThresholdOverride = 1e-2f;

  private static boolean isValueInDeadZone(final MotionEvent event, final int axis) {
    final float threshold;
    if (sDeadZoneThresholdOverride >= 0) {
      threshold = sDeadZoneThresholdOverride;
    } else {
      final InputDevice.MotionRange range = event.getDevice().getMotionRange(axis);
      threshold = range.getFlat() + range.getFuzz();
    }
    final float value = event.getAxisValue(axis);
    return (Math.abs(value) < threshold);
  }

  private static float deadZone(final MotionEvent ev, final int axis) {
    if (isValueInDeadZone(ev, axis)) {
      return 0.0f;
    }
    return ev.getAxisValue(axis);
  }

  private static void mapDpadAxis(
      final Gamepad gamepad, final boolean pressed, final float value, final int which) {
    if (pressed != gamepad.dpad[which]) {
      gamepad.dpad[which] = pressed;
      onButtonChange(gamepad.handle, FIRST_DPAD_BUTTON + which, pressed, Math.abs(value));
    }
  }

  public static boolean handleMotionEvent(final MotionEvent ev) {
    ThreadUtils.assertOnUiThread();
    if (!sStarted) {
      return false;
    }

    final Gamepad gamepad = sGamepads.get(ev.getDeviceId());
    if (gamepad == null) {
      // Not a device we care about.
      return false;
    }

    // First check the analog stick axes
    final boolean[] valid = new boolean[Axis.values().length];
    final float[] axes = new float[Axis.values().length];
    boolean anyValidAxes = false;
    for (final Axis axis : Axis.values()) {
      final float value = deadZone(ev, axis.axis);
      final int i = axis.ordinal();
      if (value != gamepad.axes[i]) {
        axes[i] = value;
        gamepad.axes[i] = value;
        valid[i] = true;
        anyValidAxes = true;
      }
    }
    if (anyValidAxes) {
      // Send an axismove event.
      onAxisChange(gamepad.handle, valid, axes);
    }

    // Map triggers to buttons.
    if (gamepad.triggerAxes != null) {
      for (final Trigger trigger : Trigger.values()) {
        final int i = trigger.ordinal();
        final int axis = gamepad.triggerAxes[i];
        final float value = deadZone(ev, axis);
        if (value != gamepad.triggers[i]) {
          gamepad.triggers[i] = value;
          final boolean pressed = value > TRIGGER_PRESSED_THRESHOLD;
          onButtonChange(gamepad.handle, trigger.button, pressed, value);
        }
      }
    }
    // Map d-pad to buttons.
    for (final DpadAxis dpadaxis : DpadAxis.values()) {
      final float value = deadZone(ev, dpadaxis.axis);
      mapDpadAxis(gamepad, value < 0.0f, value, dpadaxis.negativeButton);
      mapDpadAxis(gamepad, value > 0.0f, value, dpadaxis.positiveButton);
    }
    return true;
  }

  public static boolean handleKeyEvent(final KeyEvent ev) {
    ThreadUtils.assertOnUiThread();
    if (!sStarted) {
      return false;
    }

    final int deviceId = ev.getDeviceId();
    final List<KeyEvent> pendingGamepad = sPendingGamepads.get(deviceId);
    if (pendingGamepad != null) {
      // Queue up key events for pending devices.
      pendingGamepad.add(ev);
      return true;
    }

    if (sGamepads.get(deviceId) == null) {
      final InputDevice device = ev.getDevice();
      if (device != null
          && (device.getSources() & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD) {
        // This is a gamepad we haven't seen yet.
        addGamepad(device);
        sPendingGamepads.get(deviceId).add(ev);
        return true;
      }
      // Not a device we care about.
      return false;
    }

    int key = -1;
    for (final Button button : Button.values()) {
      if (button.button == ev.getKeyCode()) {
        key = button.ordinal();
        break;
      }
    }
    if (key == -1) {
      // Not a key we know how to handle.
      return false;
    }
    if (ev.getRepeatCount() > 0) {
      // We would handle this key, but we're not interested in
      // repeats. Eat it.
      return true;
    }

    final Gamepad gamepad = sGamepads.get(deviceId);
    final boolean pressed = ev.getAction() == KeyEvent.ACTION_DOWN;
    onButtonChange(gamepad.handle, key, pressed, pressed ? 1.0f : 0.0f);
    return true;
  }

  private static void scanForGamepads() {
    final int[] deviceIds = InputDevice.getDeviceIds();
    if (deviceIds == null) {
      return;
    }
    for (int i = 0; i < deviceIds.length; i++) {
      final InputDevice device = InputDevice.getDevice(deviceIds[i]);
      if (device == null) {
        continue;
      }
      if ((device.getSources() & InputDevice.SOURCE_GAMEPAD) != InputDevice.SOURCE_GAMEPAD) {
        continue;
      }
      addGamepad(device);
    }
  }

  private static void addGamepad(final InputDevice device) {
    sPendingGamepads.put(device.getId(), new ArrayList<KeyEvent>());
    final byte[] gamepadId = nativeAddGamepad();
    ThreadUtils.runOnUiThread(
        new Runnable() {
          @Override
          public void run() {
            handleGamepadAdded(device.getId(), gamepadId);
          }
        });
  }

  private static void removeGamepad(final int deviceId) {
    final Gamepad gamepad = sGamepads.get(deviceId);
    nativeRemoveGamepad(gamepad.handle);
    sGamepads.remove(deviceId);
  }

  private static void addDeviceListener(final Context context) {
    sListener =
        new InputManager.InputDeviceListener() {
          @Override
          public void onInputDeviceAdded(final int deviceId) {
            final InputDevice device = InputDevice.getDevice(deviceId);
            if (device == null) {
              return;
            }
            if ((device.getSources() & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD) {
              addGamepad(device);
            }
          }

          @Override
          public void onInputDeviceRemoved(final int deviceId) {
            if (sPendingGamepads.get(deviceId) != null) {
              // Got removed before Gecko's ack reached us.
              // gamepadAdded will deal with it.
              sPendingGamepads.remove(deviceId);
              return;
            }
            if (sGamepads.get(deviceId) != null) {
              removeGamepad(deviceId);
            }
          }

          @Override
          public void onInputDeviceChanged(final int deviceId) {}
        };
    final InputManager im = (InputManager) context.getSystemService(Context.INPUT_SERVICE);
    im.registerInputDeviceListener(sListener, ThreadUtils.getUiHandler());
  }

  private static void removeDeviceListener(final Context context) {
    final InputManager im = (InputManager) context.getSystemService(Context.INPUT_SERVICE);
    im.unregisterInputDeviceListener(sListener);
    sListener = null;
  }
}
