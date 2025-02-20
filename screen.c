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
#include <string.h>
#include <wchar.h>
#include <signal.h>

#include <unistd.h>
#include <sys/select.h>
#if (defined USE_CURSES)
#	include <curses.h>
#else
#	include <ncurses.h>
#endif

#include "tnylpo.h"


/*
 * size of the terminal input queue
 */
#define IN_SIZE 128


/*
 * state of the escape sequence parser
 */
static enum term_state {
	ST_NORMAL, /* not in escape sequence */
	ST_ESCAPE, /* escape seen */
	ST_ESCAPEY, /* escape-Y seen */
	ST_ESCAPEYL, /* escape-Y-<line> seen */
	ST_ESCAPES, /* escape-S seen */
	ST_ESCAPET /* escape-T seen */
} state = ST_NORMAL;
static int escape_y_line = 0, escape_y_col = 0;


/*
 * terminal input queue
 */
static unsigned char in_buffer[IN_SIZE];
static int in_count = 0, in_in = 0, in_out = 0;


/*
 * current logical cursor position
 */
static int cursor_x = 0, cursor_y = 0;


/*
 * actual size of the visible screen/window (can change at random)
 */
static int screen_lines = 0, screen_cols = 0;


/*
 * current output attributes
 */
static int is_graphics = 0, is_reverse = 0, is_bold = 0, is_standout = 0,
	is_blink = 0, is_underline = 0;


/*
 * current state of the "hold screen" mechanism
 */
static int hold_screen = 0, hold_allow = 0;


/*
 * state of the cursor visibility
 */
static int cursor_off = 0, old_cursor = 0;


/*
 * state of the application keypad (not really implemented)
 */
static int app_keypad = 0;


/*
 * color output valiables
 */
static int use_color = 0;
static int foreground = 0, background = 0;
static short pairs[8][8] = {
    { (-1), (-1), (-1), (-1), (-1), (-1), (-1), (-1) },
    { (-1), (-1), (-1), (-1), (-1), (-1), (-1), (-1) },
    { (-1), (-1), (-1), (-1), (-1), (-1), (-1), (-1) },
    { (-1), (-1), (-1), (-1), (-1), (-1), (-1), (-1) },
    { (-1), (-1), (-1), (-1), (-1), (-1), (-1), (-1) },
    { (-1), (-1), (-1), (-1), (-1), (-1), (-1), (-1) },
    { (-1), (-1), (-1), (-1), (-1), (-1), (-1), (-1) },
    { (-1), (-1), (-1), (-1), (-1), (-1), (-1), (-1) }
};


/*
 * is input currently non-blocking?
 */
static int noblock = 0;


/*
 * Curses descriptors of the actual screen/window and of the representation
 * of the VT52 screen contents
 */
static WINDOW *win_p = NULL, *pad_p = NULL;


/*
 * Curses-compatible representation of the blank character
 */
static cchar_t blank;


/*
 * conversion table between tnylpo's color numbers and curses
 */
static short curses_colors[8] = {
    COLOR_BLACK, COLOR_BLUE, COLOR_RED, COLOR_MAGENTA,
    COLOR_GREEN, COLOR_CYAN, COLOR_YELLOW, COLOR_WHITE
};


/*
 * cleanup/restore output on leaving the terminal emulation
 */
static void
reset_curses(void) {
	if (pad_p) {
		/*
		 * cleanup of the VT52 screen contents
		 */
		/*
		 * restore cursor visibility
		 */
		if (cursor_off && old_cursor != ERR) curs_set(old_cursor);
		/*
		 * restore blocking input
		 */
		if (noblock) nodelay(pad_p, 0);
		/*
		 * turn off hardware supported insert/delete line
		 */
		idlok(pad_p, 0);
		/*
		 * restore keypad
		 */
		keypad(pad_p, 0);
		/*
		 * remove the VT52 screen contents
		 */
		delwin(pad_p);
	}
	if (win_p) {
		/*
		 * cleanup curses
		 */
		/*
		 * restore terminal default parameters, turn on echo
		 */
		noraw();
		nl();
		echo();
		/*
		 * position cursor to the start of the last line
		 * for curses implementations not cleaning/restoring
		 * the screen on exit (e. g. NetBSD)
		 */
		wmove(win_p, screen_lines - 1, 0);
		wrefresh(win_p);
		/*
		 * remove the screen handle
		 */
		delwin(win_p);
		/*
		 * leave curses
		 */
		endwin();
		refresh();
	}
}


