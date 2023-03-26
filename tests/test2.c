#include <stdio.h>

int main(void) {
    unsigned char *p = "$123\r\n";
    int len = 0;
    int tmp = 0;
    p++;
    while (*p != '\r') {
        tmp = *p - '0';
        len = (len * 10) + tmp;
        p++;
    }

    /* Now p points at '\r', and the len is in bulk_len. */
    printf("%d\n", len);
    return 0;
}