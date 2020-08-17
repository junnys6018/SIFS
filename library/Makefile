#  CITS2002 Project 2 2019
#  Name(s):             Ethan Lim
#  Student number(s):   22701593

#  NOTE: File paths that end with a '/' are not supported.
#  e.g. use path/to/file instead of path/to/file/
#  SIFS_defrag() was implemented in defrag.c

HEADERS	= ../sifs.h sifs-internal.h md5.h sifsutils.h
LIBRARY	= libsifs.a

OBJECTS	= mkvolume.o mkdir.o rmdir.o dirinfo.o\
		writefile.o readfile.o rmfile.o fileinfo.o\
		perror.o md5.o sifsutils.o defrag.o

CC      = cc
CFLAGS  = -std=c99 -Wall -Werror -pedantic

$(LIBRARY):	$(OBJECTS)
	ar rc $(LIBRARY) $(OBJECTS)
	ranlib $(LIBRARY)
	cp $(LIBRARY) ..


%.o:	%.c $(HEADERS)
	$(CC) $(CFLAGS) -c $<


# ----------------------------------------------------------------

clean:
	rm -f $(LIBRARY) $(OBJECTS)
