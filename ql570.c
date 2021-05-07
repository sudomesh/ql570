/*
 * Brother QL-570 thermal printing program
 * It prints a mono PNG image directly to the printers block device
 *
 * Copyright 2011 Asbjørn Sloth Tønnesen <code@asbjorn.it>
 * Copyright 2013 Marc Juul <juul@labitat.dk>
 * Copyright 2020 Luca Zimmermann <git@sndstrm.de>
 *
 * This code is liensed under the GPLv3
 *
 * Example usage:
 *   ./ql570 /dev/usb/lp0 image.png
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>

#define PNG_DEBUG 3
#include <png.h>

#define ESC (0x1b)

const unsigned char PAPER_12DIA[4]  = {0x0c, 0x0c, 0x5e, 0x00};
const unsigned char PAPER_24DIA[4]  = {0x18, 0x18, 0xec, 0x00};
const unsigned char PAPER_58DIA[4]  = {0x3a, 0x3a, 0x6a, 0x02};
const unsigned char PAPER_17X54[4]  = {0x11, 0x36, 0x36, 0x02};
const unsigned char PAPER_17X87[4]  = {0x11, 0x57, 0xbc, 0x03};
const unsigned char PAPER_23X23[4]  = {0x17, 0x17, 0xca, 0x00};
const unsigned char PAPER_29X90[4]  = {0x1d, 0x5a, 0xdf, 0x03};
const unsigned char PAPER_38X90[4]  = {0x26, 0x5a, 0xdf, 0x03};
const unsigned char PAPER_39X48[4]  = {0x27, 0x30, 0xef, 0x01};
const unsigned char PAPER_52X29[4]  = {0x34, 0x1d, 0x0f, 0x01};
const unsigned char PAPER_62X29[4]  = {0x3e, 0x1d, 0x0f, 0x01};
const unsigned char PAPER_62X100[4] = {0x3e, 0x64, 0x55, 0x04};
const int           INDIVIDUAL      = -1;
// 0 is fast but low quality, while 1 is high quality with slower print
const _Bool         QUALITY         = 1;

#define CUTTING_EVERY 1 //Cut every n label

#define CUTTER_ON 0x08

#define DEBUG

/*
 * DATA FORMAT
 * ESC i z F1 F2 WIDTH_MM MEDIALENGTH_MM PRINTLENGHT_PX_LSB PRINTLENGHT_PX_MSB 00 00 PAGE_LSB PAGE_MSB
 * MEDIALENGTH_MM is only set if not endless
 * PRINTLENGTH in respect of set dpi
 * F1:
 * 80H (seems to be always 1 for me)
 * 40H Quality>Speed (1) / Speed>Quality (0)
 * 20H
 * 10H
 * 08H (seems to be always 1 for me)
 * 04H (seems to be always 1 for me)
 * 02H (seems to be always 1 for me)
 * 01H
 * F2:
 * 80H
 * 40H
 * 20H
 * 10H
 * 08H
 * 04H
 * 02H
 * 01H Specific Length (1) / Endless (0)
 *
 * ESC 'i' 'd' XX with XX:
 * 80H
 * 40H Cutting between lables on (1) / off (0)
 * 20H
 * 10H
 * 08H
 * 04H
 * 02H
 * 01H
 *
 * ESC 'i' 'M' XX (XX = 35D/23H 300dpi, 70D/46H for 600dpi)
 *
 * ESC 'i' 'A' XX (XX = cut between every n pages; 01H, 02H, ..., 63H, ...?)
 *
 * ESC 'i' 'K' XX with XX:
 * 80H
 * 40H 600dpi (1) / 300dpi (0)
 * 20H
 * 10H
 * 08H Cutting at end on (1) / off (0)
 * 04H
 * 02H
 */

FILE * fp;

struct {
	int16_t w;
	int16_t h;
	uint8_t * data;
} typedef pngdata_t;

FILE * ql570_open(const char * path)
{
	fp = fopen(path, "r+b");
	if (fp == NULL) {
		perror("fopen");
		exit(EXIT_FAILURE);
	}
	return fp;
}

void check_img(pngdata_t * img)
{
	int i, j;
	int lb = (img->w / 8) + (img->w % 8 > 0);
	printf("lb: %d\n", lb);
	for (i=0;i<img->h;i++) {
		for (j=img->w-1;j!=0;j--) {
			if (img->data[i*lb+j/8] & (1 << (7-(j % 8)))) {
				printf("#");
			} else {
				printf(" ");
			}
		}
		printf("\n");
	}

}

