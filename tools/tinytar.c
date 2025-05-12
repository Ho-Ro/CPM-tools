/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * tinytar.c - A minimal ANSI C89-compatible TAR archive utility.
 *
 * This program implements a simplified version of the UNIX tar utility,
 * supporting the creation, listing, extraction, and appending of files
 * to a standard POSIX/GNU-compatible TAR archive (USTAR format).
 *
 * Supported Modes (UNIX-compatible syntax):
 *   -cf archive.tar file1 [file2 ...]  # Create a new archive from files
 *   -rf archive.tar file1 [file2 ...]  # Append files to an existing archive
 *   -xf archive.tar                    # Extract all files from an archive
 *   -tf archive.tar                    # List contents of an archive
 *   archive.tar                        # Same as -tf archive.tar
 *
 * Features:
 *   - Fully ANSI C89 compatible (no POSIX-specific functions used).
 *   - Uses only the standard C library (stdio.h, string.h, etc.).
 *   - Validates USTAR magic to ensure archive format correctness.
 *   - Appending safely handles existing TAR structure and trailing blocks.
 *   - Automatically overwrites and rewrites final zero blocks.
 *   - Stores file modification time (or current time if mtime is missing).
 *   - Creates smaller files than (uncompressed) UNIX tar because it puts
 *     only 2 empty blocks at the archive end (as defined in the standard)
 *     instead of padding the archive to a multiple of 10 KiBi.
 *
 * Limitations:
 *   - Flat archives only (no directory tree structure).
 *   - No support for special files (symlinks, devices, etc.).
 *   - No built-in compression (to maintain POSIX/GNU tar compatibility).
 *
 * Target:
 *   The program is mainly intended for small 8-bit systems like CP/M 3.0
 *   that do provide neither directories nor specials files.
 *   It supports the grouping of multiple files and the transfer between
 *   CP/M and Linux with tools like KERMIT or [XYZ]MODEM.
 *
 * Building on Linux:
 *   gcc -Wall -Wextra -Wpedantic -std=c89 -o tinytar tinytar.c
 *   Keep the name "tinytar" to distinguish it from real tar.
 *
 * Building for CP/M with Z88DK:
 *   zcc +cpm -mz80 -v -otar.com tinytar.c
 *   This creates the file TAR.COM that resembles the std. tar commands.
 *
 * Author: Martin Homuth-Rosemann
 * License: GPL-3.0-or-later
 */

#define VERSION "20250512"

#include <ctype.h>    /* isprint */
#include <stdio.h>    /* fopen, fread, fwrite, fclose, printf, sprintf */
#include <stdlib.h>   /* wcmatch, malloc, free, exit */
#include <string.h>   /* strncpy, memset, strlen */
#include <sys/stat.h> /* for stat, struct stat, S_IFREG */
#include <time.h>     /* time_t */
#ifdef CPM
#include <cpm.h>
#endif

#define RECORD_SIZE 512
unsigned char record[ RECORD_SIZE ];

#define NAME_SIZE 101
char filename[ NAME_SIZE ];

#define T_19800101 315532800L
time_t now;

/* -------------------- TAR HEADER STRUCTURE -------------------- */
struct tar_header {
    char name[ 100 ];     /* ascii */
    char mode[ 8 ];       /* octal 0644 */
    char uid[ 8 ];        /* octal 0 */
    char gid[ 8 ];        /* octal 0 */
    char size[ 12 ];      /* octal */
    char mtime[ 12 ];     /* octal */
    char chksum[ 8 ];     /* octal */
    char typeflag;        /* char */
    char linkname[ 100 ]; /* ignore */
    char magic[ 8 ];      /* "ustar  " */
    char uname[ 32 ];     /* ascii "user" */
    char gname[ 32 ];     /* ascii "group" */
    char devmajor[ 8 ];   /* ignore */
    char devminor[ 8 ];   /* ignore */
    char prefix[ 155 ];   /* ignore */
    char pad[ 12 ];       /* fill to 512 byte */
};


/* -------------------- HELPERS -------------------- */

#ifdef CPM
void chk_ctrl_c( void ) {
    if ( bdos( 6, 0xff ) == 3 ) { /* Ctrl C was typed */
        fprintf( stderr, "\n^C\n" );
        exit( -1 );
    }
}
#else
#define chk_ctrl_c()
#endif


long octal_to_long( const char *str, int len ) {
    long value = 0;
    while ( len-- ) {
        if ( *str >= '0' && *str <= '7' )
            value = ( value << 3 ) + ( *str - '0' );
        ++str;
    }
    return value;
}


