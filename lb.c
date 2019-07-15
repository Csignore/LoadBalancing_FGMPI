#include <stdint.h>
#include <stdio.h>
#include <mpi.h>


#define MAXINT 9
#define FALSE 0
#define TRUE !FALSE
#define WORKREQ_TAG 999
#define WORK_TAG 888
#define STOP_TAG 444


/* forward declarations */
int FG_Process(int argc, char **argv);
int do_some_work(int times);
int noticeMyworkers(int osProcSize, int startRank, MPI_Comm proc_comm);
int getNoticefromworkers(int osProcSize, int startRank, MPI_Comm proc_comm);
int noticeMyManager(MPI_Comm proc_comm);
int finishSendfromworkers(int osProcSize, int startRank, MPI_Comm proc_comm);

/******* FG-MPI Boilerplate begin *********/
#include "fgmpi.h"
/* forward declarations */
FG_MapPtr_t map_lookup(int argc, char** argv, char* str);
FG_ProcessPtr_t random_mapper(int argc, char** argv, int rank);
FG_ProcessPtr_t binding_func(int argc, char** argv, int rank);

int main( int argc, char *argv[] )
{
    FGmpiexec(&argc, &argv, &map_lookup);

    return (0);
}

FG_MapPtr_t map_lookup(int argc, char** argv, char* str)
{
    return (&binding_func);
}

FG_ProcessPtr_t binding_func(int argc, char** argv, int rank)
{
    if ( (rank == MAP_INIT_ACTION) || (rank == MAP_FINALIZE_ACTION) ){
        return (NULL);
    }

    return (&FG_Process);
}

int FG_Process(int argc, char **argv){
	int rank, size;
	MPI_Init(&argc,&argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    

    int osProcNum, osProcSize, startRank, proc_color, mag_color = 0;
    MPIX_Get_collocated_size(&osProcSize);
    MPIX_Get_n_size(&osProcNum);
    MPIX_Get_collocated_startrank(&startRank);
    //printf("this %d processes has size %d and has %d procNum\n", rank, osProcSize, osProcNum);

    proc_color = rank / osProcSize; 

    MPI_Comm proc_comm, mag_comm;
    char name[12], nameout[12];
    int rlen;
    snprintf(name, 12, "comm_%d", proc_color);
    //printf("this %d processes has proc_color %d\n", rank, proc_color);
    MPI_Comm_split(MPI_COMM_WORLD, proc_color, rank, &proc_comm);
    MPI_Comm_set_name( proc_comm, name );

    
    char magName[12];
    if (rank == startRank) {
    	mag_color = 1;
    	snprintf(magName, 12, "magComm");
    } else {snprintf(magName, 12, "workerComm");}
    MPI_Comm_split(MPI_COMM_WORLD, mag_color, rank, &mag_comm);
    MPI_Comm_set_name( mag_comm, magName );
    //nameout[0] = 0;
    //MPI_Comm_get_name( mag_comm, nameout, &rlen );
    //printf("this %d processes belongs to mag_comm %s\n", rank, nameout);


    MPI_Barrier(MPI_COMM_WORLD);


    if (0 == rank ) {
    	printf("the program start here.\n");
    }

    if (mag_color) {
    	MPI_Request request;
    	
    	// this is the case for manager processes;
		uint32_t replyreqbuf = 0;
		uint32_t reqbuf = 0;
	    int notdoneSign = TRUE;
	    /* Set up a MPI_Irecv to stop the procComm */
    	while (notdoneSign){
	    	MPI_Status status;
			/*MPI_Recv(&reqbuf, 1, MPI_INT, MPI_ANY_SOURCE, WORKREQ_TAG, proc_comm, &status);
	        printf("the manager has received request from worker %d, value is %d.\n",status.MPI_SOURCE, reqbuf);
	        if (reqbuf == -1) {
	        	// communicate with other managers



	        	notdoneSign = FALSE;

	        } else{
	        	// send reply back to sender of the request received above
				replyreqbuf++;
				MPI_Send(&replyreqbuf, 1, MPI_INT, status.MPI_SOURCE, WORK_TAG, proc_comm);
				printf("the manager has sent the replyreqbuf as value %d to worker %d \n", replyreqbuf, status.MPI_SOURCE);	
	        }*/
	        MPI_Recv(&reqbuf, 1, MPI_INT, MPI_ANY_SOURCE, WORKREQ_TAG, proc_comm, &status);
	        if (reqbuf == -1) {
	        	// communicate with other managers


	        	finishSendfromworkers(osProcSize, startRank, proc_comm);
	        	notdoneSign = FALSE;

	        }
	    }
	    noticeMyworkers(osProcSize, startRank, proc_comm);
	    
    	getNoticefromworkers(osProcSize, startRank, proc_comm);


    } else {
    	// this is the case for worker processes;
    	MPI_Status status;
	    int reqbuf = -1;
	    int notdoneSign = TRUE; MPI_Request request;

	    MPI_Irecv(&notdoneSign,1,MPI_INT,0,STOP_TAG,proc_comm,&request);
	    
    	while (notdoneSign) {
    		/*
	        MPI_Send(&reqbuf, 1, MPI_INT, 0, WORKREQ_TAG, proc_comm);
	        printf("the worker %d  has sent reqbuf %d to the manager.\n",rank, reqbuf);


			//int result=FALSE;
			//MPI_Test(&request,&result,&status);
            //if ( result == TRUE) break;
			uint32_t replyreqbuf = 0;
			MPI_Recv(&replyreqbuf, 1, MPI_INT, 0, WORK_TAG, proc_comm, &status);
	        printf("the worker %d has received reply from the manager, replyreqbuf  is %d.\n",rank, replyreqbuf);
	        
	        reqbuf = do_some_work(replyreqbuf);
	        printf("%d is sleeping the %dth times\n", rank, reqbuf);
	        	*/
    		MPI_Send(&reqbuf, 1, MPI_INT, 0, WORKREQ_TAG, proc_comm);
    		printf("the worker %d has sent -1 to the manager\n", rank);

    	}
    	noticeMyManager(proc_comm);

    }
    printf("%d is done ==================================\n", rank);

    MPI_Finalize();
    return 0;
}

int do_some_work(int times)
{	
	if (times < MAXINT) {
		MPIX_Usleep(100); 
		return times+1;
	}
		return -1;
}

int finishSendfromworkers(int osProcSize, int startRank, MPI_Comm proc_comm){
	int i, info;
	MPI_Status status;
	for (i = 1; i < osProcSize; i++){
		MPI_Recv(&info, 1, MPI_INT, i, WORKREQ_TAG, proc_comm,&status);
		printf("the manager has received last workreq sign from worker %d\n", i);
	}
}

int noticeMyworkers(int osProcSize, int startRank, MPI_Comm proc_comm){
	int i, info = FALSE;
	for (i = 1; i < osProcSize; i++){
		MPI_Send(&info, 1, MPI_INT, i, STOP_TAG, proc_comm);
		printf("the manager has sent stop sign to worker %d\n", i);
	}
}

int getNoticefromworkers(int osProcSize, int startRank, MPI_Comm proc_comm){
	int i, info;
	MPI_Status status;
	for (i = 1; i < osProcSize; i++){
		MPI_Recv(&info, 1, MPI_INT, i, STOP_TAG, proc_comm,&status);
		printf("the manager has received end sign from worker %d\n", i);
	}
}

int noticeMyManager(MPI_Comm proc_comm){
	int info = TRUE;
	MPI_Send(&info, 1, MPI_INT, 0, STOP_TAG, proc_comm);
}

