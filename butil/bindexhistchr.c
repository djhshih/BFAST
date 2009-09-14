#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <limits.h>
#include <pthread.h>

#include "../blib/BLibDefinitions.h"
#include "../blib/BLib.h"
#include "../blib/BError.h"
#include "../blib/RGIndex.h"
#include "../blib/RGRanges.h"
#include "../blib/RGMatch.h"
#include "../blib/RGReads.h"
#include "bindexhistchr.h"

#define Name "bindexhistchr"
#define BINDEXHIST_ROTATE_NUM 1000000
#define NUM_MISMATCHES_START 0
#define NUM_MISMATCHES_END 4

/* Prints a histogram that counts the number of unique k-mers in the genome
 * that occur X number of times.  The k-mer chosen comes from the 
 * layout of the index.
 * */

int main(int argc, char *argv[]) 
{
	char indexFileName[MAX_FILENAME_LENGTH]="\0";
	char histogramFileName[MAX_FILENAME_LENGTH]="\0";
	char rgFileName[MAX_FILENAME_LENGTH]="\0";
	int numMismatchesStart = NUM_MISMATCHES_START;
	int numMismatchesEnd = NUM_MISMATCHES_END;
	int numThreads;
	int whichStrand;
	int chrIndex = 0;

	if(argc == 7) {
		RGBinary rg;
		RGIndex index;

		strcpy(rgFileName, argv[1]);
		strcpy(indexFileName, argv[2]);
		numMismatchesEnd = atoi(argv[3]);
		whichStrand = atoi(argv[4]);
		numThreads = atoi(argv[5]);
		chrIndex = atoi(argv[6]);
		assert(numMismatchesEnd >= numMismatchesStart);
		assert(whichStrand == BothStrands || whichStrand == ForwardStrand || whichStrand == ReverseStrand);

		/* Create the histogram file name */
		strcpy(histogramFileName, indexFileName);
		strcat(histogramFileName, ".histchr");

		/* Read in the rg binary file */
		RGBinaryReadBinary(&rg, rgFileName);

		/* Read the index */
		RGIndexRead(&index, indexFileName);

		assert(index.space == rg.space);

		fprintf(stderr, "%s", BREAK_LINE);
		PrintHistogram(&index, 
				&rg, 
				numMismatchesStart,
				numMismatchesEnd,
				whichStrand,
				numThreads,
				chrIndex,
				histogramFileName);
		fprintf(stderr, "%s", BREAK_LINE);

		fprintf(stderr, "%s", BREAK_LINE);
		fprintf(stderr, "Cleaning up.\n");
		/* Delete the index */
		RGIndexDelete(&index);
		/* Delete the rg */
		RGBinaryDelete(&rg);
		fprintf(stderr, "%s", BREAK_LINE);
		fprintf(stderr, "Terminating successfully!\n");
		fprintf(stderr, "%s", BREAK_LINE);
	}
	else {
		fprintf(stderr, "Usage: %s [OPTIONS]\n", Name);
		fprintf(stderr, "\t<bfast reference genome file name>\n");
		fprintf(stderr, "\t<bfast index file name>\n");
		fprintf(stderr, "\t<number of mismatches for histogram>\n");
		fprintf(stderr, "\t<0: consider both strands 1: forward strand only 2: reverse strand only>\n");
		fprintf(stderr, "\t<number of threads>\n");
		fprintf(stderr, "\t<chromosome index (one-based)>\n");
	}

	return 0;
}