/*
 * store a character in the terminal input queue (if there is room)
 */
static void
in_put(unsigned char c) {
	if (in_count < IN_SIZE) {
		in_buffer[in_in++] = c;
		if (in_in == IN_SIZE) in_in = 0;
		in_count++;
	}
}


/*
 * get a character (or (-1), if none is present) from the terminal
 * input queue
 */
static int
in_get(void) {
	int rc = (-1);
	if (in_count) {
		rc = in_buffer[in_out++];
		if (in_out == IN_SIZE) in_out = 0;
		in_count--;
	}
	return rc;
}


/*
 * refresh the screen to show the contents of the VT52 screen
 */
static void
show_pad(void) {
	wmove(pad_p, cursor_y, cursor_x);
	prefresh(pad_p, 0, 0, 0, 0,
	    (screen_lines > lines ? lines : screen_lines) - 1,
	    (screen_cols > cols ? cols : screen_cols) - 1);
}


/*
 * read a keycode from the screen, translate it to VT52 code(s) and
 * put it/them in the terminal input queue
 */
static void
try_read(void) {
	wint_t wc;
	int t;
	/*
	 * get a keycode from the screen
	 */
	switch (wget_wch(pad_p, &wc)) {
	case ERR:
		/*
		 * ignore errors (these are generated by e. g. signals
		 * like SIGWINCH interrupting a read from the screen)
		 */
		break;
	case KEY_CODE_YES:
		/*
		 * it is a function key
		 */
		switch (wc) {
		case KEY_RESIZE:
			/*
			 * resize event: get new screen/window size and
			 * redraw the screen contents
			 */
			getmaxyx(win_p, screen_lines, screen_cols);
			show_pad();
			break;
		case KEY_BACKSPACE:
			/*
			 * backspace key
			 */
			if (reverse_bs_del) {
				in_put(0x7f /* DEL */);
			} else {
				in_put(0x08 /* BS */);
			}
			break;
		case KEY_UP:
			/*
			 * cursor up key
			 */
			if (altkeys) {
				in_put(0x05 /* ^E */);
			} else {
				in_put(0x1b /* ESC */);
				in_put(0x41 /* A */);
			}
			break;
		case KEY_DOWN:
			/*
			 * cursor down key
			 */
			if (altkeys) {
				in_put(0x18 /* ^X */);
			} else {
				in_put(0x1b /* ESC */);
				in_put(0x42 /* B */);
			}
			break;
		case KEY_RIGHT:
			/*
			 * cursor right key
			 */
			if (altkeys) {
				in_put(0x04 /* ^D */);
			} else {
				in_put(0x1b /* ESC */);
				in_put(0x43 /* C */);
			}
			break;
		case KEY_LEFT:
			/*
			 * cursor left key
			 */
			if (altkeys) {
				in_put(0x13 /* ^S */);
			} else {
				in_put(0x1b /* ESC */);
				in_put(0x44 /* D */);
			}
			break;
		case KEY_F(1):
			/*
			 * F1 emulates the first blank key of the VT52
			 */
			in_put(0x1b /* ESC */);
			in_put(0x50 /* P */);
			break;
		case KEY_F(2):
			/*
			 * F2 emulates the second blank key of the VT52
			 */
			in_put(0x1b /* ESC */);
			in_put(0x51 /* Q */);
			break;
		case KEY_F(3):
			/*
			 * F3 emulates the third blank key of the VT52
			 */
			in_put(0x1b /* ESC */);
			in_put(0x52 /* R */);
			break;
		case KEY_F(4):
			/*
			 * F4 forces a screen refresh
			 */
			wrefresh(curscr);
			break;
		case KEY_F(5):
			/*
			 * F5 switches "hold screen" mode on or off
			 */
			hold_screen = ! hold_screen;
			if (hold_screen) hold_allow = 0;
			break;
		case KEY_F(6):
			/*
			 * F6 allows one further screenfull in
			 * "hold screen" mode
			 */
			if (hold_screen && ! hold_allow) hold_allow += lines;
			break;
		case KEY_F(7):
			/*
			 * F7 allows one further line in "hold screen" mode
			 */
			if (hold_screen && ! hold_allow) hold_allow++;
			break;
		case KEY_F(10):
			/*
			 * F10 is the reset switch: the emulation will
			 * be stopped by raising SIGINT
			 */
			plog("F10 key pressed --- raising SIGINT");
			raise(SIGINT);
			break;
		default:
			/*
			 * all other function keys are ignored
			 */
			break;
		}
		break;
	default:
		/*
		 * if the character from the screen can be represented in
		 * the CP/M character set, save this representation in the
		 * terminal input queue
		 */
		t = to_cpm(wc);
		if (t != (-1)) {
			if (reverse_bs_del) {
				if (t == 0x80 /* BS */) {
					t = 0x7f /* DEL */;
				} else if (t == 0x7f /* DEL */) {
					t = 0x08 /* BS */;
				}
			}
			in_put((unsigned char) t);
		}
		break;
	}
}


