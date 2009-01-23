#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include <math.h>
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "../blib/BLib.h"
#include "../blib/BLibDefinitions.h"
#include "../blib/BError.h"
#include "../blib/AlignEntry.h"
#include "ScoringMatrix.h"
#include "Align.h"
#include "AlignColorSpace.h"

/* TODO */
int AlignColorSpace(char *read,
		int readLength,
		char *reference,
		int referenceLength,
		ScoringMatrix *sm,
		AlignEntry *a,
		char strand,
		int type,
		int scoringType)
{
	char *FnName="AlignColorSpace"; 
	switch(type) {
		case MismatchesOnly:
			return AlignColorSpaceMismatchesOnly(read,
					readLength,
					reference,
					referenceLength,
					scoringType,
					sm,
					a,
					strand);
			break;
		case FullAlignment:
#ifdef COLOR_SPACE_UNOPTIMIZED
			return AlignColorSpaceFull(read,
					readLength,
					reference,
					referenceLength,
					scoringType,
					sm,
					a,
					strand);
#else
			return AlignColorSpaceFullOpt(read,
					readLength,
					reference,
					referenceLength,
					scoringType,
					sm,
					a,
					strand);
#endif
			break;
		default:
			PrintError(FnName,
					"type",
					"Could not understand alignment type",
					Exit,
					OutOfRange);
	}
	return -1;
}

/* TODO */
int AlignColorSpaceMismatchesOnly(char *read,
		int readLength,
		char *reference,
		int referenceLength,
		int scoringType,
		ScoringMatrix *sm,
		AlignEntry *a,
		char strand)
{
	/* read goes on the rows, reference on the columns */
	char *FnName = "AlignColorSpaceMismatchesOnly";
	int i, j, k, l;

	int offset=-1;
	int32_t prevScore[4];
	int32_t prevScoreNT[4];
	int prevNT[4][SEQUENCE_LENGTH];
	int32_t maxScore = NEGATIVE_INFINITY;
	int32_t maxScoreNT = NEGATIVE_INFINITY;
	int maxNT[SEQUENCE_LENGTH];
	char DNA[ALPHABET_SIZE] = "ACGT";

	if(readLength > referenceLength) {
		fprintf(stderr, "%s[%d]\n%s[%d]\n",
				read,
				readLength,
				reference,
				referenceLength);
	}
	assert(readLength <= referenceLength);

	for(i=0;i<referenceLength-readLength+1;i++) { /* Starting position */
		/* Initialize */
		for(j=0;j<4;j++) {
			if(DNA[j] == COLOR_SPACE_START_NT) { 
				prevScore[j] = prevScoreNT[j] = 0.0;
			}
			else {
				prevScore[j] = prevScoreNT[j] = NEGATIVE_INFINITY;
			}
		}
		for(j=0;j<readLength;j++) { /* Position in the alignment */
			uint8_t curColor;
			uint8_t curReadBase = read[j];
			uint8_t prevReadBase = (j==0)?COLOR_SPACE_START_NT:read[j-1];

			/* Get the current color for the read */
			if(0 == ConvertBaseToColorSpace(prevReadBase, curReadBase, &curColor)) {
				fprintf(stderr, "prevReadBase=%c\tcurReadBase=%c\n",
						prevReadBase,
						curReadBase);
				PrintError(FnName,
						"curColor",
						"Could not convert base to color space",
						Exit,
						OutOfRange);
			}
			int32_t nextScore[4];
			int32_t nextScoreNT[4];
			uint8_t nextNT[4];
			for(k=0;k<ALPHABET_SIZE;k++) { /* To NT */

				/* Get the best score to this NT */
				int32_t bestScore = NEGATIVE_INFINITY;
				int32_t bestScoreNT = NEGATIVE_INFINITY;
				int bestNT=-1;
				uint8_t bestColor = 'X';

				for(l=0;l<ALPHABET_SIZE;l++) { /* From NT */
					uint8_t convertedColor='X';
					int32_t curScore = prevScore[l];
					int32_t curScoreNT = prevScoreNT[l]; 
					/* Get color */
					if(0 == ConvertBaseToColorSpace(DNA[l], DNA[k], &convertedColor)) {
						fprintf(stderr, "DNA[l=%d]=%c\tDNA[k=%d]=%c\n",
								l,
								DNA[l],
								k,
								DNA[k]);
						PrintError(FnName,
								"convertedColor",
								"Could not convert base to color space",
								Exit,
								OutOfRange);
					}
					/* Add score for color error, if any */
					curScore += ScoringMatrixGetColorScore(curColor,
							convertedColor,
							sm);
					/* Add score for NT */
					curScore += ScoringMatrixGetNTScore(reference[i+j], DNA[k], sm);
					curScoreNT += ScoringMatrixGetNTScore(reference[i+j], DNA[k], sm);

					if(curScore < NEGATIVE_INFINITY/2) {
						curScore = NEGATIVE_INFINITY;
						curScoreNT = NEGATIVE_INFINITY;
					}

					if(bestScore < curScore) {
						bestScore = curScore;
						bestScoreNT = curScoreNT;
						bestNT = l;
						bestColor = convertedColor;
					}
				}
				nextScore[k] = bestScore;
				nextScoreNT[k] = bestScoreNT;
				nextNT[k] = bestNT;
			}
			for(k=0;k<ALPHABET_SIZE;k++) { /* To NT */
				prevScore[k] = nextScore[k];
				prevScoreNT[k] = nextScoreNT[k];
				prevNT[k][j] = nextNT[k];
				/*
				   fprintf(stderr, "k=%d\tscore=%lf\tscoreNT=%lf\tfromNT=%d\n",
				   k,
				   prevScore[k],
				   prevScoreNT[k],
				   prevNT[k][j]);
				   */
			}
		}
		/* Check if the score is better than the max */
		k=0;
		for(j=1;j<ALPHABET_SIZE;j++) { /* To NT */
			if(prevScore[k] < prevScore[j]) {
				k=j;
			}
		}
		if(maxScore < prevScore[k]) {
			maxScore = prevScore[k];
			maxScoreNT = prevScoreNT[k];
			/* TO GET COLORS WE NEED TO BACKTRACK */
			for(j=readLength-1;0<=j;j--) {
				maxNT[j] = k;
				k=prevNT[k][j];
			}
			offset = i;
		}
	}

	/* Copy over */
	a->referenceLength = readLength;
	a->length = readLength;
	/* Copy over score */
	if(scoringType == NTSpace) {
		a->score = maxScoreNT;
	}
	else {
		a->score = maxScore;
	}
	/* Allocate memory */
	assert(NULL==a->read);
	a->read = malloc(sizeof(char)*(a->length+1));
	if(NULL==a->read) {
		PrintError(FnName,
				"a->read",
				"Could not allocate memory",
				Exit,
				MallocMemory);
	}
	assert(NULL==a->reference);
	a->reference = malloc(sizeof(char)*(a->length+1));
	if(NULL==a->reference) {
		PrintError(FnName,
				"a->reference",
				"Could not allocate memory",
				Exit,
				MallocMemory);
	}
	assert(NULL==a->colorError);

	a->colorError = malloc(sizeof(char)*SEQUENCE_LENGTH);
	if(NULL==a->colorError) {
		PrintError(FnName,
				"a->colorError",
				"Could not allocate memory",
				Exit,
				MallocMemory);
	}

	/* Copy over */
	for(i=0;i<a->length;i++) {

		uint8_t c[2];
		a->read[i] = DNA[maxNT[i]];
		a->reference[i] = reference[i+offset];
		ConvertBaseToColorSpace((i==0)?COLOR_SPACE_START_NT:read[i-1],
				read[i],
				&c[0]);
		ConvertBaseToColorSpace((i==0)?COLOR_SPACE_START_NT:a->read[i-1],
				a->read[i],
				&c[1]);
		a->colorError[i] = (c[0] == c[1])?'0':'1';
	}
	a->read[a->length] = '\0';
	a->reference[a->length] = '\0';
	a->colorError[a->length] = '\0';

	/* The return is the number of gaps at the beginning of the reference */
	return offset;
}

