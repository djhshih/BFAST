#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <limits.h>
#include <sys/types.h>
#include <string.h>
#include <pthread.h>
#include "BLibDefinitions.h"
#include "BError.h"
#include "BLib.h"
#include "RGBinary.h"
#include "RGRanges.h"
#include "RGIndex.h"

/* TODO */
void RGIndexCreate(RGIndex *index, 
		RGBinary *rg, 
		RGIndexLayout *layout, 
		int32_t space,
		int32_t startContig,
		int32_t startPos,
		int32_t endContig,
		int32_t endPos,
		int32_t layoutIndex,
		int32_t numThreads,
		int32_t repeatMasker,
		int32_t includeNs,
		char *tmpDir) 
{

	/* The sort will take care of most of the work.  We just want 
	 * to make sure that we only include sequences that agrees with
	 * repeatMasker and includeNs
	 * */

	char *FnName = "RGIndexCreate";

	/* For storing the bases */
	int8_t bases[SEQUENCE_LENGTH]="\0";
	int32_t basesLength=0;
	int32_t basesIndex=0;
	int32_t curBasesPos=0; 
	int32_t toInsert=1;
	int32_t curPos=-1;
	int32_t curStartPos=-1;
	int32_t curEndPos=-1;
	int32_t curContig=-1;
	int64_t i;

	/* Make sure we have the correct reference genome */
	assert(rg->space == space);

	/* Initialize the index */
	RGIndexInitialize(index);

	/* Copy over index information from the rg */
	assert(startContig <= endContig);
	assert(startContig < endContig || (startContig == endContig && startPos <= endPos));
	index->startContig = startContig;
	index->startPos = startPos;
	index->endContig = endContig;
	index->endPos = endPos;
	/* Adjust bounds */
	AdjustBounds(rg,
			&index->startContig,
			&index->startPos,
			&index->endContig,
			&index->endPos);
	assert(index->startContig > 0);
	assert(index->endContig > 0);

	/* Copy over other metadata */
	index->id = BFAST_ID;
	index->repeatMasker = repeatMasker;
	index->space = space;

	/* Copy over index information from the layout */
	index->hashWidth = layout->hashWidths[layoutIndex];
	assert(index->hashWidth > 0);
	index->width = layout->widths[layoutIndex];
	assert(index->width > 0);
	index->keysize = layout->keysizes[layoutIndex];
	assert(index->keysize > 0);
	index->mask = malloc(sizeof(int32_t)*layout->widths[layoutIndex]);
	if(NULL == index->mask) {
		PrintError(FnName,
				"index->mask",
				"Could not allocate memory",
				Exit,
				MallocMemory);
	}
	/* Copy over mask */
	for(i=0;i<layout->widths[layoutIndex];i++) {
		index->mask[i] = layout->masks[layoutIndex][i];
	}
	/* Infer the length of the hash */
	assert(ALPHABET_SIZE==4);
	index->hashLength = pow(4, index->hashWidth);
	assert(index->hashLength > 0);
	/* Decide if we should use 1 byte or 4 byte to store the contigs. 
	 * We subtract one when comparing to UCHAR_MAX because we index 
	 * starting at one, not zero. */ 
	index->contigType = (rg->numContigs < UCHAR_MAX)?Contig_8:Contig_32;

	if(VERBOSE >= 0) {
		fprintf(stderr, "Currently on [contig,pos]:\n");
		fprintf(stderr, "\r[%d,%d]",
				-1,
				-1);
	}
	/* For each contig */
	for(curContig=index->startContig;
			curContig <= index->endContig;
			curContig++) { 
		/* Update contig index */

		/* Update start and end bounds for this contig */
		curStartPos = (curContig==startContig)?startPos:1;
		curEndPos = (curContig==endContig)?endPos:1;

		/* Initialize variables */
		basesLength = 0; /* Have not looked at any bases */
		basesIndex = 0; 

		/* For each position */
		for(curPos=curStartPos;curPos<=curEndPos;curPos++) {
			if(VERBOSE >= 0) {
				if(curPos%RGINDEX_ROTATE_NUM==0) {
					fprintf(stderr, "\r[%d,%d]",
							curContig,
							curPos);
				}
			}

			/* Get the current base and insert into bases */
			basesLength++;
			bases[basesIndex] = RGBinaryGetBase(rg,
					curContig,
					curPos);
			/* Update where to put the next base */
			basesIndex = (basesIndex+1)%index->width;

			/* Check if we have enough bases */
			if(basesLength < index->width) {
				/* Do nothing since we do not have enough bases */
			}
			else {
				basesLength=index->width;

				/* Find the starting position, this is equal to the current position since period is the same as the total length */
				curBasesPos = basesIndex;
				toInsert = 1;

				for(i=0;i<index->width && 1==toInsert;i++) { /* For each base in the mask */
					if(1==index->mask[i]) {
						if(1==repeatMasker && 1==RGBinaryIsBaseRepeat(bases[curBasesPos])) {
							/* Did not pass */
							toInsert = 0;
						}
						else if(0==includeNs && 1==RGBinaryIsBaseN(bases[curBasesPos])) {
							/* Did not pass */
							toInsert = 0;
						}
					}
					/* Update position in bases */
					curBasesPos = (curBasesPos+1)%index->width;
				}

				/* See if we should insert into the index.  We should have enough consecutive bases. */
				if(1==toInsert) {
					/* Insert */
					index->length++;

					/* Reallocate memory */
					/* Copy over.  Remember that we are at the end of the read. */
					index->positions = realloc(index->positions, sizeof(uint32_t)*index->length);
					if(NULL == index->positions) {
						PrintError("RGBinaryCreate",
								"index->positions",
								"Could not reallocate memory",
								Exit,
								ReallocMemory);
					}
					index->positions[index->length-1] = curPos - index->width + 1;
					/* Reallocate memory for the contigs based on contig type and copy over. */
					if(index->contigType == Contig_8) {
						index->contigs_8 = realloc(index->contigs_8, sizeof(uint8_t)*index->length);
						if(NULL == index->contigs_8) {
							PrintError("RGBinaryCreate",
									"index->contigs_8",
									"Could not reallocate memory",
									Exit,
									ReallocMemory);
						}
						index->contigs_8[index->length-1] = curContig;
					}
					else {
						index->contigs_32 = realloc(index->contigs_32, sizeof(uint32_t)*index->length);
						if(NULL == index->contigs_32) {
							PrintError("RGBinaryCreate",
									"index->contigs_32",
									"Could not reallocate memory",
									Exit,
									ReallocMemory);
						}
						index->contigs_32[index->length-1] = curContig;
					}
				}
			}
		}
	}
	if(VERBOSE >= 0) {
		fprintf(stderr, "\r[%d,%d]\n",
				curContig-1,
				curPos-1);
	}

	assert(index->length > 0);

	/* Sort the nodes in the index */
	if(VERBOSE >= 0) {
		fprintf(stderr, "Sorting...\n");
	}
	RGIndexSortNodes(index, rg, numThreads, tmpDir);
	if(VERBOSE >= 0) {
		fprintf(stderr, "Sorted.\n");
	}

	/* Create hash table from the index */
	RGIndexCreateHash(index, rg);

}

/* TODO */
void RGIndexCreateHash(RGIndex *index, RGBinary *rg)
{
	char *FnName = "RGIndexCreateHash";
	uint32_t start, end;
	uint32_t curHash, startHash;
	int64_t i;

	if(index->length >= UINT_MAX) {
		PrintError(FnName,
				"index->length",
				"Index length has reached its maximum",
				Exit,
				OutOfRange);
	}

	/* Allocate memory for the hash */
	index->starts = malloc(sizeof(uint32_t)*index->hashLength);
	if(NULL==index->starts) {
		PrintError(FnName,
				"index->starts",
				"Could not allocate memory",
				Exit,
				MallocMemory);
	}

	index->ends = malloc(sizeof(uint32_t)*index->hashLength);
	if(NULL==index->ends) {
		PrintError(FnName,
				"index->ends",
				"Could not allocate memory",
				Exit,
				MallocMemory);
	}

	/* initialize */
	for(i=0;i<index->hashLength;i++) {
		/* Can't use -1, so use UINT_MAX */
		index->starts[i] = UINT_MAX;
		index->ends[i] = UINT_MAX;
	}

	/* Go through index and update the hash */
	if(VERBOSE >= 0) {
		fprintf(stderr, "Creating a hash. Out of %u, currently on:\n0",
				(uint32_t)index->length);
	}
	startHash = RGIndexGetHashIndex(index, rg, 0, 0);
	for(end=1, start=0;end < index->length;end++) {
		if(VERBOSE >= 0 && end%RGINDEX_ROTATE_NUM==0) {
			fprintf(stderr, "\r%u", end);
		}
		curHash = RGIndexGetHashIndex(index, rg, end, 0);
		assert(curHash >= startHash);
		if(curHash == startHash) {
			/* Do nothing */
		}
		else {
			/* Paranoia check */
			assert(startHash < curHash);
			assert(curHash != startHash);
			/* Check that it is within bounds */
			if(startHash < 0 || startHash >= index->hashLength) {
				fprintf(stderr, "%s: %lld\t%lld\n",
						FnName,
						(long long int)startHash,
						(long long int)index->hashLength);
			}
			assert(startHash >= 0 && startHash < index->hashLength);
			/* Check that it has not been already initialized */
			if(index->starts[startHash] != UINT_MAX) {
				fprintf(stderr, "%s: %lld\t%lld\n",
						FnName,
						(long long int)startHash,
						(long long int)index->starts[startHash]);
			}
			assert(index->starts[startHash] == UINT_MAX);
			if(index->ends[startHash] != UINT_MAX) {
				fprintf(stderr, "%s: %lld\t%lld\n",
						FnName,
						(long long int)startHash,
						(long long int)index->ends[startHash]);
			}
			assert(index->ends[startHash] == UINT_MAX);

			/* Store start and end */
			index->starts[startHash] = start;
			index->ends[startHash] = end-1;

			/* Check correctness */
			if(index->starts[startHash] > 0 && index->starts[startHash] != UINT_MAX) {
				assert( RGIndexCompareAt(index, rg, index->starts[startHash]-1, index->starts[startHash], 0) < 0);
			}
			if(index->ends[startHash] < index->length-1 && index->ends[startHash] != UINT_MAX) {
				assert( RGIndexCompareAt(index, rg, index->ends[startHash], index->ends[startHash]+1, 0) < 0);
			}

			/* Update start */
			start = end;
			startHash = curHash;
		}
	}
	if(VERBOSE >= 0) {
		fprintf(stderr, "\rHash created.\n");
	}
	/* In the boundary case... */
	/* Store start and end */
	index->starts[startHash] = start;
	index->ends[startHash] = end-1;

	/* Test hash creation */
	/*
	   for(i=0;i<index->hashLength;i++) {
	   assert( (index->starts[i] == UINT_MAX && index->ends[i] == UINT_MAX) ||
	   (index->starts[i] != UINT_MAX && index->ends[i] != UINT_MAX));
	   if(index->starts[i] > 0 && index->starts[i] != UINT_MAX) {
	   assert( RGIndexCompareAt(index, rg, index->starts[i]-1, index->starts[i], 0) < 0);
	   }
	   if(index->ends[i] < index->length-1 && index->ends[i] != UINT_MAX) {
	   assert( RGIndexCompareAt(index, rg, index->ends[i], index->ends[i]+1, 0) < 0);
	   }
	   }
	   */
}