void long_to_octal( char *out, size_t size, long val ) {
    snprintf( out, size, "%0*lo", (int)( size - 1 ), val );
    out[ size - 1 ] = '\0';
}


int is_block_empty( const unsigned char *block ) {
    int i = RECORD_SIZE;
    while ( i-- )
        if ( *block++ != 0 )
            return 0;
    return 1;
}


int is_valid_tar_header( const unsigned char *header ) { return strncmp( (char *)header + 257, "ustar", 5 ) == 0; }


long find_append_position( FILE *fp ) {
    long filesize, skip, zpos;

    fseek( fp, 0, SEEK_SET );

    while ( fread( record, 1, RECORD_SIZE, fp ) == RECORD_SIZE ) {
        if ( is_block_empty( record ) ) {
            /* Possible start of trailing zero blocks */
            zpos = ftell( fp ) - RECORD_SIZE;
            /* Check if the next block is also zero */
            if ( fread( record, 1, RECORD_SIZE, fp ) == RECORD_SIZE && is_block_empty( record ) )
                /* Found trailing padding  stop here */
                return zpos;
            else
                /* False alarm  continue */
                fseek( fp, -RECORD_SIZE, SEEK_CUR );
        }

        if ( !is_valid_tar_header( record ) )
            return -1; /* Invalid TAR */

        /* Get file size and skip content */
        filesize = octal_to_long( (char *)( record + 124 ), 12 );
        skip = ( ( filesize + RECORD_SIZE - 1 ) / RECORD_SIZE ) * RECORD_SIZE;
        fseek( fp, skip, SEEK_CUR );
    }

    /* If we reached EOF with no trailing zeros: append at EOF */
    return ftell( fp );
}


long get_file_size( FILE *f ) {
    long size;
    fseek( f, 0, SEEK_END );
    size = ftell( f );
    fseek( f, 0, SEEK_SET );
    return size;
}


void write_tar_header( FILE *out, const char *filename, long filesize, long mtime ) {
    unsigned int i, checksum;
    struct tar_header header;

    memset( &header, 0, sizeof( header ) );
    strncpy( header.name, filename, 100 );
    long_to_octal( header.mode, 8, 0644 );
    long_to_octal( header.uid, 8, 1000 );
    long_to_octal( header.gid, 8, 1000 );
    long_to_octal( header.size, 12, filesize );
    long_to_octal( header.mtime, 12, mtime );
    memset( header.chksum, ' ', 8 );
    header.typeflag = '0';
    strncpy( header.magic, "ustar  ", 8 );
    strncpy( header.uname, "user", 32 );
    strncpy( header.gname, "group", 32 );

    checksum = 0;
    for ( i = 0; i < sizeof( header ); ++i )
        checksum += ( (unsigned char *)&header )[ i ];

    sprintf( header.chksum, "%06o", checksum );
    header.chksum[ 6 ] = '\0';
    header.chksum[ 7 ] = ' ';

    fwrite( &header, 1, RECORD_SIZE, out );
}


void write_file_content( FILE *out, FILE *in, long filesize ) {
    size_t n;
    long remaining = filesize;

    while ( remaining > 0 ) {
        chk_ctrl_c();
        n = fread( record, 1, RECORD_SIZE, in );
        if ( n < RECORD_SIZE )
            memset( record + n, 0, RECORD_SIZE - n );
        fwrite( record, 1, RECORD_SIZE, out );
        remaining -= n;
    }
}


void write_file( char *filename, FILE *tar ) {
    FILE *in;
    struct stat st;
    long filesize, mtime;

    in = fopen( filename, "rb" );
    if ( !in ) {
        perror( filename );
        return;
    }

    if ( stat( filename, &st ) != 0 || !S_ISREG( st.st_mode ) ) {
        fprintf( stderr, "Skipping: %s (not a regular file)\n", filename );
        fclose( in );
        return;
    }
#ifdef CPM
    mtime = st.st_atime;
    if ( mtime <= T_19800101 ) /* no valid timestamp */
        mtime = now;
#else
    mtime = st.st_mtime;
#endif
    filesize = get_file_size( in );
    fprintf( stderr, "%s (%ld)\n", filename, filesize );

    write_tar_header( tar, filename, filesize, mtime );
    write_file_content( tar, in, filesize );
    fclose( in );
}

#ifdef __Z88DK
#define MAX_FILES 1024
#define CPM_NAME_SIZE 12
char *argnames;
uint16_t argnum;

struct fcb fc_dir;

char fc_dirpos;
char *fc_dirbuf;
uint16_t dir_current_pos;
char dirbuf[ 128 ];

