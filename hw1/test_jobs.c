#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#include <sys/stat.h>

#include "tokenizer.h"
#include "jobs.h"
#include "get_full_path.h"


int main(int argc, char *argv[])
{

	static char line[4096];
    int line_num = 0;

	/* Please only print shell prompts when standard input is not a tty */
	fprintf(stdout, "%d: ", line_num);

	while (fgets(line, 4096, stdin)) {
		/* Split our line into words. */
		struct tokens *tokens = tokenize(line);
        #ifdef DEBUG
    		size_t i;
    		for (i = 0; i < tokens_get_length(tokens); i++) {
    			fprintf(stdout, "%s\t", tokens_get_token(tokens, i));
    		}
    		if (i > 0)
    			fprintf(stdout, "\n");
    	#endif

        get_job_vector(tokens);
        fprintf(stdout, "%d: ", line_num++);

        tokens_destroy(tokens);
    }
}
