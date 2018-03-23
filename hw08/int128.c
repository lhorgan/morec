#include <stdio.h>
#include <stdint.h>

#include "string.h"
#include "int128.h"

int128_t
atoh(char* text)
{
    int128_t yy = 0;
    for (int ii = 0; text[ii]; ++ii) {
        int8_t d = text[ii] - '0';
        yy *= 10;
        yy += d;
    }
    return yy;
}

static
void
reverse(char* ss)
{
    int ii = 0;
    int jj = strlen(ss) - 1;

    while (ii < jj) {
        char tmp = ss[ii];
        ss[ii] = ss[jj];
        ss[jj] = tmp;
        ++ii;
        --jj;
    }
}

void
print_int128(int128_t xx)
{
    char text[40] = {0};

    int ii = 0;
    while (xx > 0) {
        int d = xx % 10;
        text[ii++] = d + '0';
        xx = xx / 10;
    }

    reverse(text);

    printf("%s", text);
}
