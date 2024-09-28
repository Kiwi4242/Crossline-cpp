/* crossline.cpp -- Version 1.0
 *
 * Crossline is a small, self-contained, zero-config, MIT licensed,
 *   cross-platform, readline and libedit replacement.
 *
 * Press <F1> to get full shortcuts list.
 *
 * You can find the latest source code and description at:
 *
 *   https://github.com/Kiwi4242/Crossline-cpp
 *
 * This is a fork of https://github.com/jcwangxp/Crossline
 * which has moved the code from c to c++ to allow easy modification of 
 * some of the functions using overloading
 * ------------------------------------------------------------------------
 *
 * MIT License
 *
 * Copyright (c) 2019, JC Wang (wang_junchuan@163.com)
 * Copyright (c) 2022, John Burnell
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * ------------------------------------------------------------------------
 */

#include <map>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <stdint.h>

#ifdef _WIN32
	#include <io.h>
	#include <conio.h>
	#include <windows.h>
  #ifndef STDIN_FILENO
	#define STDIN_FILENO 			_fileno(stdin)
	#define STDOUT_FILENO 			_fileno(stdout)
  #endif
	#define isatty					_isatty
	#define strcasecmp				_stricmp
	#define strncasecmp				_strnicmp
	static int s_crossline_win = 1;
#else
	#include <unistd.h>
	#include <termios.h>
	#include <fcntl.h>
	#include <signal.h>
	#include <sys/ioctl.h>
	#include <sys/stat.h>
	static int s_crossline_win = 0;
#endif

#include "crossline.h"

/*****************************************************************************/

// Default word delimiters for move and cut
#define CROSS_DFT_DELIMITER			" !\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~"

#define CROSS_HISTORY_MAX_LINE		256		// Maximum history line number
#define CROSS_HISTORY_BUF_LEN		4096	// History line length
#define CROSS_HIS_MATCH_PAT_NUM		16		// History search pattern number

#define CROSS_COMPLET_MAX_LINE		1024	// Maximum completion word number
#define CROSS_COMPLET_WORD_LEN		64		// Completion word length
#define CROSS_COMPLET_HELP_LEN		256		// Completion word's help length
#define CROSS_COMPLET_HINT_LEN		128		// Completion syntax hints length

// Make control-characters readable
#define CTRL_KEY(key)				(key - 0x40)
// Build special key code for escape sequences
#define ALT_KEY(key)				(key + ((KEY_ESC+1)<<8))
#define ESC_KEY3(ch)				((KEY_ESC<<8) + ch)
#define ESC_KEY4(ch1,ch2)			((KEY_ESC<<8) + ((ch1)<<16) + ch2)
#define ESC_KEY6(ch1,ch2,ch3)		((KEY_ESC<<8) + ((ch1)<<16) + ((ch2)<<24) + ch3)
#define ESC_OKEY(ch)				((KEY_ESC<<8) + ('O'<<16) + ch)

/*****************************************************************************/

enum {
	KEY_TAB			= 9,	// Autocomplete.
	KEY_BACKSPACE	= 8,	// Delete character before cursor.
	KEY_ENTER		= 13,	// Accept line. (Linux)
	KEY_ENTER2		= 10,	// Accept line. (Windows)
	KEY_ESC			= 27,	// Escapce
	KEY_DEL2		= 127,  // It's treaded as Backspace is Linux
	KEY_DEBUG		= 30,	// Ctrl-^ Enter keyboard debug mode

#ifdef _WIN32 // Windows

	KEY_INSERT		= (KEY_ESC<<8) + 'R', // Paste last cut text.
	KEY_DEL			= (KEY_ESC<<8) + 'S', // Delete character under cursor.
	KEY_HOME		= (KEY_ESC<<8) + 'G', // Move cursor to start of line.
	KEY_END			= (KEY_ESC<<8) + 'O', // Move cursor to end of line.
	KEY_PGUP		= (KEY_ESC<<8) + 'I', // Move to first line in history.
	KEY_PGDN		= (KEY_ESC<<8) + 'Q', // Move to end of input history.
	KEY_UP			= (KEY_ESC<<8) + 'H', // Fetch previous line in history.
	KEY_DOWN		= (KEY_ESC<<8) + 'P', // Fetch next line in history.
	KEY_LEFT		= (KEY_ESC<<8) + 'K', // Move back a character.
	KEY_RIGHT		= (KEY_ESC<<8) + 'M', // Move forward a character.

	KEY_CTRL_DEL	= (KEY_ESC<<8) + 147, // Cut word following cursor.
	KEY_CTRL_HOME	= (KEY_ESC<<8) + 'w', // Cut from start of line to cursor.
	KEY_CTRL_END	= (KEY_ESC<<8) + 'u', // Cut from cursor to end of line.
	KEY_CTRL_UP		= (KEY_ESC<<8) + 141, // Uppercase current or following word.
	KEY_CTRL_DOWN	= (KEY_ESC<<8) + 145, // Lowercase current or following word.
	KEY_CTRL_LEFT	= (KEY_ESC<<8) + 's', // Move back a word.
	KEY_CTRL_RIGHT	= (KEY_ESC<<8) + 't', // Move forward a word.
	KEY_CTRL_BACKSPACE	= (KEY_ESC<<8) + 127, // Cut from start of line to cursor.

	KEY_ALT_DEL		= ALT_KEY(163),		// Cut word following cursor.
	KEY_ALT_HOME	= ALT_KEY(151), 	// Cut from start of line to cursor.
	KEY_ALT_END		= ALT_KEY(159), 	// Cut from cursor to end of line.
	KEY_ALT_UP		= ALT_KEY(152),		// Uppercase current or following word.
	KEY_ALT_DOWN	= ALT_KEY(160),		// Lowercase current or following word.
	KEY_ALT_LEFT	= ALT_KEY(155),		// Move back a word.
	KEY_ALT_RIGHT	= ALT_KEY(157),		// Move forward a word.
	KEY_ALT_BACKSPACE	= ALT_KEY(KEY_BACKSPACE), // Cut from start of line to cursor.

	KEY_F1			= (KEY_ESC<<8) + ';',	// Show help.
	KEY_F2			= (KEY_ESC<<8) + '<',	// Show history.
	KEY_F3			= (KEY_ESC<<8) + '=',	// Clear history (need confirm).
	KEY_F4			= (KEY_ESC<<8) + '>',	// Search history with current input.

#else // Linux

	KEY_INSERT		= ESC_KEY4('2','~'),	// vt100 Esc[2~: Paste last cut text.
	KEY_DEL			= ESC_KEY4('3','~'),	// vt100 Esc[3~: Delete character under cursor.
	KEY_HOME		= ESC_KEY4('1','~'),	// vt100 Esc[1~: Move cursor to start of line.
	KEY_END			= ESC_KEY4('4','~'),	// vt100 Esc[4~: Move cursor to end of line.
	KEY_PGUP		= ESC_KEY4('5','~'),	// vt100 Esc[5~: Move to first line in history.
	KEY_PGDN		= ESC_KEY4('6','~'),	// vt100 Esc[6~: Move to end of input history.
	KEY_UP			= ESC_KEY3('A'), 		//       Esc[A: Fetch previous line in history.
	KEY_DOWN		= ESC_KEY3('B'),		//       Esc[B: Fetch next line in history.
	KEY_LEFT		= ESC_KEY3('D'), 		//       Esc[D: Move back a character.
	KEY_RIGHT		= ESC_KEY3('C'), 		//       Esc[C: Move forward a character.
	KEY_HOME2		= ESC_KEY3('H'),		// xterm Esc[H: Move cursor to start of line.
	KEY_END2		= ESC_KEY3('F'),		// xterm Esc[F: Move cursor to end of line.

	KEY_CTRL_DEL	= ESC_KEY6('3','5','~'), // xterm Esc[3;5~: Cut word following cursor.
	KEY_CTRL_HOME	= ESC_KEY6('1','5','H'), // xterm Esc[1;5H: Cut from start of line to cursor.
	KEY_CTRL_END	= ESC_KEY6('1','5','F'), // xterm Esc[1;5F: Cut from cursor to end of line.
	KEY_CTRL_UP		= ESC_KEY6('1','5','A'), // xterm Esc[1;5A: Uppercase current or following word.
	KEY_CTRL_DOWN	= ESC_KEY6('1','5','B'), // xterm Esc[1;5B: Lowercase current or following word.
	KEY_CTRL_LEFT	= ESC_KEY6('1','5','D'), // xterm Esc[1;5D: Move back a word.
	KEY_CTRL_RIGHT	= ESC_KEY6('1','5','C'), // xterm Esc[1;5C: Move forward a word.
	KEY_CTRL_BACKSPACE	= 31, 				 // xterm Cut from start of line to cursor.
	KEY_CTRL_UP2	= ESC_OKEY('A'),		 // vt100 EscOA: Uppercase current or following word.
	KEY_CTRL_DOWN2	= ESC_OKEY('B'), 		 // vt100 EscOB: Lowercase current or following word.
	KEY_CTRL_LEFT2	= ESC_OKEY('D'),		 // vt100 EscOD: Move back a word.
	KEY_CTRL_RIGHT2	= ESC_OKEY('C'),		 // vt100 EscOC: Move forward a word.

	KEY_ALT_DEL		= ESC_KEY6('3','3','~'), // xterm Esc[3;3~: Cut word following cursor.
	KEY_ALT_HOME	= ESC_KEY6('1','3','H'), // xterm Esc[1;3H: Cut from start of line to cursor.
	KEY_ALT_END		= ESC_KEY6('1','3','F'), // xterm Esc[1;3F: Cut from cursor to end of line.
	KEY_ALT_UP		= ESC_KEY6('1','3','A'), // xterm Esc[1;3A: Uppercase current or following word.
	KEY_ALT_DOWN	= ESC_KEY6('1','3','B'), // xterm Esc[1;3B: Lowercase current or following word.
	KEY_ALT_LEFT	= ESC_KEY6('1','3','D'), // xterm Esc[1;3D: Move back a word.
	KEY_ALT_RIGHT	= ESC_KEY6('1','3','C'), // xterm Esc[1;3C: Move forward a word.
	KEY_ALT_BACKSPACE	= ALT_KEY(KEY_DEL2), // Cut from start of line to cursor.

