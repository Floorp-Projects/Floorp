import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card.tsx";
import { Input } from "@/components/ui/input.tsx";
import { useTranslation } from "react-i18next";
import { useFormContext } from "react-hook-form";

export function BasicSettings() {
    const { t } = useTranslation();
    const { watch, setValue } = useFormContext();
    const watchAll = watch();

    return (
        <Card>
            <CardHeader>
                <CardTitle>{t("panelSidebar.basicSettings")}</CardTitle>
            </CardHeader>
            <CardContent className="space-y-6">
                <EnableSection
                    enabled={watchAll.enabled}
                    onEnableChange={(checked) => {
                        setValue("enabled", checked);
                    }}
                />
                <OtherSettings
                    autoUnload={watchAll.autoUnload}
                    position_start={watchAll.position_start}
                    globalWidth={watchAll.globalWidth}
                    onSettingChange={setValue}
                />
            </CardContent>
        </Card>
    );
}

function EnableSection({ enabled, onEnableChange }: {
    enabled: boolean;
    onEnableChange: (checked: boolean) => void;
}) {
    const { t } = useTranslation();

    return (
        <div>
            <h3 className="text-base font-medium mb-2">{t("panelSidebar.enableOrDisable")}</h3>
            <div className="flex items-center justify-between">
                <div className="space-y-1">
                    <label htmlFor="enable-panel" className="text-sm font-medium leading-none peer-disabled:cursor-not-allowed peer-disabled:opacity-70">
                        {t("panelSidebar.enablePanelSidebar")}
                    </label>
                    <p className="text-sm text-muted-foreground">
                        {t("panelSidebar.enableDescription")}
                    </p>
                </div>
                <input
                    type="checkbox"
                    id="enable-panel"
                    checked={enabled}
                    onChange={(e) => onEnableChange(e.currentTarget.checked)}
                    className="h-4 w-4"
                />
            </div>
        </div>
    );
}

function OtherSettings({
    autoUnload,
    position_start,
    globalWidth,
    onSettingChange
}: {
    autoUnload: boolean;
    position_start: boolean;
    globalWidth: number;
    onSettingChange: (name: string, value: any) => void;
}) {
    const { t } = useTranslation();

    return (
        <div>
            <h3 className="text-base font-medium mb-2">{t("panelSidebar.otherSettings")}</h3>
            <div className="space-y-4">
                <div className="flex items-center justify-between">
                    <label htmlFor="auto-unload" className="text-sm font-medium leading-none">
                        {t("panelSidebar.autoUnloadOnClose")}
                    </label>
                    <input
                        type="checkbox"
                        id="auto-unload"
                        checked={autoUnload}
                        onChange={(e) => onSettingChange("autoUnload", e.target.checked)}
                        className="h-4 w-4"
                    />
                </div>

                <div className="space-y-2">
                    <label className="text-sm font-medium leading-none">{t("panelSidebar.position")}</label>
                    <div className="flex space-x-4">
                        <label className="flex items-center space-x-2">
                            <input
                                type="radio"
                                name="position"
                                value="end"
                                checked={!position_start}
                                onChange={() => onSettingChange("position_start", false)}
                                className="h-4 w-4"
                            />
                            <span>{t("panelSidebar.positionLeft")}</span>
                        </label>
                        <label className="flex items-center space-x-2">
                            <input
                                type="radio"
                                name="position"
                                value="start"
                                checked={position_start}
                                onChange={() => onSettingChange("position_start", true)}
                                className="h-4 w-4"
                            />
                            <span>{t("panelSidebar.positionRight")}</span>
                        </label>
                    </div>
                </div>

                <div className="space-y-2">
                    <label htmlFor="global-width" className="text-sm font-medium leading-none">
                        {t("panelSidebar.globalWidth")}
                    </label>
                    <Input
                        id="global-width"
                        type="number"
                        value={globalWidth || ""}
                        onChange={(e) => onSettingChange("globalWidth", Number(e.target.value))}
                        className="w-full"
                    />
                    <p className="text-sm text-muted-foreground">
                        {t("panelSidebar.globalWidthDescription")}
                    </p>
                </div>

                <div className="p-3 bg-muted rounded-lg">
                    <p className="text-sm">
                        {t("panelSidebar.iconProviderRemoved")}
                    </p>
                </div>
            </div>
        </div>
    );
}