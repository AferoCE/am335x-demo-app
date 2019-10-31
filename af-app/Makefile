#  Copyright (c) 2016 Afero, Inc. All rights reserved.

APP_LIBS_NEEDED :=   -lpthread -levent_pthreads -levent -laf_util -laf_edge -laf_ipc -laf_attr

default: all

all: app

app: my_app.c device-description.h
	$(CC) $(CFLAGS)  -L $(APP_LIBS_NEEDED) -L $(APP_LIBS_NEEDED) -o app my_app.c 

clean veryclean:
	$(RM) app
# my make file goes here
