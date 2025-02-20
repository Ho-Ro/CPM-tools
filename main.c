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
#include <ctype.h>
#include <wchar.h>
#include <errno.h>
#include <limits.h>
#include <time.h>

#include <unistd.h>
#include <sys/time.h>

#include "tnylpo.h"


/*
 * program name for error messages
 */
static const char *prog_name = NULL;


/*
 * file pointer for the log file (NULL if none is configured)
 */
static FILE *log_fp = NULL;


/*
 * write message to the log file, if a log file is configured
 */
static void
plog_va(const char *format, va_list params) {
	struct timeval tv;
	struct tm *tm_p;
	if (log_fp) {
		gettimeofday(&tv, NULL);
		tm_p = localtime(&tv.tv_sec);
		fprintf(log_fp, "%04d-%02d-%02d %02d:%02d:%02d.%03ld: ",
		    tm_p->tm_year + 1900, tm_p->tm_mon + 1, tm_p->tm_mday,
		    tm_p->tm_hour, tm_p->tm_min, tm_p->tm_sec,
		    (long) tv.tv_usec / 1000);
		vfprintf(log_fp, format, params);
		fprintf(log_fp, "\n");
		fflush(log_fp);
	}
}


/*
 * write a message to the log file
 */
void
plog(const char *format, ...) {
	va_list params;
	va_start(params, format);
	plog_va(format, params);
	va_end(params);
}


#define DUMP_LINE 16
#define WRAP(n) ((n) & 0xffff)
#define PRINT(c) (((c) >= 0x21 && (c) <= 0x7e) ? (c) : '.')

/*
 * dump a section of the Z80 memory to the log file; start + length
 * may overlap the end of memory (address 0x10000 wraps around to 0)
 */
void
plog_dump(int start, int length) {
	char buffer[DUMP_LINE * 4 + 10], *cp;
	int i, j, c;
	/*
	 * check if log file is open
	 */
	if (! log_fp) return;
	for (i = 0; i < length; ) {
		/*
		 * compress long stretches of uniform bytes in the dump:
		 * if there are more than two consecutive lines of the
		 * same byte value, they are replaced by a single line.
		 */
		c = memory[WRAP(start + i)];
		for (j = 1; i + j < length &&
		    memory[WRAP(start + i + j)] == c; j++);
		if (i + j < length) j = (j / DUMP_LINE) * DUMP_LINE;
		if (j > DUMP_LINE) {
			plog("%04x-%04x: all %02x (%c)", WRAP(start + i),
			    WRAP(start + i + j - 1), c, PRINT(c));
			i += j;
			continue;
		}
		cp = buffer;
		cp += sprintf(cp, "%04x:", WRAP(start + i));
		for (j = 0; j < DUMP_LINE && i + j < length; j++) {
			c = memory[WRAP(start + i + j)];
			cp += sprintf(cp, " %02x", c);
		}
		for (; j < DUMP_LINE; j++) cp += sprintf(cp, "   ");
		cp += sprintf(cp, " |");
		for (j = 0; j < DUMP_LINE && i + j < length; j++) {
			c = memory[WRAP(start + i + j)];
			cp += sprintf(cp, "%c", PRINT(c));
		}
		for (; j < DUMP_LINE; j++) cp += sprintf(cp, " ");
		cp += sprintf(cp, "|");
		plog("%s", buffer);
		i += DUMP_LINE;
	}
}


/*
 * write a message both to stderr and to the log file
 */
void
perr(const char *format, ...) {
	va_list params;
	/*
	 * once for stderr...
	 */
	va_start(params, format);
	fprintf(stderr, "%s: ", prog_name);
	vfprintf(stderr, format, params);
	fprintf(stderr, "\n");
	va_end(params);
	/*
	 * ... and once for the log file
	 */
	va_start(params, format);
	plog_va(format, params);
	va_end(params);
}


/*
 * display a short usage summary
 */
