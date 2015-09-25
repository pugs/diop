// Copyright (C) DriveScale, Inc. 2014-2015 - All Rights Reserved.

#include <stdint.h>
#include <string.h>

#define int64 long long
struct diop_args {
	char		*device;
	int64		blksize;	/* any multiple of 512 , default 4096 */
	int64		startblk;	/* stasrting block, negative for from end of disk */
	int64		incr;		/* 0 for inplace, RANDINCR for random, neg OK, default 1 */
	int64		niter;		/* # of test iterations */
	int64		range;		/* 0 for whole device */
	int		nproc;		/* # of simul processes */
	int		rdwr;		/* 0 for read, 1 for write */
	int		duration;	/* max duration in seconds */
	int		sync;		/* use O_SYNC on open */
	int		direct;		/* use O_DIRECT (default) */
	int		verbose;
};
#define	RAND_INCR	0x7FFFFFFFFFFFFFFFLL
#define	RAND_START	0x7FFFFFFFFFFFFFFFLL

static inline void diop_defaults(struct diop_args *a)
{
	memset((void *)a, 0, sizeof *a);
	a->blksize = 4096;
	a->incr = 1;
	a->direct = 1;
	a->nproc = 1;
}

struct diop_results {
	int64	count;
	double	elapsed;	/* real time - usec */
	double	lowest;		/* fastest */
	double	highest;	/* slowest */
	double	sum;		/* sum of all times */
	double	sumsq;		/* sum of squares */
};

struct diop_results diop(struct diop_args *a);
struct diop_results diop_parallel(int nproc, struct diop_args *a);
char *diop_drive_id(char *blkdev, int page);

/* lowest latency in microseconds */
static inline int diop_latency_lo(struct diop_results *r)
{
	return r->lowest;
}

/* highest latency in microseconds */
static inline int diop_latency_hi(struct diop_results *r)
{
	return r->highest;
}

/* average latency in microseconds */
static inline int diop_latency(struct diop_results *r)
{
	if (r->count == 0)
		return 0;
	return (int) (r->sum / r->count);
}

/* average iops - should use diop_parallel */
static inline int diop_iops(struct diop_results *r)
{
	return (int) (r->count * 1000000. / r->elapsed);
}

static inline double diop_bandwidth(struct diop_args *a, struct diop_results *r)
{
	return a->blksize * r->count * 1000000. / r->elapsed;
}
