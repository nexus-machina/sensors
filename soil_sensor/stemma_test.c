#include <stdio.h>
#include <fcntl.h>	/* For open() */
#include <unistd.h>	/* For write() and close() */
#include <errno.h>	/* For errno and perror() */
#include <string.h>

int main()
{
	const char *data = "0";
	char buf[4];

	int stemma_dev = open("/dev/stemma00", 0);
	if (stemma_dev < 0)
		perror("Failed to open the device file: /dev/stemma00."); 

	while(1){
		read(stemma_dev, buf, 4);
		printf(":\n", buf);
	}

	/* implement new features */

	/* write to the device */
	/*ssize_t bytes_written = write(stemma_dev, data, strlen(data));*/

	close(stemma_dev);
	
	return 0;
}
