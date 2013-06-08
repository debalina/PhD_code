#include<stdio.h>
#include<stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "scheduler.h"
#define MAX_NUM_FLOWS	200
#define SP_MAX  8	
#define SP_MIN	2
#define FRAME_DURATION 5
#define SIM_TIME 	50
#define SIM_FRAMES  18	
#define NO_CHANGE	0
#define RATE_INC	1
#define	RATE_DEC	-1
#define PERCENT	.25
#define THRESHOLD_INC 2
#define MAX_CHILDREN 4
#define MIN_SLOT_RATE	1
flow Flow[MAX_NUM_FLOWS];
int Tree[MAX_NUM_LINKS][MAX_CHILDREN];
typedef struct{
	int subchannel;
	int linkid;
	int rateChange;
}rateChange;
rateChange RateChange[15];
int numEntriesRateChange;
int numMS, numEVEN, numODD;

void readConfig(char*); 
//void printConfigFile(int Tree[MAX_NUM_LINKS][MAX_CHILDREN], link links[MAX_NUM_LINKS], Node node[MAX_NUM_NODES], int numNodes);
void readFlows();
void ReadRatesfromFile(char*);
void initialiseTreeStructure();
void initialiseLinks();
void initialiseFrames();
void computeTrafficDemand(FILE*);
int findMinRate(int linkid, int framecount);
int findLinkId(int nodeid);
int findMAX(DemandType demands[MAX_NUM_NODES]);
int computeProportion(NodeType Node, int framecount);
void computeSchedule(FILE *outfp, int framecount);
int findAllocatedBEOnThisLink(int linkid);
void  PropagateUpTheTree(int linkid, int BE);
void DropAndPropagate(int linkid, int Difference);
double computeMetric(void);

int main()
{
	int SP = SP_MAX;
	int stabilityCounter = 0;
	int countRateInc = 0;
	int frameCount = 0, totalframecount=0;;
	int SimulationTime = 0;
	int RateChangeCondition = 0;
	int adaptiveCondition = FALSE;
	time_t StartSimTime, CurSimTime;
	FILE *outfp;
	int  computeScheduleDone = FALSE;

	if ((outfp = fopen("output.txt", "w")) == NULL)
	{
			printf("Unable to open output file \n");
			exit(1);
	}
	initialiseLinks();
	initialiseFrames();
	readConfig("config2.txt");
	computeTrafficDemand(outfp);
	(void)time(&StartSimTime);
	fprintf(outfp, "Scheduling Period %d \n", totalframecount);
	computeSchedule(outfp, totalframecount);
	while(totalframecount <SIM_FRAMES)
	{
		sleep(FRAME_DURATION);
		frameCount++;
		totalframecount++;
		RateChangeCondition = ReadRateChange();
		
		switch(RateChangeCondition)
		{
			case NO_CHANGE:
			{
				stabilityCounter++;
				if ((stabilityCounter >= 2*SP) && (SP < SP_MAX))
				{
					if (SP * 2 > SP_MAX)
						SP = SP_MAX;
					else
						SP = SP * 2;
					stabilityCounter = 0;
				}//endif stabilityCounter > 2
				countRateInc = 0;	
				frameCount = 0;
				initialiseFrames();
				computeTrafficDemand(outfp);
				computeSchedule(outfp, totalframecount);
				printf("SP = %d\n", SP);
				break;
			}//end if rateChangeCondition == FALSE
	
			case RATE_INC:
			{
				countRateInc++;
				printf("countRateInc %d\n",countRateInc);
				if (countRateInc > THRESHOLD_INC)
				{
					computeTrafficDemand(outfp);
					adaptiveCondition = EvaluateRateIncCondition();
					if (adaptiveCondition == TRUE) 
					{
						if(SP/2 > SP_MIN)
							SP = SP/2;
						else
							SP = SP_MIN;

						initialiseFrames();
						computeSchedule(outfp, totalframecount);
						countRateInc = 0;
						frameCount = 0;
					} else {
						printf("Adaptive condition is false \n");
					}
				}
				printf("SP = %d\n", SP);
				break;
			}
			
			case RATE_DEC:
			{
				computeScheduleDone = rateDecrease(outfp, totalframecount);
				if (computeScheduleDone)
				{
					countRateInc = 0;
					frameCount = 0;
				}
				break;
			} 
		}
	(void)time(&CurSimTime);
	SimulationTime = CurSimTime - StartSimTime;
	printf("SimulationTime %d", SimulationTime);
	printf("totalframes %d SIM_FRAMES %d\n",totalframecount, SIM_FRAMES);
	//computeSchedule();
	}
	return 0;
	
}
double computeMetric()
{
	double metric;
	int linkid;
	int AllocatedTraffic=0;
	int DemandedTraffic = 0;

	for(linkid =0; linkid < MAX_NUM_LINKS; linkid++)
	{
		if(links[linkid].dst == 0) //0 is the base station index
		{
			if (links[linkid].AllocedUGS > links[linkid].trafficDemandUGS)
			{
				links[linkid].AllocedUGS = links[linkid].trafficDemandUGS;
			}
			if (links[linkid].AllocedertPs > links[linkid].trafficDemandertPs)
			{
				links[linkid].AllocedertPs = links[linkid].trafficDemandertPs;
			}
			if (links[linkid].AllocedrtPs > links[linkid].trafficDemandrtPs)
			{
				links[linkid].AllocedrtPs = links[linkid].trafficDemandrtPs;
			}
			if (links[linkid].AllocednrtPs > links[linkid].trafficDemandnrtPs)
			{
				links[linkid].AllocednrtPs = links[linkid].trafficDemandnrtPs;
			}
			if (links[linkid].AllocedBE > links[linkid].trafficDemandBE)
			{
				links[linkid].AllocedBE = links[linkid].trafficDemandBE;
			}
			//compute the weighted sum of all the allocated traffic
			AllocatedTraffic = AllocatedTraffic + (5*links[linkid].AllocedUGS) + (4* links[linkid].AllocedertPs) + (3* links[linkid].AllocedrtPs) + (2* links[linkid].AllocednrtPs) + links[linkid].AllocedBE;
			DemandedTraffic  = DemandedTraffic + (5*links[linkid].trafficDemandUGS) + (4* links[linkid].trafficDemandertPs) + (3* links[linkid].trafficDemandrtPs) + (2* links[linkid].trafficDemandnrtPs) + links[linkid].trafficDemandBE;
		}
	}

	metric = ((double)AllocatedTraffic)/((double)DemandedTraffic);
	return metric;
}

