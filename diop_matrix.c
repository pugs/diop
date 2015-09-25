// Copyright (C) DriveScale, Inc. 2014-2015 - All Rights Reserved.

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <json/json_object.h>

#include "diop.h"


#define	TEST_TYPES	6
#define	TEST_BW_OUTER	0
#define	TEST_BW_INNER	1
#define	TEST_IOPS	2
#define	TEST_LAT_LO	3
#define	TEST_LAT_HI	4
#define	TEST_LAT_AV	5
char *tests[TEST_TYPES] = {
	"BW-Outer ",
	"BW-Inner ",
	"IOPS     ",
	"Latency-Low",
	"Latency-High",
	"Latency-Avg",
};
char *test_units[TEST_TYPES] = {
	"(MBps)",
	"(MBps)",
	"    ",
	"(us)",
	"(us)",
	"(us)",
};

#define	TEST_MAX_DURATION	20
#define	TEST_BW_COUNT	1024

#define	STREAM_TYPES	2
#define	STREAM_SINGLE	0
#define	STREAM_MULTI	1
char *streams[STREAM_TYPES] = {
	"single",
	"multi",
};

#define	IO_TYPES	3
#define	IO_READ		0
#define	IO_WRITE	1
#define	IO_WSYNC	2
char *ios[IO_TYPES] = {
	"read",
	"write",
	"w-sync",
};

#define	SIZE_TYPES	5
int sizes[SIZE_TYPES] = { 4096, 4*4096, 16*4096, 64*4096, 256*4096 };
char *size_pretty[SIZE_TYPES] = { "4K", "16K", "64K", "256K", "1M" };

double matrix[TEST_TYPES][STREAM_TYPES][IO_TYPES][SIZE_TYPES];

void usage()
{
	printf("Usage: matrix [-W] device\n");
	exit(1);
}

