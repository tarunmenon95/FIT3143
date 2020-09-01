#include <stdio.h>
#include <mpi.h>
struct valuestruct {
int a;
double b;
} ;
int main(int argc, char** argv)
{
struct valuestruct values;
int myrank;
MPI_Datatype Valuetype;
MPI_Datatype type[2] = { MPI_INT, MPI_DOUBLE };
int blocklen[2] = { 1, 1};
MPI_Aint disp[2];
MPI_Init(&argc, &argv);
MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
MPI_Get_address(&values.a, &disp[0]);
MPI_Get_address(&values.b, &disp[1]);
//Make relative
disp[1]=disp[1]-disp[0];
disp[0]=0;
// Create MPI struct
MPI_Type_create_struct(2, blocklen, disp, type, &Valuetype);
MPI_Type_commit(&Valuetype);
do{
 if (myrank == 0){
printf("Enter an round number (>0) & a real number: ");
fflush(stdout);
scanf("%d%lf", &values.a, &values.b);
 }
 MPI_Bcast(&values, 2, Valuetype, 0, MPI_COMM_WORLD);
 printf("Rank: %d. values.a = %d. values.b = %lf\n",
myrank, values.a, values.b);
 fflush(stdout);
}while(values.a > 0);
/* Clean up the type */
MPI_Type_free(&Valuetype);
MPI_Finalize();
return 0;
}