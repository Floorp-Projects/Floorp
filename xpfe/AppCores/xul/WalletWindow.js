var walletCore;

function StartUp(windowName)
{
	dump("\nDoing " + windowName + " startup...\n");
	walletCore = XPAppCoresManager.Find("WalletCore");  
	dump("Looking up WalletCore...\n");
	if (walletCore == null)
	{
		dump("Creating WalletCore...\n");
		walletCore = new WalletCore();
		if (walletCore != null)
		{
			dump("WalletCore has been created.\n");
			walletCore.Init("WalletCore");
		}
		else
		{
			dump("WalletCore was not created");
		}
	}
	else
	{
		dump("WalletCore has already been created! Hurrah!\n");
	}
	if (walletCore != null && windowName != "Top" && windowName != "Bottom")
	{
		walletCore.PanelLoaded(window);
	}
}

function DoSave(results)
{
	walletCore.SaveWallet(results);
}

function DoCancel()
{
	walletCore.CancelWallet();
}