/* TODO */
void RGIndexSortNodes(RGIndex *index, RGBinary *rg, int32_t numThreads, char* tmpDir)
{
	char *FnName = FnName;
	int64_t i, j;
	ThreadRGIndexSortData *data=NULL;
	ThreadRGIndexSortData tempData;
	pthread_t *threads=NULL;
	int32_t errCode;
	void *status=NULL;
	int64_t *pivots=NULL;
	int64_t max, maxIndex;
	double curPercentComplete = 0.0;

	/* Only use threads if we want to divide and conquer */
	if(numThreads > 1) {
		/* Should check that the number of threads is a power of 4 since we split
		 * in half in both sorts. */
		assert(IsAPowerOfTwo(numThreads)==1);

		/* Allocate memory for the thread arguments */
		data = malloc(sizeof(ThreadRGIndexSortData)*numThreads);
		if(NULL==data) {
			PrintError(FnName,
					"data",
					"Could not allocate memory",
					Exit,
					MallocMemory);
		}
		/* Allocate memory for the thread point32_ters */
		threads = malloc(sizeof(pthread_t)*numThreads);
		if(NULL==threads) {
			PrintError(FnName,
					"threads",
					"Could not allocate memory",
					Exit,
					MallocMemory);
		}

		if(SORT_TYPE == 0) {
			/* Quick sort */

			/* Allocate memory for the pivots */
			pivots = malloc(sizeof(int64_t)*(2*numThreads));
			if(NULL == pivots) {
				PrintError(FnName,
						"pivots",
						"Could not allocate memory",
						Exit,
						MallocMemory);
			}

			for(i=0;i<2*numThreads;i++) {
				pivots[i] = -1;
			}

			/* Get the pivots and presort */
			fprintf(stderr, "\rInitializing...");
			RGIndexQuickSortNodesGetPivots(index,
					rg,
					0,
					index->length-1,
					pivots,
					1,
					numThreads);
			/* The last one must be less than index->length */
			pivots[2*numThreads-1]--;

			/* Check pivots */
			for(i=0;i<2*numThreads;i+=2) {
				if(!(pivots[i] >= 0 && pivots[i] < index->length)) {
					fprintf(stderr, "offender\ti:%d\tlength:%d\n",
							(int)i,
							(int)index->length);
					for(i=0;i<2*numThreads;i+=2) {
						fprintf(stderr, "i:%d\t%d\t%d\n",
								(int)i,
								(int)pivots[i],
								(int)pivots[i+1]);
					}
					exit(1);
				}
				assert(pivots[i] >= 0 && pivots[i] < index->length);
				assert(pivots[i+1] >= 0 && pivots[i+1] < index->length);
				assert(pivots[i] <= pivots[i+1]);
				if(i==0) {
					assert(pivots[i] == 0);
				}
				if(i+1==2*numThreads-1) {
					assert(pivots[i+1] == index->length-1);
				}
				if(i>1 && i%2==0) {
					assert(pivots[i] == pivots[i-1] + 1);
				}
				if(i>1) {
					assert(pivots[i] > pivots[i-1]);
				}
			}

			/* Initialize data */
			maxIndex=0;
			max = data[0].high-data[0].low;
			for(i=0;i<numThreads;i++) {
				data[i].index = index;
				data[i].rg = rg;
				data[i].threadID = i;
				data[i].low = pivots[2*i];
				data[i].high = pivots[2*i+1];
				data[i].showPercentComplete = 0;
				data[i].tmpDir = NULL;
				data[i].mergeMemoryLimit = -1;
				assert(data[i].low >= 0 && data[i].high < index->length);
				if(data[i].high - data[i].low >= max) {
					maxIndex = i;
				}
			}
			data[maxIndex].showPercentComplete = 1;

			/* Check that we split correctly */
			for(i=1;i<numThreads;i++) {
				assert(data[i-1].high < data[i].low);
			}

			/* Copy maxIndex to the front so that it is the first that we wait for... */
			tempData.low = data[0].low;
			tempData.high = data[0].high;
			tempData.threadID = data[0].threadID;
			tempData.showPercentComplete = data[0].showPercentComplete;
			data[0].low = data[maxIndex].low;
			data[0].high = data[maxIndex].high;
			data[0].threadID = data[maxIndex].threadID;
			data[0].showPercentComplete = data[maxIndex].showPercentComplete;
			data[maxIndex].low = tempData.low;
			data[maxIndex].high = tempData.high;
			data[maxIndex].threadID = tempData.threadID;
			data[maxIndex].showPercentComplete = tempData.showPercentComplete;

			/* Create threads */
			for(i=0;i<numThreads;i++) {
				/* Start thread */
				errCode = pthread_create(&threads[i], /* thread struct */
						NULL, /* default thread attributes */
						RGIndexQuickSortNodes, /* start routine */
						(void*)(&data[i])); /* data to routine */
				if(0!=errCode) {
					PrintError(FnName,
							"pthread_create: errCode",
							"Could not start thread",
							Exit,
							ThreadError);
				}
			}

			/* Wait for the threads to finish */
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
				if(i==maxIndex && VERBOSE >= 0) {
					fprintf(stderr, "\rWaiting for other threads to complete...");
				}
			}
			/* Free memory */
			free(pivots);
		}
		else if(SORT_TYPE == 1) {
			/* Merge sort with tmp file I/O */

			/* Initialize data */
			for(i=0;i<numThreads;i++) {
				data[i].index = index;
				data[i].rg = rg;
				data[i].threadID = i;
				data[i].low = i*(index->length/numThreads);
				data[i].high = (i+1)*(index->length/numThreads)-1;
				data[i].showPercentComplete = 0;
				data[i].tmpDir = tmpDir;
				/* Divide the maximum overhead by the number of threads */
				data[i].mergeMemoryLimit = MERGE_MEMORY_LIMIT/((int64_t)numThreads); 
				assert(data[i].low >= 0 && data[i].high < index->length);
			}
			data[0].low = 0;
			data[numThreads-1].high = index->length-1;
			data[numThreads-1].showPercentComplete = 1;

			/* Check that we split correctly */
			for(i=1;i<numThreads;i++) {
				assert(data[i-1].high < data[i].low);
			}

			/* Create threads */
			for(i=0;i<numThreads;i++) {
				/* Start thread */
				errCode = pthread_create(&threads[i], /* thread struct */
						NULL, /* default thread attributes */
						RGIndexMergeSortNodes, /* start routine */
						(void*)(&data[i])); /* data to routine */
				if(0!=errCode) {
					PrintError(FnName,
							"pthread_create: errCode",
							"Could not start thread",
							Exit,
							ThreadError);
				}
			}

			/* Wait for the threads to finish */
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
				if(i==numThreads-1 && VERBOSE >= 0) {
					fprintf(stderr, "\rWaiting for other threads to complete...");
				}
			}

			if(VERBOSE >= 0) {
				fprintf(stderr, "\rMerging sorts from threads...                    ");
			}

			/* Now we must merge the results from the threads */
			/* Merge intelligently i.e. merge recursively so 
			 * there are only nlogn merges where n is the 
			 * number of threads. */
			for(j=1;j<numThreads;j=j*2) {
				for(i=0;i<numThreads;i+=2*j) {
					RGIndexMergeHelper(index,
							rg,
							data[i].low,
							data[i+j].low-1,
							data[i+2*j-1].high,
							MERGE_MEMORY_LIMIT,
							tmpDir);
				}
			}
		}
		else {
			PrintError(FnName,
					"SORT_TYPE",
					"Could not understand sort type",
					Exit,
					OutOfRange);
		}

		/* Free memory */
		free(threads);
		threads=NULL;
		free(data);
		data=NULL;
	}
	else {
		if(SORT_TYPE == 0) {
			if(VERBOSE >= 0) {
				fprintf(stderr, "\r0 percent complete");
			}
			RGIndexQuickSortNodesHelper(index,
					rg,
					0,
					index->length-1,
					1);
			if(VERBOSE >= 0) {
				fprintf(stderr, "\r100.00 percent complete\n");
			}
		}
		else if(SORT_TYPE == 1) {
			if(VERBOSE >= 0) {
				fprintf(stderr, "\r0 percent complete");
			}
			RGIndexMergeSortNodesHelper(index,
					rg,
					0,
					index->length-1,
					1,
					&curPercentComplete,
					0,
					index->length-1,
					MERGE_MEMORY_LIMIT,
					tmpDir);
			if(VERBOSE >= 0) {
				fprintf(stderr, "\r100.00 percent complete\n");
			}
		}
		else {
			PrintError(FnName,
					"SORT_TYPE",
					"Could not understand sort type",
					Exit,
					OutOfRange);
		}
	}

	/* Test that we sorted correctly */
	/*
	for(i=1;i<index->length;i++) {
		if(RGIndexCompareAt(index, rg, i-1, i, 0) > 0) {
			RGIndexCompareAt(index, rg, i-1, i, 1);
		}
		assert(RGIndexCompareAt(index, rg, i-1, i, 0) <= 0);
	}
	*/
}

