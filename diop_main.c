// Copyright (C) DriveScale, Inc. 2014-2015 - All Rights Reserved.

#define	_LARGEFILE64_SOURCE
#define	_FILE_OFFSET_BITS	64

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#define __USE_GNU 1
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/fs.h>

#include "diop.h"

extern ssize_t pread (int __fd, void *__buf, size_t __nbytes, __off64_t __offset);
extern ssize_t pwrite (int __fd, __const void *__buf, size_t __nbytes, __off64_t __offset);

void usage()
{
	fprintf(stderr, "Usage: diop [-b blksize] [-s start] [-S range] [-d duration] [-w] [-i] [-r] [-p nproc] nIter Dev\n");
	exit(1);
}

int64 getarg(char *str)
{
	int64 foo = 0;
	char *s = str;
	int64 unit = 1;
	int last = 0;
	unsigned char c;

	if (s == NULL || *s == 0)
		goto usage;
	if (*s == '-') {
		unit = -1;
		s++;
	}
	while ((c = *s++)) {
		if (last)
			goto usage;
		switch(c) {
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			foo = foo*10 + (c - '0');
			break;
		case 'k': case 'K':
			unit *= 1LL << 10;
			last = 1;
			break;
		case 'm': case 'M':
			unit *= 1LL << 20;
			last = 1;
			break;
		case 'g': case 'G':
			unit *= 1LL << 30;
			last = 1;
			break;
		case 't': case 'T':
			unit *= 1LL << 40;
			last = 1;
			break;
		default:
			goto usage;
		}
	}
	return (foo * unit);
usage:
	fprintf(stderr, "Unknown arg '%s'\n", str);
	fprintf(stderr, "Use <int>[kmgt]\n");
	exit(1);
}
	

int main(int argc, char **argv)
{
	double mean, var, stddev;
	int64 n;
	double Bps;
	int iops;
	struct diop_args args;
	struct diop_args *a = &args;
	struct diop_results totals;
	int nproc;
	
	// set defaults
	diop_defaults(a);
	nproc = 1;

	while (argc > 1 && argv[1][0] == '-') {
		switch(argv[1][1]) {
		case 'b':
			a->blksize = getarg(argv[2]);
			argv += 2; argc -= 2;
			break;
		case 'd':
			a->duration = strtol(argv[2], NULL, 0);
			argv += 2; argc -= 2;
			break;
		case 's':
			if (strcmp(argv[2], "R") == 0)
				a->startblk = RAND_START;
			else
				a->startblk = getarg(argv[2]);
			argv += 2; argc -= 2;
			break;
		case 'S':
			a->range = getarg(argv[2]);
			argv += 2; argc -= 2;
			break;
		case 'w':
			a->rdwr = 1;
			argv += 1; argc -= 1;
			break;
		case 'i':
			a->incr = strtoll(argv[2], NULL, 0);
				argv += 2; argc -= 2;
			break;
		case 'p':
			nproc = strtol(argv[2], NULL, 0);
			argv += 2; argc -= 2;
			break;
		case 'r':
			a->incr = RAND_INCR;
			argv += 1; argc -= 1;
			break;
		case 'z':
			a->sync = 1;
			argv += 1; argc -= 1;
			break;
		case 'D':
			a->direct = 0;
			argv += 1; argc -= 1;
			break;
		default:
			usage();
			exit(1);
		}
	}
	if (argc != 3)
		usage();
	a->niter = getarg(argv[1]);
	a->device = argv[2];

	if (nproc <= 0 || a->niter <= 0)
		usage();

	a->verbose = 1;
	totals = diop_parallel(nproc, a);
			
	n = totals.count;
	mean = totals.sum / n;
	var = totals.sumsq - 2*mean*totals.sum + n*mean*mean;
	var = var / n;
	stddev = sqrt(var);
	iops = diop_iops(&totals);
	Bps = diop_bandwidth(a, &totals);
	printf("dev\tIOPS\tBps\t\tSize\tCount\tLow\t\tHigh\t\tAverage\t\tStdDev\t\t%%s/m\n");
	printf("%s\t%d\t%.2e\t%lld\t%lld\t%.2e\t%.2e\t%.2e\t%.2e\t%.1f\n",
		a->device+5,
		iops, Bps,  a->blksize, totals.count, totals.lowest, totals.highest, mean,
		stddev, 100*stddev/mean);
	exit(0);
}
