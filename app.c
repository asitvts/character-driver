#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>


#define path "/dev/my_device"
#define no_of_t 3


void* thread_func(void*){

	int fd = open(path, O_RDWR);
	if(fd==-1){
		printf("error openeing the file\n");
		return NULL;
	}
	printf("file opened\n");
	
	sleep(5);
	
	char* str="hello";
	ssize_t bytes_written = write(fd, str, strlen(str));
	if(bytes_written<=0){
		printf("some kind of error while writing\n");
		close(fd);
		return NULL;
	}
	
	printf("written\n");
	
	close(fd);
	return NULL;

}


int main(){
	

	pthread_t threads[no_of_t];
	
	for(int i=0; i<no_of_t; i++){
		if(pthread_create(&threads[i], NULL, &thread_func, NULL)){
			printf("some kind of error creating the threads\n");
		}
	}
	
	for(int i=0; i<no_of_t; i++){
		if(pthread_join(threads[i], NULL)){
			printf("some kind of error joining the threads\n");
		}
	}
	
	printf("application code exiting\n");
	
	return 0;

}




