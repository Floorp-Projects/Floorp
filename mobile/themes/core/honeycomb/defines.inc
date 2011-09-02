%filter substitution

%define color_background_active #fff
%define color_text_active #222222
%define color_background_default_window #fff
%define color_background_default rgba(255,255,255,0.95)
%define color_text_default #222222
%define color_divider_border #333333
%define color_button_border rgb(207,207,207)
%define color_background_dialog #fff
%define color_text_dialog #000
%define color_dialog_border #5a5a5a
%define color_background_dlgbuttons #9a9a9a
%define color_background_panel #d6d6d6
%define color_text_panel #000
%define color_background_header #6699ff
%define color_text_header #fff
%define color_background_scroller #9a9a9a
%define color_background_textbox #fff
%define color_text_textbox #000
%define color_text_button #000
%define color_text_disabled #808080
%define color_text_placeholder #808080
%define color_text_as_link #febc2b
%define color_background_panelrow #c8c8c8
%define color_text_panelrow #222222

%define color_background_highlight #a7c5f4
%define color_background_highlight_overlay rgba(167, 197, 244, 0.8)
%define color_text_highlight #000
%define color_selection #c0e49a
%define color_shadow #6699ff
%define color_shadow_light rgba(102,153,255, 0.2)

%define color_subtext_default #aaaaaa

%ifdef ANDROID
%define font_xlarge 6.08mozmm
%define font_xnormal 3.75mozmm
%define font_normal 3.54mozmm
%define font_snormal 3mozmm
%define font_small 2.91mozmm
%define font_xsmall 2.69mozmm
%define font_tiny 2.48mozmm
%define font_xtiny 2.27mozmm

%define touch_row 10.41mozmm
%define touch_button_xlarge 10.62mozmm
%define touch_button_large 9.77mozmm
%define touch_button_small 8.93mozmm
%define touch_button_minwidth 11.86mozmm
%define touch_action_minwidth 21.17mozmm
%define touch_normal 6.77mozmm

%define margin_context_popup 3.39mozmm
%define margin_large 2.54mozmm
%define margin_xxxnormal 1.69mozmm
%define margin_xnormal 1.06mozmm
%define margin_normal 0.85mozmm
%define margin_snormal 0.64mozmm
%define margin_small 0.42mozmm
%define margin_tiny 0.21mozmm
%define margin_xtiny 0.11mozmm

%define padding_xlarge 3.39mozmm
%define padding_large 2.54mozmm
%define padding_xxxnormal 1.69mozmm
%define padding_xxnormal 1.27mozmm
%define padding_xnormal 1.06mozmm
%define padding_normal 0.85mozmm
%define padding_snormal 0.64mozmm
%define padding_small 0.42mozmm
%define padding_xsmall 0.21mozmm
%define padding_tiny 0.11mozmm

%define border_width_xxlarge 0.64mozmm
%define border_width_xlarge 0.42mozmm
%define border_width_large 0.32mozmm
%define border_width_small 0.21mozmm
%define border_width_tiny 0.11mozmm

%define border_radius_normal 0.85mozmm
%define border_radius_small 0.64mozmm
%define border_radius_tiny 0.21mozmm

%define shadow_width_xlarge 3.06mozmm
%define shadow_width_large 2.64mozmm
%define shadow_width_medium 0.75mozmm
%define shadow_width_small 0.21mozmm
%define shadow_width_tiny 0.1mozmm

%define textbox_height 5.08mozmm

%define dropmarker_padding 0.53mozmm

%define progressmeter_height 3.39mozmm

%define urlbar_edit_height 6.35mozmm
%define urlbar_edit_indent 0.85mozmm
%define urlbar_max_width   96mozmm

%define scroller_thickness 0.64mozmm
%define scroller_minimum 1.27mozmm

%define sidebar_width_minimum 8.47mozmm
%define sidebar_button_height 7.41mozmm
%define documenttab_margin_bottom 0.85mozmm

%define placelabel_padding 8.47mozmm
%define placeitem_padding 4.23mozmm

%define awesome_header_maxwidth 25mozmm
%define autocomplete_item_container_image_padding 0.53mozmm
%define autocomplete_item_container_position 0.21mozmm
%define autocomplete_item_container_size 4mozmm
%define autocomplete_item_container_padding 5.08mozmm

%define autocomplete_item_subtitle_margin 2.75mozmm
%define autocomplete_item_label_margin 3.18mozmm
%define autocomplete_item_tags_margin 3.39mozmm

%define autocompleteresult_padding 0.53mozmm

%define dialog_width 76.2mozmm

%define appmenu_portrait_height 21.17mozmm
%define appmenu_button_height  10.48mozmm

%define tablet_panel_controls  40mozmm
%define tablet_panel_minwidth  124mozmm
%else
%define font_xlarge 48px
%define font_xnormal 26px
%define font_normal 24px
%define font_snormal 20px
%define font_small 18px
%define font_xsmall 16px
%define font_tiny 14px
%define font_xtiny 12px

%define touch_row 70px
%define touch_button_xlarge 72px
%define touch_button_large 64px
%define touch_button_small 56px
%define touch_button_minwidth 112px
%define touch_action_minwidth 200px
%define touch_normal 64px

%define margin_context_popup 32px
%define margin_large 24px
%define margin_xxxnormal 16px
%define margin_xnormal 10px
%define margin_normal 8px
%define margin_snormal 6px
%define margin_small 4px
%define margin_tiny 2px
%define margin_xtiny 1px

%define padding_xlarge 32px
%define padding_large 24px
%define padding_xxxnormal 16px
%define padding_xxnormal 12px
%define padding_xnormal 10px
%define padding_normal 8px
%define padding_snormal 6px
%define padding_small 4px
%define padding_xsmall 2px
%define padding_tiny 1px

%define border_width_xxlarge 6px
%define border_width_xlarge 4px
%define border_width_large 3px
%define border_width_small 2px
%define border_width_tiny 1px

%define border_radius_normal 8px
%define border_radius_small 6px
%define border_radius_tiny 2px

%define shadow_width_xlarge 15px
%define shadow_width_large 6px
%define shadow_width_medium 4px
%define shadow_width_small 2px
%define shadow_width_tiny 1px

%define textbox_height 48px

%define dropmarker_margin 5px

%define progressmeter_height 32px

%define urlbar_edit_height 60px
%define urlbar_edit_indent 8px
%define urlbar_max_width 596px

%define scroller_thickness 6px
%define scroller_minimum 12px

%define sidebar_width_minimum 80px
%define sidebar_button_height 70px
%define documenttab_margin_bottom 8px

%define placelabel_padding 80px
%define placeitem_padding 40px

%define awesome_header_maxwidth 100px
%define autocomplete_item_container_image_padding 5px
%define autocomplete_item_container_position 2px
%define autocomplete_item_container_size 26px
%define autocomplete_item_container_padding 48px

%define autocomplete_item_subtitle_margin 26px
%define autocomplete_item_label_margin 30px
%define autocomplete_item_tags_margin 32px

%define autocompleteresult_padding 5px

%define dialog_width 500px

%define appmenu_portrait_height 200px
%define appmenu_button_height 99px

%define tablet_panel_controls  270px
%define tablet_panel_minwidth  801px
%endif

%ifdef MOZ_PLATFORM_MAEMO
%define orientation -moz-device-orientation
%elifdef ANDROID
%define orientation -moz-device-orientation
%else
%define orientation orientation
%endif