/* TODO */
void GetPivots(RGIndex *index,
		RGBinary *rg,
		int64_t *starts,
		int64_t *ends,
		int64_t numThreads)
{
	/*
	   char *FnName="GetPivots";
	   */
	int64_t i, ind;
	int32_t returnLength, returnPosition;
	int8_t readInt[SEQUENCE_LENGTH];
	RGReads reads;
	RGRanges ranges;

	RGReadsInitialize(&reads);
	RGRangesInitialize(&ranges);

	/* One less than threads since numThreads-1 will divide
	 * the index into numThread parts */
	RGReadsAllocate(&reads, numThreads-1);
	for(i=0;i<reads.numReads;i++) {
		/* Get the place in the index */
		ind = (i+1)*((index->length)/numThreads);
		/* Initialize */
		reads.readLength[i] = index->width;
		reads.offset[i] = 0;
		/* Allocate memory */
		reads.reads[i] = NULL;
		/* Get read */
		RGBinaryGetReference(rg,
				(index->contigType == Contig_8)?(index->contigs_8[ind]):(index->contigs_32[ind]),
				index->positions[ind],
				FORWARD,
				0,
				&reads.reads[i],
				reads.readLength[i],
				&returnLength,
				&returnPosition);
		assert(returnLength == reads.readLength[i]);
		assert(returnPosition == index->positions[ind]);
	}
	/* Search reads in the index */
	/* Get the matches */
	for(i=0;i<reads.numReads;i++) {
		ConvertSequenceToIntegers(reads.reads[i], readInt, reads.readLength[i]);
		RGIndexGetRangesBothStrands(index,
				rg,
				readInt,
				reads.readLength[i],
				reads.offset[i],
				INT_MAX,
				INT_MAX,
				SpaceDoesNotMatter,
				ForwardStrand,
				&ranges);
	}

	/* Update starts and ends */
	starts[0] = 0;
	for(i=0;i<numThreads-1;i++) {
		starts[i+1] = ranges.endIndex[i]+1;
		ends[i] = ranges.endIndex[i];
	}
	ends[numThreads-1] = index->length-1;

	/*
	   for(i=0;i<numThreads;i++) {
	   fprintf(stderr, "%lld\t%lld\t%lld\n",
	   i,
	   starts[i],
	   ends[i]);
	   }
	   */

	/* Check */
	for(i=1;i<numThreads;i++) {
		assert(RGIndexCompareAt(index,
					rg,
					ends[i-1],
					starts[i],
					0)<0);
	}
	for(i=0;i<numThreads;i++) {
		assert(starts[i] <= ends[i]);
	}
	for(i=0;i<numThreads-1;i++) {
		assert(ends[i] == starts[i+1] - 1);
	}

}

