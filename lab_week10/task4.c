#include <math.h>
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

#define TAG 0

int main() {
    FILE *fp;
    float a_coeff, b_coeff, c_coeff, x1, x2, disc;
    float x1r, x1i, x2r, x2i;

    int rank, size;
    int num_rows, counter = 0;
    MPI_Status status;
    MPI_Init(NULL, NULL);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    // WRITE PART(a) HERE
    if (size != 3) {
        if (rank == 0) printf("Must be 3 processors only\n");
        MPI_Finalize();
        exit(0);
    }
    switch (rank) {
        case 0: {
            // CONTINUE WITH PART (a) HERE
            fp = fopen("quad.txt", "r");
            if (!fp) {
                printf("No quad.txt file\n");
                MPI_Abort(MPI_COMM_WORLD, 0);
            }

            fscanf(fp, "%d\n", &num_rows);
            fscanf(fp, "a b c\n");

            // how many rows to expect
            MPI_Send(&num_rows, 1, MPI_INT, 1, TAG, MPI_COMM_WORLD);

            float buf[3];
            while (counter < num_rows) {
                fscanf(fp, "%f %f %f\n", &a_coeff, &b_coeff, &c_coeff);
                disc = (b_coeff * b_coeff) - (4 * a_coeff * c_coeff);

                buf[0] = a_coeff;
                buf[1] = b_coeff;
                buf[2] = disc;
                MPI_Send(buf, 3, MPI_FLOAT, 1, TAG, MPI_COMM_WORLD);
                ++counter;
            }

            fclose(fp);
            break;
        }
        case 1: {
            // WRITE PART (b) HERE
            // how many rows to expect
            MPI_Recv(&num_rows, 1, MPI_INT, 0, TAG, MPI_COMM_WORLD,
                     MPI_STATUS_IGNORE);
            // pass along to next processor
            MPI_Send(&num_rows, 1, MPI_INT, 2, TAG, MPI_COMM_WORLD);

            float buf[5];
            while (counter < num_rows) {
                // receive a, b and discriminator
                MPI_Recv(buf, 3, MPI_FLOAT, 0, TAG, MPI_COMM_WORLD,
                         MPI_STATUS_IGNORE);
                a_coeff = buf[0] * 2.0f;
                b_coeff = buf[1] * -1.0f;
                disc = buf[2];

                if (disc < 0.0f) {
                    // complex
                    x1r = x2r = b_coeff / a_coeff;
                    x1i = sqrt(fabsf(disc)) / a_coeff;
                    x2i = -1.0f * x1i;
                    buf[0] = 0.0f;
                    buf[1] = x1r;
                    buf[2] = x2r;
                    buf[3] = x1i;
                    buf[4] = x2i;
                } else {
                    // normal
                    x1 = (b_coeff + sqrtf(disc)) / a_coeff;
                    x2 = (b_coeff - sqrtf(disc)) / a_coeff;
                    buf[0] = 1.0f;
                    buf[1] = x1;
                    buf[2] = x2;
                }

                MPI_Send(buf, 5, MPI_FLOAT, 2, TAG, MPI_COMM_WORLD);

                ++counter;
            }

            break;
        }
        case 2: {
            // WRITE PART (c) HERE
            fp = fopen("roots.txt", "w");
            if (!fp) {
                printf("Couldn't open roots.txt\n");
                MPI_Abort(MPI_COMM_WORLD, 0);
            }

            MPI_Recv(&num_rows, 1, MPI_INT, 1, TAG, MPI_COMM_WORLD,
                     MPI_STATUS_IGNORE);

            fprintf(fp, "%d\nx1 x2 x1_real x1_img x2_real x2_img\n", num_rows);

            float buf[5];
            while (counter < num_rows) {
                MPI_Recv(buf, 5, MPI_FLOAT, 1, TAG, MPI_COMM_WORLD,
                         MPI_STATUS_IGNORE);

                if (buf[0] == 0.0f) {
                    // complex
                    x1r = buf[1];
                    x2r = buf[2];
                    x1i = buf[3];
                    x2i = buf[4];
                    fprintf(fp, "N N %.2f %.2f %.2f %.2f\n", x1r, x1i, x2r,
                            x2i);
                } else {
                    // real
                    x1 = buf[1];
                    x2 = buf[2];
                    fprintf(fp, "%.2f %.2f N N N N\n", x1, x2);
                }

                ++counter;
            }

            fclose(fp);
            break;
        }
    }
    MPI_Finalize();
    return 0;
}
