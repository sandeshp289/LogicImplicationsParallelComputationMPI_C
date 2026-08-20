////////////////////////////////////////////////////////////////////////////////////
// File       : implications_parallel.c
// Language   : C/C++ and MPI
// Description: Parallel Computation of Static Logic Implications
// Author     : Sandesh Prabhakar
// Created    : November 23, 2008
// Modified   : August 19, 2026
// Version    : 1.0
//
// Copyright (c) 2026 Sandesh Prabhakar
// All rights reserved
//////////////////////////////////////////////////////////////////////////////////////

#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define WORKTAG 1
#define DIETAG 2

enum
{
   G_JUNK,         /* 0 */
   G_INPUT,        /* 1 */
   G_OUTPUT,       /* 2 */
   G_XOR,          /* 3 */
   G_XNOR,         /* 4 */
   G_DFF,          /* 5 */
   G_AND,          /* 6 */
   G_NAND,         /* 7 */
   G_OR,           /* 8 */
   G_NOR,          /* 9 */
   G_NOT,          /* 10 */
   G_BUF,          /* 11 */
};

struct node
{
        char value;
        bool flag;

	int data_value;
	signed int weight;
	node *link;
};
     void master(int argc, char **argv);
     void slave(int argc, char **argv, int myrank);
     void eval_direct_impl(int j, int count, unsigned char* gtype, short* fanin, int** faninlist, int **graph, node **pnode, node **pnode_final ); 
     void additem (int listn, int data, int wt, node **pnode, node **pnode_final);
     void additem_final (int listn, int data, int wt, node **pnode_final);
     void display(int j, node **pnode);

     void dfs(int j, signed int net_wt, int source_node, node ***scheduling, node **pnode);
     int checkschedulelist(int j, signed int net_wt, int source_node, node ***scheduling);
     void addscheduling (int data, signed int time_frame, int source_node, node ***scheduling);
     void sdisplay(int j, node ***scheduling);
     void display_final(int j, node **pnode_final);
     void add_value (int data, node **storevalue);
     void empty_value (node *gate, node **storevalue);

     void add_event (int gate_number, int level_number, node **eventlist ); 
     void logic_simulate(int level_number, node *gate, int ig, int tim, int* levelNum, short* fanin, int** faninlist, short* fanout, int** fanoutlist, unsigned char* gtype, node **eventlist, node **pnode, node **pnode_final, node **storevalue, int **graph, int *send, int *impl_count);
     void evaluate(int j, unsigned char* gtype, node* gate, short* fanin, int** faninlist );
     void contrapositive_implications(int listnn, int datan, signed wt, int count, node **pnode_final);


////////////////////////////////////////////////////////////////////////
// main starts here
////////////////////////////////////////////////////////////////////////
int main(int argc, char **argv)
{
  int myrank;
  double endTime, startTime, time;
  /* Initialize MPI */

  MPI_Init(&argc, &argv);

  /* Find out my identity in the default communicator */

  MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
  if (myrank == 0) {
    	/* start timer */
	startTime = MPI_Wtime();
        master(argc, argv);
        /* stop timer */
	endTime = MPI_Wtime();
	time = endTime - startTime;
       	fprintf(stdout,"\n Total Parallel time (Ts) = %e seconds ",time);fflush(stdout); 
  } else {
    slave(argc, argv, myrank);
  }

  /* Shut down MPI */

  MPI_Finalize();
  return 0;
}