	KEY_F1			= ESC_OKEY('P'),		 // 	  EscOP: Show help.
	KEY_F2			= ESC_OKEY('Q'),		 // 	  EscOQ: Show history.
	KEY_F3			= ESC_OKEY('R'),		 //       EscOP: Clear history (need confirm).
	KEY_F4			= ESC_OKEY('S'),		 //       EscOP: Search history with current input.

	KEY_F1_2		= ESC_KEY4('[', 'A'),	 // linux Esc[[A: Show help.
	KEY_F2_2		= ESC_KEY4('[', 'B'),	 // linux Esc[[B: Show history.
	KEY_F3_2		= ESC_KEY4('[', 'C'),	 // linux Esc[[C: Clear history (need confirm).
	KEY_F4_2		= ESC_KEY4('[', 'D'),	 // linux Esc[[D: Search history with current input.

#endif
};

/*****************************************************************************/

struct CompletionInfo {
	std::string word;
	std::string help;
	crossline_color_e	color_word;
	crossline_color_e	color_help;
	bool needQuotes;
	CompletionInfo(const std::string &w, const std::string &h, crossline_color_e cw, crossline_color_e ch, const bool needQ) {
		word = w;
		help = h;
		color_word = cw;
		color_help = ch;
		needQuotes = needQ;
	}
};



// a class for storing private variables
struct CrosslinePrivate {
	// history items
 	std::vector<HistoryItemPtr> 	s_history_items;
	bool s_history_noSearchRepeats;

	std::string	s_clip_buf; // Buf to store cut text
	crossline_completion_callback s_completion_callback = nullptr;

	int	s_paging_print_line = 0; // For paging control
	crossline_color_e s_prompt_color = CROSSLINE_COLOR_DEFAULT;

	CrosslinePrivate();
};


static std::string s_word_delimiter = CROSS_DFT_DELIMITER;
static int	s_got_resize 		= 0; // Window size changed

static bool 	crossline_readline_edit (Crossline &cLine, std::string &buf, const std::string &prompt, const bool has_input, 
										const bool in_his, const bool a_single);
static int		crossline_history_dump (Crossline &cLine, FILE *file, const bool print_id, std::string patterns, 
										std::map<int, int> &matches, const int maxNo, const bool paging,
										const bool forward);

#define isdelim(ch)		(NULL != strchr(s_word_delimiter.c_str(), ch))	// Check ch is word delimiter

// Debug macro.
#if 0
static FILE *s_crossline_debug_fp = NULL;
#define crossline_debug(...) \
	do { \
		if (NULL == s_crossline_debug_fp) { s_crossline_debug_fp = fopen("crossline_debug.txt", "a"); } \
		fprintf (s_crossline_debug_fp, __VA_ARGS__); \
		fflush (s_crossline_debug_fp); \
	} while (0)
#else
#define crossline_debug(...)
#endif

/*****************************************************************************/

static std::string s_crossline_help[] = {
" Misc Commands",
" +-------------------------+--------------------------------------------------+",
" | F1                      |  Show edit shortcuts help.                       |",
" | Ctrl-^                  |  Enter keyboard debugging mode.                  |",
" +-------------------------+--------------------------------------------------+",
" Move Commands",
" +-------------------------+--------------------------------------------------+",
" | Ctrl-B, Left            |  Move back a character.                          |",
" | Ctrl-F, Right           |  Move forward a character.                       |",
" | Up, ESC+Up              |  Move cursor to up line. (For multiple lines)    |",
" |   Ctrl-Up, Alt-Up       |  (Ctrl-Up, Alt-Up only supports Windows/Xterm)   |",
" | Down, ESC+Down          |  Move cursor to down line. (For multiple lines)  |",
" |   Ctrl-Down,Alt-Down    |  (Ctrl-Down, Alt-Down only support Windows/Xterm)|",
" | Alt-B, ESC+Left,        |  Move back a word.                               |",
" |   Ctrl-Left, Alt-Left   |  (Ctrl-Left, Alt-Left only support Windows/Xterm)|",
" | Alt-F, ESC+Right,       |  Move forward a word.                            |",
" |   Ctrl-Right, Alt-Right | (Ctrl-Right,Alt-Right only support Windows/Xterm)|",
" | Ctrl-A, Home            |  Move cursor to start of line.                   |",
" | Ctrl-E, End             |  Move cursor to end of line.                     |",
" | Ctrl-L                  |  Clear screen and redisplay line.                |",
" +-------------------------+--------------------------------------------------+",
" Edit Commands",
" +-------------------------+--------------------------------------------------+",
" | Ctrl-H, Backspace       |  Delete character before cursor.                 |",
" | Ctrl-D, DEL             |  Delete character under cursor.                  |",
" | Alt-U                   |  Uppercase current or following word.            |",
" | Alt-L                   |  Lowercase current or following word.            |",
" | Alt-C                   |  Capitalize current or following word.           |",
" | Alt-\\                  |  Delete whitespace around cursor.                |",
" | Ctrl-T                  |  Transpose character.                            |",
" +-------------------------+--------------------------------------------------+",
" Cut&Paste Commands",
" +-------------------------+--------------------------------------------------+",
" | Ctrl-K, ESC+End,        |  Cut from cursor to end of line.                 |",
" |   Ctrl-End, Alt-End     |  (Ctrl-End, Alt-End only support Windows/Xterm)  |",
" | Ctrl-U, ESC+Home,       |  Cut from start of line to cursor.               |",
" |   Ctrl-Home, Alt-Home   |  (Ctrl-Home, Alt-Home only support Windows/Xterm)|",
" | Ctrl-X                  |  Cut whole line.                                 |",
" | Alt-Backspace,          |  Cut word to left of cursor.                     |",
" |    Esc+Backspace,       |                                                  |",
" |    Clt-Backspace        |  (Clt-Backspace only supports Windows/Xterm)     |",
" | Alt-D, ESC+Del,         |  Cut word following cursor.                      |",
" |    Alt-Del, Ctrl-Del    |  (Alt-Del,Ctrl-Del only support Windows/Xterm)   |",
" | Ctrl-W                  |  Cut to left till whitespace (not word).         |",
" | Ctrl-Y, Ctrl-V, Insert  |  Paste last cut text.                            |",
" +-------------------------+--------------------------------------------------+",
" Complete Commands",
" +-------------------------+--------------------------------------------------+",
" | TAB, Ctrl-I             |  Autocomplete.                                   |",
" | Alt-=, Alt-?            |  List possible completions.                      |",
" +-------------------------+--------------------------------------------------+",
" History Commands",
" +-------------------------+--------------------------------------------------+",
" | Ctrl-P, Up              |  Fetch previous line in history.                 |",
" | Ctrl-N, Down            |  Fetch next line in history.                     |",
" | Alt-<,  PgUp            |  Move to first line in history.                  |",
" | Alt->,  PgDn            |  Move to end of input history.                   |",
" | Ctrl-R, Ctrl-S          |  Search history.                                 |",
" | F4                      |  Search history with current input.              |",
" | F1                      |  Show search help when in search mode.           |",
" | F2                      |  Show history.                                   |",
" | F3                      |  Clear history (need confirm).                   |",
" +-------------------------+--------------------------------------------------+",
" Control Commands",
" +-------------------------+--------------------------------------------------+",
" | Enter,  Ctrl-J, Ctrl-M  |  EOL and accept line.                            |",
" | Ctrl-C, Ctrl-G          |  EOF and abort line.                             |",
" | Ctrl-D                  |  EOF if line is empty.                           |",
" | Alt-R                   |  Revert line.                                    |",
" | Ctrl-Z                  |  Suspend Job. (Linux Only, fg will resume edit)  |",
" +-------------------------+--------------------------------------------------+",
" Note: If Alt-key doesn't work, an alternate way is to press ESC first then press key, see above ESC+Key.",
" Note: In multiple lines:",
"       Up/Down and Ctrl/Alt-Up, Ctrl/Alt-Down will move between lines.",
"       Up key will fetch history when cursor in first line or end of last line(for quick history move)",
"       Down key will fetch history when cursor in last line.",
"       Ctrl/Alt-Up, Ctrl/Alt-Down will just move between lines.",
""};

static std::string s_search_help[] = {
"Patterns are separated by ' ', patter match is case insensitive:",
"  (Hint: use Ctrl-Y/Ctrl-V/Insert to paste last paterns)",
"    select:   choose line including 'select'",
"    -select:  choose line excluding 'select'",
"    \"select from\":  choose line including \"select from\"",
"    -\"select from\": choose line excluding \"select from\"",
"Example:",
"    \"select from\" where -\"order by\" -limit:  ",
"         choose line including \"select from\" and 'where'",
"         and excluding \"order by\" or 'limit'",
""};


/*****************************************************************************/

// Main API to read a line, return buf if get line, return NULL if EOF.
static bool crossline_readline_internal (Crossline &cLine, const std::string &prompt, std::string &buf, bool has_input)
{
	int not_support = 0;

#ifdef TODO
	if (buf.length() == 0) { 
		buf.clear();
		return false; 
	}
#endif	
	if (!isatty(STDIN_FILENO)) {  // input is not from a terminal
		not_support = 1;
	} else {
		char *term = getenv("TERM");
		if (NULL != term) {
			if (!strcasecmp(term, "dumb") || !strcasecmp(term, "cons25") ||  !strcasecmp(term, "emacs"))
				{ not_support = 1; }
		}
	}
	if (not_support) {
		char tmpBuf[CROSS_HISTORY_BUF_LEN];
		int sz = CROSS_HISTORY_BUF_LEN;
        if (NULL == fgets(tmpBuf, sz, stdin)) { 
        	buf.clear();
        	return false; 
        }
        // int len;
        // for (len = sizeof(tmpBuf); (len > 0) && (('\n'==buf[len-1]) || ('\r'==buf[len-1])); --len) { 
        // 	// buf[len-1] = '\0'; 
        // }
        buf = tmpBuf;
        return true;
	}

	return crossline_readline_edit (cLine, buf, prompt, has_input, false, false);
}



