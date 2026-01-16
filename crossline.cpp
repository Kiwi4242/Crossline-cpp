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

#define LOGMESSAGE 0

/*****************************************************************************/

// Default word delimiters for move and cut
#define CROSS_DFT_DELIMITER			" !\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~"

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

// class for handling low level functions
class TerminalClass {
public:
#ifdef _WIN32
	HANDLE hConsole;
#else
   termios origIOS;
#endif
    int screenRows;
    int screenCols;

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

// a class for storing private hidden variables
struct CrosslinePrivate {

	TerminalClass term;
	bool history_noSearchRepeats;
	int history_search_no;  // the number of history items to show

	std::string word_delimiter;
    bool got_resize; // Window size changed

	std::string	clip_buf; // Buf to store cut text
	// crossline_completion_callback completion_callback = nullptr;

    int paging_print_line = 0; // For paging control
    crossline_color_e prompt_color = CROSSLINE_COLOR_DEFAULT;

	bool allowEscCombo;

	std::string logFile;

    int last_print_num;   // store the size of the last printed string

	CrosslinePrivate(const bool log);

	void LogMessage(const std::string &st);
	bool IsLogging() const;

	// Get screen rows and columns
	void ScreenGet (int &pRows, int &pCols);


};


bool Crossline::isdelim(const char ch)
{
	// Check ch is word delimiter
	return NULL != strchr(privData->word_delimiter.c_str(), ch);
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
        const unsigned int CROSS_HISTORY_BUF_LEN = 4096;
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
		privData->word_delimiter = delim;
	}
}

void Crossline::HistoryShow (void)
{
	int noHist = history->Size();
	for (int i = 0; i < noHist; i++) {
		HistoryItemPtr hisPtr = history->GetHistoryItem(i);
		const std::string &hisItem = hisPtr->item;
		PrintStr(hisItem);
}
}



int HistoryClass::HistorySave (const std::string &filename) const
{
	if (filename.length() == 0) {
		return -1;
	} else {
		FILE *file = fopen(filename.c_str(), "wt");
		if (file == NULL) {	return -1;	}

		int noHist = Size();
		for (int i = 0; i < noHist; i++) {
			HistoryItemPtr hisPtr = GetHistoryItem(i);
			const std::string &hisItem = hisPtr->item;
			fprintf (file, "%s\n", hisItem.c_str());
		}
		fclose(file);
	}
	return 0;
}

int HistoryClass::HistoryLoad (const std::string &filename)
{
    // Load a simple file with a command on a single line
	int		len;
	if (filename.length() == 0)	{
		return -1;
	}

    std::ifstream inp(filename);
    if (!inp.good()) {
        return -1;
		}
    bool found = false;
    for (std::string line; std::getline(inp, line);) {
        HistoryItemPtr item = std::make_shared<HistoryItem>(line);
        Add(item);
	}

	return 0;
}


void HistoryClass::Add(const std::string &item)
{
    HistoryItemPtr ptr = std::make_shared<HistoryItem>(item);
    Add(ptr);
}

HistoryItemPtr HistoryClass::GetHistoryItem(const ssize_t n) const
{
	int ind = n;
	return MakeItemPtr(Get(ind));
}

void Crossline::HistorySetup(const bool noSearchRepeats)
{
	privData->history_noSearchRepeats = noSearchRepeats;
}

void Crossline::HistorySetSearchMaxCount(const ssize_t n)
{
	privData->history_search_no = n;
}

/*****************************************************************************/

bool Crossline::PagingSet (const bool enable)
{
	bool prev = privData->paging_print_line >=0;
	privData->paging_print_line = enable ? 0 : -1;
	return prev;
}

int Crossline::PagingCheck (int line_len)
{
	std::string paging_hints("*** Press <Space> or <Enter> to continue . . .");
	int	i, ch, rows, cols, len = paging_hints.length();

	if ((privData->paging_print_line < 0) || !isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO))	{
		return 0;
	}
	ScreenGet (rows, cols);
	privData->paging_print_line += (line_len + cols - 1) / cols;
	if (privData->paging_print_line >= (rows - 1)) {
		PrintStr(paging_hints);
		ch = Getch();
		if (0 == ch) { Getch(); }	// some terminal server may send 0 after Enter
		// clear paging hints
		for (i = 0; i < len; ++i) { PrintStr("\b"); }
		for (i = 0; i < len; ++i) { PrintStr(" ");  }
		for (i = 0; i < len; ++i) { PrintStr("\b"); }
		privData->paging_print_line = 0;
		if ((' ' != ch) && (KEY_ENTER != ch) && (KEY_ENTER2 != ch)) {
			return 1;
		}
	}
	return 0;
}

/*****************************************************************************/

void  Crossline::PromptColorSet (crossline_color_e color)
{
	privData->prompt_color	= color;
}

void Crossline::ScreenClear ()
{
	int ret = system (s_crossline_win ? "cls" : "clear");
	(void) ret;
}

void Crossline::PrintStr(const std::string st)
{
    privData->term.Print(st);
}

int Crossline::Getch (void)
{
    return privData->term.GetChar();
}

void Crossline::ShowCursor(const bool show)
{
    privData->term.ShowCursor(show);
}