void master(int argc, char *argv[])
{

    char cktName[20];
    int vecWidth, poNum;

    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <ckt>\n", argv[0]);
        exit(-1);
    }

    strcpy(cktName, argv[1]);

    // circuit information
    int numgates;       // total number of gates (faulty included)
    int maxlevels;      // number of levels in gate level ckt
    int numInputs;	// number of inputs
    int numOutputs;	// number of outputs
    unsigned char *gtype;	// gate type
    short *fanin;		// number of fanins of gate
    short *fanout;		// number of fanouts of gate
    int *levelNum;		// level number of gate
    int **faninlist;		// fanin list of gate
    int **fanoutlist;		// fanout list of gate

    int **graph;
    node **pnode; 
    node **pnode_final;
    node ***scheduling;

    node *gate;
    node **eventlist;
    node **storevalue;

    FILE *inFile;
    char fName[40];
    int i, j, count;
    char c;
    int gatenum, junk;
    int f1, f2, f3;

    int tim, ig, temp_gate, temp, dummy;
    int *receive;
    int listnn, datan, wt;

    int ntasks, rank;
    MPI_Status status;

    strcpy(fName, cktName);
    strcat(fName, ".lev");
    inFile = fopen(fName, "r");
    if (inFile == NULL)
    {
	fprintf(stderr, "Can't open .lev file\n");
	exit(-1);
    }

    numgates = maxlevels = 0;
    numInputs = numOutputs = 0;

    fscanf(inFile, "%d", &count);	// number of gates
    fscanf(inFile, "%d", &junk);	// skip the second line

    int dcount = count + count - 2;

    // allocate space for gates data structure
    gtype = new unsigned char[count];
    fanin = new short[count];
    fanout = new short[count];
    levelNum = new int[count];
    faninlist = new int * [count];
    fanoutlist = new int * [count];
    receive = new int [count+500];

    gate = new node[count];

    graph = new int * [count];
    pnode = new node * [dcount+1];
    pnode_final = new node * [dcount+1];
    storevalue = new node * [1];
   
    scheduling = new node**[dcount+1];
    for(int z=0;z<=dcount;z++)
    scheduling[z] = new node*[7];

    //Initializing the implication graph
    for(i=0; i<count; i++)
     {
      graph[i] = new int[2];
	for (j=0; j<2; j++)
	     {
                 if(j==0)
	         graph[i][j] = i;
		 else
		 graph[i][j] = i + count - 1;
	     }
     }

	for (j=0; j<count+500; j++)
	     {
            receive[j]=0;
	     }

    for (j=0; j<=dcount; j++)
	{
        pnode[j] = NULL;
        pnode_final[j] = NULL; 
	}

   //Initializing the scheduling list
   for(int z=0; z<=dcount;z++)
     {
      for (int k=0; k<=6; k++)
         {
	    scheduling[z][k] = NULL;
          }
      }

    storevalue[0]=NULL;


    // now read in the circuit
    for (i=1; i<count; i++)
    {
	fscanf(inFile, "%d", &gatenum);
	fscanf(inFile, "%d", &f1);
	fscanf(inFile, "%d", &f2);
	fscanf(inFile, "%d", &f3);

	numgates++;
	gtype[gatenum] = (unsigned char) f1;
	if (gtype[gatenum] > 13)
	    printf("gate %d is an unimplemented gate type\n", gatenum);
	else if (gtype[gatenum] == G_INPUT)
	    numInputs++;
	else if (gtype[gatenum] == G_OUTPUT)
	    numOutputs++;

	f2 = (int) f2;
        //levelNum[gatenum] = f2;
        levelNum[gatenum] = (f2+5)/5;

	if (f2 >= (maxlevels))
	    maxlevels = f2 + 5;

        //initializing the output of all gates to X and flags to 0
        gate[gatenum].value = 'X';
        gate[gatenum].flag = 0; 

	fanin[gatenum] = (int) f3;
	// now read in the faninlist
	faninlist[gatenum] = new int[fanin[gatenum]];
	for (j=0; j<fanin[gatenum]; j++)
	{
	    fscanf(inFile, "%d", &f1);
	    faninlist[gatenum][j] = (int) f1;
	}

	for (j=0; j<fanin[gatenum]; j++) // followed by samethings
	    fscanf(inFile, "%d", &junk);

	// read in the fanout list
	fscanf(inFile, "%d", &f1);
	fanout[gatenum] = (int) f1;

	fanoutlist[gatenum] = new int[fanout[gatenum]];
	for (j=0; j<fanout[gatenum]; j++)
	{
	    fscanf(inFile, "%d", &f1);
	    fanoutlist[gatenum][j] = (int) f1;
	}

        //determination of direct implications
	eval_direct_impl(gatenum, count, gtype, fanin, faninlist, graph, pnode, pnode_final );


	// skip till end of line
	while ((c = getc(inFile)) != '\n' && c != EOF)
	    ;
    }	// for (i...)
    fclose(inFile);

    int total_levels = maxlevels/5;

    printf("Successfully read in circuit:\n");
    printf("\t%d PIs.\n", numInputs);
    printf("\t%d POs.\n", numOutputs);
    printf("\t%d total number of gates\n", numgates);
    printf("\t%d levels in the circuit.\n", total_levels);
    printf("\t%d Total vertices in the graph\n", dcount);

    // allocate space for event list
    eventlist = new node*[total_levels+1];
    for(i=0;i<total_levels+1;i++)
    eventlist[i]=NULL;

    printf("\n DIRECT IMPLICATIONS\n");
    printf("\n -------------------\n");
    for(i=1; i<=dcount; i++)
      {
	  display(i, pnode);
      }


