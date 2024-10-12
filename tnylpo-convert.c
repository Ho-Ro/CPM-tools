/*
 * Copyright (c) 2019 Georg Brein. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */


#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <locale.h>
#include <wchar.h>
#include <errno.h>

#include <unistd.h>

#include "tnylpo.h"


/*
 * always append ^Z when writing a CP/M output file, even if the
 * data ends on a 128 byte CP/M record boundary
 */
static int append_cntrlz = 0;
/*
 * omit padding a CP/M output file with ^Z bytes
 */
static int omit_padding = 0;
/*
 * how to handle unconvertible characters
 */
static int ignore_convert = 0;
static int error_convert = 0;
/*
 * source and target file names, types, and file pointers
 */
static char *source_name = NULL, *target_name = NULL;
static int source_unix = 0, target_unix = 0;
static int source_stdin = 0, target_stdout = 0;
static FILE *source_fp = NULL, *target_fp = NULL;
/*
 * number of bytes written to a target file in CP/M format
 */
static off_t target_size = 0;


/*
 * program name for error messages
 */
static const char *prog_name = NULL;


/*
 * write a message to stderr
 */
void
perr(const char *format, ...) {
	va_list params;
	va_start(params, format);
	fprintf(stderr, "%s: ", prog_name);
	vfprintf(stderr, format, params);
	fprintf(stderr, "\n");
	va_end(params);
}


/*
 * display a short usage summary
 */
void
usage(void) {
	perr("usage: %s [ <options> ] [ <source> [ <target> ] ]",
	    prog_name);
	perr("valid <options> are");
	perr("    -a              use alternate charset");
	perr("    -e              treat unconvertible characters as error");
	perr("    -f <fn>         read configuration file");
	perr("    -i              ignore unconvertible characters");
	perr("    -n              do not pad CP/M files with ^Z bytes");
	perr("    -z              always terminate CP/M files with ^Z");
	perr("<source> or <target> are");
	perr("    -u ( <fn> | - ) text file in host OS format");
	perr("    -c <fn>         text file in CP/M format");
	perr("if <source> resp. <target> is omitted, \"-u -\"  is assumed");
}


/*
 * print error message if a command line option has been defined more than once
 */
static void
only_once(int c) {
	perr("option -%c may be specified only once", c);
}


/*
 * get the configuration
 *
 * After parsing the command line, further configuration values are
 * read from the optional configuration file
 */