/*


0000000000000000000000000000000000000000000000000000000000000000
1b40 ESC, @ // init
1b6953 ESC, i, S // ?
1b 69 7a 00 0a 3e 00 e5 00 00 00 00 00 ESC, i, z, 0, 0a, 62, 0, 229, 0, 0, 0, 0, 0 // set media for wide paper
1b694d40 ESC, i, [M, @] or [77, 64] // ?
1b694101 ESC, i, [A, 1] or [65, 1] // enable cutter
1b694b08 ESC, i, [K, 8] or [75, 8] // set cut type
1b696424006700005a00000000000000000000000000000000000000000000000000000000000000000000000000000000000070700e0000381c070000000000000000000000000000000000000000000000000000000000000000000000000000000000006700
*/

void ql570_print(pngdata_t * img, unsigned int paper_type, unsigned char paper_params[4])
{

	/* Init */
	fprintf(fp, "%c%c", ESC, '@');

	/* Set media type */
	if (paper_type == INDIVIDUAL) {
		fprintf(fp, "%c%c%c%c%c%c%c%c%c%c%c%c%c", ESC, 'i', 'z', 0x0e | (QUALITY * 0x40), 0x0b, paper_params[0], paper_params[1], paper_params[2], paper_params[3], 0, 0, 0, 0);
	} else {
		fprintf(fp, "%c%c%c%c%c%c%c%c%c%c%c%c%c", ESC, 'i', 'z', 0xa6 | (QUALITY * 0x40), 0x0a, paper_type, 0, img->h & 0xff, img->h >> 8, 0, 0, 0, 0);
	}

	/* Set cut type */
  	fprintf(fp, "%c%c%c%c", ESC, 'i', 'K', CUTTER_ON);

	/* Enable cutter */
	fprintf(fp, "%c%c%c%c", ESC, 'i', 'A', CUTTING_EVERY);

	/* Set margin = 0 */
	fprintf(fp, "%c%c%c%c%c", ESC, 'i', 'd', 0, 0);

	int i, j;
	int lb = (img->w / 8) + (img->w % 8 > 0);
	for (i=0;i<img->h;i++) {
		fprintf(fp, "%c%c%c", 'g', 0x00, 90);
		for (j=0;j<lb;j++) {
			fprintf(fp, "%c", img->data[i*lb+j]);
		}
		for (;j<90;j++) {
			fprintf(fp, "%c", 0x00);
		}
	}

	/* Print */
	fprintf(fp, "%c", 0x1a);
}

void abort_(const char * s, ...)
{
	va_list args;
	va_start(args, s);
	vfprintf(stderr, s, args);
	fprintf(stderr, "\n");
	va_end(args);
	abort();
}

