To compile, run `make`

To execute, run `mpirun -np P prog X Y N`

Where P is number of processors, and X is number of rows of the grid and Y is the
number of columns, and X * Y + 1 = P. N is the number of iterations to run for.
If N is `-1`, then the program will run indefinitely. Program will terminate upon
finding a file called `sentinel` (doesn't need any contents, only for existence)
in its present working directory. Early terminator via sentinel works for both
iteration limited runs and infinite runs.

e.g. `touch sentinel` to terminate early

Will output a `base_station.log` file upon completion.
