/*	$OpenBSD: test_helper.c,v 1.14 2025/04/15 04:00:42 djm Exp $	*/
/*
 * Copyright (c) 2011 Damien Miller <djm@mindrot.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* Utility functions/framework for regress tests */

#include <sys/types.h>
#include <sys/uio.h>
#include <sys/time.h>

#include <assert.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vis.h>

#include <openssl/bn.h>

#include "test_helper.h"
#include "atomicio.h"
#include "match.h"
#include "misc.h"
#include "xmalloc.h"

#define BENCH_FAST_DEADLINE	1
#define BENCH_NORMAL_DEADLINE	10
#define BENCH_SLOW_DEADLINE	60
#define BENCH_SAMPLES_ALLOC	8192
#define BENCH_COLUMN_WIDTH	40

#define MINIMUM(a, b)    (((a) < (b)) ? (a) : (b))

#define TEST_CHECK_INT(r, pred) do {		\
		switch (pred) {			\
		case TEST_EQ:			\
			if (r == 0)		\
				return;		\
			break;			\
		case TEST_NE:			\
			if (r != 0)		\
				return;		\
			break;			\
		case TEST_LT:			\
			if (r < 0)		\
				return;		\
			break;			\
		case TEST_LE:			\
			if (r <= 0)		\
				return;		\
			break;			\
		case TEST_GT:			\
			if (r > 0)		\
				return;		\
			break;			\
		case TEST_GE:			\
			if (r >= 0)		\
				return;		\
			break;			\
		default:			\
			abort();		\
		}				\
	} while (0)

#define TEST_CHECK(x1, x2, pred) do {		\
		switch (pred) {			\
		case TEST_EQ:			\
			if (x1 == x2)		\
				return;		\
			break;			\
		case TEST_NE:			\
			if (x1 != x2)		\
				return;		\
			break;			\
		case TEST_LT:			\
			if (x1 < x2)		\
				return;		\
			break;			\
		case TEST_LE:			\
			if (x1 <= x2)		\
				return;		\
			break;			\
		case TEST_GT:			\
			if (x1 > x2)		\
				return;		\
			break;			\
		case TEST_GE:			\
			if (x1 >= x2)		\
				return;		\
			break;			\
		default:			\
			abort();		\
		}				\
	} while (0)

extern char *__progname;

static int verbose_mode = 0;
static int quiet_mode = 0;
static char *active_test_name = NULL;
static u_int test_number = 0;
static test_onerror_func_t *test_onerror = NULL;
static void *onerror_ctx = NULL;
static const char *data_dir = NULL;
static char subtest_info[512];
static int fast = 0;
static int slow = 0;
static int benchmark_detail_statistics = 0;

static int benchmark = 0;
static const char *bench_name = NULL;
static char *benchmark_pattern = NULL;
static struct timespec bench_start_time, bench_finish_time;
static struct timespec *bench_samples;
static int bench_skip, bench_nruns, bench_nalloc;
double bench_accum_secs;

int
main(int argc, char **argv)
{
	int ch;

	while ((ch = getopt(argc, argv, "O:bBFfvqd:")) != -1) {
		switch (ch) {
		case 'b':
			benchmark = 1;
			break;
		case 'B':
			benchmark = benchmark_detail_statistics = 1;
			break;
		case 'O':
			benchmark_pattern = xstrdup(optarg);
			break;
		case 'F':
			slow = 1;
			break;
		case 'f':
			fast = 1;
			break;
		case 'd':
			data_dir = optarg;
			break;
		case 'q':
			verbose_mode = 0;
			quiet_mode = 1;
			break;
		case 'v':
			verbose_mode = 1;
			quiet_mode = 0;
			break;
		default:
			fprintf(stderr, "Unrecognised command line option\n");
			fprintf(stderr, "Usage: %s [-vqfFbB] [-d data_dir] "
			    "[-O pattern]\n", __progname);
			exit(1);
		}
	}
	setvbuf(stdout, NULL, _IONBF, 0);
	if (!quiet_mode)
		printf("%s: ", __progname);
	if (verbose_mode)
		printf("\n");

	if (benchmark)
		benchmarks();
	else
		tests();

	if (!quiet_mode && !benchmark)
		printf(" %u tests ok\n", test_number);
	return 0;
}

int
test_is_verbose(void)
{
	return verbose_mode;
}

