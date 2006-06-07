    <toolbar id="FindToolbar" hidden="true" align="center" fullscreentoolbar="true">
      <toolbarbutton id="find-closebutton" tooltiptext="&findCloseButton.tooltip;"
                     oncommand="gFindBar.closeFindBar();"/>

      <label control="find-field" id="find-label" class="find-fast"/>
      <hbox id="find-field-container" class="find-fast">
        <textbox id="find-field" oninput="gFindBar.find(this.value);"
                onkeypress="gFindBar.onFindBarKeyPress(event);"
                onblur="gFindBar.onFindBarBlur();"
                oncompositionstart="gFindBar.onFindBarCompositionStart(event);"
                oncompositionend="gFindBar.onFindBarCompositionEnd(event);"/>
      </hbox>
      <toolbarbutton id="find-next" label="&next.label;" tooltiptext="&next.tooltip;"
                     command="cmd_findAgain" disabled="true" accesskey="&next.accesskey;"/>
      <toolbarbutton id="find-previous" label="&previous.label;" tooltiptext="&previous.tooltip;"
                     command="cmd_findPrevious" disabled="true" accesskey="&previous.accesskey;"/>
      <toolbarbutton id="highlight"
                     label="&highlight.label;"
#ifdef XP_MACOSX
                     tooltiptext="&highlight.macTooltip;"
#else
                     tooltiptext="&highlight.tooltip;"
#endif
                     oncommand="gFindBar.toggleHighlight(!this.checked);
                                if (gFindBar.mFindMode != FIND_NORMAL)
                                  gFindBar.setFindCloseTimeout();"
                     type="checkbox" disabled="true"
                     accesskey="&highlight.accesskey;"/>
      <checkbox id="find-case-sensitive"
                oncommand="gFindBar.toggleCaseSensitivity(this.checked);"
                label="&caseSensitiveCheckbox.label;" accesskey="&caseSensitiveCheckbox.accesskey;"/>
      <image id="find-status-icon"/>
      <label id="find-status"/>
    </toolbar>
