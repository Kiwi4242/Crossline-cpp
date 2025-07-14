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
 * Copyright (c) 2024, John Burnell
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

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <cctype>
#include <stdint.h>

#include <iostream>
#include <iomanip>
#include <fstream>
#include <set>

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

#define LOGMESSAGE 1

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

// class for handling low level functions
class TerminalClass {
public:
#ifdef _WIN32
	HANDLE hConsole;
#endif

protected:
	static const int BufLen = 32;
	int curBuf;
	int buffer[BufLen];
	int RawGetChar();

public:
	TerminalClass();
	void Print(const std::string &st);
	template <typename... Args>	void Printf(const char* fmt, Args&&... args);
	int GetChar();
	void PutChar(const int c);
	void ShowCursor(const bool show);
	void Beep();
};

// a class for storing private variables
struct CrosslinePrivate {

	TerminalClass term;
	// history items
 	std::vector<HistoryItemPtr> 	history_items;
	bool history_noSearchRepeats;
	int history_search_no;  // the number of history items to show

	std::string word_delimiter;
    bool got_resize; // Window size changed

	std::string	clip_buf; // Buf to store cut text
	crossline_completion_callback completion_callback = nullptr;

	int	paging_print_line = 0; // For paging control
	crossline_color_e prompt_color = CROSSLINE_COLOR_DEFAULT;

	bool allowEscCombo;

	std::string logFile;

	int last_print_num;   // store the size of the last printed string

	CrosslinePrivate(const bool log);

	void LogMessage(const std::string &st);
	bool IsLogging() const;

};


bool Crossline::isdelim(const char ch) 
{
	// Check ch is word delimiter	
	return NULL != strchr(info->word_delimiter.c_str(), ch);
}


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
bool Crossline::ReadLine (const std::string &prompt, std::string &buf, const bool useBuf)
{
	int not_support = 0;

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
        buf = tmpBuf;
        return true;
	}

	return ReadlineEdit (buf, prompt, useBuf, false, StrVec());
}




// Set move/cut word delimiter, defaut is all not digital and alphabetic characters.
void  Crossline::SetDelimiter (const std::string &delim)
{
	if (delim.length() > 0) {
		info->word_delimiter = delim;
	}
}

void Crossline::HistoryShow (void)
{
	int noHist = HistoryCount();
	for (int i = 0; i < noHist; i++) {
		HistoryItemPtr hisPtr = GetHistoryItem(i);
		const std::string &hisItem = hisPtr->item;
		PrintStr(hisItem);
	}
}

void  Crossline::HistoryClear (void)
{
	info->history_items.clear();
	// info->s_history_id = 0;
}

int Crossline::HistorySave (const std::string &filename)
{
	if (filename.length() == 0) {
		return -1;
	} else {
		FILE *file = fopen(filename.c_str(), "wt");
		if (file == NULL) {	return -1;	}

		int noHist = HistoryCount();
		for (int i = 0; i < noHist; i++) {
			HistoryItemPtr hisPtr = GetHistoryItem(i);
			const std::string &hisItem = hisPtr->item;
			fprintf (file, "%s\n", hisItem.c_str());
		}
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
	info->history_items.push_back(item);
}

void Crossline::HistoryAdd(const std::string &item) 
{
	HistoryItemPtr ptr = std::make_shared<HistoryItemBase>(item);
	HistoryAdd(ptr);
}

HistoryItemPtr Crossline::GetHistoryItem(const ssize_t n)
{
	int ind = n;	
	// if (!forward) {
	// 	ind = info->s_history_items.size() - 1 - n;
	// }
	return info->history_items[ind];
}

int Crossline::HistoryCount()
{
	return info->history_items.size();
}

void Crossline::HistorySetup(const bool noSearchRepeats)
{
	info->history_noSearchRepeats = noSearchRepeats;
}

void Crossline::HistorySetSearchMaxCount(const ssize_t n)
{
	info->history_search_no = n;
}

// Register completion callback.
void Crossline::CompletionRegister (crossline_completion_callback pCbFunc)
{
	info->completion_callback = pCbFunc;
}

// Add completion in callback. Word is must, help for word is optional.
void  Crossline::CompletionAddColor (CrosslineCompletions &pCompletions, const std::string &word, 
	  								 crossline_color_e wcolor, const std::string &help, crossline_color_e hcolor)
{
	if ((word.length() > 0)) {
		pCompletions.Add(word, help, false, wcolor, hcolor);
	}
}

void Crossline::CompletionAdd (CrosslineCompletions &pCompletions, const std::string &word, const std::string &help)
{
	CompletionAddColor (pCompletions, word, CROSSLINE_COLOR_DEFAULT, help, CROSSLINE_COLOR_DEFAULT);
}

// Set syntax hints in callback.
void  Crossline::HintsSetColor (CrosslineCompletions &pCompletions, const std::string &hints, crossline_color_e color)
{
	if ((hints.length() > 0)) {
		pCompletions.hint = hints;
		// pCompletions.hints[CROSS_COMPLET_HINT_LEN - 1] = '\0';
		pCompletions.color_hint = color;
	}
}

void Crossline::HintsSet (CrosslineCompletions &pCompletions, const std::string &hints)
{
	HintsSetColor (pCompletions, hints, CROSSLINE_COLOR_DEFAULT);
}

/*****************************************************************************/

int Crossline::PagingSet (int enable)
{
	int prev = info->paging_print_line >=0;
	info->paging_print_line = enable ? 0 : -1;
	return prev;
}

int Crossline::PagingCheck (int line_len)
{
	std::string paging_hints("*** Press <Space> or <Enter> to continue . . .");
	int	i, ch, rows, cols, len = paging_hints.length();

	if ((info->paging_print_line < 0) || !isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO))	{
		return 0; 
	}
	ScreenGet (rows, cols);
	info->paging_print_line += (line_len + cols - 1) / cols;
	if (info->paging_print_line >= (rows - 1)) {
		PrintStr(paging_hints);
		ch = Getch();
		if (0 == ch) { Getch(); }	// some terminal server may send 0 after Enter
		// clear paging hints
		for (i = 0; i < len; ++i) { PrintStr("\b"); }
		for (i = 0; i < len; ++i) { PrintStr(" ");  }
		for (i = 0; i < len; ++i) { PrintStr("\b"); }
		info->paging_print_line = 0;
		if ((' ' != ch) && (KEY_ENTER != ch) && (KEY_ENTER2 != ch)) {
			return 1; 
		}
	}
	return 0;
}

/*****************************************************************************/

void  Crossline::PromptColorSet (crossline_color_e color)
{
	info->prompt_color	= color;
}

void Crossline::ScreenClear ()
{
	int ret = system (s_crossline_win ? "cls" : "clear");
	(void) ret;
}

void Crossline::PrintStr(const std::string st)
{
	info->term.Print(st);
}

int Crossline::Getch (void)
{
	return info->term.GetChar();
}

void Crossline::ShowCursor(const bool show)
{
	info->term.ShowCursor(show);
}

#ifdef _WIN32	// Windows

int TerminalClass::RawGetChar()
{
	return _getch();
}

template <typename T>
void printAllImpl(T item) {
  std::cout << item << ' ';
}

template <typename T, typename ...Args>
void printAllImpl(T item, Args ... args) {
  printAllImpl(item);
  printAllImpl(args...);
}

#ifdef TODO
template <typename... Args>
void TerminalClass::Print(const char* fmt, Args&&... args) {
  printAllImpl(std::forward<Args>(args) ...);
  std::cout << '\n';
}
#endif