MPI_Comm_size(MPI_COMM_WORLD, &ntasks);
  /* Seed the slaves; send one unit of work to each slave. */
  fprintf(stdout,"\n Number of processors=%d \n",ntasks);fflush(stdout);
  for (rank = 1; rank < ntasks; ++rank) {
    /* Find the next item of work to do */

    /* Send it to each rank */

    MPI_Send(&rank,              /* message buffer */
             1,                 /* one data item */
             MPI_INT,           /* data item is an integer */
             rank,              /* destination process rank */
             WORKTAG,           /* user chosen message tag */
             MPI_COMM_WORLD);   /* default communicator */

                                       }

  /* Loop over getting new work requests until there is no more work
     to be done */
  ig=ntasks;
  while(ig<=dcount)
  {
    /* Receive results from a slave */

    MPI_Recv(receive,           /* message buffer */
             (count+500),             /* number of data item */
             MPI_INT,           /* of type double real */
             MPI_ANY_SOURCE,    /* receive from any sender */
             MPI_ANY_TAG,       /* any type of message */
             MPI_COMM_WORLD,    /* default communicator */
             &status);          /* info about the received message */

/* Check the tag of the received message. */
    if (status.MPI_TAG == 0) {

                         
                         listnn = receive[0];
                         temp=0;

                         for(tim=0;tim<7;tim++)
			 {
                         dummy= pow(2,tim);
                         if(tim>0)  
			 {
                        
                         for(j=1;j<=receive[temp+dummy+2];j++)
                         {
		         datan = receive[temp+dummy+j+2];
		         wt = tim-3; 
			 additem_final (listnn, datan, wt, pnode_final);
                         printf("\n Indirect Implication detected %d %d %d \n", listnn, datan, wt);
                         contrapositive_implications(listnn, datan, wt, count, pnode_final);
			 }
                         temp=temp+receive[temp+dummy+2];
			 }
                         else
			 {
                         for(j=1;j<=receive[2];j++)
                         {
		         datan = receive[j+2];
		         wt = tim-3; 
			 additem_final (listnn, datan, wt, pnode_final);
                         printf("\n Indirect Implication detected %d %d %d  \n",listnn, datan, wt );
                         contrapositive_implications(listnn, datan, wt, count, pnode_final); 
			 }
                         temp=temp+receive[2]; 
			 }
			 }
    }

    /* Send the slave a new work unit */
    MPI_Send(&ig,                /* message buffer */
             1,                 /* one data item */
             MPI_INT,           /* data item is an integer */
             status.MPI_SOURCE, /* to who we just received from */
             WORKTAG,           /* user chosen message tag */
             MPI_COMM_WORLD);   /* default communicator */

    /* Get the next unit of work to be done */
    ig++;
  }

  /* There's no more work to be done, so receive all the outstanding
     results from the slaves. */

  for (rank = 1; rank < ntasks; ++rank) {
    MPI_Recv(receive, count+500, MPI_INT, MPI_ANY_SOURCE,
             MPI_ANY_TAG, MPI_COMM_WORLD, &status);

                     if (status.MPI_TAG == 0) {

                         listnn = receive[0];
                         temp=0;

                         for(tim=0;tim<7;tim++)
			 {
                         dummy= pow(2,tim);
                         if(tim>0)  
			 {
                         for(j=1;j<=receive[temp+dummy+2];j++)
                         {
		         datan = receive[temp+dummy+j+2];
		         wt = tim-3; 
			 additem_final (listnn, datan, wt, pnode_final);
                         printf("\n Indirect Implication detected %d %d %d \n", listnn, datan, wt);
                         contrapositive_implications(listnn, datan, wt, count, pnode_final);
			 }
                         temp=temp+receive[temp+dummy+2];
			 }
                         else
			 {
                         for(j=1;j<=receive[2];j++)
                         {
		         datan = receive[j+2];
		         wt = tim-3; 
			 additem_final (listnn, datan, wt, pnode_final);
                         printf("\n Indirect Implication detected %d %d %d \n", listnn, datan, wt);
                         contrapositive_implications(listnn, datan, wt, count, pnode_final);
			 }
                         temp=temp+receive[2]; 
			 }
			 }
                                              }
   }


       printf("\n DIRECT AND INDIRECT IMPLICATIONS\n");
	   printf("\n -------------------\n");
       for(i=1; i<=dcount; i++)
	      {
			  display_final(i, pnode_final);
		  }

  /* Tell all the slaves to exit by sending an empty message with the
     DIETAG. */

  for (rank = 1; rank < ntasks; ++rank) {
    MPI_Send(0, 0, MPI_INT, rank, DIETAG, MPI_COMM_WORLD);
  //fprintf(stdout,"\n Master sends message 0 and DIETAG to slave %d \n", rank );fflush(stdout);
  }

fprintf(stdout,"\n MASTER EXITS \n");fflush(stdout);
return;
}