void
usage(void) {
	perr("usage: %s [ <options> ] command [ <parameters> ... ]",
	    prog_name);
	perr("valid <options> are");
	perr("    -a               use alternate charset");
	perr("    -b               use line mode console");
	perr("    -c (<n>|@)       number of full screen mode columns *");
	perr("    -d <drive>       set default drive");
	perr("    -e [h][b<bytes>|p<pages>|r[<addr>]-<addr>]:<fn>");
	perr("                     save memory to file <fn> after execution");
	perr("    -f <fn>          read configuration from file <fn>");
	perr("    -l (<n>|@)       number of full screen mode lines *");
	perr("    -n               never actually close files");
	perr("    -o (n|y|[y,]<fg>,<bg>)");
	perr("                     use colors *");
	perr("    -r               reverse backspace and delete keys *");
	perr("    -s               use full screen mode console");
	perr("    -t (<n>|@)       delay before exiting full screen mode *");
	perr("    -v <level>       set log level");
	perr("    -w               use alternative cursor key codes *");
	perr("    -y (n|<n>,<ns>)  add <ns> nanoseconds delay every <n> "
	    "instructions");
	perr("    -z {a|e|i|n|s|x} set dump options");
	perr("options with an asterisk (*) apply only to full screen mode");
}


/*
 * print error message if a command line option has been defined more than once
 */
static void
only_once(int c) {
	perr("option -%c may be specified only once", c);
}


/*
 * parse a screen dimension from the command line; dimensions have to be
 * decimal numbers in the range min..max or the @ sign for the current
 * screen size
 */
static int
parse_size(int c, int min, int max, const char *arg, int *size_p) {
	int rc = 0;
	unsigned long ul;
	char *cp;
	if (! strcmp(arg, "@")) {
		*size_p = (-1);
	} else {
		ul = strtoul(arg, &cp, 10);
		if (*cp || ul < min || ul > max) {
			perr("option -%c: argument out of range (%d...%d)",
			    c, min, max);
			rc = (-1);
		} else {
			*size_p = (int) ul;
		}
	}
	return rc;
}


/*
 * parse the CPU delay option parameter; this is either "n" for no
 * delay or two decimal integers separated by a comma
 */
static int
parse_delay(int *count_p, int *nanoseconds_p) {
	int rc = 0;
	unsigned long ul;
	char *cp;
	if (! strcmp(optarg, "n")) {
		*count_p = *nanoseconds_p = 0;
	} else {
		ul = strtoul(optarg, &cp, 10);
		if (ul < 1 || ul > INT_MAX) {
			perr("invalid count in -y option argument");
			rc = (-1);
		} else {
			*count_p = (int) ul;
			if (*cp != ',') {
				perr("comma expected in -y option argument");
				rc = (-1);
			} else {
				ul = strtoul(cp + 1, &cp, 10);
				if (ul < 1 || ul > INT_MAX) {
					perr("invalid nanosecond value in -y "
					    "option argument");
					rc = (-1);
				} else {
					*nanoseconds_p = (int) ul;
				}
			}
		}
	}
	return rc;
}


/*
 * helper function for parse_save()
 */
static void
range_err(void) {
	perr("option -e: range may be specified only once");
}


/*
 * parse an unsigned decimal integer
 */
static int
parse_int(const char **cpp) {
	int rc = (-1);
	unsigned long ul;
	char *rp;
	if (isdigit((unsigned char) **cpp)) {
		ul = strtoul(*cpp, &rp, 10);
		*cpp = rp;
		if (ul <= INT_MAX) rc = (int) ul;
	}
	return rc;
}


/*
 * parse a Z80 address in decimal, hexadecimal, or octal
 */
static int
parse_address(const char **cpp) {
	int rc = (-1);
	unsigned long ul;
	const char *cp = *cpp;
	char *rp;
	if (*cp == '0') {
		if (*(cp + 1) == 'x') {
			cp += 2;
			if (! isxdigit((unsigned char) *cp)) {
				goto premature_exit;
			}
			ul = strtoul(cp, &rp, 16);
		} else {
			ul = strtoul(cp, &rp, 8);
		}
	} else {
		if (! isdigit((unsigned char) *cp)) goto premature_exit;
		ul = strtoul(cp, &rp, 10);
	}
	*cpp = rp;
	if (ul >= MEMORY_SIZE) goto premature_exit;
	rc = (int) ul;
premature_exit:
	return rc;
}


