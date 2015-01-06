#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "common.h"

int64_t getMicrosecondTimeStamp() 
{
    int64_t retval;
    struct timeval tv; 
    if (gettimeofday(&tv, NULL)) {
        perror("gettimeofday");
        abort();
    }   
    retval = ((int64_t)tv.tv_sec) * 1000000 + tv.tv_usec;
    return retval;
}

static void usage(FILE *f) {
  fprintf(f, "Usage: hpx_collective [options] maxmsgsize  \n"
          "\t-h, show help\n");
  hpx_print_help();
  fflush(f);
}

/// Initialize a double zero
static void initDouble(double *input) {
  *input = 0;
}

/// Update *lhs with with the max(lhs, rhs);
static void maxDouble(double *lhs, const double *rhs) {
  *lhs = (*lhs > *rhs) ? *lhs : *rhs;
}

/// Update *lhs with with the min(lhs, rhs);
static void minDouble(double *lhs, const double *rhs) {
  *lhs = (*lhs < *rhs) ? *lhs : *rhs;
}

static void sumDouble(double *output,const double *input, const size_t size) {
  assert(sizeof(double) == size);
  *output += *input;
  return;
}

static void print_header (char *header, int rank, int full)
{
    if(rank == 0) {
        fprintf(stdout, header, "");

        if (print_size) {
            fprintf(stdout, "%-*s", 10, "# Size");
            fprintf(stdout, "%*s", FIELD_WIDTH, "Avg Latency(us)");
        }

        else {
            fprintf(stdout, "# Avg Latency(us)");
        }

        if (full) {
            fprintf(stdout, "%*s", FIELD_WIDTH, "Min Latency(us)");
            fprintf(stdout, "%*s", FIELD_WIDTH, "Max Latency(us)");
            fprintf(stdout, "%*s\n", 12, "Iterations");
        }

        else {
            fprintf(stdout, "\n");
        }

        fflush(stdout);
    }
}

static void print_data (int rank, int full, int size, double avg_time, double
        min_time, double max_time, int iterations)
{
    if(rank == 0) {
        if (print_size) {
            fprintf(stdout, "%-*d", 10, size);
            fprintf(stdout, "%*.*f", FIELD_WIDTH, FLOAT_PRECISION, avg_time);
        }

        else {
            fprintf(stdout, "%*.*f", 17, FLOAT_PRECISION, avg_time);
        }

        if (full) {
            fprintf(stdout, "%*.*f%*.*f%*d\n",
                    FIELD_WIDTH, FLOAT_PRECISION, min_time,
                    FIELD_WIDTH, FLOAT_PRECISION, max_time,
                    12, iterations);
        }

        else {
            fprintf(stdout, "\n");
        }

        fflush(stdout);
    }
}
               