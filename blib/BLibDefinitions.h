#ifndef BLIBDEFINITIONS_H_
#define BLIBDEFINITIONS_H_

#include <sys/types.h>
#include <stdint.h>

/* Program defaults */
#define PROGRAM_NAME "bfast" /* Could just use PACKAGE_NAME */
#define DEFAULT_FILENAME "Default.txt"
#define MAX_FILENAME_LENGTH 2048
#define DEFAULT_OUTPUT_ID "OutputID"
#define DEFAULT_OUTPUT_DIR "./"
#define BREAK_LINE "************************************************************\n"
#define SEQUENCE_LENGTH 2048
#define SEQUENCE_NAME_LENGTH 4028
#define MAX_MASK_LENGTH 1024
#define MAX_CONTIG_NAME_LENGTH 2048
#define MAX_CONTIG_LOG_10 7 
#define MAX_POSITION_LOG_10 10
#define MAX_HEADER_LENGTH 2048
#define ONE_GIGABYTE (int64_t)1073741824
#define MERGE_MEMORY_LIMIT 12*((int64_t)1073741824) /* In Gigabytes */

/* Testing/Debug */
#define TEST_RGINDEX_SORT 0

/* Default output */
enum {TextOutput, BinaryOutput};
enum {TextInput, BinaryInput};
enum {BRG, BIF, BMF, BAF, MAF, GFF, LastFileType};
#define BPREPROCESS_DEFAULT_OUTPUT 1 /* 0: text 1: binary */
#define BMATCHES_DEFAULT_OUTPUT 1 /* 0: text 1: binary */
#define BALIGN_DEFAULT_OUTPUT 1 /* 0: text 1: binary */

/* File extensions */
#define BFAST_RG_FILE_EXTENSION "brg"
#define BFAST_INDEX_FILE_EXTENSION "bif"
#define BFAST_MATCHES_FILE_EXTENSION "bmf"
#define BFAST_MATCHES_READS_FILTERED_FILE_EXTENSION "fa"
#define BFAST_ALIGNED_FILE_EXTENSION "baf"
#define BFAST_MAF_FILE_EXTENSION "maf"
#define BFAST_GFF_FILE_EXTENSION "gff"

#define RGMATCH_MERGE_ROTATE_NUM 100000
#define READ_ROTATE_NUM 1000000
#define RGINDEX_ROTATE_NUM 1000000
#define SORT_ROTATE_INC 0.01
#define RGINDEX_SORT_ROTATE_INC 0.001
#define ALIGN_ROTATE_NUM 1000
#define PARTITION_MATCHES_ROTATE_NUM 100000
#define ALIGNENTRIES_READ_ROTATE_NUM 10000
#define BFAST_TMP_TEMPLATE ".bfast.tmp.XXXXXX"
#define DEFAULT_RANGE "1-1:2147483647-2147483647"

/* For printing to stderr */
#define VERBOSE 0
#define DEBUG 10

/* Algorithm defaults */
#define DEFAULT_MATCH_LENGTH 11
#define ALPHABET_SIZE 4
#define FORWARD '+'
#define REVERSE '-'
#define GAP '-'
#define GAP_QUALITY -1
#define NULL_LETTER 'N'
#define COLOR_SPACE_START_NT 'A'
#define BFAST_ID 'B'+'F'+'A'+'S'+'T'

/* Macro functions */
#define MIN(_X, _Y)  ((_X) < (_Y) ? (_X) : (_Y))
#define MAX(_X, _Y)  ((_X) < (_Y) ? (_Y) : (_X))
#define QUAL_TO_MAF_QUAL(_X)  (MIN( (int)floor(_X/5), 9))
#define CHAR2QUAL(c) \
	    ((isdigit(c))? ((c)-'0') : ((islower(c))? ((c)-'a'+10) : ((isupper(c))? ((c)-'A'+36) : 0)))
#define QUAL2CHAR(q) \
	    (((q)<10)? ((q)+'0') : (((q)<36)? ((q)-10+'a') : (((q)<62)? ((q)-36+'A') : 'Z')))

enum {KILOBYTES, MEGABYTES, GIGABYTES};
enum {Contig_8, Contig_32};
enum {SingleEnd, PairedEnd, PairedEndDoesNotMatter};
enum {NTSpace, ColorSpace, SpaceDoesNotMatter};
enum {AlignedEntrySortByAll, AlignedEntrySortByContigPos};
enum {IgnoreExons, UseExons};
enum {BothStrands, ForwardStrand, ReverseStrand};
enum {BFASTReferenceGenomeFile, BFASTIndexFile};
enum {RGBinaryPacked, RGBinaryUnPacked};
enum {NoMirroring, MirrorForward, MirrorReverse, MirrorBoth};