// template <typename... Args>
// void TerminalClass::Print(const char* fmt, Args&&... args)
// {
//   va_list ap;
//   va_start(ap, fmt);
//   term_vwritef(term,fmt,ap);
//   va_end(ap);  
// }


void TerminalClass::Print(const std::string &st)
{
	// printf(st.c_str());
	WriteConsole(hConsole, st.c_str(), st.size(), nullptr, nullptr);
}

#if 0
int Crossline::crossline_getch (void)
{
	fflush (stdout);
	int ch = _getch();
	return ch;
}
#endif

void TerminalClass::ShowCursor(const bool show)
{
  CONSOLE_CURSOR_INFO curInfo;
  if (!GetConsoleCursorInfo(hConsole, &curInfo)) {
  	return;
  }
  curInfo.bVisible = show;
  SetConsoleCursorInfo(hConsole, &curInfo);
}


void Crossline::ScreenGet (int &pRows, int &pCols)
{
	CONSOLE_SCREEN_BUFFER_INFO inf;
	GetConsoleScreenBufferInfo (info->term.hConsole, &inf);
	pCols = inf.srWindow.Right - inf.srWindow.Left + 1;
	pRows = inf.srWindow.Bottom - inf.srWindow.Top + 1;
	pCols = pCols > 1 ? pCols : 160;
	pRows = pRows > 1 ? pRows : 24;
}

bool Crossline::CursorGet (int &pRow, int &pCol)
{
	CONSOLE_SCREEN_BUFFER_INFO inf;
	GetConsoleScreenBufferInfo (info->term.hConsole, &inf);
	pRow = inf.dwCursorPosition.Y - inf.srWindow.Top;
	pCol = inf.dwCursorPosition.X - inf.srWindow.Left;
	return true;
}

void Crossline::CursorSet (const int row, const int col)
{
	CONSOLE_SCREEN_BUFFER_INFO inf;
	GetConsoleScreenBufferInfo (info->term.hConsole, &inf);
	inf.dwCursorPosition.Y = (SHORT)row + inf.srWindow.Top;	
	inf.dwCursorPosition.X = (SHORT)col + inf.srWindow.Left;
	SetConsoleCursorPosition (info->term.hConsole, inf.dwCursorPosition);
}

void Crossline::CursorMove (const int row_off, const int col_off)
{
	CONSOLE_SCREEN_BUFFER_INFO inf;
	GetConsoleScreenBufferInfo (info->term.hConsole, &inf);
	inf.dwCursorPosition.Y += (SHORT)row_off;
	inf.dwCursorPosition.X += (SHORT)col_off;
	SetConsoleCursorPosition (info->term.hConsole, inf.dwCursorPosition);
}

// void Crossline::CursorHide (const bool bHide)
// {
// 	CONSOLE_CURSOR_INFO inf;
// 	GetConsoleCursorInfo (GetStdHandle(STD_OUTPUT_HANDLE), &inf);
// 	inf.bVisible = !bHide;
// 	SetConsoleCursorInfo (GetStdHandle(STD_OUTPUT_HANDLE), &inf);
// }

