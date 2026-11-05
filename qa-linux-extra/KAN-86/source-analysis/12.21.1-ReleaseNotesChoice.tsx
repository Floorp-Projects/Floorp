import { Button } from "../../../../libs/ui/button.tsx";
import { RadioCard } from "@chakra-ui/react";
import { FloorpBrand } from "../../../../libs/ui/brand.tsx";
import { SelectionBadge, SetupInfo } from "./SetupControls.tsx";
import styles from "../setup.module.css";
import prompt from "./release-notes-prompt.module.css";
import {
  createContext,
  type ReactNode,
  useContext,
  useEffect,
  useState,
} from "react";
import { useTranslation } from "react-i18next";
import { rpc } from "../lib/rpc/rpc.ts";
import {
  effectiveReleaseNotesMode,
  initialReleaseNotesChoice,
  RELEASE_NOTES_MODES,
  RELEASE_NOTES_PREFS,
  RELEASE_NOTES_SUPPORT_URL,
  type ReleaseNotesMode,
} from "../../../modules/common/release-notes.ts";

export function useReleaseNotesChoice(allowNewDefault = false) {
  const [mode, setMode] = useState<ReleaseNotesMode>("blocking");
  const [currentMode, setCurrentMode] = useState<ReleaseNotesMode | null>(null);
  const [ready, setReady] = useState(false);
  const [saving, setSaving] = useState(false);
  const [error, setError] = useState(false);

  useEffect(() => {
    let active = true;
    const load = async () => {
      try {
        const [value, confirmed] = await Promise.all([
          rpc.getStringPref(RELEASE_NOTES_PREFS.mode),
          rpc.getBoolPref(RELEASE_NOTES_PREFS.confirmed),
        ]);
        if (!active) return;
        setCurrentMode(effectiveReleaseNotesMode(value, confirmed));
        setMode(
          initialReleaseNotesChoice(
            value,
            confirmed,
            allowNewDefault ? "new" : "existing",
          ),
        );
        setReady(true);
      } catch {
        if (active) setError(true);
      }
    };
    void load();
    return () => {
      active = false;
    };
  }, [allowNewDefault]);

  async function confirm() {
    if (!ready || saving) return false;
    setSaving(true);
    setError(false);
    try {
      await rpc.setStringPref(RELEASE_NOTES_PREFS.mode, mode);
      await rpc.setBoolPref(RELEASE_NOTES_PREFS.confirmed, true);
      setCurrentMode(mode);
      return true;
    } catch {
      setError(true);
      return false;
    } finally {
      setSaving(false);
    }
  }

  return { mode, setMode, currentMode, ready, saving, error, confirm };
}

const SetupReleaseNotesContext = createContext<
  {
    choice: ReturnType<typeof useReleaseNotesChoice>;
    reviewed: boolean;
    setReviewed: (reviewed: boolean) => void;
  } | null
>(null);

export function SetupReleaseNotesProvider(
  { children }: { children: ReactNode },
) {
  const choice = useReleaseNotesChoice(true);
  const [reviewed, setReviewed] = useState(false);
  return (
    <SetupReleaseNotesContext.Provider
      value={{ choice, reviewed, setReviewed }}
    >
      {children}
    </SetupReleaseNotesContext.Provider>
  );
}

export function useSetupReleaseNotesChoice() {
  const context = useContext(SetupReleaseNotesContext);
  if (!context) throw new Error("Missing SetupReleaseNotesProvider");
  return context;
}

