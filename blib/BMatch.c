#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <sys/types.h>
#include <string.h>
#include "BLibDefinitions.h"
#include "BError.h"
#include "BMatch.h"

/* TODO */
int32_t BMatchRead(FILE *fp,
		BMatch *m,
		int32_t binaryInput)
{
	char *FnName = "BMatchRead";
	int32_t i;

	/* Read the read */
	if(BStringRead(&m->read, fp, binaryInput)==EOF) {
		return EOF;
	}

	/* Read the matches from the input file */
	if(binaryInput == TextInput) {

		/* Read in if we have reached the maximum number of matches */
		if(fscanf(fp, "%d", &m->maxReached)==EOF) {
			PrintError(FnName,
					"m->maxReached",
					"Could not read in m->maxReached",
					Exit,
					EndOfFile);
		}

		/* Read in the number of matches */
		if(fscanf(fp, "%d", &m->numEntries)==EOF) {
			PrintError(FnName,
					"m->numEntries",
					"Could not read in m->numEntries",
					Exit,
					EndOfFile);
		}
		assert(m->numEntries >= 0);

		/* Allocate memory for the matches */
		BMatchReallocate(m, m->numEntries);

		/* Read first sequence matches */
		for(i=0;i<m->numEntries;i++) {
			if(fscanf(fp, "%u %d %c", 
						&m->contigs[i],
						&m->positions[i],
						&m->strand[i])==EOF) {
				PrintError(FnName,
						NULL,
						"Could not read in match",
						Exit,
						EndOfFile);
			}
		}
	}
	else {

		/* Read in if we have reached the maximum number of matches */
		if(fread(&m->maxReached, sizeof(int32_t), 1, fp)!=1) {
			PrintError(FnName,
					"m->maxReached",
					"Could not read in m->maxReached",
					Exit,
					ReadFileError);
		}
		assert(m->maxReached == 0 || m->maxReached == 1);

		/* Read in the number of matches */
		if(fread(&m->numEntries, sizeof(int32_t), 1, fp)!=1) {
			PrintError(FnName,
					"m->numEntries",
					"Could not read in m->numEntries",
					Exit,
					ReadFileError);
		}
		assert(m->numEntries >= 0);

		/* Allocate memory for the matches */
		BMatchReallocate(m, m->numEntries);

		/* Read first sequence matches */
		if(fread(m->contigs, sizeof(uint32_t), m->numEntries, fp)!=m->numEntries) {
			PrintError(FnName,
					"m->contigs",
					"Could not read in contigs",
					Exit,
					ReadFileError);
		}
		if(fread(m->positions, sizeof(uint32_t), m->numEntries, fp)!=m->numEntries) {
			PrintError(FnName,
					"m->positions",
					"Could not read in positions",
					Exit,
					ReadFileError);
		}
		if(fread(m->strand, sizeof(int8_t), m->numEntries, fp)!=m->numEntries) {
			PrintError(FnName,
					"m->strand",
					"Could not read in strand",
					Exit,
					ReadFileError);
		}
	}

	return 1;
}

