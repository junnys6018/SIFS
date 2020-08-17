#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>

extern void free_entrynames(char** entrynames, uint32_t nentries);
extern void print_dir(const char* vol, const char* dir);
extern bool dircmp(const char* vol, const char* dir, const char** ref, uint32_t n);