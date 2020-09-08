#include <stdio.h>
#include <mpi.h>


int main(int argc, char **argv)
{
    int rank, s_value, r_value, size;
    MPI_Status status;
    MPI_Init( &argc, &argv );
    MPI_Comm_rank( MPI_COMM_WORLD, &rank );
    MPI_Comm_size( MPI_COMM_WORLD, &size );
    do {
        if (rank == 0) {
            printf("Enter a round number: ");
            fflush(stdout);
            scanf( "%d", &s_value );
            //Send s_value to the next process
            MPI_Send(&s_value, 1, MPI_INT, rank+1, 0, MPI_COMM_WORLD);
            //Recieve r_value from process size-1
            MPI_Recv(&r_value, 1, MPI_INT, size-1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            printf( "Process %d got %d from Process %d\n",
            rank, r_value, size - 1);
            fflush(stdout);
            }
        else {
        // Recieve r_value from the previous process
        MPI_Recv(&r_value, 1, MPI_INT, rank-1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        
        //If last process then send to first process
        if (rank == size - 1){
            MPI_Send(&r_value,1, MPI_INT, 0, 0, MPI_COMM_WORLD);
        }
        
        //Otherwise send to the next process
        else{
        MPI_Send(&r_value,1, MPI_INT, rank+1, 0, MPI_COMM_WORLD);
        }
        
        printf( "Process %d got %d from Process %d\n",
        rank, r_value, rank - 1);
        fflush(stdout);
        }
    } while (r_value >= 0);
MPI_Finalize( );
return 0;
}
