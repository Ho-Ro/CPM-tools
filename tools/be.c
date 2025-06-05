/*
 *  Binary Editor -- a small binary editor for programmers
 *
 *  Copyright (C) 2017-2022 Lars Lindehaven
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/**************************************************************************
*
* Small change to allow-it to be run on CP/M 2.2 (instead of CP/M 3)
*
* Adapted it to be compiled by HiTech C v3.09 compiler
*
* Changed the screen size to 48 lines x 80 columns (instead of 25 x 80)
*
* Changed to process binary files with size up to 9600H (instead of 8000H)
*
*             Szilagyi Ladislau, 2024
*
**************************************************************************/

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <conio.h>
#include <string.h>
#include <stdint.h>

/* DEFINITIONS ------------------------------------------------------------ */

#define PROG_NAME   "Binary Editor"
#define PROG_AUTH   "Lars Lindehaven"
#define PROG_VERS   "v0.1.6a 2025-06-15"
#define PROG_SYST   "CP/M"

#define WORDBITS    16                              /* # of bits in a word  */
#define MAX_FNAME   16                              /* Max filename length  */
#define MAX_WHERE   0x940                           /* Max change info size */
#define MAX_BYTES   (MAX_WHERE * WORDBITS)          /* Max byte buffer size */

#define ED_ROWS     32                              /* # of rows            */
#define ED_COLS     16                              /* # of columns         */
#define ED_PAGE     (ED_ROWS * ED_COLS)             /* Page size            */

#define ED_TITLE    1                               /* Title row            */
#define ED_INFO     (ED_TITLE + 1)                  /* Information row      */
#define ED_MSG      (ED_INFO + 1)                   /* Message row          */
#define ED_HEAD     (ED_MSG + 2)                    /* Heading row          */
#define ED_ROWT     (ED_HEAD + 1)                   /* Top of page          */
#define ED_ROWB     (ED_ROWT + ED_ROWS - 1)         /* Bottom of page       */
#define ED_TAIL     (ED_ROWB + 1)                   /* Trailing row         */

#define ED_CLM      2                               /* Column Left Margin   */
#define ED_CHW      3                               /* Column Hex Width     */
#define ED_CHL      (ED_CLM + 8)                    /* Column Hex Left      */
#define ED_CHR      (ED_CHL + ED_COLS * ED_CHW)     /* Column Hex Right     */
#define ED_CAW      1                               /* Column ASCII Width   */
#define ED_CAL      (ED_CHR + 4)                    /* Column ASCII Left    */
#define ED_CAR      (ED_CAL + ED_COLS * ED_CAW)     /* Column ASCII Right   */

/* ANSI Screen */
#define TERM_ROWS   48    /* # of rows on terminal screen.                */
#define TERM_COLS   80    /* # of columns on terminal screen.             */

/* GLOBALS ---------------------------------------------------------------- */

/* CP/M Keyboard */
char *key[] = {
    "L.Lindehaven",
    "\x0a",                 /* ^J  Help                                     */
    "\x05",                 /* ^E  Cursor one row up                        */
    "\x18",                 /* ^X  Cursor one row down                      */
    "\x13",                 /* ^S  Cursor one column left                   */
    "\x04",                 /* ^D  Cursor one column right                  */
    "\x12",                 /* ^R  Cursor one page up                       */
    "\x03",                 /* ^C  Cursor one page down                     */
    "\x14",                 /* ^T  Cursor to beginning of byte buffer (top) */
    "\x16",                 /* ^V  Cursor to end of byte buffer (bottom)    */
    "\x01",                 /* ^A  Set edit mode HEX                        */
    "\x06",                 /* ^F  Set edit mode ASCII                      */
    "\x1a",                 /* ^Z  Toggle edit mode (ASCII/HEX)             */
    "\x17",                 /* ^W  Write byte buffer to file (save)         */
    "\x1b",                 /* ESC Quit                                     */
    "\x00",                 /* Reserved for future use                      */
    "\x00",                 /* Reserved for future use                      */
};

