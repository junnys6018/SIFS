#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <ctype.h>
#include <assert.h>

#include <ncurses.h>

#include "sifs.h"
#include "library/md5.h"
#include "library/sifsutils.h"

/*
	COMMANDS:

	cd        [absolute directory]
	mkdir     [dirname]
	rmdir     [dirname]
	mkfile    [filename] [size in bytes]
	rmfile    [filename]
	finfo     [filename]
	defrag
	exit
	vol

*/

extern char* SIFS_errlist[];

char working_directory[BUFSIZ];
bool commandline = true;
int mouseY = 0, mouseX = 0;

void my_init();
void my_end();
void display(const char* volumename, WINDOW* dirView, WINDOW* volView);
bool input(const char* volumename, WINDOW* inputWin);
void mdisplay(const char* volumename, WINDOW* dirView, WINDOW* volView);
bool minput(const char* volumename, WINDOW* inputWin);

void trim(char* str);
void cut(char** body, char** head);

int main(int argc, char* argv[])
{
	char* volumename;

	if (argc == 2)
	{
		volumename = argv[1];
	}
	else if (argc == 4)
	{
		volumename = argv[1];
		int blocksize = atoi(argv[2]);
		int nblocks = atoi(argv[3]);
		SIFS_mkvolume(volumename, blocksize, nblocks);
	}
	else
	{
		printf("USAGE: ./app [volumename] or ./app [new volume] [blocksize] [nblocks]\n");
		exit(EXIT_FAILURE);
	}


	my_init();

	int y, x;
	getmaxyx(stdscr, y, x);

	WINDOW* inputWin = newwin(3, x - 10, y - 4, 5);
	WINDOW* dirView = newwin(y - 5 - 1, x / 2 - 1, 1, 1);
	WINDOW* volView = newwin(y - 5 - 1, x / 2 - 1, 1, x / 2);

	keypad(inputWin, TRUE);

	bool running = true;
	while (running)
	{
		if (commandline)
		{
			display(volumename, dirView, volView);
			running = input(volumename, inputWin);
		}
		else
		{
			mdisplay(volumename, dirView, volView);
			running = minput(volumename, inputWin);
		}
	}

	my_end();

	return 0;
}

void my_init(void)
{
	initscr();
	raw();

	if (!has_colors())
	{
		endwin();
		printf("CONSOLE DOES NOT SUPPORT COLORS!\n");
		exit(EXIT_FAILURE);
	}
	start_color();

	init_pair(1, COLOR_WHITE, COLOR_MAGENTA);
	init_pair(2, COLOR_WHITE, COLOR_RED);
	init_pair(3, COLOR_WHITE, COLOR_GREEN);
	init_pair(4, COLOR_WHITE, COLOR_BLUE);
	init_pair(5, COLOR_RED , COLOR_BLACK);
	init_pair(6, COLOR_GREEN, COLOR_BLACK);
}

void my_end(void)
{
	endwin();
}