int
test_is_quiet(void)
{
	return quiet_mode;
}

int
test_is_fast(void)
{
	return fast;
}

int
test_is_slow(void)
{
	return slow;
}

const char *
test_data_file(const char *name)
{
	static char ret[PATH_MAX];

	if (data_dir != NULL)
		snprintf(ret, sizeof(ret), "%s/%s", data_dir, name);
	else
		strlcpy(ret, name, sizeof(ret));
	if (access(ret, F_OK) != 0) {
		fprintf(stderr, "Cannot access data file %s: %s\n",
		    ret, strerror(errno));
		exit(1);
	}
	return ret;
}

void
test_info(char *s, size_t len)
{
	snprintf(s, len, "In test %u: \"%s\"%s%s\n", test_number,
	    active_test_name == NULL ? "<none>" : active_test_name,
	    *subtest_info != '\0' ? " - " : "", subtest_info);
}

static void
siginfo(int unused __attribute__((__unused__)))
{
	char buf[256];

	test_info(buf, sizeof(buf));
	atomicio(vwrite, STDERR_FILENO, buf, strlen(buf));
}

void
test_start(const char *n)
{
	assert(active_test_name == NULL);
	assert((active_test_name = strdup(n)) != NULL);
	*subtest_info = '\0';
	if (verbose_mode)
		printf("test %u - \"%s\": ", test_number, active_test_name);
	test_number++;
#ifdef SIGINFO
	signal(SIGINFO, siginfo);
#endif
	signal(SIGUSR1, siginfo);
}

void
set_onerror_func(test_onerror_func_t *f, void *ctx)
{
	test_onerror = f;
	onerror_ctx = ctx;
}

void
test_done(void)
{
	*subtest_info = '\0';
	assert(active_test_name != NULL);
	free(active_test_name);
	active_test_name = NULL;
	if (verbose_mode)
		printf("OK\n");
	else if (!quiet_mode && !benchmark) {
		printf(".");
		fflush(stdout);
	}
}

void
test_subtest_info(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(subtest_info, sizeof(subtest_info), fmt, ap);
	va_end(ap);
}

int
test_is_benchmark(void)
{
	return benchmark;
}

void
ssl_err_check(const char *file, int line)
{
	long openssl_error = ERR_get_error();

	if (openssl_error == 0)
		return;

	fprintf(stderr, "\n%s:%d: uncaught OpenSSL error: %s",
	    file, line, ERR_error_string(openssl_error, NULL));
	abort();
}

static const char *
pred_name(enum test_predicate p)
{
	switch (p) {
	case TEST_EQ:
		return "EQ";
	case TEST_NE:
		return "NE";
	case TEST_LT:
		return "LT";
	case TEST_LE:
		return "LE";
	case TEST_GT:
		return "GT";
	case TEST_GE:
		return "GE";
	default:
		return "UNKNOWN";
	}
}

static void
test_die(void)
{
	if (test_onerror != NULL)
		test_onerror(onerror_ctx);
	abort();
}

static void
test_header(const char *file, int line, const char *a1, const char *a2,
    const char *name, enum test_predicate pred)
{
	fprintf(stderr, "\n%s:%d test #%u \"%s\"%s%s\n", 
	    file, line, test_number, active_test_name,
	    *subtest_info != '\0' ? " - " : "", subtest_info);
	fprintf(stderr, "ASSERT_%s_%s(%s%s%s) failed:\n",
	    name, pred_name(pred), a1,
	    a2 != NULL ? ", " : "", a2 != NULL ? a2 : "");
}

void
assert_bignum(const char *file, int line, const char *a1, const char *a2,
    const BIGNUM *aa1, const BIGNUM *aa2, enum test_predicate pred)
{
	int r = BN_cmp(aa1, aa2);

	TEST_CHECK_INT(r, pred);
	test_header(file, line, a1, a2, "BIGNUM", pred);
	fprintf(stderr, "%12s = 0x%s\n", a1, BN_bn2hex(aa1));
	fprintf(stderr, "%12s = 0x%s\n", a2, BN_bn2hex(aa2));
	test_die();
}

