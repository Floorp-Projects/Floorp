import java.awt.*;
import java.awt.event.*;
import javax.swing.*;

public class TestPanel extends JPanel 
{
	public TestPanel()
	{
		setLayout(new GridLayout(3,2));
		for(int ctr=1; ctr<=3; ctr++ )
		{
			add( new JLabel("Label " + ctr));
			add( new JTextField());
		}
	}
}