export function ReleaseNotesPrompt() {
  const { t } = useTranslation();
  const choice = useReleaseNotesChoice();
  function dismiss() {
    // deno-lint-ignore no-window
    if (typeof window.NRDismissWelcomePage === "function") {
      // deno-lint-ignore no-window
      window.NRDismissWelcomePage();
    } else {
      // Older runtimes must never fall back to closing the last window.
      globalThis.location.replace("about:newtab");
    }
  }
  return (
    <div className={`${styles.frame} ${prompt.frame}`}>
      <a className="floorp-skip" href="#release-notes-content">
        {t("ui.skipToContent", { defaultValue: "Skip to content" })}
      </a>
      <header className={styles.header}>
        <div className={styles.brand}>
          <FloorpBrand onDark />
        </div>
      </header>
      <main className={styles.layout}>
        <section
          className={styles.explanation}
          aria-labelledby="release-notes-title"
        >
          <h1 id="release-notes-title">{t("releaseNotes.prompt.title")}</h1>
          <p>{t("releaseNotes.prompt.description")}</p>
          <p>{t("releaseNotes.prompt.hint")}</p>
        </section>
        <section
          id="release-notes-content"
          tabIndex={-1}
          className={styles.pane}
          aria-labelledby="release-notes-choice-title"
        >
          <div>
            <h2 id="release-notes-choice-title">
              {t("releaseNotes.prompt.choiceTitle")}
            </h2>
            <p className={`${styles.muted} ${prompt.intro}`}>
              {t("releaseNotes.prompt.choiceDescription")}
            </p>
            {!choice.ready && !choice.error && (
              <p role="status">{t("ui.loading")}</p>
            )}
            <RadioCard.Root
              unstyled
              name="release-notes-choice"
              value={choice.mode}
              disabled={!choice.ready || choice.saving}
              aria-labelledby="release-notes-choice-title"
              className={styles.radioStack}
              onValueChange={({ value }) => {
                const mode = RELEASE_NOTES_MODES.find((mode) => mode === value);
                if (mode) choice.setMode(mode);
              }}
            >
              {RELEASE_NOTES_MODES.map((mode) => (
                <RadioCard.Item
                  key={mode}
                  value={mode}
                  className={styles.radioItem}
                >
                  <RadioCard.ItemHiddenInput checked={choice.mode === mode} />
                  <RadioCard.ItemControl className={styles.radioControl}>
                    <RadioCard.ItemContent>
                      <RadioCard.ItemText>
                        {t(`setupV5.supportLabels.${mode}`)}
                        {choice.mode === mode && <SelectionBadge />}
                      </RadioCard.ItemText>
                      <RadioCard.ItemDescription
                        className={styles.radioDescription}
                      >
                        {t(`releaseNotes.${mode}.description`)}
                      </RadioCard.ItemDescription>
                    </RadioCard.ItemContent>
                  </RadioCard.ItemControl>
                </RadioCard.Item>
              ))}
            </RadioCard.Root>
            <SetupInfo>
              <strong>{t("setupV5.scopeTitle")}</strong>
              <p>{t("releaseNotes.scope")}</p>
              <a
                href={RELEASE_NOTES_SUPPORT_URL}
                target="_blank"
                rel="noopener noreferrer"
              >
                {t("releaseNotes.learnMore")}
              </a>
            </SetupInfo>
            <p className={styles.muted}>
              {choice.currentMode !== null && (
                <>
                  <strong id="release-notes-current-setting">
                    {t("releaseNotes.currentSetting", {
                      mode: t(`setupV5.supportLabels.${choice.currentMode}`),
                    })}
                  </strong>
                  <br />
                </>
              )}
              {t("releaseNotes.applyOnConfirm")}
            </p>
            {choice.error && (
              <p role="alert" className={styles.error}>
                {t("releaseNotes.error")}
              </p>
            )}
            <div className={prompt.actions}>
              <Button
                type="button"
                variant="secondary"
                disabled={choice.saving}
                onClick={dismiss}
              >
                {t("releaseNotes.prompt.dismiss")}
              </Button>
              <Button
                type="button"
                disabled={!choice.ready || choice.saving}
                onClick={async () => {
                  if (await choice.confirm()) dismiss();
                }}
              >
                {t("releaseNotes.confirm")}
              </Button>
            </div>
          </div>
        </section>
      </main>
    </div>
  );
}
