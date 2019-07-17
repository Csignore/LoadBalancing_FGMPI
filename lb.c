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
    	
    	// this is the case for manager processes;
		int replyreqbuf = 0;
		int reqbuf = 0;
	    int notdoneSign = TRUE;
	    int ownWorkdone = FALSE;
	    int helpOffering = FALSE;

	    // for managers communication
	    MPI_Request helprequest;
    	int prevmag, nextmag;
        who_are_my_magNeighb(rank, osProcNum, osProcSize, &prevmag, &nextmag);
        int helpComing = TRUE;
	    MPI_Irecv(&helpComing,1,MPI_INT,prevmag,HELP_TAG,mag_comm,&helprequest);
    	

    	while (notdoneSign){


    		MPI_Status helpStatus;
    		int helpReqRes=FALSE;
			MPI_Test(&helprequest,&helpReqRes,&helpStatus);
            if ( helpReqRes == TRUE) {
            	printf("manager_rank %d is requesting work from rank %d\n", prevmag*osProcSize, rank);
            	if (ownWorkdone == TRUE) {
            		replyreqbuf = -1;
            	} else {replyreqbuf++;}
            	MPI_Send(&replyreqbuf, 1, MPI_INT, prevmag, HELPREP_TAG, mag_comm);
            	printf("rank %d is sending work %d back the prevmag\n", rank, replyreqbuf);
            };




	    	MPI_Status status;
			MPI_Recv(&reqbuf, 1, MPI_INT, MPI_ANY_SOURCE, WORKREQ_TAG, proc_comm, &status);
	        //printf("the manager has received request from worker %d, value is %d.\n",status.MPI_SOURCE, reqbuf);
	        if (reqbuf == -1 || ownWorkdone == TRUE) {


	        	// communicate with other managers
	        	printf("rank %d surely has its work done\n", rank);
	        	ownWorkdone = TRUE;

	        	if (helpOffering == FALSE) {
	        		helpOffering = TRUE;
	        		MPI_Send(&helpComing, 1, MPI_INT, nextmag, HELP_TAG, mag_comm);
	        		printf("requesting the work since rank %d is out\n", rank);


	        		// Case  when all the processes happend to finish at the same time
					int temphelpReqRes=FALSE; MPI_Status temphelpStatus;
		        	MPI_Test(&helprequest,&temphelpReqRes,&temphelpStatus);
	            	if ( temphelpReqRes == TRUE) {
	            		replyreqbuf = -1;
		        		printf("this case is when all the processes happend to finish at the same time\n");
		        		MPI_Send(&replyreqbuf, 1, MPI_INT, prevmag, HELPREP_TAG, mag_comm);
		        	}
	
		        	MPI_Status tempSta;
		        	MPI_Recv(&replyreqbuf, 1, MPI_INT, nextmag, HELPREP_TAG, mag_comm, &tempSta);
		        	helpOffering = FALSE;
		        	printf("rank %d is receiving replyreqbuf %d\n", rank, replyreqbuf);
		        	if (replyreqbuf > MAXINT || replyreqbuf == -1){
		       printf("rank %d got replyreqbuf %d and proceed to end\n", rank, replyreqbuf);
		        		noticeMyPrevMag(prevmag, mag_comm);
		       printf("rank %d ending process 1\n", rank);
			        	noticeMyworkers(osProcSize, proc_comm);
			   printf("rank %d ending process 2\n", rank);
			        	finishSendfromworkers(osProcSize, proc_comm, status.MPI_SOURCE);
			   printf("rank %d ending process 3\n", rank);
			        	notdoneSign = FALSE; replyreqbuf = -1;
			        	MPI_Request tempReq;
			        	printf("the manager %d still send replyreqbuf %d to rank %d\n", rank, replyreqbuf, status.MPI_SOURCE);
			        	MPI_Isend(&replyreqbuf, 1, MPI_INT, status.MPI_SOURCE, WORK_TAG, proc_comm, &tempReq);
			        	printf("the manager %d still send replyreqbuf %d to rank %d\n", rank, replyreqbuf, status.MPI_SOURCE);
						
		        	} else{
		        		MPI_Send(&replyreqbuf, 1, MPI_INT, status.MPI_SOURCE, WORK_TAG, proc_comm);
						printf("the manager has sent a work from nextmag a replyreqbuf as value %d to worker %d \n", replyreqbuf, status.MPI_SOURCE+startRank);	
		        	}




	        	} else{
	        		printf("the manager %d is offering help now, come back later\n", rank);
	        		MPI_Send(&reqbuf, 1, MPI_INT, status.MPI_SOURCE, WORK_TAG, proc_comm);
	        	}
	        	







	        } else{
	        	// send reply back to sender of the request received above
				replyreqbuf++;
				MPI_Send(&replyreqbuf, 1, MPI_INT, status.MPI_SOURCE, WORK_TAG, proc_comm);
				//printf("the manager has sent the replyreqbuf as value %d to worker %d \n", replyreqbuf, status.MPI_SOURCE);	
	        }
	    }
	    //noticeMyworkers(osProcSize, proc_comm);
	    printf("rank %d ending process 4\n", rank);
    	getNoticefromworkers(osProcSize, proc_comm);
    	printf("mag %d has reached here===============\n", rank);
    	MPI_Barrier(mag_comm);


    } else {
    	// this is the case for worker processes;
    	MPI_Status status;
	    int reqbuf = 0;
	    int notdoneSign = TRUE; MPI_Request request;
	    MPI_Irecv(&notdoneSign,1,MPI_INT,0,STOP_TAG,proc_comm,&request);
	    
    	while (notdoneSign) {
	        MPI_Send(&reqbuf, 1, MPI_INT, 0, WORKREQ_TAG, proc_comm);
	        //printf("the worker %d  has sent reqbuf %d to the manager.\n",rank, reqbuf);

			int result=FALSE;
			MPI_Test(&request,&result,&status);
            if ( result == TRUE) break;
			int replyreqbuf = 0;
			MPI_Recv(&replyreqbuf, 1, MPI_INT, 0, WORK_TAG, proc_comm, &status);
	        printf("the worker %d has received reply from the manager %d, replyreqbuf  is %d.\n",rank, startRank, replyreqbuf);
	        
	        reqbuf = do_some_work(replyreqbuf);
	        printf("%d is sleeping the %d's %dth times\n", rank, startRank, reqbuf-1);

    	}
    	printf("%d has reached a point to notice manager\n", rank);
    	noticeMyManager(proc_comm);
    	printf("%d has noticed manager\n", rank);
    }
    printf("%d is done ==================================\n", rank);

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
		printf("the manager is ready to receive from i %d\n", i);
		MPI_Irecv(&info, 1, MPI_INT, i, WORKREQ_TAG, proc_comm, &tempReq);
	}
	return 0;
}

int noticeMyworkers(int osProcSize, MPI_Comm proc_comm){
	int i, info = FALSE;
	for (i = 1; i < osProcSize; i++){
		printf("i am noticing worker %d\n", i);
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

