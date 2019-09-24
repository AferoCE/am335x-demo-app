#  Copyright (c) 2016 Afero, Inc. All rights reserved.

APP_LIBS_NEEDED :=   -lpthread -levent_pthreads -levent -laf_util -laf_edge -laf_ipc -laf_attr
#APP_LIBS_NEEDED :=  -lpthread -levent_pthreads -levent -laf_util -laf_edge -laf_attr -laf_ipc

default: all

all: app

app: test_aflib_edge.c
	$(CC) $(CFLAGS)  -L $(APP_LIBS_NEEDED) -L $(APP_LIBS_NEEDED) -o app test_aflib_edge.c 

#$(OBJ): %.o : %.c $(DEPS)
#	$(CC) -c -o $@ $< $(CFLAGS)
#
#app: test.o
#	$(CC) $(CFLAGS) -o app -I ../attrd/pkg/src/ -I ../attrd/pkg/oe-workdir/image/usr/lib test.o $(APP_LIBS_NEEDED)
#
#
clean veryclean:
	$(RM) app
# my make file goes here