/* TODO */
void *RGIndexQuickSortNodes(void *arg)
{
	/* thread arguments */
	ThreadRGIndexSortData *data = (ThreadRGIndexSortData*)(arg);

	/* Call helper */
	if(data->showPercentComplete == 1 && VERBOSE >= 0) {
		fprintf(stderr, "\r%3.3lf percent complete", 0.0);
	}
	RGIndexQuickSortNodesHelper(data->index,
			data->rg,
			data->low,
			data->high,
			data->showPercentComplete);
	if(data->showPercentComplete == 1 && VERBOSE >= 0) {
		fprintf(stderr, "\r");
		fprintf(stderr, "thread %3.3lf percent complete", 100.0);
	}

	return arg;
}

/* TODO */
/* Call stack was getting too big, implement non-recursive sort */
void RGIndexQuickSortNodesHelper(RGIndex *index,
		RGBinary *rg,
		int64_t low,
		int64_t high,
		int32_t showPercentComplete)
{
	/* Stack for log n space and non-recursive implementation */
	int64_t *lowStack=NULL;
	int64_t *highStack=NULL;
	int64_t stackLength=0;

	/* Local Variables */
	int64_t i;
	int64_t pivot = 0;
	int64_t total = high-low+1;
	int64_t curLow, curHigh;
	double curPercent = 0.0;

	/* Initialize stack */
	stackLength=1;
	lowStack = malloc(sizeof(int64_t));
	if(NULL==lowStack) {
		PrintError("RGIndexQuickSortNodesHelper",
				"lowStack",
				"Could not allocate memory",
				Exit,
				MallocMemory);
	}
	highStack = malloc(sizeof(int64_t));
	if(NULL==highStack) {
		PrintError("RGIndexQuickSortNodesHelper",
				"highStack",
				"Could not allocate memory",
				Exit,
				MallocMemory);
	}
	lowStack[0] = low;
	highStack[0] = high;

	/* Continue while the stack is not empty */
	while(stackLength > 0) {
		/* Pop off the stack */
		curLow = lowStack[stackLength-1];
		curHigh = highStack[stackLength-1];
		stackLength--;

		/* Reallocate memory */
		lowStack = realloc(lowStack, sizeof(int64_t)*stackLength);
		if(NULL==lowStack && stackLength > 0) {
			PrintError("RGIndexQuickSortNodesHelper",
					"lowStack",
					"Could not reallocate memory",
					Exit,
					ReallocMemory);
		}
		highStack = realloc(highStack, sizeof(int64_t)*stackLength);
		if(NULL==highStack && stackLength > 0) {
			PrintError("RGIndexQuickSortNodesHelper",
					"highStack",
					"Could not reallocate memory",
					Exit,
					ReallocMemory);
		}

		/* Proceed if we are with range */
		if(curLow < curHigh && curLow >= low && curHigh <= high) {
			/* Choose a new pivot.  We could do this randomly (randomized quick sort)
			 * but lets just choose the median of the front, middle and end 
			 * */
			pivot = RGIndexGetPivot(index, rg, curLow, curHigh);
			assert(pivot >=0 && pivot<index->length);
			assert(curLow >=0 && curLow<index->length);
			assert(curHigh >=0 && curHigh<index->length);

			if(showPercentComplete == 1 && VERBOSE >= 0) {
				if(curPercent < 100.0*((double)(curLow - low))/total) {
					while(curPercent < 100.0*((double)(curLow - low))/total) {
						curPercent += RGINDEX_SORT_ROTATE_INC;
					}
					PrintPercentCompleteLong(curPercent);
				}
			}

			/* Swap the node at pivot with the node at curHigh */
			RGIndexSwapAt(index, pivot, curHigh);

			/* Store where the pivot should be */
			pivot = curLow;

			for(i=curLow;i<curHigh;i++) {
				assert(pivot >= 0 && pivot <= curHigh); 
				assert(i>=0 && i <= curHigh);
				if(RGIndexCompareAt(index, rg, i, curHigh, 0) <= 0) {
					/* Swap node at i with node at the new pivot index */
					if(i!=pivot) {
						RGIndexSwapAt(index, pivot, i);
					}
					/* Increment the new pivot index */
					pivot++;
				}
			}

			/* Move pivot element to correct place */
			if(pivot != curHigh) {
				RGIndexSwapAt(index, pivot, curHigh);
			}

			/* Add to the stack */
			stackLength+=2;
			/* Reallocate memory */
			lowStack = realloc(lowStack, sizeof(int64_t)*stackLength);
			if(NULL==lowStack) {
				PrintError("RGIndexQuickSortNodesHelper",
						"lowStack",
						"Could not reallocate memory",
						Exit,
						ReallocMemory);
			}
			highStack = realloc(highStack, sizeof(int64_t)*stackLength);
			if(NULL==highStack) {
				PrintError("RGIndexQuickSortNodesHelper",
						"highStack",
						"Could not reallocate memory",
						Exit,
						ReallocMemory);
			}
			/* Add sub array below */
			lowStack[stackLength-1] = curLow;
			highStack[stackLength-1] = pivot-1;
			/* Add sub array above */
			lowStack[stackLength-2] = pivot+1;
			highStack[stackLength-2] = curHigh;
		}
	}
}

/* TODO */
void RGIndexQuickSortNodesGetPivots(RGIndex *index,
		RGBinary *rg,
		int64_t low,
		int64_t high,
		int64_t *pivots,
		int32_t lowPivot,
		int32_t highPivot)
{
	/* local variables */
	int64_t i;
	int64_t pivot = 0;

	if(low < high ) {
		/* Choose a new pivot.  We could do this randomly (randomized quick sort)
		 * but lets just choose the median of the front, middle and end 
		 * */
		pivot = RGIndexGetPivot(index, rg, low, high);
		assert(pivot >=0 && pivot<index->length);
		assert(low >=0 && low<index->length);
		assert(high >=0 && high<index->length);

		/* Partition the array.
		 * Basically, arrange everything from low to high so that everything that
		 * has value less than or equal to the pivot is on the low of the pivot, and
		 * everthing else (greater than) is on the high side. 
		 * */

		/* Swap the node at pivot with the node at high */
		RGIndexSwapAt(index, pivot, high);

		/* Store where the pivot should be */
		pivot = low;

		for(i=low;i<high;i++) {
			assert(pivot >= 0 && pivot <= high); 
			assert(i>=0 && i <= high);
			if(RGIndexCompareAt(index, rg, i, high, 0) <= 0) {
				/* Swap node at i with node at the new pivot index */
				if(i!=pivot) {
					RGIndexSwapAt(index, pivot, i);
				}
				/* Increment the new pivot index */
				pivot++;
			}
		}

		/* Move pivot element to correct place */
		if(pivot != high) {
			RGIndexSwapAt(index, pivot, high);
		}

		if(lowPivot >= highPivot) {
			assert(pivots!=NULL);
			/* Save pivots if necessary */
			pivots[2*lowPivot-2] = low;
			pivots[2*lowPivot-1] = high+1;
			return;
		}
		else {
			/* Call recursively */

			/* Sort below */
			assert(pivot-1 < high);
			RGIndexQuickSortNodesGetPivots(index, 
					rg, 
					low, 
					pivot-1,
					pivots, 
					lowPivot, 
					(lowPivot+highPivot)/2);
			/* Sort above */
			assert(pivot+1 > low);
			RGIndexQuickSortNodesGetPivots(index, 
					rg, 
					pivot+1, 
					high, 
					pivots, 
					(lowPivot+highPivot)/2 + 1, 
					highPivot);
		}
	}
	else {
		/* Special case when saving pivots */
		assert(pivots!=NULL);
		/* Save pivots if necessary */
		pivots[2*lowPivot-2] = low;
		pivots[2*lowPivot-1] = low;
		return;
	}
}

/* TODO */
void *RGIndexMergeSortNodes(void *arg)
{
	/* thread arguments */
	ThreadRGIndexSortData *data = (ThreadRGIndexSortData*)(arg);
	double curPercentComplete = 0.0;

	/* Call helper */
	if(data->showPercentComplete == 1 && VERBOSE >= 0) {
		fprintf(stderr, "\r%3.3lf percent complete", 0.0);
	}
	RGIndexMergeSortNodesHelper(data->index,
			data->rg,
			data->low,
			data->high,
			data->showPercentComplete,
			&curPercentComplete,
			data->low,
			data->high - data->low,
			data->mergeMemoryLimit,
			data->tmpDir);
	if(data->showPercentComplete == 1 && VERBOSE >= 0) {
		fprintf(stderr, "\r");
		fprintf(stderr, "thread %3.3lf percent complete", 100.0);
	}

	return arg;
}

/* TODO */
/* Call stack was getting too big, implement non-recursive sort */
void RGIndexMergeSortNodesHelper(RGIndex *index,
		RGBinary *rg,
		int64_t low,
		int64_t high,
		int32_t showPercentComplete,
		double *curPercentComplete,
		int64_t startLow,
		int64_t total,
		int64_t mergeMemoryLimit,
		char *tmpDir)
{
	/* Local Variables */
	int64_t mid = (low + high)/2;

	if(low >= high) {
		if(VERBOSE >= 0 &&
				showPercentComplete == 1) {
			assert(NULL!=curPercentComplete);
			if((*curPercentComplete) < 100.0*((double)(low - startLow))/total) {
				while((*curPercentComplete) < 100.0*((double)(low - startLow))/total) {
					(*curPercentComplete) += RGINDEX_SORT_ROTATE_INC;
				}
				PrintPercentCompleteLong((*curPercentComplete));
			}
		}
		return;
	}
	/* Partition the list into two lists and sort them recursively */
	RGIndexMergeSortNodesHelper(index,
			rg,
			low,
			mid,
			showPercentComplete,
			curPercentComplete,
			startLow,
			total,
			mergeMemoryLimit,
			tmpDir);
	RGIndexMergeSortNodesHelper(index,
			rg,
			mid+1,
			high,
			showPercentComplete,
			curPercentComplete,
			startLow,
			total,
			mergeMemoryLimit,
			tmpDir);

	/* Merge the two lists */
	RGIndexMergeHelper(index,
			rg,
			low,
			mid,
			high,
			mergeMemoryLimit,
			tmpDir);
}

