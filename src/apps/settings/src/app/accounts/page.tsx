import { useEffect, useState } from "react";
import { useTranslation } from "react-i18next";
import { Profile } from "./components/profile";
import { Accounts } from "./components/accounts";
import { FormProvider, useForm, useWatch } from "react-hook-form";
import { useAccountAndProfileData } from "./dataManager";
import type { AccountsFormData } from "@/types/pref";

export default function Page() {
  const { t } = useTranslation();
  const methods = useForm<AccountsFormData>({
    defaultValues: {},
  });

  const { setValue } = methods;
  const watchAll = useWatch({
    control: methods.control,
  });

  const [accountAndProfileData, setAccountAndProfileData] = useState<
    AccountsFormData | null
  >(null);

  useEffect(() => {
    async function fetchAccountAndProfileData() {
      const data = await useAccountAndProfileData();
      setAccountAndProfileData(data);
    }
    fetchAccountAndProfileData();
  }, []);

  useEffect(() => {
    if (accountAndProfileData?.asyncNoesViaMozillaAccount) {
      setValue(
        "asyncNoesViaMozillaAccount",
        accountAndProfileData.asyncNoesViaMozillaAccount,
      );
    }
  }, [accountAndProfileData, setValue]);

  return (
    <div className="p-6 space-y-3">
      <div className="flex flex-col items-start pl-6">
        <h1 className="text-3xl font-bold mb-2">
          {t("accounts.profileAndAccount")}
        </h1>
        <p className="text-sm mb-8">{t("accounts.profileDescription")}</p>
      </div>

      <div className="grid grid-cols-1 gap-4 pl-6">
        <FormProvider {...methods}>
          <Accounts accountAndProfileData={accountAndProfileData} />
          <Profile accountAndProfileData={accountAndProfileData} />
        </FormProvider>
      </div>
    </div>
  );
}