void Crossline::ScreenGet(int &pRows, int &pCols)
{
    privData->ScreenGet(pRows, pCols);
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


void TerminalClass::Print(const std::string &st)
{
	// printf(st.c_str());
	WriteConsole(hConsole, st.c_str(), st.size(), nullptr, nullptr);
}


void TerminalClass::ShowCursor(const bool show)
{
  CONSOLE_CURSOR_INFO curInfo;
  if (!GetConsoleCursorInfo(hConsole, &curInfo)) {
  	return;
  }
  curInfo.bVisible = show;
  SetConsoleCursorInfo(hConsole, &curInfo);
}


void CrosslinePrivate::ScreenGet (int &pRows, int &pCols)
{
	CONSOLE_SCREEN_BUFFER_INFO inf;
	GetConsoleScreenBufferInfo (term.hConsole, &inf);
	pCols = inf.srWindow.Right - inf.srWindow.Left + 1;
	pRows = inf.srWindow.Bottom - inf.srWindow.Top + 1;
	pCols = pCols > 1 ? pCols : 160;
	pRows = pRows > 1 ? pRows : 24;
}

bool Crossline::CursorGet (int &pRow, int &pCol)
{
	CONSOLE_SCREEN_BUFFER_INFO inf;
	GetConsoleScreenBufferInfo (privData->term.hConsole, &inf);
	pRow = inf.dwCursorPosition.Y - inf.srWindow.Top;
	pCol = inf.dwCursorPosition.X - inf.srWindow.Left;
	return true;
}

void Crossline::CursorSet (const int row, const int col)
{
	CONSOLE_SCREEN_BUFFER_INFO inf;
	GetConsoleScreenBufferInfo (privData->term.hConsole, &inf);
	inf.dwCursorPosition.Y = (SHORT)row + inf.srWindow.Top;
	inf.dwCursorPosition.X = (SHORT)col + inf.srWindow.Left;
	SetConsoleCursorPosition (privData->term.hConsole, inf.dwCursorPosition);
}

void Crossline::CursorMove (const int row_off, const int col_off)
{
	CONSOLE_SCREEN_BUFFER_INFO inf;
	GetConsoleScreenBufferInfo (privData->term.hConsole, &inf);
	inf.dwCursorPosition.Y += (SHORT)row_off;
	inf.dwCursorPosition.X += (SHORT)col_off;
	SetConsoleCursorPosition (privData->term.hConsole, inf.dwCursorPosition);
}

void Crossline::ColorSet (crossline_color_e color)
{
    CONSOLE_SCREEN_BUFFER_INFO scrInfo;
	static WORD dft_wAttributes = 0;
	WORD wAttributes = 0;
	if (!dft_wAttributes) {
		GetConsoleScreenBufferInfo(privData->term.hConsole, &scrInfo);
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
	SetConsoleTextAttribute(privData->term.hConsole, wAttributes);
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

void CrosslinePrivate::ScreenGet (int &pRows, int &pCols)
{
	struct winsize ws = {};
	(void)ioctl (1, TIOCGWINSZ, &ws);
	pCols = ws.ws_col;
	pRows = ws.ws_row;
	pCols = pCols > 1 ? pCols : 160;
	pRows = pRows > 1 ? pRows : 24;
}

bool Crossline::CursorGet (int &rows, int &cols)
{
    fflush(stdout);

    struct termios raw;

    // 1. Save original terminal settings
    tcgetattr(STDIN_FILENO, &privData->term.origIOS);
    raw = privData->term.origIOS;

    // 2. Disable canonical mode (line buffering) and echoing
    raw.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

    // 3. Send the escape sequence for "Device Status Report"
    // \033[6n
    if (write(STDOUT_FILENO, "\033[6n", 4) != 4) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &privData->term.origIOS);
        return false;
    }

    // 4. Read the response: \033[rows;colsR
    char buffer[32];
    unsigned int i = 0;
    while (i < sizeof(buffer) - 1) {
        if (read(STDIN_FILENO, &buffer[i], 1) != 1) break;
        if (buffer[i] == 'R') break;
        i++;
    }
    buffer[i] = '\0';

    // 5. Restore original terminal settings immediately
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &privData->term.origIOS);

    // 6. Parse the response (skipping the first two characters '\033[')
    rows = 0;
    cols = 0;
    if (buffer[0] != '\033' || buffer[1] != '[') return false;
    if (sscanf(&buffer[2], "%d;%d", &rows, &cols) != 2) return false;

    rows--;
    cols--;
    return true;
}