/*
 * parse argument of the -e option
 */
static int
parse_save(void) {
	int rc = 0;
	int range_set = 0, n;
	const char *cp = optarg;
	while (*cp) {
		switch (*cp) {
		case 'h':
			/*
			 * save as Intel Hex file
			 */
			if (conf_save_hex) {
				perr("option -e: suboption h may be "
				    "specified only once");
				rc = (-1);
				goto premature_exit;
			}
			conf_save_hex = 1;
			cp++;
			break;
		case 'r':
			/*
			 * save arbitrary range of bytes
			 */
			if (range_set) {
				range_err();
				rc = (-1);
				goto premature_exit;
			}
			cp++;
			range_set = 1;
			/*
			 * if the first address is missing, 0x100 is assumed
			 */
			if (*cp != '-') {
				n = parse_address(&cp);
				if (n == (-1)) {
					perr("option -e: suboption r: "
					    "invalid start address");
					rc = (-1);
					goto premature_exit;
				}
				conf_save_start = n;
			} else {
				conf_save_start = 0x100;
			}
			if (*cp != '-') {
				perr("option -e: suboption r: range expected");
				rc = (-1);
				goto premature_exit;
			}
			cp++;
			n = parse_address(&cp);
			if (n == (-1) || n < conf_save_start) {
				perr("option -e: suboption r: "
				    "invalid end address");
				rc = (-1);
				goto premature_exit;
			}
			conf_save_end = n;
			break;
		case 'b':
			/*
			 * save number of bytes starting at 0x100
			 */
			if (range_set) {
				range_err();
				rc = (-1);
				goto premature_exit;
			}
			cp++;
			range_set = 1;
			n = parse_int(&cp);
			if (n < 1 || n > (MEMORY_SIZE - 0x100)) {
				perr("option -e: suboption b: "
				    "invalid byte count");
				rc = (-1);
				goto premature_exit;
			}
			conf_save_start = 0x100;
			conf_save_end = 0x100 + n - 1;
			break;
		case 'p':
			/*
			 * save number of pages starting at 0x100
			 */
			if (range_set) {
				range_err();
				rc = (-1);
				goto premature_exit;
			}
			cp++;
			range_set = 1;
			n = parse_int(&cp);
			if (n < 1 || n > (MEMORY_SIZE / 256 - 1)) {
				perr("option -e: suboption p: "
				    "invalid page count");
				rc = (-1);
				goto premature_exit;
			}
			conf_save_start = 0x100;
			conf_save_end = 0x100 + n * 256 - 1;
			break;
		case ':':
			/*
			 * rest of argument is file name
			 */
			cp++;
			conf_save_file = cp;
			cp += strlen(cp);
			break;
		default:
			/*
			 * illegal suboption
			 */
			perr("option -e: illegal suboption \'%c\'", *cp);
			rc = (-1);
			goto premature_exit;
		}
	}
	/*
	 * if no range has been specified, the whole TPA will be saved
	 */
	if (! range_set) {
		conf_save_start = 0x100;
		conf_save_end = get_tpa_end();
	}
	/*
	 * a file name must be specified and it may not be empty
	 */
	if (! conf_save_file || ! *conf_save_file) {
		perr("suboption -e: no file name specified");
		rc = (-1);
		goto premature_exit;
	}
premature_exit:
	return rc;
}


/*
 * helper function for parse_color()
 */
static void
perr_color(void) {
	perr("invalid -o option argument");
}


/*
 * helper function for parse_color(): parses a digit '0'..'7'
 */
static int
parse_colordigit(char *cp, char **cpp) {
	int rc = (-1);
	switch (*cp) {
	case '0': rc = 0; break;
	case '1': rc = 1; break;
	case '2': rc = 2; break;
	case '3': rc = 3; break;
	case '4': rc = 4; break;
	case '5': rc = 5; break;
	case '6': rc = 6; break;
	case '7': rc = 7; break;
	default: goto premature_exit;
	}
	cp++;
premature_exit:
	*cpp = cp;
	return rc;
}
		