/* TODO */
void RGIndexMergeHelper(RGIndex *index,
		RGBinary *rg,
		int64_t low,
		int64_t mid,
		int64_t high,
		int64_t mergeMemoryLimit, /* In bytes */
		char *tmpDir)
{
	/*
	   char *FnName = "RGIndexMergeHelper";
	   */

	/* Merge the two lists */
	/* Since we want to keep space requirement small, use an upper bound on memory,
	 * so that we use tmp files when memory requirements become to large */
	if(index->contigType == Contig_8) {
		if((high-low+1)*(sizeof(uint32_t) + sizeof(uint8_t)) <= mergeMemoryLimit) {
			/* Use memory */
			RGIndexMergeHelperInMemoryContig_8(index, rg, low, mid, high);
		}
		else {
			/* Use tmp files */
			RGIndexMergeHelperFromDiskContig_8(index, rg, low, mid, high, tmpDir);
		}
	}
	else {
		if((high-low+1)*(sizeof(uint32_t) + sizeof(uint32_t)) <= mergeMemoryLimit) {
			RGIndexMergeHelperInMemoryContig_32(index, rg, low, mid, high);
		}
		else {
			/* Use tmp files */
			RGIndexMergeHelperFromDiskContig_32(index, rg, low, mid, high, tmpDir);
		}
	}
	/* Test merge */
	/*
	   for(i=low+1;i<=high;i++) {
	   assert(RGIndexCompareAt(index, rg, i-1, i, 0) <= 0);
	   }
	   */
}

/* TODO */
void RGIndexMergeHelperInMemoryContig_8(RGIndex *index,
		RGBinary *rg,
		int64_t low,
		int64_t mid,
		int64_t high)
{
	char *FnName = "RGIndexMergeHelperInMemoryContig_8";
	int64_t i=0;
	uint32_t *tmpPositions=NULL;
	uint8_t *tmpContigs_8=NULL;
	int64_t startUpper, startLower, endUpper, endLower;
	int64_t ctr=0;

	assert(index->contigType == Contig_8);
	assert(index->contigs_8 != NULL);

	/* Merge the two lists using memory */

	/* Use memory */
	tmpPositions = malloc(sizeof(uint32_t)*(high-low+1));
	if(NULL == tmpPositions) {
		PrintError(FnName,
				"tmpPositions",
				"Could not allocate memory",
				Exit,
				MallocMemory);
	}
	tmpContigs_8 = malloc(sizeof(uint8_t)*(high-low+1));
	if(NULL == tmpContigs_8) {
		PrintError(FnName,
				"tmpContigs_8",
				"Could not allocate memory",
				Exit,
				MallocMemory);
	}

	/* Merge */
	startLower = low;
	endLower = mid;
	startUpper = mid+1;
	endUpper = high;
	ctr=0;
	while( (startLower <= endLower) && (startUpper <= endUpper) ) {
		if(RGIndexCompareAt(index, rg, startLower, startUpper, 0) <= 0) {
			tmpPositions[ctr] = index->positions[startLower];
			tmpContigs_8[ctr] = index->contigs_8[startLower];
			startLower++;
		}
		else {
			tmpPositions[ctr] = index->positions[startUpper];
			tmpContigs_8[ctr] = index->contigs_8[startUpper];
			startUpper++;
		}
		ctr++;
	}
	while(startLower <= endLower) {
		tmpPositions[ctr] = index->positions[startLower];
		tmpContigs_8[ctr] = index->contigs_8[startLower];
		startLower++;
		ctr++;
	}
	while(startUpper <= endUpper) {
		tmpPositions[ctr] = index->positions[startUpper];
		tmpContigs_8[ctr] = index->contigs_8[startUpper];
		startUpper++;
		ctr++;
	}
	/* Copy back */
	for(i=low, ctr=0;
			i<=high;
			i++, ctr++) {
		index->positions[i] = tmpPositions[ctr];
		index->contigs_8[i] = tmpContigs_8[ctr];
	}

	/* Free memory */
	free(tmpPositions);
	tmpPositions=NULL;
	free(tmpContigs_8);
	tmpContigs_8=NULL;
}

/* TODO */
void RGIndexMergeHelperInMemoryContig_32(RGIndex *index,
		RGBinary *rg,
		int64_t low,
		int64_t mid,
		int64_t high)
{
	char *FnName = "RGIndexMergeHelperInMemoryContig_32";
	int64_t i=0;
	uint32_t *tmpPositions=NULL;
	uint8_t *tmpContigs_32=NULL;
	int64_t startUpper, startLower, endUpper, endLower;
	int64_t ctr=0;

	assert(index->contigType == Contig_32);
	assert(index->contigs_32 != NULL);

	/* Merge the two lists using memory */

	/* Use memory */
	tmpPositions = malloc(sizeof(uint32_t)*(high-low+1));
	if(NULL == tmpPositions) {
		PrintError(FnName,
				"tmpPositions",
				"Could not allocate memory",
				Exit,
				MallocMemory);
	}
	tmpContigs_32 = malloc(sizeof(uint32_t)*(high-low+1));
	if(NULL == tmpContigs_32) {
		PrintError(FnName,
				"tmpContigs_32",
				"Could not allocate memory",
				Exit,
				MallocMemory);
	}

	/* Merge */
	startLower = low;
	endLower = mid;
	startUpper = mid+1;
	endUpper = high;
	ctr=0;
	while( (startLower <= endLower) && (startUpper <= endUpper) ) {
		if(RGIndexCompareAt(index, rg, startLower, startUpper, 0) <= 0) {
			tmpPositions[ctr] = index->positions[startLower];
			tmpContigs_32[ctr] = index->contigs_32[startLower];
			startLower++;
		}
		else {
			tmpPositions[ctr] = index->positions[startUpper];
			tmpContigs_32[ctr] = index->contigs_32[startUpper];
			startUpper++;
		}
		ctr++;
	}
	while(startLower <= endLower) {
		tmpPositions[ctr] = index->positions[startLower];
		tmpContigs_32[ctr] = index->contigs_32[startLower];
		startLower++;
		ctr++;
	}
	while(startUpper <= endUpper) {
		tmpPositions[ctr] = index->positions[startUpper];
		tmpContigs_32[ctr] = index->contigs_32[startUpper];
		startUpper++;
		ctr++;
	}
	/* Copy back */
	for(i=low, ctr=0;
			i<=high;
			i++, ctr++) {
		index->positions[i] = tmpPositions[ctr];
		index->contigs_32[i] = tmpContigs_32[ctr];
	}

	/* Free memory */
	free(tmpPositions);
	tmpPositions=NULL;
	free(tmpContigs_32);
	tmpContigs_32=NULL;
}

