/*
 * Copyright (c) 2023 Georg Brein. All rights reserved.
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


/*
 * creates or modifies disk images for use with tcpm
 */


#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include <sys/types.h>
#include <unistd.h>


enum cmd {
	CMD_INVALID = 0,
	CMD_CREATE = 1,
	CMD_SYS = 2,
	CMD_NOSYS = 3
};


/*
 * program name for error messages
 */
static char *prog_name = NULL;
/*
 * command line parameters and options
 */
static char *movcpm_fn = NULL, *system_fn = NULL, *program_fn = NULL,
    *image_fn = NULL;
static int overwrite = 0, pad_image = 0;
static enum cmd command = CMD_INVALID;
/*
 * system image (always 7168 bytes in size)
 */
static char *sys_image = NULL;
/*
 * program image (the buffer is padded to the block size of 4096 bytes)
 */
static char *prog_image = NULL;
/*
 * length of prog_image in sectors (128b) resp. blocks (32 * 128b)
 */
static size_t prog_sectors = 0, prog_blocks = 0;
/*
 * program name in CP/M format (blank padded, not terminated)
 */
static char program_name[11];


/*
 * display message on stderr
 */
static void
perr(const char *format, ...) {
	va_list params;
	va_start(params, format);
	fprintf(stderr, "%s: ", prog_name);
	vfprintf(stderr, format, params);
	fprintf(stderr, "\n");
	va_end(params);
}


/*
 * print usage information
 */
static void
usage(void) {
	perr("usage:", prog_name);
	perr("    %s [<sys option>] [-p <program>] [-f] [-a] create <image>",
	    prog_name);
	perr("    %s <sys option> sys <image>", prog_name);
	perr("    %s nosys <image>", prog_name);
	perr("<sys option> is one of");
	perr("    -m <movcpm image>   get system from a MOVCPM image");
	perr("    -s <system image>   get system from a raw system image");
	perr("other options and parameters");
	perr("    -p <program>  add CP/M .COM file <program> to the disk "
	    "image");
	perr("    -a            pad disk image to full size");
	perr("    -f            overwite existing image");
	perr("    <image>       name of the disk image to create/modify");
}


/*
 * parse command line arguments
 */
static int
parse_command(int argc, char **argv) {
	int opt;
	char *cmd;
	opterr = 0;
	while ((opt = getopt(argc, argv, "m:s:p:fa")) != (-1)) {
		switch (opt) {
		case 'm':
			movcpm_fn = optarg;
			break;
		case 's':
			system_fn = optarg;
			break;
		case 'p':
			program_fn = optarg;
			break;
		case 'a':
			pad_image = 1;
			break;
		case 'f':
			overwrite = 1;
			break;

		default:
			perr("invalid option <-%c>", optopt);
			return (-1);
		}
	}
	if (movcpm_fn && system_fn) {
		perr("options -m and -s are mutually exclusive");
		return (-1);
	}
	switch (argc - optind) {
	case 0:
	case 1:
		perr("too few arguments");
		return (-1);
	case 2:
		cmd = argv[optind];
		image_fn = argv[optind + 1];
		break;
	default:
		perr("too many arguments");
		return (-1);
	}
	if (! strcmp(cmd, "create")) {
		command = CMD_CREATE;
	} else if (! strcmp(cmd, "sys")) {
		command = CMD_SYS;
		if (program_fn || overwrite || pad_image) {
			perr("ignoring inappropriate option(s) "
			    "(-p, -a, or -f)");
		}
		if (! movcpm_fn && ! system_fn) {
			perr("either -m or -s option must be supplied");
			return (-1);
		}
	} else if (! strcmp(cmd, "nosys")) {
		command = CMD_NOSYS;
		if (program_fn || movcpm_fn || system_fn || overwrite ||
		    pad_image) {
			perr("ignoring inappropriate option(s) "
			    "(-p, -m, -s, -a, or -f)\n");
		}
	} else {
		perr("invalid command <%s>", cmd);
		return (-1);
	}
	return 0;
}

/*
 * length of CP/M 2 CCP + BDOS
 */
#define SYSTEM_LENGTH (0x800+0xe00)

/*
 * get system image from specified file
 */
