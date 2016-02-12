/*
 * Brother QL-570 thermal printing program
 * It prints a mono PNG image directly to the printers block device
 *
 * Copyright 2011 Asbjørn Sloth Tønnesen <code@asbjorn.it>
 * Copyright 2013 Marc Juul <juul@labitat.dk>
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

#define PNG_DEBUG 3
#include <png.h>

#define ESC (0x1b)
#define PAPER_WIDTH_NARROW (29)
#define PAPER_WIDTH_WIDE (62)
#define PAPER_QL700 (-1)

#define DEBUG

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

void ql570_print(pngdata_t * img, unsigned int paper_width)
{

	/* Init */
	fprintf(fp, "%c%c", ESC, '@');

	/* Set media type */
	if( paper_width == PAPER_QL700 ) {
		// sample roll that ships with QL700
		// 1BH, 69H, 7AH, 0EH, 0BH, 1DH, 5AH, DFH, 03H, 00H, 00H, 00H, 00H
		fprintf(fp, "%c%c%c%c%c%c%c%c%c%c%c%c%c", ESC, 'i', 'z', 0x0e, 0x0b, 0x1d, 0x5a, 0xdf, 0x03, 0, 0, 0, 0);
	} else {
//  	fprintf(fp, "%c%c%c%c%c%c%c%c%c%c%c%c%c", ESC, 'i', 'z', 0xa6, 0x0a, 29, 0, img->h & 0xff, img->h >> 8, 0, 0, 0, 0);
		fprintf(fp, "%c%c%c%c%c%c%c%c%c%c%c%c%c", ESC, 'i', 'z', 0xa6, 0x0a, paper_width, 0, img->h & 0xff, img->h >> 8, 0, 0, 0, 0);
	}
	
	/* Set cut type */
  //	fprintf(fp, "%c%c%c%c", ESC, 'i', 'K', 8);
  fprintf(fp, "%c%c%c", ESC, 'i', 'K');

	/* Enable cutter */
  //	fprintf(fp, "%c%c%c%c", ESC, 'i', 'A', 1);
	fprintf(fp, "%c%c%c", ESC, 'i', 'A');

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
        unsigned char header[8];    // 8 is the maximum size that can be checked
	png_byte* ptr;
	FILE *fp;
	int type = 0;
	int lb;
	uint8_t * bitmap;

        /* open file and test for it being a png */
        fp = fopen(path, "rb");
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
        //        if ((ptr[0] & (1 << (7-(x%8)))) == 0) {
        //          heat_on(x, y);
        ///        }
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
		fprintf(stderr, "Usage: %s printer n|w|7 pngfile [cutoff]\n", cmd);
    fprintf(stderr, "  Where 'n' is narrow paper (29 mm) and 'w' is wide paper (62 mm) and '7'\n");
	fprintf(stderr, "  is the 1.1\" x 3.5\" sample labels that ship with the QL-700.\n");
    fprintf(stderr, "  [cutoff] is the optional color/greyscale to monochrome conversion cutoff (default: 180).\n");
    fprintf(stderr, "  Example: %s /dev/usb/lp0 n image.png\n", cmd);
    fprintf(stderr, "  Hint: If the printer's status LED blinks red, then your media type is probably wrong.\n");
		exit(EXIT_FAILURE);
}

int main(int argc, const char ** argv) {

  int cutoff = 180;

	if ((argc < 4) || (argc > 5)) {
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
  if(argv[2][0] == 'n') {
    paper_type = PAPER_WIDTH_NARROW;
  } else if(argv[2][0] == 'w') {
    paper_type = PAPER_WIDTH_WIDE;
  } else if(argv[2][0] == '7') {
    paper_type = PAPER_QL700;
  } else {
    usage(argv[0]);
  }

	ql570_open(argv[1]);
	pngdata_t * data = loadpng(argv[3], cutoff);
  //check_img(data);
	printf("Printing image with width: %d\t and height: %d\n", data->w, data->h);
	//check_img(data);
	ql570_print(data, paper_type);
	return EXIT_SUCCESS;
}
