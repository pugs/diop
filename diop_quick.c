// Copyright (C) DriveScale, Inc. 2014-2015 - All Rights Reserved.

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "diop.h"
#include <json/json_object.h>

int duration = 10;

void
Usage()
{
	fprintf(stderr, "Usage: diop_quick [-t test_time] <blockdev>.\n");
	exit(1);
}

int main(int argc, char **argv)
{
	struct diop_args args;
	struct diop_args *a = &args;
	struct diop_results res;
	double outer, inner, avg;
	double avglat;
	int iops;
	struct json_object *j;
	const char *jout;
	char *id;

	while (argc > 2 && argv[1][0] == '-') {
		switch(argv[1][1]) {
		case 't':
			duration = atoi(argv[2]);
			argc -= 2;
			argv += 2;
			break;
		default:
			Usage();
		}
	}
	if (argc != 2)
		Usage();
	diop_defaults(a);
	a->device = argv[1];
// 10GB or <duration> seconds, whichever is shortest
	a->blksize = 1024*1024;
	a->niter = 10240;
	a->duration = duration;

	res = diop(a);
	outer = diop_bandwidth(a, &res);

	a->startblk = -10240;
	res = diop(a);
	inner = diop_bandwidth(a, &res);

	avg = (outer + inner) / 2;

	// convert Bps to bps
	avg *= 8;
	outer *= 8;
	inner *= 8;

	diop_defaults(a);
	a->device = argv[1];
	a->blksize = 4096;
	a->niter = 1000000000;
	a->duration = duration;
	a->incr = RAND_INCR;
	res = diop(a);
	avglat = diop_latency(&res);
	res = diop_parallel(32, a);
	iops = diop_iops(&res);


	j = json_object_new_object();
	id = diop_drive_id(a->device, 0x80);
	if (id)
		json_object_object_add(j, "id_p80", json_object_new_string(id));
	id = diop_drive_id(a->device, 0x83);
	if (id)
		json_object_object_add(j, "id_p83", json_object_new_string(id));
	json_object_object_add(j, "bitrate_hi", json_object_new_double(outer));
	json_object_object_add(j, "bitrate_lo", json_object_new_double(inner));
	json_object_object_add(j, "bitrate_avg", json_object_new_double(avg));
	json_object_object_add(j, "latency_avg_us", json_object_new_double(avglat));
	json_object_object_add(j, "iops", json_object_new_int(iops));
	jout = json_object_to_json_string_ext(j, JSON_C_TO_STRING_PRETTY);
	write(1, jout, strlen(jout));
	write(1, "\n", 1);
	return(0);
}
	