/*
 * parse the color (-o) option parameter; this is either "n" for no
 * colors or "y" if colors should be used. If "y" is given, it may be
 * followed by a comma and two decimal, comma separated integers in the
 * range from 0 to 7, which select the default foreground and background
 * colors (if the colors are specified, the leading "y," is optional).
 */
static int
parse_color(void) {
	int rc = 0;
	char *cp;
	cp = optarg;
	/*
	 * no color
	 */
	conf_color = 0;
	if (*cp == 'n') {
		if (*(cp + 1) != '\0') {
			perr_color();
			rc = (-1);
		}
		goto premature_exit;
	}
	/*
	 * color requested
	 */
	conf_color = 1;
	if (*cp == 'y') {
		cp++;
		if (*cp == '\0') goto premature_exit;
		if (*cp != ',') {
			perr_color();
			rc = (-1);
			goto premature_exit;
		}
		cp++;
	}
	/*
	 * get foreground color
	 */
	conf_foreground = parse_colordigit(cp, &cp);
	if (conf_foreground == (-1) || *cp != ',') {
		perr_color();
		rc = (-1);
		goto premature_exit;
	}
	cp++;
	/*
	 * get background color
	 */
	conf_background = parse_colordigit(cp, &cp);
	if (conf_background == (-1) || *cp != '\0') {
		perr_color();
		rc = (-1);
		goto premature_exit;
	}
premature_exit:
	return rc;
}


/*
 * get the configuration
 *
 * After parsing the command line, further configuration values are
 * read from the optional configuration file
 */
