#include <stdio.h>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Improper no. of arguments\n");
        return 1;
    }

    switch (*argv[1]) {
        case '1': {
            printf("one\n");
            break;
        }
        case '2': {
            printf("two\n");
            break;
        }
        case '3': {
            printf("three\n");
            break;
        }
        default: {
            printf("%c\n", *argv[1]);
        }
    }

    if (*argv[1] > '1') {
        printf("greater than char '1'\n");
    } else {
        printf("less than or equal to char '1'\n");
    }

    printf("Done\n");

    return 0;
}
