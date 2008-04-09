#ifndef RGSEQPAIR_H_
#define RGSEQPAIR_H_

#include <stdio.h>
#include "BLibDefinitions.h"
#include "RGMatch.h"
#include "RGIndex.h"
#include "RGTree.h"

void RGSeqPairFindMatchesInIndex(RGIndex*, RGMatch*, char*);
void RGSeqPairFindMatchesInTree(RGTree*, RGMatch*, char*, int**, int, int, int, int, int, int);
void RGSeqPairGeneratePairs(char*, RGSeqPair*, int**, int, int, int, int, int, int, int, int);
void RGSeqPairGenerateMismatches(char*, int, char, int, int, int, int, RGSeqPair*);
void RGSeqPairGenerateMismatchesHelper(char*, char, int, int, int, int, int, int, RGSeqPair*, char*, char*, int, int);
void RGSeqPairGenerateDeletions(char*, int, char, int, int, int, int, RGSeqPair*);
void RGSeqPairGenerateDeletionsHelper(char*, int, char, int, int, int, int, int, int, RGSeqPair*, char*, char*, int, int);
void RGSeqPairGenerateInsertions(char*, int, char, int, int, int, int, RGSeqPair*);
void RGSeqPairGenerateInsertionsHelper(char*, int, char, int, int, int, int, int, int, RGSeqPair*, char*, char*, int, int);
void RGSeqPairGenerateGapDeletions(char*, int, char, int, int, int, int, RGSeqPair*);
void RGSeqPairGenerateGapDeletionsHelper(char*, int, char, int, int, int, int, RGSeqPair*, char*, char*);
void RGSeqPairGenerateGapInsertions(char*, int, char, int, int, int, int, RGSeqPair*);
void RGSeqPairGenerateGapInsertionsHelper(char*, int, char, int, int, int, int, RGSeqPair*, char*, char*);

void RGSeqPairRemoveDuplicates(RGSeqPair*);
void RGSeqPairQuickSort(RGSeqPair*, int, int);

void GetReverseCompliment(char*, char*, int);

int RGSeqPairCompareAtIndex(RGSeqPair*, int, RGSeqPair*, int);
void RGSeqPairCopyAtIndex(RGSeqPair*, int, RGSeqPair*, int);

void RGSeqPairAllocate(RGSeqPair*, int);
void RGSeqPairReallocate(RGSeqPair*, int);

#endif

