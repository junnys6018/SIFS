#include "sifs.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

void make_fragmented_directory(void)
{
	SIFS_mkvolume("dvol", 1024, 24 * 8 + 1 + 24);
	for (int i = 0; i < 24; i++)
	{
		char name[2];
		name[0] = 'A' + i;
		name[1] = '\0';
		SIFS_errno = SIFS_EOK;
		SIFS_mkdir("dvol", name);
		printf("Making directory: %s\n", name);
		SIFS_perror(NULL);
		for (int j = 0; j < 8; j++)
		{
			char directory[BUFSIZ];
			char child[2];
			child[0] = 'a' + j;
			child[1] = '\0';
			sprintf(directory, "%s/%s", name, child);

			SIFS_errno = SIFS_EOK;
			SIFS_mkdir("dvol", directory);
			printf("Making subdirectory: %s\n", directory);
			SIFS_perror(NULL);
		}
	}
	for (int i = 0; i < 24; i++)
	{
		char name[2];
		name[0] = 'A' + i;
		name[1] = '\0';
		for (int j = 0; j < 8; j++)
		{
			if (rand() % 2 == 1)
			{
				char directory[BUFSIZ];
				char child[2];
				child[0] = 'a' + j;
				child[1] = '\0';
				sprintf(directory, "%s/%s", name, child);

				SIFS_errno = SIFS_EOK;
				SIFS_rmdir("dvol", directory);
				printf("Removing subdirectory: %s\n", directory);
				SIFS_perror(NULL);
			}
		}
	}
}

void make_fragmented_file(void)
{
	SIFS_mkvolume("dvol", 1024, 129);
	int nblocks = 128;

	int nfiles = 0;
	for (int i = 0; i < 24 && nblocks > 0; i++, nfiles++)
	{
		int blocksUsed = rand() % 7 + 1;
		blocksUsed = (blocksUsed + 1 < nblocks) ? blocksUsed : nblocks - 1;
		if (i == 23)
			blocksUsed = nblocks - 1; // Use up blocks
		nblocks -= (blocksUsed + 1);

		int size = blocksUsed * 1024;
		char* bytes = malloc(size);
		// Randomize the data
		for (int j = 0; j < size; j++)
		{
			bytes[j] = rand() % 256;
		}

		char name[2];
		name[0] = 'A' + i;
		name[1] = '\0';

		SIFS_errno = SIFS_EOK;
		SIFS_writefile("dvol", name, bytes, size);
		printf("WRITING %i bytes in %s with %i blocks left\n", size, name, nblocks);
		SIFS_perror(NULL);

		free(bytes);
	}

	for (int i = 0; i < nfiles; i++)
	{
		if (rand() % 2 == 0)
		{
			char name[2];
			name[0] = 'A' + i;
			name[1] = '\0';

			SIFS_errno = SIFS_EOK;
			SIFS_rmfile("dvol", name);
			printf("DELETING %s\n", name);
			SIFS_perror(NULL);
		}
	}
}

void make_fragmented(int32_t nblocks)
{
	SIFS_mkvolume("dvol", 1024, nblocks + 1);
	SIFS_mkdir("dvol", "A");
	nblocks--;

	bool isfile[49][24] = { 0 };

	int levels = 1;
	while (nblocks > 0 && levels < 50)
	{
		char directory[BUFSIZ];
		memset(directory, 0, BUFSIZ);
		for (int i = 0; i < 2 * levels; i += 2)
		{
			directory[i] = 'A';
			directory[i + 1] = '/';
		}

		directory[2 * levels - 1] = '\0';

		for (int i = 1; i < 24 && nblocks > 0; i++)
		{
			printf("%i: ", nblocks);
			char name[2];
			name[0] = 'A' + i;
			name[1] = '\0';
			char path[BUFSIZ];

			sprintf(path, "%s/%s", directory, name);
			if (rand() % 2)
			{
				SIFS_errno = SIFS_EOK;
				SIFS_mkdir("dvol", path);
				printf("Making directory: %s\n", path);
				SIFS_perror(NULL);

				nblocks--;
			}
			else
			{
				int size = rand() % 4 + 1;
				int bytes = size * 1024;
				char* data = malloc(bytes);
				for (int i = 0; i < 256; i++)
				{
					data[i] = rand() % 256;
				}

				SIFS_errno = SIFS_EOK;
				SIFS_writefile("dvol", path, data, bytes);
				printf("Making %i byte file %s\n", bytes, path);
				SIFS_perror(NULL);

				isfile[levels][i] = true;

				nblocks -= (size + 1);
			}
		}

		directory[2 * levels - 1] = '/';
		directory[2 * levels] = 'A';

		SIFS_errno = SIFS_EOK;
		SIFS_mkdir("dvol", directory);
		printf("Making branched directory %s %i levels\n", directory, levels);
		SIFS_perror(NULL);

		nblocks--;
		levels++;
	}

	// Delete stuff
	for (int l = 0; l < levels; l++)
	{
		char directory[BUFSIZ];
		memset(directory, 0, BUFSIZ);
		for (int i = 0; i < 2 * l; i += 2)
		{
			directory[i] = 'A';
			directory[i + 1] = '/';
		}

		directory[2 * l - 1] = '\0';
		for (int e = 1; e < 24; e++)
		{
			if (rand() % 3 == 0)
			{
				char name[2];
				name[0] = 'A' + e;
				name[1] = '\0';
				char path[BUFSIZ];

				sprintf(path, "%s/%s", directory, name);	
				if (isfile[l][e])
				{
					SIFS_errno = SIFS_EOK;
					SIFS_rmfile("dvol", path);
					printf("Removing file %s\n", path);
					SIFS_perror(NULL);
				}
				else
				{
					SIFS_errno = SIFS_EOK;
					SIFS_rmdir("dvol", path);
					printf("Removing dir %s\n", path);
					SIFS_perror(NULL);
				}
			}
		}
	}
}

void print_useage(const char* prog)
{
	printf("Useage: %s [mode] [arguments]\n", prog);
	printf("\t [mode]: file | creates fragmented file\n");
	printf("\t [mode]: dir  | creates fragmented directory\n");
	printf("\t [mode]: both | [arguments]: nblocks | creates fragmented volume of size nblocks\n");	
}

int main(int argc, char** argv)
{
	srand(time(NULL));

	if (argc == 2)
	{
		remove("dvol");
		if (strcmp("file",argv[1]) == 0)
			make_fragmented_file();
		else if (strcmp("dir", argv[1]) == 0)
			make_fragmented_directory();
		else
		{
			print_useage(argv[0]);
			exit(EXIT_FAILURE);
		}
	}
	else if (argc == 3)
	{
		remove("dvol");
		int i = atoi(argv[2]);
		make_fragmented(i);
	}
	else
	{
		print_useage(argv[0]);
		exit(EXIT_FAILURE);
	}

	return 0;
}