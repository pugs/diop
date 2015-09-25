// Copyright (C) DriveScale, Inc. 2014-2015 - All Rights Reserved.

#include <stdio.h>
#include "diop.h"

int main(int argc, char **argv)
{
	struct diop_args args;
	struct diop_args *a = &args;
	struct diop_results res;
	double outer, inner, avg;

	diop_defaults(a);
	a->device = argv[1];
// 1GB or 10 seconds, whichever is shortest
	a->blksize = 1024*1024;
	a->niter = 1024;
	a->duration = 10;

	res = diop(a);
	outer = diop_bandwidth(a, &res);

	a->startblk = -1024;
	res = diop(a);
	inner = diop_bandwidth(a, &res);

	avg = (outer + inner) / 2;

	printf("Bandwidth: Outer %.2e Inner %.2e Average %.2e\n", outer, inner, avg);
	return(0);
}
	

