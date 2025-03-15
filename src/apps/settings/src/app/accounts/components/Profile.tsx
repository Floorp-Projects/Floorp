import {
  Card,
  CardContent,
  CardHeader,
  CardTitle,
} from "@/components/common/card";
import { Separator } from "@/components/common/separator";
import { useTranslation } from "react-i18next";
import type { AccountsFormData } from "@/types/pref";
import { ExternalLink, User } from "lucide-react";

type ProfileProps = {
  accountAndProfileData: AccountsFormData | null;
};

export function Profile({ accountAndProfileData }: ProfileProps) {
  const { t } = useTranslation();

  return (
    <Card>
      <CardHeader>
        <CardTitle className="flex items-center gap-2">
          <User className="size-5" />
          {t("accounts.profileManagement")}
        </CardTitle>
      </CardHeader>
      <CardContent className="space-y-4">
        <p className="text-base-content/90">
          {t("accounts.profileManagementDescription")}
        </p>

        <div className="space-y-2">
          <p className="text-base-content/90">
            {t("accounts.currentProfileName", {
              name: accountAndProfileData?.profileName,
            })}
          </p>

          <p className="text-base-content/90">
            {t("accounts.profileSaveLocation", {
              path: accountAndProfileData?.profileDir,
            })}
          </p>
        </div>

        <div className="flex gap-4">
          <a
            href="#"
            onClick={(e) => {
              e.preventDefault();
              window.NRAddTab("about:profiles");
            }}
            className="text-[var(--link-text-color)] hover:underline inline-flex items-center gap-2"
          >
            {t("accounts.openProfileManager")}
            <ExternalLink className="size-4" />
          </a>
          <a
            href="#"
            className="text-[var(--link-text-color)] hover:underline inline-flex items-center gap-2"
          >
            {t("accounts.openProfileSaveLocation")}
            <ExternalLink className="size-4" />
          </a>
        </div>
      </CardContent>
    </Card>
  );
}
