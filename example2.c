/*

Build

# Windows MSVC
cl -D_CRT_SECURE_NO_WARNINGS -W4 User32.Lib crossline.c example2.c /Feexample2.exe

# Windows Clang
clang -D_CRT_SECURE_NO_WARNINGS -Wall -lUser32 crossline.c example2.c -o example2.exe

# Linux Clang
clang -Wall crossline.c example2.c -o example2

# GCC(Linux, MinGW, Cygwin, MSYS2)
gcc -Wall crossline.c example2.c -o example2

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "crossline.h"

#ifdef _WIN32
	#define strcasecmp				_stricmp
	#define strncasecmp				_strnicmp
#endif

class MyCrossline : public Crossline {
public:
    // Complete the string in inp, return match in completions and the prefix that was matched in pref, called when the user presses tab
    virtual bool Completer(const std::string &inp, const int pos, CrosslineCompletions &completions);
    
};

bool MyCrossline::Completer(const std::string &buf, const int pos, CrosslineCompletions &completions)
{
	int i;
	crossline_color_e wcolor, hcolor;
	static const char *cmd[] = {"INSERT", "SELECT", "UPDATE", "DELETE", "CREATE", "DROP", "SHOW", "DESCRIBE", "help", "exit", "history", "paging", "color", NULL};
	static const char* cmd_help[] = {
		"Insert a record to table ",
		"Select records from table",
		"Update records in table  ",
		"Delete records from table",
		"Create index on table    ",
		"Drop index or table      ",
		"Show tables or databases ",
		"Show table schema        ",
		"Show help for topic      ",
		"Exit shell               ",
		"Show history             ",
		"Do paing APIs test       ",
		"Do Color APIs test       "};

	for (i = 0; NULL != cmd[i]; ++i) {
		if (0 == strncasecmp(buf.c_str(), cmd[i], buf.length())) {
			if (i < 8) { 
				wcolor = CROSSLINE_FGCOLOR_BRIGHT | CROSSLINE_FGCOLOR_YELLOW; 
			} else { 
				wcolor = CROSSLINE_FGCOLOR_BRIGHT | CROSSLINE_FGCOLOR_CYAN; 
			}
			hcolor = i%2 ? CROSSLINE_FGCOLOR_WHITE : CROSSLINE_FGCOLOR_CYAN;
			completions.Add(cmd[i], cmd_help[i], wcolor, hcolor);
		}
	}
	return completions.Size();
}

static void pagint_test (Crossline &cLine)
{
	int i;
	cLine.crossline_paging_set (1);
	for (i = 0; i < 256; ++i) {
		printf ("Paging test: %3d\n", i);
		if (cLine.crossline_paging_check (sizeof("paging test: ") + 3)) {
			break;
		}
	}
}

static void color_test (Crossline &cLine)
{
	printf ("\n*** Color test *** \n");
	printf ("  Default Foregroud and Backgroud\n\n");

	cLine.crossline_color_set (CROSSLINE_FGCOLOR_BLACK | CROSSLINE_BGCOLOR_WHITE);
	printf ("  Foregroud: Black");
	cLine.crossline_color_set (CROSSLINE_COLOR_DEFAULT);
	printf ("\n");	
	cLine.crossline_color_set (CROSSLINE_UNDERLINE | CROSSLINE_FGCOLOR_RED);
	printf ("  Foregroud: Red Underline\n");
	cLine.crossline_color_set (CROSSLINE_FGCOLOR_GREEN);
	printf ("  Foregroud: Green\n");
	cLine.crossline_color_set (CROSSLINE_FGCOLOR_YELLOW);
	printf ("  Foregroud: Yellow\n");
	cLine.crossline_color_set (CROSSLINE_FGCOLOR_BLUE);
	printf ("  Foregroud: Blue\n");
	cLine.crossline_color_set (CROSSLINE_FGCOLOR_MAGENTA);
	printf ("  Foregroud: Magenta\n");
	cLine.crossline_color_set (CROSSLINE_FGCOLOR_CYAN);
	printf ("  Foregroud: Cyan\n");
	cLine.crossline_color_set (CROSSLINE_FGCOLOR_WHITE | CROSSLINE_BGCOLOR_BLACK);
	printf ("  Foregroud: White");
	cLine.crossline_color_set (CROSSLINE_COLOR_DEFAULT);
	printf ("\n\n");	

	cLine.crossline_color_set (CROSSLINE_FGCOLOR_BRIGHT | CROSSLINE_FGCOLOR_BLACK | CROSSLINE_BGCOLOR_WHITE);
	printf ("  Foregroud: Bright Black");
	cLine.crossline_color_set (CROSSLINE_COLOR_DEFAULT);
	printf ("\n");	
	cLine.crossline_color_set (CROSSLINE_FGCOLOR_BRIGHT | CROSSLINE_FGCOLOR_RED);
	printf ("  Foregroud: Bright Red\n");
	cLine.crossline_color_set (CROSSLINE_FGCOLOR_BRIGHT | CROSSLINE_FGCOLOR_GREEN);
	printf ("  Foregroud: Bright Green\n");
	cLine.crossline_color_set (CROSSLINE_FGCOLOR_BRIGHT | CROSSLINE_FGCOLOR_YELLOW);
	printf ("  Foregroud: Bright Yellow\n");
	cLine.crossline_color_set (CROSSLINE_FGCOLOR_BRIGHT | CROSSLINE_FGCOLOR_BLUE);
	printf ("  Foregroud: Bright Blue\n");
	cLine.crossline_color_set (CROSSLINE_FGCOLOR_BRIGHT | CROSSLINE_FGCOLOR_MAGENTA);
	printf ("  Foregroud: Bright Magenta\n");
	cLine.crossline_color_set (CROSSLINE_UNDERLINE | CROSSLINE_FGCOLOR_BRIGHT | CROSSLINE_FGCOLOR_CYAN);
	printf ("  Foregroud: Bright Cyan Underline\n");
	cLine.crossline_color_set (CROSSLINE_FGCOLOR_BRIGHT | CROSSLINE_FGCOLOR_WHITE | CROSSLINE_BGCOLOR_BLACK);
	printf ("  Foregroud: Bright White\n\n");

	cLine.crossline_color_set (CROSSLINE_FGCOLOR_WHITE | CROSSLINE_BGCOLOR_BLACK);
	printf ("  Backgroud: Black   ");
	cLine.crossline_color_set (CROSSLINE_COLOR_DEFAULT);
	printf ("\n");	
	cLine.crossline_color_set (CROSSLINE_FGCOLOR_WHITE | CROSSLINE_BGCOLOR_RED);
	printf ("  Backgroud: Red     ");
	cLine.crossline_color_set (CROSSLINE_COLOR_DEFAULT);
	printf ("\n");	
	cLine.crossline_color_set (CROSSLINE_FGCOLOR_WHITE | CROSSLINE_BGCOLOR_GREEN);
	printf ("  Backgroud: Green   ");
	cLine.crossline_color_set (CROSSLINE_COLOR_DEFAULT);
	printf ("\n");	
	cLine.crossline_color_set (CROSSLINE_FGCOLOR_WHITE | CROSSLINE_BGCOLOR_YELLOW);
	printf ("  Backgroud: Yellow  ");
	cLine.crossline_color_set (CROSSLINE_COLOR_DEFAULT);
	printf ("\n");	
	cLine.crossline_color_set (CROSSLINE_FGCOLOR_WHITE | CROSSLINE_BGCOLOR_BLUE);
	printf ("  Backgroud: Blue    ");
	cLine.crossline_color_set (CROSSLINE_COLOR_DEFAULT);
	printf ("\n");	
	cLine.crossline_color_set (CROSSLINE_FGCOLOR_WHITE | CROSSLINE_BGCOLOR_MAGENTA);
	printf ("  Backgroud: Magenta ");
	cLine.crossline_color_set (CROSSLINE_COLOR_DEFAULT);
	printf ("\n");	
	cLine.crossline_color_set (CROSSLINE_FGCOLOR_WHITE | CROSSLINE_BGCOLOR_CYAN);
	printf ("  Backgroud: Cyan    ");
	cLine.crossline_color_set (CROSSLINE_COLOR_DEFAULT);
	printf ("\n");	
	cLine.crossline_color_set (CROSSLINE_FGCOLOR_BRIGHT | CROSSLINE_FGCOLOR_BLACK | CROSSLINE_BGCOLOR_WHITE);
	printf ("  Backgroud: White   ");
	cLine.crossline_color_set (CROSSLINE_COLOR_DEFAULT);
	printf ("\n\n");	

	cLine.crossline_color_set (CROSSLINE_FGCOLOR_WHITE | CROSSLINE_BGCOLOR_BRIGHT | CROSSLINE_BGCOLOR_BLACK);
	printf ("  Backgroud: Bright Black   ");
	cLine.crossline_color_set (CROSSLINE_COLOR_DEFAULT);
	printf ("\n");	
	cLine.crossline_color_set (CROSSLINE_FGCOLOR_WHITE | CROSSLINE_BGCOLOR_BRIGHT | CROSSLINE_BGCOLOR_RED);
	printf ("  Backgroud: Bright Red     ");
	cLine.crossline_color_set (CROSSLINE_COLOR_DEFAULT);
	printf ("\n");	
	cLine.crossline_color_set (CROSSLINE_FGCOLOR_BRIGHT | CROSSLINE_FGCOLOR_BLACK | CROSSLINE_BGCOLOR_BRIGHT | CROSSLINE_BGCOLOR_GREEN);
	printf ("  Backgroud: Bright Green   ");
	cLine.crossline_color_set (CROSSLINE_COLOR_DEFAULT);
	printf ("\n");	
	cLine.crossline_color_set (CROSSLINE_FGCOLOR_BRIGHT | CROSSLINE_FGCOLOR_BLACK | CROSSLINE_BGCOLOR_BRIGHT | CROSSLINE_BGCOLOR_YELLOW);
	printf ("  Backgroud: Bright Yellow  ");
	cLine.crossline_color_set (CROSSLINE_COLOR_DEFAULT);
	printf ("\n");	
	cLine.crossline_color_set (CROSSLINE_FGCOLOR_WHITE | CROSSLINE_BGCOLOR_BRIGHT | CROSSLINE_BGCOLOR_BLUE);
	printf ("  Backgroud: Bright Blue    ");
	cLine.crossline_color_set (CROSSLINE_COLOR_DEFAULT);
	printf ("\n");	
	cLine.crossline_color_set (CROSSLINE_FGCOLOR_WHITE | CROSSLINE_BGCOLOR_BRIGHT | CROSSLINE_BGCOLOR_MAGENTA);
	printf ("  Backgroud: Bright Magenta ");
	cLine.crossline_color_set (CROSSLINE_COLOR_DEFAULT);
	printf ("\n");	
	cLine.crossline_color_set (CROSSLINE_FGCOLOR_BRIGHT | CROSSLINE_FGCOLOR_BLACK | CROSSLINE_BGCOLOR_BRIGHT | CROSSLINE_BGCOLOR_CYAN);
	printf ("  Backgroud: Bright Cyan    ");
	cLine.crossline_color_set (CROSSLINE_COLOR_DEFAULT);
	printf ("\n");	
	cLine.crossline_color_set (CROSSLINE_FGCOLOR_BRIGHT | CROSSLINE_FGCOLOR_BLACK | CROSSLINE_BGCOLOR_BRIGHT | CROSSLINE_BGCOLOR_WHITE);
	printf ("  Backgroud: Bright White   ");
	cLine.crossline_color_set (CROSSLINE_COLOR_DEFAULT);
	printf ("\n");	
}

int main ()
{
	std::string buf="select ";

	MyCrossline cLine;
	// crossline_completion_register (completion_hook);
	cLine.HistoryLoad ("history.txt");
	cLine.crossline_prompt_color_set (CROSSLINE_FGCOLOR_BRIGHT | CROSSLINE_FGCOLOR_GREEN);

	// Readline with initail text input
	if (cLine.ReadLine ("Crossline> ", buf)) {
		printf ("Read line: \"%s\"\n", buf);
	}
	// Readline loop
	while (cLine.ReadLine ("Crossline> ", buf)) {
		printf ("Read line: \"%s\"\n", buf.c_str());

		if (!strcmp (buf.c_str(), "history")) {
			cLine.HistoryShow ();
		}

		if (!strcmp (buf.c_str(), "paging")) {
			pagint_test (cLine);
		}
		if (!strcmp (buf.c_str(), "color")) {
			color_test (cLine);
		}
	}

	cLine.HistorySave ("history.txt");
	return 0;
}