#ifdef OLD
bool Crossline::CursorGet (int &pRow, int &pCol)
{
#ifndef OLD
	char rowSt[32];
	int rowI = 0;
	char colSt[32];
	int colI = 0;
	bool brackets = false;
	bool comma = false;
	fflush (stdout);
	PrintStr("\e[6n");

	// scanner a string with format: <bytes>[<line>;<col>R
	char ch = 0;
	while(ch != 'R') {
	  char ch;      //   = Getch();
	  if (read(STDIN_FILENO, &ch, 1) >= 0) {
        if (ch == '[') {
    	    brackets = true;
    	  } else {
    	    if (ch == ';') {
    	      comma = true;
    	    } else {
    	      if (brackets && !comma && (ch >= '0' && ch <= '9')) {
    		rowSt[rowI++] = ch;
    	      } else {
    		if (comma && (ch >= '0' && ch <= '9')) {
    		  colSt[colI++] = ch;
    		} else if (ch == 'R') {
    		  break;
    		}
    	      }
    	    }
    	  }
		}
	}
	if (rowI == 0 || colI == 0) {
	  return false;
	}
	rowSt[rowI] = '\0';
	colSt[colI] = '\0';

	pRow = std::stoi(rowSt,nullptr,10);
	pCol = std::stoi(colSt,nullptr,10);

	pRow--; pCol--;
	return true;
#else
  std::cout << "\033[6n" << std::flush;

  char c = 0;
  std::string str_line, str_col;

  // marks to help scanner the string
  bool brackets = false;
  bool comma = false;

  // scanner a string with format: <bytes>[<line>;<col>R
  while (c != 'R') {
    std::cin.get(c);

    // get the line position
    if (brackets && !comma && (c >= '0' && c <= '9')) {
      str_line += c;
    }

    // get the col position
    if (comma && (c >= '0' && c <= '9')) {
      str_col += c;
    }

    if (c == ';') {
      comma = true;
    }

    if (c == '[') {
      brackets = true;
    }
  }

  pRow = std::stoi(str_line,nullptr,10);
  pCol = std::stoi(str_col,nullptr,10);

  return true;
#endif
}
#endif

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
int Crossline::HistoryDump(const bool print_id, std::string patterns,
                           std::map<std::string, int> &matches,
                           const int maxNo, const bool forward)
{
	int	id = 0, num=0;
	std::vector<std::string> pat_list;
    matches.clear();

	bool noRepeats = privData->history_noSearchRepeats;
	std::map<std::string, int> matched;
    std::vector<std::string> patMatches;

	std::vector<char> keys = MakeIndexKeys();
	int maxKeys = keys.size();

	// first get up to maxKeys matches
	int noHist = history->Size();
	int count = 0;
	for (int i = 0; i < noHist; i++) {
		// std::string &history = cLine.info->s_history_items[i];
		int ind = i;
		if (!forward) {
			ind = noHist - 1 - i;
		}
		HistoryItemPtr hisPtr = history->GetHistoryItem(ind);
		const std::string &hisSt = hisPtr->item;
		if (hisSt.length() > 0) {
			if ((patterns.length() > 0) && hisSt.find(patterns) == hisSt.npos) {
				continue; 
			}
			// avoid repeats
            if (noRepeats && matched.count(hisSt)) {
				continue;
			}
			matched[hisSt] = ind;
			patMatches.push_back(hisSt);
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
	int noSearch = privData->history_search_no;
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
		return {id, history->GetHistoryItem(id)->item};
	}

	int his_id = matches[buf];
    return {his_id, history->GetHistoryItem(his_id)->item};

}


// Show completions returned by callback.
int Crossline::ShowCompletions (std::map<std::string, int> &matches)
{
	int ret = 0, word_len = 0, with_help = 0;

    if ((completer->HasHint()) || (completer->Size() > 0)) {
		PrintStr(" \b\n");
		ret = 1;
	}
	// Print syntax hints.
    if (completer->HasHint()) {
		PrintStr("Please input: "); 
        ColorSet (completer->GetHintColor());
        PrintStr(completer->GetHint()); 
		ColorSet (CROSSLINE_COLOR_DEFAULT);
		PrintStr("\n");
	}
    if (0 == completer->Size()) { 
		return ret; 
	}

    // get maximum length
    for (int i = 0; i < completer->Size(); ++i) {
        CompletionItemPtr comp = completer->MakeItemPtr(completer->Get(i));
        const std::string &word = comp->GetWord();
        if (word.length() > word_len) { 
            word_len = word.length(); 
		}
        const std::string &help = comp->GetHelp();
        if (help.length() > 0)  { 
			with_help = 1; 
		}
	}

    std::vector<char> keys = MakeIndexKeys();
    int maxKeys = keys.size();

	// limit the number shown to the number of keys - should remove this restriction, but 36 entries should be enough for now
    int no = completer->Size();;
	if (no > maxKeys) {
		no = maxKeys;
	}

	if (with_help) {
		// Print words with help format.
		for (int i = 0; i < no; ++i) {
            CompletionItemPtr comp = completer->MakeItemPtr(completer->Get(i));
            const std::string &word = comp->GetWord();
            ColorSet (comp->GetColor());

		    std::ostringstream msg;
            msg << std::setw(4) << keys[i] << ":  " << word;

			PrintStr(msg.str());
            int l = 4 + word_len - word.length();
			if (l > 0) {
				std::string spaces(l, ' ');
				PrintStr(spaces);
			}
            ColorSet (comp->GetHelpColor());
            PrintStr(comp->GetHelp());
			ColorSet (CROSSLINE_COLOR_DEFAULT);
			PrintStr("\n");
            if (PagingCheck(comp->GetHelp().length()+4+word_len+1))
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
        CompletionItemPtr comp = completer->MakeItemPtr(completer->Get(i));
        const std::string &word = comp->GetWord();
        ColorSet (comp->GetColor());
	    std::ostringstream msg;

  		msg << std::setw(4) << keys[i] << ":  ";
		PrintStr(msg.str());
  		ColorSet(CROSSLINE_FGCOLOR_BLACK | CROSSLINE_FGCOLOR_BRIGHT);
        PrintStr(word);
		ColorSet (CROSSLINE_COLOR_DEFAULT);

		msg.str(std::string()); // clear
		msg << keys[i];
		matches[msg.str()] = i;

        int l = word_len - word.length();
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

    if (completer->Size() % word_num) { 
        PrintStr("\n"); 
    }
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
						 const int new_pos, const int new_num, const UpdateType updateType,
						 const int drawPosIn)
{
    int prLen = prompt.length();
    int drawPos = drawPosIn;
    if (updateType == UpdateType::DRAW_ALL) {
        drawPos = 0;
    }

	if (s_crossline_win) {
		ScreenGet (privData->term.screenRows, privData->term.screenCols);
	}
	int rows = privData->term.screenRows;
	int cols = privData->term.screenCols;

	std::ostringstream msg;

    if (privData->IsLogging()) {
        msg << "Writing " << buf << "\n";
        msg << "   with: updateType, drawPos = " << int(updateType) << ", " << drawPos << "\n";
    }

    if (updateType == UpdateType::MOVE_CURSOR) {  // just move cursor
        int pos_row = (new_pos+prLen)/(cols) - (pCurPos+prLen)/(cols);
        int pos_col = (new_pos+prLen)%(cols) - (pCurPos+prLen)%(cols);
		CursorMove (pos_row, pos_col);
        if (privData->IsLogging()) {
		  msg << "\n0. Cursor move only (row, col) " << pos_row << " " << pos_col << "\n";
		    // msg << "\nbChg 0. Cursor move (row, col) " << theRow << " " << theCol << "\n";

			int origCurRow, origCurCol;
			CursorGet(origCurRow, origCurCol);
		    msg << "1. Cursor pos (row, col) " << origCurRow << " " << origCurCol << "\n";
		}
	} else {
    	int cursRows = 0;
    	int cursCols = (prLen + pCurPos) % (cols);   // Col where cursor is
    	// bool res = CursorGet(cursRows, cursCols);
    	// if (!res) {
    	//     PrintStr("Cannot obtain cursor position. Try again");
    	// 	return;
    	// }
		ShowCursor(false);
		// build a single string to write - try to reduce flicker.
		// start at current cursor position - if writing the whole buffer
		// we need to move the cursor to the start of the line
		std::string writeBuf;

        if (privData->IsLogging()) {
	      msg << "0. Cursor pos (row, col) " << cursRows << " " << cursCols << "\n";
	    }

		if (new_num > 0) {
			if (new_num < buf.length()) {
				buf = buf.substr(0, new_num);
			}
		} else {
			buf.clear();
		}

		// need to store lenWritten as some of the chars in buf are \r \n
		int lenWritten = 0;
		if (updateType == UpdateType::DRAW_FROM_POS) { // refresh the least possbile
			std::string::const_iterator it = buf.begin();
			// cLine.PrintStr(&*(it + bChg - 1));
			writeBuf += buf.substr(drawPos);
			// need to include the length already written on the line
 			lenWritten = drawPos;

            int writeCol = (prLen + drawPos) % (cols);   // chg                            // bChg-1 is start of refresh
            int rowOff = (drawPos + prLen - 1)/(cols) - (pCurPos + prLen)/(cols);  // chg        // bChg-1 is start of refresh
			// if we are updating from some posn and not writing the whole line, then move cursor
			CursorMove(rowOff, -cursCols+writeCol);
			cursRows += rowOff;
			cursCols = writeCol;

            if (privData->IsLogging()) {
                int tmpCurRow, tmpCurCol;
				CursorGet(tmpCurRow, tmpCurCol);
			    msg << "1. bChg cursor pos (row, col) " << tmpCurRow << " " << tmpCurCol << "\n";
			}

		} else {
		    // Redraw everything
			// if the text fills n lines we need to move the cursor back before writing
            int pos_row = (pCurPos + prLen) / (cols);    // -1 as have length but using pos
			CursorMove(-pos_row, -cursCols);
			cursRows -= pos_row;
			cursCols = 0;

            if (privData->IsLogging()) {
			    msg << "1. Cursor move (row, col) " << -pos_row << " " << -cursCols << "\n";
			    msg << "1. Row move " << pCurPos + prLen - 1 << " " << cols << "\n";

				int tmpCurRow, tmpCurCol;
				CursorGet(tmpCurRow, tmpCurCol);
			    msg << "1. Cursor pos (row, col) " << tmpCurRow << " " << tmpCurCol << "\n";
			}

			ColorSet (privData->prompt_color);
			// cLine.PrintStr("\r" + prompt);
			PrintStr(prompt);
			// writePos = prompt.length();
			ColorSet (CROSSLINE_COLOR_DEFAULT);
			// cLine.PrintStr(buf);
			writeBuf += buf;

            if (privData->IsLogging()) {
                int tmpCurRow, tmpCurCol;
				CursorGet(tmpCurRow, tmpCurCol);
			    msg << "2. Cursor pos (row, col) " << tmpCurRow << " " << tmpCurCol << "\n";
			}

		}
		lenWritten += writeBuf.length();

        if (privData->IsLogging()) {
            msg << "lenWritten " << lenWritten << " " << writeBuf.length() << "\n";
        }
        if (!s_crossline_win && new_num>0 && !((new_num+prLen)%cols)) {
			// cLine.PrintStr("\n"); 
			writeBuf += "\n";
		}
		// need to overwrite any old text
		int cnt = pCurNum - new_num;
		if (cnt > 0) {
			std::string st(cnt, ' ');
			// cLine.PrintStr(st);
			writeBuf += st;
			lenWritten += cnt;
		}

		// if we have scrolled to a new line, we need to blank out
		int oldRows = (pCurNum + prLen) / cols;
		int newRows = (lenWritten + prLen) / cols;
		if (oldRows < newRows) {
			int newCols = lenWritten % cols;
			int blanks = cols - newCols;
			std::string blankStr(blanks, ' ');
			writeBuf += blankStr;
			lenWritten += blanks;
			cnt += blanks;
		}

		if (privData->IsLogging()) {
            msg << "Adding " << cnt << " blanks to string" << "\n";
            msg << "oldRows, newRows: " << oldRows << ", " << newRows << "\n";
        }

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
		if (privData->IsLogging()) {
            msg << "String to write\n" << writeBuf << "\n";
        }

		PrintStr(writeBuf);

        if (privData->IsLogging()) {
            int tmpCurRow, tmpCurCol;
			CursorGet(tmpCurRow, tmpCurCol);
		    msg << "3. Cursor pos (row, col) " << tmpCurRow << " " << tmpCurCol << "\n";
		}
		// now the cursor is at the end of the text, move to cursor pos
        int nowRow = (lenWritten + prLen - 1) / (cols);     // -1 as have length but using pos
        int posRow = (new_pos + prLen) / (cols);    // chg
		int rMove =  nowRow - posRow;
		if (rMove < 0) {
			rMove = 0;
		}

		// need to find the cursor column
        int curCPos = (lenWritten + prLen) % (cols);   // chg      // -1 as need pos not len
        int reqCPos = (new_pos + prLen) % (cols);    // chg     // using posn
		// there seems to be an issue if the column should be zero
		int cMove = reqCPos - curCPos;
		CursorMove (-rMove, cMove);
		cursRows -= rMove;
		cursCols = cMove;

        if (privData->IsLogging()) {
		    msg << "4. Cursor move (row, col) " << -rMove << " " << cMove << "\n";

			int tmpCurRow, tmpCurCol;
			CursorGet(tmpCurRow, tmpCurCol);
		    msg << "4. Cursor pos (row, col) " << tmpCurRow << " " << tmpCurCol << "\n";

		    msg << "drawPos, curPos, new_pos " << drawPos << " " << pCurPos << " " << new_pos << "\n";
            msg << "moveRow, origCol, len: "<< rMove << " " << cursCols << " " << prLen << "\n";
	  		msg << "nowRow, posRow, lenWritten, cols: " << nowRow << " " << posRow << " " << lenWritten << " " << cols << "\n";
	  		msg << "cMove, curCPos, reqCPos " << cMove << " " << curCPos << " " << reqCPos << "\n";
		}
		ShowCursor(true);
	}
	pCurPos = new_pos;
	pCurNum = new_num;
	privData->last_print_num = new_num + prLen;
	if (privData->IsLogging()) {
	    privData->LogMessage(msg.str());
	}
}

void Crossline::RefreshFull(const std::string &prompt, std::string &buf, int &pCurPos, int &pCurNum, int new_pos, int new_num)
{
	pCurPos = pCurNum = 0;
	Refresh(prompt, buf, pCurPos, pCurNum, new_pos, new_num, UpdateType::DRAW_ALL, 0);
}

// Copy part text[cut_beg, cut_end] from src to dest
void Crossline::TextCopy (std::string &dest, const std::string &src, int cut_beg, int cut_end)
{
	int len = cut_end - cut_beg;
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
void Crossline::CopyFromHistory (const std::string &prompt, std::string &buf, int &pos, int &num, int history_id)
{
    HistoryItemPtr it = history->GetHistoryItem(history_id);
	buf = it->item;;
	Refresh(prompt, buf, pos, num, buf.length(), buf.length(), UpdateType::DRAW_ALL, 0);
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
	s_got_resize_ptr = &cLine.privData->got_resize;
	struct sigaction sa;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sa.sa_handler = &crossline_winchg_event;
	sigaction (SIGWINCH, &sa, NULL);
	cLine.privData->got_resize = false;
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
    history_id = history->Size();
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
        int ch = crossline_getkey (*this, is_esc, privData->allowEscCombo);
		ch = crossline_key_mapping (ch);

		if (privData->got_resize) { // Handle window resizing for Linux, Windows can handle it automatically
			new_pos = pos;
			Refresh(prompt, buf, pos, num, 0, num, UpdateType::MOVE_CURSOR, 0); // goto beginning of line
			PrintStr("\x1b[J"); // clear to end of screen
			Refresh(prompt, buf, pos, num, new_pos, num, UpdateType::DRAW_ALL, 0);
			privData->got_resize = false;
		}

		switch (ch) {
/* Misc Commands */
		case KEY_F1:	// Show help
			crossline_show_help (*this, edit_only);
			RefreshFull(prompt, buf, pos, num, pos, num);
			break;

		case KEY_DEBUG:	// Enter keyboard debug mode
			PrintStr(" \b\nEnter keyboard debug mode, <Ctrl-C> to exit debug\n");
			while (CTRL_KEY('C') != (ch=Getch())) {
				// printf ("%3d 0x%02x (%c)\n", ch, ch, isprint(ch) ? ch : ' ');
			    std::ostringstream msg;
		  		msg << std::setw(3) << ch << "0x" << std::setw(2) << ch << "x " << (isprint(ch) ? ch : ' ') << "\n";
		  		PrintStr(msg.str());
			}
			RefreshFull(prompt, buf, pos, num, pos, num);
			break;

/* Move Commands */
		case KEY_LEFT:	// Move back a character.
		case CTRL_KEY('B'):
			if (pos > 0)
				{ Refresh(prompt, buf, pos, num, pos-1, num, UpdateType::MOVE_CURSOR, 0); }
			break;

		case KEY_RIGHT:	// Move forward a character.
		case CTRL_KEY('F'):
			if (pos < num)
				{ Refresh(prompt, buf, pos, num, pos+1, num, UpdateType::MOVE_CURSOR, 0); }
			break;

		case ALT_KEY('b'):	// Move back a word.
		case ALT_KEY('B'):
		case KEY_CTRL_LEFT:
		case KEY_ALT_LEFT:
			for (new_pos=pos-1; (new_pos > 0) && isdelim(buf[new_pos]); --new_pos)	;
			for (; (new_pos > 0) && !isdelim(buf[new_pos]); --new_pos)	;
			Refresh(prompt, buf, pos, num, new_pos?new_pos+1:new_pos, num, UpdateType::MOVE_CURSOR, 0);
			break;

		case ALT_KEY('f'):	 // Move forward a word.
		case ALT_KEY('F'):
		case KEY_CTRL_RIGHT:
		case KEY_ALT_RIGHT:
			for (new_pos=pos; (new_pos < num) && isdelim(buf[new_pos]); ++new_pos)	;
			for (; (new_pos < num) && !isdelim(buf[new_pos]); ++new_pos)	;
			Refresh(prompt, buf, pos, num, new_pos, num, UpdateType::MOVE_CURSOR, 0);
			break;

		case CTRL_KEY('A'):	// Move cursor to start of line.
		case KEY_HOME:
		    Refresh(prompt, buf, pos, num, 0, num, UpdateType::MOVE_CURSOR, 0);
			break;

		case CTRL_KEY('E'):	// Move cursor to end of line
		case KEY_END:
			Refresh(prompt, buf, pos, num, num, num, UpdateType::MOVE_CURSOR, 0);
			break;

		case CTRL_KEY('L'):	// Clear screen and redisplay line
			ScreenClear ();
			RefreshFull(prompt, buf, pos, num, pos, num);
			break;

		case KEY_CTRL_UP: // Move to up line
		case KEY_ALT_UP:
			UpdownMove(prompt, pos, num, -1, true);
			break;

		case KEY_ALT_DOWN: // Move to down line
		case KEY_CTRL_DOWN:
			UpdownMove(prompt, pos, num, 1, true);
			break;

/* Edit Commands */
		case KEY_BACKSPACE: // Delete char to left of cursor (same with CTRL_KEY('H'))
			if (pos > 0) {
				buf.erase(pos-1, 1);
				Refresh(prompt, buf, pos, num, pos-1, num-1, UpdateType::DRAW_ALL, 0);
			}
			break;

		case KEY_DEL:	// Delete character under cursor
		case CTRL_KEY('D'):
			if (pos < num) {
				buf.erase(pos, 1);
				Refresh(prompt, buf, pos, num, pos, num - 1, UpdateType::DRAW_ALL, 0);
			} else if ((0 == num) && (ch == CTRL_KEY('D'))) { // On an empty line, EOF
				PrintStr(" \b\n"); read_end = -1;
			}
			break;

		case ALT_KEY('u'):	// Uppercase current or following word.
		case ALT_KEY('U'):
			for (new_pos = pos; (new_pos < num) && isdelim(buf[new_pos]); ++new_pos)	;
			for (; (new_pos < num) && !isdelim(buf[new_pos]); ++new_pos)
				{ buf[new_pos] = (char)toupper (buf[new_pos]); }
			Refresh(prompt, buf, pos, num, new_pos, num, UpdateType::DRAW_ALL, 0);
			break;

		case ALT_KEY('l'):	// Lowercase current or following word.
		case ALT_KEY('L'):
			for (new_pos = pos; (new_pos < num) && isdelim(buf[new_pos]); ++new_pos)	;
			for (; (new_pos < num) && !isdelim(buf[new_pos]); ++new_pos)
				{ buf[new_pos] = (char)tolower (buf[new_pos]); }
			Refresh(prompt, buf, pos, num, new_pos, num, UpdateType::DRAW_ALL, 0);
			break;

		case ALT_KEY('c'):	// Capitalize current or following word.
		case ALT_KEY('C'):
			for (new_pos = pos; (new_pos < num) && isdelim(buf[new_pos]); ++new_pos)	;
			if (new_pos<num)
				{ buf[new_pos] = (char)toupper (buf[new_pos]); }
			for (; new_pos<num && !isdelim(buf[new_pos]); ++new_pos)	;
			Refresh(prompt, buf, pos, num, new_pos, num, UpdateType::DRAW_ALL, 0);
			break;

		case ALT_KEY('\\'): // Delete whitespace around cursor.
			for (new_pos = pos; (new_pos > 0) && (' ' == buf[new_pos]); --new_pos)	;
			buf.erase(pos, num - pos);
			Refresh(prompt, buf, pos, num, new_pos, num - (pos-new_pos), UpdateType::DRAW_ALL, 0);
			for (new_pos = pos; (new_pos < num) && (' ' == buf[new_pos]); ++new_pos)	;
			buf.erase(pos, num - new_pos);
			Refresh(prompt, buf, pos, num, pos, num - (new_pos-pos), UpdateType::DRAW_ALL, 0);
			break;

		case CTRL_KEY('T'): // Transpose previous character with current character.
			if ((pos > 0) && !isdelim(buf[pos]) && !isdelim(buf[pos-1])) {
				ch = buf[pos];
				buf[pos] = buf[pos-1];
				buf[pos-1] = (char)ch;
				Refresh(prompt, buf, pos, num, pos<num?pos+1:pos, num, UpdateType::DRAW_ALL, 0);
			} else if ((pos > 1) && !isdelim(buf[pos-1]) && !isdelim(buf[pos-2])) {
				ch = buf[pos-1];
				buf[pos-1] = buf[pos-2];
				buf[pos-2] = (char)ch;
				Refresh(prompt, buf, pos, num, pos, num, UpdateType::DRAW_ALL, 0);
			}
			break;

/* Cut&Paste Commands */
		case CTRL_KEY('K'): // Cut from cursor to end of line.
		case KEY_CTRL_END:
		case KEY_ALT_END:
			TextCopy (privData->clip_buf, buf, pos, num);
			Refresh(prompt, buf, pos, num, pos, pos, UpdateType::DRAW_ALL, 0);
			break;

		case CTRL_KEY('U'): // Cut from start of line to cursor.
		case KEY_CTRL_HOME:
		case KEY_ALT_HOME:
			TextCopy (privData->clip_buf, buf, 0, pos);
			buf.erase(0, num-pos);
			Refresh(prompt, buf, pos, num, 0, num - pos, UpdateType::DRAW_ALL, 0);
			break;

		case CTRL_KEY('X'):	// Cut whole line.
			TextCopy (privData->clip_buf, buf, 0, num);
			// fall through
		case ALT_KEY('r'):	// Revert line
		case ALT_KEY('R'):
			Refresh(prompt, buf, pos, num, 0, 0, UpdateType::DRAW_ALL, 0);
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
			TextCopy (privData->clip_buf, buf, new_pos, pos);
			buf.erase(new_pos, pos - new_pos);
			Refresh(prompt, buf, pos, num, new_pos, num - (pos-new_pos), UpdateType::DRAW_ALL, 0);
			break;

		case ALT_KEY('d'): // Cut word following cursor.
		case ALT_KEY('D'):
		case KEY_ALT_DEL:
		case KEY_CTRL_DEL: {
			for (new_pos = pos; (new_pos < num) && isdelim(buf[new_pos]); ++new_pos)	;
			for (; (new_pos < num) && !isdelim(buf[new_pos]); ++new_pos)	;
			TextCopy (privData->clip_buf, buf, pos, new_pos);
			int no_del = new_pos - pos;
			buf.erase(pos, no_del);
			Refresh(prompt, buf, pos, num, pos, num - no_del, UpdateType::DRAW_ALL, 0);
			break;
		}
		case CTRL_KEY('Y'):	// Paste last cut text.
		case CTRL_KEY('V'):
        case KEY_INSERT: {
			// if ((len=info->s_clip_buf.length()) + num < size) {
				buf.insert(pos, privData->clip_buf);
				// memmove (&buf[pos+len], &buf[pos], num - pos);
				// memcpy (&buf[pos], info->s_clip_buf, len);
				int clipLen = privData->clip_buf.length();
				Refresh(prompt, buf, pos, num, pos+clipLen, num+clipLen, UpdateType::DRAW_ALL, 0);
			// }
			break;
        }

/* Complete Commands */
		case KEY_TAB:		// Autocomplete (same with CTRL_KEY('I'))
		case ALT_KEY('='):	// List possible completions.
		case ALT_KEY('?'): {
			if (edit_only || (nullptr == completer)) {
				break;
			}
			completer->Clear();
			completer->FindItems(buf, *this, pos);

			// int common_add = 0;
			int pos1 = pos;
			int num1 = num;
            int oldLen = 0;
			std::string buf1 = buf;
            // Setup the common completion
            if (completer->Size() >= 1) {
				if (KEY_TAB == ch) {
                    std::string common = completer->FindCommon();
					int commonLen = common.length();
					int start = completer->start;
					oldLen = completer->end - start;    // length that is being replaced

					// this is a temporary addition
                    buf1 = buf.substr(0, start) + common + buf.substr(completer->end);
					if (buf1 != buf) {
						// buf.insert(completions.end, common.substr(oldLen));
                        // common_add = commonLen;
						Refresh(prompt, buf1, pos1, num1, start+commonLen, num+commonLen-oldLen, UpdateType::DRAW_ALL, 0);
                        pos = pos1;
                        num = num1;
                        buf = buf1;
                        oldLen = common.length();
					}
				}
			}
			int newNum = num;
			int newPos = pos;
            // if have 1 match just use it, 0 or more then show completions or hint
			std::map<std::string, int> matches;
            if (((completer->Size() != 1) || (KEY_TAB != ch)) && ShowCompletions(matches)) { 

                if (matches.size() > 0) {
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
                        if (ind < completer->Size()) {
                            auto cmp = completer->MakeItemPtr(completer->Get(ind));
                            std::string newBuf = cmp->GetWord();
                            if (cmp->NeedQuotes()) {
							newBuf = "\"" + newBuf + "\"";
						}
						// pos and num have been updated with common_add
						int len3 = newBuf.length();
						// int start = completions.start + common_add;   // after adding common, move 1 to beyond common

                            // int oldLen = completions.end - completions.start;
						newPos = pos + len3 - oldLen;
						newNum = num + len3 - oldLen;

						// buf.insert(start, newBuf.substr(common_add));
                            int start = completer->start;
						// buf.insert(start, newBuf);
						buf = buf.substr(0, start) + newBuf + buf.substr(pos);
						// RefreshFull (*this, prompt, buf, &pos, &num, newPos, newNum); 
						// break;
					}
				}
				}
				// Need to clear the line as moving to a new prompt

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
                    Refresh(prompt, buf, pos, num, buf.length(), buf.length(), UpdateType::DRAW_ALL, 0);
					break;
				}
				buf = res.second;
				Refresh(prompt, buf, pos, num, buf.length(), buf.length(), UpdateType::DRAW_ALL, 0);
				hisSearch = -1;
				break;
			} else {
				canHis = false;
                hisSearch = -1;
			}

			// check multi line move up
			if (UpdownMove(prompt, pos, num, -1, false)) {
				break;
			}
			// can we use the history
			if (edit_only || !has_his) {
				privData->term.Beep();
				break;
			}
			if (!copy_buf) {
				TextCopy(input, buf, 0, num); copy_buf = 1;
			}
			if (history_id > 0) {
				CopyFromHistory(prompt, buf, pos, num, --history_id);
			} else {
				history_id = history->Size();
				buf = input;
				// strncpy (buf, input, size - 1);
				// buf[size - 1] = '\0';
				int bufLen = buf.length();
				Refresh(prompt, buf, pos, num, bufLen, bufLen, UpdateType::DRAW_ALL, 0);
			}
			break;

		case KEY_DOWN:		// Fetch next line in history.
		case CTRL_KEY('N'):
			// check multi line move down
			if (UpdownMove(prompt, pos, num, -1, false)) {
				break;
			}
			if (!edit_only && has_his) {
				if (!copy_buf) {
					TextCopy(input, buf, 0, num);
					copy_buf = 1;
				}
                if (history_id+1 < history->Size()) { 
                    CopyFromHistory(prompt, buf, pos, num, ++history_id);
				} else {
					// cycle back
                    history_id = -1;
					buf = input;
					// strncpy (buf, input, size - 1);
					// buf[size - 1] = '\0';
					int bufLen = buf.length();
					Refresh(prompt, buf, pos, num, bufLen, bufLen, UpdateType::DRAW_ALL, 0);
				}
			} else {
				privData->term.Beep();
			}
			break; //case UP/DOWN

		case ALT_KEY('<'):	// Move to first line in history.
		case KEY_PGUP:
			if (edit_only || !has_his) {
				break;
			}
			if (!copy_buf)
				{ TextCopy (input, buf, 0, num); copy_buf = 1; }
            if (history->Size() > 0) {
				history_id = 0;
                CopyFromHistory (prompt, buf, pos, num, history_id);
			}
			break;

		case ALT_KEY('>'):	// Move to end of input history.
		case KEY_PGDN: {
			if (edit_only || !has_his) {
				break;
			}
			if (!copy_buf)
				{ TextCopy (input, buf, 0, num); copy_buf = 1; }
            history_id = history->Size();
			buf = input;
			// strncpy (buf, input, size-1);
			// buf[size-1] = '\0';
			int bufLen = buf.length();
			Refresh(prompt, buf, pos, num, bufLen, bufLen, UpdateType::DRAW_ALL, 0);
			break;
		}
		case CTRL_KEY('R'):	// Search history
		case CTRL_KEY('S'):
		case KEY_F4: {		// Search history with current input.
			if (edit_only || !has_his) {
				privData->term.Beep();
				break;
			}
			TextCopy (input, buf, 0, num);
			std::pair<int, std::string> res;
			std::string search = buf;   // (KEY_F4 == ch) ? buf : "";
			res = HistorySearch (search);
			if (res.first >= 0)	{
				buf = res.second;

				buf = input;
			}

			int bufLen = buf.length();
			RefreshFull(prompt, buf, pos, num, bufLen, bufLen);
			break;
		}
		case KEY_F2:	// Show history
            if (edit_only || !has_his || (0 == history->Size())) { 
				break; 
			}
			PrintStr(" \b\n");
			HistoryShow ();
			RefreshFull(prompt, buf, pos, num, pos, num);
			break;

		case KEY_F3:	// Clear history
			if (edit_only || !has_his) {
				break;
			}
			PrintStr(" \b\n!!! Confirm to clear history [y]: ");
			if ('y' == Getch()) {
				PrintStr(" \b\nHistory are cleared!");
                history->Clear ();
				history_id = 0;
			}
			PrintStr(" \b\n");
			RefreshFull(prompt, buf, pos, num, pos, num);
			break;

/* Control Commands */
		case KEY_ENTER:		// Accept line (same with CTRL_KEY('M'))
		case KEY_ENTER2:	// same with CTRL_KEY('J')
			Refresh(prompt, buf, pos, num, num, num, UpdateType::MOVE_CURSOR, 0);
			PrintStr(" \b\n");
			read_end = 1;
			break;

		case CTRL_KEY('C'):	// Abort line.
		case CTRL_KEY('G'):
			Refresh(prompt, buf, pos, num, num, num, UpdateType::MOVE_CURSOR, 0);
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
					Refresh(prompt, buf, pos, num, pos+1, num+1, UpdateType::DRAW_ALL, 0);
				} else {
				 	Refresh(prompt, buf, pos, num, pos+1, num+1, UpdateType::DRAW_FROM_POS,pos);
				}
				copy_buf = 0;
			} else if (is_esc && !privData->allowEscCombo) {
				// clear the line
				PrintStr("\n");
				// pos = 0;
				// num = 0;
				read_end = -1;
			}
			break;
        } // switch( ch )
	 	fflush(stdout);
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
            int hisNo = history->Size();
		if (hisNo > 0) {
                HistoryItemPtr it = history->GetHistoryItem(hisNo-1);
			if (buf == it->item) {
				add = false;
			}
		}
		if (add) {
                history->Add(buf);
		}
	} else if (choices.size() > 0 && !is_choice) {
		// have response that is not one of the choices
		// put the characters back and return false
		for (auto ch : buf) {
                privData->term.PutChar(ch);
		}
		buf.clear();
	}

	return buf.length() > 0;
	} else {
		return false;
		// return buf.length();
}
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
	paging_print_line = 0;
	prompt_color = CROSSLINE_COLOR_DEFAULT;
	history_noSearchRepeats = false;
	allowEscCombo = true;

    word_delimiter = CROSS_DFT_DELIMITER;
    got_resize = false;

    history_search_no = 20;

    ScreenGet(term.screenRows, term.screenCols);

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
    std::string st(privData->last_print_num, ' ');
    PrintStr(st);
    CursorMove(0, -privData->last_print_num);
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
Crossline::Crossline(CompleterClass *comp, HistoryClass *his, const bool log)
{
    completer = comp;
    history = his;
    if (comp == nullptr) {
        throw("No completer registered");
    }
    if (history == nullptr) {
        throw("No history registered");
    }

    privData = new CrosslinePrivate(log);
}

