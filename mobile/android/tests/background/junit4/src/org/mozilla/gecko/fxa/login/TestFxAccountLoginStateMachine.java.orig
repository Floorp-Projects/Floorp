/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.fxa.login;

import java.security.NoSuchAlgorithmException;
import java.util.LinkedList;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.mozilla.gecko.background.fxa.FxAccountClient;
import org.mozilla.gecko.background.fxa.FxAccountUtils;
import org.mozilla.gecko.background.testhelpers.WaitHelper;
import org.mozilla.gecko.browserid.BrowserIDKeyPair;
import org.mozilla.gecko.browserid.RSACryptoImplementation;
import org.mozilla.gecko.fxa.login.FxAccountLoginStateMachine.LoginStateMachineDelegate;
import org.mozilla.gecko.fxa.login.FxAccountLoginTransition.Transition;
import org.mozilla.gecko.fxa.login.State.StateLabel;
import org.mozilla.gecko.sync.Utils;

public class TestFxAccountLoginStateMachine {
  // private static final String TEST_AUDIENCE = "http://testAudience.com";
  private static final String TEST_EMAIL = "test@test.com";
  private static byte[] TEST_EMAIL_UTF8;
  private static final String TEST_PASSWORD = "testtest";
  private static byte[] TEST_PASSWORD_UTF8;
  private static byte[] TEST_QUICK_STRETCHED_PW;
  private static byte[] TEST_UNWRAPKB;
  private static final byte[] TEST_SESSION_TOKEN = Utils.generateRandomBytes(32);
  private static final byte[] TEST_KEY_FETCH_TOKEN = Utils.generateRandomBytes(32);

  protected MockFxAccountClient client;
  protected FxAccountLoginStateMachine sm;

  @Before
  public void setUp() throws Exception {
    if (TEST_EMAIL_UTF8 == null) {
      TEST_EMAIL_UTF8 = TEST_EMAIL.getBytes("UTF-8");
    }
    if (TEST_PASSWORD_UTF8 == null) {
      TEST_PASSWORD_UTF8 = TEST_PASSWORD.getBytes("UTF-8");
    }
    if (TEST_QUICK_STRETCHED_PW == null) {
      TEST_QUICK_STRETCHED_PW = FxAccountUtils.generateQuickStretchedPW(TEST_EMAIL_UTF8, TEST_PASSWORD_UTF8);
    }
    if (TEST_UNWRAPKB == null) {
      TEST_UNWRAPKB = FxAccountUtils.generateUnwrapBKey(TEST_QUICK_STRETCHED_PW);
    }
    client = new MockFxAccountClient();
    sm = new FxAccountLoginStateMachine();
  }

  protected static class Trace {
    public final LinkedList<State> states;
    public final LinkedList<Transition> transitions;

    public Trace(LinkedList<State> states, LinkedList<Transition> transitions) {
      this.states = states;
      this.transitions = transitions;
    }

    public void assertEquals(String string) {
      Assert.assertArrayEquals(string.split(", "), toString().split(", "));
    }

    @Override
    public String toString() {
      final LinkedList<State> states = new LinkedList<State>(this.states);
      final LinkedList<Transition> transitions = new LinkedList<Transition>(this.transitions);
      LinkedList<String> names = new LinkedList<String>();
      State state;
      while ((state = states.pollFirst()) != null) {
        names.add(state.getStateLabel().name());
        Transition transition = transitions.pollFirst();
        if (transition != null) {
          names.add(">" + transition.toString());
        }
      }
      return names.toString();
    }

    public String stateString() {
      LinkedList<String> names = new LinkedList<String>();
      for (State state : states) {
        names.add(state.getStateLabel().name());
      }
      return names.toString();
    }

    public String transitionString() {
      LinkedList<String> names = new LinkedList<String>();
      for (Transition transition : transitions) {
        names.add(transition.toString());
      }
      return names.toString();
    }
  }

  protected Trace trace(final State initialState, final StateLabel desiredState) {
    final LinkedList<Transition> transitions = new LinkedList<Transition>();
    final LinkedList<State> states = new LinkedList<State>();
    states.add(initialState);

    WaitHelper.getTestWaiter().performWait(new Runnable() {
      @Override
      public void run() {
        sm.advance(initialState, desiredState, new LoginStateMachineDelegate() {
          @Override
          public void handleTransition(Transition transition, State state) {
            transitions.add(transition);
            states.add(state);
          }

          @Override
          public void handleFinal(State state) {
            WaitHelper.getTestWaiter().performNotify();
          }

          @Override
          public FxAccountClient getClient() {
            return client;
          }

          @Override
          public long getCertificateDurationInMilliseconds() {
            return 30 * 1000;
          }

          @Override
          public long getAssertionDurationInMilliseconds() {
            return 10 * 1000;
          }

          @Override
          public BrowserIDKeyPair generateKeyPair() throws NoSuchAlgorithmException {
            return RSACryptoImplementation.generateKeyPair(512);
          }
        });
      }
    });

    return new Trace(states, transitions);
  }