static int
get_config(int argc, char **argv) {
	int rc = 0, opt;
	char *cfn = NULL;
	opterr = 0;
	while ((opt = getopt(argc, argv, "ac:ef:inu:z")) != EOF) {
		switch (opt) {
		case 'a':
			/*
			 * use alternate character set
			 */
			if (charset) {
				only_once('a');
				rc = (-1);
			} else {
				charset = 1;
			}
			break;
		case 'c':
		case 'u':
			if (source_name && target_name) {
				perr("source and target already specified");
				rc = (-1);
			}
			if (! strcmp(optarg, "-")) {
				if (opt != 'u') {
					perr("stdin/stdout only allowed for "
					    "Unix files");
					rc = (-1);
				} else {
					if (! source_name) {
						source_name = "<stdin>";
						source_stdin = 1;
						source_unix = 1;
					} else {
						target_name = "<stdout>";
						target_stdout = 1;
						target_unix = 1;
					}
				}
			} else {
				if (! source_name) {
					source_name = optarg;
					source_stdin = 0;
					source_unix = (opt == 'u');
				} else {
					target_name = optarg;
					target_stdout = 0;
					target_unix = (opt == 'u');
				}
			}
			break;
		case 'e':
			/*
			 * treat unconvertible characters as error
			 */
			if (error_convert) {
				only_once('e');
				rc = (-1);
			} else {
				error_convert = 1;
			}
			break;
		case 'f':
			/*
			 * configuration file
			 */
			if (cfn) {
				only_once('f');
				rc = (-1);
			}
			cfn = optarg;
			break;
		case 'i':
			/*
			 * ignore unconvertible characters
			 */
			if (ignore_convert) {
				only_once('i');
				rc = (-1);
			} else {
				ignore_convert = 1;
			}
			break;
		case 'n':
			/*
			 * omit padding a CP/M target file with ^Z bytes
			 */
			if (omit_padding) {
				only_once('n');
				rc = (-1);
			} else {
				omit_padding = 1;
			}
			break;
		case 'z':
			/*
			 * always append ^Z to the end of a CP/M target file
			 */
			if (append_cntrlz) {
				only_once('z');
				rc = (-1);
			} else {
				append_cntrlz = 1;
			}
			break;
		case '?':
			perr("invalid option -%c", optopt);
			rc = (-1);
			break;
		}
	}
	/*
	 * defaults for source and target are Unix format files read from
	 * stdin resp. written to stdout
	 */
	if (! source_name) {
		source_name = "<stdin>";
		source_unix = 1;
		source_stdin = 1;
	}
	if (! target_name) {
		target_name = "<stdout>";
		target_unix = 1;
		target_stdout = 1;
	}
	/*
	 * positional arguments are not allowed on the command line
	 */
	if (argc != optind) {
		perr("positional arguments are not allowed");
		rc = (-1);
	}
	/*
	 * -i and -e are incompatible
	 */
	if (ignore_convert && error_convert) {
		perr("options -i and -e are mutually exclusive");
		rc = (-1);
	}
	/*
	 * command line error: print usage information and die
	 */
	if (rc) {
		usage();
		goto premature_exit;
	}
	/*
	 * read the optional configuration file
	 */
	rc = read_config(cfn);
premature_exit:
	return rc;
}


/*
 * write a character to the CP/M file; keeps track of the output file size
 */
static void
write_cpm(unsigned char uc) {
	target_size += fwrite(&uc, sizeof uc, 1, target_fp);
}


/*
 * For once, no comment.
 */