bool Crossline::crossline_readline (const std::string &prompt, std::string &buf)
{
	return crossline_readline_internal (*this, prompt, buf, false);
}

bool Crossline::crossline_readline2 (const std::string &prompt, std::string &buf)
{
	return crossline_readline_internal (*this, prompt, buf, true);
}

// Set move/cut word delimiter, defaut is all not digital and alphabetic characters.
void  Crossline::crossline_delimiter_set (const std::string &delim)
{
	if (delim.length() > 0) {
		s_word_delimiter = delim;
	}
}

void Crossline::crossline_history_show (void)
{
	std::map<int, int> matches;
	crossline_history_dump (*this, stdout, 1, "", matches, 0, isatty(STDIN_FILENO), false);
}

void  Crossline::HistoryClear (void)
{
	info->s_history_items.clear();
	// info->s_history_id = 0;
}

int Crossline::HistorySave (const std::string &filename)
{
	if (filename.length() == 0) {
		return -1;
	} else {
		FILE *file = fopen(filename.c_str(), "wt");
		if (file == NULL) {	return -1;	}
		std::map<int, int> matches;
		crossline_history_dump (*this, file, 0, "", matches, 0, false, true);
		fclose(file);
	}
	return 0;
}

int Crossline::HistoryLoad (const std::string &filename)
{
	int		len;
	char	buf[CROSS_HISTORY_BUF_LEN];
	FILE	*file;

	if (filename.length() == 0)	{
		return -1; 
	}
	file = fopen(filename.c_str(), "rt");
	if (NULL == file) { 
		return -1; 
	}

	while (NULL != fgets(buf, CROSS_HISTORY_BUF_LEN, file)) {
        for (len = (int)strlen(buf); (len > 0) && (('\n'==buf[len-1]) || ('\r'==buf[len-1])); --len) { 
        	buf[len-1] = '\0'; 
        }
		if (len > 0) {
			buf[CROSS_HISTORY_BUF_LEN-1] = '\0';
			HistoryItemPtr item = std::make_shared<HistoryItemBase>(buf);
			HistoryAdd(item);
			// strcpy (s_history_buf[(s_history_id++) % CROSS_HISTORY_MAX_LINE], buf);
		}
	}
	fclose(file);
	return 0;
}

void Crossline::HistoryAdd(const HistoryItemPtr &item) 
{
	info->s_history_items.push_back(item);
}

void Crossline::HistoryAdd(const std::string &item) 
{
	HistoryItemPtr ptr = std::make_shared<HistoryItemBase>(item);
	HistoryAdd(ptr);
}

HistoryItemPtr Crossline::GetHistoryItem(const ssize_t n, const bool forward)
{
	int ind = n;	
	if (!forward) {
		ind = info->s_history_items.size() - 1 - n;
	}
	return info->s_history_items[ind];
}

int Crossline::HistoryCount()
{
	return info->s_history_items.size();
}

void Crossline::HistorySetup(const bool noSearchRepeats)
{
	info->s_history_noSearchRepeats = noSearchRepeats;
}

// Register completion callback.
void Crossline::crossline_completion_register (crossline_completion_callback pCbFunc)
{
	info->s_completion_callback = pCbFunc;
}

// Add completion in callback. Word is must, help for word is optional.
void  Crossline::crossline_completion_add_color (CrosslineCompletions &pCompletions, const std::string &word, 
	  											 crossline_color_e wcolor, const std::string &help, crossline_color_e hcolor)
{
	if ((word.length() > 0)) {
		pCompletions.Add(word, help, wcolor, hcolor);
	}
}

void Crossline::crossline_completion_add (CrosslineCompletions &pCompletions, const std::string &word, const std::string &help)
{
	crossline_completion_add_color (pCompletions, word, CROSSLINE_COLOR_DEFAULT, help, CROSSLINE_COLOR_DEFAULT);
}

// Set syntax hints in callback.
void  Crossline::crossline_hints_set_color (CrosslineCompletions &pCompletions, const std::string &hints, crossline_color_e color)
{
#ifdef TODO	
	if ((hints.length() > 0)) {
		pCompletions.hints = hints;
		// pCompletions.hints[CROSS_COMPLET_HINT_LEN - 1] = '\0';
		pCompletions.color_hints = color;
	}
#endif	
}

void Crossline::crossline_hints_set (CrosslineCompletions &pCompletions, const std::string &hints)
{
	crossline_hints_set_color (pCompletions, hints, CROSSLINE_COLOR_DEFAULT);
}

/*****************************************************************************/

int Crossline::crossline_paging_set (int enable)
{
	int prev = info->s_paging_print_line >=0;
	info->s_paging_print_line = enable ? 0 : -1;
	return prev;
}

int Crossline::crossline_paging_check (int line_len)
{
	std::string paging_hints("*** Press <Space> or <Enter> to continue . . .");
	int	i, ch, rows, cols, len = paging_hints.length();

	if ((info->s_paging_print_line < 0) || !isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO))	{ 
		return 0; 
	}
	crossline_screen_get (&rows, &cols);
	info->s_paging_print_line += (line_len + cols - 1) / cols;
	if (info->s_paging_print_line >= (rows - 1)) {
		printf ("%s", paging_hints.c_str());
		ch = crossline_getch();
		if (0 == ch) { crossline_getch(); }	// some terminal server may send 0 after Enter
		// clear paging hints
		for (i = 0; i < len; ++i) { printf ("\b"); }
		for (i = 0; i < len; ++i) { printf (" ");  }
		for (i = 0; i < len; ++i) { printf ("\b"); }
		info->s_paging_print_line = 0;
		if ((' ' != ch) && (KEY_ENTER != ch) && (KEY_ENTER2 != ch)) {
			return 1; 
		}
	}
	return 0;
}

/*****************************************************************************/

void  Crossline::crossline_prompt_color_set (crossline_color_e color)
{
	info->s_prompt_color	= color;
}

void Crossline::crossline_screen_clear ()
{
	int ret = system (s_crossline_win ? "cls" : "clear");
	(void) ret;
}

#ifdef _WIN32	// Windows

int Crossline::crossline_getch (void)
{
	fflush (stdout);
	return _getch();
}

void Crossline::crossline_screen_get (int *pRows, int *pCols)
{
	CONSOLE_SCREEN_BUFFER_INFO inf;
	GetConsoleScreenBufferInfo (GetStdHandle(STD_OUTPUT_HANDLE), &inf);
	*pCols = inf.srWindow.Right - inf.srWindow.Left + 1;
	*pRows = inf.srWindow.Bottom - inf.srWindow.Top + 1;
	*pCols = *pCols > 1 ? *pCols : 160;
	*pRows = *pRows > 1 ? *pRows : 24;
}

int Crossline::crossline_cursor_get (int *pRow, int *pCol)
{
	CONSOLE_SCREEN_BUFFER_INFO inf;
	GetConsoleScreenBufferInfo (GetStdHandle(STD_OUTPUT_HANDLE), &inf);
	*pRow = inf.dwCursorPosition.Y - inf.srWindow.Top;
	*pCol = inf.dwCursorPosition.X - inf.srWindow.Left;
	return 0;
}

void Crossline::crossline_cursor_set (int row, int col)
{
	CONSOLE_SCREEN_BUFFER_INFO inf;
	GetConsoleScreenBufferInfo (GetStdHandle(STD_OUTPUT_HANDLE), &inf);
	inf.dwCursorPosition.Y = (SHORT)row + inf.srWindow.Top;	
	inf.dwCursorPosition.X = (SHORT)col + inf.srWindow.Left;
	SetConsoleCursorPosition (GetStdHandle(STD_OUTPUT_HANDLE), inf.dwCursorPosition);
}

void Crossline::crossline_cursor_move (int row_off, int col_off)
{
	CONSOLE_SCREEN_BUFFER_INFO inf;
	GetConsoleScreenBufferInfo (GetStdHandle(STD_OUTPUT_HANDLE), &inf);
	inf.dwCursorPosition.Y += (SHORT)row_off;
	inf.dwCursorPosition.X += (SHORT)col_off;
	SetConsoleCursorPosition (GetStdHandle(STD_OUTPUT_HANDLE), inf.dwCursorPosition);
}

void Crossline::crossline_cursor_hide (int bHide)
{
	CONSOLE_CURSOR_INFO inf;
	GetConsoleCursorInfo (GetStdHandle(STD_OUTPUT_HANDLE), &inf);
	inf.bVisible = !bHide;
	SetConsoleCursorInfo (GetStdHandle(STD_OUTPUT_HANDLE), &inf);
}

