#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include "../rpthread.h"

/* A scratch program template on which to call and
 * test rpthread library functions as you implement
 * them.
 *
 * You can modify and use this program as much as possible.
 * This will not be graded.
 */
void bar(void * arg){
  printf("bar\n");
  int* ret=(int*)malloc(sizeof(int));
  ret=(int*)arg;
  
  while(1);

  pthread_exit((void*)ret);
  
}

void foo(void * arg){
  int s=1;
  int k=1;
  pthread_t * threads=(pthread_t*)malloc(s*sizeof(pthread_t));
  int i;
  void * y;
  for (i=0;i<s;i++){
    pthread_create(&threads[i],NULL,&bar, (void*)&k);
    k++;
  }
  printf("foo\n");
  for (i=0; i<s; i++){
    pthread_join(threads[i],&y);
    printf("%d\n",*(int*)y);
  }
}
int main(int argc, char **argv) {
  pthread_t t;
  pthread_create(&t, NULL, &foo,NULL);
  pthread_join(t, NULL);
	/* Implement HERE */

	return 0;
}