pngdata_t * loadpng(const char * path, int cutoff) {

	png_structp png_ptr;
	png_infop info_ptr;
	png_bytep * row_pointers;
	int width, height, rowbytes;
	int x, y;
	unsigned char header[8]; // 8 is the maximum size that can be checked
	png_byte* ptr;
	FILE *fp;
	int type = 0;
	int lb;
	uint8_t * bitmap;

	/* open file and test for it being a png */
	if(!strcmp(path,"-")) fp=stdin;
	else fp = fopen(path, "rb");
	if (!fp)
		abort_("[read_png_file] File %s could not be opened for reading", path);
	fread(header, 1, 8, fp);
	if (png_sig_cmp(header, 0, 8))
		abort_("[read_png_file] File %s is not recognized as a PNG file", path);

	/* initialize stuff */
	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

	if (!png_ptr)
		abort_("[read_png_file] png_create_read_struct failed");

	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr)
		abort_("[read_png_file] png_create_info_struct failed");

	if (setjmp(png_jmpbuf(png_ptr)))
		abort_("[read_png_file] Error during init_io");

	png_init_io(png_ptr, fp);
	png_set_sig_bytes(png_ptr, 8);

	png_read_info(png_ptr, info_ptr);

	if (png_get_channels(png_ptr, info_ptr) == 1
	    && png_get_bit_depth(png_ptr, info_ptr) == 1) {
		type = 1;
	} else if (png_get_channels(png_ptr, info_ptr) == 4
	    && png_get_bit_depth(png_ptr, info_ptr) == 8) {
		type = 2;
	}

	if (type == 0) {
		fprintf(stderr, "Invalid PNG! Only mono or 4x8-bit RGBA PNG files are allowed\n");
		exit(1);
	}

	width = png_get_image_width(png_ptr, info_ptr);
	height = png_get_image_height(png_ptr, info_ptr);

	png_read_update_info(png_ptr, info_ptr);

	/* read file */
	if (setjmp(png_jmpbuf(png_ptr)))
		abort_("[read_png_file] Error during read_image");

	row_pointers = (png_bytep*) malloc(sizeof(png_bytep) * height);

	rowbytes = type == 1 ? (width / 8) + (width % 8 > 0) : width << 2;

	for (y=0; y<height; y++)
		row_pointers[y] = (png_byte*) malloc(rowbytes);

	png_read_image(png_ptr, row_pointers);

	fclose(fp);

	lb = (height / 8) + (height % 8 > 0);
	bitmap = malloc(width*lb);
	memset(bitmap, 0, width*lb);

	#define heat_on(x, y) bitmap[(x*lb)+(y/8)] |= 1 << (7-(y%8))

	for (y=0; y<height; y++) {
		png_byte* row = row_pointers[y];
		for (x=0; x<width; x++) {
			if (type == 1) {
				ptr = &(row[x/8]);
				if ((ptr[0] & (1 << (7-(x%8)))) == 0) {
					heat_on(x, y);
				}
			} else {
				// Color 0 (red?)
				ptr = &(row[(x<<2)]);
				if (ptr[0] < cutoff) { // Adjust the cutoff (0 to 255). Any pixel darker (lower) than this will be black. Higher will be white. This can be used to turn anti-aliased pngs into monochrome with some desirable cutoff for black and white.
					heat_on(x, y);
				}
				// Color 1 (green?)
				ptr = &(row[(x<<2)+1]);
				if (ptr[0] < cutoff) {
					heat_on(x, y);
				}
				// Color 2 (blue?)
				ptr = &(row[(x<<2)+2]);
				if (ptr[0] < cutoff) {
					heat_on(x, y);
				}
				//if ((ptr[0] & (1 << (7-(x%8)))) == 0) {
				//	heat_on(x, y);
				//}
			}
		}
	}
	/*
	for (y=0; y<height; y++) {
		png_byte* row = row_pointers[y+height*2];
		for (x=0; x<width; x++) {
			ptr = &(row[(x<<2)]);
			if ((ptr[0] & (1 << (7-(x%8)))) == 0) {
				heat_on(x, y+height*2);
			}
		}
	}
	*/

	pngdata_t * ret = malloc(sizeof(pngdata_t));
	ret->w = height;
	ret->h = width;
	ret->data = bitmap;
	return ret;
}

void usage(const char* cmd) {
	fprintf(stderr, "Usage: %s printer format [pngfile] [cutoff]\n", cmd);
	fprintf(stderr, "  format:\n");
	fprintf(stderr, "  - 12      12 mm endless (DK-22214)\n");
	fprintf(stderr, "  - 29\n");
	fprintf(stderr, "    n       29 mm endless (DK-22210, 22211)\n");
	fprintf(stderr, "  - 38      38 mm endless (DK-22225)\n");
	fprintf(stderr, "  - 50      50 mm endless (DK-22246)\n");
	fprintf(stderr, "  - 54      54 mm endless (DK-N55224)\n");
	fprintf(stderr, "  - 62\n");
	fprintf(stderr, "    w       62 mm endless (DK-22205, 44205, 44605, 22212, 22251, 22606, 22113)\n");
	fprintf(stderr, "  - 12d     Ø 12 mm round (DK-11219)\n");
	fprintf(stderr, "  - 24d     Ø 24 mm round (DK-11218)\n");
	fprintf(stderr, "  - 58d     Ø 58 mm round (DK-11207)\n");
	fprintf(stderr, "  - 17x54   17x54 mm      (DK-11204)\n");
	fprintf(stderr, "  - 17x87   17x87 mm      (DK-11203)\n");
	fprintf(stderr, "  - 23x23   23x23 mm      (DK-11221)\n");
	fprintf(stderr, "  - 29x90\n");
	fprintf(stderr, "    7       29x90 mm      (DK-11201)\n");
	fprintf(stderr, "  - 38x90   38x90 mm      (DK-11208)\n");
	fprintf(stderr, "  - 39x48   39x48 mm\n");
	fprintf(stderr, "  - 52x29   52x29 mm\n");
	fprintf(stderr, "  - 62x29   62x29 mm      (DK-11209)\n");
	fprintf(stderr, "  - 62x100  62x100 mm     (DK-11202)\n");
	fprintf(stderr, "  [cutoff] is the optional color/greyscale to monochrome conversion cutoff (default: 180).\n");
	fprintf(stderr, "  Example: %s /dev/usb/lp0 n image.png\n", cmd);
	fprintf(stderr, "           qrencode -s10 -m1 \"test\" -o -|%s /dev/usb/lp0 n; # read qrencode output from stdin\n", cmd);
	fprintf(stderr, "  Hint: If the printer's status LED blinks red, then your media type is probably wrong.\n");
	exit(EXIT_FAILURE);
}