  @Test
  public void testEnagedUnverified() throws Exception {
    client.addUser(TEST_EMAIL, TEST_QUICK_STRETCHED_PW, false, TEST_SESSION_TOKEN, TEST_KEY_FETCH_TOKEN);
    Trace trace = trace(new Engaged(TEST_EMAIL, "uid", true, TEST_UNWRAPKB, TEST_SESSION_TOKEN, TEST_KEY_FETCH_TOKEN), StateLabel.Married);
    trace.assertEquals("[Engaged, >AccountNeedsVerification, Engaged]");
  }

  @Test
  public void testEngagedTransitionToAccountVerified() throws Exception {
    client.addUser(TEST_EMAIL, TEST_QUICK_STRETCHED_PW, true, TEST_SESSION_TOKEN, TEST_KEY_FETCH_TOKEN);
    Trace trace = trace(new Engaged(TEST_EMAIL, "uid", false, TEST_UNWRAPKB, TEST_SESSION_TOKEN, TEST_KEY_FETCH_TOKEN), StateLabel.Married);
    trace.assertEquals("[Engaged, >AccountVerified, Cohabiting, >LogMessage('sign succeeded'), Married]");
  }

  @Test
  public void testEngagedVerified() throws Exception {
    client.addUser(TEST_EMAIL, TEST_QUICK_STRETCHED_PW, true, TEST_SESSION_TOKEN, TEST_KEY_FETCH_TOKEN);
    Trace trace = trace(new Engaged(TEST_EMAIL, "uid", true, TEST_UNWRAPKB, TEST_SESSION_TOKEN, TEST_KEY_FETCH_TOKEN), StateLabel.Married);
    trace.assertEquals("[Engaged, >LogMessage('keys succeeded'), Cohabiting, >LogMessage('sign succeeded'), Married]");
  }

  @Test
  public void testPartial() throws Exception {
    client.addUser(TEST_EMAIL, TEST_QUICK_STRETCHED_PW, true, TEST_SESSION_TOKEN, TEST_KEY_FETCH_TOKEN);
    // What if we stop at Cohabiting?
    Trace trace = trace(new Engaged(TEST_EMAIL, "uid", true, TEST_UNWRAPKB, TEST_SESSION_TOKEN, TEST_KEY_FETCH_TOKEN), StateLabel.Cohabiting);
    trace.assertEquals("[Engaged, >LogMessage('keys succeeded'), Cohabiting]");
  }

  @Test
  public void testBadSessionToken() throws Exception {
    client.addUser(TEST_EMAIL, TEST_QUICK_STRETCHED_PW, true, TEST_SESSION_TOKEN, TEST_KEY_FETCH_TOKEN);
    client.sessionTokens.clear();
    Trace trace = trace(new Engaged(TEST_EMAIL, "uid", true, TEST_UNWRAPKB, TEST_SESSION_TOKEN, TEST_KEY_FETCH_TOKEN), StateLabel.Married);
    trace.assertEquals("[Engaged, >LogMessage('keys succeeded'), Cohabiting, >Log(<FxAccountClientRemoteException 401 [110]: invalid sessionToken>), Separated, >PasswordRequired, Separated]");
  }

  @Test
  public void testBadKeyFetchToken() throws Exception {
    client.addUser(TEST_EMAIL, TEST_QUICK_STRETCHED_PW, true, TEST_SESSION_TOKEN, TEST_KEY_FETCH_TOKEN);
    client.keyFetchTokens.clear();
    Trace trace = trace(new Engaged(TEST_EMAIL, "uid", true, TEST_UNWRAPKB, TEST_SESSION_TOKEN, TEST_KEY_FETCH_TOKEN), StateLabel.Married);
    trace.assertEquals("[Engaged, >Log(<FxAccountClientRemoteException 401 [110]: invalid keyFetchToken>), Separated, >PasswordRequired, Separated]");
  }

  @Test
  public void testMarried() throws Exception {
    client.addUser(TEST_EMAIL, TEST_QUICK_STRETCHED_PW, true, TEST_SESSION_TOKEN, TEST_KEY_FETCH_TOKEN);
    Trace trace = trace(new Engaged(TEST_EMAIL, "uid", true, TEST_UNWRAPKB, TEST_SESSION_TOKEN, TEST_KEY_FETCH_TOKEN), StateLabel.Married);
    trace.assertEquals("[Engaged, >LogMessage('keys succeeded'), Cohabiting, >LogMessage('sign succeeded'), Married]");
    // What if we're already in the desired state?
    State married = trace.states.getLast();
    Assert.assertEquals(StateLabel.Married, married.getStateLabel());
    trace = trace(married, StateLabel.Married);
    trace.assertEquals("[Married]");
  }
}