/* TODO */
void BMatchPrint(FILE *fp,
		BMatch *m,
		int32_t binaryOutput)
{
	char *FnName = "BMatchPrint";
	int32_t i;
	assert(fp!=NULL);
	assert(m->read.length > 0);

	/* Print the read */
	if(BStringPrint(&m->read, fp, binaryOutput)<0) {
		PrintError(FnName,
				NULL,
				"Could not write m->read",
				Exit,
				WriteFileError);
	}

	/* Print the matches to the output file */
	if(binaryOutput == TextOutput) {
		if(0 > fprintf(fp, "%d\t%d",
					m->maxReached,
					m->numEntries)) {
			PrintError(FnName,
					NULL,
					"Could not write m->maxReached, and m->numEntries",
					Exit,
					WriteFileError);
		}

		for(i=0;i<m->numEntries;i++) {
			assert(m->contigs[i] > 0);
			if(0 > fprintf(fp, "\t%u\t%d\t%c", 
						m->contigs[i],
						m->positions[i],
						m->strand[i])) {
				PrintError(FnName,
						NULL,
						"Could not write m->contigs[i], m->positions[i], and m->strand[i]",
						Exit,
						WriteFileError);
			}
		}
		if(0 > fprintf(fp, "\n")) {
			PrintError(FnName,
					NULL,
					"Could not write newline",
					Exit,
					WriteFileError);
		}
	}
	else {
		if(fwrite(&m->maxReached, sizeof(int32_t), 1, fp) != 1 ||
				fwrite(&m->numEntries, sizeof(int32_t), 1, fp) != 1) {
			PrintError(FnName,
					NULL,
					"Could not write m->maxReached, and m->numEntries",
					Exit,
					WriteFileError);
		}

		/* Print the contigs, positions, and strands */
		if(fwrite(m->contigs, sizeof(uint32_t), m->numEntries, fp) != m->numEntries ||
				fwrite(m->positions, sizeof(uint32_t), m->numEntries, fp) != m->numEntries ||
				fwrite(m->strand, sizeof(int8_t), m->numEntries, fp) != m->numEntries) {
			PrintError(FnName,
					NULL,
					"Could not write contigs, positions and strands",
					Exit,
					WriteFileError);
		}
	}
}

/* TODO */
void BMatchRemoveDuplicates(BMatch *m,
		int32_t maxNumMatches)
{
	int32_t i;
	int32_t prevIndex=0;

	/* Check to see if the max has been reached.  If so free all matches and return.
	 * We should remove duplicates before checking against maxNumMatches. */
	if(m->maxReached == 1) {
		/* Clear the matches but don't free the read name */
		BMatchClearMatches(m);
		m->maxReached=1;
		return;
	}

	if(m->numEntries > 0) {
		/* Quick sort the data structure */
		BMatchQuickSort(m, 0, m->numEntries-1);

		/* Remove duplicates */
		prevIndex=0;
		for(i=1;i<m->numEntries;i++) {
			if(BMatchCompareAtIndex(m, prevIndex, m, i)==0) {
				/* ignore */
			}
			else {
				prevIndex++;
				/* Copy to prevIndex (incremented) */
				BMatchCopyAtIndex(m, i, m, prevIndex);
			}
		}

		/* Reallocate pair */
		/* does not make sense if there are no entries */
		BMatchReallocate(m, prevIndex+1);

		/* Check to see if we have too many matches */
		if(m->numEntries > maxNumMatches) {
			/* Clear the entries but don't free the read */
			BMatchClearMatches(m);
			m->maxReached=1;
		}
		else { 
			m->maxReached = 0;
		}
	}
}

/* TODO */
void BMatchQuickSort(BMatch *m, int32_t low, int32_t high)
{
	int32_t i;
	int32_t pivot=-1;
	BMatch *temp=NULL;

	if(low < high) {
		/* Allocate memory for the temp used for swapping */
		temp=malloc(sizeof(BMatch));
		BMatchInitialize(temp);
		if(NULL == temp) {
			PrintError("BMatchQuickSort",
					"temp",
					"Could not allocate memory",
					Exit,
					MallocMemory);
		}
		BMatchAllocate(temp, 1);

		pivot = (low+high)/2;

		BMatchCopyAtIndex(m, pivot, temp, 0);
		BMatchCopyAtIndex(m, high, m, pivot);
		BMatchCopyAtIndex(temp, 0, m, high);

		pivot = low;

		for(i=low;i<high;i++) {
			if(BMatchCompareAtIndex(m, i, m, high) <= 0) {
				if(i!=pivot) {
					BMatchCopyAtIndex(m, i, temp, 0);
					BMatchCopyAtIndex(m, pivot, m, i);
					BMatchCopyAtIndex(temp, 0, m, pivot);
				}
				pivot++;
			}
		}
		BMatchCopyAtIndex(m, pivot, temp, 0);
		BMatchCopyAtIndex(m, high, m, pivot);
		BMatchCopyAtIndex(temp, 0, m, high);

		/* Free temp before the recursive call, otherwise we have a worst
		 * case of O(n) space (NOT IN PLACE) 
		 * */
		BMatchFree(temp);
		free(temp);
		temp=NULL;

		BMatchQuickSort(m, low, pivot-1);
		BMatchQuickSort(m, pivot+1, high);
	}
}