void ReadRatesfromFile(char *ratefile)
{
	FILE *fp;
	char line[1000];
	int lineLength;
	char* parseline;
	char *last=NULL;
	int linkid, subchannel, rate;
	int i,j;

	fp = fopen(ratefile,"r");
	if (fp == NULL)
		exit(EXIT_FAILURE);

	
	if (fgets(line,1000,fp)==NULL) 
		exit(EXIT_FAILURE);
					
	while ((fgets(line,1000,fp)!=NULL))
	{
		printf("line = %s\n", line);
		lineLength = strlen(line);
		if (lineLength == 1)
			break;	
		if ((parseline = (char *) malloc(lineLength+1)) == NULL) {
			printf("memory allocation failed \n");
		}
		parseline[lineLength] = '\0';
		strcpy(parseline,line);
		linkid = atoi(strtok_r(parseline," ",&last));
		subchannel = atoi(strtok_r(NULL," ",&last));
		rate = atoi(strtok_r(NULL," ",&last));
		links[linkid].rate[subchannel] = rate;
		for (i=0; i<MAX_NUM_NODES; i++)
		{
			for (j=0; j<NUM_TIMESLOTS; j++)
			{
				Frames[i].Slots[j][subchannel].currentModRate = rate;
			}
		}
	}

	//for debug purposes
	/*
	for (i=0; i< MAX_NUM_LINKS; i++)
	{
		printf("\n linkid = %d ", i);
		for (j=0; j < NUM_SUBCHANNELS; j++)
		{
			printf("%d ",links[i].rate[j]);
		}
	}
	*/	
	return;
}

int ReadRateChange()
{
	int rateChange; 
	int i=0;
	int id = 0;
	char c;
	int ugs, ertps, rtps, nrtps, be;
	printf("\n Enter Decrease (-1), Increase (+1), No Change (0) ");
	scanf("%d", &rateChange);
	if ((rateChange==-1)||(rateChange==1))
	{
		do
		{
			printf("\n subChannel, linkid, change in rate ");
			scanf("%d %d %d", &RateChange[i].subchannel, &RateChange[i].linkid, &RateChange[i].rateChange);
			//get the newline
			c = getchar();
			printf("Enter C to continue, any other character to exit");
			c = getchar();
			i++;
		}while ((c == 'C') || (c == 'c'));
	}
        numEntriesRateChange = i;	
	return rateChange;
}

int EvaluateRateIncCondition()
{
	int i,j, k,linkIndex, nodeIndex;
	int slotCounter = 0;
	int sumRateChange = 0;
	int sumOldRate = 0;
	int UnsatisfiedDemand = 0;
	int Condition = FALSE;

	
	for ( i=0; i < numEntriesRateChange; i++)
	{
		linkIndex = RateChange[i].linkid;
		nodeIndex = links[linkIndex].dst;
		//compute number of slots for that subchannel for that link
		for(k=0; k < NUM_TIMESLOTS; k++)
		{
			if (Frames[nodeIndex].Slots[k][RateChange[i].subchannel].linkId == RateChange[i].linkid)
			{
					sumOldRate = sumOldRate + Frames[j].Slots[k][RateChange[i].subchannel].currentModRate;
					slotCounter++;
			}
		}

		UnsatisfiedDemand = UnsatisfiedDemand + links[linkIndex].trafficDemandUGS + links[linkIndex].trafficDemandertPs + links[linkIndex].trafficDemandrtPs+ links[linkIndex].trafficDemandnrtPs+ links[linkIndex].trafficDemandBE -links[linkIndex].AllocedUGS - links[linkIndex].AllocedertPs - links[linkIndex].AllocedrtPs- links[linkIndex].AllocednrtPs- links[linkIndex].AllocedBE; 
		sumRateChange = sumRateChange + slotCounter * RateChange[i].rateChange;
		printf("SumRateChange %d SumOldRate %d \n", sumRateChange, sumOldRate);
		slotCounter = 0;
	}
	Condition = ((sumRateChange >= PERCENT * sumOldRate) && (UnsatisfiedDemand > 0.0));
	//change the rate in the link and frame structures
	if(Condition == TRUE)
	{
		for ( i=0; i < numEntriesRateChange; i++)
		{
			linkIndex = RateChange[i].linkid;
			links[linkIndex].rate[RateChange[i].subchannel] += RateChange[i].rateChange;
			nodeIndex = links[linkIndex].dst;
		//compute number of slots for that subchannel for that link
			for(k=0; k < NUM_TIMESLOTS; k++)
				if (Frames[nodeIndex].Slots[k][RateChange[i].subchannel].linkId == RateChange[i].linkid)
				{
					printf("currentModrate %d linkrate %d rateChange %d\n", Frames[nodeIndex].Slots[k][RateChange[i].subchannel].currentModRate, links[linkIndex].rate[RateChange[i].subchannel],RateChange[i].rateChange);
					Frames[nodeIndex].Slots[k][RateChange[i].subchannel].currentModRate += RateChange[i].rateChange; 
				}
		}
	}

	return Condition;
}


