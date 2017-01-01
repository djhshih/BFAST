add_executable(
	balignmentscoredistribution
	../bfast/BError.c
	../bfast/RGIndex.c
	../bfast/BLib.c
	../bfast/RGBinary.c
	../bfast/RGRanges.c
	../bfast/RGMatch.c
	../bfast/AlignedRead.c
	../bfast/AlignedReadConvert.c
	../bfast/AlignedEnd.c
	../bfast/AlignedEntry.c
	balignmentscoredistribution.c
)

target_link_libraries(balignmentscoredistribution ${STANDARD_LIBRARIES} ${ZLIB_LIBRARIES})

add_executable(
	balignsim
	SimRead.c
	../bfast/BError.c
	../bfast/RGIndexExons.c
	../bfast/RGIndex.c
	../bfast/BLib.c
	../bfast/RGBinary.c
	../bfast/RGMatch.c
	../bfast/RGMatches.c
	../bfast/RGRanges.c
	../bfast/AlignedRead.c
	../bfast/AlignedEnd.c
	../bfast/RGIndexAccuracy.c
	../bfast/AlignedEntry.c
	../bfast/RunLocalAlign.c
	../bfast/ScoringMatrix.c
	../bfast/Align.c
	../bfast/AlignColorSpace.c
	../bfast/AlignNTSpace.c
	../bfast/AlignMatrix.c
	balignsim.c
)

target_link_libraries(balignsim ${STANDARD_LIBRARIES} ${ZLIB_LIBRARIES})

add_executable(
	bevalsim
	../bfast/BError.c
	../bfast/RGIndexExons.c
	../bfast/RGIndex.c
	../bfast/BLib.c
	../bfast/RGBinary.c
	../bfast/RGRanges.c
	../bfast/RGMatches.c
	../bfast/RGMatch.c
	../bfast/AlignedRead.c
	../bfast/AlignedEnd.c
	../bfast/AlignedEntry.c
	bevalsim.c
)

target_link_libraries(bevalsim ${STANDARD_LIBRARIES} ${ZLIB_LIBRARIES})

add_executable(
	bgeneratereads
	../bfast/BError.c
	../bfast/RGIndexExons.c
	../bfast/RGIndex.c
	../bfast/BLib.c
	../bfast/RGBinary.c
	../bfast/RGMatch.c
	../bfast/RGRanges.c
	SimRead.c
	bgeneratereads.c
)

target_link_libraries(bgeneratereads ${STANDARD_LIBRARIES} ${ZLIB_LIBRARIES})

add_executable(
	bindexdist
	../bfast/BError.c
	../bfast/RGIndexExons.c
	../bfast/RGIndex.c
	../bfast/BLib.c
	../bfast/RGBinary.c
	../bfast/RGMatch.c
	../bfast/RGRanges.c
	../bfast/RGReads.c
	bindexdist.c
)

target_link_libraries(bindexdist ${STANDARD_LIBRARIES} ${ZLIB_LIBRARIES})

add_executable(
	bindexhist
	../bfast/BError.c
	../bfast/RGIndexExons.c
	../bfast/RGIndex.c
	../bfast/BLib.c
	../bfast/RGBinary.c
	../bfast/RGMatch.c
	../bfast/RGRanges.c
	../bfast/RGReads.c
	bindexhist.c
)

target_link_libraries(bindexhist ${STANDARD_LIBRARIES} ${ZLIB_LIBRARIES})
	

add_executable(
	bmfmerge
	../bfast/BError.c
	../bfast/RGIndexExons.c
	../bfast/RGIndex.c
	../bfast/BLib.c
	../bfast/RGBinary.c
	../bfast/RGRanges.c
	../bfast/RGReads.c
	../bfast/RGMatch.c
	../bfast/RGMatches.c
	../bfast/MatchesReadInputFiles.c
	../bfast/aflib.c
	bmfmerge.c
)

target_link_libraries(bmfmerge ${STANDARD_LIBRARIES} ${BZIP2_LIBRARIES} ${ZLIB_LIBRARIES})


add_executable(
	brepeat
	../bfast/BError.c
	../bfast/RGIndexExons.c
	../bfast/RGIndex.c
	../bfast/BLib.c
	../bfast/RGBinary.c
	../bfast/RGMatch.c
	../bfast/RGRanges.c
	brepeat.c
)

target_link_libraries(brepeat ${STANDARD_LIBRARIES} ${ZLIB_LIBRARIES})


add_executable(
	btestindexes
	../bfast/RGMatch.c
	../bfast/RGRanges.c
	../bfast/RGBinary.c
	../bfast/RGIndex.c
	../bfast/RGIndexAccuracy.c
	../bfast/BError.c
	../bfast/BLib.c
	btestindexes.c
)

target_link_libraries(btestindexes ${STANDARD_LIBRARIES} ${ZLIB_LIBRARIES})

