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

class MyCrossline : public Crossline {
public:
    // Complete the string in inp, return match in completions and the prefix that was matched in pref, called when the user presses tab
    virtual bool Completer(const std::string &inp, const int pos, CrosslineCompletions &completions);
    
};

bool MyCrossline::Completer(const std::string &buf, const int pos, CrosslineCompletions &completions)
{
    static std::vector<std::string> cmd = {"insert", "select", "update", "delete", "create", "drop", "show",
                                            "describe", "help", "exit", "history"};

    // find the current word
    int pos1 = pos - 1;    // pos is the next character - we want to start to the left at pos-1
    int spPos = 0;
    if (pos1 < 0) {
        // we are at the start of the line
        spPos = 0;
    } else {
       spPos = buf.rfind(" ", pos1);   
        if (spPos == buf.npos) {
            // no space
            spPos = 0;
        } else {
            spPos += 1;
        }
    }
    std::string search = buf.substr(spPos, pos-spPos);

    completions.Setup(spPos, pos);
    for (int i = 0; i < cmd.size(); ++i) {
        if (cmd[i].find(search) == 0) {
            completions.Add(cmd[i], "", false);
        }
    }
    return completions.Size();
}

int main ()
{
    MyCrossline cLine;    
    cLine.HistoryLoad ("history.txt");

    std::string buf;
    while (cLine.ReadLine("Crossline> ", buf)) {
        printf ("Read line: \"%s\"\n", buf.c_str());
    }    

    cLine.HistorySave ("history.txt");
    return 0;
}