int rateDecrease(FILE *outfp, int framecount)
{
	int PerLinkTotalBw[MAX_NUM_LINKS];
	int sumAllRates[MAX_NUM_LINKS];
	int Difference = 0.0;
	int computeScheduleDone = FALSE;
	int i,j, subIndex;
	int debug;
	int linkIndex, linkid, nodeIndex;

	for(i=0; i < MAX_NUM_LINKS; i++)
	{
		PerLinkTotalBw[i] = 0;
		sumAllRates[i] = 0;
	}
	
	//compute per link information -- total allocated bw and capacity
	for ( i = 0; i < MAX_NUM_LINKS; i++)
	{
		PerLinkTotalBw[i] =  links[i].AllocedUGS + links[i].AllocedertPs + links[i].AllocedrtPs+ links[i].AllocednrtPs+ links[i].AllocedBE;
	}

	for ( i=0; i < numEntriesRateChange; i++)
	{
		linkIndex = RateChange[i].linkid;
		nodeIndex = links[linkIndex].dst;
		//compute number of slots for that subchannel for that link
		for(j=0; j < NUM_TIMESLOTS; j++)
		{
			printf("\n Framelinkid = %d, RateChangeLinkid = %d", (Frames[nodeIndex].Slots[j][RateChange[i].subchannel]).linkId, RateChange[i].linkid);
			if ((Frames[nodeIndex].Slots[j][RateChange[i].subchannel]).linkId == RateChange[i].linkid)
			{
				Frames[nodeIndex].Slots[j][RateChange[i].subchannel].currentModRate -= RateChange[i].rateChange; 
				links[linkIndex].rate[RateChange[i].subchannel] -= RateChange[i].rateChange;

			}//endif linkid matches
			for(subIndex = 0; subIndex < NUM_SUBCHANNELS; subIndex++)
			{
				if (Frames[nodeIndex].Slots[j][subIndex].linkId == RateChange[i].linkid)
				{
					sumAllRates[linkIndex] = sumAllRates[linkIndex] + (Frames[nodeIndex].Slots[j][subIndex]).currentModRate;
				}
			}
		}//endfor NUM_TIMESLOTS
	}// endfor numEntriesRateChange


	for(i=0; i < MAX_NUM_LINKS; i++)
	{
		//this link has suffered a change and the available capacity is less than required
			printf("\n RATE DECREASE linkid = %d, sumALlRate = %d PerLinkTotalBW= %d\n", i, sumAllRates[i], PerLinkTotalBw[i]);
		if ( (sumAllRates[i] != 0) && (sumAllRates[i] < PerLinkTotalBw[i])) 
		{
			// determine if there is space in the zone to accomodate the decrease
			//determine if dropping Be flows be sufficient
			Difference = PerLinkTotalBw[i] - sumAllRates[i];
			printf("\n RATE DECREASE linkid = %d, Difference = %d AllocedBE = %d\n", i, Difference, links[i].AllocedBE);
			if (Difference <= links[i].AllocedBE)
			{
				for(debug=0; debug<MAX_NUM_LINKS; debug++)
				{
					printf("\n linkid = %d, BEAlloced = %d", debug, links[debug].AllocedBE);
				}
				//Drop BE flows on this link and propagate the info up
				 DropAndPropagate(i, Difference);
				for(debug=0; debug<MAX_NUM_LINKS; debug++)
				{
					printf("\n linkid = %d, BEAlloced = %d", debug, links[debug].AllocedBE);
				}
			}//Difference can be met by dropping BE flows
			else
			{
				//re-route and  reschedule
				computeSchedule(outfp, framecount);
				computeScheduleDone = TRUE;
			}
				
		}//if the link has suffered a change
	}//for max-num-links
	return computeScheduleDone;
} 

int findAllocatedBEOnThisLink(int linkid)
{
	int j = 0;
	int BE;

	BE = links[linkid].AllocedBE;
	while(Tree[linkid][j] != -1)
	{
		BE = BE - links[Tree[linkid][j]].AllocedBE;
		j++;
	}
	return BE;
}