/* TODO */
void PrintHistogram(RGIndex *index, 
		RGBinary *rg,
		int numMismatchesStart,
		int numMismatchesEnd,
		int whichStrand,
		int numThreads,
		int chr,
		char *histogramFileName)
{
	char *FnName = "PrintHistogram";
	int64_t i, j;
	int64_t *starts, *ends;
	pthread_t *threads=NULL;
	ThreadData *data=NULL;
	int errCode;
	void *status;
	FILE *fp;
	char tmpFileName[2048]="\0";
	int64_t numDifferent, numTotal, cur, sum, totalForward, totalReverse;
	int numCountsLeft;

	/* Not implemented for numMismatchesStart > 0 */
	assert(numMismatchesStart == 0);

	/* Allocate memory for the thread starts and ends */
	starts = malloc(sizeof(int64_t)*numThreads);
	if(NULL == starts) {
		PrintError(FnName,
				"starts",
				"Could not allocate memory",
				Exit,
				OutOfRange);
	}
	ends = malloc(sizeof(int64_t)*numThreads);
	if(NULL == ends) {
		PrintError(FnName,
				"ends",
				"Could not allocate memory",
				Exit,
				OutOfRange);
	}

	/* Allocate memory for threads */
	threads=malloc(sizeof(pthread_t)*numThreads);
	if(NULL==threads) {
		PrintError(FnName,
				"threads",
				"Could not allocate memory",
				Exit,
				MallocMemory);
	}
	/* Allocate memory to pass data to threads */
	data=malloc(sizeof(ThreadData)*numThreads);
	if(NULL==data) {
		PrintError(FnName,
				"data",
				"Could not allocate memory",
				Exit,
				MallocMemory);
	}

	/* Get pivots */
	GetPivots(index,
			rg,
			starts,
			ends,
			numThreads);

	/* Initialize thread data */
	numTotal = 0;
	for(i=0;i<numThreads;i++) {
		data[i].startIndex = starts[i];
		data[i].endIndex = ends[i];
		numTotal += ends[i] - starts[i] + 1;
		data[i].index = index;
		data[i].rg = rg;
		data[i].c.counts = NULL;
		data[i].c.maxCount = NULL;
		data[i].numMismatchesStart = numMismatchesStart;
		data[i].numMismatchesEnd = numMismatchesEnd;
		data[i].whichStrand = whichStrand;
		data[i].chr = chr;
		data[i].numDifferent = 0;
		data[i].threadID = i+1;
	}
	assert(numTotal == index->length || (ColorSpace == rg->space && numTotal == index->length - 1));

	fprintf(stderr, "In total, will examine at most %lld reads.\n",
			(long long int)(index->length));
	fprintf(stderr, "For a given thread, out of %lld, currently on:\n0",
			(long long int)(index->length/numThreads)
		   );

	/* Open threads */
	for(i=0;i<numThreads;i++) {
		/* Start thread */
		errCode = pthread_create(&threads[i], /* thread struct */
				NULL, /* default thread attributes */
				PrintHistogramThread, /* start routine */
				&data[i]); /* data to routine */
		if(0!=errCode) {
			PrintError(FnName,
					"pthread_create: errCode",
					"Could not start thread",
					Exit,
					ThreadError);
		}
	}
	/* Wait for threads to return */
	for(i=0;i<numThreads;i++) {
		/* Wait for the given thread to return */
		errCode = pthread_join(threads[i],
				&status);
		/* Check the return code of the thread */
		if(0!=errCode) {
			PrintError(FnName,
					"pthread_join: errCode",
					"Thread returned an error",
					Exit,
					ThreadError);
		}
	}
	fprintf(stderr, "\n");

	/* Get the total number of different */
	numDifferent = 0;
	totalForward = 0;
	totalReverse = 0;
	for(i=0;i<numThreads;i++) {
		numDifferent += data[i].numDifferent;
		totalForward += data[i].totalForward;
		totalReverse += data[i].totalReverse;
	}

	/* Print counts from threads */
	for(i=numMismatchesStart;i<=numMismatchesEnd;i++) {
		/* Create file name */
		sprintf(tmpFileName, "%s.%d.%lld",
				histogramFileName,
				index->width,
				(long long int)i);
		if(!(fp = fopen(tmpFileName, "w"))) {
			PrintError(FnName,
					tmpFileName,
					"Could not open file for writing",
					Exit,
					OpenFileError);
		}

		fprintf(fp, "# Number of unique reads was: %lld\n",
				(long long int)numDifferent);
		fprintf(fp, "# Found counts for %lld mismatches:\n",
				(long long int)i);

		/* Print the counts, sum over all threads */
		numCountsLeft = numThreads;
		cur = 0;
		while(numCountsLeft > 0) {
			/* Get the result from all threads */
			sum = 0;
			for(j=0;j<numThreads;j++) {
				if(cur <= data[j].c.maxCount[i]) {
					assert(data[j].c.counts[i][cur] >= 0);
					sum += data[j].c.counts[i][cur]; 
					/* Update */
					if(data[j].c.maxCount[i] == cur) {
						numCountsLeft--;
					}
				}
			}
			assert(sum >= 0);
			/* Print */
			if(cur>0) {
				fprintf(fp, "%lld\t%lld\n",
						(long long int)cur,
						(long long int)sum);
			}
			cur++;
		}
		fclose(fp);
	}

	/* Free memory */
	for(i=0;i<numThreads;i++) {
		for(j=numMismatchesStart;j<=numMismatchesEnd;j++) {
			free(data[i].c.counts[j]);
			data[i].c.counts[j] = NULL;
		}
		free(data[i].c.counts);
		data[i].c.counts=NULL;
		free(data[i].c.maxCount);
		data[i].c.maxCount = NULL;
	}
	free(threads);
	free(data);
	free(starts);
	starts=NULL;
	free(ends);
	ends=NULL;
}

