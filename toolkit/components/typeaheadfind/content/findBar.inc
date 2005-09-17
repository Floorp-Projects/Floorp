    <toolbar id="FindToolbar" hidden="true" align="center" fullscreentoolbar="true">
      <toolbarbutton id="find-closebutton" tooltiptext="&findCloseButton.tooltip;"
                     oncommand="closeFindBar();"/>

      <label control="find-field" value="&find.label;"/>
      <hbox id="find-field-container">
        <textbox id="find-field" oninput="find(this.value);"
                onkeypress="onFindBarKeyPress(event);" onblur="onFindBarBlur();"
                oncompositionstart="onFindBarCompositionStart(event);"
                oncompositionend="onFindBarCompositionEnd(event);"/>
      </hbox>
      <toolbarbutton id="find-next" label="&findNext.label;" tooltiptext="&findNext.tooltip;"
                     command="cmd_findAgain" disabled="true" accesskey="&findNext.accesskey;"/>
      <toolbarbutton id="find-previous" label="&findPrevious.label;" tooltiptext="&findPrevious.tooltip;"
                     command="cmd_findPrevious" disabled="true" accesskey="&findPrevious.accesskey;"/>
      <toolbarbutton id="highlight"
                     label="&highlight.label;"
#ifdef XP_MACOSX
                     tooltiptext="&highlight.macTooltip;"
#else
                     tooltiptext="&highlight.tooltip;"
#endif
                     oncommand="toggleHighlight(!this.checked);
                                if (gFindMode != FIND_NORMAL) setFindCloseTimeout();"
                     type="checkbox" disabled="true"
                     accesskey="&highlight.accesskey;"/>
      <checkbox id="find-case-sensitive" oncommand="toggleCaseSensitivity(this.checked);"
                label="&caseSensitiveCheckbox.label;" accesskey="&caseSensitiveCheckbox.accesskey;"/>
      <image id="find-status-icon"/>
      <label id="find-status"/>
    </toolbar>