static int
get_system(const char *fn, int movcpm) {
	int rc = 0;
	size_t l;
	FILE *fp = NULL;
	fp = fopen(fn, "rb");
	if (! fp) {
		perr("cannot open %s: %s", fn, strerror(errno));
		rc = (-1);
		goto premature_exit;
	}
	sys_image = malloc(SYSTEM_LENGTH);
	if (! sys_image) {
		perr("out of memory");
		rc = (-1);
		goto premature_exit;
	}
	if (movcpm) {
		/*
		 * in a file generated by MOVCPM nn * / SAVE 34 CPMnn.COM 
		 * the start of the CCP is at offset 0x880; in a raw image,
		 * the CCP is expected to start at offset 0
		 */
		if (fseek(fp, 0x880L, SEEK_SET) == (-1)) {
			perr("%s: cannot seek to offset 0x880: %s",
				fn, strerror(errno));
			rc = (-1);
			goto premature_exit;
		}
	}
	l = fread(sys_image, 1, SYSTEM_LENGTH, fp);
	if (! l && ferror(fp)) {
		perr("%s: cannot read: %s", fn, strerror(errno));
		rc = (-1);
		goto premature_exit;
	}
	/*
	 * the file may be longer than SYSTEM_LENGTH (e.g., because it
	 * contains BIOS code), but must contain at least the CCP and the BDOS
	 */
	if (l != SYSTEM_LENGTH) {
		perr("%s: file too short", fn);
		rc = (-1);
		goto premature_exit;
	}
premature_exit:
	if (rc) {
		free(sys_image);
		sys_image = NULL;
	}
	fclose(fp);
	return rc;
}


/*
 * read program file
 */
static int
get_program(const char *fn) {
	int rc = 0, i, c;
	FILE *fp = NULL;
	const char *name, *ext, *dot;
	static const char valid[] = "@$#-0123456789ABCDEFGHIJKLMNOPqRSTUVWXYZ";
	size_t l, tl, l_name, l_ext;
	char *tp;
	/*
	 * check file name for compatibility with CP/M and construct
	 * a CP/M name from it
	 */
	dot = strchr(program_fn, '.');
	if (! dot) {
		name = program_fn;
		l_name = strlen(name);
		ext = name + l_name;
		l_ext = 0;
	} else {
		name = program_fn;
		l_name = dot - name;
		ext = dot + 1;
		l_ext = strlen(ext);
	}
	while (l_name && name[l_name - 1] == ' ') l_name--;
	while (l_ext && ext[l_ext - 1] == ' ') l_ext--;
	if (l_name < 1 || l_name > 8 || (dot && l_ext < 1) || l_ext > 3) {
		rc = (-1);
	} else {
		memset(program_name, ' ', 11);
		for (i = 0; i < l_name; i++) {
			c = (unsigned char) name[i];
			c = toupper(c);
			if (! strchr(valid, c)) {
				rc = (-1);
			} else {
				program_name[i] = c;
			}
		}
		for (i = 0; i < l_ext; i++) {
			c = (unsigned char) ext[i];
			c = toupper(c);
			if (! strchr(valid, c)) {
				rc = (-1);
			} else {
				program_name[i + 8] = c;
			}
		}
	}
	if (rc) {
		perr("%s: invalid CP/M file name", program_fn);
		goto premature_exit;
	}
	/*
	 * read CP/M program file (this may not be longer than 64KB)
	 */
	prog_image = malloc(1024 * 64 + 1);
	if (! prog_image) {
		perr("out of memory");
		rc = (-1);
		goto premature_exit;
	}
	fp = fopen(program_fn, "rb");
	if (! fp) {
		perr("%s: cannot open: %s", program_fn, strerror(errno));
		rc = (-1);
		goto premature_exit;
	}
	/*
	 * To keep things simple (only one directory entry for the
	 * program file), the file size is limited to 64Kb - 128,
	 * i.e. 511 sectors; this shouldn't be a problem, since even
	 * this size would be much too large for any CP/M-80 TPA.
	 */
	l = fread(prog_image, 1, 1024 * 64 - 128 + 1, fp);
	if (l == 0 && ferror(fp)) {
		perr("%s: cannot read: %s", program_fn, strerror(errno));
		rc = (-1);
		goto premature_exit;
	}
	if (l == 1024 * 64 - 128 + 1) {
		perr("%s: file to large", program_fn);
		rc = (-1);
		goto premature_exit;
	}
	/*
	 * calculate the length of the program file in blocks and pad it to
	 * this size
	 */
	prog_blocks = (l + 4095) / 4096;
	prog_sectors = (l + 127) / 128;
	memset(prog_image + l, 0, prog_blocks * 4096 - l);
	tp = realloc(prog_image, prog_blocks * 4096);
	if (! tp) {
		perr("out of memory");
		rc = (-1);
		goto premature_exit;
	}
	prog_image = tp;
premature_exit:
	if (rc) {
		free(prog_image);
		prog_image = NULL;
	}
	fclose(fp);
	return rc;
}


