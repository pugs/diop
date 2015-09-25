// Copyright (C) DriveScale, Inc. 2014-2015 - All Rights Reserved.

#define	_LARGEFILE64_SOURCE
#define	_FILE_OFFSET_BITS	64

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#define __USE_GNU 1
#include <fcntl.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <linux/fs.h>

#include "diop.h"

void diop_show(struct diop_args *a);

static int stopnow;

static void alarm_handler(int foo)
{
	stopnow = 1;
}

struct diop_results diop(struct diop_args *a)
{
	int64 i;
	int64 blk;
	char *buf;
	int fd;
	struct timeval tv0, tv1, tv2;
	int64 usec;
	double us;
	static struct diop_results res_zero = { 0 };
	struct diop_results results = res_zero;
	int flags;

	if (a->range == 0) {
		fd = open(a->device, O_RDONLY);
		if (fd < 0) {
			perror(a->device);
			exit(1);
		}
		if (ioctl(fd, BLKGETSIZE64, &a->range) < 0) {
			fprintf(stderr, "Cannot get size of %s: use -S to specify\n", a->device);
			exit(1);
		}
		a->range /= a->blksize;
		close(fd);
	}
	if (a->startblk < 0)
		a->startblk = a->range + a->startblk;

	buf = malloc(2 * a->blksize);
	/* pointers and longs are same size */
	buf = (char *)(((long)buf + a->blksize) & ~(a->blksize - 1));
	if (buf == NULL) {
		fprintf(stderr, "diop: cannot alloc buf\n");
		exit(1);
	}
	flags = 0;
	if (a->direct)
		flags |= O_DIRECT;
	if (a->sync)
		flags |= O_SYNC;
	if (a->rdwr)
		flags |= O_WRONLY;
	else
		flags |= O_RDONLY;
	fd = open(a->device, flags);
	if (fd < 0) {
		perror(a->device);
		exit(1);
	}

	srand48(getpid());
	if (a->incr == RAND_INCR)
		a->startblk = 0;
	if (a->startblk == RAND_START) {
		int64 numblk = a->incr * a->niter;
		int64 last;

		a->startblk = lrand48() % a->range;
		last = a->startblk + numblk;
		if (last >= a->range)
			a->startblk = a->range - numblk;
		else if (last < 0)
			a->startblk = numblk;
	}
	if (a->incr == RAND_INCR)
		blk = a->startblk + (lrand48() % a->range);
	else
		blk = a->startblk;

	if (a->verbose)
		diop_show(a);

	stopnow = 0;
	if (a->duration) {
		signal(SIGALRM, alarm_handler);
		alarm(a->duration);
	}
	gettimeofday(&tv0, NULL);
	for (i=0; i<a->niter; i++) {
		gettimeofday(&tv1, NULL);
		if (lseek64(fd, blk*a->blksize, SEEK_SET) < 0) {
			fprintf(stderr, "Blk %lld of %lld, size %lld: ", blk, a->range, a->blksize);
			perror("lseek64");
			exit(1);
		}
		if (a->rdwr) {
			if (write(fd, buf, (size_t)a->blksize) != a->blksize) {
				fprintf(stderr, "block %lld: ", blk);
				perror("write");
				goto next;
			}
		} else {
			if (read(fd, buf, (size_t)a->blksize) != a->blksize) {
				fprintf(stderr, "block %lld: ", blk);
				perror("read");
				goto next;
			}
		}
		if (stopnow)
			break;
		gettimeofday(&tv2, NULL);
		usec = tv2.tv_usec - tv1.tv_usec;
		usec += 1000000 * (tv2.tv_sec - tv1.tv_sec);
		if (usec==0) usec=1;
		us = usec;
		if (us < results.lowest || results.count == 1)
			results.lowest = us;
		if (us > results.highest)
			results.highest = us;
		results.sum += us;
		results.sumsq += us*us;
		results.count++;
		
	next:
		if (a->incr == RAND_INCR)
			blk = lrand48() % a->range;
		else 
			blk = (blk + a->incr) % a->range;
	}
	results.elapsed = tv2.tv_usec - tv0.tv_usec;
	results.elapsed += 1000000. * (tv2.tv_sec - tv0.tv_sec);
	return results;
}

