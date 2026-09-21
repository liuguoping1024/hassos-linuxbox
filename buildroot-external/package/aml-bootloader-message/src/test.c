#include <stdio.h>
#include <stdlib.h>

#include "bootloader_message.h"

void usage() {
        printf("ab system select!!!");
        printf("slot get---get current slot\n");
        printf("slot set---set slot\n");
        printf("example: test_ab get\n");
        printf("example: test_ab set _a\n");
        printf("example: test_ab set _b\n");
}

int main(int argc, char **argv) {

    int ret = 0;
    if ((argc != 2) && (argc != 3))
    {
        usage();
        return -1;
    }
    if (argc == 3)
    {
        if (!strcmp(argv[1], "set"))
        {
            if (!strcmp(argv[2],"_a"))
            {
                ret = set_active_slot(0);
                if (ret != 0)
                {
                    printf("set otapath : %s failed!\n", argv[1]);
                    return -1;
                }
            }
            else if (!strcmp(argv[2],"_b"))
            {
                ret = set_active_slot(1);
                if (ret != 0)
                {
                    printf("set otapath : %s failed!\n", argv[1]);
                    return -1;
                }
            }
            else
            {
                usage();
                return -1;
            }
        }
    }
    if (!strcmp(argv[1], "get"))
    {
        int active = 0;
        get_active_slot_from_misc(&active);
        printf("active is %d\n", active);
    }

    return 0;
}