void Crossline::crossline_color_set (crossline_color_e color)
{
    CONSOLE_SCREEN_BUFFER_INFO info;
	static WORD dft_wAttributes = 0;
	WORD wAttributes = 0;
	if (!dft_wAttributes) {
		GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info);
		dft_wAttributes = info.wAttributes;
	}
	if (CROSSLINE_FGCOLOR_DEFAULT == (color&CROSSLINE_FGCOLOR_MASK)) {
		wAttributes |= dft_wAttributes & (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
	} else {
		wAttributes |= (color&CROSSLINE_FGCOLOR_BRIGHT) ? FOREGROUND_INTENSITY : 0;
		switch (color&CROSSLINE_FGCOLOR_MASK) {
			case CROSSLINE_FGCOLOR_RED:  	wAttributes |= FOREGROUND_RED;	break;
			case CROSSLINE_FGCOLOR_GREEN:  	wAttributes |= FOREGROUND_GREEN;break;
			case CROSSLINE_FGCOLOR_BLUE:  	wAttributes |= FOREGROUND_BLUE;	break;
			case CROSSLINE_FGCOLOR_YELLOW:  wAttributes |= FOREGROUND_RED | FOREGROUND_GREEN;	break;
			case CROSSLINE_FGCOLOR_MAGENTA: wAttributes |= FOREGROUND_RED | FOREGROUND_BLUE;	break;
			case CROSSLINE_FGCOLOR_CYAN:	wAttributes |= FOREGROUND_GREEN | FOREGROUND_BLUE;	break;
			case CROSSLINE_FGCOLOR_WHITE:   wAttributes |= FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;break;
			case CROSSLINE_FGCOLOR_BLUEGREEN:	wAttributes |= 11;	break;    // fixt this later
		}
	}
	if (CROSSLINE_BGCOLOR_DEFAULT == (color&CROSSLINE_BGCOLOR_MASK)) {
		wAttributes |= dft_wAttributes & (BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE | BACKGROUND_INTENSITY);
	} else {
		wAttributes |= (color&CROSSLINE_BGCOLOR_BRIGHT) ? BACKGROUND_INTENSITY : 0;
		switch (color&CROSSLINE_BGCOLOR_MASK) {
			case CROSSLINE_BGCOLOR_RED:  	wAttributes |= BACKGROUND_RED;	break;
			case CROSSLINE_BGCOLOR_GREEN:  	wAttributes |= BACKGROUND_GREEN;break;
			case CROSSLINE_BGCOLOR_BLUE:  	wAttributes |= BACKGROUND_BLUE;	break;
			case CROSSLINE_BGCOLOR_YELLOW:  wAttributes |= BACKGROUND_RED | BACKGROUND_GREEN;	break;
			case CROSSLINE_BGCOLOR_MAGENTA: wAttributes |= BACKGROUND_RED | BACKGROUND_BLUE;	break;
			case CROSSLINE_BGCOLOR_CYAN:	wAttributes |= BACKGROUND_GREEN | BACKGROUND_BLUE;	break;
			case CROSSLINE_BGCOLOR_WHITE:   wAttributes |= BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE;break;
		}
	}
	if (color & CROSSLINE_UNDERLINE)
		{ wAttributes |= COMMON_LVB_UNDERSCORE; }
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), wAttributes);
}

#else // Linux

int crossline_getch ()
{
	char ch = 0;
	struct termios old_term, cur_term;
	fflush (stdout);
	if (tcgetattr(STDIN_FILENO, &old_term) < 0)	{ perror("tcsetattr"); }
	cur_term = old_term;
	cur_term.c_lflag &= ~(ICANON | ECHO | ISIG); // echoing off, canonical off, no signal chars
	cur_term.c_cc[VMIN] = 1;
	cur_term.c_cc[VTIME] = 0;
	if (tcsetattr(STDIN_FILENO, TCSANOW, &cur_term) < 0)	{ perror("tcsetattr"); }
	if (read(STDIN_FILENO, &ch, 1) < 0)	{ /* perror("read()"); */ } // signal will interrupt
	if (tcsetattr(STDIN_FILENO, TCSADRAIN, &old_term) < 0)	{ perror("tcsetattr"); }
	return ch;
}
void crossline_screen_get (int *pRows, int *pCols)
{
	struct winsize ws = {};
	(void)ioctl (1, TIOCGWINSZ, &ws);
	*pCols = ws.ws_col;
	*pRows = ws.ws_row;
	*pCols = *pCols > 1 ? *pCols : 160;
	*pRows = *pRows > 1 ? *pRows : 24;
}
int crossline_cursor_get (int *pRow, int *pCol)
{
	int i;
	char buf[32];
	printf ("\e[6n");
	for (i = 0; i < (char)sizeof(buf)-1; ++i) {
		buf[i] = (char)crossline_getch ();
		if ('R' == buf[i]) { break; }
	}
	buf[i] = '\0';
	if (2 != sscanf (buf, "\e[%d;%dR", pRow, pCol)) { return -1; }
	(*pRow)--; (*pCol)--;
	return 0;
}
void crossline_cursor_set (int row, int col)
{
	printf("\e[%d;%dH", row+1, col+1);
}
void crossline_cursor_move (int row_off, int col_off)
{
	if (col_off > 0)		{ printf ("\e[%dC", col_off);  }
	else if (col_off < 0)	{ printf ("\e[%dD", -col_off); }
	if (row_off > 0)		{ printf ("\e[%dB", row_off);  }
	else if (row_off < 0)	{ printf ("\e[%dA", -row_off); }
}
void crossline_cursor_hide (int bHide)
{
	printf("\e[?25%c", bHide?'l':'h');
}

void crossline_color_set (crossline_color_e color)
{
	if (!isatty(STDOUT_FILENO))		{ return; }
	printf ("\033[m");
	if (CROSSLINE_FGCOLOR_DEFAULT != (color&CROSSLINE_FGCOLOR_MASK)) 
		{ printf ("\033[%dm", 29 + (color&CROSSLINE_FGCOLOR_MASK) + ((color&CROSSLINE_FGCOLOR_BRIGHT)?60:0)); }
	if (CROSSLINE_BGCOLOR_DEFAULT != (color&CROSSLINE_BGCOLOR_MASK)) 
		{ printf ("\033[%dm", 39 + ((color&CROSSLINE_BGCOLOR_MASK)>>8) + ((color&CROSSLINE_BGCOLOR_BRIGHT)?60:0)); }
	if (color & CROSSLINE_UNDERLINE)
		{ printf ("\033[4m"); }
}

#endif // #ifdef _WIN32

/*****************************************************************************/

static void crossline_show_help (Crossline &cLine, int show_search)
{
	int	i;
	std::string *help = show_search ? s_search_help : s_crossline_help;
 	printf (" \b\n");
	for (i = 0; help[i].length()>0; ++i) {
		printf ("%s\n", help[i].c_str());
		if (cLine.crossline_paging_check (help[i].length()+1))
			{ break; }
	}
}

static void str_to_lower (std::string &str)
{
	for (int i = 0; i < str.length(); i++) { 
		str[i] = (char)tolower (str[i]); 
	}
}

// Match including(no prefix) and excluding(with prefix: '-') patterns.
static bool crossline_match_patterns (const std::string &str, std::vector<std::string> &word, int num)
{
	int i;
	std::string buf;

	buf = str;
	str_to_lower (buf);
	for (i = 0; i < num; ++i) {
		std::string::const_iterator wp = word[i].begin();
		if ('-' == word[i][0]) {
			if (NULL != strstr (buf.c_str(), &*(wp+1))) { 
				return false; 
			}
		} else if (NULL == strstr (buf.c_str(), &*wp)) { 
			return false; 
		}
	}
	return true;
}

// Split pattern string to individual pattern list, handle composite words embraced with " ".
static int crossline_split_patterns (std::string &patterns, std::vector<std::string> &pat_list)
{
	int i, num = 0;

	if (patterns.length() == 0) { 
		return 0; 
	}

	std::string::iterator pch = patterns.begin();	
	while (' ' == *pch)	{ 
		++pch; 
	}
	while ((pch != patterns.end())) {
		if (('"' == *pch) || (('-' == *pch) && ('"' == *(pch+1)))) {
			if ('"' != *pch)	{ 
				*(pch+1) = '-'; 
			}
			pat_list.push_back(&*(++pch));
			num++;

			int pos = pch - patterns.begin();			
			int ind = patterns.find("\"", pos);
			if (ind != patterns.npos) {
				pch = patterns.begin() + pos;
				*pch = '\0';
				pch++;
				while (' ' == *pch)	{ ++pch; }
			} else {
				pch = patterns.end();
			}
		} else {
			pat_list.push_back(&*pch);
			num++;
			int pos = pch - patterns.begin();			
			int ind = patterns.find(" ", pos);
			if (ind != patterns.npos) {
				pch = patterns.begin() + pos;
				*pch = '\0';
				pch++;
				while (' ' == *pch)	{ ++pch; }
			} else {
				pch = patterns.end();
			}

		}
	}
	for (i = 0; i < num; ++i)
		{ str_to_lower (pat_list[i]); }

	return num;
}

// If patterns is not NULL, will filter history.
// matches gives a list matching the dump id with the index in history
static int crossline_history_dump (Crossline &cLine, FILE *file, const bool print_id, std::string patterns, 
								   std::map<int, int> &matches, const int maxNo, const bool paging,
								   const bool forward)
{
	uint32_t i;
	int		id = 0, num=0;
	std::vector<std::string> pat_list;

	bool noRepeats = cLine.info->s_history_noSearchRepeats;
	std::map<std::string, int> matched;

	num = crossline_split_patterns (patterns, pat_list);
	int noShow = cLine.HistoryCount();
	if (maxNo > 0 and noShow > maxNo) {
		noShow = maxNo;
	}

	// search through history starting at most recent
	int no = 0;
	for (i = 0; i < cLine.HistoryCount(); i++) {
		// std::string &history = cLine.info->s_history_items[i];
		HistoryItemPtr hisPtr = cLine.GetHistoryItem(i, forward);
		const std::string &history = hisPtr->item;
		if (history.length() > 0) {
			if ((patterns.length() > 0) && !crossline_match_patterns (history, pat_list, num)) { 
				continue; 
			}
			if (noRepeats && matched.count(history)) {
				continue;
			}
			matched[history] = i;
			if (print_id) { 
				fprintf (file, "%4d:  %s\n", ++id, history.c_str()); 
				matches[id] = i;
			} else { 
				fprintf (file, "%s\n", history.c_str()); 
			}
			if (++no > noShow) {
				break;
			}
			if (paging) {
				if (cLine.crossline_paging_check (history.length()+(print_id?7:1)))	{ 
					break; 
				}
			}
		}
	}
	return no;
}