void PropagateUpTheTree(int linkid, int BE)
{
	int src = links[linkid].src;
	int i = 0;

	//release the BE flows and propagate the info up
	while ( src !=0 )
	{
			linkid = findLinkId(src);
			links[linkid].AllocedBE = links[linkid].AllocedBE - BE;
			src = links[linkid].dst;
	}
	return;
}

void DropAndPropagate(int linkid, int Difference)
{
	int j ;
	int BE;

	if (Difference <=0)
		return;
	BE = findAllocatedBEOnThisLink(linkid);
	PropagateUpTheTree(linkid, BE);
	Difference = Difference - BE;
	j=0;
	while(Tree[linkid][j] !=-1)
	{
		DropAndPropagate(Tree[linkid][j], BE);
		j++;
	}
	return;
}


void readConfig(char* configFileName)
{
	FILE *config;
	char str[100];
	int i,j;
	int linkid, src, dest, nodeid;
	int numLinks;
	int route;
	char c;
	int numNodes;
	int parentlink, index, childlink;
	int MSindex, EVENindex, ODDindex;

	if ((config = fopen(configFileName, "r")) == NULL)
	{
			printf("Unable to open config file \n");
			exit(1);
	}
	/*while (str[0] == '#')
	{
			fgets(str, 100, config);
	}*/
	fscanf(config,"%d",&numNodes);
	c = fgetc(config);
	printf("numNodes = %d\n", numNodes);

	fgets(str, 100, config);
	fgets(str, 100, config);
	for (i =0; i < MAX_NUM_LINKS ; i++)
	{
		
		do {
			fscanf(config, "%d %d %d", &parentlink, &index, &childlink);
			c = fgetc(config);
			Tree[parentlink][index]=childlink;
		
		} while (childlink!=-1);
	}

	fgets(str, 100, config);
	fgets(str, 100, config);
	//fscanf(config, "%d", &numRoutes);
	for ( i=0; i < MAX_NUM_LINKS ; i++)
	{
		fscanf(config, "%d %d %d", &linkid, &dest, &src);
		links[i].src = src; 
		links[i].dst = dest;
		c = fgetc(config);
	}
	fgets(str, 100, config);
	fgets(str, 100, config);

	for ( i=0; i < 3; i++)
	{
		fscanf(config, "%d", &nodeid);
		printf("nodeid = %d\n", nodeid);
		if (i== 0) 
		{
			MSindex = 0;
		} else if (i==1)
		{
			ODDindex = 0;
		} else
		{
			EVENindex = 0;
		}

		while (nodeid!= -1)
		{
			if (i==0)
			{
				MS.nodeId [MSindex]= nodeid;
				MSindex++;
			} else if (i==1)
			{
				ODD.nodeId[ODDindex] = nodeid;
				ODDindex++;
			} else 
			{	
				EVEN.nodeId[EVENindex] = nodeid;
				EVENindex++;
			}
			fscanf(config, "%d", &nodeid);
		}
		c=fgetc(config);
	}
      	
	MS.num = MSindex;
	ODD.num = ODDindex;
	EVEN.num = EVENindex;
	printf("MS = %d odd = %d even = %d\n", MS.num, ODD.num, EVEN.num);
	return;
}

void printConfigFile(int Tree[MAX_NUM_LINKS][MAX_CHILDREN], link links[MAX_NUM_LINKS],  int numNodes)
{
	int i, j;

	for (i=0; i<numNodes ; i++)
	{
		for (j=0; j<MAX_CHILDREN; j++)	
			printf("%d %d %d\n",i,j,Tree[i][j]);
		printf("\n");
	}
	
	printf("\n");
	printf("\n");
	for (i=0; i<numNodes; i++)
	{
		printf("%d %d %d\n",i, links[i].src, links[i].dst);
	}
}


void initialiseFrames()
{

	int i, j, k;

	for (i=0; i<MAX_NUM_NODES; i++)
	{
		for (j=0; j< NUM_TIMESLOTS; j++)
			for (k=0; k< NUM_SUBCHANNELS; k++)
			{
				Frames[i].Slots[j][k].allocatedFlag = FALSE; 
				Frames[i].Slots[j][k].linkId = -1;
				Frames[i].Slots[j][k].trafficClass = -1;	
				Frames[i].Slots[j][k].currentModRate = MIN_SLOT_RATE ;
			}
	}
	return;
}	



void initialiseLinks()
{
	int i, j;
	for (i = 0; i < MAX_NUM_LINKS; i++)
	{
		links[i].trafficDemandUGS = 0;
		links[i].trafficDemandertPs = 0;
		links[i].trafficDemandrtPs = 0;
		links[i].trafficDemandnrtPs = 0;
		links[i].trafficDemandBE = 0;
		links[i].AllocedUGS = 0;
		links[i].AllocedertPs = 0;
		links[i].AllocedrtPs = 0;
		links[i].AllocednrtPs = 0;
		links[i].AllocedBE = 0;
		links[i].slotsAlloced = 0;
		for (j=0; j <NUM_SUBCHANNELS;j++)
		{
			links[i].rate[j] = MIN_SLOT_RATE; //most robust rate;
			links[i].subchannelsAlloced[j] = 0; //no subchannels allocated
		}
		
	}
}