/*
 * open existing image
 */
static FILE *
open_image(const char *fn) {
	FILE *fp = NULL;
	char buffer[128];
	size_t l;
	fp = fopen(fn, "r+b");
	if (! fp) {
		perr("%s: cannot open: %s", fn, strerror(errno));
		goto premature_exit;
	}
	/*
	 * check for image signature
	 */
	l = fread(buffer, 1, 128, fp);
	if (! l && ferror(fp)) {
		perr("%s: cannot read: %s", fn, strerror(errno));
		fclose(fp);
		fp = NULL;
		goto premature_exit;
	}
	if (l != 128 || buffer[0] != 'D' || (buffer[1] != 'D' &&
	    buffer[1] != 'S')) {
		perr("%s: not a disk image", fn);
		fclose(fp);
		fp = NULL;
		goto premature_exit;
	}
	if (fseek(fp, 0L, SEEK_SET)) {
		perr("%s: cannot rewind: %s", fn, strerror(errno));
		fclose(fp);
		fp = NULL;
		goto premature_exit;
	}
premature_exit:
	return fp;
}


/*
 * write a sector to the disk image
 */
static int
write_sector(FILE *fp, const char buffer[128]) {
	int rc = 0;
	size_t l;
	l = fwrite(buffer, 1, 128, fp);
	if (l != 128 && ferror(fp)) {
		perr("%s: write error: %s", image_fn, strerror(errno));
		rc = (-1);
	} else if (l != 128) {
		perr("%s: short write", image_fn);
		rc = (-1);
	}
	return rc;
}


/*
 * write system to first track of the disk image
 */
static int
do_sys(void) {
	int rc = 0;
	FILE *fp = NULL;
	char buffer[128];
	size_t i;
	fp = open_image(image_fn);
	if (! fp) {
		rc = (-1);
		goto premature_exit;
	}
	/*
	 * write systemdisk signature
	 */
	memset(buffer, 0xe5, 128);
	buffer[0] = 'D';
	buffer[1] = 'S';
	rc = write_sector(fp, buffer);
	if (rc) goto premature_exit;
	/*
	 * write CP/M to disk image
	 */
	for (i = 0; i < SYSTEM_LENGTH; i += 128) {
		rc = write_sector(fp, sys_image + i);
		if (rc) goto premature_exit;
	}
premature_exit:
	if (fclose(fp)) {
		perr("%s: cannot close: %s", image_fn,
		    strerror(errno));
		rc = (-1);
	}
	return rc;
}


/*
 * convert disk image to data only disk
 */
static int
do_nosys(void) {
	int rc = 0, i;
	FILE *fp = NULL;
	char buffer[128];
	fp = open_image(image_fn);
	if (! fp) {
		rc = (-1);
		goto premature_exit;
	}
	/*
	 * write data disk signature
	 */
	memset(buffer, 0xe5, 128);
	buffer[0] = buffer[1] = 'D';
	rc = write_sector(fp, buffer);
	if (rc) goto premature_exit;
	/*
	 * overwrite CP/M system
	 */
	buffer[0] = buffer[1] = 0xe5;
	for (i = 0; i < SYSTEM_LENGTH / 128; i++) {
		rc = write_sector(fp, buffer);
		if (rc) goto premature_exit;
	}
premature_exit:
	if (fclose(fp)) {
		perr("%s: cannot close: %s", image_fn,
		    strerror(errno));
		rc = (-1);
	}
	return rc;
}


/*
 * create a new disk image
 */