Crossline::~Crossline()
{
    if (completer != nullptr) {
        delete completer;
    }
    if (history != nullptr) {
        delete history;
}
    if (privData != nullptr) {
        delete privData;
    }
}


void Crossline::AllowESCCombo(const bool all)
{
    privData->allowEscCombo = all;
}


HistoryItemPtr HistoryClass::MakeItemPtr(const SearchItemPtr &p) const
{
    return std::dynamic_pointer_cast<HistoryItem>(p);
}


void HistoryClass::HistoryDelete(const ssize_t ind, const ssize_t n)
{
    std::vector<SearchItemPtr>::iterator it = items.begin();
    items.erase(it+ind, it + (ind+n));
}

// Completions for completion or history
CompleterClass::CompleterClass()
{
    Clear();
}

void CompleterClass::Clear()
{
    BaseSearchable::Clear();
	start = 0;
	end = 0;
}

void CompleterClass::Setup(const int startIn, const int endIn)
{
	start = startIn;
	end = endIn;
}

void CompleterClass::Add(const std::string &word, const std::string &help)
{
    Add(word, help, false, CROSSLINE_COLOR_DEFAULT, CROSSLINE_COLOR_DEFAULT);
}

void CompleterClass::Add(const std::string &word, const std::string &help, const bool needQuotes, 
                    crossline_color_e wcolor, crossline_color_e hcolor) 
{
    items.push_back(std::make_shared<CompletionItem>(word, help, needQuotes, wcolor, hcolor));
}

