#include <stdio.h>

int main(int argc, char* argv[]) {
    for (int i = 0; i < 10; ++i) {
        int counter = 0;
        while (counter < 10) {
            printf("For: %d; While: %d\n", i, counter);
            counter++;
        }
    }

    return 0;
}
