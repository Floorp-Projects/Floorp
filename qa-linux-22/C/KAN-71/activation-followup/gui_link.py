#!/usr/bin/python3
"""Independent GTK link app; default GTK handler creates its own launch context."""
import gi,json,sys,time
from pathlib import Path
gi.require_version('Gtk','3.0')
from gi.repository import Gtk,Gdk,GLib
out=Path(sys.argv[2]);url=sys.argv[1]
window=Gtk.Window(title='KAN71 Independent GTK Link App')
window.set_default_size(520,180)
window.move(100,100)
box=Gtk.Box(orientation=Gtk.Orientation.VERTICAL,spacing=20)
box.set_border_width(25)
box.pack_start(Gtk.Label(label='Native GTK link → default desktop browser'),False,False,0)
link=Gtk.LinkButton.new_with_label(url,'Open local activation fixture')
def observed(button):
 event=Gtk.get_current_event()
 out.write_text(json.dumps({'monotonic':time.monotonic(),'eventTimestamp':Gtk.get_current_event_time(),'eventType':str(event.type) if event else None,'handler':'Gtk.LinkButton default activate-link handler; no fabricated startup ID'},indent=2))
 return False
link.connect('activate-link',observed)
box.pack_start(link,False,False,0);window.add(box)
def ready():
 allocation=link.get_allocation();point=link.translate_coordinates(window,allocation.width//2,allocation.height//2)
 origin=window.get_window().get_origin()
 out.with_suffix('.ready.json').write_text(json.dumps({'x':origin[-2]+point[0],'y':origin[-1]+point[1],'source':'actual Gtk.LinkButton allocation translated to root coordinates'}))
 return False
window.connect('destroy',Gtk.main_quit);window.show_all();GLib.timeout_add(200,ready);Gtk.main()