/* TODO */
int32_t BMatchCompareAtIndex(BMatch *mOne, int32_t indexOne, BMatch *mTwo, int32_t indexTwo) 
{
	assert(indexOne >= 0 && indexOne < mOne->numEntries);
	assert(indexTwo >= 0 && indexTwo < mTwo->numEntries);
	if(mOne->contigs[indexOne] < mTwo->contigs[indexTwo] ||
			(mOne->contigs[indexOne] == mTwo->contigs[indexTwo] && mOne->positions[indexOne] < mTwo->positions[indexTwo]) ||
			(mOne->contigs[indexOne] == mTwo->contigs[indexTwo] && mOne->positions[indexOne] == mTwo->positions[indexTwo] && mOne->strand[indexOne] < mTwo->strand[indexTwo])) {
		return -1;
	}
	else if(mOne->contigs[indexOne] ==  mTwo->contigs[indexTwo] && mOne->positions[indexOne] == mTwo->positions[indexTwo] && mOne->strand[indexOne] == mTwo->strand[indexTwo]) {
		return 0;
	}
	else {
		return 1;
	}
}

/* TODO */
void BMatchAppend(BMatch *src, BMatch *dest)
{
	char *FnName = "BMatchAppend";
	int32_t i, start;

	/* Make sure we are not appending to ourselves */
	assert(src != dest);

	/* Check to see if we need to copy over the read as well */
	if(dest->length <= 0) {
		BStringCopy(&dest->read, &src->read);
	}

	start = dest->numEntries;
	/* Allocate memory for the entires */
	BMatchReallocate(dest, dest->numEntries + src->numEntries);

	assert(dest->numEntries == start + src->numEntries);
	assert(start <= dest->numEntries);

	/* Copy over the entries */
	for(i=start;i<dest->numEntries;i++) {
		BMatchCopyAtIndex(src, i-start, dest, i);
	}
}

/* TODO */
void BMatchCopyAtIndex(BMatch *src, int32_t srcIndex, BMatch *dest, int32_t destIndex)
{
	assert(srcIndex >= 0 && srcIndex < src->numEntries);
	assert(destIndex >= 0 && destIndex < dest->numEntries);

	if(src != dest || srcIndex != destIndex) {
		dest->positions[destIndex] = src->positions[srcIndex];
		dest->contigs[destIndex] = src->contigs[srcIndex];
		dest->strand[destIndex] = src->strand[srcIndex];
	}
}

/* TODO */
void BMatchAllocate(BMatch *m, int32_t numEntries)
{
	char *FnName = "BMatchAllocate";
	assert(m->numEntries==0);
	m->numEntries = numEntries;
	assert(m->positions==NULL);
	m->positions = malloc(sizeof(uint32_t)*numEntries); 
	if(NULL == m->positions) {
		PrintError(FnName,
				"m->positions",
				"Could not allocate memory",
				Exit,
				MallocMemory);
	}
	assert(m->contigs==NULL);
	m->contigs = malloc(sizeof(uint32_t)*numEntries); 
	if(NULL == m->contigs) {
		PrintError(FnName,
				"m->contigs",
				"Could not allocate memory",
				Exit,
				MallocMemory);
	}
	assert(m->strand==NULL);
	m->strand = malloc(sizeof(int8_t)*numEntries); 
	if(NULL == m->strand) {
		PrintError(FnName,
				"m->strand",
				"Could not allocate memory",
				Exit,
				MallocMemory);
	}
}

/* TODO */
void BMatchReallocate(BMatch *m, int32_t numEntries)
{
	char *FnName = "BMatchReallocate";
	if(numEntries > 0) {
		m->numEntries = numEntries;
		m->positions = realloc(m->positions, sizeof(uint32_t)*numEntries); 
		if(numEntries > 0 && NULL == m->positions) {
			/*
			   fprintf(stderr, "numEntries:%d\n", numEntries);
			   */
			PrintError(FnName,
					"m->positions",
					"Could not reallocate memory",
					Exit,
					ReallocMemory);
		}
		m->contigs = realloc(m->contigs, sizeof(uint32_t)*numEntries); 
		if(numEntries > 0 && NULL == m->contigs) {
			PrintError(FnName,
					"m->contigs",
					"Could not reallocate memory",
					Exit,
					ReallocMemory);
		}
		m->strand = realloc(m->strand, sizeof(int8_t)*numEntries); 
		if(numEntries > 0 && NULL == m->strand) {
			PrintError(FnName,
					"m->strand",
					"Could not reallocate memory",
					Exit,
					ReallocMemory);
		}
	}
	else {
		/* Free just the matches part, not the meta-data */
		BMatchClearMatches(m);
	}
}

