#include "sifs.h"
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

void write_dir(const char* volume, const char* dirname, const char* volumedirname, bool write)
{
    struct dirent* dp;
    DIR* dir;

    if ((dir = opendir(dirname)) == NULL)
    {
        return;
    }

    char filename[512];
    char volumefilename[512];

    if (write)
    {
        SIFS_mkdir(volume, volumedirname);
        SIFS_perror(NULL);
    }

    while ((dp = readdir(dir)) != NULL)
    {
        struct stat st;
        sprintf(filename, "%s/%s", dirname, dp->d_name);
        sprintf(volumefilename, "%s/%s", volumedirname, dp->d_name);
        stat(filename, &st);
        if (strcmp(dp->d_name, volume) != 0 && strcmp(dp->d_name, ".") != 0 && strcmp(dp->d_name, "..") != 0)
        {
            if (S_ISDIR(st.st_mode))
            {
                printf("Writing directory %s as %s\n", filename, volumefilename);
                write_dir(volume, filename, volumefilename, true);
            }
            else if (S_ISREG(st.st_mode))
            {
                printf("Writing file %s as %s\n", filename, volumefilename);
                FILE* f = fopen(filename, "r");
				assert(f);
                void* buffer = malloc(st.st_size);
				assert(buffer);
                fread(buffer, 1, st.st_size, f);
				SIFS_errno = SIFS_EOK;
                SIFS_writefile(volume, volumefilename, buffer, st.st_size);
                SIFS_perror(NULL);
                fclose(f);
                free(buffer);
            }   
        }     
    }
}

int main(int argc, char** argv)
{
    char* dirname;
    if (argc == 2)
    {
        dirname = argv[1];
    }
    else
    {
        dirname = ".";
    }
    
    remove("vol");
    SIFS_mkvolume("vol", 1024, 1000);

    write_dir("vol", dirname, "", false);
}