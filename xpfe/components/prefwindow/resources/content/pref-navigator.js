

function viewSignons()
{
  window.openDialog("chrome://wallet/content/SignonViewer.xul","","modal=yes,chrome,resizable=no");
}

function viewWallet()
{
  window.openDialog("chrome://wallet/content/WalletEditor.xul","","modal=yes,chrome,resizable=no");
}

function changePasswords()
{
  wallet = Components.classes['@mozilla.org/wallet;1'];
  wallet = wallet.getService();
  wallet = wallet.QueryInterface(Components.interfaces.nsIWalletService);
  wallet.WALLET_ChangePassword();
}
