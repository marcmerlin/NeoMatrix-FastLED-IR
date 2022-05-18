/*
 *  V4L2 video capture example
 *
 *  This program can be used and distributed without restrictions.
 *
 *      This program is provided with the V4L2 API
 * see https://linuxtv.org/docs.php for more information
 */

#ifdef __linux__
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <getopt.h>             /* getopt_long() */

#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <linux/videodev2.h>

#define CLEAR(x) memset(&(x), 0, sizeof(x))

struct buffer {
        char   *start;
        size_t  length;
};

const char            *dev_name = "/dev/video0";
static int              fd = -1;
struct buffer          buffer_;

// Compat with arduino C++
#ifndef ARDUINO
#define uint8_t u_int8_t
#define uint16_t u_int16_t
#define uint32_t u_int32_t
#endif

uint16_t CAPTURE_W = 320;
uint16_t CAPTURE_H = 240;

static void exit_(int val) {
// In production, don't exit when camera is missing
#ifndef ARDUINOONPC
    exit(val);
#endif
}

static void errno_exit_(const char *s)
{
        fprintf(stderr, "%s error %d, %s\n", s, errno, strerror(errno));
        exit_(EXIT_FAILURE);
}

static int xioctl(int fh, int request, void *arg)
{
        int r;

        do {
                r = ioctl(fh, request, arg);
        } while (-1 == r && EINTR == errno);

        return r;
}

static void process_image(const void *p, int size)
{
#ifndef V4LCAPTURE_INCLUDE
	fwrite(p, size, 1, stdout);
        fflush(stdout);
#endif
}

static int read_frame(void)
{
        struct v4l2_buffer buf;

	CLEAR(buf);

	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;

	if (-1 == xioctl(fd, VIDIOC_DQBUF, &buf)) {
		switch (errno) {
		case EAGAIN:
			return 0;

		case EIO:
			/* Could ignore EIO, see spec. */

			/* fall through */

		default:
			errno_exit_("VIDIOC_DQBUF");
		}
	}

	process_image(buffer_.start, buf.bytesused);

	if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
		errno_exit_("VIDIOC_QBUF");
        return 1;
}

static void mainloop(void)
{
	for (;;) {
		fd_set fds;
		struct timeval tv;
		int r;

		FD_ZERO(&fds);
		FD_SET(fd, &fds);

		/* Timeout. */
		tv.tv_sec = 2;
		tv.tv_usec = 0;

		r = select(fd + 1, &fds, NULL, NULL, &tv);

		if (-1 == r) {
			if (EINTR == errno)
				continue;
			errno_exit_("select");
		}

		if (0 == r) {
			fprintf(stderr, "select timeout\n");
			exit_(EXIT_FAILURE);
		}

		if (read_frame())
			break;
		/* EAGAIN - continue select loop. */
	}
}

static void start_capturing(void)
{
        enum v4l2_buf_type type;

	struct v4l2_buffer buf;

	CLEAR(buf);
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;
	buf.index = 0;

	if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
		errno_exit_("VIDIOC_QBUF");
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (-1 == xioctl(fd, VIDIOC_STREAMON, &type))
		errno_exit_("VIDIOC_STREAMON");
}

static void init_mmap(void)
{
        struct v4l2_requestbuffers req;

        CLEAR(req);

        req.count = 4;
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_MMAP;

        if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req)) {
                if (EINVAL == errno) {
                        fprintf(stderr, "%s does not support "
                                 "memory mappingn", dev_name);
                        exit_(EXIT_FAILURE);
                } else {
                        errno_exit_("VIDIOC_REQBUFS");
                }
        }

        if (req.count < 2) {
                fprintf(stderr, "Insufficient buffer memory on %s\n",
                         dev_name);
                exit_(EXIT_FAILURE);
        }

	struct v4l2_buffer buf;

	CLEAR(buf);

	buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory      = V4L2_MEMORY_MMAP;
	buf.index       = 0;

	if (-1 == xioctl(fd, VIDIOC_QUERYBUF, &buf))
		errno_exit_("VIDIOC_QUERYBUF");

	buffer_.length = buf.length;
	buffer_.start = (char *)
		mmap(NULL /* start anywhere */,
		      buf.length,
		      PROT_READ | PROT_WRITE /* required */,
		      MAP_SHARED /* recommended */,
		      fd, buf.m.offset);

	if (MAP_FAILED == buffer_.start) errno_exit_("mmap");
}

