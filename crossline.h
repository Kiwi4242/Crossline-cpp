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
	CROSSLINE_FGCOLOR_BLUEGREEN    	= 0x09,
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

// Class for reading and writing to the console
class Crossline {

protected:

public:
	CrosslinePrivate *info;

	Crossline();

    // Complete the string in inp, return match in completions and the prefix that was matched, called when the user presses tab
    virtual bool Completer(const std::string &inp, const int pos, CrosslineCompletions &completions) = 0;

    void Printf(const std::string &fmt, const std::string st="");
    void Print(const std::string &msg);

    // ESC clears
    void AllowESCCombo(const bool);
    
	// Main API to read input
    std::string ReadLine(const std::string &prompt);

    // Hook to change the processing of the input
    bool ReadHook(const char ch);

	// Helper API to read a line, return buf if get line, return false if EOF.
	bool crossline_readline (const std::string &prompt, std::string &buf);

	// Same with crossline_readline except buf holding initial input for editing.
	bool crossline_readline2 (const std::string &prompt, std::string &buf);


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
    virtual HistoryItemPtr GetHistoryItem(const ssize_t n, const bool forward);
    virtual void HistoryDelete(const ssize_t ind, const ssize_t n);
    virtual void HistoryAdd(const HistoryItemPtr &st);
    virtual void HistoryAdd(const std::string &st);

    // search the history
	int crossline_history_search (std::string &input);
	// Show history in buffer
	void  crossline_history_show (void);

	// Set move/cut word delimiter, default is all not digital and alphabetic characters.
	void  crossline_delimiter_set (const std::string &delim);

	// Read a character from terminal without echo
	int	 crossline_getch (void);



	/*
	 * Completion APIs
	 */
	// C++ functions
	void CompleteStart(CrosslineCompletions &pCompletions, const int start, const int end);
	void CompleteAdd(CrosslineCompletions &pCompletions, const std::string &word, const std::string &help);

	// Register completion callback
	void  crossline_completion_register (crossline_completion_callback pCbFunc);

	// Add completion in callback. Word is must, help for word is optional.
	// start, end denotes the position in the current buffer to replace with the completion
	void  crossline_completion_add (CrosslineCompletions &pCompletions, const std::string &word, const std::string &help);

	// Add completion with color.
	void  crossline_completion_add_color (CrosslineCompletions &pCompletions, const std::string &word, 
										  crossline_color_e wcolor, const std::string &help, crossline_color_e hcolor);

	// Set syntax hints in callback
	void  crossline_hints_set (CrosslineCompletions &pCompletions, const std::string &hints);

	// Set syntax hints with color
	void  crossline_hints_set_color (CrosslineCompletions &pCompletions, const std::string &hints, crossline_color_e color);

	/*
	 * Paging APIs
	 */

	// Enable/Disble paging control
	int crossline_paging_set (int enable);

	// Check paging after print a line, return 1 means quit, 0 means continue
	// if you know only one line is printed, just give line_len = 1
	int  crossline_paging_check (int line_len);


	/* 
	 * Cursor APIs
	 */

	// Get screen rows and columns
	void crossline_screen_get (int *pRows, int *pCols);

	// Clear current screen
	void crossline_screen_clear (void);

	// Get cursor postion (0 based)
	int  crossline_cursor_get (int *pRow, int *pCol);

	// Set cursor postion (0 based)
	void crossline_cursor_set (int row, int col);

	// Move cursor with row and column offset, row_off>0 move up row_off lines, <0 move down abs(row_off) lines
	// =0 no move for row, similar with col_off
	void crossline_cursor_move (int row_off, int col_off);

	// Hide or show cursor
	void crossline_cursor_hide (int bHide);


	/* 
	 * Color APIs
	 */

	// Set text color, CROSSLINE_COLOR_DEFAULT will revert to default setting
	// `\t` is not supported in Linux terminal, same below. Don't use `\n` in Linux terminal, same below.
	void crossline_color_set (crossline_color_e color);

	// Set default prompt color
	void crossline_prompt_color_set (crossline_color_e color);

};


#endif /* __CROSSLINE_H */
