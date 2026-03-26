.PHONY: clean win dc

#Windows
win:
	C:/raylib/w64devkit/bin/mingw32-make -f platform/windows/Makefile

#Dreamcast
dc:
	$(MAKE) -f platform/dc/Makefile

clean:
	C:/raylib/w64devkit/bin/mingw32-make -f platform/windows/Makefile clean
	$(MAKE) -f platform/dc/Makefile clean