void slave(int argc, char *argv[], int myrank)
{
    char cktName[20];
    int vecWidth, poNum;

    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <ckt>\n", argv[0]);
        exit(-1);
    }

    strcpy(cktName, argv[1]);

    // circuit information
    int numgates;       // total number of gates (faulty included)
    int maxlevels;      // number of levels in gate level ckt
    int numInputs;	// number of inputs
    int numOutputs;	// number of outputs
    unsigned char *gtype;	// gate type
    short *fanin;		// number of fanins of gate
    short *fanout;		// number of fanouts of gate
    int *levelNum;		// level number of gate
    int **faninlist;		// fanin list of gate
    int **fanoutlist;		// fanout list of gate

    int **graph;
    node **pnode; 
    node **pnode_final;
    node ***scheduling;

    node *gate;
    node **eventlist;
    node **storevalue;

    FILE *inFile;
    char fName[40];
    int i, j, count;
    char c;
    int gatenum, junk;
    int f1, f2, f3;
    int *send;
    int tim, ig, temp_gate, temp, dummy;
    MPI_Status status;

    strcpy(fName, cktName);
    strcat(fName, ".lev");
    inFile = fopen(fName, "r");
    if (inFile == NULL)
    {
	fprintf(stderr, "Can't open .lev file\n");
	exit(-1);
    }

    numgates = maxlevels = 0;
    numInputs = numOutputs = 0;

    fscanf(inFile, "%d", &count);	// number of gates
    fscanf(inFile, "%d", &junk);	// skip the second line

    int dcount = count + count - 2;

    // allocate space for gates data structure
    gtype = new unsigned char[count];
    fanin = new short[count];
    fanout = new short[count];
    levelNum = new int[count];
    faninlist = new int * [count];
    fanoutlist = new int * [count];
    send = new int [count+500];
    gate = new node[count];

    graph = new int * [count];
    pnode = new node * [dcount+1];
    pnode_final = new node * [dcount+1];
    storevalue = new node* [1];

    scheduling = new node**[dcount+1];
    for(int z=0;z<=dcount;z++)
    scheduling[z] = new node*[7];

    int *impl_count;
    impl_count = new int[1]; 

    impl_count[0]=0;

    //Initializing the implication graph
    for(i=0; i<count; i++)
     {
      graph[i] = new int[2];
	for (j=0; j<2; j++)
	     {
                 if(j==0)
	         graph[i][j] = i;
		 else
		 graph[i][j] = i + count - 1;
	     }
     }

	for (j=0; j<count+500; j++)
	     {
              send[j]=0;
	     }

    for (j=0; j<=dcount; j++)
	{
        pnode[j] = NULL;
        pnode_final[j] = NULL; 
	}

   //Initializing the scheduling list
   for(int z=0; z<=dcount;z++)
     {
      for (int k=0; k<=6; k++)
         {
	    scheduling[z][k] = NULL;
          }
      }

    storevalue[0]=NULL;


    // now read in the circuit
    for (i=1; i<count; i++)
    {
	fscanf(inFile, "%d", &gatenum);
	fscanf(inFile, "%d", &f1);
	fscanf(inFile, "%d", &f2);
	fscanf(inFile, "%d", &f3);

	numgates++;
	gtype[gatenum] = (unsigned char) f1;
	if (gtype[gatenum] > 13)
	    printf("gate %d is an unimplemented gate type\n", gatenum);
	else if (gtype[gatenum] == G_INPUT)
	    numInputs++;
	else if (gtype[gatenum] == G_OUTPUT)
	    numOutputs++;

	f2 = (int) f2;
        //levelNum[gatenum] = f2;
        levelNum[gatenum] = (f2+5)/5;

	if (f2 >= (maxlevels))
	    maxlevels = f2 + 5;

        //initializing the output of all gates to X and flags to 0
        gate[gatenum].value = 'X';
        gate[gatenum].flag = 0; 

	fanin[gatenum] = (int) f3;
	// now read in the faninlist
	faninlist[gatenum] = new int[fanin[gatenum]];
	for (j=0; j<fanin[gatenum]; j++)
	{
	    fscanf(inFile, "%d", &f1);
	    faninlist[gatenum][j] = (int) f1;
	}

	for (j=0; j<fanin[gatenum]; j++) // followed by samethings
	    fscanf(inFile, "%d", &junk);

	// read in the fanout list
	fscanf(inFile, "%d", &f1);
	fanout[gatenum] = (int) f1;

	fanoutlist[gatenum] = new int[fanout[gatenum]];
	for (j=0; j<fanout[gatenum]; j++)
	{
	    fscanf(inFile, "%d", &f1);
	    fanoutlist[gatenum][j] = (int) f1;
	}

        //determination of direct implications
	eval_direct_impl(gatenum, count, gtype, fanin, faninlist, graph, pnode, pnode_final );


	// skip till end of line
	while ((c = getc(inFile)) != '\n' && c != EOF)
	    ;
    }	// for (i...)
    fclose(inFile);

    int total_levels = maxlevels/5;

