# 担当B最終matrix

|key|実施試験|分類|残条件|PR|Jira comment ID|
|---|---|---|---|---|---|
|KAN-3|PYTHONPATH=python python3 run.py floorp 29284 split|実再現・CSS欠陥修正Draft|元Fedora desktop/package・物理ドラッグ/メニュー; native Wayland adoption; full production build; 追加test head e7dda31fの標準CI（初期headの282modules CIは成功）|https://github.com/Floorp-Projects/Floorp/pull/3033|10294|
|KAN-5|PYTHONPATH=python python3 run.py floorp 29284 desktop-drag|限定条件で未再現・既存修正と整合|Fedora44実desktop/元package/Flatpak; 物理入力; 既存窓へのproxyドラッグ（native adoptのみ実施）|https://github.com/Floorp-Projects/Floorp/pull/2763|10283|
|KAN-29|dbus-run-session -- bash nested.sh (isolated KWin + Floorp visual/controls/layout)|限定条件で未再現|CachyOS/Flatpak; panel本体open・反復hover・ピン留め差; PR2877適用時のRTL/boxless比較|https://github.com/Floorp-Projects/Floorp/pull/2877|10284|
|KAN-31|PYTHONPATH=python python3 run.py floorp 29284 search|限定条件で未再現|Fedora44実desktop/元package/元配布形式; 起動直後のより早期入力/他言語|—|10285|
|KAN-39|PYTHONPATH=python python3 run.py floorp 29284 wheel|正規化実測・原症状の解消は未確定|Fedora44実desktop/元package/物理wheel・DPI/加速; 通常tabbarとの有効な同入力距離比較|https://github.com/Floorp-Projects/Floorp/pull/2763|10286|
|KAN-40|PYTHONPATH=python python3 run.py floorp 29284 middle|限定条件で未再現・既存修正と整合|Fedora44実desktop/元package/Flatpak/物理中クリック; 通常タブ・stack全体中クリックの追加対照|https://github.com/Floorp-Projects/Floorp/pull/2763|10287|
|KAN-54|dbus-run-session -- bash nested.sh (isolated KWin + Floorp visual/controls/layout)|表示重複を実観測・Sprint対象外|Manjaro/完全なPlasma session; minimize/maximize含む3ボタン/仕様確認|—|10288|
|KAN-87|dbus-run-session -- bash nested-floorp-final.sh; dbus-run-session -- bash nested-firefox.sh|限定条件で未再現・既存修正と整合|CachyOS/Niri/Hyprland/MangoWM; 右panel本体の同時reveal/画像theme|https://github.com/Floorp-Projects/Floorp/pull/2767|10289|
|KAN-93|dbus-run-session -- bash nested-floorp-final.sh; dbus-run-session -- bash nested-firefox.sh|限定条件で未再現|KDE Neon/実theme・accent・font条件; 元版12.17.1/Firefox154+Lepton|—|10290|
|KAN-98|PYTHONPATH=python python3 gesture-run.py floorp 29311 gesture; PYTHONPATH=python python3 kan98-sparse-product-run.py floorp 29333 sparse-product|実再現・独立した2欠陥を限定修正Draft|Arch/Sway/Wayland/物理入力/DPI; full production build; PR3034 full CI（PR3031 full Ubuntu browser CIは成功）|https://github.com/Floorp-Projects/Floorp/pull/3031; https://github.com/Floorp-Projects/Floorp/pull/3034|10291|
