import React from "react";
import { FormProvider, useForm, useWatch } from "react-hook-form";
import { useTranslation } from "react-i18next";
import { BasicSettings } from "./components/BasicSettings.tsx";
import { BackupSettings } from "./components/BackupSettings.tsx";
import { getWorkspaceSettings, saveWorkspaceSettings } from "./dataManager.ts";
import type { WorkspacesFormData } from "@/types/pref.ts";

export default function Page() {
  const { t } = useTranslation();
  const methods = useForm<WorkspacesFormData>({});

  const { control, setValue } = methods;
  const watchAll = useWatch({ control });

  React.useEffect(() => {
    const fetchDefaultValues = async () => {
      const values = await getWorkspaceSettings();
      if (!values) return;

      for (const key in values) {
        setValue(key as keyof WorkspacesFormData, values[key], {
          shouldValidate: true,
        });
      }
    };

    fetchDefaultValues();
    window.addEventListener("focus", fetchDefaultValues);
    return () => {
      window.removeEventListener("focus", fetchDefaultValues);
    };
  }, [setValue]);

  React.useEffect(() => {
    if (Object.keys(watchAll).length === 0) return;

    try {
      saveWorkspaceSettings(watchAll);
    } catch (error) {
      globalThis.console?.error("Failed to save workspace settings:", error);
    }
  }, [watchAll]);

  return (
    <div className="p-6 space-y-3">
      <div className="flex flex-col items-start pl-6">
        <h1 className="text-3xl font-bold mb-2">
          {t("workspaces.workspaces")}
        </h1>
        <p className="text-sm mb-8">{t("workspaces.workspacesDescription")}</p>
      </div>

      <FormProvider {...methods}>
        <form className="space-y-3 pl-6">
          <BasicSettings />
          <BackupSettings />
        </form>
      </FormProvider>
    </div>
  );
}