int
main(int argc, char **argv) {
	int rc = 0, t, last_was_cr, convert_error = 0;
	wint_t wc;
	unsigned char uc;
	char *temp_name = NULL;
	prog_name = base_name(argv[0]);
	if (! setlocale(LC_CTYPE, "")) {
		perr("setlocale(LC_CTYPE) failed");
		rc = (-1);
		goto premature_exit;
	}
	/*
	 * if the only parameter is -h, print the usage summary and exit
	 */
	if (argc == 2 && ! strcmp(argv[1], "-h")) {
		usage();
		goto premature_exit;
	}
	/*
	 * parse command line and read configuration file
	 */
	if (get_config(argc, argv)) {
		perr("command line or configuration error");
		goto premature_exit;
	}
	/*
	 * open source file
	 */
	if (source_stdin) {
		source_fp = stdin;
	} else {
		source_fp = fopen(source_name, source_unix ? "r" : "rb");
		if (! source_fp) {
			perr("couldn't open %s: %s", source_name,
			    strerror(errno));
			rc = (-1);
			goto premature_exit;
		}
	}
	/*
	 * open target file
	 */
	if (target_stdout) {
		target_fp = stdout;
	} else {
		/*
		 * build temporary target file name
		 */
		temp_name = alloc(strlen(target_name) + 30);
		sprintf(temp_name, "%s.temp.%lu", target_name,
		    (unsigned long) getpid());
		target_fp = fopen(temp_name, target_unix ? "w" : "wb");
		if (! target_fp) {
			perr("couldn\'t open %s: %s", temp_name,
			    strerror(errno));
			rc = (-1);
			goto premature_exit;
		}
	}
	/*
	 * main loop: copy characters
	 */
	if (source_unix && target_unix) {
		/*
		 * Unix to Unix: no translation whatsoever
		 */
		while ((wc = fgetwc(source_fp)) != WEOF) fputwc(wc, target_fp);
	} else if (source_unix && ! target_unix) {
		/*
		 * Unix to CP/M: convert characters, translate LF to CR/LF
		 */
		while ((wc = fgetwc(source_fp)) != WEOF) {
			if (wc == L'\n') {
				write_cpm(0x0d /* CR */);
				write_cpm(0x0a /* LF */);
			} else {
				t = to_cpm(wc);
				if (t == (-1)) {
					convert_error = 1;
				} else {
					write_cpm((unsigned char) t);
				}
			}
		}
	} else if (! source_unix && target_unix) {
		/*
		 * CP/M to Unix: stop at first SUB, convert characters,
		 * translate CR/LF to LF
		 */
		last_was_cr = 0;
		while (fread(&uc, sizeof uc, 1, source_fp) &&
		     uc != 0x1a /* SUB */) {
			if (uc == 0x0a /* LF */) {
				/*
				 * bare LF and CR/LF are translated to LF
				 */
		     		fputwc(L'\n', target_fp);
				last_was_cr = 0;
			} else {
				/*
				 * let bare CRs survive
				 */
				if (last_was_cr) fputwc(L'\r', target_fp);
				if (uc == 0x0d /* CR */) {
					/*
					 * remember CR
					 */
					last_was_cr = 1;
				} else {
					/*
					 * convert other character
					 */
					wc = from_cpm(uc);
					if (wc == WEOF) {
						convert_error = 1;
					} else {
						fputwc(wc, target_fp);
					}
					last_was_cr = 0;
				}
			}
		}
		/*
		 * copy last bare CR
		 */
		if (last_was_cr) fputwc(L'\r', target_fp);
	} else {
		/*
		 * CP/M to CP/M: stop at first SUB
		 */
		while (fread(&uc, sizeof uc, 1, source_fp) &&
		     uc != 0x1a /* SUB */) write_cpm(uc);
	}
	/*
	 * terminate CP/M format output
	 */
	if (! target_unix) {
		if (append_cntrlz) write_cpm(0x1a /* SUB */);
		if (! omit_padding) {
			while (target_size % 128) write_cpm(0x1a /* SUB */);
		}
	}
	/*
	 * check for source read errors
	 */
	if (ferror(source_fp)) {
		perr("read error on %s: %s", source_name, strerror(errno));
		rc = (-1);
	}
	/*
	 * check for target write errors
	 */
	if (ferror(target_fp)) {
		perr("write error on %s: %s", temp_name ? temp_name :
		    target_name, strerror(errno));
		rc = (-1);
	}
	/*
	 * handle unconvertible characters
	 */
	if (convert_error) {
		if (! ignore_convert) {
			perr("%s%s contains untranslateable characters",
			    error_convert ? "" : "warning: ", source_name);
			if (error_convert) rc = (-1);
		}
	}
premature_exit:
	if (source_fp && ! source_stdin) {
		/*
		 * close the source file, if it isn't stdin
		 */
		if (fclose(source_fp)) {
			perr("couldn\'t close %s: %s", source_name,
			    strerror(errno));
			rc = (-1);
		}
	}
	if (target_fp && ! target_stdout) {
		/*
		 * close the target file, if it isn't stdout
		 */
		if (fclose(target_fp)) {
			perr("couldn\'t close %s: %s", temp_name,
			    strerror(errno));
			rc = (-1);
		}
		if (! rc) {
			/*
			 * rename the temporary file to the target name
			 */
			if (rename(temp_name, target_name)) {
				perr("couldn\'t rename %s to %s: %s",
				    temp_name, target_name, strerror(errno));
				rc = (-1);
			}
		}
		/*
		 * in case of error, try to remove the temporary file
		 */
		if (rc) remove(temp_name);
	}
	free(temp_name);
	exit(rc ? EXIT_FAILURE : EXIT_SUCCESS);
	return 0;
}
