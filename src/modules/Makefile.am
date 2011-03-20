common_ldflags = -module

sequencer_la_SOURCES = sequencer.c
sequencer_la_LDFLAGS = $(common_ldflags)

comedi_la_SOURCES = comedi.c
comedi_la_LDFLAGS = $(common_ldflags)
comedi_la_LIBADD = $(COMEDILIBS)

math_la_SOURCES = math.c
math_la_LDFLAGS = $(common_ldflags)
math_la_LIBADD = -lm

fpga_la_SOURCES = fpga.c
fpga_la_LDFLAGS = $(common_ldflags)

usb_spi_la_SOURCES = usb_spi.c
usb_spi_la_CFLAGS = -Ilibsub -D__SUB_STATIC__
usb_spi_la_LDFLAGS = $(common_ldflags) -Llibsub
usb_spi_la_LIBADD = -lusb-1.0 -lsub -lm

if HAVE_LIBUSB
SUBDIRS=libsub
endif

if HAVE_COMEDI
COMEDI = comedi.la
endif

if HAVE_SEQUENCER
SEQ = sequencer.la
endif

if HAVE_LIBUSB
USB_SPI = usb_spi.la
endif

MODULES = math.la fpga.la $(SEQ) $(COMEDI) $(USB_SPI) 

pkglib_LTLIBRARIES = $(MODULES)