/*    printf("Successfully read in circuit:\n");
    printf("\t%d PIs.\n", numInputs);
    printf("\t%d POs.\n", numOutputs);
    printf("\t%d total number of gates\n", numgates);
    printf("\t%d levels in the circuit.\n", total_levels);
    printf("\t%d Total vertices in the graph\n", dcount);
*/
    // allocate space for event list
    eventlist = new node*[total_levels+1];
    for(i=0;i<total_levels+1;i++)
    eventlist[i]=NULL;

    /*printf("\n DIRECT IMPLICATIONS\n");
    printf("\n -------------------\n");
    for(i=1; i<=dcount; i++)
      {
	  display(i, pnode);
      }
*/
//determination of indirect implication starts here
     printf("\n SCHEDULING LIST FOR INDIRECT IMPLICATION DETERMINATION\n");
     printf("\n ------------------------------------------------------\n");

    while (1) {
    /* Receive a message from the master */

    MPI_Recv(&ig, 1, MPI_INT, 0, MPI_ANY_TAG,
             MPI_COMM_WORLD, &status);

    /* Check the tag of the received message. */

    if (status.MPI_TAG == DIETAG) {
      fprintf(stdout,"\n SLAVE %d DIES \n", myrank);fflush(stdout);
      return;
    }
    else if(status.MPI_TAG == WORKTAG)
    {
    /* Do the work */
    //calling the depth first search function
       dfs(ig, 0, ig, scheduling, pnode );
       sdisplay(ig, scheduling);

       impl_count[0]=0;
       for(tim = 0; tim<=6; tim++)
          {

	   node *current = scheduling[ig][tim];

		while(current != NULL)
		  {
		   if(current->data_value > numgates)
		       {
		       temp_gate = (current->data_value) - numgates;
		       gate[temp_gate].value = '1';
                       add_value(temp_gate, storevalue); 
		       }

	           else
		       {
		       temp_gate = current->data_value;
		       gate[temp_gate].value = '0';
                       add_value(temp_gate, storevalue); 
		       }

                   for(i=0;i<fanout[temp_gate]; i++)
		      {
		      int tmp_gate = fanoutlist[temp_gate][i];
		      int tmp_level = levelNum[tmp_gate];
		      add_event(tmp_gate, tmp_level, eventlist);
                      gate[tmp_gate].flag = 1;   
                      add_value(tmp_gate, storevalue); 
		      }

	 	   current = current->link;
	           }
             
               //For each level
	       temp=impl_count[0];
               for(i=1;i<=total_levels;i++)            
               {
                 //Calling the Logic Simulate function
                 logic_simulate(i, gate, ig, tim, levelNum, fanin, faninlist, fanout, fanoutlist, gtype, eventlist, pnode, pnode_final, storevalue, graph, send, impl_count);
               }

               dummy=pow(2, tim);
               if(tim>0)
               {
               send[temp+dummy+1]=tim;
               send[temp+dummy+2]=impl_count[0]-temp;  
               }
               else
	       {
               send[temp+1]=tim;
               send[temp+2]=impl_count[0]; 
	       }

               empty_value(gate, storevalue);
	  }
               send[0]=ig;
               MPI_Send(send, count+500, MPI_INT, 0, 0, MPI_COMM_WORLD);
    }
    }
}

////////////////////////////////////////////////////////////////////////
// other member functions here
////////////////////////////////////////////////////////////////////////

//Evaluation of gates to determine direct implications