void computeTrafficDemand(FILE *outfp)
{

	int i;
	int index;
	int child;

	for (i=0; i<MAX_NUM_LINKS; i++)
	{
		links[i].trafficDemandUGS = 0;
		links[i].trafficDemandertPs = 0;
		links[i].trafficDemandrtPs = 0;
		links[i].trafficDemandnrtPs = 0;
		links[i].trafficDemandBE = 0;
	}
	for (i= MAX_NUM_LINKS - 1 ; i>=0; i--)
	{
		if (Tree[i][0]==-1) 
		{
			//link starts at an MS
			printf("enter demands for link %d for service class UGS, ertps, rtps, nrtps and BE ",i);
			scanf("%d %d %d %d %d", &links[i].trafficDemandUGS, &links[i].trafficDemandertPs, &links[i].trafficDemandrtPs, &links[i].trafficDemandnrtPs, &links[i].trafficDemandBE);
		}
	}
	for (i= MAX_NUM_LINKS - 1 ; i>=0; i--)
	{
		if (Tree[i][0]!=-1) 
		{
			index = 0;
			while (Tree[i][index]!=-1)
			{
				child = Tree[i][index];	
				links[i].trafficDemandUGS += links[child].trafficDemandUGS;
				links[i].trafficDemandertPs += links[child].trafficDemandertPs;
				links[i].trafficDemandrtPs += links[child].trafficDemandrtPs;
				links[i].trafficDemandnrtPs += links[child].trafficDemandnrtPs;
				links[i].trafficDemandBE += links[child].trafficDemandBE;
				index++;
			}
		}
	}
	for (i=0;i< MAX_NUM_LINKS;i++)
	{
		fprintf(outfp,"link %d UGS %d ertps %d rtps %d nrtps %d BE %d", i, links[i].trafficDemandUGS,links[i].trafficDemandertPs, links[i].trafficDemandrtPs, links[i].trafficDemandnrtPs, links[i].trafficDemandBE);
		fprintf(outfp,"\n");
	}
}


int computeTotalDemand(int linkId)
{
	int trafficDemandUGS;
	int trafficDemandertPs;
	int trafficDemandrtPs;
	int trafficDemandnrtPs;
	int trafficDemandBE;
 	int TotalDemand = links[linkId].trafficDemandUGS + links[linkId].trafficDemandertPs + links[linkId].trafficDemandrtPs+ links[linkId].trafficDemandnrtPs+ links[linkId].trafficDemandBE; 
	return TotalDemand;
}