void
assert_string(const char *file, int line, const char *a1, const char *a2,
    const char *aa1, const char *aa2, enum test_predicate pred)
{
	int r;

	/* Verify pointers are not NULL */
	assert_ptr(file, line, a1, "NULL", aa1, NULL, TEST_NE);
	assert_ptr(file, line, a2, "NULL", aa2, NULL, TEST_NE);

	r = strcmp(aa1, aa2);
	TEST_CHECK_INT(r, pred);
	test_header(file, line, a1, a2, "STRING", pred);
	fprintf(stderr, "%12s = %s (len %zu)\n", a1, aa1, strlen(aa1));
	fprintf(stderr, "%12s = %s (len %zu)\n", a2, aa2, strlen(aa2));
	test_die();
}

void
assert_mem(const char *file, int line, const char *a1, const char *a2,
    const void *aa1, const void *aa2, size_t l, enum test_predicate pred)
{
	int r;
	char *aa1_tohex = NULL;
	char *aa2_tohex = NULL;

	if (l == 0)
		return;
	/* If length is >0, then verify pointers are not NULL */
	assert_ptr(file, line, a1, "NULL", aa1, NULL, TEST_NE);
	assert_ptr(file, line, a2, "NULL", aa2, NULL, TEST_NE);

	r = memcmp(aa1, aa2, l);
	TEST_CHECK_INT(r, pred);
	test_header(file, line, a1, a2, "STRING", pred);
	aa1_tohex = tohex(aa1, MINIMUM(l, 256));
	aa2_tohex = tohex(aa2, MINIMUM(l, 256));
	fprintf(stderr, "%12s = %s (len %zu)\n", a1, aa1_tohex, l);
	fprintf(stderr, "%12s = %s (len %zu)\n", a2, aa2_tohex, l);
	free(aa1_tohex);
	free(aa2_tohex);
	test_die();
}

static int
memvalcmp(const u_int8_t *s, u_char v, size_t l, size_t *where)
{
	size_t i;

	for (i = 0; i < l; i++) {
		if (s[i] != v) {
			*where = i;
			return 1;
		}
	}
	return 0;
}

void
assert_mem_filled(const char *file, int line, const char *a1,
    const void *aa1, u_char v, size_t l, enum test_predicate pred)
{
	size_t where = -1;
	int r;
	char tmp[64];
	char *aa1_tohex = NULL;

	if (l == 0)
		return;
	/* If length is >0, then verify the pointer is not NULL */
	assert_ptr(file, line, a1, "NULL", aa1, NULL, TEST_NE);

	r = memvalcmp(aa1, v, l, &where);
	TEST_CHECK_INT(r, pred);
	test_header(file, line, a1, NULL, "MEM_ZERO", pred);
	aa1_tohex = tohex(aa1, MINIMUM(l, 20));
	fprintf(stderr, "%20s = %s%s (len %zu)\n", a1,
	    aa1_tohex, l > 20 ? "..." : "", l);
	free(aa1_tohex);
	snprintf(tmp, sizeof(tmp), "(%s)[%zu]", a1, where);
	fprintf(stderr, "%20s = 0x%02x (expected 0x%02x)\n", tmp,
	    ((u_char *)aa1)[where], v);
	test_die();
}

void
assert_int(const char *file, int line, const char *a1, const char *a2,
    int aa1, int aa2, enum test_predicate pred)
{
	TEST_CHECK(aa1, aa2, pred);
	test_header(file, line, a1, a2, "INT", pred);
	fprintf(stderr, "%12s = %d\n", a1, aa1);
	fprintf(stderr, "%12s = %d\n", a2, aa2);
	test_die();
}

void
assert_size_t(const char *file, int line, const char *a1, const char *a2,
    size_t aa1, size_t aa2, enum test_predicate pred)
{
	TEST_CHECK(aa1, aa2, pred);
	test_header(file, line, a1, a2, "SIZE_T", pred);
	fprintf(stderr, "%12s = %zu\n", a1, aa1);
	fprintf(stderr, "%12s = %zu\n", a2, aa2);
	test_die();
}

void
assert_u_int(const char *file, int line, const char *a1, const char *a2,
    u_int aa1, u_int aa2, enum test_predicate pred)
{
	TEST_CHECK(aa1, aa2, pred);
	test_header(file, line, a1, a2, "U_INT", pred);
	fprintf(stderr, "%12s = %u / 0x%x\n", a1, aa1, aa1);
	fprintf(stderr, "%12s = %u / 0x%x\n", a2, aa2, aa2);
	test_die();
}