/* TODO */
void RGIndexMergeHelperFromDiskContig_8(RGIndex *index,
		RGBinary *rg,
		int64_t low,
		int64_t mid,
		int64_t high,
		char *tmpDir)
{
	char *FnName = "RGIndexMergeHelperFromDiskContig_8";
	int64_t i=0;
	int64_t ctr=0;
	FILE *tmpLowerFP=NULL;
	FILE *tmpUpperFP=NULL;
	char *tmpLowerFileName=NULL;
	char *tmpUpperFileName=NULL;
	uint32_t tmpLowerPosition=0;
	uint32_t tmpUpperPosition=0;
	uint8_t tmpLowerContig_8=0;
	uint8_t tmpUpperContig_8=0;

	assert(index->contigType == Contig_8);
	assert(index->contigs_8 != NULL);

	/* Merge the two lists */
	/* Since we want to keep space requirement small, use an upper bound on memory,
	 * so that we use tmp files when memory requirements become to large */
	/* Use tmp files */

	/* Open tmp files */
	tmpLowerFP = OpenTmpFile(tmpDir, &tmpLowerFileName);
	tmpUpperFP = OpenTmpFile(tmpDir, &tmpUpperFileName);

	/* Print to tmp files */
	for(i=low;i<=mid;i++) {
		if(1 != fwrite(&index->positions[i], sizeof(uint32_t), 1, tmpLowerFP)) {
			PrintError(FnName,
					"index->positions",
					"Could not write positions to tmp lower file",
					Exit,
					WriteFileError);
		}
		if(1 != fwrite(&index->contigs_8[i], sizeof(uint8_t), 1, tmpLowerFP)) {
			PrintError(FnName,
					"index->contigs_8",
					"Could not write contigs_8 to tmp lower file",
					Exit,
					WriteFileError);
		}
	}
	for(i=mid+1;i<=high;i++) {
		if(1 != fwrite(&index->positions[i], sizeof(uint32_t), 1, tmpUpperFP)) {
			PrintError(FnName,
					"index->positions",
					"Could not write positions to tmp upper file",
					Exit,
					WriteFileError);
		}
		if(1 != fwrite(&index->contigs_8[i], sizeof(uint8_t), 1, tmpUpperFP)) {
			PrintError(FnName,
					"index->contigs_8",
					"Could not write contigs_8 to tmp upper file",
					Exit,
					WriteFileError);
		}
	}

	/* Move to beginning of the files */
	fseek(tmpLowerFP, 0 , SEEK_SET);
	fseek(tmpUpperFP, 0 , SEEK_SET);

	/* Merge tmp files back into index */
	/* Get first contig/pos */

	if(1!=fread(&tmpLowerPosition, sizeof(uint32_t), 1, tmpLowerFP) ||
			1!=fread(&tmpLowerContig_8, sizeof(uint8_t), 1, tmpLowerFP)) {
		PrintError(FnName,
				NULL,
				"Could not read in tmp lower",
				Exit,
				ReadFileError);
	}
	if(1!=fread(&tmpUpperPosition, sizeof(uint32_t), 1, tmpUpperFP) ||
			1!=fread(&tmpUpperContig_8, sizeof(uint8_t), 1, tmpUpperFP)) {
		PrintError(FnName,
				NULL,
				"Could not read in tmp upper",
				Exit,
				ReadFileError);
	}

	for(i=low, ctr=0;
			i<=high &&
			tmpLowerPosition != 0 &&
			tmpUpperPosition != 0;
			i++, ctr++) {
		if(RGIndexCompareContigPos(index,
					rg,
					tmpLowerContig_8,
					tmpLowerPosition,
					tmpUpperContig_8,
					tmpUpperPosition,
					0)<=0) {
			/* Copy lower */
			index->positions[i] = tmpLowerPosition;
			index->contigs_8[i] = tmpLowerContig_8;
			/* Get new tmpLower */
			if(1!=fread(&tmpLowerPosition, sizeof(uint32_t), 1, tmpLowerFP) ||
					1!=fread(&tmpLowerContig_8, sizeof(uint8_t), 1, tmpLowerFP)) {
				tmpLowerPosition = 0;
				tmpLowerContig_8 = 0;
			}
		}
		else {
			/* Copy upper */
			index->positions[i] = tmpUpperPosition;
			index->contigs_8[i] = tmpUpperContig_8;
			/* Get new tmpUpper */
			if(1!=fread(&tmpUpperPosition, sizeof(uint32_t), 1, tmpUpperFP) ||
					1!=fread(&tmpUpperContig_8, sizeof(uint8_t), 1, tmpUpperFP)) {
				tmpUpperPosition = 0;
				tmpUpperContig_8 = 0;
			}
		}
	}
	while(tmpLowerPosition != 0 && tmpUpperPosition == 0) {
		/* Copy lower */
		index->positions[i] = tmpLowerPosition;
		index->contigs_8[i] = tmpLowerContig_8;
		/* Get new tmpLower */
		if(1!=fread(&tmpLowerPosition, sizeof(uint32_t), 1, tmpLowerFP) ||
				1!=fread(&tmpLowerContig_8, sizeof(uint8_t), 1, tmpLowerFP)) {
			tmpLowerPosition = 0;
			tmpLowerContig_8 = 0;
		}
		i++;
		ctr++;
	}
	while(tmpLowerPosition == 0 && tmpUpperPosition != 0) {
		/* Copy upper */
		index->positions[i] = tmpUpperPosition;
		index->contigs_8[i] = tmpUpperContig_8;
		/* Get new tmpUpper */
		if(1!=fread(&tmpUpperPosition, sizeof(uint32_t), 1, tmpUpperFP) ||
				1!=fread(&tmpUpperContig_8, sizeof(uint8_t), 1, tmpUpperFP)) {
			tmpUpperPosition = 0;
			tmpUpperContig_8 = 0;
		}
		i++;
		ctr++;
	}
	assert(ctr == (high - low + 1));
	assert(i == high + 1);

	/* Close tmp files */
	CloseTmpFile(&tmpLowerFP, &tmpLowerFileName);
	CloseTmpFile(&tmpUpperFP, &tmpUpperFileName);
	/* Test merge */
	/*
	   for(i=low+1;i<=high;i++) {
	   assert(RGIndexCompareAt(index, rg, i-1, i, 0) <= 0);
	   }
	   */
}

/* TODO */
void RGIndexMergeHelperFromDiskContig_32(RGIndex *index,
		RGBinary *rg,
		int64_t low,
		int64_t mid,
		int64_t high,
		char *tmpDir)
{
	char *FnName = "RGIndexMergeHelperFromDiskContig_32";
	int64_t i=0;
	int64_t ctr=0;
	FILE *tmpLowerFP=NULL;
	FILE *tmpUpperFP=NULL;
	char *tmpLowerFileName=NULL;
	char *tmpUpperFileName=NULL;
	uint32_t tmpLowerPosition=0;
	uint32_t tmpUpperPosition=0;
	uint32_t tmpLowerContig_32=0;
	uint32_t tmpUpperContig_32=0;

	assert(index->contigType == Contig_32);
	assert(index->contigs_32 != NULL);

	/* Merge the two lists */
	/* Since we want to keep space requirement small, use an upper bound on memory,
	 * so that we use tmp files when memory requirements become to large */
	/* Use tmp files */

	/* Open tmp files */
	tmpLowerFP = OpenTmpFile(tmpDir, &tmpLowerFileName);
	tmpUpperFP = OpenTmpFile(tmpDir, &tmpUpperFileName);

	/* Print to tmp files */
	for(i=low;i<=mid;i++) {
		if(1 != fwrite(&index->positions[i], sizeof(uint32_t), 1, tmpLowerFP)) {
			PrintError(FnName,
					"index->positions",
					"Could not write positions to tmp lower file",
					Exit,
					WriteFileError);
		}
		if(1 != fwrite(&index->contigs_32[i], sizeof(uint32_t), 1, tmpLowerFP)) {
			PrintError(FnName,
					"index->contigs_32",
					"Could not write contigs_32 to tmp lower file",
					Exit,
					WriteFileError);
		}
	}
	for(i=mid+1;i<=high;i++) {
		if(1 != fwrite(&index->positions[i], sizeof(uint32_t), 1, tmpUpperFP)) {
			PrintError(FnName,
					"index->positions",
					"Could not write positions to tmp upper file",
					Exit,
					WriteFileError);
		}
		if(1 != fwrite(&index->contigs_32[i], sizeof(uint32_t), 1, tmpUpperFP)) {
			PrintError(FnName,
					"index->contigs_32",
					"Could not write contigs_32 to tmp upper file",
					Exit,
					WriteFileError);
		}
	}

	/* Move to beginning of the files */
	fseek(tmpLowerFP, 0 , SEEK_SET);
	fseek(tmpUpperFP, 0 , SEEK_SET);

	/* Merge tmp files back into index */
	/* Get first contig/pos */

	if(1!=fread(&tmpLowerPosition, sizeof(uint32_t), 1, tmpLowerFP) ||
			1!=fread(&tmpLowerContig_32, sizeof(uint32_t), 1, tmpLowerFP)) {
		PrintError(FnName,
				NULL,
				"Could not read in tmp lower",
				Exit,
				ReadFileError);
	}
	if(1!=fread(&tmpUpperPosition, sizeof(uint32_t), 1, tmpUpperFP) ||
			1!=fread(&tmpUpperContig_32, sizeof(uint32_t), 1, tmpUpperFP)) {
		PrintError(FnName,
				NULL,
				"Could not read in tmp upper",
				Exit,
				ReadFileError);
	}

	for(i=low, ctr=0;
			i<=high &&
			tmpLowerPosition != 0 &&
			tmpUpperPosition != 0;
			i++, ctr++) {
		if(RGIndexCompareContigPos(index,
					rg,
					tmpLowerContig_32,
					tmpLowerPosition,
					tmpUpperContig_32,
					tmpUpperPosition,
					0)<=0) {
			/* Copy lower */
			index->positions[i] = tmpLowerPosition;
			index->contigs_32[i] = tmpLowerContig_32;
			/* Get new tmpLower */
			if(1!=fread(&tmpLowerPosition, sizeof(uint32_t), 1, tmpLowerFP) ||
					1!=fread(&tmpLowerContig_32, sizeof(uint32_t), 1, tmpLowerFP)) {
				tmpLowerPosition = 0;
				tmpLowerContig_32 = 0;
			}
		}
		else {
			/* Copy upper */
			index->positions[i] = tmpUpperPosition;
			index->contigs_32[i] = tmpUpperContig_32;
			/* Get new tmpUpper */
			if(1!=fread(&tmpUpperPosition, sizeof(uint32_t), 1, tmpUpperFP) ||
					1!=fread(&tmpUpperContig_32, sizeof(uint32_t), 1, tmpUpperFP)) {
				tmpUpperPosition = 0;
				tmpUpperContig_32 = 0;
			}
		}
	}
	while(tmpLowerPosition != 0 && tmpUpperPosition == 0) {
		/* Copy lower */
		index->positions[i] = tmpLowerPosition;
		index->contigs_32[i] = tmpLowerContig_32;
		/* Get new tmpLower */
		if(1!=fread(&tmpLowerPosition, sizeof(uint32_t), 1, tmpLowerFP) ||
				1!=fread(&tmpLowerContig_32, sizeof(uint32_t), 1, tmpLowerFP)) {
			tmpLowerPosition = 0;
			tmpLowerContig_32 = 0;
		}
		i++;
		ctr++;
	}
	while(tmpLowerPosition == 0 && tmpUpperPosition != 0) {
		/* Copy upper */
		index->positions[i] = tmpUpperPosition;
		index->contigs_32[i] = tmpUpperContig_32;
		/* Get new tmpUpper */
		if(1!=fread(&tmpUpperPosition, sizeof(uint32_t), 1, tmpUpperFP) ||
				1!=fread(&tmpUpperContig_32, sizeof(uint32_t), 1, tmpUpperFP)) {
			tmpUpperPosition = 0;
			tmpUpperContig_32 = 0;
		}
		i++;
		ctr++;
	}
	assert(ctr == (high - low + 1));
	assert(i == high + 1);

	/* Close tmp files */
	CloseTmpFile(&tmpLowerFP, &tmpLowerFileName);
	CloseTmpFile(&tmpUpperFP, &tmpUpperFileName);
	/* Test merge */
	/*
	   for(i=low+1;i<=high;i++) {
	   assert(RGIndexCompareAt(index, rg, i-1, i, 0) <= 0);
	   }
	   */
}

/* TODO */
void RGIndexDelete(RGIndex *index)
{
	/* Free memory and initialize */
	free(index->contigs_8);
	free(index->contigs_32);
	free(index->positions);
	free(index->mask);
	free(index->starts);
	free(index->ends);

	RGIndexInitialize(index);
}