void ScheduleZone(NodeType nodes)
{
	int linkId;
	int i,j, totalLinks;
	int minUGSDemand;
	int minertPsDemand;
	int minrtPsDemand;
	int minnrtPsDemand;
	int minBEDemand;
	int linkIDslist[MAX_NUM_LINKS];
	int bwAlloc;

	//schedule by traffic class  UGS, ertPS, rtPS, nrtPS and finally BE
	//
	//
	do {
		j= 0;
		for (i = 0; i < nodes.num; i++)
		{
			linkId = findLinkId(nodes.nodeId[i]);
			printf("schedulezone nodeid %d \n",nodes.nodeId[i]);
			links[linkId].UGSDemand = (links[linkId].trafficDemandUGS - links[linkId].AllocedUGS);
			//find all links with nonzero demand and store those link ids
			if ((links[linkId].UGSDemand > 0) && (links[linkId].noFreeSlots == FALSE))
			{
				linkIDslist[j] = linkId;
				j++;
			}
		}
		totalLinks = j;

		
		minUGSDemand = links[linkIDslist[0]].UGSDemand;
		for (i=0; i< totalLinks; i++)
		{
			if (links[linkIDslist[i]].UGSDemand < minUGSDemand)
			{
				minUGSDemand = links[linkIDslist[i]].UGSDemand;
			}
		}
		for (i=0; i< totalLinks; i++)
		{
			bwAlloc = AllocateSlots(minUGSDemand, 0, linkIDslist[i]);
			if (bwAlloc < minUGSDemand)
			{
				links[linkIDslist[i]].noFreeSlots = TRUE;
			}
			printf("schedulezone linkid %d \n",linkIDslist[i]);
			links[linkIDslist[i]].AllocedUGS += bwAlloc; 
		}
	} while (totalLinks > 0);

	//ertps
	do {
		j= 0;
		for (i = 0; i < nodes.num; i++)
		{
			linkId = findLinkId(nodes.nodeId[i]);
			links[linkId].ertPsDemand = (links[linkId].trafficDemandertPs - links[linkId].AllocedertPs);
			//find all links with nonzero demand and store those link ids
			if ((links[linkId].ertPsDemand > 0) && (links[linkId].noFreeSlots == FALSE))
			{
				linkIDslist[j] = linkId;
				j++;
			}
		}
		totalLinks = j;

		
		minertPsDemand = links[linkIDslist[0]].ertPsDemand;
		for (i=0; i< totalLinks; i++)
		{
			if (links[linkIDslist[i]].ertPsDemand < minertPsDemand)
			{
				minertPsDemand = links[linkIDslist[i]].ertPsDemand;
			}
		}
		for (i=0; i< totalLinks; i++)
		{
			bwAlloc = AllocateSlots(minertPsDemand, 0, linkIDslist[i]);
			if (bwAlloc < minertPsDemand)
			{
				links[linkIDslist[i]].noFreeSlots = TRUE;
			}
			links[linkIDslist[i]].AllocedertPs += bwAlloc; 
		}
	} while (totalLinks > 0);
	
	//rtPs	
	do {
		j= 0;
		for (i = 0; i < nodes.num; i++)
		{
			linkId = findLinkId(nodes.nodeId[i]);
			links[linkId].rtPsDemand = (links[linkId].trafficDemandrtPs - links[linkId].AllocedrtPs);
			//find all links with nonzero demand and store those link ids
			if ((links[linkId].rtPsDemand > 0) && (links[linkId].noFreeSlots == FALSE))
			{
				linkIDslist[j] = linkId;
				j++;
			}
		}
		totalLinks = j;

		
		minrtPsDemand = links[linkIDslist[0]].rtPsDemand;
		for (i=0; i< totalLinks; i++)
		{
			if (links[linkIDslist[i]].rtPsDemand < minrtPsDemand)
			{
				minrtPsDemand = links[linkIDslist[i]].rtPsDemand;
			}
		}
		for (i=0; i< totalLinks; i++)
		{
			bwAlloc = AllocateSlots(minrtPsDemand, 0, linkIDslist[i]);
			if (bwAlloc < minrtPsDemand)
			{
				links[linkIDslist[i]].noFreeSlots = TRUE;
			}
			links[linkIDslist[i]].AllocedrtPs += bwAlloc; 
		}
	} while (totalLinks > 0);

	//nrtPs
	do {
		j= 0;
		for (i = 0; i < nodes.num; i++)
		{
			linkId = findLinkId(nodes.nodeId[i]);
			links[linkId].nrtPsDemand = (links[linkId].trafficDemandnrtPs - links[linkId].AllocednrtPs);
			//find all links with nonzero demand and store those link ids
			if ((links[linkId].nrtPsDemand > 0) && (links[linkId].noFreeSlots == FALSE))
			{
				linkIDslist[j] = linkId;
				j++;
			}
		}
		totalLinks = j;

		
		minnrtPsDemand = links[linkIDslist[0]].nrtPsDemand;
		for (i=0; i< totalLinks; i++)
		{
			if (links[linkIDslist[i]].nrtPsDemand < minnrtPsDemand)
			{
				minnrtPsDemand = links[linkIDslist[i]].nrtPsDemand;
			}
		}
		for (i=0; i< totalLinks; i++)
		{
			bwAlloc = AllocateSlots(minnrtPsDemand, 0, linkIDslist[i]);
			if (bwAlloc < minnrtPsDemand)
			{
				links[linkIDslist[i]].noFreeSlots = TRUE;
			}
			links[linkIDslist[i]].AllocednrtPs += bwAlloc; 
		}
	} while (totalLinks > 0);

	//BE
	do {
		j= 0;
		for (i = 0; i < nodes.num; i++)
		{
			linkId = findLinkId(nodes.nodeId[i]);
			links[linkId].BEDemand = (links[linkId].trafficDemandBE - links[linkId].AllocedBE);
			//find all links with nonzero demand and store those link ids
			if ((links[linkId].BEDemand > 0) && (links[linkId].noFreeSlots == FALSE))
			{
				linkIDslist[j] = linkId;
				j++;
			}
		}
		totalLinks = j;

		
		minBEDemand = links[linkIDslist[0]].BEDemand;
		for (i=0; i< totalLinks; i++)
		{
			if (links[linkIDslist[i]].BEDemand < minBEDemand)
			{
				minBEDemand = links[linkIDslist[i]].BEDemand;
			}
		}
		for (i=0; i< totalLinks; i++)
		{
			bwAlloc = AllocateSlots(minBEDemand, 0, linkIDslist[i]);
			if (bwAlloc < minBEDemand)
			{
				links[linkIDslist[i]].noFreeSlots = TRUE;
			}
			links[linkIDslist[i]].AllocedBE += bwAlloc; 
		}
	} while (totalLinks > 0);


			

	return;
}
	

