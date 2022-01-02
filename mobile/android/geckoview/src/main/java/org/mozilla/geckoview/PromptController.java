package org.mozilla.geckoview;

import android.util.Log;
import java.util.HashMap;
import java.util.Map;
import org.json.JSONException;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.geckoview.Autocomplete.AddressSaveOption;
import org.mozilla.geckoview.Autocomplete.AddressSelectOption;
import org.mozilla.geckoview.Autocomplete.CreditCardSaveOption;
import org.mozilla.geckoview.Autocomplete.CreditCardSelectOption;
import org.mozilla.geckoview.Autocomplete.LoginSaveOption;
import org.mozilla.geckoview.Autocomplete.LoginSelectOption;
import org.mozilla.geckoview.GeckoSession.PromptDelegate;
import org.mozilla.geckoview.GeckoSession.PromptDelegate.AlertPrompt;
import org.mozilla.geckoview.GeckoSession.PromptDelegate.AuthPrompt;
import org.mozilla.geckoview.GeckoSession.PromptDelegate.AuthPrompt.AuthOptions;
import org.mozilla.geckoview.GeckoSession.PromptDelegate.AutocompleteRequest;
import org.mozilla.geckoview.GeckoSession.PromptDelegate.BasePrompt;
import org.mozilla.geckoview.GeckoSession.PromptDelegate.BasePrompt.Observer;
import org.mozilla.geckoview.GeckoSession.PromptDelegate.BeforeUnloadPrompt;
import org.mozilla.geckoview.GeckoSession.PromptDelegate.ButtonPrompt;
import org.mozilla.geckoview.GeckoSession.PromptDelegate.ChoicePrompt;
import org.mozilla.geckoview.GeckoSession.PromptDelegate.ColorPrompt;
import org.mozilla.geckoview.GeckoSession.PromptDelegate.DateTimePrompt;
import org.mozilla.geckoview.GeckoSession.PromptDelegate.FilePrompt;
import org.mozilla.geckoview.GeckoSession.PromptDelegate.PopupPrompt;
import org.mozilla.geckoview.GeckoSession.PromptDelegate.PromptInstanceDelegate;
import org.mozilla.geckoview.GeckoSession.PromptDelegate.PromptResponse;
import org.mozilla.geckoview.GeckoSession.PromptDelegate.RepostConfirmPrompt;
import org.mozilla.geckoview.GeckoSession.PromptDelegate.SharePrompt;
import org.mozilla.geckoview.GeckoSession.PromptDelegate.TextPrompt;

