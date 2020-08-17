PROJECT		= sifs

HEADER		= $(PROJECT).h testutils.h
LIBRARY		= lib$(PROJECT).a

APPLICATIONS	= test_mkdir.a test_rmdir.a test_writefile.a test_recursive.a test_mkvol.a app.a

# ----------------------------------------------------------------

CC      = cc
CFLAGS  = -std=c99 -Wall -Werror -pedantic
LIBS	= -L. -lsifs -lm


all:	$(APPLICATIONS)

$(LIBRARY):
	make -C library

%.a:	%.c $(LIBRARY) testutils.o
	$(CC) $(CFLAGS) -o $@ $< testutils.o $(LIBS)

testutils.o:	testutils.c $(HEADER)
	$(CC) $(CFLAGS) -c testutils.c
	
app.a: app.c $(LIBRARY)
	$(CC) $(CFLAGS) -o app.a app.c $(LIBS) -lncurses

clean:
	rm -f $(LIBRARY) $(APPLICATIONS) testutils.o app
	make -C library clean

