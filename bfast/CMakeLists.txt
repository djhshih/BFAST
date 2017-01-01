
add_executable(
	bfast
	AlignedEnd.c
	AlignedEntry.c 
	AlignedRead.c 
	AlignedReadConvert.c 
	BError.c 
	BLib.c 
	BLibDefinitions.h 
	RGBinary.c RGBinary.h 
	RGIndex.c RGIndex.h 
	RGIndexAccuracy.c RGIndexAccuracy.h 
	RGIndexExons.c RGIndexExons.h 
	RGIndexLayout.c RGIndexLayout.h 
	RGMatch.c RGMatch.h 
	RGMatches.c RGMatches.h 
	RGRanges.c RGRanges.h 
	RGReads.c RGReads.h 
	ScoringMatrix.c 
	Align.c 
	AlignNTSpace.c 
	AlignColorSpace.c 
	AlignMatrix.c 
	MatchesReadInputFiles.c  
	RunMatch.c 
	RunLocalAlign.c 
	RunPostProcess.c 
	RunAlign.c 
	BfastFasta2BRG.c 
	BfastIndex.c 
	BfastMatch.c 
	BfastLocalAlign.c 
	BfastPostProcess.c 
	BfastBAFConvert.c 
	BfastHeader.c 
	BfastBMFConvert.c 
	BfastBRG2Fasta.c 
	BfastAlign.c 
	aflib.c 
	Main.c 
)

target_link_libraries(bfast ${BZIP2_LIBRARIES} ${ZLIB_LIBRARIES})

