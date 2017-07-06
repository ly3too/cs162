#include "get_full_path.h"

/*find, creat and return full path if exists, else return NLL*/
char *get_fpath(char * const *charv, const char *file, int how){
    char *full_name = NULL;

    while ((*charv) != NULL) {
        full_name = (char *)malloc((strlen(*charv)+strlen(file) + 2) * sizeof(char));
        strcpy(full_name, *charv);
        strcat(full_name, "/");
        strcat(full_name, file);

        if( access( full_name, how) != -1 ) {
            //file exists
            #ifdef DEBUG_GET_FULL_PATH
            printf("the full path is : %s\n", full_name);
            #endif
            return full_name;
        } else {
            // file doesn't exist
            charv ++;
        }
        #ifdef DEBUG_GET_FULL_PATH
        printf("no file exist : %s\n", full_name);
        #endif
        free(full_name);
    }
    #ifdef DEBUG_GET_FULL_PATH
    printf("find nothing\n");
    #endif
    //no exist file
    return NULL;
}

void fpath_destory(char *fpath){
    if (fpath == NULL) {
      return;
    }
    free(fpath);
}

char ** get_path_vector(const char *path) {

    char *env = strdup(path); //duplicate the path
    int size = 1; // vector size
    char *cur = env; // current path;
    char **vec = (char**)malloc((size+1) * sizeof(char*));

    vec[size-1] = cur;
    vec[size] = NULL;

    cur = strchr(cur,':');
    while(cur != NULL) {

        *cur = 0;
        cur ++;

        size++;
        vec = (char**) realloc(vec,(size+1) * sizeof(char*));
        vec[size-1] = cur;
        vec[size] = NULL;

        cur = strchr(cur,':');
    }

    return vec;
}

void destroy_path_vector(char ** vec){
    if (vec == NULL) {
      return;
    }
    free(*vec);
    free(vec);
}

/* check and get full path */
int get_cmd(char *const wd, char ** cmd)
{
	struct stat sb; // check file status

  *cmd = NULL;

	if (access(wd, X_OK) != -1) {
		/* file excutable*/
		*cmd = strdup(wd);
	} else {
		/*find it in path*/

		/* try to get full path */
		char * path = getenv("PATH");
    #ifdef DEBUG_GET_FULL_PATH
		printf("PATH = %s\n", path);
    #endif
		char ** pvec = get_path_vector(path);
    //free(path);
    #ifdef DEBUG_GET_FULL_PATH
		for (int i = 0; pvec[i] != NULL; ++i)
			printf("pvec[%d] = %s\n", i, pvec[i]);
    #endif
		*cmd = get_fpath(pvec, wd, X_OK);
    destroy_path_vector(pvec);
    #ifdef DEBUG_GET_FULL_PATH
		printf("cmd = %s\n", *cmd);
    #endif

    if(*cmd == NULL) {
      return -1;
    }

  }

  //check if is a regular file
  if (stat(*cmd, &sb) == -1) {
    perror("stat error:");
    *cmd = NULL;
    return -1;
  }

  if (S_ISDIR(sb.st_mode)) {
    *cmd = NULL;
    return 1;
  }

	return 0;
}

void destroy_cmd(char * cmd)
{
  fpath_destory(cmd);
}