void *PrintHistogramThread(void *arg)
{
	char *FnName = "PrintHistogramThread";

	/* Get thread data */
	ThreadData *data = (ThreadData*)arg;
	int64_t startIndex = data->startIndex;
	int64_t endIndex = data->endIndex;
	RGIndex *index = data->index;
	RGBinary *rg = data->rg;
	Counts *c = &data->c;
	int numMismatchesStart = data->numMismatchesStart;
	int numMismatchesEnd = data->numMismatchesEnd;
	int whichStrand = data->whichStrand;
	int threadID = data->threadID;
	int chr = data->chr;

	/* Local variables */
	int skip;
	int64_t i=0;
	int64_t j=0;
	int64_t curIndex=0, nextIndex=0;
	int64_t counter=0;
	int64_t numDifferent = 0;
	int64_t numReadsNoMismatches = 0;
	int64_t numReadsNoMismatchesTotal = 0;
	int64_t numForward, numReverse;
	int64_t totalForward, totalReverse; 
	int64_t numMatches;
	int32_t curChr;

	/* Not implemented for numMismatchesStart > 0 */
	assert(numMismatchesStart == 0);

	/* Allocate memory to hold histogram data */
	c->counts = malloc(sizeof(int64_t*)*(numMismatchesEnd - numMismatchesStart + 1));
	if(NULL == c->counts) {
		PrintError(FnName,
				"c->counts",
				"Could not allocate memory",
				Exit,
				MallocMemory);
	}
	c->maxCount = malloc(sizeof(int64_t)*(numMismatchesEnd - numMismatchesStart + 1));
	if(NULL == c->maxCount) {
		PrintError(FnName,
				"c->maxCount",
				"Could not allocate memory",
				Exit,
				MallocMemory);
	}

	/* Initialize counts */
	for(i=0;i<(numMismatchesEnd - numMismatchesStart + 1);i++) {
		c->counts[i] = malloc(sizeof(int64_t));
		if(NULL == c->counts[i]) {
			PrintError(FnName,
					"c->counts[i]",
					"Could not allocate memory",
					Exit,
					MallocMemory);
		}
		c->counts[i][0] = 0;
		c->maxCount[i] = 0;
	}

	/* Go through every possible read in the genome using the index */
	/* Go through the index */
	totalForward = 0;
	totalReverse = 0;
	for(curIndex=startIndex, nextIndex=startIndex, counter=0, numDifferent=0;
			curIndex <= endIndex;
			curIndex = nextIndex) {
		if(counter >= BINDEXHIST_ROTATE_NUM) {
			fprintf(stderr, "\rthreadID:%2d\t%10lld", 
					threadID,
					(long long int)(curIndex-startIndex));
			counter -= BINDEXHIST_ROTATE_NUM;
		}
		/* Try each mismatch */
		skip=0;

		curChr = (index->contigType == Contig_8)?(index->contigs_8[curIndex]):(index->contigs_32[curIndex]);
		if(chr == curChr) {
			for(i=numMismatchesStart;i<=numMismatchesEnd && skip == 0;i++) {

				/* Get the matches for the contig/pos */
				if(0==GetMatchesFromContigPos(index,
							rg,
							(index->contigType == Contig_8)?(index->contigs_8[curIndex]):(index->contigs_32[curIndex]),
							index->positions[curIndex],
							i,
							&numForward, 
							&numReverse) && 0 == i) {
					/* Skip over the rest */
					skip =1 ;
					nextIndex++;
				}
				else {
					numMatches = numForward + numReverse;
					assert(numMatches > 0);

					/* Update the value of numReadsNoMismatches and numDifferent
					 * if we have the results for no mismatches */
					if(i==0) {
						assert(numForward > 0);
						totalForward += numForward;
						assert(numReverse >= 0);
						totalReverse += numReverse;
						/* If the reverse compliment does not match the + strand then it will only match the - strand.
						 * Count it as unique as well as the + strand read.
						 * */
						if((BothStrands == whichStrand || ReverseStrand == whichStrand) &&
								numReverse == 0) {
							numDifferent+=2;
							numReadsNoMismatches = 2;
						}
						else {
							/* Count only the + strand as a unique read. */
							numReadsNoMismatches = 1;
							numDifferent++;
						}	
						/* This will be the basis for update c->counts */
						numReadsNoMismatchesTotal += numReadsNoMismatches;

						/* Add the range since we will be skipping over them */
						if(numForward <= 0) {
							nextIndex++;
							counter++;
						}
						else {
							nextIndex += numForward;
							counter += numForward;
						}
					}

					/* Add to our list.  We may have to reallocate this array */
					if(numMatches > c->maxCount[i]) {
						j = c->maxCount[i]+1; /* This will determine where we begin initialization after reallocation */
						/* Reallocate */
						c->maxCount[i] = numMatches;
						assert(c->maxCount[i] > 0);
						c->counts[i] = realloc(c->counts[i], sizeof(int64_t)*(c->maxCount[i]+1));
						if(NULL == c->counts[i]) {
							PrintError(FnName,
									"counts",
									"Could not allocate memory",
									Exit,
									MallocMemory);
						}
						/* Initialize from j to maxCount */
						while(j<=c->maxCount[i]) {
							c->counts[i][j] = 0;
							j++;
						}
					}
					assert(numReadsNoMismatches > 0);
					assert(numMatches <= c->maxCount[i]);
					assert(c->counts[i][numMatches] >= 0);
					/* Add the number of reads that were found with no mismatches */
					c->counts[i][numMatches] += numReadsNoMismatches;
					assert(c->counts[i][numMatches] > 0);
				}
			}
		}
		else {
			nextIndex++;
		}
	}
	fprintf(stderr, "\rthreadID:%2d\t%10lld", 
			threadID,
			(long long int)(curIndex-startIndex));

	/* Copy over numDifferent */
	data->numDifferent = numDifferent;
	data->totalForward = totalForward;
	data->totalReverse = totalReverse;

	return NULL;
}