void diop_show(struct diop_args *a)
{
	int64 lastblk;

	printf("Doing %lld ", a->niter);
	if (a->duration)
		printf("or %ds of ", a->duration);
	if (a->incr == RAND_INCR)
		printf("random ");
	else if (a->incr == 0)
		printf("inplace ");
	else if (a->incr < 0)
		printf("reverse ");
	else
		printf("sequential ");
	if (a->rdwr) {
		if (a->sync)
			printf("sync ");
		printf("writes");
	} else
		printf("reads");
	if (a->nproc != 1)
		printf("(in each of %d processes)", a->nproc);
	if (a->incr == RAND_INCR)
		lastblk = a->range;
	else
		lastblk = a->startblk + (a->niter - 1) * a->incr;
	if (a->startblk == RAND_START) 
		printf("; random start block; block size %lld\n", a->blksize);
	else
		printf("; block range from %lld to %lld; block size %lld\n",
			a->startblk, lastblk, a->blksize);
	fflush(stdout);
}

struct diop_results diop_parallel(int nproc, struct diop_args *a)
{
	double elapsed;
	struct diop_results tmp;
	int i, status;
	int result_fd;
	int pnum;
	struct timeval tv1, tv2;
	char tempname[20];
	struct diop_results totals = { 0 };
	
	a->nproc = nproc;
	if (nproc <= 1)
		return diop(a);

	strcpy(tempname, "/tmp/diopXXXXXX");
	result_fd = mkstemp(tempname);
	if (result_fd < 0) {
		perror(tempname);
		exit(1);
	}
	
	gettimeofday(&tv1, NULL);
	for (pnum = 0; pnum < nproc; pnum++) {
		if (fork() == 0) {
			struct diop_results results;
			int fd;

			if (a->verbose && pnum > 0)
				a->verbose = 0;
			results = diop(a);
			fd = open(tempname, O_WRONLY);
			if (fd < 0) {
				fprintf(stderr, "%d: %s\n", pnum, strerror(errno));
				exit(1);
			}
			if (lseek(fd, (off_t)(pnum * sizeof results), SEEK_SET) < 0) {
				fprintf(stderr, "%d: %s\n", pnum, strerror(errno));
				exit(1);
			}
			if (write(fd, (void *)&results, (size_t)sizeof results) < 0) {
				fprintf(stderr, "%d: %s\n", pnum, strerror(errno));
				exit(1);
			}
			exit(0);
		}
	}
	while (wait(&status) > 0)
		;
	gettimeofday(&tv2, NULL);
	elapsed = tv2.tv_usec - tv1.tv_usec;
	elapsed += 1000000. * (tv2.tv_sec - tv1.tv_sec);
	if (elapsed==0)
		elapsed=1;
	totals.elapsed = elapsed;

	/*
	 * Merge results
	 * Each child will have written its results into an offset in the
	 * results file.
	 */
	for (i = 0; i < nproc; i++) {
		if (pread(result_fd, (void *)&tmp, (size_t)sizeof tmp, (off_t)(i * (sizeof tmp))) != sizeof tmp) {
			perror("pread");
			exit(1);
		}
		if (tmp.lowest < totals.lowest || totals.lowest == 0)
			totals.lowest = tmp.lowest;
		if (tmp.highest > totals.highest)
			totals.highest = tmp.highest;
		totals.sum += tmp.sum;
		totals.sumsq += tmp.sumsq;
		totals.count += tmp.count;
	}
	unlink(tempname);
	return totals;
}

char *
diop_drive_id(char *blkdev, int page)
{
	char *pagename;
	char command[200], out[100], *op, *new;
	FILE *fp;
	int n;

	if (page == 0x83)
		pagename = "--page=0x83";
	else
		pagename = "--page=0x80";
	setenv("PATH", "/sbin:/lib/udev:/bin:/usr/bin:/usr/sbin", 1);
	sprintf(command, "scsi_id --whitelisted --replace-whitespace %s %s",
		pagename, blkdev);
	fp = popen(command, "r");
	if (fp == NULL) {
		fprintf(stderr, "scsi_id command failed for %s\n", blkdev);
		return NULL;
	}
	n = fread(out, 1, sizeof(out), fp);
	if (n < 6) {
		fprintf(stderr, "bogus scsi_id info for %s\n", blkdev);
		return NULL;
	}
	if (out[n-1] == '\n')
		out[n-1] = 0;
	op = out;
	if (strncmp(op, "SATA_", 5) == 0)
		op += 5;
	else if (strncmp(op, "1ATA_", 5) == 0)
		op += 5;
	new = malloc(strlen(op) + 1);
	strcpy(new, op);
	return new;
}