bool CompleterClass::FindItems(const std::string &buf, Crossline &cLine, const int pos)
{
    return true;
}




void BaseSearchable::AddHint(const std::string &h, crossline_color_e col)
{
    if ((h.length() > 0)) {
        hint = h;
        // pCompletions.hints[CROSS_COMPLET_HINT_LEN - 1] = '\0';
        hintColor = col;
    }
}

void BaseSearchable::Clear()
{
    items.clear();
    hint = "";
}

void BaseSearchable::Add(const SearchItemPtr &item)
{
    items.push_back(item);
}

bool BaseSearchable::HasHint() const
{
    return hint.length() > 0;
}

std::string BaseSearchable::GetHint() const
{
    return hint;
}

crossline_color_e BaseSearchable::GetHintColor() const
{
    return hintColor;
}

CompletionItemPtr CompleterClass::MakeItemPtr(const SearchItemPtr &p) const
{
    return std::dynamic_pointer_cast<CompletionItem>(p);
}

std::string CompleterClass::FindCommon()  const
{
	// Find the common part of the completions
    std::string common;
    if (items.size() == 0) {
        return common;
    }
    auto item = std::dynamic_pointer_cast<CompletionItem>(items[0]);
    common = item->GetWord();
	int len = common.length();
    if (items.size() == 1 && item->NeedQuotes()) {
		common = "\"" + common + "\"";
	}

    for (int i = 1; (i < items.size()) && (len > 0); ++i) {
        item = std::dynamic_pointer_cast<CompletionItem>(items[i]);
        std::string word = item->GetWord().substr(0, len);
		if (word.length() < len) {
			len = word.length();
		}
		while (len > 0 && common.find(word) == common.npos) {
			len--;
			word.pop_back();
		}
		if (len > 0) {  // there should be at least 1 common character
			common = common.substr(0, len);
        } else {
            common = "";
		}
	}
	return common;
}


CompletionItem::CompletionItem()
{
    color = CROSSLINE_COLOR_DEFAULT;
    helpColor = CROSSLINE_COLOR_DEFAULT;
}

CompletionItem::CompletionItem(const std::string w, const std::string h, const bool needQuot,
                                const crossline_color_e wCol, const crossline_color_e hCol)
{
    word = w;
    help = h;
    needQuotes = needQuot;
    color = wCol;
    helpColor = hCol;
}

std::string CompletionItem::GetStItem(const int no) const
{
    return word;
}

const std::string &CompletionItem::GetWord() const
{
    return word;
}

crossline_color_e CompletionItem::GetColor() const
{
    return color;
}

const std::string &CompletionItem::GetHelp() const
{
    return help;
}

crossline_color_e CompletionItem::GetHelpColor() const
{
    return helpColor;
}

bool CompletionItem::NeedQuotes() const
{
    return needQuotes;
}



HistoryItem::HistoryItem(const std::string &st)
{
    item = st;
}

std::string HistoryItem::GetStItem(const int no) const
{
    return item;
}


HistoryClass::HistoryClass()
{

}

bool HistoryClass::FindItems(const std::string &buf, Crossline &cLine, const int pos)
{
    return false;
}
