add_executable(
	solid2fastq
	../bfast/BError.c
	../bfast/RGIndex.c
	../bfast/BLib.c
	../bfast/RGBinary.c
	../bfast/RGRanges.c
	../bfast/RGMatch.c
	../bfast/aflib.c
	solid2fastq.c
)

target_link_libraries(solid2fastq ${STANDARD_LIBRARIES} ${BZIP2_LIBRARIES} ${ZLIB_LIBRARIES})

install(TARGETS solid2fastq RUNTIME DESTINATION bin)

install(PROGRAMS bfast.resubmit.pl bfast.submit.pl ill2fastq.pl
	DESTINATION bin
)