/*
 * Get the color pair for a given color combination; if it does not
 * exist, allocate it; if it cannot be allocated, return the standard
 * pair.
 */
static inline short
get_pair(int fg, int bg) {
	static int out_of_pairs = 0;
	/*
	 * color pair 0 is reserved
	 */
	static short free_pair = 1;
	short p;
	p = pairs[fg][bg];
	if (p == (-1)) {
		if (free_pair >= COLOR_PAIRS || init_pair(free_pair,
                    curses_colors[fg], curses_colors[bg]) == ERR) {
		    	/*
			 * complain if we run out of color pairs, but only once
			 */
		    	if (! out_of_pairs) {
				plog("out of color pairs");
				out_of_pairs = 1;
			}
			p = 0;
		} else {
			p = free_pair++;
		}
		pairs[fg][bg] = p;
	}
	return p;
}


/*
 * initializes the VT52 terminal emulation; must be called before all
 * other crt_xxx() functions
 */
int
crt_init(void) {
	int rc = 0;
	wchar_t wcs[2];
	wint_t wc;
	short int pair;
	/*
	 * redirections are not allowed
	 */
	if (! isatty(fileno(stdin)) || ! isatty(fileno(stdout))) {
		rc = 1;
		goto premature_exit;
	}
	/*
	 * initialize curses; in current implementations, any error
	 * during initialization will abort the program, so the
	 * error checking is futile.
	 */
	win_p = initscr();
	if (! win_p) {
		rc = 2;
		goto premature_exit;
	}
	/*
	 * set up colors if requested and available
	 */
	if (conf_color && has_colors()) {
		use_color = 1;
		foreground = conf_foreground;
		background = conf_background;
		if (start_color() == ERR) {
			rc = 4;
			goto premature_exit;
		}
	}
	/*
	 * get physical screen dimensions; if requested, these will be
	 * used to dimension the VT52 emulation screen, but only as
	 * far as they are within the legal maximal and minimal values.
	 */
	getmaxyx(win_p, screen_lines, screen_cols);
	if (cols == (-1)) {
		cols = screen_cols;
		if (cols < MIN_COLS) cols = MIN_COLS;
		if (cols > MAX_COLS) cols = MAX_COLS;
	}
	if (lines == (-1)) {
		lines = screen_lines;
		if (lines < MIN_LINES) lines = MIN_LINES;
		if (lines > MAX_LINES) lines = MAX_LINES;
	}
	/*
	 * turn off input echoing and CR/NL translation; switch of
	 * input postprocessing
	 */
	noecho();
	nonl();
	raw();
	/*
	 * create the representation of the VT52 screen contents
	 */
	pad_p = newpad(lines, cols);
	if (! pad_p) {
		rc = 3;
		goto premature_exit;
	}
	/*
	 * initialize the blank character
	 */
	wc = from_cpm(0x20 /* SPC */);
	if (wc != (-1)) wc = L' ';
	wcs[0] = wc;
	wcs[1] = L'\0';
	pair = use_color ? get_pair(foreground, background) : 0;
	setcchar(&blank, wcs, 0, pair, NULL);
	bkgrnd(&blank);
	wbkgrnd(pad_p, &blank);
	/*
	 * switch to application keypad mode, allow hardware assisted
	 * line insertion/deletion; show the current (empty) VT52 screen
	 */
	keypad(pad_p, 1);
	idlok(pad_p, 1);
	erase();
	refresh();
	werase(pad_p);
	show_pad();
premature_exit:
	/*
	 * in case of errors, reset the screen; error messages from
	 * screen initialization are deferred until after resetting the
	 * screen
	 */
	if (rc) reset_curses();
	switch (rc) {
	case 1:
		perr("stdin or stdout must be a terminal");
		break;
	case 2:
		perr("initscr() failed, TERM undefined?");
		break;
	case 3:
		perr("newpad() failed");
		break;
	case 4:
		perr("cannot initialize colors");
		break;
	}
	return rc ? (-1) : 0;
}