char *help[] = {
   "^J      Help (this)",
   "^E      Row up     ",
   "^X      Row down   ",
   "^S      Col left   ",
   "^D      Col right  ",
   "^R      Page up    ",
   "^C      Page down  ",
   "^T      Top        ",
   "^V      Bottom     ",
   "^A      HEX mode   ",
   "^F      ASCII mode ",
   "^Z      Toggle mode",
   "^W      Write file ",
   "ESC ESC Quit       ",
   "                   ",
   "                   "
};

char fname[MAX_FNAME];      /* Filename                                     */
unsigned int eatop = 0;              /* Address on top row in editor                 */
unsigned int aoffs = 0x0000;         /* Offset when displaying address               */
uint8_t erow = 0;                    /* Row in editor                                */
uint8_t ecol = 0;                    /* Column in editor                             */
uint8_t eascii = 0;                  /* Edit mode: 0 (HEX) or 1 (ASCII)              */
unsigned int bcurr = 0;              /* Current position in byte buffer              */
unsigned int bsize = 0;              /* Size of byte buffer                          */
unsigned int bchanges = 0;           /* # of changes made in byte buffer             */
unsigned int bwhere[MAX_WHERE];      /* Where changes have been made                 */
uint8_t bbuff[MAX_BYTES];            /* Byte buffer (maximum 32767 bytes)            */


/* PROGRAM ---------------------------------------------------------------- */
uint8_t edLoop(void);
void    edPosCur(void);
void    edInput(char ch);
int     edHex2Nibble(uint8_t ch);
void    edSetChange(uint16_t bindex);
uint8_t edIsChanged(uint16_t bindex);
void    edResetChanges(void);
void    edUpd(uint8_t fromrow, uint8_t nrows);
void    edUpdAll(void);
void    edHelp(void);
void    rowUp(void);
void    rowDown(void);
void    colLeft(void);
void    colRight(void);
void    pageUp(void);
void    pageDown(void);
void    buffTop(void);
void    buffBottom(void);
int8_t  fileRead(void);
int8_t  fileWrite(void);
void    fileQuit(void);
void    sysTitle(void);
void    sysInfo(void);
void    sysHead(void);
void    sysMsg(char* s);
uint8_t sysMsgKey(char* s);
void    scrClr(void);
void    scrPosCur(uint8_t row, uint8_t col);
void    scrClrEol(void);
void    scrClrRow(uint8_t row);
void    scrInvVideo(void);
void    scrNorVideo(void);
void    scrHideCursor(void);
void    scrShowCursor(void);
uint8_t keyPressed(void);

int main(int argc, char* argv[]) {
    int rc = 0;

    if (argc > 1) {
        if (strlen(argv[1]) > MAX_FNAME - 1) {
            printf("Filename is too long.");
            return -1;
        } else {
            strcpy(fname, argv[1]);
            if (fileRead())
                return -1;
        }
        if (argc == 3) {
            aoffs = atoi(argv[2]);
        }
    } else {
        printf("Usage: be filename.ext [address offset]");
        return -1;
    }
    scrClr();
    rc = edLoop();
    scrClr();
    return rc;
}

/* EDITING ---------------------------------------------------------------- */

/* Main editor loop */
uint8_t edLoop(void) {
    unsigned int ch;

    sysTitle();
    sysHead();
    edResetChanges();
    edUpdAll();
    for (;;) {
        sysInfo();
        edPosCur();
        ch = keyPressed();
        if (ch == *key[1])
            edHelp();
        else if (ch == *key[2])
            rowUp();
        else if (ch == *key[3])
            rowDown();
        else if (ch == *key[4] || ch == 127)
            colLeft();
        else if (ch == *key[5] || ch == 9)
            colRight();
        else if (ch == *key[6])
            pageUp();
        else if (ch == *key[7])
            pageDown();
        else if (ch == *key[8])
            buffTop();
        else if (ch == *key[9])
            buffBottom();
        else if (ch == *key[10])
            eascii = 0;
        else if (ch == *key[11])
            eascii = 1;
        else if (ch == *key[12])
            eascii ^= 1;
        else if (ch == *key[13])
            fileWrite();
        else if (ch == *key[14]) {
            if ( keyPressed() == *key[14] ) {
                fileQuit();
                return 0;
            }
        /* *key[15] and *key[16] are free to use */
        }
        else
            edInput(ch);
    }
}