void eval_direct_impl(int j, int count, unsigned char* gtype, short* fanin, int** faninlist, int **graph, node **pnode, node **pnode_final )
    {

    int nn;
    int listn;
    int data;
    int wt;

       switch (gtype[j])

         { //switch begins

          //INPUT
	  case 1:
	       break;

          //NAND gate
	  case 7:

                wt = 0;
		for (nn=0; nn<=(fanin[j]-1);nn++)
		{
                 listn = graph[j][0];
                 data =  graph[faninlist[j][nn]][1];
                 additem(listn, data, wt, pnode, pnode_final);

                 listn = graph[faninlist[j][nn]][0];
                 data = graph[j][1];
                 additem (listn, data, wt, pnode, pnode_final);
                }

	       break;

         //AND gate
         case 6:

                 wt = 0;
                 for (nn=0; nn<=(fanin[j]-1);nn++)
		  {
		  listn = graph[j][1];
		  data =  graph[faninlist[j][nn]][1];
		  additem(listn, data, wt, pnode, pnode_final);

		  listn = graph[faninlist[j][nn]][0];
		  data = graph[j][0];
		  additem (listn, data, wt, pnode, pnode_final);
                  }

		break;

        //OR gate
        case 8:

                wt = 0;
                for (nn=0; nn<=(fanin[j]-1);nn++)
		{
		listn = graph[j][0];
		data =  graph[faninlist[j][nn]][0];
		additem(listn, data, wt, pnode, pnode_final);

		listn = graph[faninlist[j][nn]][1];
		data = graph[j][1];
		additem (listn, data, wt, pnode, pnode_final);
                }

		  break;


       //NOR gate
       case 9:

                wt = 0;
                for (nn=0; nn<=(fanin[j]-1);nn++)
		{
		listn = graph[j][1];
		data =  graph[faninlist[j][nn]][0];
		additem(listn, data, wt, pnode, pnode_final);

		listn = graph[faninlist[j][nn]][1];
		data = graph[j][0];
		additem (listn, data, wt, pnode, pnode_final);
                }

		   break;

       //XOR gate
	case 3:
	         break;

       //XNOR gate
	case 4:
                 break;

	//NOT gate
        case 10:
                wt = 0;

                listn = graph[j][1];
		data =  graph[faninlist[j][0]][0];
		additem(listn, data, wt, pnode, pnode_final);

	        listn = graph[j][0];
		data =  graph[faninlist[j][0]][1];
		additem(listn, data, wt, pnode, pnode_final);

		listn = graph[faninlist[j][0]][1];
		data = graph[j][0];
		additem (listn, data, wt, pnode, pnode_final);

		listn = graph[faninlist[j][0]][0];
		data = graph[j][1];
		additem (listn, data, wt, pnode, pnode_final);

		break;

       //BUF gate
	case 11:

		wt = 0;

                listn = graph[j][1];
		data =  graph[faninlist[j][0]][1];
		additem(listn, data, wt, pnode, pnode_final);

	        listn = graph[j][0];
		data =  graph[faninlist[j][0]][0];
		additem(listn, data, wt, pnode, pnode_final);

		listn = graph[faninlist[j][0]][1];
		data = graph[j][1];
		additem (listn, data, wt, pnode, pnode_final);

		listn = graph[faninlist[j][0]][0];
		data = graph[j][0];
		additem (listn, data, wt, pnode, pnode_final);

		break;

      //TIE_1
      case 12:

		break;

      //TIE_0
      case 13:

		break;

       //OUTPUT
	case 2:

	        wt = 0;

                listn = graph[j][1];
	        data =  graph[faninlist[j][0]][1];
	        additem(listn, data, wt, pnode, pnode_final);

	        listn = graph[j][0];
		data =  graph[faninlist[j][0]][0];
		additem(listn, data, wt, pnode, pnode_final);

		listn = graph[faninlist[j][0]][1];
		data = graph[j][1];
                additem (listn, data, wt, pnode, pnode_final);

		listn = graph[faninlist[j][0]][0];
		data = graph[j][0];
		additem (listn, data, wt, pnode, pnode_final);

		break;

	//D_FF
	case 5:

		wt = -1;

                listn = graph[j][1];
		data =  graph[faninlist[j][0]][1];
		additem(listn, data, wt, pnode, pnode_final);

	        listn = graph[j][0];
		data =  graph[faninlist[j][0]][0];
		additem(listn, data, wt, pnode, pnode_final);

		wt = 1;
		listn = graph[faninlist[j][0]][1];
		data = graph[j][1];
		additem (listn, data, wt, pnode, pnode_final);

		listn = graph[faninlist[j][0]][0];
		data = graph[j][0];
		additem (listn, data, wt, pnode, pnode_final);

		break;

	} //switch ends

    } //function ends


void additem (int listn, int data, int wt, node **pnode, node **pnode_final)
	{
        node *newlink = new node;
	newlink->data_value = data;
	newlink->weight = wt;
	newlink->link = pnode[listn];
	pnode[listn] = newlink;

	node *newlink_final = new node;
	newlink_final->data_value = data;
	newlink_final->weight = wt;
	newlink_final->link = pnode_final[listn];
        pnode_final[listn] = newlink_final;
	}	

void additem_final (int listn, int data, int wt, node **pnode_final)
        {
	node *newlink_final = new node;
	newlink_final->data_value = data;
	newlink_final->weight = wt;
	newlink_final->link = pnode_final[listn];
        pnode_final[listn] = newlink_final;
	}

void display(int j, node **pnode)
       {
	  node *current = pnode[j];

          printf("\nlist number  %d \n", j);
	  printf("data     weight\n");
	  printf("----     ------\n");

	   while(current != NULL)
	     {
		printf("%d         %d\n", current->data_value, current->weight);
		current = current->link;
	     }

       }

//Functions for Indirect Implication Determination

//Depth First Search (DFS)

void dfs(int j, int net_wt, int source_node, node ***scheduling, node **pnode)
	 {
               node *current = pnode[j];
               while(current != NULL )
	   	 {
                   signed int temp0;
                   //signed int temp0_store = temp0;
                   int pass, flag_list;
                   temp0 = net_wt + current->weight ;
		   pass = temp0 + 3; 

                   if (temp0 <=3 && temp0 >=-3)
		        {
			flag_list = checkschedulelist(current->data_value, pass, source_node, scheduling);
                        if(flag_list == 0 && (current->data_value !=source_node))
			  {
                           addscheduling(current->data_value, pass, source_node, scheduling);
                           dfs(current->data_value, temp0, source_node, scheduling, pnode);
                          }
                    //    else
                     //   temp0 = temp0_store;
                        }

	   	    current = current->link;

                  }

         }

