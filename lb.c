#include <stdint.h>
#include <stdio.h>
#include <mpi.h>


#define MAXINT 211
#define FALSE 0
#define TRUE !FALSE
#define WORKREQ_TAG 999
#define WORK_TAG 888
#define STOP_TAG 444
#define END_TAG 443
#define HELP_TAG 333
#define HELPREP_TAG 332



/* forward declarations */
int FG_Process(int argc, char **argv);
int do_some_work(int times);
int noticeMyworkers(int osProcSize, MPI_Comm proc_comm);
int getNoticefromworkers(int osProcSize, MPI_Comm proc_comm);
int noticeMyManager(MPI_Comm proc_comm);
int noticeMyPrevMag(int prevmag,MPI_Comm mag_comm);
int finishSendfromworkers(int osProcSize, MPI_Comm proc_comm,int noticeWorker);
int who_are_my_magNeighb(int rank, int osProcNum, int osProcSize, int *prevmag_ptr, int *nextmag_ptr);

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
	int rank, size, osProcNum, osProcSize, startRank, proc_color, mag_color = 0;
	MPI_Init(&argc,&argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPIX_Get_collocated_size(&osProcSize);
    MPIX_Get_n_size(&osProcNum);
    MPIX_Get_collocated_startrank(&startRank);
    proc_color = rank / osProcSize;
    mag_color =  (rank == startRank) ? 1 : 0;
    MPI_Comm proc_comm, mag_comm;
    MPI_Comm_split(MPI_COMM_WORLD, proc_color, rank, &proc_comm);
    MPI_Comm_split(MPI_COMM_WORLD, mag_color, rank, &mag_comm);

    MPI_Barrier(MPI_COMM_WORLD);


    if (0 == rank ) {
    	printf("the program start here.\n");
    }

    if (mag_color) {
    	
    	// this is the case for manager processes;
		int replyreqbuf = 0, reqbuf = 0;
	    int notdoneSign = TRUE, ownWorkdone = FALSE, helpOffering = FALSE;


	    // for managers communication
	    MPI_Request helprequest;
    	int prevmag, nextmag, helpComing = TRUE;
        who_are_my_magNeighb(rank, osProcNum, osProcSize, &prevmag, &nextmag);
	    MPI_Irecv(&helpComing,1,MPI_INT,prevmag,HELP_TAG,mag_comm,&helprequest);
    	

    	while (notdoneSign){

    		// check if any help is offered
    		MPI_Status helpStatus;
    		int helpReqRes=FALSE;
			MPI_Test(&helprequest,&helpReqRes,&helpStatus);
            if (helpReqRes) {
            	replyreqbuf = (ownWorkdone) ? -1 : replyreqbuf+1;
            	MPI_Send(&replyreqbuf, 1, MPI_INT, prevmag, HELPREP_TAG, mag_comm);
            };




	    	MPI_Status status;
			MPI_Recv(&reqbuf, 1, MPI_INT, MPI_ANY_SOURCE, WORKREQ_TAG, proc_comm, &status);
	        if (reqbuf == -1 || ownWorkdone == TRUE) {
	        	// communicate with other managers
	        	if (ownWorkdone == FALSE) printf("proc_comm with manager %d has finished their own job.\n", rank);
	        	ownWorkdone = TRUE;
				if (!helpOffering) {
	        		helpOffering = TRUE;
	        		MPI_Send(&helpComing, 1, MPI_INT, nextmag, HELP_TAG, mag_comm);
	        		
	        		// Case  when all the processes happend to finish at the same time
					int temphelpReqRes=FALSE; 
					MPI_Status temphelpStatus;
		        	MPI_Test(&helprequest,&temphelpReqRes,&temphelpStatus);
	            	if (temphelpReqRes) {
	            		replyreqbuf = -1;
		        		MPI_Send(&replyreqbuf, 1, MPI_INT, prevmag, HELPREP_TAG, mag_comm);
		        	}
	
		        	MPI_Status tempSta;
		        	MPI_Recv(&replyreqbuf, 1, MPI_INT, nextmag, HELPREP_TAG, mag_comm, &tempSta);
		        	helpOffering = FALSE;
		        	if (replyreqbuf > MAXINT || replyreqbuf == -1){
		       			noticeMyPrevMag(prevmag, mag_comm);
		       			noticeMyworkers(osProcSize, proc_comm);
						finishSendfromworkers(osProcSize, proc_comm, status.MPI_SOURCE);
						notdoneSign = FALSE; replyreqbuf = -1;
			        	MPI_Request tempReq;
			        	MPI_Isend(&replyreqbuf, 1, MPI_INT, status.MPI_SOURCE, WORK_TAG, proc_comm, &tempReq);
		        	} else{
		        		printf("The worker %d is helping the other proc_comm doing work No. %d.\n", status.MPI_SOURCE, replyreqbuf);
		        		MPI_Send(&replyreqbuf, 1, MPI_INT, status.MPI_SOURCE, WORK_TAG, proc_comm);
		        	}
	        	} else{
	        		MPI_Send(&reqbuf, 1, MPI_INT, status.MPI_SOURCE, WORK_TAG, proc_comm);
	        	}
	        } else{
	        	replyreqbuf++;
				MPI_Send(&replyreqbuf, 1, MPI_INT, status.MPI_SOURCE, WORK_TAG, proc_comm);
	        }
	    }
    	getNoticefromworkers(osProcSize, proc_comm);
    	MPI_Barrier(mag_comm);


    } else {
    	// this is the case for worker processes;
    	MPI_Status status; MPI_Request request;
	    int reqbuf = 0, notdoneSign = TRUE;
	    MPI_Irecv(&notdoneSign,1,MPI_INT,0,STOP_TAG,proc_comm,&request);
    	while (notdoneSign) {
	        MPI_Send(&reqbuf, 1, MPI_INT, 0, WORKREQ_TAG, proc_comm);
			int result=FALSE;
			MPI_Test(&request,&result,&status);
            if ( result == TRUE) break;
			int replyreqbuf = 0;
			MPI_Recv(&replyreqbuf, 1, MPI_INT, 0, WORK_TAG, proc_comm, &status);
	        reqbuf = do_some_work(replyreqbuf);
	        //if (reqbuf > 0) printf("rank %d is doing %dth work\n", rank, reqbuf-1); 
    	}
    	noticeMyManager(proc_comm);
    }
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Comm_free(&proc_comm);
    MPI_Comm_free(&mag_comm);
    MPI_Finalize();
    return 0;
}