/* Position cursor depending on edit mode (HEX or ASCII) */
void edPosCur(void) {
    if (eascii)
        scrPosCur(ED_ROWT + erow, ED_CAL + ED_CAW * ecol);
    else
        scrPosCur(ED_ROWT + erow, ED_CHL + ED_CHW * ecol);
}

/* Edit current line */
void edInput(char ch) {
    int hi, lo, new;

    if (eascii) {
        if (ch != bbuff[bcurr] && ch > 0x1f && ch < 0x7f) {
            bbuff[bcurr] = ch;
            edSetChange(bcurr);
            edUpd(erow, 1);
            colRight();
        }
    } else {
        if ((hi = edHex2Nibble(ch)) > -1) {
            putchar(tolower(ch));
            ch = keyPressed();
            if ((lo = edHex2Nibble(ch)) > -1) {
                putchar(tolower(ch));
                new = 16 * hi + lo;
                if (new != bbuff[bcurr]) {
                    bbuff[bcurr] = new;
                    edSetChange(bcurr);
                }
            }
            edUpd(erow, 1);
            colRight();
        }
    }
}

/* Convert hexadecimal to nibble (0-15) */
int edHex2Nibble(uint8_t ch) {
    if (ch >= '0' && ch <= '9')
        return (ch - '0');
    else if (ch >= 'A' && ch <= 'F')
        return (ch - 'A' + 10);
    else if (ch >= 'a' && ch <= 'f')
        return (ch - 'a' + 10);
    else
        return -1;
}

/* Store number of changes and where they have been made  */
void edSetChange(uint16_t bindex) {
    if (!edIsChanged(bindex)) {
        bwhere[bindex / WORDBITS] |= 1 << bindex % WORDBITS;
        bchanges += 1;
    }
}

/* Check if byte is changed */
uint8_t edIsChanged(uint16_t bindex) {
    if ( bchanges ) { /* any changes? */
        uint16_t bline = bwhere[ bindex >> 4 ];
        if ( bline ) /* change in this line? */
            return ( bline >> ( bindex & 0x0f ) ) & 1;
    }
    return 0; /* nope */
}

/* Reset number of changes and where they have been made  */
void edResetChanges(void) {
    int i;

    for (i = 0; i < MAX_WHERE; i++)
        bwhere[i] = 0;
    bchanges = 0;
}

/* Update all columns on row(s) on editor screen */
void edUpd(uint8_t fromrow, uint8_t nrows) {
    unsigned int r, c, i;
    unsigned char *bp;

    for (r = fromrow; r < fromrow + nrows && r < ED_ROWS; r++) {
        scrClrRow(ED_ROWT + r);
        scrPosCur(ED_ROWT + r, ED_CLM); /* go to beginning of line */
        printf("%04x    ", aoffs + eatop + r * ED_COLS);
        /* update the hex line */
        i = eatop + r * ED_COLS;
        bp = bbuff + i;
        for (c = 0; c < ED_COLS; ++c, ++i, ++bp) {
            if (edIsChanged(i)) {
                scrInvVideo();
                printf("%02x ", *bp);
                scrNorVideo();
            } else {
                printf("%02x ", *bp);
            }
        }
        printf("    "); /* skip over to ascii */
        /* now update ascii line */
        i = eatop + r * ED_COLS;
        bp = bbuff + i;
        for (c = 0; c < ED_COLS; ++c, ++i, ++bp) {
            if (edIsChanged(i))
                scrInvVideo();
            if (*bp > 0x1f && *bp < 0x7f)
                putchar(*bp);
            else
                putchar('.');
            if (edIsChanged(i))
            scrNorVideo();
        }
    }
}

/* Update editor screen from first row to last row */
void edUpdAll(void) {
    scrHideCursor();
    edUpd(0, ED_ROWS);
    scrShowCursor();
}

