import java.awt.event.*;
import java.applet.*;
import javax.swing.*;
import java.awt.*;

public class TestApplet extends JApplet implements ActionListener
{
	TestFrame tFrame;

	public void init()
	{
		getContentPane().setLayout(new BorderLayout());
		TestPanel tPanel = new TestPanel();
		getContentPane().add("Center", tPanel);

		Button showBtn = new Button("Show");
		showBtn.addActionListener(this);
		Panel bottomPanel = new Panel();
		bottomPanel.setLayout(new FlowLayout());
		bottomPanel.add(showBtn);
		getContentPane().add("South", bottomPanel);

		tFrame = new TestFrame();
	}

	public void actionPerformed(ActionEvent e)
	{
		tFrame.setVisible(true);
//		tFrame.dispose();
	}

}
