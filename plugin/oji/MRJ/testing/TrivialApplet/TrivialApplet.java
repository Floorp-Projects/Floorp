/*
	Trivial applet that displays a string - 4/96 PNL
*/

import java.awt.*;
import java.awt.event.*;
import java.applet.Applet;
import java.net.URL;
import java.net.MalformedURLException;

import netscape.javascript.JSObject;

class AboutBox extends Frame {
	AboutBox(Menu aboutMenu, ActionListener[] actionListeners) {
		super("About Applet");
		
		addWindowListener(
			new WindowAdapter() {
				public void windowClosing(WindowEvent e) {
					dispose();
				}
			});
		
		Button okButton = new Button("OK");
		okButton.addActionListener(
			new ActionListener() {
				public void actionPerformed(ActionEvent e) {
					dispose();
				}
			});
		
		// Annoying use of flow layout managers.
		Panel labelPanel = new Panel();
		Panel buttonPanel = new Panel();

		labelPanel.add(new Label("This applet's about box..."));
		buttonPanel.add(okButton);

		add(labelPanel, "North");
		add(buttonPanel, "Center");
		add(new TextField(), "South");
		
		// test menu bar stuff.
		MenuBar menuBar = new MenuBar();
		aboutMenu = (Menu) cloneMenu(aboutMenu);
		for (int i = 0; i < actionListeners.length; i++)
			aboutMenu.getItem(i).addActionListener(actionListeners[i]);
		menuBar.add(aboutMenu);
		setMenuBar(menuBar);

		pack();
		show();
	}
	
	private MenuItem cloneMenu(MenuItem oldItem) {
		if (oldItem instanceof Menu) {
			Menu oldMenu = (Menu) oldItem;
			Menu newMenu = new Menu(oldMenu.getLabel());
			int count = oldMenu.getItemCount();
			for (int i = 0; i < count; i++) {
				newMenu.add(cloneMenu(oldMenu.getItem(i)));
			}
			return newMenu;
		} else {
			MenuItem newItem = new MenuItem(oldItem.getLabel());
			return newItem;
		}
	}
}

class ExceptionThread extends Thread {
	ExceptionThread() {
		start();
	}
	
	public void run() {
		throw new Error("this is an error!");
	}
}

public class TrivialApplet extends Applet {
	public Button goButton;
	public Button aboutButton;
	public TextField urlField;
	public PopupMenu contextMenu;
	public Menu aboutMenu;
	public ActionListener[] actionListeners;
	private static int appletCount;

	public void init() {
		++appletCount;
	
		goButton = new Button("Go");
		aboutButton = new Button("About");
		
		String urlText = getParameter("URL");
		if (urlText == null)
			urlText = "http://www.apple.com";
		
		urlField = new TextField(urlText);
		
		ActionListener goListener =
			new ActionListener() {
				public void actionPerformed(ActionEvent e) {
					try {
						URL location = new URL(urlField.getText());
						System.out.println("going to URL: " + location);
						JSObject window = JSObject.getWindow(TrivialApplet.this);
						window.eval("alert('going to location " + location + "');");
						getAppletContext().showDocument(location, "_new");
					} catch (MalformedURLException mfue) {
					}
				}
			};
		
		ActionListener aboutListener =
			new ActionListener() {
				public void actionPerformed(ActionEvent e) {
					new AboutBox(aboutMenu, actionListeners);
				}
			};
		
		goButton.addActionListener(goListener);
		aboutButton.addActionListener(aboutListener);
			
		add(goButton);
		add(aboutButton);
		add(urlField);
		
		// Try a pop-up menu, and a menu in the menubar.
		contextMenu = new PopupMenu();
		aboutMenu = new Menu("About");
		
		contextMenu.add(newItem("About", aboutListener));
		aboutMenu.add(newItem("About", aboutListener));
		
		contextMenu.add(newItem("Go", goListener));
		aboutMenu.add(newItem("Go", goListener));
		
		ActionListener errorListener = new ActionListener() {
				public void actionPerformed(ActionEvent e) {
					new ExceptionThread();
				}
			};
		contextMenu.add(newItem("Error", errorListener));
		aboutMenu.add(newItem("Error", errorListener));
		
		ActionListener[] listeners = { aboutListener, goListener, errorListener };
		actionListeners = listeners;
		
		add(contextMenu);

		// add a mouse listener that causes the pop-up to appear appropriately.
		MouseListener mouseListener = new MouseAdapter() {
			public void mousePressed(MouseEvent e) {
				if (e.isPopupTrigger()) {
					e.consume();
					contextMenu.show(TrivialApplet.this, e.getX(), e.getY());
				}
			}
		};
		
		addMouseListener(mouseListener);
	}
	
	private MenuItem newItem(String title, ActionListener listener) {
		MenuItem item = new MenuItem(title);
		item.addActionListener(listener);
		return item;
	}
	
	private Frame getFrame() {
		Component p = this;
		while (p != null && !(p instanceof Frame))
			p = getParent();
		return (Frame)p;
	}
	
	// public void paint( Graphics g ) {}

    public boolean mouseEnter(Event evt, int x, int y) {
    	showStatus("Welcome!");
		return true;
    }

    public boolean mouseExit(Event evt, int x, int y) {
    	showStatus("See you later!");
		return true;
    }
    
	public void print(String message) {
		JSObject window = JSObject.getWindow(this);
		Object[] args = { message };
		window.call("println", args);
    }
    
    public int getAppletCount() {
    	return appletCount;
    }
}