/* Display command help */
void edHelp(void) {
    unsigned int r, helprows;

    helprows = sizeof(help) / sizeof(help[0]);
    for (r = 0; r < ED_ROWS; r++)
        scrClrRow(ED_ROWT + r);
    for (r = 0; r < ED_ROWS && r < helprows; r++) {
        scrPosCur(ED_ROWT + r, TERM_COLS/2 - strlen(help[0]));
        printf("%s", help[r]);
    }
    sysMsgKey("Press any key to continue editing: ");
    edUpdAll();
}

/* CURSOR MOVEMENT -------------------------------------------------------- */

/* Move cursor one row up */
void rowUp(void) {
    if (bcurr >= ED_COLS) {
        bcurr -= ED_COLS;
        if (eatop >= ED_COLS && erow == 0) {
            eatop -= ED_COLS;
            edUpdAll();
        } else if (erow > 0) {
            erow--;
        }
        edPosCur();
    }
}

/* Move cursor one row down */
void rowDown(void) {
    if (bcurr < bsize-ED_COLS) {
        bcurr += ED_COLS;
        if (eatop <= bsize-ED_COLS && erow == ED_ROWS-1) {
            eatop += ED_COLS;
            edUpdAll();
        } else if (erow < ED_ROWS-1) {
            erow++;
        }
        edPosCur();
    }
}

/* Move cursor one column left */
void colLeft(void) {
    if (bcurr > 0) {
        bcurr--;
        if (ecol > 0) {
            ecol--;
        } else {
            ecol = ED_COLS-1;
            if (eatop >= ED_COLS && erow == 0) {
                eatop -= ED_COLS;
                edUpdAll();
            } else if (erow > 0) {
                erow--;
            }
        }
        edPosCur();
    }
}

/* Move cursor one column right */
void colRight(void) {
    if (bcurr < bsize-1) {
        bcurr++;
        if (ecol < ED_COLS-1) {
            ecol++;
        } else {
            ecol = 0;
            if (eatop <= bsize-ED_COLS && erow == ED_ROWS-1) {
                eatop += ED_COLS;
                edUpdAll();
            } else if (erow < ED_ROWS-1) {
                erow++;
            }
        }
        edPosCur();
    }
}

/* Move cursor one page up */
void pageUp(void) {
    if (bcurr >= ED_PAGE) {
        bcurr -= ED_PAGE;
        if (bcurr < ED_PAGE) {
            bcurr = ecol;
            eatop = 0;
            erow = 0;
        } else {
            eatop -= ED_PAGE;
        }
        edUpdAll();
        edPosCur();
    }
}

/* Move cursor one page down */
void pageDown(void) {
    if (bcurr < bsize - ED_PAGE) {
        bcurr += ED_PAGE;
        eatop += ED_PAGE;
        edUpdAll();
        edPosCur();
    }
}

/* Move cursor to beginning of buffer */
void buffTop(void) {
    if (bcurr != 0) {
        bcurr = 0;
        eatop = 0;
        erow = 0;
        ecol = 0;
        edUpdAll();
        edPosCur();
    }
}

/* Move cursor to end of buffer */
void buffBottom(void) {
    if (bcurr != bsize - 1) {
        bcurr = bsize - 1;
        eatop = bsize - ED_COLS;
        erow = 0;
        ecol = bcurr % ED_COLS;
        edUpdAll();
        edPosCur();
    }
}

/* FILE I/O --------------------------------------------------------------- */

/* Read file to byte buffer */
int8_t fileRead(void) {
    FILE *fp;
    unsigned int i;

    if (!(fp = fopen(fname, "rb"))) {
        printf("Cannot open %s", fname);
        return -1;
    }
    for (i = 0; i < MAX_BYTES; ++i) {
        bbuff[i] = fgetc(fp);
        if (feof(fp))
            break;
    }
    fclose(fp);
    if (i >= MAX_BYTES) {
        printf("Not enough memory to read %s", fname);
        return -1;
    }
    bsize = i;
    return 0;
}

