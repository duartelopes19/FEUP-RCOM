#ifndef _INFO_H_
#define _INFO_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_STRING_LENGTH 256
typedef char info_string[MAX_STRING_LENGTH];

typedef struct info {
	info_string user; 
	info_string password; 
	info_string host; 
	info_string path; 
	info_string filename;
	int port; 
} info;

void resetinfo(info* info);
void parseinfo(info* info, const char *input_info);
void getFilename(info* info, char* filename);
char *strrev(char *str);
#endif