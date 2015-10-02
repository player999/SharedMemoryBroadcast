CFLAGS=`pkg-config --cflags gstreamer-0.10`
LDFLAGS=`pkg-config --libs gstreamer-0.10` -lrt
all: clean
	gcc -o streaming $(CFLAGS) ./streaming.c $(LDFLAGS)
	gcc -o test_streaming $(CFLAGS) ./test_streaming.c $(LDFLAGS)

clean:
	rm -f streaming
	rm -f test_streaming