void display(const char* volumename, WINDOW* dirView, WINDOW* volView)
{
	// Try to open volume
	FILE* vol = fopen(volumename, "r");
	if (!vol)
	{
		SIFS_errno = SIFS_ENOVOL;
		SIFS_perror(NULL);
		my_end();
		exit(EXIT_FAILURE);
	}
	// Read and validate header
	SIFS_VOLUME_HEADER header = get_volumeheader(vol);
	if (header.blocksize < SIFS_MIN_BLOCKSIZE || header.nblocks == 0)
	{
		SIFS_errno = SIFS_ENOTVOL;
		fclose(vol);
		SIFS_perror(NULL);
		my_end();
		exit(EXIT_FAILURE);
	}
	// Read and validate bitmap
	SIFS_BIT* bitmap;
	get_volumebitmap(vol, &bitmap);
	if (!validate_bitmap(bitmap, header.nblocks))
	{
		SIFS_errno = SIFS_ENOTVOL;
		fclose(vol);
		SIFS_perror(NULL);
		my_end();
		exit(EXIT_FAILURE);
	}

	wclear(dirView);
	wclear(volView);

	box(dirView, 0, 0);
	box(volView, 0, 0);

	refresh();
	wrefresh(dirView);
	wrefresh(volView);

	int y, x;
	getmaxyx(volView, y, x);
	int width = x / 3 - 1;
	int maxBlocks = y * x / 3;
	maxBlocks = maxBlocks < header.nblocks ? maxBlocks : header.nblocks;
	for (int i = 0; i < maxBlocks; i++)
	{
		int att = 1;
		switch (bitmap[i])
		{
		case SIFS_UNUSED:
			att = 1;
			break;
		case SIFS_DIR:
			att = 2;
			break;
		case SIFS_FILE:
			att = 3;
			break;
		case SIFS_DATABLOCK:
			att = 4;
			break;
		}
		wattron(volView, COLOR_PAIR(att));	
		mvwprintw(volView, i / width + 1, 3 * (i % width) + 2, " %c ", bitmap[i]);
		wattroff(volView, COLOR_PAIR(att));
	}
	wrefresh(volView);

	mvwprintw(dirView, 1, 1, "Working Directory: %s | blocksize: %i | nblocks: %i",
		*working_directory == '\0' ? "root" : working_directory, header.blocksize, header.nblocks);

	char** entrynames = NULL;
	uint32_t nentries;
	time_t modtime;
	if (SIFS_dirinfo(volumename, working_directory, &entrynames, &nentries, &modtime) == 1)
	{
		attron(COLOR_PAIR(2));
		mvprintw(0, 0, "%s", SIFS_errlist[SIFS_errno]);
		attroff(COLOR_PAIR(2));
	}
	else
	{
		int err;
		SIFS_BLOCKID id = (*working_directory == '\0') ? SIFS_ROOTDIR_BLOCKID : find_dir(header, bitmap, vol, SIFS_ROOTDIR_BLOCKID, working_directory, &err);
		SIFS_DIRBLOCK dblock = get_dirblock(header, vol, id);
		assert(dblock.nentries == nentries);
		for (int i = 0; i < dblock.nentries; i++)
		{
			int att = (bitmap[dblock.entries[i].blockID] == SIFS_DIR) ? 5 : 6;
			wattron(dirView, COLOR_PAIR(att));
			mvwprintw(dirView, i + 3, 5, "%s", entrynames[i]);
			wattroff(dirView, COLOR_PAIR(att));
		}
	}
	wrefresh(dirView);

	getmaxyx(dirView, y, x);
	char* time = ctime(&modtime);
	char* pnewline = strchr(time, '\n'); // Remove newline
	*pnewline = '\0';
	mvwprintw(dirView, y - 2, 1, "Last modified: %s", time);

	wrefresh(dirView);

	if (entrynames)
		free(entrynames);
	free(bitmap);
	fclose(vol);
}

bool input(const char* volumename, WINDOW* inputWin)
{
	wclear(inputWin);

	box(inputWin, 0, 0);
	refresh();
	wrefresh(inputWin);

	char* body = malloc(BUFSIZ);
	char* head = malloc(BUFSIZ);
	memset(body, 0, BUFSIZ);
	memset(head, 0, BUFSIZ);

	char* obody = body, * ohead = head;


	wmove(inputWin, 1, 1);
	wgetnstr(inputWin, body, BUFSIZ);
	trim(body);
	deleteln();
	mvprintw(0, 0, "c: %s ", body);
	refresh();

	cut(&body, &head);

	printw("h: %s b: %s", head, body);

	if (strcmp(head, "defrag") == 0)
	{
		if (SIFS_defrag(volumename) != 0)
		{
			attron(COLOR_PAIR(2));
			deleteln();
			mvprintw(0, 0, "%s", SIFS_errlist[SIFS_errno]);
			attroff(COLOR_PAIR(2));
		}
	}
	else if (strcmp(head, "exit") == 0)
	{
		return false;
	}
	else if (strcmp(head, "cd") == 0)
	{
		cut(&body, &head);
		memset(working_directory, 0, BUFSIZ);
		strcpy(working_directory, head);
	}
	else if (strcmp(head, "mkdir") == 0)
	{
		cut(&body, &head);
		if (SIFS_mkdir(volumename, head) != 0)
		{
			attron(COLOR_PAIR(2));
			deleteln();
			mvprintw(0, 0, "%s", SIFS_errlist[SIFS_errno]);
			attroff(COLOR_PAIR(2));
		}
	}
	else if (strcmp(head, "rmdir") == 0)
	{
		cut(&body, &head);
		if (SIFS_rmdir(volumename, head) != 0)
		{
			attron(COLOR_PAIR(2));
			deleteln();
			mvprintw(0, 0, "%s", SIFS_errlist[SIFS_errno]);
			attroff(COLOR_PAIR(2));
		}
	}
	else if (strcmp(head, "mkfile") == 0)
	{
		cut(&body, &head);
		char* size = malloc(BUFSIZ);
		memset(size, 0, BUFSIZ);
		cut(&body, &size);

		int sizei = atoi(size);

		void* data = malloc(sizei);
		memset(data, 1, sizei);
		if (SIFS_writefile(volumename, head, data, sizei) != 0)
		{
			attron(COLOR_PAIR(2));
			deleteln();
			mvprintw(0, 0, "%s: path: %s sz: %i", SIFS_errlist[SIFS_errno], head, sizei);
			attroff(COLOR_PAIR(2));
		}

		free(size);
		free(data);

	}
	else if (strcmp(head, "rmfile") == 0)
	{
		cut(&body, &head);
		if (SIFS_rmfile(volumename, head) != 0)
		{
			attron(COLOR_PAIR(2));
			deleteln();
			mvprintw(0, 0, "%s", SIFS_errlist[SIFS_errno]);
			attroff(COLOR_PAIR(2));
		}
	}
	else if (strcmp(head, "finfo") == 0)
	{
		cut(&body, &head);
		size_t length;
		time_t modtime;
		if (SIFS_fileinfo(volumename, head, &length, &modtime) != 0)
		{
			attron(COLOR_PAIR(2));
			deleteln();
			mvprintw(0, 0, "%s", SIFS_errlist[SIFS_errno]);
			attroff(COLOR_PAIR(2));
		}
		else
		{
			char* time = ctime(&modtime);
			deleteln();
			mvprintw(0, 0, "%s | %i bytes | %s", head, length, time);
		}
	}
	else if (strcmp(head, "vol") == 0)
	{
		commandline = false;
	}
	else
	{
		attron(COLOR_PAIR(2));
		deleteln();
		mvprintw(0, 0, "COMMAND NOT FOUND: %s %s", head, body);
		attroff(COLOR_PAIR(2));
	}


	free(ohead);
	free(obody);
	return true;
}