static int
do_create(void) {
	int rc = 0;
	FILE *fp = NULL;
	char buffer[128];
	int i, ex;
	/*
	 * check for existence of the named image
	 */
	if (! overwrite) {
		fp = fopen(image_fn, "rb");
		if (fp) {
			perr("%s already exists - specify -f option",
			   image_fn);
			rc = (-1);
			fclose(fp);
			fp = NULL;
			goto premature_exit;
		}
	}
	/*
	 * create image file
	 */
	fp = fopen(image_fn, "wb");
	if (! fp) {
		perr("%s: cannot create: %s", image_fn, strerror(errno));
		rc = (-1);
		goto premature_exit;
	}
	/*
	 * write image signature
	 */
	memset(buffer, 0xe5, 128);
	buffer[0] = 'D';
	buffer[1] = sys_image ? 'S' : 'D';
	rc = write_sector(fp, buffer);
	if (rc) goto premature_exit;
	if (sys_image) {
		/*
		 * write system image to track 0
		 */
		for (i = 0; i < SYSTEM_LENGTH; i += 128) {
			rc = write_sector(fp, sys_image + i);
			if (rc) goto premature_exit;
		}
		i = 64 - 1 - SYSTEM_LENGTH / 128;
	} else {
		i = 64 - 1;
	}
	/*
	 * pad track 0 with empty sectors
	 */
	buffer[0] = buffer[1] = 0xe5;
	for (; i; i--) {
		rc = write_sector(fp, buffer);
		if (rc) goto premature_exit;
	}
	if (prog_image) {
		/*
		 * create directory entry for program file
		 */
		memset(buffer, 0, 32);
		memcpy(buffer + 1, program_name, 11);
		/*
		 * EX field
		 */
		buffer[12] = ex = (prog_sectors - 1) / 128;
		/*
		 * RC field
		 */
		buffer[15] = prog_sectors - (ex * 128);
		/*
		 * program file will be on the first prog_blocks blocks
		 * after the two directory blocks
		 */
		for (i = 0; i < prog_blocks; i++) buffer[16 + i] = i + 2;
		/*
		 * write first directory sector
		 */
		rc = write_sector(fp, buffer);
		if (rc) goto premature_exit;
		memset(buffer, 0xe5, 32);
		i = 64 - 1;
	} else {
		i = 64;
	}
	/*
	 * pad the directory (= track 1) with empty sectors
	 */
	for (; i; i--) {
		rc = write_sector(fp, buffer);
		if (rc) goto premature_exit;
	}
	if (prog_image) {
		/*
		 * write the sectors of the program to the disk image
		 */
		for (i = 0; i < prog_blocks * 4096; i += 128) {
			rc = write_sector(fp, prog_image + i);
			if (rc) goto premature_exit;
		}
		/*
		 * remaining sector count for padding
		 */
		i = 64 * 128 - 2 * 64 - prog_blocks * 32;
	} else {
		/*
		 * remaining sector count for padding (no program)
		 */
		i = 64 * 128 - 2 * 64;
	}
	if (pad_image) {
		/*
		 * padding disk image with empty sectors
		 */
		for (; i; i--) {
			rc = write_sector(fp, buffer);
			if (rc) goto premature_exit;
		}
	}
premature_exit:
	if (fclose(fp)) {
		perr("%s: cannot close: %s", image_fn,
		    strerror(errno));
		rc = (-1);
	}
	return rc;
}


int
main(int argc, char **argv) {
	int rc = 0;
	/*
	 * extract base name of program
	 */
	for (prog_name = argv[0] + strlen(argv[0]);
	    prog_name != argv[0] && *(prog_name - 1) != '/'; prog_name--);
	/*
	 * parse command line
	 */
	rc = parse_command(argc, argv);
	if (rc) {
		usage();
		goto premature_exit;
	}
	/*
	 * read system image
	 */
	if (command == CMD_CREATE || command == CMD_SYS) {
		if (movcpm_fn) {
			rc = get_system(movcpm_fn, 1);
		} else if (system_fn) {
			rc = get_system(system_fn, 0);
		}
		if (rc) goto premature_exit;
	}
	/*
	 * read program file
	 */
	if (command == CMD_CREATE && program_fn) {
		rc = get_program(program_fn);
		if (rc) goto premature_exit;
	}
	/*
	 * process command
	 */
	switch (command) {
	case CMD_CREATE:
		rc = do_create();
		break;
	case CMD_SYS:
		rc = do_sys();
		break;
	case CMD_NOSYS:
		rc = do_nosys();
		break;
	default:
		break;
	}
premature_exit:
	return (rc ? EXIT_FAILURE : EXIT_SUCCESS);
	return 0;
}
