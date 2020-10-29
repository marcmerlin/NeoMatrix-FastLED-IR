#include <errno.h>
#include <fcntl.h> 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/stat.h>


int set_interface_attribs(int ttyfd, int speed)
{
    struct termios tty;

    if (tcgetattr(ttyfd, &tty) < 0) {
        printf("Error from tcgetattr: %s\n", strerror(errno));
        return -1;
    }

    cfsetospeed(&tty, (speed_t)speed);
    cfsetispeed(&tty, (speed_t)speed);

    tty.c_cflag |= (CLOCAL | CREAD);    /* ignore modem controls */
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;         /* 8-bit characters */
    tty.c_cflag &= ~PARENB;     /* no parity bit */
    tty.c_cflag &= ~CSTOPB;     /* only need 1 stop bit */
    tty.c_cflag &= ~CRTSCTS;    /* no hardware flowcontrol */

    /* setup for non-canonical mode */
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    tty.c_oflag &= ~OPOST;

    /* fetch bytes as they become available */
    // https://blog.mbedded.ninja/programming/operating-systems/linux/linux-serial-ports-using-c-cpp/
    // Non blocking
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 0;

    if (tcsetattr(ttyfd, TCSANOW, &tty) != 0) {
        printf("Error from tcsetattr: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

int openttyUSB(char **serialdev) {
    // static because the name is sent back to the caller
    static char *dev[] = { "/dev/ttyUSB0", "/dev/ttyUSB1", "/dev/ttyUSB2" };
    int devidx = 0;
    int ttyfd;

    while (devidx<3 && (ttyfd = open((*serialdev = dev[devidx]), O_RDWR | O_NOCTTY | O_SYNC)) < 0 && ++devidx) {
        printf("Error opening %s: %s\n", *serialdev, strerror(errno));
    }
    /*baudrate 115200, 8 bits, no parity, 1 stop bit */
    if (ttyfd >= 0) {
	set_interface_attribs(ttyfd, B115200);
        printf("Opened %s\n", *serialdev);
    }
    return ttyfd;
}

int send_serial(int ttyfd, char *xstr) {
    int wlen;
    int xlen = strlen(xstr);

    wlen = write(ttyfd, xstr, xlen);
    // tcdrain(ttyfd);
    if (wlen != xlen) {
        printf("Error from write: %d, %d\n", wlen, errno);
	return -1;
    }
    printf(">>>>>>>>>>>>>>>>>>> n\n");
    return 0;
}

int main()
{
    static int ttyfd = -1;
    static char *serialdev = NULL;
    static int counter = 0;

    do {
        static char buf[1024];
	static char *ptr = buf;
        char s;
        int rdlen;
	struct stat stbuf;

	if (! (counter++ % 1000000)) send_serial(ttyfd, "n\n");

	if (ttyfd > -1 && stat(serialdev, &stbuf)) { 
	    printf("ttyfd closed %d, (%s)\n", ttyfd, serialdev);
	    close(ttyfd); 
	    ttyfd = -1; 
	    serialdev = NULL;
	}
	//printf("Serial0 %d, %s\n", ttyfd, serialdev);
	if (serialdev && (ttyfd < 0)) printf("Serial closed, re-opening\n");
	if (ttyfd < 0) ttyfd = openttyUSB(&serialdev);
	//printf("Serial %d, %s\n", ttyfd, serialdev);
	if (ttyfd < 0) continue;
		
	if ( (rdlen = read(ttyfd, &s, 1)) > 0) {
	    ptr[0] = s;
	    ptr[1] = 0;
	    ptr++;
	    if (s == '\n' ) {
		printf("ESP> %s", buf);
		ptr = buf;
		char numbuf[4];
		if (! strncmp(buf, "|D:", 3)) {
		    int num;
		    strncpy(numbuf, buf+3, 3);
		    numbuf[3] = 0;
		    num = atoi(numbuf);
		    printf("Got demo %d\n", num);
		}
	    }
        }               
	if (rdlen < 0) {
	    printf("Error from read: %d: %s\n", rdlen, strerror(errno));
	} else {
	    //printf("read done\n");
	}
    } while (1);
}