void Crossline::ColorSet (crossline_color_e color)
{
    CONSOLE_SCREEN_BUFFER_INFO scrInfo;
	static WORD dft_wAttributes = 0;
	WORD wAttributes = 0;
	if (!dft_wAttributes) {
		GetConsoleScreenBufferInfo(info->term.hConsole, &scrInfo);
		dft_wAttributes = scrInfo.wAttributes;
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
			// case CROSSLINE_FGCOLOR_BLUEGREEN:	wAttributes |= 11;	break;    // fixt this later
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
	SetConsoleTextAttribute(info->term.hConsole, wAttributes);
}


#else // Linux

int TerminalClass::RawGetChar()
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

void TerminalClass::Print(const std::string &st)
{
	printf("%s", st.c_str());
}

void TerminalClass::ShowCursor(const bool show)
{
 	printf("\e[?25%c", show?'h':'l');
}

void Crossline::ScreenGet (int &pRows, int &pCols)
{
	struct winsize ws = {};
	(void)ioctl (1, TIOCGWINSZ, &ws);
	pCols = ws.ws_col;
	pRows = ws.ws_row;
	pCols = pCols > 1 ? pCols : 160;
	pRows = pRows > 1 ? pRows : 24;
}

bool Crossline::CursorGet (int &pRow, int &pCol)
{
	int i;
	char buf[32];
	PrintStr("\e[6n");
	for (i = 0; i < (char)sizeof(buf)-1; ++i) {
		buf[i] = (char)Getch ();
		if ('R' == buf[i]) { break; }
	}
	buf[i] = '\0';
	if (2 != sscanf (buf, "\e[%d;%dR", &pRow, &pCol)) { return false; }
	pRow--; pCol--;
	return true;
}

void Crossline::CursorSet (int row, int col)
{
	printf("\e[%d;%dH", row+1, col+1);
}

void Crossline::CursorMove (int row_off, int col_off)
{
	if (col_off > 0)		{ printf ("\e[%dC", col_off);  }
	else if (col_off < 0)	{ printf ("\e[%dD", -col_off); }
	if (row_off > 0)		{ printf ("\e[%dB", row_off);  }
	else if (row_off < 0)	{ printf ("\e[%dA", -row_off); }
}

// void Crossline::CursorHide (const bool bHide)
// {
// 	printf("\e[?25%c", bHide?'l':'h');
// }

void Crossline::ColorSet (crossline_color_e color)
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
 	cLine.PrintStr(" \b\n");
	for (i = 0; help[i].length()>0; ++i) {
		cLine.PrintStr(help[i] + "\n");
		if (cLine.PagingCheck (help[i].length()+1))
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

std::vector<char> MakeIndexKeys()
{
	const int maxKeys = 9 + 2*26;
	std::vector<char> keys(maxKeys, ' ');

	char noKeys[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
	for (int i = 0; i < 9; i++) {
		keys[i] = noKeys[i];
	}
	int k = 9;
	for (char c = 'a'; c <= 'z'; c++) {
		keys[k++] = c;
	}
	for (char c = 'A'; c <= 'Z'; c++) {
		keys[k++] = c;
	}

	return keys;
}


// If patterns is not NULL, will filter history.
// matches gives a list matching the dump id with the index in history
// update to assume that patterns is a string to be matched in the history
// if forward then move from 0 to no
int Crossline::HistoryDump (const bool print_id, std::string patterns,
							std::map<std::string, int> &matches,
							const int maxNo, const bool forward)
{
	int	id = 0, num=0;
	std::vector<std::string> pat_list;
	matches.clear();

	bool noRepeats = info->history_noSearchRepeats;
	std::map<std::string, int> matched;
	std::vector<std::string> patMatches;

	std::vector<char> keys = MakeIndexKeys();
	int maxKeys = keys.size();

	// first get up to maxKeys matches
	int noHist = HistoryCount();
	int count = 0;
	for (int i = 0; i < noHist; i++) {
		// std::string &history = cLine.info->s_history_items[i];
		int ind = i;
		if (!forward) {
			ind = noHist - 1 - i;
		}
		HistoryItemPtr hisPtr = GetHistoryItem(ind);
		const std::string &history = hisPtr->item;
		if (history.length() > 0) {
			if ((patterns.length() > 0) && history.find(patterns) == history.npos) {
				continue;
			}
			// avoid repeats
			if (noRepeats && matched.count(history)) {
				continue;
			}
			matched[history] = ind;
			patMatches.push_back(history);
			if (++count > maxKeys) {
				break;
			}
		}
	}

	// num = crossline_split_patterns (patterns, pat_list);
	int noShow = count;
	if (maxNo > 0 and noShow > maxNo) {
		noShow = maxNo;
	}

	for (int i = 0; i < noShow; i++) {
		std::string &hisSt = patMatches[i];
		if (print_id) {

		    std::ostringstream msg;
		    char k = keys[i];
      		msg << std::setw(4) << k << " ";
			PrintStr(msg.str());
      		ColorSet(CROSSLINE_FGCOLOR_BLACK | CROSSLINE_FGCOLOR_BRIGHT);
			PrintStr(hisSt + "\n");
      		ColorSet(CROSSLINE_FGCOLOR_DEFAULT);

			msg.str(std::string()); // clear
			msg << k;
			matches[msg.str()] = matched[hisSt];
		} else {
			PrintStr(hisSt + "\n");
		}
	}

#ifdef OLD
	// search through history starting at most recent
	int no = 0;
	for (int i = 0; i < noHist; i++) {
		// std::string &history = cLine.info->s_history_items[i];
		int ind = i;
		if (!forward) {
			ind = noHist - 1 matches[hisSt] i;
		}
		HistoryItemPtr hisPtr = GetHistoryItem(ind);
		const std::string &history = hisPtr->item;
		if (history.length() > 0) {
			// if ((patterns.length() > 0) && !crossline_match_patterns (history, pat_list, num)) { 
			if ((patterns.length() > 0) && history.find(patterns) == history.npos) { 
				continue; 
			}
			// avoid repeats
			if (noRepeats && matched.count(history)) {
				continue;
			}
			matched[history] = i;
			if (useFile) {
				if (print_id) {
				    std::ostringstream msg;
			  		msg << std::setw(4) << keys[id];
					fprintf (file, "%s:  %s\n", msg.str().c_str(), history.c_str());

					msg.str(std::string()); // clear
					msg << keys[id++];
					matches[msg.str()] = ind;
				} else { 
					fprintf (file, "%s\n", history.c_str()); 
				}
			} else {
				if (print_id) {

				    std::ostringstream msg;
				    char k = keys[id++];
		      		msg << std::setw(4) << k << " ";
					PrintStr(msg.str()); 
		      		ColorSet(CROSSLINE_FGCOLOR_BLACK | CROSSLINE_FGCOLOR_BRIGHT);
					PrintStr(history + "\n");
		      		ColorSet(CROSSLINE_FGCOLOR_DEFAULT);

					msg.str(std::string()); // clear
					msg << k;
					matches[msg.str()] = ind;
				} else { 
					PrintStr(history + "\n"); 
				}

			}
			if (++no > noShow) {
				break;
			}
			if (paging) {
				if (PagingCheck (history.length()+(print_id?7:1)))	{ 
					break; 
				}
			}
		}
	}
	return no;
#endif
	return noShow;
}


// Search history, input will be initial search patterns.
std::pair<int, std::string> Crossline::HistorySearch (std::string &input)
{ 
	uint32_t count;
	std::string buf = "1";
	std::string pattern;

	PrintStr(" \b\n");
	if (input.length() > 0) {
		pattern = input;
	} else {
		// Get search patterns
		if (!ReadlineEdit(pattern, "History Search: ", (input.length()>0), true, StrVec(), true)) {
			return {-1, ""}; 
		}
	}
	//info->clip_buf = pattern;

	std::map<std::string, int> matches;
	bool forward = false;
	int noSearch = info->history_search_no;
	count = HistoryDump(true, pattern, matches, noSearch, forward);
	if (0 == count)	{
		return {-1, ""}; 
	} // Nothing found, just return

	if (count > 1) {
		// Get choice
		StrVec choices;
		for(auto const & pair : matches) {
			choices.push_back(pair.first);
		}
		if (!ReadlineEdit (buf, "Input history id: ", (1==count), true, choices, true)) {
			return {-1, ""}; 
		}
		if (matches.count(buf) == 0) {
			return {-2, buf}; 
		}

	} else {
		int id = matches.begin()->second;
		return {id, GetHistoryItem(id)->item};
	}

	int his_id = matches[buf];
	return {his_id, GetHistoryItem(his_id)->item};

}


// Show completions returned by callback.
int Crossline::ShowCompletions (CrosslineCompletions &pCompletions, std::map<std::string, int> &matches)
{
	int ret = 0, word_len = 0, with_help = 0;

	if ((pCompletions.hint.length()) || (pCompletions.Size() > 0)) {
		PrintStr(" \b\n");
		ret = 1;
	}
	// Print syntax hints.
	if (pCompletions.hint.length()) {
		PrintStr("Please input: "); 
		ColorSet (pCompletions.color_hint);
		PrintStr(pCompletions.hint); 
		ColorSet (CROSSLINE_COLOR_DEFAULT);
		PrintStr("\n");
	}
	if (0 == pCompletions.Size())	{ 
		return ret; 
	}
	for (int i = 0; i < pCompletions.Size(); ++i) {
		CompletionInfoPtr comp = pCompletions.Get(i);
		if (comp->word.length() > word_len) { 
			word_len = comp->word.length(); 
		}
		if (comp->help.length() > 0)	{ 
			with_help = 1; 
		}
	}

	std::vector<char> keys = MakeIndexKeys();
	int maxKeys = keys.size();

	// limit the number shown to the number of keys - should remove this restriction, but 36 entries should be enough for now
	int no = pCompletions.Size();;
	if (no > maxKeys) {
		no = maxKeys;
	}

	if (with_help) {
		// Print words with help format.
		for (int i = 0; i < no; ++i) {
			CompletionInfoPtr comp = pCompletions.Get(i);
			ColorSet (comp->color_word);

		    std::ostringstream msg;
      		msg << std::setw(4) << keys[i] << ":  " << comp->word;

			PrintStr(msg.str());
			int l = 4 + word_len - comp->word.length();
			if (l > 0) {
				std::string spaces(l, ' ');
				PrintStr(spaces); 
			}
			ColorSet (comp->color_help);
			PrintStr(comp->help);
			ColorSet (CROSSLINE_COLOR_DEFAULT);
			PrintStr("\n");
			if (PagingCheck(comp->help.length()+4+word_len+1))
				{ break; }

			msg.str(std::string()); // clear
			msg << keys[i];
			matches[msg.str()] = i;
		}
		return ret;
	}

	// Print words list in multiple columns.
	int rows, cols;
	ScreenGet (rows, cols);

	int extra = 4;                                               // in each column have "____: word_len____"
	int word_num = std::min(cols / (word_len + 6 + extra), 3);   // 6 = "____: ""

	for (int i = 0; i < no; ++i) {
		CompletionInfoPtr comp = pCompletions.Get(i);
		ColorSet (comp->color_word);
	    std::ostringstream msg;

  		msg << std::setw(4) << keys[i] << ":  ";
		PrintStr(msg.str());
  		ColorSet(CROSSLINE_FGCOLOR_BLACK | CROSSLINE_FGCOLOR_BRIGHT);
		PrintStr(comp->word);
		ColorSet (CROSSLINE_COLOR_DEFAULT);

		msg.str(std::string()); // clear
		msg << keys[i];
		matches[msg.str()] = i;

		int l = word_len - comp->word.length();
		if ((i+1) % word_num > 0) {
			l += extra;
		}
		if (l > 0) {
			std::string spaces(l, ' ');
			PrintStr(spaces); 
		}

		if (0 == ((i+1) % word_num)) {
			PrintStr("\n");
			if (PagingCheck (word_len)) { 
				return ret; 
			}
		}
	}

	if (pCompletions.Size() % word_num) { PrintStr("\n"); }
	return ret;
}

bool Crossline::UpdownMove (const std::string &prompt, int &pCurPos, const int pCurNum, const int off, 
						     const bool bForce)
{
	int rows, cols, len = prompt.length();
	int cur_pos=pCurPos;
	ScreenGet (rows, cols);
	if (!bForce && (pCurPos == pCurNum)) { 
		return false; 
	} // at end of last line
	if (off < 0) {
		if ((pCurPos+len)/cols == 0) { 
			return false; 
		} // at first line
		pCurPos -= cols;
		if (pCurPos < 0) { 
			pCurPos = 0; 
		}
		CursorMove (-1, (pCurPos+len)%cols-(cur_pos+len)%cols);
	} else {
		if ((pCurPos+len)/cols == (pCurNum+len)/cols) { 
			return false; 
		} // at last line
		pCurPos += cols;
		if (pCurPos > pCurNum) { 
			pCurPos = pCurNum - 1; 
		} // one char left to avoid history shortcut
		CursorMove (1, (pCurPos+len)%cols-(cur_pos+len)%cols);
	}
	return true;
}

// refresh current print line and move cursor to new_pos.
// bChg > 1 then refresh from bChg-1 in buf
// cursor pos is one past the position
void Crossline::Refresh(const std::string &prompt, std::string &buf, int &pCurPos, int &pCurNum, 
						 const int new_pos, const int new_num, const int bChg)
{
	int prLen = prompt.length();
	static int rows = 0, cols = 0;

	if (bChg || !rows || s_crossline_win) { 
		ScreenGet (rows, cols); 
	}

#ifdef LOGMESSAGE
	info->LogMessage("Writing " + buf);
#endif	

	if (!bChg) { // just move cursor
		int pos_row = (new_pos+prLen)/(cols) - (pCurPos+prLen)/(cols);
		int pos_col = (new_pos+prLen)%(cols) - (pCurPos+prLen)%(cols);
		CursorMove (pos_row, pos_col);
#ifdef LOGMESSAGE
		if (info->IsLogging()) {
		    std::ostringstream msg;
		    msg << "\nbChg 0. Cursor move (row, col) " << pos_row << " " << pos_col << "\n";
		    // msg << "\nbChg 0. Cursor move (row, col) " << theRow << " " << theCol << "\n";

			int origCurRow, origCurCol;
			CursorGet(origCurRow, origCurCol);
		    msg << "1. Cursor pos (row, col) " << origCurRow << " " << origCurCol << "\n";
		    info->LogMessage(msg.str());
		}
#endif	    
	} else {
		ShowCursor(false);
		// build a single string to write - try to reduce flicker.
		// start at current cursor position - if writing the whole buffer
		// we need to move the cursor to the start of the line
		std::string writeBuf;
		int origCurRow, origCurCol;
		CursorGet(origCurRow, origCurCol);

	    std::ostringstream msg;
#ifdef LOGMESSAGE
	    msg << "0. Cursor pos (row, col) " << origCurRow << " " << origCurCol << "\n";
#endif

		if (new_num > 0) {
			if (new_num < buf.length()) {
				buf = buf.substr(0, new_num);
			}
		} else {
			buf.clear();
		}


		int tmpCurRow, tmpCurCol;
		int writeLen = 0;              // need to store writeLen as some of the chars are \r \n
		if (bChg > 1) { // refresh as less as possbile
			std::string::const_iterator it = buf.begin();
			// cLine.PrintStr(&*(it + bChg - 1));
			writeBuf += buf.substr(bChg - 1);
			writeLen = bChg - 1;    // len to posn

			int writePos = (prLen + bChg - 1) % (cols);   // chg                            // bChg-1 is start of refresh
			int rowOff = (bChg + prLen - 1)/(cols) - (pCurPos + prLen)/(cols);  // chg        // bChg-1 is start of refresh
			// if we are updating from some posn and not writing the whole line, then move cursor
			CursorMove (rowOff, -origCurCol+writePos); 
#ifdef LOGMESSAGE
			if (info->IsLogging()) {
				CursorGet(tmpCurRow, tmpCurCol);
			    msg << "1. bChg cursor pos (row, col) " << tmpCurRow << " " << tmpCurCol << "\n";
			}
#endif		    

		} else {
			// if the text fills n lines we need to move the cursor back before writing
			int pos_row = (pCurPos + prLen - 1) / (cols);    // -1 as have length but using pos
			CursorMove (-pos_row, -origCurCol);
#ifdef LOGMESSAGE
			if (info->IsLogging()) {
			    msg << "1. Cursor move (row, col) " << -pos_row << " " << -origCurCol << "\n";

				CursorGet(tmpCurRow, tmpCurCol);
			    msg << "1. Cursor pos (row, col) " << tmpCurRow << " " << tmpCurCol << "\n";
			}
#endif

			origCurCol = 0;

			ColorSet (info->prompt_color);
			// cLine.PrintStr("\r" + prompt);
			PrintStr(prompt);
			// writePos = prompt.length();
			ColorSet (CROSSLINE_COLOR_DEFAULT);
			// cLine.PrintStr(buf);
			writeBuf += buf;

#ifdef LOGMESSAGE
			if (info->IsLogging()) {
				CursorGet(tmpCurRow, tmpCurCol);
			    msg << "2. Cursor pos (row, col) " << tmpCurRow << " " << tmpCurCol << "\n";
			}
#endif		    

		}
		writeLen += writeBuf.length();

#ifdef LOGMESSAGE
		msg << "writeLen " << writeLen << " " << writeBuf.length() << "\n";
#endif
		if (!s_crossline_win && new_num>0 && !((new_num+prLen)%cols)) {
			// cLine.PrintStr("\n"); 
			writeBuf += "\n";
		}
		// need to overwrite any old text
		// JGB should this be i=0
		int cnt = pCurNum - new_num;
		if (cnt > 0) {
			std::string st(cnt, ' ');
			// cLine.PrintStr(st);
			writeBuf += st;
			writeLen += cnt;
		}

		// for (i=*pCurNum-new_num; i > 0; --i) { 
		//for (i=*pCurNum-new_num; i >= 0; --i) { 
		//	PrintStr(" "); 
		//}

		if (!s_crossline_win && pCurNum>new_num && !((pCurNum+prLen)%cols)) {
			// cLine.PrintStr("\n"); 
			writeBuf += "\n";
		}
		// int pos_row = (new_num+len)/cols - (pCurNum+len)/cols;
		// move to start of previous row - this should be done with curor posn not num
#ifdef TODO		
		if (pos_row < 0) { 
			CursorMove (pos_row, 0); 
		} 
#endif		
		// !!!!! why was this "\r" here?		
		// cLine.PrintStr("\r");
		// writeBuf += "\r";

		PrintStr(writeBuf);

#ifdef LOGMESSAGE
		if (info->IsLogging()) {
			CursorGet(tmpCurRow, tmpCurCol);
		    msg << "3. Cursor pos (row, col) " << tmpCurRow << " " << tmpCurCol << "\n";
		}
#endif
		// now the cursor is at the end of the text, move to cursor pos
		int nowRow = (writeLen + prLen - 1) / (cols);     // -1 as have length but using pos
		int posRow = (new_pos + prLen) / (cols);    // chg
		int rMove =  nowRow - posRow;
		if (rMove < 0) {
			rMove = 0;
		}

		// need to find the cursor column
		int curCPos = (writeLen + prLen) % (cols);   // chg      // -1 as need pos not len
		int reqCPos = (new_pos + prLen) % (cols);    // chg     // using posn
		// there seems to be an issue if the column should be zero
		int cMove = reqCPos - curCPos;
		CursorMove (-rMove, cMove); 

#ifdef LOGMESSAGE
		if (info->IsLogging()) {
		    msg << "2. Cursor move (row, col) " << -rMove << " " << cMove << "\n";

			CursorGet(tmpCurRow, tmpCurCol);
		    msg << "3. Cursor pos (row, col) " << tmpCurRow << " " << tmpCurCol << "\n";

		    msg << "bChg, curPos, new_pos " << bChg << " " << pCurPos << " " << new_pos << "\n";
	  		msg << "moveRow, origCol, len: "<< rMove << " " << origCurCol << " " << prLen << "\n";
	  		msg << "nowRow, posRow, writeLen, cols: " << nowRow << " " << posRow << " " << writeLen << " " << cols << "\n";
	  		msg << "cMove, curCPos, reqCPos " << cMove << " " << curCPos << " " << reqCPos << "\n";
			info->LogMessage(msg.str());
		}
#endif
		ShowCursor(true);
	}
	pCurPos = new_pos;
	pCurNum = new_num;
	info->last_print_num = new_num + prLen;
}

void Crossline::RefreshFull(const std::string &prompt, std::string &buf, int &pCurPos, int &pCurNum, int new_pos, int new_num)
{
	pCurPos = pCurNum = 0;
	Refresh (prompt, buf, pCurPos, pCurNum, new_pos, new_num, 1);
}

// Copy part text[cut_beg, cut_end] from src to dest
void Crossline::TextCopy (std::string &dest, const std::string &src, int cut_beg, int cut_end)
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

// Copy from history buffer to buf
void Crossline::HistoryCopy (const std::string &prompt, std::string &buf, int &pos, int &num, int history_id)
{
	HistoryItemPtr it = GetHistoryItem(history_id);
	buf = it->item;;
	Refresh (prompt, buf, pos, num, buf.length(), buf.length(), 1);
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
static int crossline_getkey (Crossline &cLine, bool &is_esc, const bool check_esc)
{
	int ch = cLine.Getch();
	is_esc = KEY_ESC == ch;

	if ((GetKeyState (VK_CONTROL) & 0x8000) && (KEY_DEL2 == ch)) {
		ch = KEY_CTRL_BACKSPACE;
	} else if ((224 == ch) || (0 == ch)) {
		is_esc = true;
		ch = cLine.Getch ();
		ch = (GetKeyState (VK_MENU) & 0x8000) ? ALT_KEY(ch) : ch + (KEY_ESC<<8);
	} else if (check_esc && KEY_ESC == ch) { // Handle ESC+Key
		bool esc;
		ch = crossline_getkey (cLine, esc, check_esc);
		ch = crossline_key_esc2alt (ch);
	} else if (GetKeyState (VK_MENU) & 0x8000 && !(GetKeyState (VK_CONTROL) & 0x8000) ) {
		is_esc = true; 
		ch = ALT_KEY(ch);
	}
	return ch;
}

void crossline_winchg_reg (Crossline &)	{ }

#else // Linux

// Convert escape sequences to internal special function key
static int crossline_get_esckey (Crossline &cLine, int ch)
{
	int ch2;
	if (0 == ch)	{ ch = cLine.Getch (); }
	if ('[' == ch) {
		ch = cLine.Getch ();
		if ((ch>='0') && (ch<='6')) {
			ch2 = cLine.Getch ();
			if ('~' == ch2)	{ ch = ESC_KEY4 (ch, ch2); } // ex. Esc[4~
			else if (';' == ch2) {
				ch2 = cLine.Getch();
				if (('5' != ch2) && ('3' != ch2))
					{ return 0; }
				ch = ESC_KEY6 (ch, ch2, cLine.Getch()); // ex. Esc[1;5B
			}
		} else if ('[' == ch) {
			ch = ESC_KEY4 ('[', cLine.Getch());	// ex. Esc[[A
		} else { ch = ESC_KEY3 (ch); }	// ex. Esc[A
	} else if ('O' == ch) {
		ch = ESC_OKEY (cLine.Getch());	// ex. EscOP
	} else { ch = ALT_KEY (ch); } // ex. Alt+Backspace
	return ch;
}

// Read a KEY from keyboard, is_esc indicats whether it's a function key.
static int crossline_getkey (Crossline &cLine, bool &is_esc, const bool check_esc)
{
	int ch = cLine.Getch();
	is_esc = KEY_ESC == ch;
	if (check_esc && is_esc) {
		ch = cLine.Getch ();
		if (KEY_ESC == ch) { // Handle ESC+Key
			ch = crossline_get_esckey (cLine, 0);
			ch = crossline_key_mapping (ch);
			ch = crossline_key_esc2alt (ch);
		} else { ch = crossline_get_esckey (cLine, ch); }
	}
	return ch;
}

// not ideal, but a temporary solution
static bool *s_got_resize_ptr = nullptr;

static void crossline_winchg_event (int arg)
{ 
	*s_got_resize_ptr = true; 
}

static void crossline_winchg_reg (Crossline &cLine)
{
	s_got_resize_ptr = &cLine.info->got_resize;
	struct sigaction sa;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sa.sa_handler = &crossline_winchg_event;
	sigaction (SIGWINCH, &sa, NULL);
	cLine.info->got_resize = false;
}

#endif // #ifdef _WIN32

/*****************************************************************************/

/* Internal readline from terminal. has_input indicates buf has inital input.
 * edit_only will disable history and complete shortcuts, only allowing line editing
 */
bool Crossline::ReadlineEdit (std::string &buf, const std::string &prompt, const bool has_input, 
							  const bool edit_only, const StrVec &choices, const bool clear)
{
	int		pos = 0, num = 0, read_end = 0;
	bool    is_esc;
	int     new_pos, copy_buf=0;

	// are we moving back through history or searching
	bool canHis = !edit_only;
	int hisSearch = 1;

	uint32_t search_his;
	std::string input;

	int rows, cols;
	ScreenGet (rows, cols);

	bool is_choice = false;
	bool has_his = false;
	int32_t history_id;
    history_id = HistoryCount();
	if (history_id > 0) {
	 	// history_id--;
	 	has_his = true;
	} 

	if (has_input) {
		num = pos = buf.length();
		input = buf;
		// TextCopy (input, buf, pos, num);
	} else { 
		// buf[0] = input[0] = '\0'; 
		buf.clear();
		input.clear();
	}
	// draw the prompt and any text if buf
	RefreshFull(prompt, buf, pos, num, pos, num);
	crossline_winchg_reg (*this);

	do {
		is_esc = 0;
		int ch = crossline_getkey (*this, is_esc, info->allowEscCombo);
		ch = crossline_key_mapping (ch);

		if (info->got_resize) { // Handle window resizing for Linux, Windows can handle it automatically
			new_pos = pos;
			Refresh (prompt, buf, pos, num, 0, num, 0); // goto beginning of line
			PrintStr("\x1b[J"); // clear to end of screen
			Refresh (prompt, buf, pos, num, new_pos, num, 1);
			info->got_resize = false;
		}

		switch (ch) {
/* Misc Commands */
		case KEY_F1:	// Show help
			crossline_show_help (*this, edit_only);
			RefreshFull (prompt, buf, pos, num, pos, num);
			break;

		case KEY_DEBUG:	// Enter keyboard debug mode
			PrintStr(" \b\nEnter keyboard debug mode, <Ctrl-C> to exit debug\n");
			while (CTRL_KEY('C') != (ch=Getch())) { 
				// printf ("%3d 0x%02x (%c)\n", ch, ch, isprint(ch) ? ch : ' '); 
			    std::ostringstream msg;
		  		msg << std::setw(3) << ch << "0x" << std::setw(2) << ch << "x " << (isprint(ch) ? ch : ' ') << "\n";
		  		PrintStr(msg.str());
			}
			RefreshFull (prompt, buf, pos, num, pos, num);
			break;

/* Move Commands */
		case KEY_LEFT:	// Move back a character.
		case CTRL_KEY('B'):
			if (pos > 0)
				{ Refresh (prompt, buf, pos, num, pos-1, num, 0); }
			break;

		case KEY_RIGHT:	// Move forward a character.
		case CTRL_KEY('F'):
			if (pos < num)
				{ Refresh (prompt, buf, pos, num, pos+1, num, 0); }
			break;

		case ALT_KEY('b'):	// Move back a word.
		case ALT_KEY('B'):
		case KEY_CTRL_LEFT:
		case KEY_ALT_LEFT:
			for (new_pos=pos-1; (new_pos > 0) && isdelim(buf[new_pos]); --new_pos)	;
			for (; (new_pos > 0) && !isdelim(buf[new_pos]); --new_pos)	;
			Refresh (prompt, buf, pos, num, new_pos?new_pos+1:new_pos, num, 0);
			break;

		case ALT_KEY('f'):	 // Move forward a word.
		case ALT_KEY('F'):
		case KEY_CTRL_RIGHT:
		case KEY_ALT_RIGHT:
			for (new_pos=pos; (new_pos < num) && isdelim(buf[new_pos]); ++new_pos)	;
			for (; (new_pos < num) && !isdelim(buf[new_pos]); ++new_pos)	;
			Refresh (prompt, buf, pos, num, new_pos, num, 0);
			break;

		case CTRL_KEY('A'):	// Move cursor to start of line.
		case KEY_HOME:
			Refresh (prompt, buf, pos, num, 0, num, 0);
			break;

		case CTRL_KEY('E'):	// Move cursor to end of line
		case KEY_END:
			Refresh (prompt, buf, pos, num, num, num, 0);
			break;

		case CTRL_KEY('L'):	// Clear screen and redisplay line
			ScreenClear ();
			RefreshFull (prompt, buf, pos, num, pos, num);
			break;

		case KEY_CTRL_UP: // Move to up line
		case KEY_ALT_UP:
			UpdownMove (prompt, pos, num, -1, true);
			break;

		case KEY_ALT_DOWN: // Move to down line
		case KEY_CTRL_DOWN:
			UpdownMove (prompt, pos, num, 1, true);
			break;

/* Edit Commands */
		case KEY_BACKSPACE: // Delete char to left of cursor (same with CTRL_KEY('H'))
			if (pos > 0) {
				buf.erase(pos-1, 1);
				Refresh (prompt, buf, pos, num, pos-1, num-1, 1);
			}
			break;

		case KEY_DEL:	// Delete character under cursor
		case CTRL_KEY('D'):
			if (pos < num) {
				buf.erase(pos, 1);
				Refresh (prompt, buf, pos, num, pos, num - 1, 1);
			} else if ((0 == num) && (ch == CTRL_KEY('D'))) { // On an empty line, EOF
				PrintStr(" \b\n"); read_end = -1; 
			}
			break;

		case ALT_KEY('u'):	// Uppercase current or following word.
		case ALT_KEY('U'):
			for (new_pos = pos; (new_pos < num) && isdelim(buf[new_pos]); ++new_pos)	;
			for (; (new_pos < num) && !isdelim(buf[new_pos]); ++new_pos)
				{ buf[new_pos] = (char)toupper (buf[new_pos]); }
			Refresh (prompt, buf, pos, num, new_pos, num, 1);
			break;

		case ALT_KEY('l'):	// Lowercase current or following word.
		case ALT_KEY('L'):
			for (new_pos = pos; (new_pos < num) && isdelim(buf[new_pos]); ++new_pos)	;
			for (; (new_pos < num) && !isdelim(buf[new_pos]); ++new_pos)
				{ buf[new_pos] = (char)tolower (buf[new_pos]); }
			Refresh (prompt, buf, pos, num, new_pos, num, 1);
			break;

		case ALT_KEY('c'):	// Capitalize current or following word.
		case ALT_KEY('C'):
			for (new_pos = pos; (new_pos < num) && isdelim(buf[new_pos]); ++new_pos)	;
			if (new_pos<num)
				{ buf[new_pos] = (char)toupper (buf[new_pos]); }
			for (; new_pos<num && !isdelim(buf[new_pos]); ++new_pos)	;
			Refresh (prompt, buf, pos, num, new_pos, num, 1);
			break;

		case ALT_KEY('\\'): // Delete whitespace around cursor.
			for (new_pos = pos; (new_pos > 0) && (' ' == buf[new_pos]); --new_pos)	;
			buf.erase(pos, num - pos);
			Refresh (prompt, buf, pos, num, new_pos, num - (pos-new_pos), 1);
			for (new_pos = pos; (new_pos < num) && (' ' == buf[new_pos]); ++new_pos)	;
			buf.erase(pos, num - new_pos);
			Refresh (prompt, buf, pos, num, pos, num - (new_pos-pos), 1);
			break;

		case CTRL_KEY('T'): // Transpose previous character with current character.
			if ((pos > 0) && !isdelim(buf[pos]) && !isdelim(buf[pos-1])) {
				ch = buf[pos];
				buf[pos] = buf[pos-1];
				buf[pos-1] = (char)ch;
				Refresh (prompt, buf, pos, num, pos<num?pos+1:pos, num, 1);
			} else if ((pos > 1) && !isdelim(buf[pos-1]) && !isdelim(buf[pos-2])) {
				ch = buf[pos-1];
				buf[pos-1] = buf[pos-2];
				buf[pos-2] = (char)ch;
				Refresh (prompt, buf, pos, num, pos, num, 1);
			}
			break;

/* Cut&Paste Commands */
		case CTRL_KEY('K'): // Cut from cursor to end of line.
		case KEY_CTRL_END:
		case KEY_ALT_END:
			TextCopy (info->clip_buf, buf, pos, num);
			Refresh (prompt, buf, pos, num, pos, pos, 1);
			break;

		case CTRL_KEY('U'): // Cut from start of line to cursor.
		case KEY_CTRL_HOME:
		case KEY_ALT_HOME:
			TextCopy (info->clip_buf, buf, 0, pos);
			buf.erase(0, num-pos);
			Refresh (prompt, buf, pos, num, 0, num - pos, 1);
			break;

		case CTRL_KEY('X'):	// Cut whole line.
			TextCopy (info->clip_buf, buf, 0, num);
			// fall through
		case ALT_KEY('r'):	// Revert line
		case ALT_KEY('R'):
			Refresh (prompt, buf, pos, num, 0, 0, 1);
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
			TextCopy (info->clip_buf, buf, new_pos, pos);
			buf.erase(new_pos, pos - new_pos);
			Refresh (prompt, buf, pos, num, new_pos, num - (pos-new_pos), 1);
			break;

		case ALT_KEY('d'): // Cut word following cursor.
		case ALT_KEY('D'):
		case KEY_ALT_DEL:
		case KEY_CTRL_DEL: {
			for (new_pos = pos; (new_pos < num) && isdelim(buf[new_pos]); ++new_pos)	;
			for (; (new_pos < num) && !isdelim(buf[new_pos]); ++new_pos)	;
			TextCopy (info->clip_buf, buf, pos, new_pos);
			int no_del = new_pos - pos;
			buf.erase(pos, no_del);
			Refresh (prompt, buf, pos, num, pos, num - no_del, 1);
			break;
		}
		case CTRL_KEY('Y'):	// Paste last cut text.
		case CTRL_KEY('V'):
		case KEY_INSERT: {
			// if ((len=info->s_clip_buf.length()) + num < size) {
				buf.insert(pos, info->clip_buf);
				// memmove (&buf[pos+len], &buf[pos], num - pos);
				// memcpy (&buf[pos], info->s_clip_buf, len);
				int clipLen = info->clip_buf.length();
				Refresh (prompt, buf, pos, num, pos+clipLen, num+clipLen, 1);
			// }
			break;
		}

/* Complete Commands */
		case KEY_TAB:		// Autocomplete (same with CTRL_KEY('I'))
		case ALT_KEY('='):	// List possible completions.
		case ALT_KEY('?'): {
			if (edit_only || (NULL == info->completion_callback)) {
				break; 
			}
			CrosslineCompletions completions;
			completions.Clear();
			info->completion_callback (buf, *this, pos, completions);
			// int common_add = 0;
			int pos1 = pos;
			int num1 = num;
			int oldLen = 0;
			std::string buf1 = buf;
			if (completions.Size() >= 1) {
				if (KEY_TAB == ch) {
					std::string common = completions.FindCommon();
					int commonLen = common.length();
					int start = completions.start;
					oldLen = completions.end - start;    // length that is being replaced

					// this is a temporary addition
					buf1 = buf.substr(0, start) + common + buf.substr(completions.end);
					if (buf1 != buf) {
						// buf.insert(completions.end, common.substr(oldLen));
						// common_add = commonLen;
						Refresh (prompt, buf1, pos1, num1, start+commonLen, num+commonLen-oldLen, 1);
						pos = pos1;
						num = num1;
						buf = buf1;
						oldLen = common.length();
					}
				}
			}
			int newNum = num;
			int newPos = pos;
			std::map<std::string, int> matches;
			if (((completions.Size() > 1) || (KEY_TAB != ch)) && ShowCompletions(completions, matches)) { 

				StrVec inChoices;
				for(auto const & pair : matches) {
					inChoices.push_back(pair.first);
				}
				std::string inBuf;
				// StrVec inChoices = {"1", "2", "3", "4", "5", "6", "7", "8", "9"};
				if (ReadlineEdit(inBuf, "Input match id: ", false, true, inChoices, true)) {
					if (inBuf.length() == 0) {
						PrintStr(" \b\n");
						break;
					}

					if (matches.count(inBuf) == 0) {
						PrintStr(" \b\n");
						break;
					}
					// int ind = atoi(inBuf.c_str()) - 1;   // id is 1-based
					int ind = matches[inBuf];
					if (ind < completions.Size()) {
						auto cmp = completions.Get(ind);
						std::string newBuf = cmp->word;
						if (cmp->needQuotes) {
							newBuf = "\"" + newBuf + "\"";
						}
						// pos and num have been updated with common_add
						int len3 = newBuf.length();
						// int start = completions.start + common_add;   // after adding common, move 1 to beyond common

						// int oldLen = completions.end - completions.start;
						newPos = pos + len3 - oldLen;
						newNum = num + len3 - oldLen;

						// buf.insert(start, newBuf.substr(common_add));
						int start = completions.start;
						// buf.insert(start, newBuf);
						buf = buf.substr(0, start) + newBuf + buf.substr(pos);
						// RefreshFull (*this, prompt, buf, &pos, &num, newPos, newNum); 
						// break;
					}
				}
				// Need to clear the line as moving to a new prompt

				RefreshFull(prompt, buf, pos, num, newPos, newNum); 
			} else if (completions.hint.length() > 0) {
				std::map<std::string, int> inChoices;
				ShowCompletions(completions, inChoices);
				RefreshFull(prompt, buf, pos, num, newPos, newNum); 
			} else {
				pos = pos1;
				num = num1;
				buf = buf1;
			}
			break;
		}

/* History Commands */
		case KEY_UP:		// Fetch previous line in history.
		case CTRL_KEY('P'):
			// at end of line with text entered, so search
			if (canHis && has_his && hisSearch > 0 && pos > 0 && buf.length() == pos) {
				// already have text so search
				std::pair<int, std::string> res;
				std::string search = buf;
				res = HistorySearch (search);
				history_id = res.first;

				// if (search_his > 0)	{ 
				// 	buf = GetHistoryItem(search_his, false)->item;
				// } else { 
				// 	buf = input;
				// 	// strncpy (buf, input, size-1); 
				// }
				if (history_id < 0) {
					PrintStr("\n");   // go to next line
					Refresh (prompt, buf, pos, num, buf.length(), buf.length(), 1);
					break;
				}
				buf = res.second;
				Refresh (prompt, buf, pos, num, buf.length(), buf.length(), 1);
				hisSearch = -1;
				break;
			} else {
				canHis = false;
				hisSearch = -1;
			}

			// check multi line move up
			if (UpdownMove (prompt, pos, num, -1, false)) { 
				break; 
			} 
			// can we use the history
			if (edit_only || !has_his) { 
				info->term.Beep();
				break; 
			}
			if (!copy_buf) { 
				TextCopy (input, buf, 0, num); copy_buf = 1; 
			}
			if (history_id > 0) {
				HistoryCopy (prompt, buf, pos, num, --history_id);
			} else {
				history_id = HistoryCount();
				buf = input;
				// strncpy (buf, input, size - 1);
				// buf[size - 1] = '\0';
				int bufLen = buf.length();
				Refresh (prompt, buf, pos, num, bufLen, bufLen, 1);
			}
			break;

		case KEY_DOWN:		// Fetch next line in history.
		case CTRL_KEY('N'):
			// check multi line move down
			if (UpdownMove (prompt, pos, num, -1, false)) { 
				break; 
			} 
			if (!edit_only && has_his) { 
				if (!copy_buf) { 
					TextCopy (input, buf, 0, num); 
					copy_buf = 1; 
				}
				if (history_id+1 < HistoryCount()) { 
					HistoryCopy (prompt, buf, pos, num, ++history_id); 
				} else {
					// cycle back
					history_id = -1;
					buf = input;
					// strncpy (buf, input, size - 1);
					// buf[size - 1] = '\0';
					int bufLen = buf.length();
					Refresh (prompt, buf, pos, num, bufLen, bufLen, 1);
				}
			} else {
				info->term.Beep();
			}
			break; //case UP/DOWN

		case ALT_KEY('<'):	// Move to first line in history.
		case KEY_PGUP:
			if (edit_only || !has_his) { 
				break; 
			}
			if (!copy_buf)
				{ TextCopy (input, buf, 0, num); copy_buf = 1; }
			if (HistoryCount() > 0) {
				history_id = 0;
				HistoryCopy (prompt, buf, pos, num, history_id);
			}
			break;

		case ALT_KEY('>'):	// Move to end of input history.
		case KEY_PGDN: {
			if (edit_only || !has_his) { 
				break; 
			}
			if (!copy_buf)
				{ TextCopy (input, buf, 0, num); copy_buf = 1; }
			history_id = HistoryCount();
			buf = input;
			// strncpy (buf, input, size-1);
			// buf[size-1] = '\0';
			int bufLen = buf.length();
			Refresh (prompt, buf, pos, num, bufLen, bufLen, 1);
			break;
		}
		case CTRL_KEY('R'):	// Search history
		case CTRL_KEY('S'):
		case KEY_F4: {		// Search history with current input.
			if (edit_only || !has_his) { 
				info->term.Beep();
				break; 
			}
			TextCopy (input, buf, 0, num);
			std::pair<int, std::string> res;
			std::string search = buf;   // (KEY_F4 == ch) ? buf : "";
			res = HistorySearch (search);
			if (res.first >= 0)	{ 
				buf = res.second;
				// buf = GetHistoryItem(search_his, true)->item;
			} else { 
				buf = input;
				// strncpy (buf, input, size-1); 
			}
			// buf[size-1] = '\0';
			int bufLen = buf.length();
			RefreshFull (prompt, buf, pos, num, bufLen, bufLen);
			break;
		}
		case KEY_F2:	// Show history
			if (edit_only || !has_his || (0 == HistoryCount())) { 
				break; 
			}
			PrintStr(" \b\n");
			HistoryShow ();
			RefreshFull (prompt, buf, pos, num, pos, num);
			break;

		case KEY_F3:	// Clear history
			if (edit_only || !has_his) { 
				break; 
			}
			PrintStr(" \b\n!!! Confirm to clear history [y]: ");
			if ('y' == Getch()) {
				PrintStr(" \b\nHistory are cleared!");
				HistoryClear ();
				history_id = 0;
			}
			PrintStr(" \b\n");
			RefreshFull (prompt, buf, pos, num, pos, num);
			break;

/* Control Commands */
		case KEY_ENTER:		// Accept line (same with CTRL_KEY('M'))
		case KEY_ENTER2:	// same with CTRL_KEY('J')
			Refresh (prompt, buf, pos, num, num, num, 0);
			PrintStr(" \b\n");
			read_end = 1;
			break;

		case CTRL_KEY('C'):	// Abort line.
		case CTRL_KEY('G'):
			Refresh (prompt, buf, pos, num, num, num, 0);
			if (CTRL_KEY('C') == ch)	{ PrintStr(" \b^C\n"); }
			else	{ PrintStr(" \b\n"); }
			num = pos = 0;
			errno = EAGAIN;
			read_end = -1;
			break;;

		case CTRL_KEY('Z'):
#ifndef _WIN32
			raise(SIGSTOP);    // Suspend current process
			RefreshFull (prompt, buf, pos, num, pos, num);
#endif
			break;

		default:
			canHis = !edit_only;
			if (!is_esc && isprint(ch)) {  // && (num < size-1)) {
				buf.insert(pos, 1, ch);
				// memmove (&buf[pos+1], &buf[pos], num - pos);
				// buf[pos] = (char)ch;
				if (prompt.length() + pos + 1 > cols) {
					Refresh (prompt, buf, pos, num, pos+1, num+1, 1);
				} else {
				 	Refresh (prompt, buf, pos, num, pos+1, num+1, pos+1);
				}
				copy_buf = 0;
			} else if (is_esc && !info->allowEscCombo) {
				// clear the line
				PrintStr("\n");
				// pos = 0;
				// num = 0;
				read_end = -1;
			}
			break;
        } // switch( ch )
// #ifdef TODO        
	 	fflush(stdout);
// #endif
	 	if (choices.size() > 0 and num > 0) {
		 	bool hasMatch = false;
	 		for (auto const &st : choices) {
	 			if (buf == st) {
					PrintStr(" \b\n");
					read_end = 1;
					is_choice = true;
					hasMatch = true;
					break;
	 			} else {
	 				if (st.find(buf) == 0) {
	 					// matches start of string
	 					hasMatch = true;
	 					break;
	 				}
	 			}
	 		}
	 		if (!hasMatch) {
	 			read_end = 1;
	 			is_choice = false;
	 		}
		}

	 	// if (history_id < 0 && HistoryCount() > 0) {
	 	// 	history_id = HistoryCount() - 1;
	 	// }
	 	hisSearch++;   // not doing a history search
	} while ( !read_end );

	if (clear) {
		ClearLine();
	}

	if (read_end > 0) {
		// success completion

		// add to history if not a canned response
		if (choices.size() == 0 && !edit_only && (num > 0)) {
			// check if already stored
			bool add = true;
			int hisNo = HistoryCount();
			if (hisNo > 0) {
				HistoryItemPtr it = GetHistoryItem(hisNo-1);
				if (buf == it->item) {
					add = false;
				}
			}
			if (add) {
				HistoryAdd(buf);
			}
		} else if (choices.size() > 0 && !is_choice) {
			// have response that is not one of the choices
			// put the characters back and return false
			for (auto ch : buf) {
				info->term.PutChar(ch);
			}
			buf.clear();
		}

		return buf.length() > 0;
	} else {
		return false;
		// return buf.length();
	}
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

const int TerminalClass::BufLen;

TerminalClass::TerminalClass()
{
#ifdef _WIN32
    hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
#endif	
    curBuf = -1;
}

int TerminalClass::GetChar()
{
	if (curBuf >= 0) {
		return buffer[curBuf--];
	}
	return RawGetChar();
}

void TerminalClass::PutChar(const int c)
{
	if (curBuf < BufLen) {
		buffer[++curBuf] = c;
	}
}

void TerminalClass::Beep()
{
	Print("\x7");
}

CrosslinePrivate::CrosslinePrivate(const bool log)
{
	completion_callback = nullptr;
	paging_print_line = 0;
	prompt_color = CROSSLINE_COLOR_DEFAULT;
	history_noSearchRepeats = false;
	allowEscCombo = true;

	word_delimiter = CROSS_DFT_DELIMITER;
	got_resize = false;

	history_search_no = 20;

	if (log) {	
		logFile = "Messages.log";
		std::ofstream ofs(logFile);
		ofs.close();
	} else {
		logFile = "";
	}

}

void Crossline::ClearLine()
{
	std::string st(info->last_print_num, ' ');
	PrintStr(st);
	CursorMove(0, -info->last_print_num);
}

bool CrosslinePrivate::IsLogging() const
{
	return logFile.length() > 0;
}

void CrosslinePrivate::LogMessage(const std::string &st)
{
	if (IsLogging()) {
	    std::ofstream ofs(logFile, std::ios::app);
	    if (ofs) {
	        ofs << st << "\n";
	    }
	    ofs.close();
	}
}


// Crossline class
Crossline::Crossline(const bool log)
{
	info = new CrosslinePrivate(log);
    CompletionRegister (completion_hook);
}

void Crossline::AllowESCCombo(const bool all)
{
	info->allowEscCombo = all;
}


void Crossline::HistoryDelete(const ssize_t ind, const ssize_t n)
{
}

// void Crossline::Printf(const std::string &fmt, const std::string st)
// {
// 	printf(fmt.c_str(), st.c_str());
// }

// void Crossline::Print(const std::string &msg)
// {
// 	printf(msg.c_str());
// }


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

std::string CrosslineCompletions::FindCommon() 
{
	// Find the common part of the completions
	std::string common = comps[0]->word;
	int len = common.length();
	if (comps.size() == 1 && comps[0]->needQuotes) {
		common = "\"" + common + "\"";
	}

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
