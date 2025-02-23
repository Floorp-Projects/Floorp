import { useTranslation } from "react-i18next"
import { useForm, Controller } from "react-hook-form"
import { AlertCircle } from "lucide-react"
import { useState, type ChangeEvent } from "react"
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card.tsx"

export default function Preferences() {
    const { t } = useTranslation()
    const { control } = useForm()
    const [isRestartDialogOpen, setIsRestartDialogOpen] = useState(false)

    const handleCheckboxChange = (onChange: (value: boolean) => void) => (e: ChangeEvent<HTMLInputElement>) => {
        const newValue = e.currentTarget.checked
        setIsRestartDialogOpen(true)
        onChange(newValue)
    }

    return (
        <>
            <dialog open={isRestartDialogOpen} onClose={() => setIsRestartDialogOpen(false)}>
                <div className="p-4">
                    <h2 className="text-lg font-semibold">{t("common.restart.required")}</h2>
                    <p className="mt-2">
                        {t("panelSidebar.needRestartDescriptionForEnableAndDisable")}
                    </p>
                </div>
            </dialog>

            <Card>
                <CardHeader>
                    <CardTitle className="flex items-center gap-2">
                        {t("panelSidebar.basicSettings")}
                    </CardTitle>
                </CardHeader>

                <CardContent className="space-y-6">
                    <div className="space-y-4">
                        <h3 className="text-lg font-medium">{t("panelSidebar.enableOrDisable")}</h3>
                        <div className="flex items-center justify-between space-x-2">
                            <label htmlFor="enabled">{t("panelSidebar.enablePanelSidebar")}</label>
                            <Controller
                                name="enabled"
                                control={control}
                                render={({ field: { onChange, value } }) => (
                                    <input
                                        id="enabled"
                                        type="checkbox"
                                        checked={value}
                                        onChange={handleCheckboxChange(onChange)}
                                        className="h-4 w-4"
                                    />
                                )}
                            />
                        </div>
                    </div>

                    <hr className="border-t" />

                    <div className="space-y-4">
                        <h3 className="text-lg font-medium">{t("panelSidebar.otherSettings")}</h3>
                        <div className="space-y-4">
                            <div className="flex items-center justify-between">
                                <label htmlFor="autoUnload">{t("panelSidebar.autoUnloadOnClose")}</label>
                                <Controller
                                    name="autoUnload"
                                    control={control}
                                    render={({ field: { onChange, value } }) => (
                                        <input
                                            id="autoUnload"
                                            type="checkbox"
                                            checked={value}
                                            onChange={(e: ChangeEvent<HTMLInputElement>) => onChange(e.currentTarget.checked)}
                                            className="h-4 w-4"
                                        />
                                    )}
                                />
                            </div>

                            <div className="space-y-2">
                                <h4 className="font-medium">{t("panelSidebar.position")}</h4>
                                <Controller
                                    name="position_start"
                                    control={control}
                                    render={({ field: { onChange, value } }) => (
                                        <div className="grid grid-cols-2 gap-4">
                                            <div className="flex items-center space-x-2">
                                                <input
                                                    type="radio"
                                                    id="position-end"
                                                    checked={!value}
                                                    onChange={(e: ChangeEvent<HTMLInputElement>) => onChange(!e.currentTarget.checked)}
                                                    className="h-4 w-4"
                                                />
                                                <label htmlFor="position-end">{t("panelSidebar.positionLeft")}</label>
                                            </div>
                                            <div className="flex items-center space-x-2">
                                                <input
                                                    type="radio"
                                                    id="position-start"
                                                    checked={value}
                                                    onChange={(e: ChangeEvent<HTMLInputElement>) => onChange(e.currentTarget.checked)}
                                                    className="h-4 w-4"
                                                />
                                                <label htmlFor="position-start">{t("panelSidebar.positionRight")}</label>
                                            </div>
                                        </div>
                                    )}
                                />
                            </div>

                            <div className="space-y-2">
                                <div className="flex items-center justify-between">
                                    <label htmlFor="globalWidth">{t("panelSidebar.globalWidth")}</label>
                                    <Controller
                                        name="globalWidth"
                                        control={control}
                                        render={({ field: { onChange, value } }) => (
                                            <input
                                                id="globalWidth"
                                                type="number"
                                                value={value}
                                                className="w-[150px] px-2 py-1 border rounded"
                                                onChange={(e: ChangeEvent<HTMLInputElement>) => onChange(Number(e.currentTarget.value))}
                                            />
                                        )}
                                    />
                                </div>
                                <p className="text-sm text-muted-foreground">
                                    {t("panelSidebar.globalWidthDescription")}
                                </p>
                            </div>
                        </div>
                    </div>
                </CardContent>
            </Card>
        </>
    )
}