/* package */ class PromptController {
  private static final String LOGTAG = "Prompts";

  private static class PromptStorage implements BasePrompt.Observer {
    private final Map<String, BasePrompt> mPrompts = new HashMap<>();

    public void addPrompt(final String id, final BasePrompt prompt) {
      if (mPrompts.containsKey(id)) {
        Log.e(LOGTAG, "Prompt already exists! id=" + id);
        if (BuildConfig.DEBUG) {
          throw new RuntimeException("Prompt already exists! id=" + id);
        }
      }
      mPrompts.put(id, prompt);
    }

    @Override
    public void onPromptCompleted(final BasePrompt prompt) {
      // No need to notify this delegate since the prompt has been completed already.
      mPrompts.remove(prompt.id);
    }

    public void dismiss(final String id) {
      final BasePrompt prompt = mPrompts.get(id);
      if (prompt == null) {
        return;
      }
      final PromptInstanceDelegate delegate = prompt.getDelegate();
      if (delegate != null) {
        delegate.onPromptDismiss(prompt);
      }
      mPrompts.remove(prompt.id);
    }
  }

  final PromptStorage mStorage = new PromptStorage();

  public void dismissPrompt(final String id) {
    mStorage.dismiss(id);
  }

  public void handleEvent(
      final GeckoSession session, final GeckoBundle message, final EventCallback callback) {
    final PromptDelegate delegate = session.getPromptDelegate();
    if (delegate == null) {
      // Default behavior is same as calling dismiss() on callback.
      callback.sendSuccess(null);
      return;
    }

    final String type = message.getString("type");
    final PromptHandler<?> handler = sPromptHandlers.handlerFor(type);
    if (handler == null) {
      callback.sendError("Invalid type: " + type);
      return;
    }
    final GeckoResult<PromptResponse> res = getResponse(message, session, delegate, handler);

    if (res == null) {
      // Adhere to default behavior if the delegate returns null.
      callback.sendSuccess(null);
    } else {
      res.accept(
          value -> value.dispatch(callback),
          exception -> callback.sendError("Failed to get prompt response."));
    }
  }

  private <PromptType extends BasePrompt> GeckoResult<PromptResponse> getResponse(
      final GeckoBundle message,
      final GeckoSession session,
      final PromptDelegate delegate,
      final PromptHandler<PromptType> handler) {
    final PromptType prompt = handler.newPrompt(message, mStorage);
    if (prompt == null) {
      try {
        Log.e(LOGTAG, "Invalid prompt: " + message.toJSONObject().toString());
      } catch (final JSONException ex) {
        Log.e(LOGTAG, "Invalid prompt, invalid data", ex);
      }

      return GeckoResult.fromException(new IllegalArgumentException("Invalid prompt data."));
    }

    mStorage.addPrompt(prompt.id, prompt);
    return handler.callDelegate(prompt, session, delegate);
  }

  private interface PromptHandler<PromptType extends BasePrompt> {
    PromptType newPrompt(GeckoBundle info, Observer observer);

    GeckoResult<PromptResponse> callDelegate(
        PromptType prompt, GeckoSession session, PromptDelegate delegate);
  }

  private static final class AlertHandler implements PromptHandler<AlertPrompt> {
    @Override
    public AlertPrompt newPrompt(final GeckoBundle info, final Observer observer) {
      return new AlertPrompt(
          info.getString("id"), info.getString("title"), info.getString("msg"), observer);
    }

    @Override
    public GeckoResult<PromptResponse> callDelegate(
        final AlertPrompt prompt, final GeckoSession session, final PromptDelegate delegate) {
      return delegate.onAlertPrompt(session, prompt);
    }
  }

  private static final class BeforeUnloadHandler implements PromptHandler<BeforeUnloadPrompt> {
    @Override
    public BeforeUnloadPrompt newPrompt(final GeckoBundle info, final Observer observer) {
      return new BeforeUnloadPrompt(info.getString("id"), observer);
    }

    @Override
    public GeckoResult<PromptResponse> callDelegate(
        final BeforeUnloadPrompt prompt,
        final GeckoSession session,
        final PromptDelegate delegate) {
      return delegate.onBeforeUnloadPrompt(session, prompt);
    }
  }

  private static final class ButtonHandler implements PromptHandler<ButtonPrompt> {
    @Override
    public ButtonPrompt newPrompt(final GeckoBundle info, final Observer observer) {
      return new ButtonPrompt(
          info.getString("id"), info.getString("title"), info.getString("msg"), observer);
    }

    @Override
    public GeckoResult<PromptResponse> callDelegate(
        final ButtonPrompt prompt, final GeckoSession session, final PromptDelegate delegate) {
      return delegate.onButtonPrompt(session, prompt);
    }
  }

  private static final class TextHandler implements PromptHandler<TextPrompt> {
    @Override
    public TextPrompt newPrompt(final GeckoBundle info, final Observer observer) {
      return new TextPrompt(
          info.getString("id"),
          info.getString("title"),
          info.getString("msg"),
          info.getString("value"),
          observer);
    }

    @Override
    public GeckoResult<PromptResponse> callDelegate(
        final TextPrompt prompt, final GeckoSession session, final PromptDelegate delegate) {
      return delegate.onTextPrompt(session, prompt);
    }
  }

  private static final class AuthHandler implements PromptHandler<AuthPrompt> {
    @Override
    public AuthPrompt newPrompt(final GeckoBundle info, final Observer observer) {
      return new AuthPrompt(
          info.getString("id"),
          info.getString("title"),
          info.getString("msg"),
          new AuthOptions(info.getBundle("options")),
          observer);
    }

    @Override
    public GeckoResult<PromptResponse> callDelegate(
        final AuthPrompt prompt, final GeckoSession session, final PromptDelegate delegate) {
      return delegate.onAuthPrompt(session, prompt);
    }
  }

  private static final class ChoiceHandler implements PromptHandler<ChoicePrompt> {
    @Override
    public ChoicePrompt newPrompt(final GeckoBundle info, final Observer observer) {
      final int intMode;
      final String mode = info.getString("mode");
      if ("menu".equals(mode)) {
        intMode = ChoicePrompt.Type.MENU;
      } else if ("single".equals(mode)) {
        intMode = ChoicePrompt.Type.SINGLE;
      } else if ("multiple".equals(mode)) {
        intMode = ChoicePrompt.Type.MULTIPLE;
      } else {
        return null;
      }

      final GeckoBundle[] choiceBundles = info.getBundleArray("choices");
      final ChoicePrompt.Choice[] choices;
      if (choiceBundles == null || choiceBundles.length == 0) {
        choices = new ChoicePrompt.Choice[0];
      } else {
        choices = new ChoicePrompt.Choice[choiceBundles.length];
        for (int i = 0; i < choiceBundles.length; i++) {
          choices[i] = new ChoicePrompt.Choice(choiceBundles[i]);
        }
      }

      return new ChoicePrompt(
          info.getString("id"),
          info.getString("title"),
          info.getString("msg"),
          intMode,
          choices,
          observer);
    }

    @Override
    public GeckoResult<PromptResponse> callDelegate(
        final ChoicePrompt prompt, final GeckoSession session, final PromptDelegate delegate) {
      return delegate.onChoicePrompt(session, prompt);
    }
  }

  private static final class ColorHandler implements PromptHandler<ColorPrompt> {
    @Override
    public ColorPrompt newPrompt(final GeckoBundle info, final Observer observer) {
      return new ColorPrompt(
          info.getString("id"), info.getString("title"), info.getString("value"), observer);
    }

    @Override
    public GeckoResult<PromptResponse> callDelegate(
        final ColorPrompt prompt, final GeckoSession session, final PromptDelegate delegate) {
      return delegate.onColorPrompt(session, prompt);
    }
  }

  private static final class DateTimeHandler implements PromptHandler<DateTimePrompt> {
    @Override
    public DateTimePrompt newPrompt(final GeckoBundle info, final Observer observer) {
      final String mode = info.getString("mode");
      final int intMode;
      if ("date".equals(mode)) {
        intMode = DateTimePrompt.Type.DATE;
      } else if ("month".equals(mode)) {
        intMode = DateTimePrompt.Type.MONTH;
      } else if ("week".equals(mode)) {
        intMode = DateTimePrompt.Type.WEEK;
      } else if ("time".equals(mode)) {
        intMode = DateTimePrompt.Type.TIME;
      } else if ("datetime-local".equals(mode)) {
        intMode = DateTimePrompt.Type.DATETIME_LOCAL;
      } else {
        return null;
      }

      final String defaultValue = info.getString("value");
      final String minValue = info.getString("min");
      final String maxValue = info.getString("max");
      return new DateTimePrompt(
          info.getString("id"),
          info.getString("title"),
          intMode,
          defaultValue,
          minValue,
          maxValue,
          observer);
    }

    @Override
    public GeckoResult<PromptResponse> callDelegate(
        final DateTimePrompt prompt, final GeckoSession session, final PromptDelegate delegate) {
      return delegate.onDateTimePrompt(session, prompt);
    }
  }

  private static final class FileHandler implements PromptHandler<FilePrompt> {
    @Override
    public FilePrompt newPrompt(final GeckoBundle info, final Observer observer) {
      final String mode = info.getString("mode");
      final int intMode;
      if ("single".equals(mode)) {
        intMode = FilePrompt.Type.SINGLE;
      } else if ("multiple".equals(mode)) {
        intMode = FilePrompt.Type.MULTIPLE;
      } else {
        return null;
      }

      final String[] mimeTypes = info.getStringArray("mimeTypes");
      final int capture = info.getInt("capture");
      return new FilePrompt(
          info.getString("id"), info.getString("title"), intMode, capture, mimeTypes, observer);
    }

    @Override
    public GeckoResult<PromptResponse> callDelegate(
        final FilePrompt prompt, final GeckoSession session, final PromptDelegate delegate) {
      return delegate.onFilePrompt(session, prompt);
    }
  }

  private static final class PopupHandler implements PromptHandler<PopupPrompt> {
    @Override
    public PopupPrompt newPrompt(final GeckoBundle info, final Observer observer) {
      return new PopupPrompt(info.getString("id"), info.getString("targetUri"), observer);
    }

    @Override
    public GeckoResult<PromptResponse> callDelegate(
        final PopupPrompt prompt, final GeckoSession session, final PromptDelegate delegate) {
      return delegate.onPopupPrompt(session, prompt);
    }
  }

  private static final class RepostHandler implements PromptHandler<RepostConfirmPrompt> {
    @Override
    public RepostConfirmPrompt newPrompt(final GeckoBundle info, final Observer observer) {
      return new RepostConfirmPrompt(info.getString("id"), observer);
    }

    @Override
    public GeckoResult<PromptResponse> callDelegate(
        final RepostConfirmPrompt prompt,
        final GeckoSession session,
        final PromptDelegate delegate) {
      return delegate.onRepostConfirmPrompt(session, prompt);
    }
  }

  private static final class ShareHandler implements PromptHandler<SharePrompt> {
    @Override
    public SharePrompt newPrompt(final GeckoBundle info, final Observer observer) {
      return new SharePrompt(
          info.getString("id"),
          info.getString("title"),
          info.getString("text"),
          info.getString("uri"),
          observer);
    }

    @Override
    public GeckoResult<PromptResponse> callDelegate(
        final SharePrompt prompt, final GeckoSession session, final PromptDelegate delegate) {
      return delegate.onSharePrompt(session, prompt);
    }
  }

  private static final class LoginSaveHandler
      implements PromptHandler<AutocompleteRequest<LoginSaveOption>> {
    @Override
    public AutocompleteRequest<LoginSaveOption> newPrompt(
        final GeckoBundle info, final Observer observer) {
      final int hint = info.getInt("hint");
      final GeckoBundle[] loginBundles = info.getBundleArray("logins");

      if (loginBundles == null) {
        return null;
      }

      final Autocomplete.LoginSaveOption[] options =
          new Autocomplete.LoginSaveOption[loginBundles.length];

      for (int i = 0; i < options.length; ++i) {
        options[i] =
            new Autocomplete.LoginSaveOption(new Autocomplete.LoginEntry(loginBundles[i]), hint);
      }

      return new AutocompleteRequest<>(info.getString("id"), options, observer);
    }

    @Override
    public GeckoResult<PromptResponse> callDelegate(
        final AutocompleteRequest<LoginSaveOption> prompt,
        final GeckoSession session,
        final PromptDelegate delegate) {
      return delegate.onLoginSave(session, prompt);
    }
  }

  private static final class CreditCardSaveHandler
      implements PromptHandler<AutocompleteRequest<CreditCardSaveOption>> {
    @Override
    public AutocompleteRequest<CreditCardSaveOption> newPrompt(
        final GeckoBundle info, final Observer observer) {
      final int hint = info.getInt("hint");
      final GeckoBundle[] creditCardBundles = info.getBundleArray("creditCards");

      if (creditCardBundles == null) {
        return null;
      }

      final Autocomplete.CreditCardSaveOption[] options =
          new Autocomplete.CreditCardSaveOption[creditCardBundles.length];

      for (int i = 0; i < options.length; ++i) {
        options[i] =
            new Autocomplete.CreditCardSaveOption(
                new Autocomplete.CreditCard(creditCardBundles[i]), hint);
      }

      return new PromptDelegate.AutocompleteRequest<>(info.getString("id"), options, observer);
    }

    @Override
    public GeckoResult<PromptResponse> callDelegate(
        final AutocompleteRequest<CreditCardSaveOption> prompt,
        final GeckoSession session,
        final PromptDelegate delegate) {
      return delegate.onCreditCardSave(session, prompt);
    }
  }

  private static final class AddressSaveHandler
      implements PromptHandler<AutocompleteRequest<AddressSaveOption>> {
    @Override
    public AutocompleteRequest<AddressSaveOption> newPrompt(
        final GeckoBundle info, final Observer observer) {
      final GeckoBundle[] addressBundles = info.getBundleArray("addresses");

      if (addressBundles == null) {
        return null;
      }

      final Autocomplete.AddressSaveOption[] options =
          new Autocomplete.AddressSaveOption[addressBundles.length];

      final int hint = info.getInt("hint");
      for (int i = 0; i < options.length; ++i) {
        options[i] =
            new Autocomplete.AddressSaveOption(new Autocomplete.Address(addressBundles[i]), hint);
      }

      return new AutocompleteRequest<>(info.getString("id"), options, observer);
    }

    @Override
    public GeckoResult<PromptResponse> callDelegate(
        final AutocompleteRequest<AddressSaveOption> prompt,
        final GeckoSession session,
        final PromptDelegate delegate) {
      return delegate.onAddressSave(session, prompt);
    }
  }

  private static final class LoginSelectHandler
      implements PromptHandler<AutocompleteRequest<LoginSelectOption>> {
    @Override
    public AutocompleteRequest<LoginSelectOption> newPrompt(
        final GeckoBundle info, final Observer observer) {
      final GeckoBundle[] optionBundles = info.getBundleArray("options");

      if (optionBundles == null) {
        return null;
      }

      final Autocomplete.LoginSelectOption[] options =
          new Autocomplete.LoginSelectOption[optionBundles.length];

      for (int i = 0; i < options.length; ++i) {
        options[i] = Autocomplete.LoginSelectOption.fromBundle(optionBundles[i]);
      }

      return new AutocompleteRequest<>(info.getString("id"), options, observer);
    }

    @Override
    public GeckoResult<PromptResponse> callDelegate(
        final AutocompleteRequest<LoginSelectOption> prompt,
        final GeckoSession session,
        final PromptDelegate delegate) {
      return delegate.onLoginSelect(session, prompt);
    }
  }

  private static final class CreditCardSelectHandler
      implements PromptHandler<AutocompleteRequest<CreditCardSelectOption>> {
    @Override
    public AutocompleteRequest<CreditCardSelectOption> newPrompt(
        final GeckoBundle info, final Observer observer) {
      final GeckoBundle[] optionBundles = info.getBundleArray("options");

      if (optionBundles == null) {
        return null;
      }

      final Autocomplete.CreditCardSelectOption[] options =
          new Autocomplete.CreditCardSelectOption[optionBundles.length];

      for (int i = 0; i < options.length; ++i) {
        options[i] = Autocomplete.CreditCardSelectOption.fromBundle(optionBundles[i]);
      }

      return new AutocompleteRequest<>(info.getString("id"), options, observer);
    }

    @Override
    public GeckoResult<PromptResponse> callDelegate(
        final AutocompleteRequest<CreditCardSelectOption> prompt,
        final GeckoSession session,
        final PromptDelegate delegate) {
      return delegate.onCreditCardSelect(session, prompt);
    }
  }

  private static final class AddressSelectHandler
      implements PromptHandler<AutocompleteRequest<AddressSelectOption>> {
    @Override
    public AutocompleteRequest<AddressSelectOption> newPrompt(
        final GeckoBundle info, final Observer observer) {
      final GeckoBundle[] optionBundles = info.getBundleArray("options");

      if (optionBundles == null) {
        return null;
      }

      final Autocomplete.AddressSelectOption[] options =
          new Autocomplete.AddressSelectOption[optionBundles.length];

      for (int i = 0; i < options.length; ++i) {
        options[i] = Autocomplete.AddressSelectOption.fromBundle(optionBundles[i]);
      }

      return new AutocompleteRequest<>(info.getString("id"), options, observer);
    }

    @Override
    public GeckoResult<PromptResponse> callDelegate(
        final AutocompleteRequest<AddressSelectOption> prompt,
        final GeckoSession session,
        final PromptDelegate delegate) {
      return delegate.onAddressSelect(session, prompt);
    }
  }

  private static class PromptHandlers {
    final Map<String, PromptHandler<?>> mPromptHandlers = new HashMap<>();

    public void register(final PromptHandler<?> handler, final String type) {
      mPromptHandlers.put(type, handler);
    }

    public PromptHandler<?> handlerFor(final String type) {
      return mPromptHandlers.get(type);
    }
  }

  private static final PromptHandlers sPromptHandlers = new PromptHandlers();

  static {
    sPromptHandlers.register(new AlertHandler(), "alert");
    sPromptHandlers.register(new BeforeUnloadHandler(), "beforeUnload");
    sPromptHandlers.register(new ButtonHandler(), "button");
    sPromptHandlers.register(new TextHandler(), "text");
    sPromptHandlers.register(new AuthHandler(), "auth");
    sPromptHandlers.register(new ChoiceHandler(), "choice");
    sPromptHandlers.register(new ColorHandler(), "color");
    sPromptHandlers.register(new DateTimeHandler(), "datetime");
    sPromptHandlers.register(new FileHandler(), "file");
    sPromptHandlers.register(new PopupHandler(), "popup");
    sPromptHandlers.register(new RepostHandler(), "repost");
    sPromptHandlers.register(new ShareHandler(), "share");
    sPromptHandlers.register(new LoginSaveHandler(), "Autocomplete:Save:Login");
    sPromptHandlers.register(new CreditCardSaveHandler(), "Autocomplete:Save:CreditCard");
    sPromptHandlers.register(new AddressSaveHandler(), "Autocomplete:Save:Address");
    sPromptHandlers.register(new LoginSelectHandler(), "Autocomplete:Select:Login");
    sPromptHandlers.register(new CreditCardSelectHandler(), "Autocomplete:Select:CreditCard");
    sPromptHandlers.register(new AddressSelectHandler(), "Autocomplete:Select:Address");
  }
}