void computeSchedule(FILE *outfp, int framecount)
{
	double AZProp = 0;
	double OZProp = 0;
	double EZProp = 0;
	double DivisionProp;
	int i, j, k;
	double  AllocatedAZSlots, AllocatedOZSlots, AllocatedEZSlots;
	double TotalAvailableSlots, TotalRequiredSlots;
	int linkid;
	FILE *fp;

	AZProp = computeProportion( MS, framecount);
	OZProp = computeProportion( ODD, framecount);
	EZProp = computeProportion( EVEN, framecount);

	fprintf(outfp,"AZprop = %lf, OZProp = %lf, EZProp = %lf\n", AZProp, OZProp, EZProp);
	//Computing the division proportion
	TotalAvailableSlots = NUM_TIMESLOTS*NUM_SUBCHANNELS;
	TotalRequiredSlots = AZProp + OZProp + EZProp;
	//DivisionProp = (double)(NUM_TIMESLOTS * NUM_SUBCHANNELS)/(AZProp + OZProp + EZProp); 
	AllocatedAZSlots = ceil ((AZProp * TotalAvailableSlots)/TotalRequiredSlots) ;
	AllocatedOZSlots = ceil ((OZProp * TotalAvailableSlots)/TotalRequiredSlots) ;
	AllocatedEZSlots = ceil ((EZProp * TotalAvailableSlots)/TotalRequiredSlots) ;
	//AllocatedOZSlots = (double)OZProp/DivisionProp;
//	AllocatedEZSlots = (double)EZProp/DivisionProp;
	fprintf(outfp,"AZSlots %lf OZSlots %lf EZSlots %lf\n", AllocatedAZSlots, AllocatedOZSlots, AllocatedEZSlots);
	
	//Dividing the frame based on the above, we divide it on the time line 
	// So the number of subchannels is fixed to the MAX available subchannels for each zone
	// The equal sign is present in the for loop as we have to consider BS also
	for(i=0; i<MAX_NUM_NODES; i++)
	{
		Frames[i].AZCutOffTimeSlot = ceil (AllocatedAZSlots/NUM_SUBCHANNELS);
		if (Frames[i].AZCutOffTimeSlot == 0)
		{
			Frames[i].AZCutOffTimeSlot = 1;
		}

		Frames[i].OZCutOffTimeSlot = Frames[i].AZCutOffTimeSlot + ceil(AllocatedOZSlots/NUM_SUBCHANNELS);
		if (Frames[i].OZCutOffTimeSlot == Frames[i].AZCutOffTimeSlot)
		{
			Frames[i].OZCutOffTimeSlot = Frames[i].AZCutOffTimeSlot + 1;
		}
		//Frames[i].EZCutOffTimeSlot = Frames[i].OZCutOffTimeSlot + AllocatedEZSlots/NUM_SUBCHANNELS;
		Frames[i].EZCutOffTimeSlot = NUM_TIMESLOTS;	
	}
	fprintf(outfp," AZ %d OZ %d EZ %d\n",  Frames[0].AZCutOffTimeSlot, Frames[0].OZCutOffTimeSlot, Frames[0].EZCutOffTimeSlot);

	//Reinitiliaze links subchannelsalloced and nofreeslots
	for (i=0; i< MAX_NUM_LINKS; i++)
	{
		links[i].slotsAlloced = 0;
		links[i].noFreeSlots = FALSE;
		for (j=0; j< NUM_SUBCHANNELS; j++)
		{
			links[i].subchannelsAlloced[j] = 0;
		}
		links[i].AllocedUGS = 0;
		links[i].AllocedertPs = 0;
		links[i].AllocedrtPs = 0;
		links[i].AllocednrtPs = 0;
		links[i].AllocedBE = 0;

	}		

	//At this point the frame.EZCutOffTimeSlot = NUM_TIMESLOTS
	
	//schedule access links
	ScheduleZone(MS);

	//schedule ODD zone
	ScheduleZone(ODD);

	//schedule EVEN zone
	ScheduleZone(EVEN);

	for (i=0; i<MAX_NUM_NODES; i++)
	{
		fprintf(outfp, "Frame %d \n",i);
		printf("Frame %d \n",i);
		fprintf(outfp,"----------------------------------------------------------------------------------------------------------------\n");
		for (j=0; j<NUM_TIMESLOTS;j++)
		{
			for (k=0; k< NUM_SUBCHANNELS; k++)
			{
				
				fprintf(outfp," %d[%d] \t", Frames[i].Slots[j][k].linkId, Frames[i].Slots[j][k].currentModRate);
				printf(" %d %d", Frames[i].Slots[j][k].linkId, Frames[i].Slots[j][k].currentModRate);
			}
			fprintf(outfp,"\n");
			printf("\n");
		}

	}
	

	for (i=0; i< MAX_NUM_LINKS; i++)
	{

		fprintf(outfp, "Link %d ",i);

		for (j=0; j<NUM_SUBCHANNELS; j++)
		{
			if (links[i].subchannelsAlloced[j] == 1)
			{
				fprintf(outfp, " %d ",j);
			}
		}
		fprintf(outfp, "UGS %d ertPS %d rtPs %d nrtPs %d BE %d\n", links[i].AllocedUGS, links[i].AllocedertPs, links[i].AllocedrtPs, links[i].AllocednrtPs, links[i].AllocedBE);
		fprintf(outfp,"\n");
	}	
	printf("\n The Weighted Throuput Metric = %lf\nn", computeMetric());	
	fprintf(outfp,"\n The Weighted Throuput Metric = %lf\nn", computeMetric());	

	return;
}