int checkschedulelist(int j, signed int net_wt, int source_node, node ***scheduling)
 {
     node *newlink = scheduling[source_node][net_wt];

	while(newlink!= NULL)
	{
	if(newlink->data_value == j)
        return 1;

	newlink = newlink->link;
	}
     return 0;
 }


//Maintaining the scheduling list to input to the logic simulator
//to determine indirect implications

void addscheduling (int data, signed int time_frame, int source_node, node ***scheduling)
  {

	node *newslink = new node;
	newslink->data_value = data;
	newslink->link = scheduling[source_node][time_frame];
        scheduling[source_node][time_frame] = newslink;

  }

void sdisplay(int j, node ***scheduling)
 {
   printf("\n scheduled gates for node %d \n", j);
   printf("----     \n");

   for(int yx = 0; yx<=6;yx++)
     {
     node *current = scheduling[j][yx];
     printf("\n time frame    %d \n", yx-3);
     printf("--------------------     \n");
	   while(current != NULL)
	     {
	     printf("\n%d\n", current->data_value);
	     current = current->link;
             }
     }

 }

//displaying the implication graph adjacency list
//including direct and indirect implications

void display_final(int j, node **pnode_final)
 {
  node *current = pnode_final[j];
  printf("\nlist number  %d \n", j);
  printf("data     weight\n");
  printf("----     ------\n");

   while(current != NULL)
     {
	printf("%d         %d\n", current->data_value, current->weight);
	current = current->link;
     }

 }

//Logic simulator function
void logic_simulate(int level_number, node *gate, int ig, int tim, int* levelNum, short* fanin, int** faninlist, short* fanout, int** fanoutlist, unsigned char* gtype, node **eventlist, node **pnode, node **pnode_final, node **storevalue, int **graph, int *send, int *impl_count)

       {
           node *delNode = new node;
           char temp;
           int i, num, dummy;
           int listnn, datan, wt;
           dummy=pow(2, tim);
           while(eventlist[level_number] != NULL)
	   {
           num = eventlist[level_number]->data_value;
           temp = gate[num].value;

           if(gtype[num] != 5)  
           {
           evaluate(num, gtype, gate, fanin, faninlist );
           gate[num].flag = 0;
           add_value(num, storevalue);
           }
           if((temp != gate[num].value))
	   {
                    for(i=0; i<fanout[num]; i++)
                    { 
                      if(gate[fanoutlist[num][i]].flag == 0)
		      { 
                      add_event(fanoutlist[num][i], levelNum[fanoutlist[num][i]], eventlist);
                      gate[fanoutlist[num][i]].flag = 1;
                      }
                    }


                    if(gate[num].value == '1')
		      {
			 listnn = ig;
		         datan = graph[num][1];
		         wt = tim -3; 

			 if(gtype[num] != 5 && listnn !=datan)
			 {
                         impl_count[0]++;

                         if(tim>0)
                         {
                         send[impl_count[0]+dummy+2]=datan;
                         //printf("\n Indirect Implication detected \n");
			 }
                         else
			 {
                         send[impl_count[0]+2]=datan;
                         //printf("\n Indirect Implication detected \n");
			 } 
                         // contrapositive_implications(listnn, datan, wt, count, pnode_final);
			 }
		      }
		    else if(gate[num].value == '0')
			{
                         listnn = ig;
		         datan = graph[num][0];
			 wt = tim - 3;	

			 if(gtype[num] != 5 && listnn !=datan)
			 {
                         impl_count[0]++;
                          if(tim>0)
                         {
                         send[impl_count[0]+dummy+2]=datan;
                         //printf("\n Indirect Implication detected \n");
			 }
                         else
			 {
                         send[impl_count[0]+2]=datan;
                         //printf("\n Indirect Implication detected \n");
			 } 
                         // contrapositive_implications(listnn, datan, wt, count, pnode_final);
			 }
			}
	   }
	   delNode = eventlist[level_number];
	   eventlist[level_number] = eventlist[level_number]->link;
	   delete delNode;
	   }
        }