/* TODO */
int AlignColorSpaceFull(char *read,
		int readLength,
		char *reference,
		int referenceLength,
		int scoringType,
		ScoringMatrix *sm,
		AlignEntry *a,
		char strand)
{
	/* read goes on the rows, reference on the columns */
	char *FnName = "AlignColorSpaceFull2";
	AlignMatrixCS **matrix=NULL;
	int offset = 0;
	int i, j, k, l;

	/* Allocate memory for the matrix */
	matrix = malloc(sizeof(AlignMatrixCS*)*(readLength+1));
	if(NULL==matrix) {
		PrintError(FnName,
				"matrix",
				"Could not allocate memory",
				Exit,
				MallocMemory);
	}
	for(i=0;i<readLength+1;i++) {
		matrix[i] = malloc(sizeof(AlignMatrixCS)*(referenceLength+1));
		if(NULL==matrix[i]) {
			PrintError(FnName,
					"matrix[i]",
					"Could not allocate memory",
					Exit,
					MallocMemory);
		}
	}

	/* Initialize "the matrix" */
	/* Row i (i>0) column 0 should be negative infinity since we want to
	 * align the full read */
	for(i=1;i<readLength+1;i++) {
		for(k=0;k<ALPHABET_SIZE+1;k++) {
			matrix[i][0].h.score[k] = NEGATIVE_INFINITY;
			matrix[i][0].h.scoreNT[k] = NEGATIVE_INFINITY;
			matrix[i][0].h.from[k] = Start;
			matrix[i][0].h.length[k] = 0;
			matrix[i][0].h.colorError[k] = '0';

			matrix[i][0].s.score[k] = NEGATIVE_INFINITY;
			matrix[i][0].s.scoreNT[k] = NEGATIVE_INFINITY;
			matrix[i][0].s.from[k] = Start;
			matrix[i][0].s.length[k] = 0;
			matrix[i][0].s.colorError[k] = '0';

			matrix[i][0].v.score[k] = NEGATIVE_INFINITY;
			matrix[i][0].v.scoreNT[k] = NEGATIVE_INFINITY;
			matrix[i][0].v.from[k] = Start;
			matrix[i][0].v.length[k] = 0;
			matrix[i][0].v.colorError[k] = '0';
		}
	}
	/* Row 0 column j should be zero since we want to find the best
	 * local alignment within the reference */
	for(j=0;j<referenceLength+1;j++) {
		for(k=0;k<ALPHABET_SIZE+1;k++) {
			matrix[0][j].h.score[k] = NEGATIVE_INFINITY;
			matrix[0][j].h.scoreNT[k] = NEGATIVE_INFINITY;
			matrix[0][j].h.from[k] = Start;
			matrix[0][j].h.length[k] = 0;
			matrix[0][j].h.colorError[k] = '0';

			/* Assumes both DNA and COLOR_SPACE_START_NT are upper case */
			if(DNA[k] == COLOR_SPACE_START_NT) { 
				/* Starting adaptor NT */
				matrix[0][j].s.score[k] = 0;
				matrix[0][j].s.scoreNT[k] = 0;
			}
			else {
				matrix[0][j].s.score[k] = NEGATIVE_INFINITY;
				matrix[0][j].s.scoreNT[k] = NEGATIVE_INFINITY;
			}
			matrix[0][j].s.from[k] = Start;
			matrix[0][j].s.length[k] = 0;
			matrix[0][j].s.colorError[k] = '0';

			matrix[0][j].v.score[k] = NEGATIVE_INFINITY;
			matrix[0][j].v.scoreNT[k] = NEGATIVE_INFINITY;
			matrix[0][j].v.from[k] = Start;
			matrix[0][j].v.length[k] = 0;
			matrix[0][j].v.colorError[k] = '0';
		}
	}

	/* Fill in the matrix according to the recursive rules */
	for(i=0;i<readLength;i++) { /* read/rows */
		/* Get the current color */
		uint8_t curColor;
		char curReadBase, prevReadBase;
		/* In color space, the first color is determined by the adapter NT */
		curReadBase = read[i];
		prevReadBase = (i==0)?COLOR_SPACE_START_NT:read[i-1];

		/* Get the current color for the read */
		if(0 == ConvertBaseToColorSpace(prevReadBase, curReadBase, &curColor)) {
			fprintf(stderr, "prevReadBase=%c\tcurReadBase=%c\n",
					prevReadBase,
					curReadBase);
			PrintError(FnName,
					"curColor",
					"Could not convert base to color space",
					Exit,
					OutOfRange);
		}

		for(j=0;j<referenceLength;j++) { /* reference/columns */

			/* Deletion */
			for(k=0;k<ALPHABET_SIZE+1;k++) { /* To NT */
				int32_t maxScore = NEGATIVE_INFINITY-1;
				int32_t maxScoreNT = NEGATIVE_INFINITY-1;
				int maxFrom = -1;
				char maxColorError = '0';
				int maxLength = 0;

				int32_t curScore=NEGATIVE_INFINITY;
				int32_t curScoreNT=NEGATIVE_INFINITY;
				int curLength=-1;

				/* Deletion starts or extends from the same base */

				/* New deletion */
				curLength = matrix[i+1][j].s.length[k] + 1;
				/* Deletion - previous column */
				/* Ignore color error since one color will span the entire
				 * deletion.  We will consider the color at the end of the deletion.
				 * */
				curScore = curScoreNT = matrix[i+1][j].s.score[k] + sm->gapOpenPenalty;
				/* Make sure we aren't below infinity */
				if(curScore < NEGATIVE_INFINITY/2) {
					curScore = NEGATIVE_INFINITY;
					curScoreNT = NEGATIVE_INFINITY;
					assert(curScore < 0);
				}
				if(curScore > maxScore) {
					maxScore = curScore;
					maxScoreNT = curScoreNT;
					maxFrom = k + 1 + (ALPHABET_SIZE + 1); /* see the enum */ 
					maxColorError = '0';
					maxLength = curLength;
				}

				/* Extend current deletion */
				curLength = matrix[i+1][j].h.length[k] + 1;
				/* Deletion - previous column */
				curScore = curScoreNT = matrix[i+1][j].h.score[k] + sm->gapExtensionPenalty;
				/* Ignore color error since one color will span the entire
				 * deletion.  We will consider the color at the end of the deletion.
				 * */
				/* Make sure we aren't below infinity */
				if(curScore < NEGATIVE_INFINITY/2) {
					curScore = NEGATIVE_INFINITY;
					curScoreNT = NEGATIVE_INFINITY;
				}
				if(curScore > maxScore) {
					maxScore = curScore;
					maxScoreNT = curScoreNT;
					maxFrom = k + 1; /* see the enum */ 
					maxColorError = '0';
					maxLength = curLength;
				}
				/* Update */
				matrix[i+1][j+1].h.score[k] = maxScore;
				matrix[i+1][j+1].h.scoreNT[k] = maxScoreNT;
				matrix[i+1][j+1].h.from[k] = maxFrom;
				matrix[i+1][j+1].h.colorError[k] = maxColorError;
				matrix[i+1][j+1].h.length[k] = maxLength;
			}

			/* Match/Mismatch */
			for(k=0;k<ALPHABET_SIZE+1;k++) { /* To NT */
				int32_t maxScore = NEGATIVE_INFINITY-1;
				int32_t maxScoreNT = NEGATIVE_INFINITY-1;
				int maxFrom = -1;
				char maxColorError = '0';
				int maxLength = 0;

				for(l=0;l<ALPHABET_SIZE+1;l++) { /* From NT */
					int32_t curScore=NEGATIVE_INFINITY;
					int32_t curScoreNT=NEGATIVE_INFINITY;
					int curLength=-1;
					uint8_t convertedColor='X';
					int32_t scoreNT, scoreColor;

					/* Get color */
					if(0 == ConvertBaseToColorSpace(DNA[l], DNA[k], &convertedColor)) {
						fprintf(stderr, "DNA[l=%d]=%c\tDNA[k=%d]=%c\n",
								l,
								DNA[l],
								k,
								DNA[k]);
						PrintError(FnName,
								"convertedColor",
								"Could not convert base to color space",
								Exit,
								OutOfRange);
					}
					/* Get NT and Color scores */
					scoreNT = ScoringMatrixGetNTScore(reference[j], DNA[k], sm);
					scoreColor = ScoringMatrixGetColorScore(curColor,
							convertedColor,
							sm);

					/* From Horizontal - Deletion */
					curLength = matrix[i][j].h.length[l] + 1;
					/* Add previous with current NT */
					curScore = curScoreNT = matrix[i][j].h.score[l] + scoreNT;
					/* Add score for color error, if any */
					curScore += scoreColor;
					/* Make sure we aren't below infinity */
					if(curScore < NEGATIVE_INFINITY/2) {
						curScore = NEGATIVE_INFINITY;
						curScoreNT = NEGATIVE_INFINITY;
					}
					if(curScore > maxScore) {
						maxScore = curScore;
						maxScoreNT = curScoreNT;
						maxFrom = l + 1; /* see the enum */ 
						maxColorError = (curColor == convertedColor)?'0':'1';
						maxLength = curLength;
					}

					/* From Vertical - Insertion */
					curLength = matrix[i][j].v.length[l] + 1;
					/* Add previous with current NT */
					curScore = curScoreNT = matrix[i][j].v.score[l] + scoreNT;
					/* Add score for color error, if any */
					curScore += scoreColor;
					/* Make sure we aren't below infinity */
					if(curScore < NEGATIVE_INFINITY/2) {
						curScore = NEGATIVE_INFINITY;
						curScoreNT = NEGATIVE_INFINITY;
					}
					if(curScore > maxScore) {
						maxScore = curScore;
						maxScoreNT = curScoreNT;
						maxFrom = l + 1 + 2*(ALPHABET_SIZE + 1); /* see the enum */ 
						maxColorError = (curColor == convertedColor)?'0':'1';
						maxLength = curLength;
					}

					/* From Diagonal - Match/Mismatch */
					curLength = matrix[i][j].s.length[l] + 1;
					/* Add previous with current NT */
					curScore = curScoreNT = matrix[i][j].s.score[l] + scoreNT;
					/* Add score for color error, if any */
					curScore += scoreColor;
					/* Make sure we aren't below infinity */
					if(curScore < NEGATIVE_INFINITY/2) {
						curScore = NEGATIVE_INFINITY;
						curScoreNT = NEGATIVE_INFINITY;
					}
					if(curScore > maxScore) {
						maxScore = curScore;
						maxScoreNT = curScoreNT;
						maxFrom = l + 1 + (ALPHABET_SIZE + 1); /* see the enum */ 
						maxColorError = (curColor == convertedColor)?'0':'1';
						maxLength = curLength;
					}
				}
				/* Update */
				matrix[i+1][j+1].s.score[k] = maxScore;
				matrix[i+1][j+1].s.scoreNT[k] = maxScoreNT;
				matrix[i+1][j+1].s.from[k] = maxFrom;
				matrix[i+1][j+1].s.colorError[k] = maxColorError;
				matrix[i+1][j+1].s.length[k] = maxLength;
			}

			/* Insertion */
			for(k=0;k<ALPHABET_SIZE+1;k++) { /* To NT */
				int32_t maxScore = NEGATIVE_INFINITY-1;
				int32_t maxScoreNT = NEGATIVE_INFINITY-1;
				int maxFrom = -1;
				char maxColorError = '0';
				int maxLength = 0;

				int32_t curScore=NEGATIVE_INFINITY;
				int32_t curScoreNT=NEGATIVE_INFINITY;
				int curLength=-1;
				uint8_t B;
				int fromNT=-1;

				/* Get from base for extending an insertion */
				if(0 == ConvertBaseAndColor(DNA[k], curColor, &B)) {
					PrintError(FnName,
							NULL,
							"Could not convert base and color",
							Exit,
							OutOfRange);
				}
				switch(B) {
					case 'a':
					case 'A':
						fromNT=0;
						break;
					case 'c':
					case 'C':
						fromNT=1;
						break;
					case 'g':
					case 'G':
						fromNT=2;
						break;
					case 't':
					case 'T':
						fromNT=3;
						break;
					default:
						fromNT=4;
						break;
				}

				/* New insertion */
				curScore=NEGATIVE_INFINITY;
				curScoreNT=NEGATIVE_INFINITY;
				curLength=-1;
				/* Get NT and Color scores */
				curLength = matrix[i][j+1].s.length[fromNT] + 1;
				curScore = curScoreNT = matrix[i][j+1].s.score[fromNT] + sm->gapOpenPenalty;
				/*
				   curScore += ScoringMatrixGetColorScore(curColor,
				   convertedColor,
				   sm);
				   */
				/* Make sure we aren't below infinity */
				if(curScore < NEGATIVE_INFINITY/2) {
					curScore = NEGATIVE_INFINITY;
					curScoreNT = NEGATIVE_INFINITY;
				}
				if(curScore > maxScore) {
					maxScore = curScore;
					maxScoreNT = curScoreNT;
					maxFrom = fromNT + 1 + (ALPHABET_SIZE + 1); /* see the enum */ 
					maxColorError = '0';
					maxLength = curLength;
				}

				/* Extend current insertion */
				curLength = matrix[i][j+1].v.length[fromNT] + 1;
				/* Insertion - previous row */
				curScore = curScoreNT = matrix[i][j+1].v.score[fromNT] + sm->gapExtensionPenalty;
				/* Make sure we aren't below infinity */
				if(curScore < NEGATIVE_INFINITY/2) {
					curScore = NEGATIVE_INFINITY;
					curScoreNT = NEGATIVE_INFINITY;
				}
				if(curScore > maxScore) {
					maxScore = curScore;
					maxScoreNT = curScoreNT;
					maxFrom = fromNT + 1 + 2*(ALPHABET_SIZE + 1); /* see the enum */ 
					maxColorError = '0';
					maxLength = curLength;
				}

				/* Update */
				matrix[i+1][j+1].v.score[k] = maxScore;
				matrix[i+1][j+1].v.scoreNT[k] = maxScoreNT;
				matrix[i+1][j+1].v.from[k] = maxFrom;
				matrix[i+1][j+1].v.colorError[k] = maxColorError;
				matrix[i+1][j+1].v.length[k] = maxLength;
			}
		}
	}

	offset = FillAlignEntryFromMatrixColorSpace(a,
			matrix,
			read,
			readLength,
			reference,
			referenceLength,
			scoringType,
			0,
			0);

	/* Free the matrix, free your mind */
	for(i=0;i<readLength+1;i++) {
		free(matrix[i]);
		matrix[i]=NULL;
	}
	free(matrix);
	matrix=NULL;

	/* The return is the number of gaps at the beginning of the reference */
	return offset;
}