/*
 * helper function for crt_out(): set foreground or background color
 */
static inline int
set_color(unsigned char c, int curr_color, int def_color) {
	switch (c) {
	case 0x30 /* 0 */: return 0;
	case 0x31 /* 1 */: return 1;
	case 0x32 /* 2 */: return 2;
	case 0x33 /* 3 */: return 3;
	case 0x34 /* 4 */: return 4;
	case 0x35 /* 5 */: return 5;
	case 0x36 /* 6 */: return 6;
	case 0x37 /* 7 */: return 7;
	case 0x3d /* = */: return def_color;
	}
	return curr_color;
}


/*
 * display a character on the emulated VT52 screen, handle escape sequences
 */
void
crt_out(unsigned char c) {
	int t;
	wint_t wc;
	short pair;
	cchar_t cc;
	wchar_t wcs[2];
	/*
	 * handle ASCII control characters
	 */
	if (c <= 0x1f /* US */) {
		switch (c) {
		case 0x07 /* BEL */:
			beep();
			break;
		case 0x08 /* BS */:
			if (cursor_x > 0) {
				cursor_x--;
				goto move_cursor;
			}
			break;
		case 0x09 /* TAB */:
			/*
			 * This is the VT52 way of TAB expansion: every
			 * eighth columns below column 72, then a single
			 * column up to column 80, where TABs are ignored.
			 */
			t = ((cursor_x / 8) + 1) * 8;
			if (t >= cols) t = cursor_x + 1;
			if (t < cols) {
				cursor_x = t;
				goto move_cursor;
			}
			break;
		case 0x0a /* LF */:
			if (cursor_y + 1 < lines) {
				/*
				 * before last line: LF always means
				 * "cursor down"
				 */
				cursor_y++;
				goto move_cursor;
			}
			/*
			 * In "hold screen" mode, scrolling is only allowed
			 * hold_allow times; if hold_allow is decreased to
			 * zero, output is blocked until the "hold screen"
			 * mode is ended by pressing the "hold screen"
			 * key or by allowing a further line resp. a further
			 * screenful by pressing the respectve keys.
			 */
			if (hold_screen) {
				while (hold_screen && ! hold_allow) {
					if (noblock) {
						nodelay(pad_p, 0);
						noblock = 0;
					}
					try_read();
				}
				if (hold_screen) hold_allow--;
			}
			/*
			 * scroll down one line
			 */
			scrollok(pad_p, 1);
			wscrl(pad_p, 1);
			scrollok(pad_p, 0);
			goto redraw_screen;
		case 0x0d /* CR */:
			if (cursor_x > 0) {
				cursor_x = 0;
				goto move_cursor;
			}
			break;
		case 0x1b /* ESC */:
			/*
			 * start a new escape sequence
			 */
			state = ST_ESCAPE;
			break;
		}
		/*
		 * ignore all other control characters below 0x20/SPC
		 */
		goto do_nothing;
	}
	/*
	 * ignore the DEL character
	 */
	if (c == 0x7f /* DEL */) goto do_nothing;
	/*
	 * (potentially) printable character
	 */
	switch (state) {
	case ST_NORMAL:
		/*
		 * character is not part of an escape sequence
		 */
		/*
		 * translate to Unix wide character
		 */
		wc = is_graphics ? from_graph(c) : from_cpm(c);
		/*
		 * ignore untranslateable characters
		 */
		if (wc == (-1)) goto do_nothing;
		/*
		 * create curses representation of current character
		 * with all currently active attributes...
		 */
		wcs[0] = wc;
		wcs[1] = L'\0';
		pair = use_color ? get_pair(foreground, background) : 0;
		setcchar(&cc, wcs, ((is_standout ? A_STANDOUT : 0) |
		    (is_underline ? A_UNDERLINE : 0) |
		    (is_blink ? A_BLINK : 0) |
		    (is_reverse ? A_REVERSE : 0) |
		    (is_bold ? A_BOLD : 0)), pair, NULL);
		/*
		 * ... and add it to the VT52 screen
		 */
		wadd_wch(pad_p, &cc);
		if (cursor_x + 1 < cols) cursor_x++;
		goto redraw_screen;
	case ST_ESCAPE:
		/*
		 * second (and apart from escape-Y, last) character of
		 * an escape sequence
		 */
		state = ST_NORMAL;
		switch (c) {
		case 0x29 /* ) */:
			/*
			 * switch to application keypad mode (not implemented)
			 */
			app_keypad = 0;
			break;
		case 0x3d /* = */:
			/*
			 * switch to regular keypad mode (not implemented)
			 */
			app_keypad = 1;
			break;
		case 0x41 /* A */:
			/*
			 * cursor up, stop at first line
			 */
			if (cursor_y > 0) {
				cursor_y--;
				goto move_cursor;
			}
			break;
		case 0x42 /* B */:
			/*
			 * cursor down, stop at last line
			 */
			if (cursor_y + 1 < lines) {
				cursor_y++;
				goto move_cursor;
			}
			break;
		case 0x43 /* C */:
			/*
			 * cursor right, stop at last column
			 */
			if (cursor_x + 1 < cols) {
				cursor_x++;
				goto move_cursor;
			}
			break;
		case 0x44 /* D */:
			/*
			 * cursor left, stop at first column
			 */
			if (cursor_x > 0) {
				cursor_x--;
				goto move_cursor;
			}
			break;
		case 0x45 /* E */:
			/*
			 * clear screen, cursor home; extension to VT52
			 */
			cursor_x = 0;
			cursor_y = 0;
			werase(pad_p);
			goto redraw_screen;
		case 0x46 /* F */:
			/*
			 * switch codes 0x5e-0x7e to "graphics" mode
			 */
			is_graphics = 1;
			break;
		case 0x47 /* G */:
			/*
			 * switch codes 0x5e-0x7e to ASCII mode
			 */
			is_graphics = 0;
			break;
		case 0x48 /* H */:
			/*
			 * cursor home
			 */
			if (! cursor_x && ! cursor_y) goto do_nothing;
			cursor_x = 0;
			cursor_y = 0;
			goto move_cursor;
		case 0x49 /* I */:
			/*
			 * cursor up, scroll back at first line
			 */
			if (cursor_y > 0) {
				cursor_y--;
				goto move_cursor;
			}
			scrollok(pad_p, 1);
			wscrl(pad_p, (-1));
			scrollok(pad_p, 0);
			goto redraw_screen;
		case 0x4a /* J */:
			/*
			 * clear to end of screen
			 */
			wclrtobot(pad_p);
			goto redraw_screen;
		case 0x4b /* K */:
			/*
			 * clear to end of line
			 */
			wclrtoeol(pad_p);
			goto redraw_screen;
		case 0x4c /* L */:
			/*
			 * insert empty line at cursor; extension to VT52
			 */
			winsertln(pad_p);
			goto redraw_screen;
		case 0x4d /* M */:
			/*
			 * delete line at cursor; extension to VT52
			 */
			wdeleteln(pad_p);
			goto redraw_screen;
		case 0x4e /* N */:
			/*
			 * insert blank character at cursor; extension to VT52
			 */
			wins_wch(pad_p, &blank);
			goto redraw_screen;
		case 0x4f /* O */:
			/*
			 * delete character at cursor: extension to VT52
			 */
			wdelch(pad_p);
			goto redraw_screen;
		case 0x53 /* S */:
			/*
			 * set beckground color --- expect color code
			 */
			state = ST_ESCAPES;
			break;
		case 0x54 /* T */:
			/*
			 * set foreground color --- expect color code
			 */
			state = ST_ESCAPET;
			break;
		case 0x59 /* Y */:
			/*
			 * direct cursor positioning --- expect line number
			 */
			state = ST_ESCAPEY;
			break;
		case 0x5a /* Z */:
			/*
			 * send terminal identification: VT52 without
			 * hardcopy device
			 */
			in_put(0x1b /* ESC */);
			in_put(0x2f /* / */);
			in_put(0x4b /* K */);
			break;
		case 0x5b /* [ */:
			/*
			 * enter "hold screen" mode
			 */
			hold_screen = 1;
			hold_allow = cursor_y;
			break;
		case 0x5c /* \ */:
			/*
			 * exit "hold screen" mode
			 */
			hold_screen = 0;
			break;
		case 0x61 /* a */:
			/*
			 * turn off cursor; extension to VT52
			 */
			if (cursor_off) break;
			cursor_off = 1;
			if (old_cursor == ERR) break;
			old_cursor = curs_set(0);
			goto redraw_screen;
		case 0x62 /* b */:
			/*
			 * restore cursor; extension to VT52
			 */
			if (! cursor_off) break;
			if (old_cursor == ERR) break;
			curs_set(old_cursor);
			goto redraw_screen;
		case 0x63 /* c */:
			/*
			 * switch to alternate character set; extension to VT52
			 */
			charset = 1;
			break;
		case 0x64 /* d */:
			/*
			 * switch to regular character set; extension to VT52
			 */
			charset = 0;
			break;
		case 0x65 /* e */:
			/*
			 * bold on; extension to VT52
			 */
			is_bold = 1;
			break;
		case 0x66 /* f */:
			/*
			 * bold off; extension to VT52
			 */
			is_bold = 0;
			break;
		case 0x67 /* g */:
			/*
			 * underline on; extension to VT52
			 */
			is_underline = 1;
			break;
		case 0x68 /* h */:
			/*
			 * underline off; extension to VT52
			 */
			is_underline = 0;
			break;
		case 0x69 /* i */:
			/*
			 * reverse video on; extension to VT52
			 */
			is_reverse = 1;
			break;
		case 0x6a /* j */:
			/*
			 * reverse video off; extension to VT52
			 */
			is_reverse = 0;
			break;
		case 0x6b /* k */:
			/*
			 * blinking characters on; extension to VT52
			 */
			is_blink = 1;
			break;
		case 0x6c /* l */:
			/*
			 * blinking characters off; extension to VT52
			 */
			is_blink = 0;
			break;
		case 0x6d /* m */:
			/*
			 * all attributes off, reset colors to default; extension to VT52
			 */
			is_bold = 0;
			is_blink = 0;
			is_reverse = 0;
			is_underline = 0;
			is_standout = 0;
			foreground = conf_foreground;
			background = conf_background;
			break;
		case 0x6e /* n */:
			/*
			 * use alternative cursor keys; extension to VT52
			 */
			altkeys = 1;
			break;
		case 0x6f /* o */:
			/*
			 * use VT52 cursor keys; extension to VT52
			 */
			altkeys = 0;
			break;
		case 0x70 /* p */:
			/*
			 * standout (usually reverse) on; extension to VT52
			 */
			is_standout = 1;
			break;
		case 0x71 /* q */:
			/*
			 * standout (usually reverse) off; extension to VT52
			 */
			is_standout = 0;
			break;
		/*
		 * all other characters terminate the current escape
		 * sequence, but otherwise have no effect
		 */
		}
		break;
	case ST_ESCAPEY:
		/*
		 * third character of an escape-Y sequence: line number
		 */
		state = ST_ESCAPEYL;
		escape_y_line = c - 32;
		break;
	case ST_ESCAPEYL:
		/*
		 * fourth and last character of an escape-Y sequence:
		 * column number
		 */
		state = ST_NORMAL;
		escape_y_col = c - 32;
		/*
		 * a line number greater than the screen size positions
		 * the cursor on the last line of the screen
		 */
		if (escape_y_line >= lines) escape_y_line = lines - 1;
		/*
		 * a column number greater than the screen size leaves
		 * the column position unchanged
		 */
		if (escape_y_col >= cols) escape_y_col = cursor_x;
		/*
		 * ignore positioning to the current position
		 */
		if (escape_y_line != cursor_y || escape_y_col != cursor_x) {
			cursor_y = escape_y_line;
			cursor_x = escape_y_col;
			goto move_cursor;
		}
		break;
	case ST_ESCAPES:
		/*
		 * set background color
		 */
		state = ST_NORMAL;
		background = set_color(c, background, conf_background);
		break;
	case ST_ESCAPET:
		/*
		 * set foreground color
		 */
		state = ST_NORMAL;
		foreground = set_color(c, foreground, conf_foreground);
		break;
	}
	goto do_nothing;
redraw_screen:
move_cursor:
	/*
	 * changing the screen contents and cursor positioning are likewise
	 * handled by a screen refresh
	 */
	show_pad();
do_nothing:
	return;
}