void
assert_long(const char *file, int line, const char *a1, const char *a2,
    long aa1, long aa2, enum test_predicate pred)
{
	TEST_CHECK(aa1, aa2, pred);
	test_header(file, line, a1, a2, "LONG", pred);
	fprintf(stderr, "%12s = %ld / 0x%lx\n", a1, aa1, aa1);
	fprintf(stderr, "%12s = %ld / 0x%lx\n", a2, aa2, aa2);
	test_die();
}

void
assert_long_long(const char *file, int line, const char *a1, const char *a2,
    long long aa1, long long aa2, enum test_predicate pred)
{
	TEST_CHECK(aa1, aa2, pred);
	test_header(file, line, a1, a2, "LONG LONG", pred);
	fprintf(stderr, "%12s = %lld / 0x%llx\n", a1, aa1, aa1);
	fprintf(stderr, "%12s = %lld / 0x%llx\n", a2, aa2, aa2);
	test_die();
}

void
assert_char(const char *file, int line, const char *a1, const char *a2,
    char aa1, char aa2, enum test_predicate pred)
{
	char buf[8];

	TEST_CHECK(aa1, aa2, pred);
	test_header(file, line, a1, a2, "CHAR", pred);
	fprintf(stderr, "%12s = '%s' / 0x02%x\n", a1,
	    vis(buf, aa1, VIS_SAFE|VIS_NL|VIS_TAB|VIS_OCTAL, 0), aa1);
	fprintf(stderr, "%12s = '%s' / 0x02%x\n", a1,
	    vis(buf, aa2, VIS_SAFE|VIS_NL|VIS_TAB|VIS_OCTAL, 0), aa2);
	test_die();
}

void
assert_u8(const char *file, int line, const char *a1, const char *a2,
    u_int8_t aa1, u_int8_t aa2, enum test_predicate pred)
{
	TEST_CHECK(aa1, aa2, pred);
	test_header(file, line, a1, a2, "U8", pred);
	fprintf(stderr, "%12s = 0x%02x %u\n", a1, aa1, aa1);
	fprintf(stderr, "%12s = 0x%02x %u\n", a2, aa2, aa2);
	test_die();
}

void
assert_u16(const char *file, int line, const char *a1, const char *a2,
    u_int16_t aa1, u_int16_t aa2, enum test_predicate pred)
{
	TEST_CHECK(aa1, aa2, pred);
	test_header(file, line, a1, a2, "U16", pred);
	fprintf(stderr, "%12s = 0x%04x %u\n", a1, aa1, aa1);
	fprintf(stderr, "%12s = 0x%04x %u\n", a2, aa2, aa2);
	test_die();
}

void
assert_u32(const char *file, int line, const char *a1, const char *a2,
    u_int32_t aa1, u_int32_t aa2, enum test_predicate pred)
{
	TEST_CHECK(aa1, aa2, pred);
	test_header(file, line, a1, a2, "U32", pred);
	fprintf(stderr, "%12s = 0x%08x %u\n", a1, aa1, aa1);
	fprintf(stderr, "%12s = 0x%08x %u\n", a2, aa2, aa2);
	test_die();
}

void
assert_u64(const char *file, int line, const char *a1, const char *a2,
    u_int64_t aa1, u_int64_t aa2, enum test_predicate pred)
{
	TEST_CHECK(aa1, aa2, pred);
	test_header(file, line, a1, a2, "U64", pred);
	fprintf(stderr, "%12s = 0x%016llx %llu\n", a1,
	    (unsigned long long)aa1, (unsigned long long)aa1);
	fprintf(stderr, "%12s = 0x%016llx %llu\n", a2,
	    (unsigned long long)aa2, (unsigned long long)aa2);
	test_die();
}

void
assert_ptr(const char *file, int line, const char *a1, const char *a2,
    const void *aa1, const void *aa2, enum test_predicate pred)
{
	TEST_CHECK(aa1, aa2, pred);
	test_header(file, line, a1, a2, "PTR", pred);
	fprintf(stderr, "%12s = %p\n", a1, aa1);
	fprintf(stderr, "%12s = %p\n", a2, aa2);
	test_die();
}

static double
tstod(const struct timespec *ts)
{
	return (double)ts->tv_sec + ((double)ts->tv_nsec / 1000000000.0);
}