void mdisplay(const char* volumename, WINDOW* dirView, WINDOW* volView)
{
	// Try to open volume
	FILE* vol = fopen(volumename, "r");
	if (!vol)
	{
		SIFS_errno = SIFS_ENOVOL;
		SIFS_perror(NULL);
		my_end();
		exit(EXIT_FAILURE);
	}
	// Read and validate header
	SIFS_VOLUME_HEADER header = get_volumeheader(vol);
	if (header.blocksize < SIFS_MIN_BLOCKSIZE || header.nblocks == 0)
	{
		SIFS_errno = SIFS_ENOTVOL;
		fclose(vol);
		SIFS_perror(NULL);
		my_end();
		exit(EXIT_FAILURE);
	}
	// Read and validate bitmap
	SIFS_BIT* bitmap;
	get_volumebitmap(vol, &bitmap);
	if (!validate_bitmap(bitmap, header.nblocks))
	{
		SIFS_errno = SIFS_ENOTVOL;
		fclose(vol);
		SIFS_perror(NULL);
		my_end();
		exit(EXIT_FAILURE);
	}

	wclear(dirView);

	box(dirView, 0, 0);
	box(volView, 0, 0);

	int y, x;
	getmaxyx(volView, y, x);
	int width = x / 3 - 1;

	int* underLine = malloc(header.nblocks * sizeof(int));
	memset(underLine, 0, header.nblocks * sizeof(int));

	SIFS_BLOCKID id = mouseY * width + mouseX;
	if (bitmap[id] == SIFS_DIR)
	{
		SIFS_DIRBLOCK dblock = get_dirblock(header, vol, id);
		mvwprintw(dirView, 2, 3, "    name = \"%s\"", dblock.name);

		char* time = ctime(&dblock.modtime);
		char* pnewline = strchr(time, '\n'); // Remove newline
		*pnewline = '\0';
		mvwprintw(dirView, 3, 3, "Modified = %li (%s)", dblock.modtime, time);

		mvwprintw(dirView, 4, 3, "nentries = %i", dblock.nentries);

		if (dblock.nentries == 0)
			mvwprintw(dirView, 6, 3, "No entries");
		for (int i = 0; i < dblock.nentries; i++)
		{
			underLine[dblock.entries[i].blockID] = 1;

			char msg[BUFSIZ] = { 0 };
			if (bitmap[dblock.entries[i].blockID] == SIFS_FILE)
			{
				sprintf(msg, "fileindex %i", dblock.entries[i].fileindex);
			}
			else
			{
				strcpy(msg, "fileindex not used");
			}
			mvwprintw(dirView, 6 + i, 3, "entry %.2i | blockID %.3i | %s", i, dblock.entries[i].blockID, msg);
		}
	}
	else if (bitmap[id] == SIFS_FILE)
	{
		SIFS_FILEBLOCK fblock = get_fileblock(header, vol, id);

		char* time = ctime(&fblock.modtime);
		char* pnewline = strchr(time, '\n'); // Remove newline
		*pnewline = '\0';
		mvwprintw(dirView, 2, 3, "    Modified = %li (%s)", fblock.modtime, time);
		mvwprintw(dirView, 3, 3, "      length = %i", fblock.length);
		mvwprintw(dirView, 4, 3, "         md5 = \"%s\"", MD5_str((char*)fblock.md5));
		mvwprintw(dirView, 5, 3, "firstblockID = %i", fblock.firstblockID);
		mvwprintw(dirView, 6, 3, "      nfiles = %i", fblock.nfiles);

		for (int i = 0; i < fblock.nfiles; i++)
		{
			mvwprintw(dirView, 8 + i, 3, "filename %.2i | \"%s\"", i, fblock.filenames[i]);
		}
		for (SIFS_BLOCKID i = 0; i < (fblock.length + header.blocksize - 1) / header.blocksize; i++)
		{
			underLine[i + fblock.firstblockID] = 1;
		}

	}
	else if (bitmap[id] == SIFS_DATABLOCK)
	{
		for (SIFS_BLOCKID i = 0; i < header.nblocks; i++)
		{
			if (bitmap[i] == SIFS_FILE)
			{
				SIFS_FILEBLOCK fblock = get_fileblock(header, vol, i);
				if (id >= fblock.firstblockID && id < fblock.firstblockID + (fblock.length + header.blocksize - 1) / header.blocksize)
				{
					underLine[i] = 1;
					break;
				}
			}
		}
	}

	int maxBlocks = y * x / 3;
	maxBlocks = maxBlocks < header.nblocks ? maxBlocks : header.nblocks;
	for (int i = 0; i < maxBlocks; i++)
	{
		int att;
		switch (bitmap[i])
		{
		case SIFS_UNUSED:
			att = COLOR_PAIR(1);
			break;
		case SIFS_DIR:
			att = COLOR_PAIR(2);
			break;
		case SIFS_FILE:
			att = COLOR_PAIR(3);
			break;
		case SIFS_DATABLOCK:
			att = COLOR_PAIR(4);
			break;
		}
		if (i == mouseY * width + mouseX)
		{
			att = COLOR_PAIR(6);
		}
		if (underLine[i])
		{
			att |= A_BOLD | A_REVERSE;
		}
		wattron(volView, att);
		mvwprintw(volView, i / width + 1, 3 * (i % width) + 2, " %c ", bitmap[i]);
		wattroff(volView, att);
	}

	refresh();
	wrefresh(dirView);
	wrefresh(volView);

	free(underLine);
	free(bitmap);
	fclose(vol);
}

