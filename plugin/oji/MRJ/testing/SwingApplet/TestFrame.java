import java.awt.*;
import java.awt.event.*;
import javax.swing.*;

public class TestFrame extends JFrame implements ActionListener 
{
	public TestFrame()
	{
		Container pane =  getContentPane();
		pane.setLayout(new BorderLayout());
		TestPanel tPanel = new TestPanel();
		pane.add("Center", tPanel);
		Button closeBtn = new Button("Close");
		closeBtn.addActionListener(this);
		Panel bottomPanel = new Panel();
		bottomPanel.setLayout(new FlowLayout());
		bottomPanel.add(closeBtn);
		pane.add("South", bottomPanel);
		pack();
	}

	public void actionPerformed(ActionEvent e)
	{
		setVisible(false);
	}

}