int main(int argc, char **argv)
{
	struct diop_args args, *a = &args, tmpa;
	struct diop_results result, *r = &result;
	int test, stream, io, size, nproc;
	int count;
	int writeallow = 0;
	int i;
	char filename[30];
	FILE *fout;

	if (argc == 3) {
		if (strcmp(argv[1], "-W") == 0)
			writeallow = 1;
		else
			usage();
		argc--;
		argv++;
	}
	if (argc != 2)
		usage();
	for (test=0; test<TEST_TYPES; test++) {
		diop_defaults(a);
		a->device = argv[1];
		a->duration = TEST_MAX_DURATION;
		a->niter = 1000000;
		sleep(2);	/* pause between test types */
		switch(test) {
		case TEST_IOPS:
			a->incr = RAND_INCR;
			break;
		case TEST_LAT_LO:
		case TEST_LAT_HI:
			continue;	/* skip these tests, covere in next */
		case TEST_LAT_AV:
			a->incr = RAND_INCR;
			break;
		}
		for (stream=0; stream<STREAM_TYPES; stream++) {
			switch (stream) {
			case STREAM_SINGLE:
				nproc = 1;
				break;
			case STREAM_MULTI:
				nproc = 32;
				break;
			}
			for (io=0; io<IO_TYPES; io++) {
				a->sync = 0;
				switch(io) {
				case IO_READ:
					a->rdwr = 0;
					break;
				case IO_WRITE:
					if (!writeallow)
						continue;
					a->rdwr = 1;
					break;
				case IO_WSYNC:
					if (!writeallow)
						continue;
					a->rdwr = 1;
					a->sync = 1;
					break;
				}
				for (size=0; size<SIZE_TYPES; size++) {
					a->blksize = sizes[size];
					count = (1 << 30) / a->blksize; // 1GB
					switch(test) {
					case TEST_BW_OUTER:
						a->niter = count;
						break;
					case TEST_BW_INNER:
						a->niter = count;
						a->startblk = -count;
						break;
					}
					printf("%s %s %s size %lld...\t", tests[test],
						streams[stream], ios[io], a->blksize);
					fflush(stdout);
					tmpa = args;
					result = diop_parallel(nproc, &tmpa);
					switch(test) {
					case TEST_BW_OUTER:
					case TEST_BW_INNER:
						matrix[test][stream][io][size] = diop_bandwidth(a, r);
						break;
					case TEST_IOPS:
						matrix[test][stream][io][size] = diop_iops(r);
						break;
					case TEST_LAT_AV:
						matrix[test][stream][io][size] = diop_latency(r);
						matrix[TEST_LAT_LO][stream][io][size] = diop_latency_lo(r);
						matrix[TEST_LAT_HI][stream][io][size] = diop_latency_hi(r);
						break;
					}
					printf("%.3e\n", matrix[test][stream][io][size]);
					fflush(stdout);
				}
			}
		}
	}

	struct json_object *j_tests, *test_res;
	struct json_object *s_tests, *s_res;
	struct json_object *io_tests, *io_res;
	struct json_object *blk_tests, *blk_res;
	const char *jout;

	j_tests = json_object_new_array();
	for (test=0; test<TEST_TYPES; test++) {
		s_tests = json_object_new_array();
		for (stream=0; stream<STREAM_TYPES; stream++) {
			io_tests = json_object_new_array();
			for (io=0; io<IO_TYPES; io++) {
				if (!writeallow && io != IO_READ)
					continue;
				blk_tests = json_object_new_array();
				for (size=0; size<SIZE_TYPES; size++) {
					blk_res = json_object_new_object();
					json_object_object_add(blk_res, "blksize", json_object_new_int64(sizes[size]));
					json_object_object_add(blk_res, "result",
						json_object_new_double(matrix[test][stream][io][size]));
					json_object_array_add(blk_tests, blk_res);
				}
				io_res = json_object_new_object();
				json_object_object_add(io_res, "io", json_object_new_string(ios[io]));
				json_object_object_add(io_res, "result", blk_tests);
				json_object_array_add(io_tests, io_res);
			}
			s_res = json_object_new_object();
			json_object_object_add(s_res, "streams", json_object_new_string(streams[stream]));
			json_object_object_add(s_res, "result", io_tests);
			json_object_array_add(s_tests, s_res);
		}
		test_res = json_object_new_object();
		json_object_object_add(test_res, "test", json_object_new_string(tests[test]));
		json_object_object_add(test_res, "result", s_tests);
		json_object_array_add(j_tests, test_res);
	}
	jout = json_object_to_json_string_ext(j_tests, JSON_C_TO_STRING_PRETTY);
	sprintf(filename, "perf.%s.json", basename(a->device));
	fout = fopen(filename, "w");
	if (fout) 
		fputs(jout, fout);
	else
		fprintf(stderr, "Cannot write %s\n", filename);

	sprintf(filename, "perf.%s.mat", basename(a->device));
	fout = fopen(filename, "w");
	if (fout == NULL) {
		fprintf(stderr, "Cannot write %s\n", filename);
		exit(1);
	};
	fprintf(fout, "\nTest\t\tUnit\tStrs\tio   |");
	for (size=0; size<SIZE_TYPES; size++)
		fprintf(fout, "\t%s", size_pretty[size]);
	fprintf(fout, "\n");
	for (i=0; i<10; i++) fprintf(fout, "--------");
	fprintf(fout, "\n");
	for (test=0; test<TEST_TYPES; test++) {
	for (stream=0; stream<STREAM_TYPES; stream++) {
	for (io=0; io<IO_TYPES; io++) {
		if (!writeallow && io != IO_READ)
			continue;
		fprintf(fout, "%s\t%s\t%s\t%s", tests[test], test_units[test], streams[stream], ios[io]);
		for (size=0; size<SIZE_TYPES; size++) {
			int ival;

			switch(test) {
			case TEST_BW_OUTER:
			case TEST_BW_INNER:
				ival = matrix[test][stream][io][size] / 1000000;
				break;
			default:
				ival = matrix[test][stream][io][size];
			}
			fprintf(fout, "\t%d", ival);
		}
		fprintf(fout, "\n");
	}}}
	return 0;
}