/* TODO */
double RGIndexGetSize(RGIndex *index, int32_t outputSize) 
{
	double total=0.0;

	/* memory used by positions */
	total += (index->contigType==Contig_8)?(sizeof(uint8_t)*index->length):(sizeof(uint32_t)*index->length);
	/* memory used by positions */
	total += sizeof(uint32_t)*index->length;
	/* memory used by the mask */
	total += sizeof(int32_t)*index->width;
	/* memory used by starts */
	total += sizeof(uint32_t)*index->hashLength;
	/* memory used by ends */
	total += sizeof(uint32_t)*index->hashLength;
	/* memory used by the index base structure */
	total += sizeof(RGIndex); 

	switch(outputSize) {
		case KILOBYTES:
			return (total/pow(2, 10));
			break;
		case MEGABYTES:
			return (total/pow(2, 20));
			break;
		case GIGABYTES:
			return (total/pow(2, 30));
			break;
		default:
			return total;
			break;
	}
}

/* TODO */
void RGIndexPrint(FILE *fp, RGIndex *index, int32_t binaryOutput)
{
	char *FnName="RGIndexPrint";
	int64_t i;

	/* Print header */
	RGIndexPrintHeader(fp, index, binaryOutput);

	if(binaryOutput == TextOutput) {

		/* Print the positions and contigs */
		if(index->contigType == Contig_8) {
			for(i=0;i<index->length;i++) {
				fprintf(fp, "%u\t%u\n", 
						index->positions[i],
						(uint32_t)index->contigs_8[i]);
			}
		}
		else {
			for(i=0;i<index->length;i++) {
				fprintf(fp, "%u\t%u\n", 
						index->positions[i],
						index->contigs_32[i]);
			}
		}

		/* Print the starts and ends */
		for(i=0;i<index->hashLength;i++) {
			fprintf(fp, "%u\t%u\n",
					index->starts[i],
					index->ends[i]);
		}
	}
	else {
		/* Print positions */
		if(index->contigType == Contig_8) {
			if(fwrite(index->positions, sizeof(uint32_t), index->length, fp) != index->length || 
					/* Print chomosomes */
					fwrite(index->contigs_8, sizeof(uint8_t), index->length, fp) != index->length ||
					/* Print the starts */
					fwrite(index->starts, sizeof(uint32_t), index->hashLength, fp) != index->hashLength ||
					/* Print the ends */
					fwrite(index->ends, sizeof(uint32_t), index->hashLength, fp) != index->hashLength) {
				PrintError(FnName,
						NULL,
						"Could not write index and hash",
						Exit,
						WriteFileError);
			}
		}
		else {
			if(fwrite(index->positions, sizeof(uint32_t), index->length, fp) != index->length || 
					/* Print chomosomes */
					fwrite(index->contigs_32, sizeof(uint32_t), index->length, fp) != index->length ||
					/* Print the starts */
					fwrite(index->starts, sizeof(uint32_t), index->hashLength, fp) != index->hashLength ||
					/* Print the ends */
					fwrite(index->ends, sizeof(uint32_t), index->hashLength, fp) != index->hashLength) {
				PrintError(FnName,
						NULL,
						"Could not write index and hash",
						Exit,
						WriteFileError);
			}
		}
	}
}

/* TODO */
void RGIndexRead(FILE *fp, RGIndex *index, int32_t binaryInput)
{
	char *FnName="RGIndexRead";
	int64_t i;
	uint32_t tempInt;

	/* Read in the header */
	RGIndexReadHeader(fp, index, binaryInput);

	assert(index->length > 0);

	/* Allocate memory for the positions */
	index->positions = malloc(sizeof(uint32_t)*index->length);
	if(NULL == index->positions) {
		PrintError(FnName,
				"index->positions",
				"Could not allocate memory",
				Exit,
				MallocMemory);
	}
	/* Allocate memory for the contigs */
	if(index->contigType == Contig_8) {
		index->contigs_8 = malloc(sizeof(uint8_t)*index->length);
		if(NULL == index->contigs_8) {
			PrintError(FnName,
					"index->contigs",
					"Could not allocate memory",
					Exit,
					MallocMemory);
		}
	}
	else {
		index->contigs_32 = malloc(sizeof(uint32_t)*index->length);
		if(NULL == index->contigs_32) {
			PrintError(FnName,
					"index->contigs",
					"Could not allocate memory",
					Exit,
					MallocMemory);
		}
	}
	/* Allocate memory for the starts */
	index->starts = malloc(sizeof(uint32_t)*index->hashLength);
	if(NULL == index->starts) {
		PrintError(FnName,
				"index->starts",
				"Could not allocate memory",
				Exit,
				MallocMemory);
	}
	/* Allocate memory for the ends */
	index->ends = malloc(sizeof(uint32_t)*index->hashLength);
	if(NULL == index->ends) {
		PrintError(FnName,
				"index->ends",
				"Could not allocate memory",
				Exit,
				MallocMemory);
	}

	if(binaryInput == TextInput) {
		/* Read the positions and contigs */
		for(i=0;i<index->length;i++) {
			if(fscanf(fp, "%u\t%u\n",
						&index->positions[i],
						&tempInt)==EOF) {
				PrintError(FnName,
						NULL,
						"Could not read in contig and position",
						Exit,
						EndOfFile);
			}
			if(index->contigType == Contig_8) {
				index->contigs_8[i] = (uint8_t)tempInt;
			}
			else {
				index->contigs_32[i] = tempInt;
			}
		}

		/* Read the positions and contigs */
		for(i=0;i<index->hashLength;i++) {
			if(fscanf(fp, "%u\t%u\n",
						&index->starts[i],
						&index->ends[i])==EOF) {
				PrintError(FnName,
						NULL,
						"Could not read in starts and ends",
						Exit,
						EndOfFile);
			}
		}
	}
	else {
		/* Read in positions */
		if(fread(index->positions, sizeof(uint32_t), index->length, fp)!=index->length) {
			PrintError(FnName,
					NULL,
					"Could not read in positions",
					Exit,
					ReadFileError);
		}

		/* Read in the contigs */
		if(index->contigType == Contig_8) {
			if(fread(index->contigs_8, sizeof(uint8_t), index->length, fp)!=index->length) {
				PrintError(FnName,
						NULL,
						"Could not read in contigs_8",
						Exit,
						ReadFileError);
			}
		}
		else {
			if(fread(index->contigs_32, sizeof(uint32_t), index->length, fp)!=index->length) {
				PrintError(FnName,
						NULL,
						"Could not read in contigs_32",
						Exit,
						ReadFileError);
			}
		}

		/* Read in starts */
		if(fread(index->starts, sizeof(uint32_t), index->hashLength, fp)!=index->hashLength) {
			PrintError(FnName,
					NULL,
					"Could not read in starts",
					Exit,
					ReadFileError);
		}

		/* Read in ends */
		if(fread(index->ends, sizeof(uint32_t), index->hashLength, fp)!=index->hashLength) {
			PrintError(FnName,
					NULL,
					"Could not read in ends",
					Exit,
					ReadFileError);
		}
	}
}

/* TODO */
/* Debugging function */
void RGIndexPrintInfo(FILE *fp, int32_t binaryInput)
{
	/*
	   char *FnName = "RGIndexPrintInfo";
	   */
	int64_t i;
	RGIndex index;

	/* Read in the header */
	RGIndexReadHeader(fp, &index, binaryInput);

	/* Print the info */
	fprintf(stderr, "start contig:\t%d\n",
			index.startContig);
	fprintf(stderr, "start position:\t\t%d\n",
			index.startPos);
	fprintf(stderr, "end contig:\t\t%d\n",
			index.endContig);
	fprintf(stderr, "end position:\t\t%d\n",
			index.endPos);
	fprintf(stderr, "index length:\t\t%lld\n",
			(long long int)index.length);
	fprintf(stderr, "repeat masker:\t\t%d\n",
			index.repeatMasker);
	fprintf(stderr, "space:\t\t\t%d\n",
			index.space);
	fprintf(stderr, "hash width:\t\t%u\n",
			index.hashWidth);
	fprintf(stderr, "hash length:\t\t%lld\n",
			(long long int)index.hashLength);
	fprintf(stderr, "width:\t\t\t%d\n",
			index.width);
	fprintf(stderr, "keysize:\t\t\t%d\n",
			index.keysize);
	fprintf(stderr, "mask:\t\t\t");
	for(i=0;i<index.width;i++) {
		fprintf(stderr, "%1d", index.mask[i]);
	}
	fprintf(stderr, "\n");

	/* Free masks and initialize */
	free(index.mask);
	RGIndexInitialize(&index);
}

/* TODO */
void RGIndexPrintHeader(FILE *fp, RGIndex *index, int32_t binaryOutput)
{
	char *FnName="RGIndexPrintHeader";
	int i;
	if(binaryOutput == 0) {
		fprintf(fp, "%d\t%lld\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%u\t%lld\t",
				index->id,
				(long long int)index->length,
				index->contigType,
				index->startContig,
				index->startPos,
				index->endContig,
				index->endPos,
				index->width,
				index->keysize,
				index->repeatMasker,
				index->space,
				index->hashWidth,
				(long long int)index->hashLength
			   );
		/* Print the mask */
		for(i=0;i<index->width;i++) {
			fprintf(stderr, "%1d", index->mask[i]);
		}
		fprintf(stderr, "\n");
	}
	else {
		/* Print Header */
		if(fwrite(&index->id, sizeof(int32_t), 1, fp) != 1 ||
				fwrite(&index->length, sizeof(int64_t), 1, fp) != 1 || 
				fwrite(&index->contigType, sizeof(int32_t), 1, fp) != 1 ||
				fwrite(&index->startContig, sizeof(int32_t), 1, fp) != 1 ||
				fwrite(&index->startPos, sizeof(int32_t), 1, fp) != 1 ||
				fwrite(&index->endContig, sizeof(int32_t), 1, fp) != 1 ||
				fwrite(&index->endPos, sizeof(int32_t), 1, fp) != 1 ||
				fwrite(&index->width, sizeof(int32_t), 1, fp) != 1 ||
				fwrite(&index->keysize, sizeof(int32_t), 1, fp) != 1 ||
				fwrite(&index->repeatMasker, sizeof(int32_t), 1, fp) != 1 ||
				fwrite(&index->space, sizeof(int32_t), 1, fp) != 1 ||
				fwrite(&index->hashWidth, sizeof(uint32_t), 1, fp) != 1 ||
				fwrite(&index->hashLength, sizeof(int64_t), 1, fp) != 1 ||
				fwrite(index->mask, sizeof(int32_t), index->width, fp) != index->width) {
			PrintError(FnName,
					NULL,
					"Could not write header",
					Exit,
					WriteFileError);
		}
	}
}

