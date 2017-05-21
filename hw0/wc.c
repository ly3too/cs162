#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <ctype.h>
#include <string.h>

# define COUNT_T unsigned long long

enum {
	WC_LINES    = 0, /* -l */
	WC_WORDS    = 1, /* -w */
	WC_UNICHARS = 2, /* -m */
	WC_BYTES    = 3, /* -c */
	WC_LENGTH   = 4, /* -L */
	NUM_WCS     = 5,
};

int main (int argc, char **argv)
{
  int c;
  int idx = 0;
  int ok = 1;
  COUNT_T counts[NUM_WCS] = {0};
  COUNT_T totals[NUM_WCS] = {0};
  char flag[NUM_WCS] = {0, 0, 0, 0, 0};


//parse argv
  while (ok)
    {
      static struct option long_options[] =
        {
          /* These options set a flag. */
          {"bytes",     no_argument,       0, 'c'},
          {"chars",     no_argument,       0, 'm'},
          {"lines",     no_argument,       0, 'l'},
          {"files0-from=F",     no_argument,       0, 0},
          {"max-line-length",   no_argument, 0, 'L'},
          {"words",   no_argument, 0, 'w'},
          {"help",    no_argument, 0, 0},
          {"version", no_argument, 0, 0},
          {0, 0, 0, 0}
        };
      /* getopt_long stores the option index here. */
      int option_index = 0;

      c = getopt_long (argc, argv, "cmlwL",
                       long_options, &option_index);

      /* Detect the end of the options. */
      if (c == -1)
        break;

  //    printf ("idx=%d, c='%c', optind=%d, optopt=%d, option_index=%d, *optarg=\"%s\"\n", idx, c, optind, optopt, option_index, optarg);
      idx++;

      switch (c)
        {
        case 0:
          /* If this option set a flag, do nothing else now. */
          if (long_options[option_index].flag != 0)
            break;
          printf ("option %s", long_options[option_index].name);
          break;

        case 'c':
          flag[WC_BYTES] = 1;
          break;

        case 'm':
          flag[WC_UNICHARS] = 1;
          break;

        case 'l':
          flag[WC_LINES] = 1;
          break;

        case 'w':
          flag[WC_WORDS] = 1;
          break;

        case 'L':
          flag[WC_LENGTH] = 1;
          break;

        case '?':
          /* unknown option */
          printf ("Try 'wc --help' for more information\n");
          ok = 0; //stop here
          break;

        default:
          abort ();
        }
    }
    if (idx == 0) { //defult value
      flag[WC_BYTES] = 1;
      flag[WC_LINES] = 1;
      flag[WC_WORDS] = 1;
    }

  //printf("end, optind=%d\n", optind);

  /* process files */
  if (ok)
  {
    int num_files = 0;
    int c;
    FILE *pcur_file;
    int linepos;
    int in_word;
    int i;
    char *pfname;

    if (optind < argc) // use files
    {
      num_files = argc - optind;
    }

    do {

      if (num_files) {
        pfname = argv[optind];
        pcur_file = fopen(argv[optind++],"r");
      }
      else {
        pcur_file = stdin;
      }

      //processing files or inputs
      linepos = 0;
      in_word = 0;
      memset(counts, 0, sizeof(counts));
      while ((c = getc(pcur_file)) != EOF){

        ++counts[WC_BYTES];
        if ((c & 0xc0) != 0x80) { //test unicodes
  				++counts[WC_UNICHARS];
  			}

        if (isprint(c) || (c & 0xc0) == 0x80) {
  				++linepos;
  				if (!isspace(c)) {
  					in_word = 1;
  					continue;
  				}
  			}

        else if ((unsigned)(c - 9) <= 4) {
  				/* \t  9
  				 * \n 10
  				 * \v 11
  				 * \f 12
  				 * \r 13
  				 */

  				if (c == '\t') {
  					linepos = (linepos | 7) + 1;
  				} else {  /* '\n', '\r', '\f', or '\v' */

  					if (linepos > counts[WC_LENGTH]) {
  						counts[WC_LENGTH] = linepos;
  					}
  					if (c == '\n') {
  						++counts[WC_LINES];
  					}
  					if (c != '\v') {
  						linepos = 0;
  					}
  				}
  			} else {
  				continue;
  			}

  			counts[WC_WORDS] += in_word;
  			in_word = 0;
      }
      counts[WC_WORDS] += in_word;
      in_word = 0;

      //close file if not stdin
      if (num_files)
        fclose(pcur_file);

      //print count result
      for (i = 0; i < NUM_WCS; ++i)
      {
        if(flag[i]) {
          printf("%llu\t", counts[i]);
        }
        totals[i] += counts[i];
      }

      if(num_files)
        printf(" %s\n", pfname, num_files);


    } while(optind < argc);

    if (num_files > 1)
    {
      //print total
      for (i = 0; i < NUM_WCS; ++i)
      {
        if(flag[i]) {
          printf("%llu\t", totals[i]);
        }
      }
      printf(" total\n");
    }


  }

  exit (0);
}
