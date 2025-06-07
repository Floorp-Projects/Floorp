import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { useTranslation } from "react-i18next";

export function BackupSettings() {
    const { t } = useTranslation();

    return (
        <Card>
            <CardHeader>
                <CardTitle>{t("workspaces.backup")}</CardTitle>
            </CardHeader>
            <CardContent className="space-y-6">
                <div>
                    <h3 className="text-base font-medium">{t("workspaces.backupComingSoon")}</h3>
                    <p className="text-sm text-muted-foreground mt-1">
                        {t("workspaces.backupDescription")}
                    </p>
                </div>

                <div className="p-3 bg-muted rounded-lg">
                    <p className="text-sm">
                        {t("workspaces.backupComingSoon")}
                    </p>
                </div>
            </CardContent>
        </Card>
    );
}