// Search history, input will be initial search patterns.
int Crossline::crossline_history_search (std::string &input)
{ 
	uint32_t his_id = 0, count;
	// char buf[8] = "1";
	std::string buf = "1";
	std::string pattern;

	printf (" \b\n");
	if (input.length() > 0) {
		pattern = input;
	} else {
		// Get search patterns
		if (!crossline_readline_edit(*this, pattern, "History Search: ", (input.length()>0), true, false)) { 
			return 0; 
		}
	}
	info->s_clip_buf = pattern;

	std::map<int, int> matches;
	count = crossline_history_dump(*this, stdout, 1, pattern, matches, 8, true, false);
	if (0 == count)	{ 
		return 0; 
	} // Nothing found, just return

	if (count > 0) {
		// Get choice
		if (!crossline_readline_edit (*this, buf, "Input history id: ", (1==count), true, true)) {
			return 0;
		}
		his_id = atoi(buf.c_str());

		if (('\0' != buf[0]) && ((his_id > count) || (his_id <= 0))) {
			printf ("Invalid history id: %s\n", buf.c_str());
			return 0;
		}
	} else {
		his_id = 1;
	}
	his_id = matches[his_id];
	// return crossline_history_dump (*this, stdout, 1, pattern, false);
	return his_id;
}

// Show completions returned by callback.
static int crossline_show_completions (Crossline &cLine, CrosslineCompletions &pCompletions)
{
	int i, j, ret = 0, word_len = 0, with_help = 0, rows, cols, word_num;

#ifdef TODO
	if (('\0' != pCompletions.hints[0]) || (pCompletions.num > 0)) {
		printf (" \b\n");
		ret = 1;
	}
	// Print syntax hints.
	if ('\0' != pCompletions.hints[0]) {
		printf ("Please input: "); 
		cLine.crossline_color_set (pCompletions.color_hints);
		printf ("%s", pCompletions.hints.c_str()); 
		cLine.crossline_color_set (CROSSLINE_COLOR_DEFAULT);
		printf ("\n");
	}
	if (0 == pCompletions.num)	{ 
		return ret; 
	}
#else	
	if (pCompletions.Size() > 0) {
		printf (" \b\n");
		ret = 1;
	}
	// Print syntax hints.
	if (pCompletions.hint.length() > 0) {
		printf ("Please input: "); 
		cLine.crossline_color_set (pCompletions.color_hint);
		printf ("%s", pCompletions.hint.c_str()); 
		cLine.crossline_color_set (CROSSLINE_COLOR_DEFAULT);
		printf ("\n");
	}
#endif	
	for (i = 0; i < pCompletions.Size(); ++i) {
		CompletionInfoPtr comp = pCompletions.Get(i);
		if (comp->word.length() > word_len) { 
			word_len = comp->word.length(); 
		}
		if (comp->help.length() > 0)	{ 
			with_help = 1; 
		}
	}
	if (with_help) {
		// Print words with help format.
		for (i = 0; i < pCompletions.Size(); ++i) {
			CompletionInfoPtr comp = pCompletions.Get(i);
			cLine.crossline_color_set (comp->color_word);
			printf ("%4d:  %s", i+1, comp->word.c_str());
			for (j = 0; j < 4+word_len-comp->word.length(); ++j) { 
				printf (" "); 
			}
			cLine.crossline_color_set (comp->color_help);
			printf ("%s", comp->help.c_str());
			cLine.crossline_color_set (CROSSLINE_COLOR_DEFAULT);
			printf ("\n");
			if (cLine.crossline_paging_check(comp->help.length()+4+word_len+1))
				{ break; }
		}
		return ret;
	}

	// Print words list in multiple columns.
	cLine.crossline_screen_get (&rows, &cols);
	word_len += 7;    // add "1:  ""
	word_num = (cols - 1 - word_len) / (word_len + 4) + 1;
	for (i = 1; i <= pCompletions.Size(); ++i) {
		CompletionInfoPtr comp = pCompletions.Get(i-1);
		cLine.crossline_color_set (comp->color_word);
		printf ("%4d: %s", i, comp->word.c_str());
		cLine.crossline_color_set (CROSSLINE_COLOR_DEFAULT);
		for (j = 0; j < ((i%word_num)?4:0)+word_len-comp->word.length(); ++j)
			{ printf (" "); }
		if (0 == (i % word_num)) {
			printf ("\n");
			if (cLine.crossline_paging_check (word_len))
				{ return ret; }
		}
	}

	if (pCompletions.Size() % word_num) { printf ("\n"); }
	return ret;
}

static int crossline_updown_move (Crossline &cLine, const std::string &prompt, int *pCurPos, int *pCurNum, int off, int bForce)
{
	int rows, cols, len = prompt.length(), cur_pos=*pCurPos;
	cLine.crossline_screen_get (&rows, &cols);
	if (!bForce && (*pCurPos == *pCurNum))	{ return 0; } // at end of last line
	if (off < 0) {
		if ((*pCurPos+len)/cols == 0) { return 0; } // at first line
		*pCurPos -= cols;
		if (*pCurPos < 0) { *pCurPos = 0; }
		cLine.crossline_cursor_move (-1, (*pCurPos+len)%cols-(cur_pos+len)%cols);
	} else {
		if ((*pCurPos+len)/cols == (*pCurNum+len)/cols) { return 0; } // at last line
		*pCurPos += cols;
		if (*pCurPos > *pCurNum) { *pCurPos = *pCurNum - 1; } // one char left to avoid history shortcut
		cLine.crossline_cursor_move (1, (*pCurPos+len)%cols-(cur_pos+len)%cols);
	}
	return 1;
}

// refresh current print line and move cursor to new_pos.
static void crossline_refresh (Crossline &cLine, const std::string &prompt, std::string &buf, int *pCurPos, int *pCurNum, 
							  const int new_pos, const int new_num, const int bChg)
{
	int i, pos_row, pos_col, len = prompt.length();
	static int rows = 0, cols = 0;

	if (len + buf.length() + 1 > 119) {
		rows = 0;
	}
	if (bChg || !rows || s_crossline_win) { 
		cLine.crossline_screen_get (&rows, &cols); 
	}
	if (!bChg) { // just move cursor
		pos_row = (new_pos+len)/cols - (*pCurPos+len)/cols;
		pos_col = (new_pos+len)%cols - (*pCurPos+len)%cols;
		cLine.crossline_cursor_move (pos_row, pos_col);
	} else {
		if (new_num > 0) {
			if (new_num < buf.length()) {
				buf = buf.substr(0, new_num);
			}
		} else {
			buf.clear();
		}
		// buf[new_num] = '\0';
		if (bChg > 1) { // refresh as less as possbile
			std::string::const_iterator it = buf.begin();
			printf ("%s", &*(it + bChg - 1));
		} else {
			pos_row = (*pCurPos + len - 1) / cols;      // not sure why -1
			if (pos_row != 0) {
				cLine.crossline_cursor_move (-pos_row, 0);
			}
			cLine.crossline_color_set (cLine.info->s_prompt_color);
			printf ("\r%s", prompt.c_str());
			cLine.crossline_color_set (CROSSLINE_COLOR_DEFAULT);
			printf ("%s", buf.c_str());
		}
		if (!s_crossline_win && new_num>0 && !((new_num+len)%cols)) { 
			printf("\n"); 
		}
		// JGB should this be i=0
		// for (i=*pCurNum-new_num; i > 0; --i) { 
		for (i=*pCurNum-new_num; i >= 0; --i) { 
			printf (" "); 
		}
		if (!s_crossline_win && *pCurNum>new_num && !((*pCurNum+len)%cols)) { 
			printf("\n"); 
		}
		pos_row = (new_num+len)/cols - (*pCurNum+len)/cols;
		if (pos_row < 0) { 
			cLine.crossline_cursor_move (pos_row, 0); 
		} 
		printf ("\r");
		pos_row = (new_pos+len)/cols - (new_num+len)/cols;
		cLine.crossline_cursor_move (pos_row, (new_pos+len)%cols);
	}
	*pCurPos = new_pos;
	*pCurNum = new_num;
}

static void crossline_print (Crossline &cLine, const std::string &prompt, std::string &buf, int *pCurPos, int *pCurNum, int new_pos, int new_num)
{
	*pCurPos = *pCurNum = 0;
	crossline_refresh (cLine, prompt, buf, pCurPos, pCurNum, new_pos, new_num, 1);
}

// Copy part text[cut_beg, cut_end] from src to dest
static void crossline_text_copy (std::string &dest, const std::string &src, int cut_beg, int cut_end)
{
	int len = cut_end - cut_beg;
	len = (len < CROSS_HISTORY_BUF_LEN) ? len : (CROSS_HISTORY_BUF_LEN - 1);
	if (len > 0) {
		if (cut_beg == 0 && len == src.length()) {
			dest = src;
		} else {
			dest = src.substr(cut_beg, len);
		}
	} else {
		dest.clear();
	}
}

// Copy from history buffer to dest
static void crossline_history_copy (Crossline &cLine, const std::string &prompt, std::string &buf, int *pos, int *num, int history_id)
{
	HistoryItemPtr it = cLine.GetHistoryItem(history_id, true);
	buf = it->item;;
	crossline_refresh (cLine, prompt, buf, pos, num, buf.length(), buf.length(), 1);
}

/*****************************************************************************/

// Convert ESC+Key to Alt-Key
static int crossline_key_esc2alt (int ch)
{
	switch (ch) {
	case KEY_DEL:	ch = KEY_ALT_DEL;	break;
	case KEY_HOME:	ch = KEY_ALT_HOME;	break;
	case KEY_END:	ch = KEY_ALT_END;	break;
	case KEY_UP:	ch = KEY_ALT_UP;	break;
	case KEY_DOWN:	ch = KEY_ALT_DOWN;	break;
	case KEY_LEFT:	ch = KEY_ALT_LEFT;	break;
	case KEY_RIGHT:	ch = KEY_ALT_RIGHT;	break;
	case KEY_BACKSPACE:	ch = KEY_ALT_BACKSPACE;	break;
	}
	return ch;
}

