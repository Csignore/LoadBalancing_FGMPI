# LoadBalancing_FGMPI
Load Balancing project based on FGMPI

## Introduction
MPI processors, individually, perform a load of tasks. Generally, the efficiency of the MPI program depends on the slowest processor in the program; however, the tasks are not equally distributed among the processors,  which means that a fast and reliable load balancing mechanism in MPI can remarkably increase the efficiency.

### Example:
Three processors A,B,C
Nine computational processes/tasks 0 to 8: task 0 requires much more time than the others.

**Case required load balancing**: Each processor are programmed to perform three tasks, assume processor A has task 0, 1, 2. In this case, when the program is executed a while, processor A is likely still occupied by its tasks while the other two are free already. Having the load balancing mechanism here can help the processors balance the workload at some point in the loop of the program and keep the processors B, C busy, resulting in a faster computation.

## Mechanism
The library [FGMPI](https://www.cs.ubc.ca/~humaira/fgmpi.html) allows for interleaved execution of multiple concurrent MPI processes inside an OS-process. In this load balacing project, all  concurrent MPI processes in each OS-process are assigned to a role: a manager process and the rest are workers.

The manager process is in charge of assigning task to every workers that belong to the same OS-process, which allows the workload of each worker processes to evenly distribute. In addition, the communnication between the managers allows the work of each OS-process movable, which likely to minimum the total running time of the program.



## Build and Run
This project is based on an open source MPI library: [FGMPI](https://www.cs.ubc.ca/~humaira/fgmpi.html). Follow instruction [here](https://www.cs.ubc.ca/~humaira/docs/fgmpi_userguide.pdf) to build the environment.

**Running Command**

```
mpiexec -nfg %d -n %d ./lb  
```

FG-MPI uses a packed assignment, the first input is the number of concurrent processes created in one OS-process; the seconde input is the number of OS-processes created in the program.

For example, command ```
mpiexec -nfg 10 -n 10 ./lb  ``` creates 10 OS-processes, and each OS-process has 10 concurrent processes. The concurrent processes in the OS-process are assigned a process number as the range of [0..9] [10..19] ... [80..89] [90..99].  The use above has 0, 10, 20 ... 80, 90 as the managers so that the rest of processes are workers for the respective manager.