/* TODO */
void RGIndexReadHeader(FILE *fp, RGIndex *index, int32_t binaryInput)
{
	char *FnName = "RGIndexReadHeader";
	int i;
	char tempChar;
	long long int tempLongLongInt[2];
	/* Read in header */
	if(binaryInput == 0) {
		if(fscanf(fp, "%d %lld %d %d %d %d %d %d %d %d %d %u %lld",
					&index->id,
					&tempLongLongInt[0],
					&index->contigType,
					&index->startContig,
					&index->startPos,
					&index->endContig,
					&index->endPos,
					&index->width,
					&index->keysize,
					&index->repeatMasker,
					&index->space,
					&index->hashWidth,
					&tempLongLongInt[1])==EOF) {
			PrintError(FnName,
					NULL,
					"Could not read header",
					Exit,
					EndOfFile);
		}
		index->length = tempLongLongInt[0];
		index->hashLength = tempLongLongInt[1];
		/* Allocate memory for the mask */
		index->mask = malloc(sizeof(int32_t)*index->width);
		if(NULL==index->mask) {
			PrintError(FnName,
					"index->mask",
					"Could not allocate memory",
					Exit,
					MallocMemory);
		}
		/* Read the mask */
		for(i=0;i<index->width;i++) {
			if(fscanf(fp, "%c", &tempChar)==EOF) {
				PrintError(FnName,
						NULL,
						"Could not read header",
						Exit,
						EndOfFile);
			}
			switch(tempChar) {
				case '0':
					index->mask[i] = 0;
					break;
				case '1':
					index->mask[i] = 1;
					break;
				default:
					PrintError(FnName,
							NULL,
							"Could not read mask",
							Exit,
							OutOfRange);
			}
		}
	}
	else {
		if(fread(&index->id, sizeof(int32_t), 1, fp) != 1 ||
				fread(&index->length, sizeof(int64_t), 1, fp) != 1 || 
				fread(&index->contigType, sizeof(int32_t), 1, fp) != 1 ||
				fread(&index->startContig, sizeof(int32_t), 1, fp) != 1 ||
				fread(&index->startPos, sizeof(int32_t), 1, fp) != 1 ||
				fread(&index->endContig, sizeof(int32_t), 1, fp) != 1 ||
				fread(&index->endPos, sizeof(int32_t), 1, fp) != 1 ||
				fread(&index->width, sizeof(int32_t), 1, fp) != 1 ||
				fread(&index->keysize, sizeof(int32_t), 1, fp) != 1 ||
				fread(&index->repeatMasker, sizeof(int32_t), 1, fp) != 1 ||
				fread(&index->space, sizeof(int32_t), 1, fp) != 1 ||
				fread(&index->hashWidth, sizeof(uint32_t), 1, fp) != 1 ||
				fread(&index->hashLength, sizeof(int64_t), 1, fp) != 1) {
			PrintError(FnName,
					NULL,
					"Could not read header",
					Exit,
					ReadFileError);
		}
		/* Allocate memory for the mask */
		index->mask = malloc(sizeof(int32_t)*index->width);
		if(NULL==index->mask) {
			PrintError(FnName,
					"index->mask",
					"Could not allocate memory",
					Exit,
					MallocMemory);
		}
		/* Read the mask */
		if(fread(index->mask, sizeof(int32_t), index->width, fp) != index->width) {
			PrintError(FnName,
					NULL,
					"Could not read header",
					Exit,
					ReadFileError);
		}
	}

	/* Error checking */
	assert(index->id == (int)BFAST_ID);
	assert(index->length > 0);
	assert(index->contigType == Contig_8 || index->contigType == Contig_32);
	assert(index->startContig > 0);
	assert(index->startPos > 0);
	assert(index->endContig > 0);
	assert(index->endPos > 0);
	assert(index->width > 0);
	assert(index->keysize > 0);
	assert(index->repeatMasker == 0 || index->repeatMasker == 1);
	assert(index->space == NTSpace || index->space == ColorSpace);
	assert(index->hashWidth > 0);
	assert(index->hashLength > 0);
}

/* TODO */
/* We will append the matches if matches have already been found */
void RGIndexGetRanges(RGIndex *index, RGBinary *rg, char *read, int32_t readLength, int8_t direction, int32_t offset, RGRanges *r)
{
	int64_t startIndex=-1;
	int64_t endIndex=-1;
	int64_t foundIndex=0;
	uint32_t hashIndex=0;

	/* Get the hash index */
	/* The hope is that the hash will give better smaller bounds (if not
	 * zero bounds for the binary search on the index */
	hashIndex = RGIndexGetHashIndexFromRead(index, rg, read, readLength, 0);
	assert(hashIndex >= 0 && hashIndex < index->hashLength);

	if(index->starts[hashIndex] == UINT_MAX || 
			index->ends[hashIndex] == UINT_MAX) {
		/* Skip */
	}
	else {
		assert(index->starts[hashIndex] >=0 && index->starts[hashIndex] < index->length);
		assert(index->ends[hashIndex] >=0 && index->ends[hashIndex] < index->length);

		/* Search the index using the bounds from the hash */
		foundIndex=RGIndexGetIndex(index, 
				rg, 
				index->starts[hashIndex],  
				index->ends[hashIndex],
				read,
				&startIndex,
				&endIndex);

		if(foundIndex>0) {
			/* (Re)Allocate memory for the new range */
			RGRangesReallocate(r, r->numEntries+1);
			assert(endIndex >= startIndex);
			assert(startIndex >= 0 && startIndex < index->length);
			assert(endIndex >= 0 && endIndex < index->length);
			/* Copy over to the range list */
			r->startIndex[r->numEntries-1] = startIndex;
			r->endIndex[r->numEntries-1] = endIndex;
			r->strand[r->numEntries-1] = direction;
			r->offset[r->numEntries-1] = offset;
		}
	}
}

/* TODO */
int64_t RGIndexGetIndex(RGIndex *index,
		RGBinary *rg,
		int64_t low,
		int64_t high,
		char *read,
		int64_t *startIndex,
		int64_t *endIndex)
{
	int64_t mid=-1;
	int32_t cmp;
	int32_t cont = 1;
	int64_t tmpLow, tmpMid, tmpHigh;

	assert(low==0 || RGIndexCompareRead(index, rg, read, low-1, 0) > 0);
	assert(high==index->length-1 || RGIndexCompareRead(index, rg, read, high+1, 0) < 0); 

	while(low <= high && cont==1) {
		mid = (low+high)/2;
		cmp = RGIndexCompareRead(index, rg, read, mid, 0);
		if(VERBOSE >= DEBUG) {
			fprintf(stderr, "low:%lld\tmid:%lld\thigh:%lld\tcmp:%d\n",
					(long long int)low,
					(long long int)mid,
					(long long int)high,
					cmp);
		}
		if(cmp == 0) {
			cont = 0;
		}
		else if(cmp < 0) {
			high = mid-1;
		}
		else {
			low = mid + 1;
		}
	}
	/* If we found an entry that matches, get the bounds (start and end indexes */
	if(cont == 0) {
		assert(low==0 || RGIndexCompareRead(index, rg, read, low-1, 0) > 0);
		assert(high==index->length-1 || RGIndexCompareRead(index, rg, read, high+1, 0) < 0); 
		assert(RGIndexCompareRead(index, rg, read, mid, 0) == 0);
		tmpLow = low;
		tmpMid = mid;
		tmpHigh = high;
		/*
		   fprintf(stderr, "Getting start and end:\t%lld\t%lld\t%lld\n",
		   low,
		   mid,
		   high);
		   */
		/* Get lower start Index */
		low = tmpLow;
		high = tmpMid;
		while(low < high) {
			mid = (low+high)/2;
			cmp = RGIndexCompareRead(index, rg, read, mid, 0);
			assert(cmp >= 0);
			/*
			   fprintf(stderr, "start:%lld\t%lld\t%lld\t%d\n",
			   low,
			   mid,
			   high,
			   cmp);
			   */
			if(cmp == 0) {
				high = mid;
			}
			else {
				/* mid is less than */
				low = mid+1;
			}
		}
		(*startIndex) = low;
		assert(low == high);
		assert(RGIndexCompareRead(index, rg, read, (*startIndex), 0)==0);
		assert((*startIndex) == 0 || RGIndexCompareRead(index, rg, read, (*startIndex)-1, 0)>0);
		/* Get upper start Index */
		low = tmpMid;
		high = tmpHigh;
		while(low < high) {
			mid = (low+high)/2+1;
			cmp = RGIndexCompareRead(index, rg, read, mid, 0);
			assert(cmp <= 0);
			/*
			   fprintf(stderr, "end:%lld\t%lld\t%lld\t%d\n",
			   low,
			   mid,
			   high,
			   cmp);
			   */
			if(cmp == 0) {
				low = mid;
			}
			else {
				/* mid is less than */
				high = mid-1;
			}
		}
		assert(low == high);
		/* adjust endIndex */
		(*endIndex) = low;
		assert(RGIndexCompareRead(index, rg, read, (*endIndex), 0)==0);
		assert((*endIndex) == index->length-1 || RGIndexCompareRead(index, rg, read, (*endIndex)+1, 0)<0);
		return 1;
	}
	else {
		return 0;
	}

}