/************************************/
/* 		Data structures 			*/
/************************************/

/* TODO */
typedef struct {
	int32_t readLength;
	char *read;
	int32_t qualLength;
	char *qual;
	int32_t maxReached;
	int32_t numEntries;
	uint32_t *contigs;
	int32_t *positions;
	char *strands;
} RGMatch;

/* TODO */
typedef struct {
	int32_t readNameLength;
	char *readName;
	int32_t numEnds;
	RGMatch *ends;
} RGMatches;

/* TODO */
typedef struct { 
	int64_t *startIndex;
	int64_t *endIndex;
	char *strand;
	int32_t *offset;
	int32_t numEntries;
} RGRanges;

/* TODO */
typedef struct {
	int32_t numReads;
	char **reads;
	int32_t *readLength;
	int32_t *offset;
} RGReads;

/* TODO */
typedef struct {
	/* Storage */
	int32_t contigNameLength;
	char *contigName;
	/* Metadata */
	char *sequence; 
	int32_t sequenceLength;
	uint32_t numBytes;
} RGBinaryContig;

/* TODO */
typedef struct {
	/* Storage type */
	int32_t id;
	int32_t packageVersionLength;
	char *packageVersion;
	int32_t packed;
	/* RG storage */
	RGBinaryContig *contigs;
	int32_t numContigs;
	/* Metadata */
	int32_t space;
} RGBinary;

/* TODO */
typedef struct {
	/* Storage type */
	int32_t id;
	int32_t packageVersionLength;
	char *packageVersion;
	/* Index storage */
	uint8_t *contigs_8;
	uint32_t *contigs_32;
	int32_t *positions;
	int64_t length;
	int32_t contigType; 
	/* Index range */
	int32_t startContig;
	int32_t startPos;
	int32_t endContig;
	int32_t endPos;
	/* Index layout */
	int32_t width;
	int32_t keysize;
	int32_t *mask;
	/* Index properties */
	int32_t repeatMasker;
	int32_t space;
	/* Hash storage */
	uint32_t hashWidth; /* in bases */
	int64_t hashLength; 
	uint32_t *starts;
} RGIndex;

/* TODO */
typedef struct {
	int32_t numIndexes;
	int32_t *hashWidths;
	int32_t **masks;
	int32_t *widths;
	int32_t *keysizes;
} RGIndexLayout;

/* TODO */
typedef struct {
	uint32_t startContig;
	uint32_t startPos;
	uint32_t endContig;
	uint32_t endPos;
} RGIndexExon;

/* TODO */
typedef struct {
	int numExons;
	RGIndexExon *exons;
} RGIndexExons;

/* TODO */
typedef struct {
	int32_t contigNameLength;
	char *contigName;
	uint32_t contig;
	uint32_t position;
	char strand;
	double score;
	uint32_t referenceLength; /* The length of the reference alignment substracting gaps */
	uint32_t length; /* The length of the alignment */
	char *read; /* The read */
	char *reference;
	char *colorError;
} AlignedEntry;

/* TODO */
typedef struct {
	int32_t readLength;
	char *read; /* Original read */
	int32_t qualLength;
	char *qual; /* Original quality */
	int32_t numEntries;
	AlignedEntry *entries;
} AlignedEnd;

/* TODO */
typedef struct {
	int32_t readNameLength;
	char *readName;
	int32_t space;
	int32_t numEnds;
	AlignedEnd *ends;
} AlignedRead;

/* TODO */
typedef struct {
	RGIndex *index;
	RGBinary *rg;
	int32_t space;
	int64_t low;
	int64_t high;
	int32_t threadID;
	int32_t showPercentComplete;
	char *tmpDir;
	int64_t mergeMemoryLimit;
} ThreadRGIndexSortData;

/* TODO */
typedef struct {
	RGIndex *index;
	RGBinary *rg;
	int32_t threadID;
	int64_t low;
	int64_t mid;
	int64_t high;
	int64_t mergeMemoryLimit;
	char *tmpDir;
} ThreadRGIndexMergeData;

/* TODO */
typedef struct {
	int32_t contigStart;
	int32_t contigEnd;
	int32_t positionStart;
	int32_t positionEnd;
} Range;

#endif
