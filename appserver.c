// appserver.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "Bank.h"

int main()
{
    printf("Initing db\n");
    int ret = initialize_accounts(50);

    if (ret != 1) {
        printf("fail\n");
    } else {
        printf("gr8 success\n");
    }
    exit(0);
}