// Map other function keys to main key
static int crossline_key_mapping (int ch)
{
	switch (ch) {
#ifndef _WIN32
	case KEY_HOME2:			ch = KEY_HOME;			break;
	case KEY_END2:			ch = KEY_END;			break;
	case KEY_CTRL_UP2:		ch = KEY_CTRL_UP;		break;
	case KEY_CTRL_DOWN2:	ch = KEY_CTRL_DOWN;		break;
	case KEY_CTRL_LEFT2:	ch = KEY_CTRL_LEFT;		break;
	case KEY_CTRL_RIGHT2:	ch = KEY_CTRL_RIGHT;	break;
	case KEY_F1_2:			ch = KEY_F1;			break;
	case KEY_F2_2:			ch = KEY_F2;			break;
	case KEY_F3_2:			ch = KEY_F3;			break;
	case KEY_F4_2:			ch = KEY_F4;			break;
#endif
	case KEY_DEL2:			ch = KEY_BACKSPACE;		break;
	}
	return ch;
}

#ifdef _WIN32	// Windows
// Read a KEY from keyboard, is_esc indicats whether it's a function key.
static int crossline_getkey (Crossline &cLine, int *is_esc)
{
	int ch = cLine.crossline_getch (), esc;
	if ((GetKeyState (VK_CONTROL) & 0x8000) && (KEY_DEL2 == ch)) {
		ch = KEY_CTRL_BACKSPACE;
	} else if ((224 == ch) || (0 == ch)) {
		*is_esc = 1;
		ch = cLine.crossline_getch ();
		ch = (GetKeyState (VK_MENU) & 0x8000) ? ALT_KEY(ch) : ch + (KEY_ESC<<8);
	} else if (KEY_ESC == ch) { // Handle ESC+Key
		*is_esc = 1;
		ch = crossline_getkey (cLine, &esc);
		ch = crossline_key_esc2alt (ch);
	} else if (GetKeyState (VK_MENU) & 0x8000 && !(GetKeyState (VK_CONTROL) & 0x8000) ) {
		*is_esc = 1; ch = ALT_KEY(ch);
	}
	return ch;
}

void crossline_winchg_reg (void)	{ }

#else // Linux

// Convert escape sequences to internal special function key
static int crossline_get_esckey (int ch)
{
	int ch2;
	if (0 == ch)	{ ch = crossline_getch (); }
	if ('[' == ch) {
		ch = crossline_getch ();
		if ((ch>='0') && (ch<='6')) {
			ch2 = crossline_getch ();
			if ('~' == ch2)	{ ch = ESC_KEY4 (ch, ch2); } // ex. Esc[4~
			else if (';' == ch2) {
				ch2 = crossline_getch();
				if (('5' != ch2) && ('3' != ch2))
					{ return 0; }
				ch = ESC_KEY6 (ch, ch2, crossline_getch()); // ex. Esc[1;5B
			}
		} else if ('[' == ch) {
			ch = ESC_KEY4 ('[', crossline_getch());	// ex. Esc[[A
		} else { ch = ESC_KEY3 (ch); }	// ex. Esc[A
	} else if ('O' == ch) {
		ch = ESC_OKEY (crossline_getch());	// ex. EscOP
	} else { ch = ALT_KEY (ch); } // ex. Alt+Backspace
	return ch;
}

// Read a KEY from keyboard, is_esc indicats whether it's a function key.
static int crossline_getkey (int *is_esc)
{
	int ch = crossline_getch();
	if (KEY_ESC == ch) {
		*is_esc = 1;
		ch = crossline_getch ();
		if (KEY_ESC == ch) { // Handle ESC+Key
			ch = crossline_get_esckey (0);
			ch = crossline_key_mapping (ch);
			ch = crossline_key_esc2alt (ch);
		} else { ch = crossline_get_esckey (ch); }
	}
	return ch;
}

static void crossline_winchg_event (int arg)
{ s_got_resize = 1; }
static void crossline_winchg_reg (void)
{
	struct sigaction sa;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sa.sa_handler = &crossline_winchg_event;
	sigaction (SIGWINCH, &sa, NULL);
	s_got_resize = 0;
}

#endif // #ifdef _WIN32

/*****************************************************************************/

/* Internal readline from terminal. has_input indicates buf has inital input.
 * in_his will disable history and complete shortcuts
 */
