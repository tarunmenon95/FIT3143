#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <memory.h>
#include "mpi.h"

#define SENTINEL 0.0

int main(int argc, char *argv[])
{
	FILE *pInfile;
	double x0, x1, x2, x3, x4;
	double *pX4Buff = NULL;
	float x;
	int fileElementCount = 0;
	int counter = 0;

	int my_rank;
	int p;
	MPI_Status status;

	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &my_rank); 
	MPI_Comm_size(MPI_COMM_WORLD, &p);

	switch (my_rank)
	{ 
		case 0:
		{
			pInfile = fopen("ExpResults.txt","r");
			fscanf(pInfile, "%d", &fileElementCount);

			pX4Buff = (double*)malloc(fileElementCount * sizeof(double));
			memset(pX4Buff, 0, fileElementCount * sizeof(double)); // Optional

			// Send the counter to the last process
			MPI_Send(&fileElementCount, 1, MPI_INT, (p - 1), 0, MPI_COMM_WORLD);

			// Read each element from the file
			while(counter < fileElementCount)
			{
				fscanf(pInfile, "%f", &x);
				x0 = x;
				x1 = x0 - (4 * x0) + 7;
				MPI_Send(&x1, 1, MPI_DOUBLE, 1, 0, MPI_COMM_WORLD);
				counter++;
			}

			// File end, send a SENTINEL value to complete calculation
			fclose(pInfile);
			pInfile = NULL;

			x1 = SENTINEL;
			MPI_Send(&x1, 1, MPI_DOUBLE, 1, 0, MPI_COMM_WORLD);

			// Wait for buffer from last node
			MPI_Recv((void*)pX4Buff, counter, MPI_DOUBLE, (p - 1), 0, MPI_COMM_WORLD, &status);

			// Print results
			for(int i = 0; i < counter; i++)
			{
				printf("Result[%d]: %g\n", i, pX4Buff[i]);
			}
			free(pX4Buff);
			pX4Buff = NULL;
			break;
		}
		case 1:
		{
			do
			{ 
				MPI_Recv(&x1, 1, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD, &status);
				if(x1 != SENTINEL)
				{ 
					x2 = pow(x1, 3) + sin(x1/8);
				}
				else
				{
					x2 = SENTINEL;
				}
				MPI_Send(&x2, 1, MPI_DOUBLE, 2, 0, MPI_COMM_WORLD);
			} while (x1 != SENTINEL);
			break;
		}
		case 2:
		{
			do
			{ 
				MPI_Recv(&x2, 1, MPI_DOUBLE, 1, 0, MPI_COMM_WORLD, &status);
				if(x2 != SENTINEL)
				{ 
					x3 = (2 * pow(x2, 4)) + cos(4 * x2) + (3 * M_PI);
				}
				else
				{
					x3 = SENTINEL;
				}
				MPI_Send(&x3, 1, MPI_DOUBLE, 3, 0, MPI_COMM_WORLD);
			} while (x2 != SENTINEL);
			break;
		}

		case 3:
		{
			// Get the file element count first
			MPI_Recv(&fileElementCount, 1, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD, &status);
			pX4Buff = (double*)malloc(fileElementCount * sizeof(double));
			memset(pX4Buff, 0, fileElementCount * sizeof(double));

			// Now, receive the pipelined data
			counter = 0;
			do
			{ 
				MPI_Recv(&x3, 1, MPI_DOUBLE, 2, 0, MPI_COMM_WORLD, &status);
				if(x3 != SENTINEL)
				{ 
					x4 = (3 * pow(x3, 2)) - (2 * x3) + (tan(x3) / 3);

					// Save the result into buffer & increment the buffer counter
					pX4Buff[counter] = x4;
					counter++;
				}
			} while (x3 != SENTINEL);

			// End of file reached, send the buffer back to the root
			if(counter > 0)
			{
				MPI_Send(pX4Buff, counter, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD);
			}
			// Clean up
			free(pX4Buff);
			pX4Buff = NULL;
			break;
		}
		default:
		{
			printf("Process %d unused.\n",my_rank);
			break;
		}
	}

	MPI_Finalize();

  return 0;
}