bool minput(const char* volumename, WINDOW* inputWin)
{
	wclear(inputWin);

	box(inputWin, 0, 0);
	mvwprintw(inputWin, 1, 3, "ESC to go back");
	wrefresh(inputWin);

	int c = wgetch(inputWin);
	switch (c)
	{
	case KEY_UP:
		if (mouseY > 0)
			mouseY--;
		break;
	case KEY_DOWN:
		mouseY++;
		break;
	case KEY_LEFT:
		if (mouseX > 0)
			mouseX--;
		break;
	case KEY_RIGHT:
		mouseX++;
		break;
	case 27: // ESC character
		commandline = true;
		break;
	}

	return true;
}

void trim(char* str)
{
	char* end = str + strlen(str) - 1; // One before null byte
	while (isspace(*end))
	{
		end--;
	}
	end[1] = '\0';
}

void cut(char** body, char** head)
{
	memset(*head, 0, BUFSIZ);
	char* firstSpace = strchr(*body, ' ');

	if (!firstSpace)
	{
		strcpy(*head, *body);
		*body += strlen(*body);
		return;
	}
	int len = firstSpace - *body;
	strncpy(*head, *body, len);
	*body += len;

	// Trim extra whitespace
	while (**body == ' ')
	{
		(*body)++;
	}
}