int dir_find_first( char *p ) {
    fc_dirbuf = dirbuf;
    bdos( CPM_SDMA, fc_dirbuf );
    parsefcb( &fc_dir, p );
    fc_dirpos = bdos( CPM_FFST, &fc_dir );
    /* fprintf( stderr, "dir_find_first(): %d\n", fc_dirpos ); */
    dir_current_pos = 0;
    /* let's simulate FLOS error code $24 (= Reached end of directory) */
    return ( fc_dirpos == -1 ? 0x24 : 0 );
}

uint16_t entry_count;
uint16_t current_entry;

int dir_find_next( char *p ) {
    current_entry = dir_current_pos;
    dir_current_pos = current_entry + 1;
    fc_dirpos = bdos( CPM_FNXT, &fc_dir );
    /* fprintf( stderr, "dir_find_next(): %d %d\n", current_entry, fc_dirpos ); */
    /* let's simulate FLOS error code $24 (= Reached end of directory) */
    return ( fc_dirpos == -1 ? 0x24 : 0 );
}


void save_entry_name( int num ) {
    char *source, *dest;
    source = fc_dirbuf + fc_dirpos * 32 + 1;
    dest = argnames + num * CPM_NAME_SIZE;
    memcpy( dest, source, CPM_NAME_SIZE );
}


char *get_entry_name( int num ) {
    static char tmpnam[ 13 ]; // temp filename buffer
    uint8_t iii;
    char *source = argnames + num * CPM_NAME_SIZE;
    char *dest = tmpnam;

    for ( iii = 0; iii < 8; iii++ ) {
        if ( *source == ' ' )
            break;
        *dest++ = *source++ & 0x7F;
    }
    source = argnames + num * CPM_NAME_SIZE + 8;
    if ( *source != ' ' ) {
        *dest++ = '.';
        *dest++ = *source++;
        for ( iii = 9; iii < 11; iii++ ) {
            if ( *source == ' ' )
                break;
            *dest++ = *source++ & 0x7F;
        }
    }
    *dest = '\0';

    return tmpnam;
}

#endif


/* ------------------------------------------------------ */
/* --------------- CREATE OR APPEND MODE ---------------- */
/* ------------------------------------------------------ */
void mode_create_append( int append, int argc, char *argv[] ) {
    FILE *tar;
    int i;

    if ( append ) {
        long append_pos;
        tar = fopen( argv[ 2 ], "r+b" );
        if ( !tar ) {
            perror( "Cannot open archive" );
            exit( 1 );
        }
        append_pos = find_append_position( tar );
        if ( append_pos < 0 ) {
            fprintf( stderr, "Invalid or corrupt TAR archive\n" );
            fclose( tar );
            exit( 1 );
        }
        fseek( tar, append_pos, SEEK_SET );
    } else {
        tar = fopen( argv[ 2 ], "wb" );
        if ( !tar ) {
            perror( "Cannot create archive" );
            exit( 1 );
        }
    }

#ifdef __Z88DK
    char *filename;
    /* create the big buffer on the stack and not in .bss (that goes into binary) */
    char argnamesbuf[ MAX_FILES * CPM_NAME_SIZE ];
    argnames = argnamesbuf;
    argnum = 0;
    /* parse wildcard args and put all raw CP/M filenames into argnames buffer*/
    for ( i = 3; i < argc; ++i ) {
        int x;
        if ( ( x = dir_find_first( argv[ i ] ) ) != 0 ) /* no match */
            continue;

        while ( x == 0 ) { /* while match */
            chk_ctrl_c();
            if ( argnum >= MAX_FILES ) {
                fprintf( stderr, "too many input files\n" );
                exit( -1 );
            }
            save_entry_name( argnum++ ); /* put raw name into big buffer */
            x = dir_find_next( argv[ i ] );
        }
    }
    /* now process all filenames in argnames buffer */
    for ( i = 0; i < argnum; ++i ) {
        chk_ctrl_c();
        filename = get_entry_name( i );        /* format raw CP/M filename */
        if ( strcmp( filename, argv[ 2 ] ) ) { /* exclude this target archive */
            /* fprintf( stderr, "%s\n", filename ); */
            write_file( filename, tar );
        }
    }

#else

    for ( i = 3; i < argc; ++i ) {
        chk_ctrl_c();
        write_file( argv[ i ], tar );
    }
#endif

    /* Write final two 512-byte zero blocks */
    memset( record, 0, RECORD_SIZE );
    fwrite( record, 1, RECORD_SIZE, tar );
    fwrite( record, 1, RECORD_SIZE, tar );

    fclose( tar );
}