static int
get_config(int argc, char **argv) {
	int rc = 0, opt, i;
	char *cfn = NULL, *cp;
	static const char valid_drives[] = "abcdefghijklmnop";
	size_t l;
	unsigned long ul;
	opterr = 0;
	while ((opt = getopt(argc, argv,
	    "abc:d:e:f:l:no:rst:v:wy:z:")) != EOF) {
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
		case 's':
			/*
			 * use terminal emulation (-s) or the line
			 * orientated console interface (-b)
			 */
			if (conf_interactive != (-1)) {
				perr("options -b and -s may be specified "
				    "only once and are mutually exclusive");
				rc = (-1);
			} else {
				conf_interactive = 1;
			}
			break;
		case 'b':
			/*
			 * use terminal emulation (-s) or the line
			 * orientated console interface (-b)
			 */
			if (conf_interactive != (-1)) {
				perr("options -b and -s may be specified "
				    "only once and are mutually exclusive");
				rc = (-1);
			} else {
				conf_interactive = 0;
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
		case 'l':
			/*
			 * number of lines of the VT52 emulation
			 */
			if (lines) {
				only_once('l');
				rc = (-1);
			}
			if (parse_size('l', MIN_LINES, MAX_LINES,
			    optarg, &lines)) {
				rc = (-1);
			}
			break;
		case 'c':
			/*
			 * number of columns of the VT52 emulation
			 */
			if (cols) {
				only_once('c');
				rc = (-1);
			}
			if (parse_size('c', MIN_COLS, MAX_COLS,
			    optarg, &cols)) {
				rc = (-1);
			}
			break;
		case 'd':
			/*
			 * initial default drive
			 */
			if (default_drive != (-1)) {
				only_once('d');
				rc = (-1);
			}
			l = strlen(optarg);
			cp = strchr(valid_drives, *optarg);
			if (l < 1 || l > 2 || (l == 2 && optarg[1] != ':')
			    || ! cp) {
				perr("invalid default drive");
				rc = (-1);
			} else {
				default_drive = cp - valid_drives;
			}
			break;
		case 'v':
			/*
			 * log level
			 */
			if (log_level != LL_UNSET) {
				only_once('v');
				rc = (-1);
			}
			ul = strtoul(optarg, &cp, 10);
			if (*cp || ul >= LL_INVALID) {
				perr("invalid log level");
				rc = (-1);
			} else {
				log_level = (enum log_level) ul;
			}
		       	break;
		case 'w':
			/*
			 * use WordStar cursor key sequences
			 */
			if (altkeys != (-1)) {
				only_once('w');
				rc = (-1);
			}
			altkeys = 1;
			break;
		case 'r':
			/*
			 * reverse backspace and delete keys
			 */
			if (reverse_bs_del != (-1)) {
				only_once('r');
				rc = (-1);
			}
			reverse_bs_del = 1;
			break;
		case 'n':
			/*
			 * don't actually close files closed
			 * by BDOS function 19
			 */
			if (dont_close != (-1)) {
				only_once('n');
				rc = (-1);
			}
			dont_close = 1;
			break;
		case 't':
			if (screen_delay != (-1)) {
				only_once('t');
				rc = (-1);
			}
			if (! strcmp(optarg, "@")) {
				screen_delay = (-2);
			} else {
				ul = strtoul(optarg, &cp, 10);
				if (*cp || ul > INT_MAX) {
					perr("invalid delay");
					rc = (-1);
				} else {
					screen_delay = (int) ul;
				}
			}
			break;
		case 'y':
			/*
			 * set CPU delay
			 *
			 * The parameter of the -y option takes the form
			 * of two comma separated integers; the first one
			 * is the number of instructions after which
			 * the delay is added, and the second one is the
			 * delay in nanoseconds.
			 *
			 * Alternatively, a parameter "n" overrides any
			 * CPU delay specified in the configuration file.
			 */
			if (delay_count != (-1)) {
				only_once('y');
				rc = (-1);
			} else {
				if (parse_delay(&delay_count,
				    &delay_nanoseconds)) rc = (-1);
			}
			break;
		case 'z':
			/*
			 * set dump configuration
			 *
			 * Adding a dump function (and using -z as the
			 * command line option) has been inspired by a
			 * branch by Eric Scott, which implemented a
			 * post mortem dump only.
			 */
			if (conf_dump) {
				only_once('z');
				rc = (-1);
			} else {
				/*
				 * parse valid suboptions
				 */
				for (cp = optarg; *cp; cp++) {
					switch (*cp) {
					case 'n':
						conf_dump |= DUMP_NONE;
						break;
					case 's':
						conf_dump |= DUMP_STARTUP;
						break;
					case 'x':
						conf_dump |= DUMP_EXIT;
						break;
					case 'i':
						conf_dump |= DUMP_SIGNAL;
						break;
					case 'e':
						conf_dump |= DUMP_ERROR;
						break;
					case 'a':
						conf_dump |= DUMP_ALL;
						break;
					default:
						perr("illegal -z suboption "
						    "\'%c\'", (int) *cp);
						rc = (-1);
						break;
					}
				}
				/*
				 * check for valid combinations
				 */
				if (((conf_dump & DUMP_ALL) &&
				    (conf_dump & ~DUMP_ALL)) ||
				    ((conf_dump & DUMP_NONE) &&
				    (conf_dump & ~DUMP_NONE)) ||
				    ((conf_dump & DUMP_EXIT) &&
				    (conf_dump & DUMP_ERROR))) {
					perr("inconsistent -z suboptions");
					rc = (-1);
				}
				/*
				 * expand -za macro option
				 */
				if (conf_dump & DUMP_ALL) {
					conf_dump |= DUMP_STARTUP |
					    DUMP_EXIT | DUMP_SIGNAL;
				}
			}
			break;
		case 'e':
			/*
			 * save (part of) the Z80 memory after
			 * program execution
			 */
			if (conf_save_file) {
				only_once('e');
				rc = (-1);
			} else {
				/*
				 * the -e option argument is complex,
				 * so it is parsed in a separate function
				 */
				if (parse_save()) rc = (-1);
			}
			break;
		case 'o':
			/*
			 * use terminal colors if available
			 */
			if (conf_color != (-1)) {
				only_once('o');
				rc = (-1);
			} else {
				/*
				 * the -o option argument is complex,
				 * so it is parsed in a separate function
				 */
				if (parse_color()) rc = (-1);
			}
			break;
		case '?':
			perr("invalid option -%c", optopt);
			rc = (-1);
			break;
		}
	}
	/*
	 * there must be a command name on the command line; further
	 * parameters are passed to the CP/M emulator as CP/M command
	 * line parameters
	 */
	if (argc - optind) {
		conf_command = argv[optind];
		conf_argc = argc - optind - 1;
		conf_argv = argv + optind + 1;
	} else {
		perr("command name expected");
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
	if (rc) goto premature_exit;
	/*
	 * default drive if none specified is A
	 */
	if (default_drive == (-1)) default_drive = 0;
	/*
	 * default screen size for the VT52 emulation
	 */
	if (! lines) lines = 24;
	if (! cols) cols = 80;
	/*
	 * massage screen delay value
	 */
	switch (screen_delay) {
	case (-2): screen_delay = (-1); break;
	case (-1): screen_delay = 0; break;
	}
	/*
	 * default log level is LL_ERRORS
	 */
	if (log_level == LL_UNSET) log_level = LL_ERRORS;
	/*
	 * character I/O is by default text
	 */
	if (conf_printer_raw == (-1)) conf_printer_raw = 0;
	if (conf_punch_raw == (-1)) conf_punch_raw = 0;
	if (conf_reader_raw == (-1)) conf_reader_raw = 0;
	/*
	 * if not a single drive is defined, define drive a: as
	 * the current working directory
	 */
	for (i = 0; i < 16 && ! conf_drives[i]; i++);
	if (i == 16) {
		conf_drives[0] = alloc(2);
		strcpy(conf_drives[0], ".");
	}
	/*
	 * default drive must be defined
	 */
	if (! conf_drives[default_drive]) {
		perr("default drive has no definition");
		rc = (-1);
	}
	/*
	 * close files by default
	 */
	if (dont_close == (-1)) dont_close = 0;
	/*
	 * use VT52 cursor keys by default
	 */
	if (altkeys == (-1)) altkeys = 0;
	/*
	 * don't reverse backspace and delete keys by default
	 */
	if (reverse_bs_del == (-1)) reverse_bs_del = 0;
	/*
	 * massage color option
	 */
	if (conf_color == (-1)) conf_color = 0; /* no color */
	if (conf_foreground == (-1)) conf_foreground = 7; /* white */
	if (conf_background == (-1)) conf_background = 0; /* black */
	/*
	 * default mode is batch
	 */
	if (conf_interactive == (-1)) conf_interactive = 0;
premature_exit:
	return rc;
}


/*
 * For once, no comment.
 */
int
main(int argc, char **argv) {
	int rc = 0;
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
	 * for security reasons, refuse to run as root user
	 */
	if (geteuid() == 0) {
		perr("I'm sorry, but I refuse to run as super user.");
		rc = (-1);
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
	 * open log file, if one is configured
	 */
	if (conf_log) {
		log_fp = fopen(conf_log, "a");
		if (! log_fp) {
			perr("cannot open log file %s: %s", conf_log,
			    strerror(errno));
			rc = (-1);
			goto premature_exit;
		}
		if (log_level > LL_ERRORS) plog("log opened");
	}
	/*
	 * initialize CPU and OS emulation
	 */
	rc = cpu_init();
	if (rc) goto premature_exit;
	/*
	 * initialize console emulation
	 */
	rc = console_init();
	if (! rc) {
		/*
		 * run program in emulated environment;
		 * errors are indicated by the return
		 * code of cpu_exit();
		 */
		cpu_run();
		/*
		 * clean up console
		 */
		if (console_exit()) rc = (-1);
	}
	/*
	 * clean up after emulation run
	 */
	if (cpu_exit()) rc = (-1);
	/*
	 * close character devices
	 */
	if (finalize_chario()) rc = (-1);
premature_exit:
	/*
	 * close log file, if one is open
	 */
	if (log_fp) {
		if (log_level > LL_ERRORS) plog("log closed");
		if (fclose(log_fp)) {
			perr("cannot close log file %s: %s", conf_log,
			    strerror(errno));
			rc = (-1);
		}
	}
	exit(rc ? EXIT_FAILURE : EXIT_SUCCESS);
	return 0;
}