void
bench_start(const char *file, int line, const char *name)
{
	char *cp;

	if (bench_name != NULL) {
		fprintf(stderr, "\n%s:%d internal error: BENCH_START() called "
		    "while previous benchmark \"%s\" incomplete",
		    file, line, bench_name);
		abort();
	}
	cp = xstrdup(name);
	lowercase(cp);
	bench_skip = benchmark_pattern != NULL &&
	    match_pattern_list(cp, benchmark_pattern, 1) != 1;
	free(cp);

	bench_name = name;
	bench_nruns = 0;
	if (bench_skip)
		return;
	free(bench_samples);
	bench_nalloc = BENCH_SAMPLES_ALLOC;
	bench_samples = xcalloc(sizeof(*bench_samples), bench_nalloc);
	bench_accum_secs = 0;
}

int
bench_done(void)
{
	return bench_skip || bench_accum_secs >= (fast ? BENCH_FAST_DEADLINE :
	    (slow ? BENCH_SLOW_DEADLINE : BENCH_NORMAL_DEADLINE));
}

void
bench_case_start(const char *file, int line)
{
	clock_gettime(CLOCK_REALTIME, &bench_start_time);
}

void
bench_case_finish(const char *file, int line)
{
	struct timespec ts;

	clock_gettime(CLOCK_REALTIME, &bench_finish_time);
	timespecsub(&bench_finish_time, &bench_start_time, &ts);
	if (bench_nruns >= bench_nalloc) {
		if (bench_nalloc >= INT_MAX / 2) {
			fprintf(stderr, "\n%s:%d benchmark %s too many samples",
			    __FILE__, __LINE__, bench_name);
			abort();
		}
		bench_samples = xrecallocarray(bench_samples, bench_nalloc,
		    bench_nalloc * 2, sizeof(*bench_samples));
		bench_nalloc *= 2;
	}
	bench_samples[bench_nruns++] = ts;
	bench_accum_secs += tstod(&ts);
}

static int
tscmp(const void *aa, const void *bb)
{
	const struct timespec *a = (const struct timespec *)aa;
	const struct timespec *b = (const struct timespec *)bb;

	if (timespeccmp(a, b, ==))
		return 0;
	return timespeccmp(a, b, <) ? -1 : 1;
}

void
bench_finish(const char *file, int line, const char *unit)
{
	double std_dev = 0, mean_spr, mean_rps, med_spr, med_rps;
	int i;

	if (bench_skip)
		goto done;

	if (bench_nruns < 1) {
		fprintf(stderr, "\n%s:%d benchmark %s never ran", file, line,
		    bench_name);
		abort();
	}
	/* median */
	qsort(bench_samples, bench_nruns, sizeof(*bench_samples), tscmp);
	i = bench_nruns / 2;
	med_spr = tstod(&bench_samples[i]);
	if (bench_nruns > 1 && bench_nruns & 1)
		med_spr = (med_spr + tstod(&bench_samples[i - 1])) / 2.0;
	med_rps = (med_spr == 0.0) ? INFINITY : 1.0/med_spr;
	/* mean */
	mean_spr = bench_accum_secs / (double)bench_nruns;
	mean_rps = (mean_spr == 0.0) ? INFINITY : 1.0/mean_spr;
	/* std. dev */
	std_dev = 0;
	for (i = 0; i < bench_nruns; i++) {
		std_dev = tstod(&bench_samples[i]) - mean_spr;
		std_dev *= std_dev;
	}
	std_dev /= (double)bench_nruns;
	std_dev = sqrt(std_dev);
	if (benchmark_detail_statistics) {
		printf("%s: %d runs in %0.3fs, %0.03f/%0.03f ms/%s "
		    "(mean/median), std.dev %0.03f ms, "
		    "%0.2f/%0.2f %s/s (mean/median)\n",
		    bench_name, bench_nruns, bench_accum_secs,
		    mean_spr * 1000, med_spr * 1000, unit, std_dev * 1000,
		    mean_rps, med_rps, unit);
	} else {
		printf("%-*s %0.2f %s/s\n", BENCH_COLUMN_WIDTH,
		    bench_name, med_rps, unit);
	}
 done:
	bench_name = NULL;
	bench_nruns = 0;
	free(bench_samples);
	bench_samples = NULL;
	bench_skip = 0;
}