/*
 * polls the keyboard to keep things like "hold screen" feature and
 * screen redrawing functional, even in the absence of regular
 * calls to crt_in() or crt_status()
 */
void
crt_poll(void) {
	/*
	 * do a non-blocking read from the screen
	 */
	if (! noblock) {
		nodelay(pad_p, 1);
		noblock = 1;
	}
	try_read();
}


/*
 * return 1 if a character can be read from the emulated terminal,
 * otherwise return 0
 */
int
crt_status(void) {
	int rc = 0;
	if (in_count) {
		/*
		 * input queue not empty
		 */
		rc = 1;
	} else {
		/*
		 * poll the keyboard
		 */
		crt_poll();
		/*
		 * this might result in characters being deposited
		 * in the input queue.
		 */
		if (in_count) rc = 1;
	}
	return rc;
}


/*
 * return a character from the emulated terminal, block until one is available
 */
unsigned char
crt_in(void) {
	int t;
	while ((t = in_get()) == (-1)) {
		/*
		 * do a blocking read from the screen
		 */
		if (noblock) {
			nodelay(pad_p, 0);
			noblock = 0;
		}
		try_read();
	}
	return (unsigned char) t;
}


/*
 * reset emulated terminal
 */
void
crt_exit(void) {
	struct timeval tv;
	/*
	 * insert delay to allow reading of the final screen contents
	 */
	switch (screen_delay) {
	case (-1):
		/*
		 * wait for a keypress
		 */
		crt_in();
		break;
	case 0:
		/*
		 * no delay
		 */
		break;
	default:
		/*
		 * wait the specified number of seconds
		 */
		tv.tv_sec = screen_delay;
		tv.tv_usec = 0;
		select(0, NULL, NULL, NULL, &tv);
		break;
	}
	reset_curses();
}
