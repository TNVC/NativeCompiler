#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

int main(int argc, char *argv[])
{
  timeval stop{}, start{};
  gettimeofday(&start, nullptr);
  system(argv[1]);
  gettimeofday(&stop , nullptr);
  printf("Time: %ld\n",
           (stop.tv_sec  - start.tv_sec )*1000000 +
           (stop.tv_usec - start.tv_usec)*1);

  return 0;
}
