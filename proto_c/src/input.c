#include "pch.h"

// TODO (scottnm): replace _kbhit with something portable
#include <conio.h> // _kbhit

raw_key_t
get_raw_key()
{
    if (!_kbhit())
    {
        return (raw_key_t){0};
    }

    const int firstPoll = _getch();
    const bool isSpecialKey = firstPoll == 0;

    int keyCode = isSpecialKey ? _getch() : firstPoll;
    assert(keyCode != 0);
    return (raw_key_t) { .hit = true, .keyCode = keyCode };
}
