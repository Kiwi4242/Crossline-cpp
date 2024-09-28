/*

Build

# Windows MSVC
cl -D_CRT_SECURE_NO_WARNINGS -W4 User32.Lib crossline.c example.c /Feexample.exe

# Windows Clang
clang -D_CRT_SECURE_NO_WARNINGS -Wall -lUser32 crossline.c example.c -o example.exe

# Linux Clang
clang -Wall crossline.c example.c -o example

# GCC(Linux, MinGW, Cygwin, MSYS2)
gcc -Wall crossline.c example.c -o example

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "crossline.h"

static void completion_hook (const std::string &buf, Crossline &cLine, const int pos, CrosslineCompletions &pCompletion)
{
    std::vector<std::string> files = {"NG28PT.xlsx", "NG30PT.xslx"};
    //static std::vector<std::string> cmd = {"insert", "select", "update", "delete", "create", "drop", "show",
    //                                        "describe", "help", "exit", "history"};

    int spPos = buf.rfind(" ", pos);
    if (spPos == buf.npos) {
        spPos = 0;
    } else {
        spPos += 1;
    }
    std::string search = buf.substr(spPos, pos-spPos);

    for (int i = 0; i < files.size(); ++i) {
        if (files[i].find(search) == 0) {
            cLine.crossline_completion_add (pCompletion, files[i], "", spPos, pos);
        }
    }

}

int main ()
{
    Crossline cLine;    
    cLine.crossline_completion_register (completion_hook);
    cLine.crossline_history_load ("history.txt");

    std::string buf;
    while (cLine.crossline_readline ("Crossline> ", buf)) {
        printf ("Read line: \"%s\"\n", buf.c_str());
    }    

    cLine.crossline_history_save ("history.txt");
    return 0;
}