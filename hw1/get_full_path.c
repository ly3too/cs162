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
            #ifdef DEBUG
            printf("the full path is : %s\n", full_name);
            #endif
            return full_name;
        } else {
            // file doesn't exist
            charv ++;
        }
        #ifdef DEBUG
        printf("no file exist : %s\n", full_name);
        #endif
        free(full_name);
    }
    #ifdef DEBUG
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
    char **vec = malloc((size+1) * sizeof(char*));

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