//Gate evaluations for logic simulator
void evaluate (int j, unsigned char* gtype, node* gate, short* fanin, int** faninlist )
   {
    int nn;
    char out;
    bool out_int;

         switch (gtype[j])
              { //switch begins
			 //INPUT
		          case 1:
		          break;

                          //NAND gate
			  case 7:
                          out='0';
			  for (nn=0; nn<=(fanin[j]-1);nn++)
				     {
                                        if (gate[faninlist[j][nn]].value == '0')
					 {
					 gate[j].value='1';
					 return;
                                         }
                                         else if (gate[faninlist[j][nn]].value == 'X')
                                         out='X';
				     }
                                         gate[j].value=out; 
					 break;

                              //AND gate
                              case 6:

                                     out='1';
			             for (nn=0; nn<=(fanin[j]-1);nn++)
				     {
                                        if (gate[faninlist[j][nn]].value == '0')
					 {
					 gate[j].value='0';
					 return;
                                         }
                                         else if (gate[faninlist[j][nn]].value == 'X')
                                         out='X';
				     }
                                         gate[j].value=out; 
					 break;

                              //OR gate
                              case 8:

                                     out='0';
			             for (nn=0; nn<=(fanin[j]-1);nn++)
				     {
                                        if (gate[faninlist[j][nn]].value == '1')
					 {
					 gate[j].value='1';
					 return;
                                         }
                                         else if (gate[faninlist[j][nn]].value == 'X')
                                         out='X';
				     }
                                         gate[j].value=out; 
					 break;

                              //NOR gate
                              case 9:
                                     out='1';
			             for (nn=0; nn<=(fanin[j]-1);nn++)
				     {
                                        if (gate[faninlist[j][nn]].value == '1')
					 {
					 gate[j].value='0';
					 return;
                                         }
                                         else if (gate[faninlist[j][nn]].value == 'X')
                                         out='X';
				     }
                                         gate[j].value=out; 
					 break; 

                              //XOR gate
			            case 3:
                                      out_int=0;
                                      for (nn=0; nn<=(fanin[j]-1);nn++)
				         {
                                        if (gate[faninlist[j][nn]].value == 'X')
					 {
					 gate[j].value='X';
					 return;
                                         }
                                         else if (gate[faninlist[j][nn]].value == '1')
                                         out_int=!out_int;
				        }

                                        switch (out_int)
                                            {
                                         case 0: out = '0';
                                                 break;
                                         case 1: out = '1';
                                                 break;
                                            }
                                         gate[j].value=out; 
					 break; 

                             //XNOR gate
				    case 4:
				      out_int=0;
                                      for (nn=0; nn<=(fanin[j]-1);nn++)
				         {
                                        if (gate[faninlist[j][nn]].value == 'X')
					 {
					 gate[j].value='X';
					 return;
                                         }
                                         else if (gate[faninlist[j][nn]].value == '1')
                                         out_int=!out_int;
				        }

                                        switch (out_int)
                                            {
                                         case 0: out = '1';
                                                 break;
                                         case 1: out = '0';
                                                 break;
                                            }
                                         gate[j].value=out; 
					 break; 

			      //NOT gate
                              case 10:
				        if ( gate[faninlist[j][0]].value == '0')
				  	       {
					        gate[j].value='1';
					       }
					else if (gate[faninlist[j][0]].value == '1')
					       gate[j].value='0';
					else if (gate[faninlist[j][0]].value == 'X')
						{
						gate[j].value='X';
					        }
					break;

                             //BUF gate
			      case 11:

				  	gate[j].value=gate[faninlist[j][0]].value;
					break;

                              //TIE_1
                              case 12:
					gate[j].value='1';
					break;

                              //TIE_0
			      case 13:
				        gate[j].value = '0';
				        break;

                              //OUTPUT
			      case 2:
				        gate[j].value=gate[faninlist[j][0]].value;
				        break;

			      //D_FF
			      case 5:
                                        gate[j].value=gate[faninlist[j][0]].value;
				        break;

			   } //switch ends


		} //function ends

//Function to schedule gate onto the eventwheel
void add_event (int gate_number, int level_number, node **eventlist )
 	{
	node *current = new node;
	current->data_value =gate_number; 
	current->link = eventlist[level_number];
        eventlist[level_number] = current;
        }

void add_value (int data, node **storevalue)
  	{
	node *newlink = new node;
	newlink->data_value = data;
	newlink->link = storevalue[0];
        storevalue[0] = newlink;
	}

void empty_value (node *gate, node **storevalue)
   {
         node *delNode = new node;

	while(storevalue[0] != NULL)
	{
	gate[storevalue[0]->data_value].value = 'X';
	delNode = storevalue[0];
	storevalue[0] = storevalue[0]->link;
	delete delNode;
         }
   }

void contrapositive_implications(int listnn,int datan, signed wt, int count, node **pnode_final)
   {
        node *current = pnode_final[listnn];

             int data_pass = 0, weight_pass = 0, source_pass=0, temp=0;
                  if(listnn>(count-1))
                     {
                     data_pass = listnn - (count-1);
                     weight_pass = -(wt);
                     }
                   else
		    {
		    data_pass = listnn + count-1;
                    weight_pass = -(wt);
		    }

                   if(datan > (count-1))
                    {
                    source_pass = datan - (count-1);
                    }
                 else
                    source_pass = datan + count-1;

                   printf("\n contrapositive implications determined \n");
                   printf("\n %d %d %d \n", source_pass, data_pass, weight_pass);
                   additem_final (source_pass, data_pass, weight_pass, pnode_final);

       }




