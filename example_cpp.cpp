/*

Example of Crossline-cpp library
* Copyright (c) 2024, John Burnell

Build directly

# Windows MSVC
cl -D_CRT_SECURE_NO_WARNINGS -W4 User32.Lib crossline.c example.c /Feexample.exe

# Windows Clang
clang -D_CRT_SECURE_NO_WARNINGS -Wall -lUser32 crossline.c example.c -o example.exe

# Linux Clang
clang -Wall crossline.c example.c -o example

# GCC(Linux, MinGW, Cygwin, MSYS2)
gcc -Wall crossline.c example.c -o example

or 

Use CMake to configure and build
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "crossline.h"


class MyCompleter : public CompleterClass {
public:
    // Complete the string in inp, return match in completions and the prefix that was matched in pref, called when the user presses tab
    bool FindItems(const std::string &buf, Crossline &cLine, const int pos);

};


bool MyCompleter::FindItems(const std::string &buf, Crossline &cLine, const int pos)
{
    std::vector<std::string> files = {"Some File Name.dat", "SomeOtherName.txt", "F1.dat", "F2.dat", "F3.dat", "F4.dat",
                                     "F5.dat", "F6.dat", "F7.dat", "F8.dat", "F9.dat", "F10.dat", "F11.dat"};

    int spPos = buf.rfind(" ", pos);
    if (spPos == buf.npos) {
        spPos = 0;
    } else {
        spPos += 1;
    }
    std::string search = buf.substr(spPos, pos-spPos);

    Setup(spPos, pos);
    for (int i = 0; i < files.size(); ++i) {
        std::string file = files[i];
        bool needQuotes = false;
        if (file.find(search) == 0) {
            if (file.find(" ") != file.npos) {
            //     file = "\"" + file + "\"";
                needQuotes = true;
            }
            Add(file, "", needQuotes);
        }
    }
    return Size() > 0;
}


int main ()
{
    MyCompleter *comp = new MyCompleter();
    HistoryClass *his = new HistoryClass();
    Crossline cLine(comp, his);

    // cLine.crossline_completion_register (completion_hook);
    his->HistoryLoad ("history.txt");

    cLine.PromptColorSet(CROSSLINE_FGCOLOR_CYAN | CROSSLINE_FGCOLOR_BRIGHT);
    cLine.AllowESCCombo(false);

    std::string buf;
    while (cLine.ReadLine ("Crossline> ", buf)) {
        printf ("Read line: \"%s\"\n", buf.c_str());
    }    

    his->HistorySave ("history.txt");
    return 0;
}