/* Get the matches for the contig/pos */
int GetMatchesFromContigPos(RGIndex *index,
		RGBinary *rg,
		uint32_t curContig,
		uint32_t curPos,
		int numMismatches,
		int64_t *numForward,
		int64_t *numReverse)
{
	char *FnName = "GetMatchesFromContigPos";
	int returnLength, returnPosition;
	char *read=NULL;
	RGRanges ranges;
	int readLength = index->width;
	int8_t readInt[SEQUENCE_LENGTH];
	int32_t i;

	/* Initialize */
	RGRangesInitialize(&ranges);

	/* Get the read */
	RGBinaryGetReference(rg,
			curContig,
			curPos,
			FORWARD,
			0,
			&read,
			readLength,
			&returnLength,
			&returnPosition);
	assert(returnLength == readLength);
	assert(returnPosition == curPos);

	ConvertSequenceToIntegers(read, readInt, readLength);

	RGIndexGetRangesBothStrands(index,
			rg,
			readInt,
			readLength,
			0,
			INT_MAX,
			INT_MAX,
			rg->space,
			BothStrands,
			&ranges);

	/* This exploits the fact that the ranges are non-overlapping */
	RGRangesRemoveDuplicates(&ranges);

	assert(0 < ranges.numEntries);
	(*numForward) = (*numReverse) = 0;
	for(i=0;i<ranges.numEntries;i++) {
		switch(ranges.strand[i]) {
			case FORWARD:
				(*numForward) += ranges.endIndex[i] - ranges.startIndex[i] + 1;
				break;
			case REVERSE:
				(*numReverse) += ranges.endIndex[i] - ranges.startIndex[i] + 1;
				break;
			default:
				PrintError(FnName,
						NULL,
						"Could not understand strand",
						Exit,
						OutOfRange);
				break;
		}
	}

	assert((*numForward) > 0); /* It should at least match itself ! */
	assert((*numReverse) >= 0);

	RGRangesFree(&ranges);
	free(read);
	read=NULL;

	return 1;
}