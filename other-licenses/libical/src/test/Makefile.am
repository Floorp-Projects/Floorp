noinst_PROGRAMS = copycluster regression parser storage stow recur testmime testvcal process testclassify

LDADD =  ../libicalss/.libs/libicalss.a ../libicalvcal/.libs/libicalvcal.a ../libical/.libs/libical.a 

INCLUDES = 				\
       -I. 				\
       -I../libicalvcal 		\
       -I$(srcdir)/../libicalvcal 	\
       -I$(srcdir) 			\
       -I../libical 			\
       -I$(srcdir)/../libical 		\
       -I../libicalss 			\
       -I$(srcdir)/../libicalss           

copycluster_SOURCES = copycluster.c
regression_SOURCES = regression.c
parser_SOURCES = icaltestparser.c
storage_SOURCES = storage.c
stow_SOURCES = stow.c
testvcal_SOURCES = testvcal.c
recur_SOURCES = recur.c 
testmime_SOURCES = testmime.c
process_SOURCES = process.c
testclassify_SOURCES = testclassify.c
