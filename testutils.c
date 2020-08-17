#include "sifs.h"
#include "testutils.h"

void free_entrynames(char** entrynames, uint32_t nentries)
{
	if (entrynames != NULL) {
		for (int e = 0; e < nentries; ++e) {
			free(entrynames[e]);
		}
		free(entrynames);
	}
}

void print_dir(const char* vol, const char* dir)
{
	char** log;
	uint32_t nentries;
	time_t modtime;
	if (SIFS_dirinfo(vol, dir, &log, &nentries, &modtime) == 1)
	{
		SIFS_perror(NULL);
	}
	else
	{
		printf("nentries: %i | modtime: %sContents:\n", nentries, ctime(&modtime));
		for (int i = 0; i < nentries; i++)
		{
			printf("\t%s\n", log[i]);
		}
	}
	free_entrynames(log, nentries);
}

bool dircmp(const char* vol, const char* dir, const char** ref, uint32_t n)
{
	char** log;
	uint32_t nentries;
	time_t modtime;
	if (SIFS_dirinfo(vol, dir, &log, &nentries, &modtime) == 1)
	{
		SIFS_perror(NULL);
		return false;
	}
	if (n != nentries)
		return false;
	for (int i = 0; i < nentries; i++)
	{
		if (strcmp(ref[i], log[i]) != 0)
		{
			return false;
		}
	}

	return true;
}