int FillAlignEntryFromMatrixColorSpace(AlignEntry *a,
		AlignMatrixCS **matrix,
		char *read,
		int readLength,
		char *reference,
		int referenceLength,
		int scoringType,
		int toExclude,
		int debug)
{
	char *FnName="FillAlignEntryFromMatrixColorSpace";
	int curRow, curCol, startRow, startCol, startCell;
	char curReadBase;
	int nextRow, nextCol, nextCell, nextFrom;
	char nextReadBase;
	int curFrom=-1;
	double maxScore;
	double maxScoreNT=NEGATIVE_INFINITY;
	int i, j;
	int offset;

	curReadBase = nextReadBase = 'X';
	nextRow = nextCol = nextCell = -1;

	/* Get the best alignment.  We can find the best score in the last row and then
	 * trace back.  We choose the best score from the last row since we want to 
	 * align the read completely and only locally to the reference. */
	startRow=-1;
	startCol=-1;
	startCell=-1;
	maxScore = NEGATIVE_INFINITY-1;
	maxScoreNT = NEGATIVE_INFINITY-1;
	for(i=1+toExclude;i<referenceLength+1;i++) {
		for(j=0;j<ALPHABET_SIZE+1;j++) {
			/* Don't end with a Deletion in the read */

			/* End with a Match/Mismatch */
			if(maxScore < matrix[readLength][i].s.score[j]) {
				maxScore = matrix[readLength][i].s.score[j];
				maxScoreNT = matrix[readLength][i].s.scoreNT[j];
				startRow = readLength;
				startCol = i;
				startCell = j + 1 + (ALPHABET_SIZE + 1);
			}

			/* End with a Insertion */
			if(maxScore < matrix[readLength][i].v.score[j]) {
				maxScore = matrix[readLength][i].v.score[j];
				maxScoreNT = matrix[readLength][i].v.scoreNT[j];
				startRow = readLength;
				startCol = i;
				startCell = j + 1 + 2*(ALPHABET_SIZE + 1);
			}
		}
	}
	assert(startRow >= 0 && startCol >= 0 && startCell >= 0);

	/* Initialize variables for the loop */
	curRow=startRow;
	curCol=startCol;
	curFrom=startCell;

	a->referenceLength=0;
	/* Init */
	if(curFrom <= (ALPHABET_SIZE + 1)) {
		a->length = matrix[curRow][curCol].h.length[(curFrom - 1) % (ALPHABET_SIZE + 1)];
	}
	else if(2*(ALPHABET_SIZE + 1) < curFrom) {
		a->length = matrix[curRow][curCol].v.length[(curFrom - 1) % (ALPHABET_SIZE + 1)];
	}
	else {
		a->length = matrix[curRow][curCol].s.length[(curFrom - 1) % (ALPHABET_SIZE + 1)];
	}
	if(a->length < readLength) {
		fprintf(stderr, "\na->length=%d\nreadLength=%d", a->length, readLength);
	}
	assert(readLength <= a->length);
	assert(readLength <= a->length);
	i=a->length-1;
	/* Copy over score */
	if(scoringType == NTSpace) {
		a->score = maxScoreNT;
	}
	else {
		a->score = maxScore;
	}

	/* Allocate memory */
	assert(NULL==a->read);
	a->read = malloc(sizeof(char)*(a->length+1));
	if(NULL==a->read) {
		PrintError(FnName,
				"a->read",
				"Could not allocate memory",
				Exit,
				MallocMemory);
	}
	assert(NULL==a->reference);
	a->reference = malloc(sizeof(char)*(a->length+1));
	if(NULL==a->reference) {
		PrintError(FnName,
				"a->reference",
				"Could not allocate memory",
				Exit,
				MallocMemory);
	}
	assert(NULL==a->colorError);
	a->colorError = malloc(sizeof(char)*(a->length+1));
	if(NULL==a->colorError) {
		PrintError(FnName,
				"a->colorError",
				"Could not allocate memory",
				Exit,
				MallocMemory);
	}

	/* Now trace back the alignment using the "from" member in the matrix */
	while(curRow > 0 && curCol > 0) {
		assert(i>=0);
		/* Where did the current cell come from */
		/* Get if there was a color error */
		if(curFrom <= (ALPHABET_SIZE + 1)) {
			/*
			   fprintf(stderr, "\ni=%d\ncurFrom=%d\nh.length=%d\n%s",
			   i,
			   curFrom,
			   matrix[curRow][curCol].h.length[(curFrom - 1) % (ALPHABET_SIZE + 1)],
			   BREAK_LINE);
			   assert(i + 1 == matrix[curRow][curCol].h.length[(curFrom - 1) % (ALPHABET_SIZE + 1)]);
			   */
			nextFrom = matrix[curRow][curCol].h.from[(curFrom - 1) % (ALPHABET_SIZE + 1)];
			a->colorError[i] = matrix[curRow][curCol].h.colorError[(curFrom - 1) % (ALPHABET_SIZE + 1)];
		}
		else if(2*(ALPHABET_SIZE + 1) < curFrom) {
			/*
			   fprintf(stderr, "\ni=%d\ncurFrom=%d\nv.length=%d\n%s",
			   i,
			   curFrom,
			   matrix[curRow][curCol].v.length[(curFrom - 1) % (ALPHABET_SIZE + 1)],
			   BREAK_LINE);
			   assert(i + 1 == matrix[curRow][curCol].v.length[(curFrom - 1) % (ALPHABET_SIZE + 1)]);
			   */
			nextFrom = matrix[curRow][curCol].v.from[(curFrom - 1) % (ALPHABET_SIZE + 1)];
			a->colorError[i] = matrix[curRow][curCol].v.colorError[(curFrom - 1) % (ALPHABET_SIZE + 1)];
		}
		else {
			/*
			   fprintf(stderr, "\ni=%d\ncurFrom=%d\ns.length=%d\n%s",
			   i,
			   curFrom,
			   matrix[curRow][curCol].s.length[(curFrom - 1) % (ALPHABET_SIZE + 1)],
			   BREAK_LINE);
			   assert(i + 1 == matrix[curRow][curCol].s.length[(curFrom - 1) % (ALPHABET_SIZE + 1)]);
			   */
			nextFrom = matrix[curRow][curCol].s.from[(curFrom - 1) % (ALPHABET_SIZE + 1)];
			a->colorError[i] = matrix[curRow][curCol].s.colorError[(curFrom - 1) % (ALPHABET_SIZE + 1)];
		}

		switch(curFrom) {
			case MatchA:
			case InsertionA:
				a->read[i] = 'A';
				break;
			case MatchC:
			case InsertionC:
				a->read[i] = 'C';
				break;
			case MatchG:
			case InsertionG:
				a->read[i] = 'G';
				break;
			case MatchT:
			case InsertionT:
				a->read[i] = 'T';
				break;
			case MatchN:
			case InsertionN:
				a->read[i] = 'N';
				break;
			case DeletionA:
			case DeletionC:
			case DeletionG:
			case DeletionT:
			case DeletionN:
				a->read[i] = GAP;
				break;
			default:
				fprintf(stderr, "curFrom=%d\n",
						curFrom);
				PrintError(FnName,
						"curFrom",
						"Could not understand curFrom",
						Exit,
						OutOfRange);
		}

		switch(curFrom) {
			case InsertionA:
			case InsertionC:
			case InsertionG:
			case InsertionT:
			case InsertionN:
				a->reference[i] = GAP;
				break;
			default:
				a->reference[i] = reference[curCol-1];
				a->referenceLength++;
				break;
		}

		assert(a->read[i] != GAP || a->read[i] != a->reference[i]);

		/* Update next row/col */
		if(curFrom <= (ALPHABET_SIZE + 1)) {
			nextRow = curRow;
			nextCol = curCol-1;
		}
		else if(2*(ALPHABET_SIZE +1) < curFrom) {
			nextRow = curRow-1;
			nextCol = curCol;
		}
		else {
			nextRow = curRow-1;
			nextCol = curCol-1;
		}

		/* Update for next loop iteration */
		curFrom = nextFrom;
		assert(curFrom > 0);
		curRow = nextRow;
		curCol = nextCol;
		i--;
	} /* End loop */
	if(-1!=i) {
		fprintf(stderr, "i=%d\n", i);
	}
	assert(-1==i);
	assert(a->length >= a->referenceLength);

	offset = curCol;
	a->read[a->length]='\0';
	a->reference[a->length]='\0';
	a->colorError[a->length]='\0';

	return offset;
}