/* TODO */
/* Does not free read */
void BMatchClearMatches(BMatch *m) 
{
	m->maxReached=0;
	m->numEntries=0;
	/* Free */
	free(m->contigs);
	free(m->positions);
	free(m->strand);
	m->contigs=NULL;
	m->positions=NULL;
	m->strand=NULL;
}

/* TODO */
void BMatchFree(BMatch *m) 
{
	BStringFree(&m->read);
	free(m->contigs);
	free(m->positions);
	free(m->strand);
	BMatchInitialize(m);
}

/* TODO */
void BMatchInitialize(BMatch *m)
{
	BStringInitialize(&read);
	m->maxReached=0;
	m->numEntries=0;
	m->contigs=NULL;
	m->positions=NULL;
	m->strand=NULL;
}

/* TODO */
void BMatchCheck(BMatch *m)
{
	char *FnName="BMatchCheck";
	/* Basic asserts */
	assert(m->read.length >= 0);
	assert(m->maxReached == 0 || m->maxReached == 1);
	assert(m->maxReached == 0 || m->numEntries == 0);
	assert(m->numEntries >= 0);
	/* Check that if the read length is greater than zero the read is not null */
	if(m->read.length > 0 && m->read.string == NULL) {
		PrintError(FnName,
				NULL,
				"m->read.length > 0 && m->read.string == NULL",
				Exit,
				OutOfRange);
	}
	/* Check that the read length matches the read */
	if(((int)strlen((char*)m->read.string)) != m->read.length) {
		PrintError(FnName,
				NULL,
				"m->read.length and strlen(m->read.string) do not match",
				Exit,
				OutOfRange);
	}
	/* Check that if the max has been reached then there are no entries */
	if(1==m->maxReached && m->numEntries > 0) {
		PrintError(FnName,
				NULL,
				"1==m->maxReached and m->numEntries>0",
				Exit,
				OutOfRange);
	}
	/* Check that if the number of entries is greater than zero that the entries are not null */
	if(m->numEntries > 0 && (m->contigs == NULL || m->positions == NULL || m->strand == NULL)) {
		PrintError(FnName,
				NULL,
				"m->numEntries > 0 && (m->contigs == NULL || m->positions == NULL || m->strand == NULL)",
				Exit,
				OutOfRange);
	}
}

/* TODO */
void BMatchFilterOutOfRange(BMatch *m,
		int32_t startContig,
		int32_t startPos,
		int32_t endContig,
		int32_t endPos,
		int32_t maxNumMatches)
{
	int32_t i, prevIndex;

	/* Filter contig/pos */
	/* Remove duplicates */
	prevIndex = -1;
	int filter;
	for(i=0;i<m->numEntries;i++) {
		filter = 0;
		if(m->contigs[i] < startContig || 
				(m->contigs[i] == startContig && (m->positions[i] + m->read.length - 1) < startPos) ||
				(m->contigs[i] == endContig && m->positions[i] > endPos) ||
				(m->contigs[i] > endContig)) {
			/* ignore */
		}
		else {
			/* Do not filter */
			prevIndex++;
			/* Copy contig/pos at i to contig/pos at prevIndex */
			BMatchCopyAtIndex(m, i, m, prevIndex);
		}
	}

	/* Reallocate pair */
	BMatchReallocate(m, prevIndex+1);

	/* Filter based on the maximum number of matches */
	if(maxNumMatches != 0 && m->numEntries > maxNumMatches) {
		/* Do not align this one */
		BMatchClearMatches(m);
		assert(m->read.length > 0);
	}
}