int do_some_work(int times)
{	
	if (times <= MAXINT && times >=0) {
		MPIX_Usleep(100);
		return times+1;
	}
		return -1;
}

int finishSendfromworkers(int osProcSize, MPI_Comm proc_comm, int noticeWorker){
	int i, info;
	//MPI_Status status;
	for (i = 1; i < osProcSize; i++){
		if (i == noticeWorker) continue;
		MPI_Request tempReq;
		MPI_Irecv(&info, 1, MPI_INT, i, WORKREQ_TAG, proc_comm, &tempReq);
	}
	return 0;
}

int noticeMyworkers(int osProcSize, MPI_Comm proc_comm){
	int i, info = FALSE;
	for (i = 1; i < osProcSize; i++){
		MPI_Send(&info, 1, MPI_INT, i, STOP_TAG, proc_comm);
	}
	return 0;
}

int getNoticefromworkers(int osProcSize, MPI_Comm proc_comm){
	int i, info;
	MPI_Status status;
	for (i = 1; i < osProcSize; i++){
		MPI_Recv(&info, 1, MPI_INT, i, END_TAG, proc_comm,&status);
	}
	return 0;
}

int noticeMyManager(MPI_Comm proc_comm){
	int info = TRUE;
	MPI_Send(&info, 1, MPI_INT, 0, END_TAG, proc_comm);
	return 0;
}

int noticeMyPrevMag(int prevmag,MPI_Comm mag_comm){
	int info = -1;
	MPI_Request tempReq;
	MPI_Isend(&info, 1, MPI_INT, prevmag, HELPREP_TAG, mag_comm, &tempReq);
	return 0;
}

int who_are_my_magNeighb(int rank, int osProcNum, int osProcSize, int *prevmag_ptr, int *nextmag_ptr)
{
    int prevmag = -1, nextmag = -1;
 	int procOrder = rank / osProcSize;
    prevmag = (0 ==rank) ? (osProcNum-1) : procOrder-1;
    nextmag = ((osProcNum-1)*osProcSize ==rank) ? 0 : procOrder+1;
    *prevmag_ptr = prevmag;
    *nextmag_ptr = nextmag;
    return (0);
}

