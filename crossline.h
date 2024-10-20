/* crossline.h -- Version 1.0
 *
 * Crossline is a small, self-contained, zero-config, MIT licensed,
 *   cross-platform, readline and libedit replacement.
 *
 * Press <F1> to get full shortcuts list.
 *
 * See crossline.c for more information.
 *
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

#ifndef __CROSSLINE_H
#define __CROSSLINE_H

#include <string>
#include <vector>
#include <memory>
#include <utility>
#include <map>

typedef enum {
	CROSSLINE_FGCOLOR_DEFAULT       = 0x00,
	CROSSLINE_FGCOLOR_BLACK         = 0x01,
	CROSSLINE_FGCOLOR_RED           = 0x02,
	CROSSLINE_FGCOLOR_GREEN         = 0x03,
	CROSSLINE_FGCOLOR_YELLOW        = 0x04,
	CROSSLINE_FGCOLOR_BLUE          = 0x05,
	CROSSLINE_FGCOLOR_MAGENTA       = 0x06,
	CROSSLINE_FGCOLOR_CYAN          = 0x07,
	CROSSLINE_FGCOLOR_WHITE     	= 0x08,
	CROSSLINE_FGCOLOR_BRIGHT     	= 0x80,
	CROSSLINE_FGCOLOR_MASK     	    = 0x7F,

	CROSSLINE_BGCOLOR_DEFAULT       = 0x0000,
	CROSSLINE_BGCOLOR_BLACK         = 0x0100,
	CROSSLINE_BGCOLOR_RED           = 0x0200,
	CROSSLINE_BGCOLOR_GREEN         = 0x0300,
	CROSSLINE_BGCOLOR_YELLOW        = 0x0400,
	CROSSLINE_BGCOLOR_BLUE          = 0x0500,
	CROSSLINE_BGCOLOR_MAGENTA       = 0x0600,
	CROSSLINE_BGCOLOR_CYAN          = 0x0700,
	CROSSLINE_BGCOLOR_WHITE     	= 0x0800,
	CROSSLINE_BGCOLOR_BRIGHT     	= 0x8000,
	CROSSLINE_BGCOLOR_MASK     	    = 0x7F00,

	CROSSLINE_UNDERLINE     	    = 0x10000,

	CROSSLINE_COLOR_DEFAULT         = CROSSLINE_FGCOLOR_DEFAULT | CROSSLINE_BGCOLOR_DEFAULT
} CrosslineColorEnum;

typedef int crossline_color_e;

class Crossline;
class CompletionInfo;
typedef std::shared_ptr<CompletionInfo> CompletionInfoPtr;

class CrosslineCompletions {
public:	
	std::vector<CompletionInfoPtr> comps;
	std::string hint;
	crossline_color_e	color_hint;
	int start;                         // start of position in buffer to replace
	int end; 						   // end of position in buffer to replace
	CrosslineCompletions();	

	void Clear();

	int Size() {return comps.size();}

	const CompletionInfoPtr &Get(const int i) const {
		return comps[i];
	}

	std::string GetComp(const int i) const;
	
	void Setup(const int startIn, const int endIn);
	void Add(const std::string &word, const std::string &help, const bool quotes=false, 
 			 const crossline_color_e wcolor=CROSSLINE_FGCOLOR_DEFAULT, 
			 const crossline_color_e hcolor=CROSSLINE_FGCOLOR_DEFAULT);

	std::string FindCommon();
};

typedef void (*crossline_completion_callback) (const std::string &buf, Crossline &cLine, const int pos, CrosslineCompletions &pCompletions);
class CrosslinePrivate;

// struct CompletionItem {
// public:	
//     std::string comp;
//     int start;
// };

struct HistoryItemBase {
public:
	std::string item;	
	HistoryItemBase() {}
	HistoryItemBase(const std::string &st) {
		item = st;
	}
};

typedef std::shared_ptr<HistoryItemBase> HistoryItemPtr;
typedef std::vector<std::string> StrVec;

// Class for reading and writing to the console
class Crossline {

protected:
//	bool ReadlineInternal (const std::string &prompt, std::string &buf, bool has_input);
	bool ReadlineEdit (std::string &buf, const std::string &prompt, const bool has_input, 
						const bool edit_only, const StrVec &choices);

	int	HistoryDump (FILE *file, const bool print_id, std::string patterns, 
								std::map<std::string, int> &matches, const int maxNo, const bool paging,
								const bool forward, const bool useFile);

	int ShowCompletions (CrosslineCompletions &pCompletions, std::map<std::string, int> &matches);

	// Register completion callback
	void  CompletionRegister (crossline_completion_callback pCbFunc);

	// update the line starting at the beginning of the line
	void RefreshFull(const std::string &prompt, std::string &buf, int &pCurPos, int &pCurNum, int new_pos, int new_num);

	// update the line starting from current position
	void Refresh(const std::string &prompt, std::string &buf, int &pCurPos, int &pCurNum, 
				 const int new_pos, const int new_num, const int bChg);

	void HistoryCopy (const std::string &prompt, std::string &buf, int &pos, int &num, int history_id);

	void TextCopy (std::string &dest, const std::string &src, int cut_beg, int cut_end);

	bool UpdownMove (const std::string &prompt, int &pCurPos, const int pCurNum, const int off, 
				      const bool bForce);


public:
	CrosslinePrivate *info;

	Crossline(const bool log=false);

	// Main API to read a line, return input in buf if get line, return false if EOF. If buf has content use that to start
	bool ReadLine (const std::string &prompt, std::string &buf, const bool useBuf=false);

    // Complete the string in inp, return match in completions and the prefix that was matched, called when the user presses tab
    virtual bool Completer(const std::string &inp, const int pos, CrosslineCompletions &completions) = 0;

    // void Printf(const std::string &fmt, const std::string st="");
    void PrintStr(const std::string msg);

    // ESC clears
    void AllowESCCombo(const bool);

	/* 
	 * History APIs
	 */

    // setup the use of history, don't show repeats in search
    void HistorySetup(const bool noSearchRepeats);

	// Load history from file, stored as a list of commands
	virtual int HistoryLoad (const std::string &filename);

	// Save history to file
	virtual int HistorySave(const std::string &filename);

	// Clear history
	virtual void  HistoryClear (void);

    virtual int HistoryCount();
    virtual HistoryItemPtr GetHistoryItem(const ssize_t n);
    virtual void HistoryDelete(const ssize_t ind, const ssize_t n);
    virtual void HistoryAdd(const HistoryItemPtr &st);
    virtual void HistoryAdd(const std::string &st);

    // search the history
	std::pair<int, std::string> HistorySearch (std::string &input);
	// Show history in buffer
	void  HistoryShow (void);

	// Set move/cut word delimiter, default is all not digital and alphabetic characters.
	void  SetDelimiter (const std::string &delim);

	bool isdelim(const char ch);

	// Read a character from terminal without echo
	int	 Getch (void);


	/*
	 * Completion APIs
	 */
	// C++ functions
	void CompleteStart(CrosslineCompletions &pCompletions, const int start, const int end);
	void CompleteAdd(CrosslineCompletions &pCompletions, const std::string &word, const std::string &help);

	// Add completion in callback. Word is must, help for word is optional.
	// start, end denotes the position in the current buffer to replace with the completion
	void  CompletionAdd (CrosslineCompletions &pCompletions, const std::string &word, const std::string &help);

	// Add completion with color.
	void CompletionAddColor (CrosslineCompletions &pCompletions, const std::string &word, 
										  crossline_color_e wcolor, const std::string &help, crossline_color_e hcolor);

	// Set syntax hints in callback
	void  HintsSet (CrosslineCompletions &pCompletions, const std::string &hints);

	// Set syntax hints with color
	void  HintsSetColor (CrosslineCompletions &pCompletions, const std::string &hints, crossline_color_e color);

	/*
	 * Paging APIs
	 */

	// Enable/Disble paging control
	int PagingSet (int enable);

	// Check paging after print a line, return 1 means quit, 0 means continue
	// if you know only one line is printed, just give line_len = 1
	int  PagingCheck (int line_len);

	/* 
	 * Cursor APIs
	 */

	// show or hide the cursor
	void ShowCursor(const bool show);

	// Get screen rows and columns
	void ScreenGet (int &pRows, int &pCols);

	// Clear current screen
	void ScreenClear (void);

	// Get cursor postion (0 based)
	bool  CursorGet (int &pRow, int &pCol);

	// Set cursor postion (0 based)
	void CursorSet (const int row, int const col);

	// Move cursor with row and column offset, row_off>0 move up row_off lines, <0 move down abs(row_off) lines
	// =0 no move for row, similar with col_off
	void CursorMove (const int row_off, const int col_off);

	/* 
	 * Color APIs
	 */

	// Set text color, CROSSLINE_COLOR_DEFAULT will revert to default setting
	// `\t` is not supported in Linux terminal, same below. Don't use `\n` in Linux terminal, same below.
	void ColorSet (crossline_color_e color);

	// Set default prompt color
	void PromptColorSet (crossline_color_e color);

};


#endif /* __CROSSLINE_H */