/* Write byte buffer to file */
int8_t fileWrite(void) {
    FILE *fp;
    unsigned int bytes;

    if (!(fp = fopen(fname, "wb"))) {
        sysMsgKey("Could not open file for writing! Press any key: ");
        return -1;
    }
    for (bytes = 0; bytes < bsize; bytes++) {
        fputc(bbuff[bytes], fp);
    }
    if (fclose(fp) == EOF) {
        sysMsgKey("Could not close file after writing! Press any key: ");
        return -1;
    }
    if (bytes < bsize) {
        sysMsgKey("Could not write to file! Press any key: ");
        return -1;
    }
    edResetChanges();
    edUpdAll();
    return 0;
}

/* Let user choose to save or disregard any changes made */
void fileQuit(void) {
    char ch = ' ';

    if (bchanges) {
        while (ch != 'S' && ch != 'Q') {
            ch = sysMsgKey("There are unsaved changes. S(ave) or Q(uit)? ");
            ch = toupper(ch);
            if (ch == 'S')
                fileWrite();
        }
    }
}

/* SYSTEM INFORMATION ----------------------------------------------------- */

/* Print system title. */
void sysTitle(void) {
    scrPosCur(ED_TITLE, ED_CLM);
    printf("%s %s for %s by %s", PROG_NAME, PROG_VERS, PROG_SYST, PROG_AUTH);
}

/* Display the file information */
void sysInfo(void) {
    scrPosCur(ED_INFO, ED_CLM);
    printf("%s ", fname);
    if (bchanges) {
        scrInvVideo();
        putchar('*');
    } else {
        scrNorVideo();
        putchar(' ');
    }
    printf(" %5d ", bchanges);
    scrNorVideo();
    printf(" %04x/%04x ", bcurr, bsize-1);
    if (eascii) printf("ASCII"); else printf("HEX  ");
    printf("  Press ^J for help");
}

/* Print header on system line */
void sysHead(void) {
    unsigned int i;

    scrPosCur(ED_HEAD, 0);
    for (i = 0; i < TERM_COLS; i++)
        putchar('=');
    scrPosCur(ED_HEAD, ED_CLM-1);
    printf(" ADDR ");
    scrPosCur(ED_HEAD, ED_CHL-1);
    printf(" HEX ");
    scrPosCur(ED_HEAD, ED_CAL-1);
    printf(" ASCII ");
    scrPosCur(ED_TAIL, 0);
    for (i = 0; i < TERM_COLS; i++)
        putchar('=');
}

/* Print message on system line */
void sysMsg(s) char *s; {
    scrClrRow(ED_MSG);
    scrPosCur(ED_MSG, ED_CLM);
    printf("%s", s);
}

/* Print message on system line and wait for a key press */
uint8_t sysMsgKey(char *s) {
    unsigned int ch;

    scrInvVideo();
    sysMsg(s);
    ch = keyPressed();
    scrNorVideo();
    scrClrRow(ED_MSG);
    return ch;
}

/* ANSI SCREEN ------------------------------------------------------------ */

/* Clear screen and send cursor to upper left corner. */
void scrClr(void) {
    printf("\x1b[2J");
    printf("\x1b[H");
}

/* Move cursor to row, col */
void scrPosCur(uint8_t row, uint8_t col) {
    printf("\x1b[%d;%dH", row+1, col+1);
}

/* Erase from the cursor to the end of the line */
void scrClrEol(void) {
    printf("\x1b[K");
}

/* Move cursor to row and clear line */
void scrClrRow(uint8_t row) {
    scrPosCur(row, 0);
    scrClrEol();
}

/* Set inverse video */
void scrInvVideo(void) {
    printf("\x1b[7m");
}

/* Set normal video */
void scrNorVideo(void) {
    printf("\x1b[27m");
}

/* Hide cursor */
void scrHideCursor(void) {
    printf("\x1b[?25l");
}

/* Show cursor */
void scrShowCursor(void) {
    printf("\x1b[?25h");
}

/* CP/M KEYBOARD ---------------------------------------------------------- */
uint8_t keyPressed(void)
{
    return getch();
}
