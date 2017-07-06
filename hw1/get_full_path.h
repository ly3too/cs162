#pragma once

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>

/*find, creat and return full path if exists, else return NLL*/
char *get_fpath(char * const *charv, const char *file, int how);

void fpath_destory(char *fpath);

char ** get_path_vector(const char *path);

void destroy_path_vector(char ** vec);

int get_cmd(char *const wd, char ** cmd);