void ql570_ping() {
	fprintf(fp, "%c", 0x00);
}

int main(int argc, const char ** argv) {
	int cutoff = 180;
	pngdata_t *data;

	if ((argc < 3) || (argc > 5)) {
		usage(argv[0]);
		return -1;
	}
	if(argc == 5) {
		cutoff = atoi(argv[4]);
		if(cutoff <= 0) {
			fprintf(stderr, "Error: Setting cutoff to 0 will result in an all-white image.\n");
			exit(EXIT_FAILURE);
		}
		if(cutoff >= 255) {
			fprintf(stderr, "Error: Setting cutoff to 255 or higher will result in an all-black image.\n");
			exit(EXIT_FAILURE);
		}
	}

	int paper_type;
	unsigned char paper_param[4];
	if (!strcmp(argv[2], "12")) {
		paper_type = 12;
	} else if (!strcmp(argv[2], "n") || !strcmp(argv[2], "29")) {
		paper_type = 29;
	} else if (!strcmp(argv[2], "38")) {
		paper_type = 38;
	} else if (!strcmp(argv[2], "50")) {
		paper_type = 50;
	} else if (!strcmp(argv[2], "54")) {
		paper_type = 54;
	} else if (!strcmp(argv[2], "w") || !strcmp(argv[2], "62")) {
		paper_type = 62;
	} else if (!strcmp(argv[2], "12d")) {
		paper_type = INDIVIDUAL;
		memcpy(paper_param, PAPER_12DIA, 4);
	} else if (!strcmp(argv[2], "24d")) {
		paper_type = INDIVIDUAL;
		memcpy(paper_param, PAPER_24DIA, 4);
	} else if (!strcmp(argv[2], "58d")) {
		paper_type = INDIVIDUAL;
		memcpy(paper_param, PAPER_58DIA, 4);
	} else if (!strcmp(argv[2], "17x54")) {
		paper_type = INDIVIDUAL;
		memcpy(paper_param, PAPER_17X54, 4);
	} else if (!strcmp(argv[2], "17x87")) {
		paper_type = INDIVIDUAL;
		memcpy(paper_param, PAPER_17X87, 4);
	} else if (!strcmp(argv[2], "23x23")) {
		paper_type = INDIVIDUAL;
		memcpy(paper_param, PAPER_23X23, 4);
	} else if (!strcmp(argv[2], "7") || !strcmp(argv[2], "29x90")) {
		paper_type = INDIVIDUAL;
		memcpy(paper_param, PAPER_29X90, 4);
	} else if (!strcmp(argv[2], "38x90")) {
		paper_type = INDIVIDUAL;
		memcpy(paper_param, PAPER_38X90, 4);
	} else if (!strcmp(argv[2], "39x48")) {
		paper_type = INDIVIDUAL;
		memcpy(paper_param, PAPER_39X48, 4);
	} else if (!strcmp(argv[2], "52x29")) {
		paper_type = INDIVIDUAL;
		memcpy(paper_param, PAPER_52X29, 4);
	} else if (!strcmp(argv[2], "62x29")) {
		paper_type = INDIVIDUAL;
		memcpy(paper_param, PAPER_62X29, 4);
	} else if (!strcmp(argv[2], "62x29")) {
		paper_type = INDIVIDUAL;
		memcpy(paper_param, PAPER_62X100, 4);
	} else if (!strcmp(argv[2], "ping")) {
		ql570_open(argv[1]);
		ql570_ping();
		return EXIT_SUCCESS;
	} else {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	ql570_open(argv[1]);
	if(argc<4){
		printf("Loading image from stdin\n");
		data = loadpng("-", cutoff);
	} else data = loadpng(argv[3], cutoff);
	//check_img(data);
	printf("Printing image with width: %d\t and height: %d\n", data->w, data->h);
	//check_img(data);
	ql570_print(data, paper_type, paper_param);
	return EXIT_SUCCESS;
}

