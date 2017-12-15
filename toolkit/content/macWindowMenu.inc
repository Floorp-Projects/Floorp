    <script type="application/javascript" src="chrome://global/content/macWindowMenu.js"/>
    <commandset id="baseMenuCommandSet">
        <command id="minimizeWindow"
                 label="&minimizeWindow.label;"
                 oncommand="window.minimize();" />
        <command id="zoomWindow"
                 label="&zoomWindow.label;"
                 oncommand="zoomWindow();" />
    </commandset>
    <keyset id="baseMenuKeyset">
        <key id="key_minimizeWindow"
             command="minimizeWindow"
             key="&minimizeWindow.key;"
             modifiers="accel"/>
    </keyset>
    <menu id="windowMenu"
          label="&windowMenu.label;"
          onpopupshowing="macWindowMenuDidShow();"
          onpopuphidden="macWindowMenuDidHide();"
          >
        <menupopup id="windowPopup">
            <menuitem command="minimizeWindow" key="key_minimizeWindow"/>
            <menuitem command="zoomWindow"/>
            <!-- decomment when "BringAllToFront" is implemented
                <menuseparator/>
                <menuitem label="&bringAllToFront.label;" disabled="true"/> -->
            <menuseparator id="sep-window-list"/>
        </menupopup>
    </menu>