static bool crossline_readline_edit (Crossline &cLine, std::string &buf, const std::string &prompt, const bool has_input, 
									const bool in_his, const bool a_single)
{
	int		pos = 0, num = 0, read_end = 0, is_esc;
	int		ch, len, new_pos, copy_buf = 0, i, len2;
	uint32_t search_his;
	// char	input[CROSS_HISTORY_BUF_LEN];
	std::string input;

	int rows, cols;
	cLine.crossline_screen_get (&rows, &cols);

	bool has_his = false;
	int32_t history_id;
    history_id = cLine.HistoryCount();
	if (history_id > 0) {
	 	history_id--;
	 	has_his = true;
	} 

	if (has_input) {
		num = pos = buf.length();
		input = buf;
		// crossline_text_copy (input, buf, pos, num);
	} else { 
		// buf[0] = input[0] = '\0'; 
		buf.clear();
		input.clear();
	}
	crossline_print (cLine, prompt, buf, &pos, &num, pos, num);
	crossline_winchg_reg ();

	do {
		is_esc = 0;
		ch = crossline_getkey (cLine, &is_esc);
		ch = crossline_key_mapping (ch);

		if (s_got_resize) { // Handle window resizing for Linux, Windows can handle it automatically
			new_pos = pos;
			crossline_refresh (cLine, prompt, buf, &pos, &num, 0, num, 0); // goto beginning of line
			printf ("\x1b[J"); // clear to end of screen
			crossline_refresh (cLine, prompt, buf, &pos, &num, new_pos, num, 1);
			s_got_resize = 0;
		}

		switch (ch) {
/* Misc Commands */
		case KEY_F1:	// Show help
			crossline_show_help (cLine, in_his);
			crossline_print (cLine, prompt, buf, &pos, &num, pos, num);
			break;

		case KEY_DEBUG:	// Enter keyboard debug mode
			printf(" \b\nEnter keyboard debug mode, <Ctrl-C> to exit debug\n");
			while (CTRL_KEY('C') != (ch=cLine.crossline_getch()))
				{ printf ("%3d 0x%02x (%c)\n", ch, ch, isprint(ch) ? ch : ' '); }
			crossline_print (cLine, prompt, buf, &pos, &num, pos, num);
			break;

/* Move Commands */
		case KEY_LEFT:	// Move back a character.
		case CTRL_KEY('B'):
			if (pos > 0)
				{ crossline_refresh (cLine, prompt, buf, &pos, &num, pos-1, num, 0); }
			break;

		case KEY_RIGHT:	// Move forward a character.
		case CTRL_KEY('F'):
			if (pos < num)
				{ crossline_refresh (cLine, prompt, buf, &pos, &num, pos+1, num, 0); }
			break;

		case ALT_KEY('b'):	// Move back a word.
		case ALT_KEY('B'):
		case KEY_CTRL_LEFT:
		case KEY_ALT_LEFT:
			for (new_pos=pos-1; (new_pos > 0) && isdelim(buf[new_pos]); --new_pos)	;
			for (; (new_pos > 0) && !isdelim(buf[new_pos]); --new_pos)	;
			crossline_refresh (cLine, prompt, buf, &pos, &num, new_pos?new_pos+1:new_pos, num, 0);
			break;

		case ALT_KEY('f'):	 // Move forward a word.
		case ALT_KEY('F'):
		case KEY_CTRL_RIGHT:
		case KEY_ALT_RIGHT:
			for (new_pos=pos; (new_pos < num) && isdelim(buf[new_pos]); ++new_pos)	;
			for (; (new_pos < num) && !isdelim(buf[new_pos]); ++new_pos)	;
			crossline_refresh (cLine, prompt, buf, &pos, &num, new_pos, num, 0);
			break;

		case CTRL_KEY('A'):	// Move cursor to start of line.
		case KEY_HOME:
			crossline_refresh (cLine, prompt, buf, &pos, &num, 0, num, 0);
			break;

		case CTRL_KEY('E'):	// Move cursor to end of line
		case KEY_END:
			crossline_refresh (cLine, prompt, buf, &pos, &num, num, num, 0);
			break;

		case CTRL_KEY('L'):	// Clear screen and redisplay line
			cLine.crossline_screen_clear ();
			crossline_print (cLine, prompt, buf, &pos, &num, pos, num);
			break;

		case KEY_CTRL_UP: // Move to up line
		case KEY_ALT_UP:
			crossline_updown_move (cLine, prompt, &pos, &num, -1, 1);
			break;

		case KEY_ALT_DOWN: // Move to down line
		case KEY_CTRL_DOWN:
			crossline_updown_move (cLine, prompt, &pos, &num, 1, 1);
			break;

/* Edit Commands */
		case KEY_BACKSPACE: // Delete char to left of cursor (same with CTRL_KEY('H'))
			if (pos > 0) {
				buf.erase(pos-1, 1);
				crossline_refresh (cLine, prompt, buf, &pos, &num, pos-1, num-1, 1);
			}
			break;

		case KEY_DEL:	// Delete character under cursor
		case CTRL_KEY('D'):
			if (pos < num) {
				buf.erase(pos, 1);
				crossline_refresh (cLine, prompt, buf, &pos, &num, pos, num - 1, 1);
			} else if ((0 == num) && (ch == CTRL_KEY('D'))) // On an empty line, EOF
				 { printf (" \b\n"); read_end = -1; }
			break;

		case ALT_KEY('u'):	// Uppercase current or following word.
		case ALT_KEY('U'):
			for (new_pos = pos; (new_pos < num) && isdelim(buf[new_pos]); ++new_pos)	;
			for (; (new_pos < num) && !isdelim(buf[new_pos]); ++new_pos)
				{ buf[new_pos] = (char)toupper (buf[new_pos]); }
			crossline_refresh (cLine, prompt, buf, &pos, &num, new_pos, num, 1);
			break;

		case ALT_KEY('l'):	// Lowercase current or following word.
		case ALT_KEY('L'):
			for (new_pos = pos; (new_pos < num) && isdelim(buf[new_pos]); ++new_pos)	;
			for (; (new_pos < num) && !isdelim(buf[new_pos]); ++new_pos)
				{ buf[new_pos] = (char)tolower (buf[new_pos]); }
			crossline_refresh (cLine, prompt, buf, &pos, &num, new_pos, num, 1);
			break;

		case ALT_KEY('c'):	// Capitalize current or following word.
		case ALT_KEY('C'):
			for (new_pos = pos; (new_pos < num) && isdelim(buf[new_pos]); ++new_pos)	;
			if (new_pos<num)
				{ buf[new_pos] = (char)toupper (buf[new_pos]); }
			for (; new_pos<num && !isdelim(buf[new_pos]); ++new_pos)	;
			crossline_refresh (cLine, prompt, buf, &pos, &num, new_pos, num, 1);
			break;

		case ALT_KEY('\\'): // Delete whitespace around cursor.
			for (new_pos = pos; (new_pos > 0) && (' ' == buf[new_pos]); --new_pos)	;
			buf.erase(pos, num - pos);
			crossline_refresh (cLine, prompt, buf, &pos, &num, new_pos, num - (pos-new_pos), 1);
			for (new_pos = pos; (new_pos < num) && (' ' == buf[new_pos]); ++new_pos)	;
			buf.erase(pos, num - new_pos);
			crossline_refresh (cLine, prompt, buf, &pos, &num, pos, num - (new_pos-pos), 1);
			break;

		case CTRL_KEY('T'): // Transpose previous character with current character.
			if ((pos > 0) && !isdelim(buf[pos]) && !isdelim(buf[pos-1])) {
				ch = buf[pos];
				buf[pos] = buf[pos-1];
				buf[pos-1] = (char)ch;
				crossline_refresh (cLine, prompt, buf, &pos, &num, pos<num?pos+1:pos, num, 1);
			} else if ((pos > 1) && !isdelim(buf[pos-1]) && !isdelim(buf[pos-2])) {
				ch = buf[pos-1];
				buf[pos-1] = buf[pos-2];
				buf[pos-2] = (char)ch;
				crossline_refresh (cLine, prompt, buf, &pos, &num, pos, num, 1);
			}
			break;

/* Cut&Paste Commands */
		case CTRL_KEY('K'): // Cut from cursor to end of line.
		case KEY_CTRL_END:
		case KEY_ALT_END:
			crossline_text_copy (cLine.info->s_clip_buf, buf, pos, num);
			crossline_refresh (cLine, prompt, buf, &pos, &num, pos, pos, 1);
			break;

		case CTRL_KEY('U'): // Cut from start of line to cursor.
		case KEY_CTRL_HOME:
		case KEY_ALT_HOME:
			crossline_text_copy (cLine.info->s_clip_buf, buf, 0, pos);
			buf.erase(0, num-pos);
			crossline_refresh (cLine, prompt, buf, &pos, &num, 0, num - pos, 1);
			break;

		case CTRL_KEY('X'):	// Cut whole line.
			crossline_text_copy (cLine.info->s_clip_buf, buf, 0, num);
			// fall through
		case ALT_KEY('r'):	// Revert line
		case ALT_KEY('R'):
			crossline_refresh (cLine, prompt, buf, &pos, &num, 0, 0, 1);
			break;

		case CTRL_KEY('W'): // Cut whitespace (not word) to left of cursor.
		case KEY_ALT_BACKSPACE: // Cut word to left of cursor.
		case KEY_CTRL_BACKSPACE:
			new_pos = pos;
			if ((new_pos > 1) && isdelim(buf[new_pos-1]))	{ 
				--new_pos; 
			}
			for (; (new_pos > 0) && isdelim(buf[new_pos]); --new_pos) ;
			if (CTRL_KEY('W') == ch) {
				for (; (new_pos > 0) && (' ' != buf[new_pos]); --new_pos)	;
			} else {
				for (; (new_pos > 0) && !isdelim(buf[new_pos]); --new_pos)	;
			}
			if ((new_pos>0) && (new_pos<pos) && isdelim(buf[new_pos]))	{ 
				new_pos++; 
			}
			crossline_text_copy (cLine.info->s_clip_buf, buf, new_pos, pos);
			buf.erase(new_pos, num - pos);
			crossline_refresh (cLine, prompt, buf, &pos, &num, new_pos, num - (pos-new_pos), 1);
			break;

		case ALT_KEY('d'): // Cut word following cursor.
		case ALT_KEY('D'):
		case KEY_ALT_DEL:
		case KEY_CTRL_DEL:
			for (new_pos = pos; (new_pos < num) && isdelim(buf[new_pos]); ++new_pos)	;
			for (; (new_pos < num) && !isdelim(buf[new_pos]); ++new_pos)	;
			crossline_text_copy (cLine.info->s_clip_buf, buf, pos, new_pos);
			buf.erase(pos, num - new_pos);
			crossline_refresh (cLine, prompt, buf, &pos, &num, pos, num - (new_pos-pos), 1);
			break;

		case CTRL_KEY('Y'):	// Paste last cut text.
		case CTRL_KEY('V'):
		case KEY_INSERT:
			// if ((len=cLine.info->s_clip_buf.length()) + num < size) {
				buf.insert(pos, cLine.info->s_clip_buf, len);
				// memmove (&buf[pos+len], &buf[pos], num - pos);
				// memcpy (&buf[pos], cLine.info->s_clip_buf, len);
				crossline_refresh (cLine, prompt, buf, &pos, &num, pos+len, num+len, 1);
			// }
			break;

/* Complete Commands */
		case KEY_TAB:		// Autocomplete (same with CTRL_KEY('I'))
		case ALT_KEY('='):	// List possible completions.
		case ALT_KEY('?'): {
			if (in_his || (NULL == cLine.info->s_completion_callback) || (pos != num)) { 
				break; 
			}
			CrosslineCompletions completions;
			completions.Clear();
			cLine.info->s_completion_callback (buf, cLine, pos, completions);
			int common_add = 0;
			int pos1 = pos;
			int num1 = num;
			std::string buf1 = buf;
			if (completions.Size() >= 1) {
				if (KEY_TAB == ch) {
					std::string common = completions.FindCommon();
#ifdef TODO					
					len2 = len = completions.Get(0)->word.length();
					// Find common string for autocompletion
					if (len > 0) {
						if (len2 > num) len2 = num;
						// find common string in buf
						const char* bufIt = &(*buf.begin());   // pointer to start of buf
						// TODO use c++ methods
						while ((len2 > 0) && strncasecmp(completions.word[0].c_str(), bufIt+num-len2, len2)) { 
							len2--; 
						}
						new_pos = num - len2;
						// if (new_pos+i+1 < size) {
							for (i = 0; i < len; ++i) { buf[new_pos+i] = completions.word[0][i]; }
							if (1 == completions.num) { buf[new_pos + (i++)] = ' '; }
							crossline_refresh (cLine, prompt, buf, &pos, &num, new_pos+i, new_pos+i, 1);
						//}
					}
#else					
					int commonLen = common.length();
					int start = completions.start;
					int oldLen = completions.end - start;    // length that is being replaced

					// this is a temporary addition
					buf1 = buf.substr(0, start) + common + buf.substr(completions.end);
					if (buf1 != buf) {
						// buf.insert(completions.end, common.substr(oldLen));
						common_add = commonLen;
						pos1 = pos;
						num1 = num;
						crossline_refresh (cLine, prompt, buf1, &pos1, &num1, start+commonLen, num+commonLen-oldLen, 1);
					}
#endif					
				}
			}
			int newNum = num;
			int newPos = pos;
			if (((completions.Size() > 1) || (KEY_TAB != ch)) && crossline_show_completions(cLine, completions)) { 
				std::string inBuf;
				if (crossline_readline_edit(cLine, inBuf, "Input match id: ", false, false, true)) {
					if (inBuf.length() == 0) {
						printf(" \b\n");
						break;
					}
					char c = inBuf[0];
					if (c < '1' || c > '9') {
						printf(" \b\n");
						break;
					}
					int ind = c - '1';   // id is 1-based
					// int ind = atoi(inBuf.c_str()) - 1;   // id is 1-based
					if (ind < completions.Size()) {
						auto cmp = completions.Get(ind);
						std::string newBuf = cmp->word;
						if (cmp->needQuotes) {
							newBuf = "\"" + newBuf + "\"";
						}
						// pos and num have been updated with common_add
						int len3 = newBuf.length();
						// int start = completions.start + common_add;   // after adding common, move 1 to beyond common

						int oldLen = completions.end - completions.start;
						newPos = pos + len3 - oldLen;
						newNum = num + len3 - oldLen;

						// buf.insert(start, newBuf.substr(common_add));
						int start = completions.start;
						// buf.insert(start, newBuf);
						buf = buf.substr(0, start) + newBuf + buf.substr(pos);
						// crossline_print (cLine, prompt, buf, &pos, &num, newPos, newNum); 
						// break;
					}
				}
				crossline_print(cLine, prompt, buf, &pos, &num, newPos, newNum); 
			} else {
				pos = pos1;
				num = num1;
				buf = buf1;
			}
			break;
		}

/* History Commands */
		case KEY_UP:		// Fetch previous line in history.
			if (crossline_updown_move (cLine, prompt, &pos, &num, -1, 0)) { break; } // check multi line move up
		case CTRL_KEY('P'):
			if (in_his || !has_his) { 
				break; 
			}
			if (!copy_buf) { 
				crossline_text_copy (input, buf, 0, num); copy_buf = 1; 
			}
			if ((history_id >= 0))	{ 
				crossline_history_copy (cLine, prompt, buf, &pos, &num, history_id--); 
			} else if (history_id == 0) {
				history_id = cLine.HistoryCount() - 1;
				if (history_id >= 0) {
					crossline_history_copy (cLine, prompt, buf, &pos, &num, history_id--); 
				}
			}
			break;

		case KEY_DOWN:		// Fetch next line in history.
			if (crossline_updown_move (cLine, prompt, &pos, &num, 1, 0)) { break; } // check multi line move down
		case CTRL_KEY('N'):
			if (in_his || !has_his) { 
				break; 
			}
			if (!copy_buf) { 
				crossline_text_copy (input, buf, 0, num); 
				copy_buf = 1; 
			}
			if (history_id+1 < cLine.HistoryCount()-1) { 
				crossline_history_copy (cLine, prompt, buf, &pos, &num, history_id++); 
			} else {
				// cycle back
				history_id = 0;
				buf = input;
				// strncpy (buf, input, size - 1);
				// buf[size - 1] = '\0';
				int bufLen = buf.length();
				crossline_refresh (cLine, prompt, buf, &pos, &num, bufLen, bufLen, 1);
			}
			break; //case UP/DOWN

		case ALT_KEY('<'):	// Move to first line in history.
		case KEY_PGUP:
			if (in_his || !has_his) { 
				break; 
			}
			if (!copy_buf)
				{ crossline_text_copy (input, buf, 0, num); copy_buf = 1; }
			if (cLine.HistoryCount() > 0) {
				history_id = 0;
				crossline_history_copy (cLine, prompt, buf, &pos, &num, history_id);
			}
			break;

		case ALT_KEY('>'):	// Move to end of input history.
		case KEY_PGDN: {
			if (in_his || !has_his) { 
				break; 
			}
			if (!copy_buf)
				{ crossline_text_copy (input, buf, 0, num); copy_buf = 1; }
			history_id = cLine.HistoryCount();
			buf = input;
			// strncpy (buf, input, size-1);
			// buf[size-1] = '\0';
			int bufLen = buf.length();
			crossline_refresh (cLine, prompt, buf, &pos, &num, bufLen, bufLen, 1);
			break;
		}
		case CTRL_KEY('R'):	// Search history
		case CTRL_KEY('S'):
		case KEY_F4: {		// Search history with current input.
			if (in_his || !has_his) { 
				break; 
			}
			crossline_text_copy (input, buf, 0, num);
			std::string search = buf;   // (KEY_F4 == ch) ? buf : "";
			search_his = cLine.crossline_history_search (search);
			if (search_his > 0)	{ 
				buf = cLine.GetHistoryItem(search_his, false)->item;
			} else { 
				buf = input;
				// strncpy (buf, input, size-1); 
			}
			// buf[size-1] = '\0';
			int bufLen = buf.length();
			crossline_print (cLine, prompt, buf, &pos, &num, bufLen, bufLen);
			break;
		}
		case KEY_F2:	// Show history
			if (in_his || !has_his || (0 == cLine.HistoryCount())) { 
				break; 
			}
			printf (" \b\n");
			cLine.crossline_history_show ();
			crossline_print (cLine, prompt, buf, &pos, &num, pos, num);
			break;

		case KEY_F3:	// Clear history
			if (in_his || !has_his) { 
				break; 
			}
			printf(" \b\n!!! Confirm to clear history [y]: ");
			if ('y' == cLine.crossline_getch()) {
				printf(" \b\nHistory are cleared!");
				cLine.HistoryClear ();
				history_id = 0;
			}
			printf (" \b\n");
			crossline_print (cLine, prompt, buf, &pos, &num, pos, num);
			break;

/* Control Commands */
		case KEY_ENTER:		// Accept line (same with CTRL_KEY('M'))
		case KEY_ENTER2:	// same with CTRL_KEY('J')
			crossline_refresh (cLine, prompt, buf, &pos, &num, num, num, 0);
			printf (" \b\n");
			read_end = 1;
			break;

		case CTRL_KEY('C'):	// Abort line.
		case CTRL_KEY('G'):
			crossline_refresh (cLine, prompt, buf, &pos, &num, num, num, 0);
			if (CTRL_KEY('C') == ch)	{ printf (" \b^C\n"); }
			else	{ printf (" \b\n"); }
			num = pos = 0;
			errno = EAGAIN;
			read_end = -1;
			break;;

		case CTRL_KEY('Z'):
#ifndef _WIN32
			raise(SIGSTOP);    // Suspend current process
			crossline_print (cLine, prompt, buf, &pos, &num, pos, num);
#endif
			break;

		default:
			if (!is_esc && isprint(ch)) {  // && (num < size-1)) {
				buf.insert(pos, 1, ch);
				// memmove (&buf[pos+1], &buf[pos], num - pos);
				// buf[pos] = (char)ch;
				if (prompt.length() + pos + 1 > cols) {
					crossline_refresh (cLine, prompt, buf, &pos, &num, pos+1, num+1, 1);
				} else {
					crossline_refresh (cLine, prompt, buf, &pos, &num, pos+1, num+1, pos+1);
				}
				copy_buf = 0;
			} else if (is_esc) {
				printf (" \b\n");
				num = pos = 0;
				errno = EAGAIN;
				read_end = -1;
			}
			break;
        } // switch( ch )
	 	fflush(stdout);

	 	if (a_single and num > 0) {
			printf (" \b\n");
			read_end = 1;
		}

	 	if (history_id < 0 && cLine.HistoryCount() > 0) {
	 		history_id = cLine.HistoryCount() - 1;
	 	}
	} while ( !read_end );

	if (read_end < 0) { 
		return false; 
	}

#ifdef TODO
	if ((num > 0) && (' ' == buf[num-1])) { 
		num--; 
	}
	buf[num] = '\0';

	if (!in_his && (num > 0) && strcmp(buf.c_str(), "history")) { // Save history

		if ((0 == cLine.info->s_history_id) 
			|| buf.find(cLine.info->s_history_buf[(cLine.info->s_history_id-1)%CROSS_HISTORY_MAX_LINE], 0) == buf.npos) {
			// || strncmp (buf.c_str(), cLine.info->s_history_buf[(cLine.info->s_history_id-1)%CROSS_HISTORY_MAX_LINE], CROSS_HISTORY_BUF_LEN)) {
			cLine.info->s_history_buf.push_back(buf);

			// strncpy (cLine.info->s_history_buf[cLine.info->s_history_id % CROSS_HISTORY_MAX_LINE], buf, CROSS_HISTORY_BUF_LEN);
			// cLine.info->s_history_buf[s_history_id % CROSS_HISTORY_MAX_LINE][CROSS_HISTORY_BUF_LEN - 1] = '\0';

			history_id = ++cLine.info->s_history_id;
			copy_buf = 0;
		}
	}
#else
	// add to history
	if (!a_single && !in_his && (num > 0)) {
		// check if already stored
		bool add = true;
		int hisNo = cLine.HistoryCount();
		if (hisNo > 0) {
			HistoryItemPtr it = cLine.GetHistoryItem(hisNo-1, false);
			if (buf == it->item) {
				add = false;
			}
		}
		if (add) {
			cLine.HistoryAdd(buf);
		}
	}
#endif	

	return buf.length() > 0;
}