static void init_device(void)
{
        struct v4l2_capability cap;
        struct v4l2_cropcap cropcap;
        struct v4l2_crop crop;
        struct v4l2_format fmt;
        unsigned int min;

        if (-1 == xioctl(fd, VIDIOC_QUERYCAP, &cap)) {
                if (EINVAL == errno) {
                        fprintf(stderr, "%s is no V4L2 device\n",
                                 dev_name);
                        exit_(EXIT_FAILURE);
                } else {
                        errno_exit_("VIDIOC_QUERYCAP");
                }
        }

        if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
                fprintf(stderr, "%s is no video capture device\n",
                         dev_name);
                exit_(EXIT_FAILURE);
        }

	if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
		fprintf(stderr, "%s does not support streaming i/o\n",
			 dev_name);
		exit_(EXIT_FAILURE);
	}


        /* Select video input, video standard and tune here. */


        CLEAR(cropcap);

        cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

        if (0 == xioctl(fd, VIDIOC_CROPCAP, &cropcap)) {
                crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                crop.c = cropcap.defrect; /* reset to default */

                if (-1 == xioctl(fd, VIDIOC_S_CROP, &crop)) {
                        switch (errno) {
                        case EINVAL:
                                /* Cropping not supported. */
                                break;
                        default:
                                /* Errors ignored. */
                                break;
                        }
                }
        } else {
                /* Errors ignored. */
        }


        CLEAR(fmt);

        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width       = CAPTURE_W;
	fmt.fmt.pix.height      = CAPTURE_H;
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
	//fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;

	if (-1 == xioctl(fd, VIDIOC_S_FMT, &fmt))
		errno_exit_("VIDIOC_S_FMT");

	/* Note VIDIOC_S_FMT may change width and height. */

        /* Buggy driver paranoia. */
        min = fmt.fmt.pix.width * 2;
        if (fmt.fmt.pix.bytesperline < min)
                fmt.fmt.pix.bytesperline = min;
        min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
        if (fmt.fmt.pix.sizeimage < min)
                fmt.fmt.pix.sizeimage = min;

	init_mmap();
}

static void open_device(void)
{
        struct stat st;

        if (-1 == stat(dev_name, &st)) {
                fprintf(stderr, "Cannot identify '%s': %d, %s\n",
                         dev_name, errno, strerror(errno));
                exit_(EXIT_FAILURE);
        }

        if (!S_ISCHR(st.st_mode)) {
                fprintf(stderr, "%s is no device\n", dev_name);
                exit_(EXIT_FAILURE);
        }

        fd = open(dev_name, O_RDWR /* required */ | O_NONBLOCK, 0);

        if (-1 == fd) {
                fprintf(stderr, "Cannot open '%s': %d, %s\n",
                         dev_name, errno, strerror(errno));
                exit_(EXIT_FAILURE);
        }
}

#ifndef V4LCAPTURE_INCLUDE

static void stop_capturing(void)
{
        enum v4l2_buf_type type;

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (-1 == xioctl(fd, VIDIOC_STREAMOFF, &type))
		errno_exit_("VIDIOC_STREAMOFF");
}

static void uninit_device(void)
{
	if (-1 == munmap(buffer_.start, buffer_.length)) errno_exit_("munmap");
}

static void close_device(void)
{
        if (-1 == close(fd))
                errno_exit_("close");

        fd = -1;
}

static void usage(FILE *fp, int argc, char **argv)
{
        fprintf(fp,
                 "Usage: %s [options]\n\n"
                 "Version 1.3\n"
                 "Options:\n"
                 "-d | --device name   Video device name [%s]\n"
                 "-h | --help          Print this message\n"
                 "",
                 argv[0], dev_name);
}

static const char short_options[] = "d:hmruofc:";

static const struct option
long_options[] = {
        { "device", required_argument, NULL, 'd' },
        { "help",   no_argument,       NULL, 'h' },
        { }
};

int main(int argc, char **argv)
{
        dev_name = "/dev/video0";

        for (;;) {
                int idx;
                int c;

                c = getopt_long(argc, argv,
                                short_options, long_options, &idx);

                if (-1 == c)
                        break;

                switch (c) {
                case 0: /* getopt_long() flag */
                        break;

                case 'd':
                        dev_name = optarg;
                        break;

                case 'h':
                        usage(stdout, argc, argv);
                        exit_(EXIT_SUCCESS);

                default:
                        usage(stderr, argc, argv);
                        exit_(EXIT_FAILURE);
                }
        }

        open_device();
        init_device();
        start_capturing();
        mainloop();
        stop_capturing();
        uninit_device();
        close_device();
        fprintf(stderr, "\n");
        return 0;
}
#endif // V4LCAPTURE_INCLUDE
#endif // __linux__
