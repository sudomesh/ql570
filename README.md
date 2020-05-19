# About #

This program prints 1083 by 336 pixel monochrome PNG files on the Brother QL-570 thermal sticker printer by writing to the Linux device file (e.g. /dev/usb/lp0).

Also tested with model QL-700 and DK 29mm x 90.3mm non-continuous sample labels

# Prerequisites #

You need libpng and the libpng header files in order to compile. To install the required packages on a Debiab-based system:

```
sudo aptitude install build-essential libpng12-0 libpng12-dev pkg-config
```

# Compiling #

Simply run:

```
make
```

# Usage #

```
Usage: ./ql570 printer format pngfile [cutoff]
  format:
  - 12      12 mm endless (DK-22214)
  - 29
    n       29 mm endless (DK-22210, 22211)
  - 38      38 mm endless (DK-22225)
  - 50      50 mm endless (DK-22246)
  - 54      54 mm endless (DK-N55224)
  - 62
    w       62 mm endless (DK-22205, 44205, 44605, 22212, 22251, 22606, 22113)
  - 12d     Ø 12 mm round (DK-11219)
  - 24d     Ø 24 mm round (DK-11218)
  - 58d     Ø 58 mm round (DK-11207)
  - 17x54   17x54 mm      (DK-11204)
  - 17x87   17x87 mm      (DK-11203)
  - 23x23   23x23 mm      (DK-11221)
  - 29x90
    7       29x90 mm      (DK-11201)
  - 38x90   38x90 mm      (DK-11208)
  - 39x48   39x48 mm
  - 52x29   52x29 mm
  - 62x29   62x29 mm      (DK-11209)
  - 62x100  62x100 mm     (DK-11202)
  [cutoff] is the optional color/greyscale to monochrome conversion cutoff (default: 180).
  Example: ./ql570 /dev/usb/lp0 n image.png
  Hint: If the printer's status LED blinks red, then your media type is probably wrong.
```

If you try to print a greyscale or monochrome PNG, then it will be converted to monochrome before printing. The image is converted by turning all pixels that have a value of less than 180 out of 255 in either color channel to black, and the rest to white. The cutoff can be adjusted to something other than 180 by specifying it on the command line.

# Credit #

Asbjørn Sloth Tønnesen reverse-engineered the QL-570 protocol and wrote all the important parts of this program.

Marc Juul added support for different paper sizes and conversion cutoff.

Luca Zimmermann added support for mostly all paper sizes and option for print quality vs. speed.
