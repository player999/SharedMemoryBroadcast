CFLAGS=`pkg-config --cflags gstreamer-0.10`
LDFLAGS=`pkg-config --libs gstreamer-0.10`
all: clean
	gcc -o streaming $(CFLAGS) ./streaming.c $(LDFLAGS)

clean:
	rm -f streaming