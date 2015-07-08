#include <stdio.h>
#include <stdlib.h>

#define NUM_COLS (8)
#define COL_RANGE (1000000)

int main(int argc, char **argv) {
  if (5 != argc) {
    fprintf(stderr, "Usage %s <num keys> <rows per key> <offset> <rand seed>\n", argv[0]);
    return 1;
  }

  char *endptr;
  long long numkeys = strtoll(argv[1], &endptr, 10);
  long long rowsperkey = strtoll(argv[2], &endptr, 10);
  long long offset = strtoll(argv[3], &endptr, 10) * numkeys;
  int seed = atoi(argv[4]);

  struct drand48_data lcg;
  srand48_r(seed, &lcg);

  double rval;
  long long i, j, k;
  for (i = offset; i < offset+numkeys; i++) {
    for (j = 0; j < rowsperkey; j++) {
      printf("%lld,%lld", i, j);
      for (k = 0; k < NUM_COLS; k++) {
	drand48_r(&lcg, &rval);
	printf(",%lld", (long long)(rval * COL_RANGE));
      }
      printf("\n");
    }
  }


  return 0;
}