/* TODO */
void RGIndexSwapAt(RGIndex *index, int64_t a, int64_t b)
{
	uint32_t tempContig, tempPos;

	tempPos = index->positions[a];
	index->positions[a] = index->positions[b];
	index->positions[b] = tempPos;

	if(index->contigType == Contig_8) {
		tempContig = index->contigs_8[a];
		index->contigs_8[a] = index->contigs_8[b];
		index->contigs_8[b] = tempContig;
	}
	else {
		tempContig = index->contigs_32[a];
		index->contigs_32[a] = index->contigs_32[b];
		index->contigs_32[b] = tempContig;
	}
}

/* TODO */
int64_t RGIndexGetPivot(RGIndex *index, RGBinary *rg, int64_t low, int64_t high)
{
	int64_t pivot = (low+high)/2;
	int32_t cmp[3];
	cmp[0] = RGIndexCompareAt(index, rg, low, pivot, 0);
	cmp[1] = RGIndexCompareAt(index, rg, low, high, 0);
	cmp[2] = RGIndexCompareAt(index, rg, pivot, high, 0);

	if(cmp[0] <= 0) {
		/* low <= pivot */
		if(cmp[1] >= 0) {
			/* high <= low */
			/* so high <= low <= pivot */
			pivot = low;
		}
		else {
			/* low < high */
			if(cmp[2] <= 0) {
				/* pivot <= high */
				/* so low <= pivot <= high */
				/* choose pivot */
			}
			else {
				/* high < pivot */
				/* so low < high < pivot */
				pivot = high;
			}
		}
	}
	else {
		/* pivot < low */
		if(cmp[1] <= 0) {
			/* low <= high */
			/* so pivot < low <= high */
			pivot = low;
		}
		else {
			/* high < low */
			if(cmp[2] <= 0) {
				/* pivot <= high */
				/* so pivot <= high < low */
				pivot = high;
			}
			else {
				/* high < pivot */
				/* so high < pivot < low */
				/* choose pivot */
			}
		}
	}
	return pivot;
}

/* TODO */
int32_t RGIndexCompareContigPos(RGIndex *index,
		RGBinary *rg,
		uint32_t aContig,
		uint32_t aPos,
		uint32_t bContig,
		uint32_t bPos,
		int debug)
{
	char *FnName="RGIndexCompareContigPos";
	int64_t i;
	uint8_t aBase;
	uint8_t bBase;

	if(!(aContig >= index->startContig && aContig <= index->endContig)) {
	}
	assert(aContig >= index->startContig && aContig <= index->endContig);
	assert( (aContig != index->startContig || aPos >= index->startPos) &&
			(aContig != index->endContig || aPos <= index->endPos));
	assert(bContig >= index->startContig && bContig <= index->endContig);
	assert( (bContig != index->startContig || bPos >= index->startPos) &&
			(bContig != index->endContig || bPos <= index->endPos));

	/* Initialize for color space */

	if(debug == 1) {
		fprintf(stderr, "\n[%d,%d]\t[%d,%d]\n",
				(int)aContig,
				aPos,
				(int)bContig,
				bPos);
	}

	/* Go across the mask */
	for(i=0;i<index->width;i++) {
		switch(index->mask[i]) {
			case 0:
				/* Ignore base */
				break;
			case 1:
				/* Get bases */
				aBase = ToLower(RGBinaryGetBase(rg,
							aContig,
							aPos + i));
				bBase = ToLower( RGBinaryGetBase(rg,
							bContig,
							bPos + i));
				/* Compare */
				if(aBase < bBase) {
					return -1;
				}
				else if(aBase > bBase) {
					return 1;
				}
				/* Continue if the current bases are equal */
				break;
			default:
				PrintError(FnName,
						NULL,
						"Could not understand mask",
						Exit,
						OutOfRange);
		}
	}

	/* All bases were equal, return 0 */
	return 0;
}

/* TODO */
int32_t RGIndexCompareAt(RGIndex *index,
		RGBinary *rg,
		int64_t a,
		int64_t b, 
		int debug)
{
	assert(a>=0 && a<index->length);
	assert(b>=0 && b<index->length);

	if(index->contigType == Contig_8) {
		return RGIndexCompareContigPos(index,
				rg,
				index->contigs_8[a],
				index->positions[a],
				index->contigs_8[b],
				index->positions[b],
				debug);
	}
	else {
		return RGIndexCompareContigPos(index,
				rg,
				index->contigs_32[a],
				index->positions[a],
				index->contigs_32[b],
				index->positions[b],
				debug);
	}
}

/* TODO */
int32_t RGIndexCompareRead(RGIndex *index,
		RGBinary *rg,
		char *read,
		int64_t a,
		int debug)
{
	char *FnName="RGIndexCompareRead";
	assert(a>=0 && a<index->length);

	int32_t i;
	uint32_t aContig = (index->contigType==Contig_8)?index->contigs_8[a]:index->contigs_32[a];
	uint32_t aPos = index->positions[a];

	uint8_t aBase;
	uint8_t readBase;

	if(debug > 0) {
		fprintf(stderr, "%d\n%s", 
				index->width,
				BREAK_LINE);
		fprintf(stderr, "read[%d]:%s\n", 
				(int)strlen(read),
				read);
	}

	/* Go across the mask */
	for(i=0;i<index->width;i++) {
		switch(index->mask[i]) {
			case 0:
				/* Ignore base */
				break;
			case 1:
				/* Get bases */
				aBase = ToLower(RGBinaryGetBase(rg,
							aContig,
							aPos + i));
				readBase = ToLower(read[i]);
				/* Compare */
				if(readBase < aBase) {
					return -1;
				}
				else if(readBase > aBase) {
					return 1;
				}
				break;
			default:
				PrintError(FnName,
						NULL,
						"Could not understand mask",
						Exit,
						OutOfRange);
		}
	}

	/* All bases were equal, return 0 */
	return 0;
}

/* TODO */
uint32_t RGIndexGetHashIndex(RGIndex *index,
		RGBinary *rg,
		uint32_t a,
		int debug)
{
	assert(a>=0 && a<index->length);

	char *FnName = "RGIndexGetHashIndex";

	int32_t i;
	uint32_t aContig = (index->contigType==Contig_8)?index->contigs_8[a]:index->contigs_32[a];
	uint32_t aPos = index->positions[a];
	uint8_t aBase;
	int32_t cur = index->hashWidth-1;
	uint32_t hashIndex = 0;
	assert(ALPHABET_SIZE == 4);

	/* Go across the mask */
	for(i=0;cur >= 0 && i<index->width;i++) {
		switch(index->mask[i]) {
			case 0:
				/* Ignore base */
				break;
			case 1:
				aBase = ToLower(RGBinaryGetBase(rg,
							aContig,
							aPos + i));
				switch(aBase) {
					case 0:
					case 'a':
						/* Do nothing since a is zero base 4 */
						break;
					case 1:
					case 'c':
						hashIndex += pow(ALPHABET_SIZE, cur);
						break;
					case 2:
					case 'g':
						hashIndex += pow(ALPHABET_SIZE, cur)*2;
						break;
					case 3:
					case 't':
						hashIndex += pow(ALPHABET_SIZE, cur)*3;
						break;
					default:
						PrintError(FnName,
								"aBase",
								"Could not understand base",
								Exit,
								OutOfRange);
						break;
				}
				/* Update */
				cur--;
				break;
			default:
				PrintError(FnName,
						NULL,
						"Could not understand mask",
						Exit,
						OutOfRange);
		}
	}

	return hashIndex;
}

/* TODO */
uint32_t RGIndexGetHashIndexFromRead(RGIndex *index,
		RGBinary *rg,
		char *read,
		int32_t readLength,
		int debug)
{
	char *FnName = "RGIndexGetHashIndexFromRead";
	int32_t i;

	int32_t cur = index->hashWidth-1;
	uint32_t hashIndex = 0;
	uint8_t readBase;

	/* Go across the mask */
	for(i=0;cur >= 0 && i<index->width;i++) {
		switch(index->mask[i]) {
			case 0:
				/* Ignore base */
				break;
			case 1:
				readBase = ToLower(read[i]);
				/* Only works with a four letter alphabet */
				hashIndex = hashIndex << 2;
				switch(readBase) {
					case 0:
					case 'a':
						break;
					case 1:
					case 'c':
						hashIndex += 1;
						break;
					case 2:
					case 'g':
						hashIndex += 2;
						break;
					case 3:
					case 't':
						hashIndex += 3;
						break;
					default:
						PrintError(FnName,
								"readBase",
								"Could not understand base",
								Exit,
								OutOfRange);
						break;
				}
				cur--;
				break;
			default:
				PrintError(FnName,
						NULL,
						"Could not understand mask",
						Exit,
						OutOfRange);
		}
	}

	return hashIndex;
}

/* TODO */
/* Debug function */
void RGIndexPrintReadMasked(RGIndex *index, char *read, int offset, FILE *fp) 
{
	char *FnName="RGIndexPrintReadMasked";
	int i;
	for(i=0;i<index->width;i++) {
		switch(index->mask[i]) {
			case 0:
				/* Ignore base */
				break;
			case 1:
				fprintf(stderr, "%c", read[i]);
				break;
			default:
				PrintError(FnName,
						NULL,
						"Could not understand mask",
						Exit,
						OutOfRange);
		}
	}   
	fprintf(fp, "\n");
}

/* TODO */
void RGIndexInitialize(RGIndex *index)
{
	index->id = 0;

	index->contigs_8 = NULL;
	index->contigs_32 = NULL;
	index->positions = NULL;
	index->length = 0;
	index->contigType = 0;

	index->startContig = 0;
	index->startPos = 0;
	index->endContig = 0;
	index->endPos = 0;

	index->width = 0;
	index->keysize = 0;
	index->mask = NULL;

	index->repeatMasker = 0;
	index->space = 0;

	index->hashWidth = 0;
	index->hashLength = 0;
	index->starts = NULL;
	index->ends = NULL;
}
