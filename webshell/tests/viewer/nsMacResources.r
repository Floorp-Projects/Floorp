#include <types.r>

resource 'MENU' (2000, "file")
{
	2000, textMenuProc,
	allEnabled, 
	enabled, "File",
	{
		"new viewer", noicon, nokey, nomark, plain;
		"Open URLÉ", noicon, nokey, nomark, plain;
		"Sample", noicon, "\$1B", "\0D128", plain;
		"Print Preview", noicon, "\$1B", "\0D129", plain;
		"Quit", noicon, nokey, nomark, plain
	}
};

resource 'MENU' (2000+1, "edit")
{
	2000+1, textMenuProc,
	allEnabled, 
	enabled, "Edit",
	{
		"Cut", noicon, nokey, nomark, plain;
		"Copy", noicon, nokey, nomark, plain;
		"Paste", noicon, nokey, nomark, plain;
		"Select All", noicon, nokey, nomark, plain;
		"Find", noicon, nokey, nomark, plain
	}
};

resource 'MENU' (2000+2, "debug")
{
	2000+2, textMenuProc,
	allEnabled, 
	enabled, "Debug",
	{
		"Visual Debugging", noicon, nokey, nomark, plain;
		"Reflow Test", noicon, nokey, nomark, plain;
		
		"Dump Content", noicon, nokey, nomark, plain;
		"Dump Frames", noicon, nokey, nomark, plain;
		"Dump Views", noicon, nokey, nomark, plain;
		
		"Dump Style Sheets", noicon, nokey, nomark, plain;
		"Dump Style Contexts", noicon, nokey, nomark, plain;
		
		"Show Content Size", noicon, nokey, nomark, plain;
		"Show Frame Size", noicon, nokey, nomark, plain;
		"Show Style Size", noicon, nokey, nomark, plain;
		
		"Debug Save", noicon, nokey, nomark, plain;
		"Debug Toggle Selection", noicon, nokey, nomark, plain;
		
		"Debug Robot", noicon, nokey, nomark, plain;
		
		"Show Content Quality", noicon, nokey, nomark, plain
	}
};

resource 'MENU' (2000+3, "demo")
{
	128, textMenuProc,
	allEnabled, 
	enabled, "demo menu",
	{
		"demo #0", noicon, nokey, nomark, plain;
		"demo #1", noicon, nokey, nomark, plain;
		"demo #2", noicon, nokey, nomark, plain;
		"demo #3", noicon, nokey, nomark, plain;
		"demo #4", noicon, nokey, nomark, plain;
		"demo #5", noicon, nokey, nomark, plain;
		"demo #6", noicon, nokey, nomark, plain;
		"demo #7", noicon, nokey, nomark, plain;
		"demo #8", noicon, nokey, nomark, plain;
		"demo #9", noicon, nokey, nomark, plain;
		"demo #10", noicon, nokey, nomark, plain;
		"demo #11", noicon, nokey, nomark, plain;
		"demo #12", noicon, nokey, nomark, plain;
		"demo #13", noicon, nokey, nomark, plain;
	}
};

resource 'MENU' (2000+4, "print preview")
{
	129, textMenuProc,
	allEnabled, 
	enabled, "print preview",
	{
		"One Column", noicon, nokey, nomark, plain;
		"Two Column", noicon, nokey, nomark, plain;
		"Three Column", noicon, nokey, nomark, plain
	}
};