/* TODO */
int AlignColorSpaceFullOpt(char *read,
		int readLength,
		char *reference,
		int referenceLength,
		int scoringType,
		ScoringMatrix *sm,
		AlignEntry *a,
		char strand)
{
	char *FnName = "AlignColorSpaceFullOpt";
	AlignMatrixCS **matrix=NULL;
	int i, j, k, l;
	int32_t offset;
	double lowerBound;

	/* Priorize reads */

	/* 1 - Try no indels, misatches and color errors */
	offset = KnuthMorrisPratt(read, readLength, reference, referenceLength);
	/*
	   offset = NaiveSubsequence(read, readLength, reference, referenceLength);
	   */
	/* HERE */
	offset = -1;
	if(0 <= offset) {
		/* Copy over */
		a->referenceLength = readLength;
		a->length = readLength;
		/* Copy over score */
		a->score = 0;
		char prevReadBase =  COLOR_SPACE_START_NT;
		for(i=0;i<readLength;i++) {
			if(ColorSpace == scoringType) {
				uint8_t curColor='X';
				if(0 == ConvertBaseToColorSpace(prevReadBase, read[i], &curColor)) {
					PrintError(FnName,
							"curColor",
							"Could not convert base to color space",
							Exit,
							OutOfRange);
				}
				/* Add score for color error, if any */
				a->score += ScoringMatrixGetColorScore(curColor,
						curColor,
						sm);
			}
			assert(ToLower(read[i]) == ToLower(reference[i+offset]));
			a->score += ScoringMatrixGetNTScore(read[i], read[i], sm);
		}
		/* Allocate memory */
		assert(NULL==a->read);
		a->read = malloc(sizeof(char)*(a->length+1));
		if(NULL==a->read) {
			PrintError(FnName,
					"a->read",
					"Could not allocate memory",
					Exit,
					MallocMemory);
		}
		assert(NULL==a->reference);
		a->reference = malloc(sizeof(char)*(a->length+1));
		if(NULL==a->reference) {
			PrintError(FnName,
					"a->reference",
					"Could not allocate memory",
					Exit,
					MallocMemory);
		}
		assert(NULL==a->colorError);
		a->colorError = malloc(sizeof(char)*SEQUENCE_LENGTH);
		if(NULL==a->colorError) {
			PrintError(FnName,
					"a->colorError",
					"Could not allocate memory",
					Exit,
					MallocMemory);
		}

		/* Copy over */
		for(i=0;i<a->length;i++) {
			a->read[i] = a->reference[i] = read[i];
			a->colorError[i] = '0';
		}
		a->read[a->length] = '\0';
		a->reference[a->length] = '\0';
		a->colorError[a->length] = '\0';
		return offset;
	}

	/* 2 - Get lower bound with just mismatches and color errors */
	offset = AlignColorSpaceMismatchesOnly(read,
			readLength,
			reference,
			referenceLength,
			scoringType,
			sm,
			a,
			strand);
	lowerBound = a->score;

	/* 3 - Do full alignment */

	/* Get cells to exclude */
	int toExclude=0;
	if(sm->gapOpenPenalty < sm->gapExtensionPenalty) {
		/*
		   fprintf(stderr, "\n%lf / %lf\n",
		   (double)(lowerBound - (sm->maxColorScore + sm->maxNTScore)*readLength  - sm->gapOpenPenalty + sm->gapExtensionPenalty),
		   (double)(sm->gapExtensionPenalty - sm->maxColorScore - sm->maxNTScore)); 
		   */
		double value = (lowerBound - (sm->maxColorScore + sm->maxNTScore)*readLength  - sm->gapOpenPenalty + sm->gapExtensionPenalty) / (sm->gapExtensionPenalty - sm->maxColorScore - sm->maxNTScore); 
		if(0 < value) {
			toExclude = ceil(value);
			assert(0 < toExclude);
		}
	}
	else {
		PrintError(FnName,
				PACKAGE_BUGREPORT,
				"This is currently not implemented.  Please report",
				Exit,
				OutOfRange);
	}
	if(toExclude <= 0) {
		/* Use result from searching only mismatches */
		return offset;
	}
	AlignEntryInitialize(a);

	/* Allocate memory for the matrix */
	matrix = malloc(sizeof(AlignMatrixCS*)*(readLength+1));
	if(NULL==matrix) {
		PrintError(FnName,
				"matrix",
				"Could not allocate memory",
				Exit,
				MallocMemory);
	}
	for(i=0;i<readLength+1;i++) {
		matrix[i] = malloc(sizeof(AlignMatrixCS)*(referenceLength+1));
		if(NULL==matrix[i]) {
			PrintError(FnName,
					"matrix[i]",
					"Could not allocate memory",
					Exit,
					MallocMemory);
		}
	}

	/* Initialize "the matrix" */
	/* Row i (i>0) column 0 should be negative infinity since we want to
	 * align the full read */
	for(i=1;i<readLength+1;i++) {
		for(k=0;k<ALPHABET_SIZE+1;k++) {
			matrix[i][0].h.score[k] = NEGATIVE_INFINITY;
			matrix[i][0].h.scoreNT[k] = NEGATIVE_INFINITY;
			matrix[i][0].h.from[k] = Start;
			matrix[i][0].h.length[k] = 0;
			matrix[i][0].h.colorError[k] = '0';

			matrix[i][0].s.score[k] = NEGATIVE_INFINITY;
			matrix[i][0].s.scoreNT[k] = NEGATIVE_INFINITY;
			matrix[i][0].s.from[k] = Start;
			matrix[i][0].s.length[k] = 0;
			matrix[i][0].s.colorError[k] = '0';

			matrix[i][0].v.score[k] = NEGATIVE_INFINITY;
			matrix[i][0].v.scoreNT[k] = NEGATIVE_INFINITY;
			matrix[i][0].v.from[k] = Start;
			matrix[i][0].v.length[k] = 0;
			matrix[i][0].v.colorError[k] = '0';
		}
	}
	/* Row 0 column j should be zero since we want to find the best
	 * local alignment within the reference */
	for(j=0;j<referenceLength+1;j++) {
		for(k=0;k<ALPHABET_SIZE+1;k++) {
			matrix[0][j].h.score[k] = NEGATIVE_INFINITY;
			matrix[0][j].h.scoreNT[k] = NEGATIVE_INFINITY;
			matrix[0][j].h.from[k] = Start;
			matrix[0][j].h.length[k] = 0;
			matrix[0][j].h.colorError[k] = '0';

			/* Assumes both DNA and COLOR_SPACE_START_NT are upper case */
			if(DNA[k] == COLOR_SPACE_START_NT) { 
				/* Starting adaptor NT */
				matrix[0][j].s.score[k] = 0;
				matrix[0][j].s.scoreNT[k] = 0;
			}
			else {
				matrix[0][j].s.score[k] = NEGATIVE_INFINITY;
				matrix[0][j].s.scoreNT[k] = NEGATIVE_INFINITY;
			}
			matrix[0][j].s.from[k] = Start;
			matrix[0][j].s.length[k] = 0;
			matrix[0][j].s.colorError[k] = '0';

			matrix[0][j].v.score[k] = NEGATIVE_INFINITY;
			matrix[0][j].v.scoreNT[k] = NEGATIVE_INFINITY;
			matrix[0][j].v.from[k] = Start;
			matrix[0][j].v.length[k] = 0;
			matrix[0][j].v.colorError[k] = '0';
		}
	}

	/* Fill in the matrix according to the recursive rules */
	for(i=0;i<readLength;i++) { /* read/rows */
		/* Get the current color */
		uint8_t curColor;
		char curReadBase, prevReadBase;
		/* In color space, the first color is determined by the adapter NT */
		curReadBase = read[i];
		prevReadBase = (i==0)?COLOR_SPACE_START_NT:read[i-1];

		/* Get the current color for the read */
		if(0 == ConvertBaseToColorSpace(prevReadBase, curReadBase, &curColor)) {
			fprintf(stderr, "prevReadBase=%c\tcurReadBase=%c\n",
					prevReadBase,
					curReadBase);
			PrintError(FnName,
					"curColor",
					"Could not convert base to color space",
					Exit,
					OutOfRange);
		}

		for(j=MAX(0, i - readLength + toExclude + 1);
				j <= MIN(referenceLength-1, referenceLength - toExclude + i - 1);
				j++) { /* reference/columns */
			if(!(i <= readLength - toExclude + j && j <= referenceLength - toExclude + i - 1)) {
				fprintf(stderr, "\n%d <= %d && %d <= %d\n",
						i,
						readLength - toExclude + j,
						j,
						referenceLength - toExclude + i);
				fprintf(stderr, "toExclude=%d\n", toExclude);
			}
			assert(i <= readLength - toExclude + j - 1 && j <= referenceLength - toExclude + i - 1);

			/* Deletion */
			if(i == readLength - toExclude + j - 1) {
				/* We are on the boundary, do not consider a deletion */
				for(k=0;k<ALPHABET_SIZE+1;k++) { /* To NT */
					/* Update */
					matrix[i+1][j+1].h.score[k] = NEGATIVE_INFINITY-1;
					matrix[i+1][j+1].h.scoreNT[k] = NEGATIVE_INFINITY-1;
					matrix[i+1][j+1].h.from[k] = NoFrom;
					matrix[i+1][j+1].h.colorError[k] = '0';
					matrix[i+1][j+1].h.length[k] = INT_MIN;
				}
			}
			else {
				for(k=0;k<ALPHABET_SIZE+1;k++) { /* To NT */
					int32_t maxScore = NEGATIVE_INFINITY-1;
					int32_t maxScoreNT = NEGATIVE_INFINITY-1;
					int maxFrom = -1;
					char maxColorError = '0';
					int maxLength = 0;

					int32_t curScore=NEGATIVE_INFINITY;
					int32_t curScoreNT=NEGATIVE_INFINITY;
					int curLength=-1;

					/* Deletion starts or extends from the same base */

					/* New deletion */
					curLength = matrix[i+1][j].s.length[k] + 1;
					/* Deletion - previous column */
					/* Ignore color error since one color will span the entire
					 * deletion.  We will consider the color at the end of the deletion.
					 * */
					curScore = curScoreNT = matrix[i+1][j].s.score[k] + sm->gapOpenPenalty;
					/* Make sure we aren't below infinity */
					if(curScore < NEGATIVE_INFINITY/2) {
						curScore = NEGATIVE_INFINITY;
						curScoreNT = NEGATIVE_INFINITY;
						assert(curScore < 0);
					}
					if(curScore > maxScore) {
						maxScore = curScore;
						maxScoreNT = curScoreNT;
						maxFrom = k + 1 + (ALPHABET_SIZE + 1); /* see the enum */ 
						maxColorError = '0';
						maxLength = curLength;
					}

					/* Extend current deletion */
					curLength = matrix[i+1][j].h.length[k] + 1;
					/* Deletion - previous column */
					curScore = curScoreNT = matrix[i+1][j].h.score[k] + sm->gapExtensionPenalty;
					/* Ignore color error since one color will span the entire
					 * deletion.  We will consider the color at the end of the deletion.
					 * */
					/* Make sure we aren't below infinity */
					if(curScore < NEGATIVE_INFINITY/2) {
						curScore = NEGATIVE_INFINITY;
						curScoreNT = NEGATIVE_INFINITY;
					}
					if(curScore > maxScore) {
						maxScore = curScore;
						maxScoreNT = curScoreNT;
						maxFrom = k + 1; /* see the enum */ 
						maxColorError = '0';
						maxLength = curLength;
					}
					/* Update */
					matrix[i+1][j+1].h.score[k] = maxScore;
					matrix[i+1][j+1].h.scoreNT[k] = maxScoreNT;
					matrix[i+1][j+1].h.from[k] = maxFrom;
					matrix[i+1][j+1].h.colorError[k] = maxColorError;
					matrix[i+1][j+1].h.length[k] = maxLength;
				}
			}

			/* Match/Mismatch */
			for(k=0;k<ALPHABET_SIZE+1;k++) { /* To NT */
				int32_t maxScore = NEGATIVE_INFINITY-1;
				int32_t maxScoreNT = NEGATIVE_INFINITY-1;
				int maxFrom = -1;
				char maxColorError = '0';
				int maxLength = 0;

				for(l=0;l<ALPHABET_SIZE+1;l++) { /* From NT */
					int32_t curScore=NEGATIVE_INFINITY;
					int32_t curScoreNT=NEGATIVE_INFINITY;
					int curLength=-1;
					uint8_t convertedColor='X';
					int32_t scoreNT, scoreColor;

					/* Get color */
					if(0 == ConvertBaseToColorSpace(DNA[l], DNA[k], &convertedColor)) {
						fprintf(stderr, "DNA[l=%d]=%c\tDNA[k=%d]=%c\n",
								l,
								DNA[l],
								k,
								DNA[k]);
						PrintError(FnName,
								"convertedColor",
								"Could not convert base to color space",
								Exit,
								OutOfRange);
					}
					/* Get NT and Color scores */
					scoreNT = ScoringMatrixGetNTScore(reference[j], DNA[k], sm);
					scoreColor = ScoringMatrixGetColorScore(curColor,
							convertedColor,
							sm);

					/* From Horizontal - Deletion */
					curLength = matrix[i][j].h.length[l] + 1;
					/* Add previous with current NT */
					curScore = curScoreNT = matrix[i][j].h.score[l] + scoreNT;
					/* Add score for color error, if any */
					curScore += scoreColor;
					/* Make sure we aren't below infinity */
					if(curScore < NEGATIVE_INFINITY/2) {
						curScore = NEGATIVE_INFINITY;
						curScoreNT = NEGATIVE_INFINITY;
					}
					if(curScore > maxScore) {
						maxScore = curScore;
						maxScoreNT = curScoreNT;
						maxFrom = l + 1; /* see the enum */ 
						maxColorError = (curColor == convertedColor)?'0':'1';
						maxLength = curLength;
					}

					/* From Vertical - Insertion */
					curLength = matrix[i][j].v.length[l] + 1;
					/* Add previous with current NT */
					curScore = curScoreNT = matrix[i][j].v.score[l] + scoreNT;
					/* Add score for color error, if any */
					curScore += scoreColor;
					/* Make sure we aren't below infinity */
					if(curScore < NEGATIVE_INFINITY/2) {
						curScore = NEGATIVE_INFINITY;
						curScoreNT = NEGATIVE_INFINITY;
					}
					if(curScore > maxScore) {
						maxScore = curScore;
						maxScoreNT = curScoreNT;
						maxFrom = l + 1 + 2*(ALPHABET_SIZE + 1); /* see the enum */ 
						maxColorError = (curColor == convertedColor)?'0':'1';
						maxLength = curLength;
					}

					/* From Diagonal - Match/Mismatch */
					curLength = matrix[i][j].s.length[l] + 1;
					/* Add previous with current NT */
					curScore = curScoreNT = matrix[i][j].s.score[l] + scoreNT;
					/* Add score for color error, if any */
					curScore += scoreColor;
					/* Make sure we aren't below infinity */
					if(curScore < NEGATIVE_INFINITY/2) {
						curScore = NEGATIVE_INFINITY;
						curScoreNT = NEGATIVE_INFINITY;
					}
					if(curScore > maxScore) {
						maxScore = curScore;
						maxScoreNT = curScoreNT;
						maxFrom = l + 1 + (ALPHABET_SIZE + 1); /* see the enum */ 
						maxColorError = (curColor == convertedColor)?'0':'1';
						maxLength = curLength;
					}
				}
				/* Update */
				matrix[i+1][j+1].s.score[k] = maxScore;
				matrix[i+1][j+1].s.scoreNT[k] = maxScoreNT;
				matrix[i+1][j+1].s.from[k] = maxFrom;
				matrix[i+1][j+1].s.colorError[k] = maxColorError;
				matrix[i+1][j+1].s.length[k] = maxLength;
			}

			/* Insertion */
			if(j == referenceLength - toExclude + i - 1) {
				/* We are on the boundary, do not consider an insertion */
				for(k=0;k<ALPHABET_SIZE+1;k++) { /* To NT */
					/* Update */
					matrix[i+1][j+1].v.score[k] = NEGATIVE_INFINITY-1;
					matrix[i+1][j+1].v.scoreNT[k] = NEGATIVE_INFINITY-1;
					matrix[i+1][j+1].v.from[k] = NoFrom ;
					matrix[i+1][j+1].v.colorError[k] = '0';
					matrix[i+1][j+1].v.length[k] = INT_MIN;
				}
			}
			else {
				for(k=0;k<ALPHABET_SIZE+1;k++) { /* To NT */
					int32_t maxScore = NEGATIVE_INFINITY-1;
					int32_t maxScoreNT = NEGATIVE_INFINITY-1;
					int maxFrom = -1;
					char maxColorError = '0';
					int maxLength = 0;

					int32_t curScore=NEGATIVE_INFINITY;
					int32_t curScoreNT=NEGATIVE_INFINITY;
					int curLength=-1;
					uint8_t B;
					int fromNT=-1;

					/* Get from base for extending an insertion */
					if(0 == ConvertBaseAndColor(DNA[k], curColor, &B)) {
						PrintError(FnName,
								NULL,
								"Could not convert base and color",
								Exit,
								OutOfRange);
					}
					switch(B) {
						case 'a':
						case 'A':
							fromNT=0;
							break;
						case 'c':
						case 'C':
							fromNT=1;
							break;
						case 'g':
						case 'G':
							fromNT=2;
							break;
						case 't':
						case 'T':
							fromNT=3;
							break;
						default:
							fromNT=4;
							break;
					}

					/* New insertion */
					curScore=NEGATIVE_INFINITY;
					curScoreNT=NEGATIVE_INFINITY;
					curLength=-1;
					/* Get NT and Color scores */
					curLength = matrix[i][j+1].s.length[fromNT] + 1;
					curScore = curScoreNT = matrix[i][j+1].s.score[fromNT] + sm->gapOpenPenalty;
					/*
					   curScore += ScoringMatrixGetColorScore(curColor,
					   convertedColor,
					   sm);
					   */
					/* Make sure we aren't below infinity */
					if(curScore < NEGATIVE_INFINITY/2) {
						curScore = NEGATIVE_INFINITY;
						curScoreNT = NEGATIVE_INFINITY;
					}
					if(curScore > maxScore) {
						maxScore = curScore;
						maxScoreNT = curScoreNT;
						maxFrom = fromNT + 1 + (ALPHABET_SIZE + 1); /* see the enum */ 
						maxColorError = '0';
						maxLength = curLength;
					}

					/* Extend current insertion */
					curLength = matrix[i][j+1].v.length[fromNT] + 1;
					/* Insertion - previous row */
					curScore = curScoreNT = matrix[i][j+1].v.score[fromNT] + sm->gapExtensionPenalty;
					/* Make sure we aren't below infinity */
					if(curScore < NEGATIVE_INFINITY/2) {
						curScore = NEGATIVE_INFINITY;
						curScoreNT = NEGATIVE_INFINITY;
					}
					if(curScore > maxScore) {
						maxScore = curScore;
						maxScoreNT = curScoreNT;
						maxFrom = fromNT + 1 + 2*(ALPHABET_SIZE + 1); /* see the enum */ 
						maxColorError = '0';
						maxLength = curLength;
					}

					/* Update */
					matrix[i+1][j+1].v.score[k] = maxScore;
					matrix[i+1][j+1].v.scoreNT[k] = maxScoreNT;
					matrix[i+1][j+1].v.from[k] = maxFrom;
					matrix[i+1][j+1].v.colorError[k] = maxColorError;
					matrix[i+1][j+1].v.length[k] = maxLength;
				}
			}
		}
	}

	offset = FillAlignEntryFromMatrixColorSpace(a,
			matrix,
			read,
			readLength,
			reference,
			referenceLength,
			scoringType,
			toExclude,
			0);

	/* Free the matrix, free your mind */
	for(i=0;i<readLength+1;i++) {
		free(matrix[i]);
		matrix[i]=NULL;
	}
	free(matrix);
	matrix=NULL;

	AlignEntry tmp;
	AlignEntryInitialize(&tmp);
	int tmpOffset = AlignColorSpaceFull(read,
			readLength,
			reference,
			referenceLength,
			scoringType,
			sm,
			&tmp,
			strand);
	assert(offset == tmpOffset);
	assert(a->length == tmp.length);
	assert(a->referenceLength == tmp.referenceLength);
	assert(0 == strcmp(a->read, tmp.read));
	assert(0 == strcmp(a->reference, tmp.reference));
	assert(0 == strcmp(a->colorError, tmp.colorError));
	AlignEntryFree(&tmp);
	/* The return is the number of gaps at the beginning of the reference */
	return offset;
}