static void completion_hook (const std::string &buf, Crossline &cLine, const int pos, CrosslineCompletions &pCompletion)
{

    pCompletion.Clear();
    if (!cLine.Completer(buf, pos, pCompletion)) {
        return;
    }

    // for (unsigned int i = 0; i < comps.size(); i++) {
    //     std::string &f = comps[i].comp;
    //     int start = comps[i].start;
    //     cLine.crossline_completion_add (pCompletion, f, "", start, pos);
    // }
}


CrosslineCompletions::CrosslineCompletions()
{
	start = 0;
}

CrosslinePrivate::CrosslinePrivate()
{
	s_completion_callback = nullptr;
	s_paging_print_line = 0;
	s_prompt_color = CROSSLINE_COLOR_DEFAULT;
	s_history_noSearchRepeats = false;
}


// Crossline class
Crossline::Crossline()
{
	info = new CrosslinePrivate();
    crossline_completion_register (completion_hook);
}

void Crossline::HistoryDelete(const ssize_t ind, const ssize_t n)
{
}

void Crossline::Printf(const std::string &fmt, const std::string st)
{
	printf(fmt.c_str(), st.c_str());
}

void Crossline::Print(const std::string &msg)
{
	printf(msg.c_str());
}

std::string Crossline::ReadLine(const std::string &prompt)
{
	std::string buf;
    crossline_readline (prompt, buf);
    return buf;
}


void CrosslineCompletions::Clear() 
{
	comps.clear();
	start = 0;
	end = 0;
}

void CrosslineCompletions::Setup(const int startIn, const int endIn)
{
	start = startIn;
	end = endIn;
}

void CrosslineCompletions::Add(const std::string &word, const std::string &help, const bool needQuotes, crossline_color_e wcolor, 
							   crossline_color_e hcolor) 
{
	comps.push_back(std::make_shared<CompletionInfo>(word, help, wcolor, hcolor, needQuotes));
}

std::string CrosslineCompletions::FindCommon() {
	std::string common = comps[0]->word;
	int len = common.length();
	for (int i = 1; (i < comps.size()) && (len > 0); ++i) {
		std::string word = comps[i]->word.substr(0, len);
		if (word.length() < len) {
			len = word.length();
		}
		while (len > 0 && common.find(word) == common.npos) {
			len--;
			word.pop_back();
		}
		if (len > 0) {  // there should be at least 1 common character
			common = common.substr(0, len);
		}
	}
	return common;
}

std::string CrosslineCompletions::GetComp(const int i) const
{
	return Get(i)->word;
}
