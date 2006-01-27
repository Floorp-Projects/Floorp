

%.class : %.java
	 javac $<


default: p4jdb/TreeDisplay.class p4jdb/P4File.class p4jdb/P4Folder.class \
         p4jdb/P4DirTree.class p4jdb/P4DirTreeApplet.class

p4jdb/TreeDisplay.class p4jdb/P4File.class p4jdb/P4Folder.class \
p4jdb/P4DirTree.class p4jdb/P4DirTreeApplet.class: $(wildcard p4jdb/*.java) 
	javac p4jdb/*.java

doc	: 
	javadoc -link http://localhost/~fredric/java_docs/api $(wildcard p4jdb/*.java) 


#p4jdb/P4File.class: p4jdb/TreeDisplay.class p4jdb/P4DirTree.class
#p4jdb/P4Folder.class: p4jdb/TreeDisplay.class p4jdb/P4DirTree.class
#p4jdb/P4DirTree.class: p4jdb/P4Folder.class
#p4jdb/P4DirTreeApplet.class: p4jdb/TreeDisplay.class p4jdb/P4DirTree.class