/* ------------------------------------------------------ */
/* --------------------- LIST MODE ---------------------- */
/* ------------------------------------------------------ */
void mode_list( const char *tarfile ) {
    FILE *fp;
    long filesize;
    int i;

    fp = fopen( tarfile, "rb" );
    if ( !fp ) {
        perror( "open" );
        exit( 1 );
    }

    while ( fread( record, 1, RECORD_SIZE, fp ) == RECORD_SIZE ) {
        chk_ctrl_c();

        if ( is_block_empty( record ) )
            break;

        if ( !is_valid_tar_header( record ) ) {
            fprintf( stderr, "Invalid TAR format: missing ustar magic.\n" );
            break;
        }

        for ( i = 0; i < NAME_SIZE - 1 && record[ i ]; ++i )
            filename[ i ] = isprint( record[ i ] ) ? record[ i ] : '?';
        filename[ i ] = '\0';

        filesize = octal_to_long( (char *)( record + 124 ), 12 );
        printf( "%s (%ld bytes)\n", filename, filesize );

        fseek( fp, ( ( filesize + 511 ) / 512 ) * 512, SEEK_CUR );
    }

    fclose( fp );
}


/* ------------------------------------------------------ */
/* -------------------- EXTRACT MODE -------------------- */
/* ------------------------------------------------------ */
void mode_extract( const char *tarfile ) {
    FILE *tar, *out;
    long filesize, remaining;
    int i, to_read;

    tar = fopen( tarfile, "rb" );
    if ( !tar ) {
        perror( "open" );
        exit( 1 );
    }

    while ( fread( record, 1, RECORD_SIZE, tar ) == RECORD_SIZE ) {
        chk_ctrl_c();

        if ( is_block_empty( record ) )
            break;

        if ( !is_valid_tar_header( record ) ) {
            fprintf( stderr, "Invalid TAR format: missing ustar magic.\n" );
            break;
        }

        for ( i = 0; i < NAME_SIZE - 1 && record[ i ]; ++i )
            filename[ i ] = isprint( record[ i ] ) ? record[ i ] : '?';
        filename[ i ] = '\0';

        filesize = octal_to_long( (char *)( record + 124 ), 12 );

        out = fopen( filename, "wb" );
        if ( out )
            printf( "%s (%ld)\n", filename, filesize );
        else {
            perror( filename );
            fseek( tar, ( ( filesize + 511 ) / 512 ) * 512, SEEK_CUR );
            continue;
        }

        remaining = filesize;
        while ( remaining > 0 ) {
            chk_ctrl_c();
            to_read = remaining > RECORD_SIZE ? RECORD_SIZE : (int)remaining;
            fread( record, 1, RECORD_SIZE, tar );
            fwrite( record, 1, to_read, out );
            remaining -= to_read;
        }
        fclose( out );
    }
    fclose( tar );
}


void usage( char *argv0 ) {
    printf( "Tiny TAR archiving tool version %s\n", VERSION );
    printf( "Usage:\n" );
    printf( "  %s -cf archive.tar file1 [file2 ...]  # Create archive from files.\n", argv0 );
    printf( "  %s -rf archive.tar file1 [file2 ...]  # Append files to archive.\n", argv0 );
    printf( "  %s [-tf] archive.tar                  # List all files in archive.\n", argv0 );
    printf( "  %s -xf archive.tar                    # Extract all files from archive.\n", argv0 );
}


/* command line arguments, CP/M converts everything into upper case */
#ifdef CPM
#define TF "-TF"
#define XF "-XF"
#define CF "-CF"
#define RF "-RF"
#else
#define TF "-tf"
#define XF "-xf"
#define CF "-cf"
#define RF "-rf"
#endif


/* ---------------------------------------------- */
/* -------------------- MAIN -------------------- */
/* ---------------------------------------------- */
int main( int argc, char *argv[] ) {
    if ( argc < 2 ) {
#ifdef CPM
        *argv = "tar"; /* argv[0] CP/M is the empty string ("") */
#endif
        usage( *argv );
        return 1;
    }

    now = time( NULL );
    if ( now <= T_19800101 ) /* no valid time */
        now = T_19800101;

    if ( argc == 2 || strcmp( argv[ 1 ], TF ) == 0 ) {
        mode_list( argv[ argc == 2 ? 1 : 2 ] );
    } else if ( argc == 3 && strcmp( argv[ 1 ], XF ) == 0 ) {
        mode_extract( argv[ 2 ] );
    } else if ( argc >= 4 && strcmp( argv[ 1 ], CF ) == 0 ) {
        mode_create_append( 0, argc, argv );
    } else if ( argc >= 4 && strcmp( argv[ 1 ], RF ) == 0 ) {
        mode_create_append( 1, argc, argv );
    } else {
        usage( *argv );
        return 1;
    }

    return 0;
}
