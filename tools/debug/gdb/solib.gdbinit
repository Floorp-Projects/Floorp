##
## Use this gdb hack to save memory
##
## auto-solib-add suggested by Arun Sharma
## macros suggested by Ramiro Estrugo
##
## XXX document some more
##

set auto-solib-add 0

def load-symbols-nspr
shared plds3
shared plc3
shared nspr3
shared pthread
end

def load-symbols-base
shared reg
shared xpcom
shared xp
shared mozjs
end

def load-symbols-gtk
shared gtk-1.2
shared gdk-1.2
shared gmodule-1.2
shared glib-1.2
end

def load-symbols-render
shared raptorgfx
shared gfxgtk
shared gfxps
end

def load-symbols-widget
shared widgetgtk
shared gmbasegtk
end

def load-symbols-gecko
load-symbols-render
load-symbols-widget
shared raptorbase
shared raptorwebwidget
shared raptorhtml
shared raptorhtmlpars
shared expat
shared xmltok
shared raptorview
shared jsdom
shared raptorplugin
end

def load-symbols-netlib
shared abouturl
shared fileurl
shared ftpurl
shared gophurl
shared httpurl
shared remoturl
shared sockstuburl
shared mimetype
shared netcache
shared netcnvts
shared net
shared netutil
shared network
shared jsurl
end

def load-symbols-misc
shared mozdbm
shared pwcac
shared pref
shared mozutil
end

def load-symbols-img
shared img
shared jpeg
shared png
end

def load-symbols-seamonkey
load-symbols-nspr
load-symbols-base
load-symbols-gtk
load-symbols-widget
load-symbols-gecko
load-symbols-netlib
load-symbols-misc
load-symbols-img
end