int AllocateSlots(int bwDemand, int serviceClass, int linkId)
{

	int numslots = 0;
	int bwAlloc = 0;
	int numSubChannel = 0;
	int parent, src, framebdryStart, framebdryEnd;
	frame *currentframe;
	int i;
	int subchannel;
	
	parent = links[linkId].dst;
	src = links[linkId].src;
	for (i=0; i<MS.num; i++)
	{
		//node is an MS
		if (MS.nodeId[i]==src)
		{
			framebdryStart = 0;
			framebdryEnd = Frames[parent].AZCutOffTimeSlot;
		}
	}
	for (i=0; i<ODD.num; i++)
	{
		// node is ODD
		if (ODD.nodeId[i]==src)
		{
			framebdryStart = Frames[parent].AZCutOffTimeSlot;
			framebdryEnd = Frames[parent].OZCutOffTimeSlot;
		}
	}
	for (i=0; i<EVEN.num; i++)
	{
		if (EVEN.nodeId[i]==src)
		{
			framebdryStart = Frames[parent].OZCutOffTimeSlot;
			framebdryEnd = NUM_TIMESLOTS;
		}
	}

	printf("Allocate Slots for link %d\n",linkId);	

	subchannel = FindBestAvailableSubChannel(linkId);
	numSubChannel = 0;
	bwAlloc = 0;
	while ((numSubChannel < NUM_SUBCHANNELS )&& (bwAlloc < bwDemand))
	{
		numSubChannel++;
		for (i=framebdryStart; i < framebdryEnd; i++)
		{
			if (Frames[parent].Slots[i][subchannel].allocatedFlag == FALSE)
			{
			//Mark Slots allocated
				Frames[parent].Slots[i][subchannel].allocatedFlag = TRUE;
				Frames[parent].Slots[i][subchannel].linkId = linkId;
				Frames[parent].Slots[i][subchannel].trafficClass = serviceClass;
				printf("currentModRate %d subchannel rate %d\n", Frames[parent].Slots[i][subchannel].currentModRate, links[linkId].rate[subchannel]);	
				Frames[parent].Slots[i][subchannel].currentModRate = links[linkId].rate[subchannel] ;	
				bwAlloc = bwAlloc + links[linkId].rate[subchannel];
				links[linkId].slotsAlloced++;
				links[linkId].subchannelsAlloced[subchannel] = 1;
			}
			if (bwAlloc >= bwDemand)
				break;
		}
		subchannel++;
		subchannel = (subchannel) % (NUM_SUBCHANNELS);
		if (bwAlloc >= bwDemand)
			break;
	}
	return  bwAlloc;
}

int FindBestAvailableSubChannel(linkId)
{
	int i;
	int maxRate = 0;
	int maxRateSubchannelIndex;

	// find the maximum rate subchannel for this link
	for(i=0; i < NUM_SUBCHANNELS; i++)
	{
		if (maxRate < links[linkId].rate[i])
		{
			maxRate = links[linkId].rate[i];
			maxRateSubchannelIndex = i;
		}
	}
	return maxRateSubchannelIndex;
}


int computeProportion(NodeType node, int framecount)
{
	int Demands[MAX_NUM_NODES];
	int i, j;
	int minrate[MAX_NUM_NODES];
	int Proportion = 0;
	DemandType  SumDemands[MAX_NUM_NODES];
	int linkid;

	for (i=0; i<node.num; i++)
	{
		// find linkid which has the node as the src
		// find what is the minrate among all the subchannels
		// compute demand 
		if ((linkid = findLinkId(node.nodeId[i]))!=-1)
		{
			minrate[i] = findMinRate(linkid, framecount);
			Demands[i] = computeTotalDemand(linkid)/minrate[i];
		}
	}
			
	for (i = 0; i < MAX_NUM_NODES; i++)
	{
		SumDemands[i].totalDemand = 0;
		SumDemands[i].dst = i;
	}	

	//Sum the demands of all links with same parent	
	for (i = 0; i < node.num; i++)
	{
		if (node.nodeId[i]!=0) //not the BS
		{
			
			linkid = findLinkId(node.nodeId[i]);
			//printf("NodeType %d linkid %d \n", node.nodeId[i], linkid);
			for (j=0; j< MAX_NUM_NODES; j++)
			{
				if (links[linkid].dst == SumDemands[j].dst)
				{
					SumDemands[j].totalDemand += Demands[i];
					//printf("parent = %d linkid = %d Demands = %d SumDemand %d = %d\n",SumDemands[j].dst, linkid, Demands[i], j, SumDemands[j].totalDemand);
				}	
			}
		}
	}	
	for (i = 0; i < node.num; i++)
	{
		//printf("SumDemand %d = %d\n", i, SumDemands[i].totalDemand);
	}

	Proportion = findMAX(SumDemands);
	return Proportion;
}

int findMAX(DemandType demands[MAX_NUM_NODES])
{
	int maxDemand;
	int i;

	maxDemand = demands[0].totalDemand;
	for (i = 1; i < MAX_NUM_NODES; i++)
	{
		if (demands[i].totalDemand > maxDemand)
		{
			maxDemand = demands[i].totalDemand;
		}
	}
	return maxDemand;
}
int findLinkId(int nodeid)
{
	int i;

	for (i=0; i<MAX_NUM_LINKS; i++)
	{
		if (links[i].src == 0) //BS is the src
			return -1;
		if (links[i].src == nodeid)
			return i;
	}
	return -1;
}

int findMinRate(int linkid, int framecount)
{
	int minrate = MAX_SLOT_RATE;
	int i;

	if (framecount == 0)
	{
		//initialisation
		return MIN_SLOT_RATE;
	}
	for (i=0; i < NUM_SUBCHANNELS; i++)
	{
	
		// if subchannel was used in previous scheduling period
		// consider that subchannel in finding minrate
		if (links[linkid].subchannelsAlloced[i]==1)
		{
			if (minrate > links[linkid].rate[i])
			{
				minrate = links[linkid].rate[i];
			}
		}
	}
	return minrate;
}
