/*

This Example implements a simple SQL syntax parser.
	INSERT INTO <table> SET column1=value1,column2=value2,...
	SELECT <* | column1,columnm2,...> FROM <table> [WHERE] [ORDER BY] [LIMIT] [OFFSET]
	UPDATE <table> SET column1=value1,column2=value2 [WHERE] [ORDER BY] [LIMIT] [OFFSET]
	DELETE FROM <table> [WHERE] [ORDER BY] [LIMIT] [OFFSET]
	CREATE [UNIQUE] INDEX <name> ON <table> (column1,column2,...)
	DROP {TABLE | INDEX} <name>
	SHOW {TABLES | DATABASES}
	DESCRIBE <TABLE>
	help {INSERT | SELECT | UPDATE | DELETE | CREATE | DROP | SHOW | DESCRIBE | help | exit | history}


Build

# Windows MSVC
cl -D_CRT_SECURE_NO_WARNINGS -W4 User32.Lib crossline.c example_sql.c /Feexample_sql.exe

# Windows Clang
clang -D_CRT_SECURE_NO_WARNINGS -Wall -lUser32 crossline.c example_sql.c -o example_sql.exe

# Linux Clang
clang -Wall crossline.c example_sql.c -o example_sql

# GCC(Linux, MinGW, Cygwin, MSYS2)
gcc -Wall crossline.c example_sql.c -o example_sql

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "crossline.h"

#ifdef _WIN32
	#define strcasecmp				_stricmp
	#define strncasecmp				_strnicmp
#endif

class SQLCrossline : public Crossline {
public:
    // Complete the string in inp, return match in completions and the prefix that was matched in pref, called when the user presses tab
    virtual bool Completer(const std::string &inp, const int pos, CrosslineCompletions &completions);
    
};

void sql_add_completion (Crossline &cLine, CrosslineCompletions &completions, const char *prefix, const char **match, const char **help)
{
	int i, len = (int)strlen(prefix);
	crossline_color_e wcolor, hcolor;
	for (i = 0;  NULL != match[i]; ++i) {
		if (0 == strncasecmp(prefix, match[i], len)) {
			if (NULL != help) {
				if (i < 8) { 
					wcolor = CROSSLINE_FGCOLOR_BRIGHT | CROSSLINE_FGCOLOR_YELLOW; 
				} else { 
					wcolor = CROSSLINE_FGCOLOR_BRIGHT | CROSSLINE_FGCOLOR_CYAN; 
				}
				hcolor = i%2 ? CROSSLINE_FGCOLOR_WHITE : CROSSLINE_FGCOLOR_CYAN;
				cLine.crossline_completion_add_color (completions, match[i], wcolor, help[i], hcolor);
			} else {
				cLine.crossline_completion_add_color (completions, match[i], 
												CROSSLINE_FGCOLOR_BRIGHT | CROSSLINE_FGCOLOR_MAGENTA, "", 0);
			}
		}
	}
}

int sql_find_key (const char **match, const char *prefix)
{
	int i;
	for (i = 0; NULL != match[i]; ++i) {
		if (0 == strcasecmp(prefix, match[i])) {
		  return i;
		}
	}
	return -1;
}

enum {
	CMD_INSERT,
	CMD_SELECT,
	CMD_UPDATE,
	CMD_DELETE,
	CMD_CREATE,
	CMD_DROP,
	CMD_SHOW,
	CMD_DESCRIBE,
	CMD_HELP,
	CMD_EXIT,
	CMD_HISTORY,
};

bool SQLCrossline::Completer(const std::string &buf, const int pos, CrosslineCompletions &completions)
{
	int	num, cmd, bUnique = 0;
	char split[8][128], last_ch;
	static const char* sql_cmd[] = {"INSERT", "SELECT", "UPDATE", "DELETE", "CREATE", "DROP", "SHOW", "DESCRIBE", "help", "exit", "history", NULL};
	static const char* sql_cmd_help[] = {
		"Insert a record to table",
		"Select records from table",
		"Update records in table",
		"Delete records from table",
		"Create index on table",
		"Drop index or table",
		"Show tables or databases",
		"Show table schema",
		"Show help for topic",
		"Exit shell",
		"Show history"};
	static const char* sql_caluse[] = {"WHERE", "ORDER BY", "LIMIT", "OFFSET", NULL};
	static const char* sql_index[]  = {"UNIQUE", "INDEX", NULL};
	static const char* sql_drop[]   = {"TABLE", "INDEX", NULL};
	static const char* sql_show[]   = {"TABLES", "DATABASES", NULL};
	crossline_color_e tbl_color = CROSSLINE_FGCOLOR_WHITE | CROSSLINE_BGCOLOR_GREEN;
	crossline_color_e col_color = CROSSLINE_FGCOLOR_WHITE | CROSSLINE_BGCOLOR_CYAN;	
	crossline_color_e idx_color = CROSSLINE_FGCOLOR_WHITE | CROSSLINE_BGCOLOR_YELLOW;

	memset (split, '\0', sizeof (split));
	num = sscanf (buf.c_str(), "%s %s %s %s %s %s %s %s", split[0], split[1], split[2], split[3], split[4], split[5], split[6], split[7]);
	cmd = sql_find_key (sql_cmd, split[0]);

	int first = 0;
	for (int i = 0; i < num; i++) {
		int l = strlen(split[i]) + 1;   // include space
		if (first + l > pos) {
			break;
		}
		first += l;
	}

	completions.Setup(first, pos);

	if ((cmd < 0) && (num <= 1)) {
		sql_add_completion (*this, completions, split[0], sql_cmd, sql_cmd_help);
	}
	last_ch = buf[buf.length() - 1];
	switch (cmd) {
	case CMD_INSERT: // INSERT INTO <table> SET column1=value1,column2=value2,...
		if ((1 == num) && (' ' == last_ch)) {
			crossline_completion_add (completions, "INTO", "");
		} else if ((2 == num) && (' ' == last_ch)) {
			crossline_hints_set_color (completions, "table name", tbl_color);
		} else if ((3 == num) && (' ' == last_ch)) {
			crossline_completion_add (completions, "SET", "");
		} else if ((4 == num) && (' ' == last_ch)) {
			crossline_hints_set_color (completions, "column1=value1,column2=value2,...", col_color);
		}
		break;
	case CMD_SELECT: // SELECT < * | column1,columnm2,...> FROM <table> [WHERE] [ORDER BY] [LIMIT] [OFFSET]
		if ((1 == num) && (' ' == last_ch)) {
			crossline_hints_set_color (completions, "* | column1,columnm2,...", col_color);
		} else if ((2 == num) && (' ' == last_ch)) {
			crossline_completion_add (completions, "FROM", "");
		} else if ((3 == num) && (' ' == last_ch)) {
			crossline_hints_set_color (completions, "table name", tbl_color);
		} else if ((4 == num) && (' ' == last_ch)) {
			sql_add_completion (*this, completions, "", sql_caluse, nullptr);
		} else if ((num > 4) && (' ' != last_ch)) {
			sql_add_completion (*this, completions, split[num-1], sql_caluse, nullptr);
		}
		break;
	case CMD_UPDATE: // UPDATE <table> SET column1=value1,column2=value2 [WHERE] [ORDER BY] [LIMIT] [OFFSET]
		if ((1 == num) && (' ' == last_ch)) {
			crossline_hints_set_color (completions, "table name", tbl_color);
		} else if ((2 == num) && (' ' == last_ch)) {
			crossline_completion_add (completions, "SET", "");
		} else if ((3 == num) && (' ' == last_ch)) {
			crossline_hints_set_color (completions, "column1=value1,column2=value2,...", col_color);
		} else if ((4 == num) && (' ' == last_ch)) {
			sql_add_completion (*this, completions, "", sql_caluse, nullptr);
		} else if ((num > 4) && (' ' != last_ch)) {
			sql_add_completion (*this, completions, split[num-1], sql_caluse, nullptr);
		}
		break;
	case CMD_DELETE: // DELETE FROM <table> [WHERE] [ORDER BY] [LIMIT] [OFFSET]
		if ((1 == num) && (' ' == last_ch)) {
			crossline_completion_add (completions, "FROM", "");
		} else if ((2 == num) && (' ' == last_ch)) {
			crossline_hints_set_color (completions, "table name", tbl_color);
		} else if ((3 == num) && (' ' == last_ch)) {
			sql_add_completion (*this, completions, "", sql_caluse, nullptr);
		} else if ((num > 3) && (' ' != last_ch)) {
			sql_add_completion (*this, completions, split[num-1], sql_caluse, nullptr);
		}
		break;
	case CMD_CREATE: // CREATE [UNIQUE] INDEX <name> ON <table> (column1,column2,...)
		if ((1 == num) && (' ' == last_ch)) {
			sql_add_completion (*this, completions, "", sql_index, nullptr);
		} else if ((2 == num) && (' ' != last_ch)) {
			sql_add_completion (*this, completions, split[1], sql_index, nullptr);
		} else {
			if ((num >= 2) && !strcasecmp (split[1], "UNIQUE")) {
				bUnique = 1;
			}
			if ((2 == num) && bUnique && (' ' == last_ch)) {
				crossline_completion_add (completions, "INDEX", "");
			}
			else if ((2+bUnique == num) && (' ' == last_ch)) {
				crossline_hints_set_color (completions, "index name", idx_color);
			} else if ((3+bUnique == num) && (' ' == last_ch)) {
				crossline_completion_add (completions, "ON", "");
			} else if ((4+bUnique == num) && (' ' == last_ch)) {
				crossline_hints_set_color (completions, "table name", tbl_color);
			} else if ((5+bUnique == num) && (' ' == last_ch)) {
				crossline_hints_set_color (completions, "(column1,column2,...)", col_color);
			}
		}
		break;
	case CMD_DROP:	// DROP TABLE <name>, DROP INDEX <name>
		if ((1 == num) && (' ' == last_ch)) {
			sql_add_completion (*this, completions, "", sql_drop, nullptr);
		} else if ((2 == num) && (' ' != last_ch)) {
			sql_add_completion (*this, completions, split[1], sql_drop, nullptr);
		} else if ((num == 2) && (' ' == last_ch)) {
			if (!strcasecmp (split[1], "TABLE")) {
				crossline_hints_set_color (completions, "table name", tbl_color);
			} else if (!strcasecmp (split[1], "INDEX")) {
				crossline_hints_set_color (completions, "index name", idx_color);
			}
		}
		break;
	case CMD_SHOW: // SHOW TABLES, SHOW DATABASES
		if ((1 == num) && (' ' == last_ch)) {
			sql_add_completion (*this, completions, "", sql_show, nullptr);
		} else if ((2 == num) && (' ' != last_ch)) {
			sql_add_completion (*this, completions, split[1], sql_show, nullptr);
		}
		break;
	case CMD_DESCRIBE: // describe <table>
		if (' ' == last_ch) {
			crossline_hints_set_color (completions, "table name", tbl_color);
		}
		break;
	case CMD_HELP:
		if ((1 == num) && (' ' == last_ch)) {
			sql_add_completion (*this, completions, "", sql_cmd, nullptr);
		} else if ((2 == num) && (' ' != last_ch)) {
			sql_add_completion (*this, completions, split[1], sql_cmd, nullptr);
		}
		break;
	case CMD_EXIT:
		break;
	}
	return completions.Size();
}

int main ()
{

	SQLCrossline cLine;

	// crossline_completion_register (sql_completion_hook);
	cLine.HistoryLoad ("history.txt");
	cLine.crossline_prompt_color_set (CROSSLINE_FGCOLOR_BRIGHT | CROSSLINE_FGCOLOR_GREEN);

	std::string buf;
	while (cLine.ReadLine ("SQL> ", buf)) {
		printf ("Read line: \"%s\"\n", buf.c_str());
		if (!strcmp (buf.c_str(), "history")) {
			cLine.HistoryShow ();
		} else if (!strcmp (buf.c_str(), "exit")) {
			break;
		}
	}

	cLine.HistorySave ("history.txt");
	return 0;
}
