#!/binb/wmake.exe

# This file was automatically generated by tmake 
# DO NOT CHANGE THIS FILE, YOUR CHANGES WILL BE LOST! CHANGE WATBASE.T!

##############################################################################
# Name:         makebase.wat
# Purpose:      Builds wxBase library for Watcom C++ under Win32
# Author:       Vadim Zeitlin
# Created:      21.01.03
# RCS-ID:       $Id$
# Copyright:    (c) 2003 Vadim Zeitlin <vadim@wxwindows.org>
# Licence:      wxWindows licence
##############################################################################

wxUSE_GUI=0

!include ..\makewat.env

LIBTARGET   = $(WXDIR)\lib\$(LIBNAME).lib

EXTRATARGETS = zlib regex
EXTRATARGETSCLEAN = clean_zlib clean_regex
COMMDIR=$(WXDIR)\src\common
MSWDIR=$(WXDIR)\src\msw
OLEDIR=$(MSWDIR)\ole

COMMONOBJS = &
	$(OUTPUTDIR)\appcmn.obj &
	$(OUTPUTDIR)\clntdata.obj &
	$(OUTPUTDIR)\cmdline.obj &
	$(OUTPUTDIR)\config.obj &
	$(OUTPUTDIR)\datetime.obj &
	$(OUTPUTDIR)\datstrm.obj &
	$(OUTPUTDIR)\db.obj &
	$(OUTPUTDIR)\dbtable.obj &
	$(OUTPUTDIR)\dircmn.obj &
	$(OUTPUTDIR)\dynarray.obj &
	$(OUTPUTDIR)\dynlib.obj &
	$(OUTPUTDIR)\dynload.obj &
	$(OUTPUTDIR)\encconv.obj &
	$(OUTPUTDIR)\event.obj &
	$(OUTPUTDIR)\extended.obj &
	$(OUTPUTDIR)\ffile.obj &
	$(OUTPUTDIR)\file.obj &
	$(OUTPUTDIR)\fileconf.obj &
	$(OUTPUTDIR)\filefn.obj &
	$(OUTPUTDIR)\filename.obj &
	$(OUTPUTDIR)\filesys.obj &
	$(OUTPUTDIR)\fontmap.obj &
	$(OUTPUTDIR)\fs_inet.obj &
	$(OUTPUTDIR)\fs_mem.obj &
	$(OUTPUTDIR)\fs_zip.obj &
	$(OUTPUTDIR)\ftp.obj &
	$(OUTPUTDIR)\hash.obj &
	$(OUTPUTDIR)\hashmap.obj &
	$(OUTPUTDIR)\http.obj &
	$(OUTPUTDIR)\intl.obj &
	$(OUTPUTDIR)\ipcbase.obj &
	$(OUTPUTDIR)\list.obj &
	$(OUTPUTDIR)\log.obj &
	$(OUTPUTDIR)\longlong.obj &
	$(OUTPUTDIR)\memory.obj &
	$(OUTPUTDIR)\mimecmn.obj &
	$(OUTPUTDIR)\module.obj &
	$(OUTPUTDIR)\msgout.obj &
	$(OUTPUTDIR)\mstream.obj &
	$(OUTPUTDIR)\object.obj &
	$(OUTPUTDIR)\process.obj &
	$(OUTPUTDIR)\protocol.obj &
	$(OUTPUTDIR)\regex.obj &
	$(OUTPUTDIR)\sckaddr.obj &
	$(OUTPUTDIR)\sckfile.obj &
	$(OUTPUTDIR)\sckipc.obj &
	$(OUTPUTDIR)\sckstrm.obj &
	$(OUTPUTDIR)\socket.obj &
	$(OUTPUTDIR)\strconv.obj &
	$(OUTPUTDIR)\stream.obj &
	$(OUTPUTDIR)\string.obj &
	$(OUTPUTDIR)\sysopt.obj &
	$(OUTPUTDIR)\textbuf.obj &
	$(OUTPUTDIR)\textfile.obj &
	$(OUTPUTDIR)\timercmn.obj &
	$(OUTPUTDIR)\tokenzr.obj &
	$(OUTPUTDIR)\txtstrm.obj &
	$(OUTPUTDIR)\unzip.obj &
	$(OUTPUTDIR)\url.obj &
	$(OUTPUTDIR)\utilscmn.obj &
	$(OUTPUTDIR)\variant.obj &
	$(OUTPUTDIR)\wfstream.obj &
	$(OUTPUTDIR)\wxchar.obj &
	$(OUTPUTDIR)\zipstrm.obj &
	$(OUTPUTDIR)\zstream.obj &
	$(OUTPUTDIR)\init.obj

MSWOBJS = &
	$(OUTPUTDIR)\dde.obj &
	$(OUTPUTDIR)\dir.obj &
	$(OUTPUTDIR)\gsocket.obj &
	$(OUTPUTDIR)\gsockmsw.obj &
	$(OUTPUTDIR)\main.obj &
	$(OUTPUTDIR)\mimetype.obj &
	$(OUTPUTDIR)\regconf.obj &
	$(OUTPUTDIR)\registry.obj &
	$(OUTPUTDIR)\snglinst.obj &
	$(OUTPUTDIR)\thread.obj &
	$(OUTPUTDIR)\utils.obj &
	$(OUTPUTDIR)\utilsexc.obj &
	$(OUTPUTDIR)\volume.obj

OBJECTS = $(COMMONOBJS) $(MSWOBJS)

SETUP_H=$(ARCHINCDIR)\wx\setup.h

all: $(SETUP_H) $(OUTPUTDIR) $(OBJECTS) $(LIBTARGET) $(EXTRATARGETS) .SYMBOLIC

$(ARCHINCDIR)\wx:
	mkdir $(ARCHINCDIR)
	mkdir $(ARCHINCDIR)\wx

$(OUTPUTDIR):
	mkdir $(OUTPUTDIR)

$(SETUP_H): $(WXDIR)\include\wx\msw\setup.h $(ARCHINCDIR)\wx
	copy $(WXDIR)\include\wx\msw\setup.h $@

LBCFILE=$(OUTPUTDIR)\wx$(TOOLKIT).lbc
$(LIBTARGET) : $(OBJECTS)
    %create $(LBCFILE)
    @for %i in ( $(OBJECTS) ) do @%append $(LBCFILE) +%i
    wlib /q /b /c /n /p=512 $^@ @$(LBCFILE)


clean:   .SYMBOLIC $(EXTRATARGETSCLEAN)
    -erase $(OUTPUTDIR)\*.obj
    -erase $(LIBTARGET)
    -erase $(OUTPUTDIR)\*.pch
    -erase $(OUTPUTDIR)\*.err
    -erase $(OUTPUTDIR)\*.lbc

cleanall:   clean
    -erase $(LBCFILE)

$(OUTPUTDIR)\dde.obj:     $(MSWDIR)\dde.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\dir.obj:     $(MSWDIR)\dir.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\gsocket.obj:     $(MSWDIR)\gsocket.c
  *$(CC) $(CFLAGS) $<

$(OUTPUTDIR)\gsockmsw.obj:     $(MSWDIR)\gsockmsw.c
  *$(CC) $(CFLAGS) $<

$(OUTPUTDIR)\main.obj:     $(MSWDIR)\main.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\mimetype.obj:     $(MSWDIR)\mimetype.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\regconf.obj:     $(MSWDIR)\regconf.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\registry.obj:     $(MSWDIR)\registry.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\snglinst.obj:     $(MSWDIR)\snglinst.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\thread.obj:     $(MSWDIR)\thread.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\utils.obj:     $(MSWDIR)\utils.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\utilsexc.obj:     $(MSWDIR)\utilsexc.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\volume.obj:     $(MSWDIR)\volume.cpp
  *$(CXX) $(CXXFLAGS) $<



########################################################
# Common objects (always compiled)

$(OUTPUTDIR)\appcmn.obj:     $(COMMDIR)\appcmn.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\clntdata.obj:     $(COMMDIR)\clntdata.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\cmdline.obj:     $(COMMDIR)\cmdline.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\config.obj:     $(COMMDIR)\config.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\datetime.obj:     $(COMMDIR)\datetime.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\datstrm.obj:     $(COMMDIR)\datstrm.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\db.obj:     $(COMMDIR)\db.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\dbtable.obj:     $(COMMDIR)\dbtable.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\dircmn.obj:     $(COMMDIR)\dircmn.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\dynarray.obj:     $(COMMDIR)\dynarray.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\dynlib.obj:     $(COMMDIR)\dynlib.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\dynload.obj:     $(COMMDIR)\dynload.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\encconv.obj:     $(COMMDIR)\encconv.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\event.obj:     $(COMMDIR)\event.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\extended.obj:     $(COMMDIR)\extended.c
  *$(CC) $(CFLAGS) $<

$(OUTPUTDIR)\ffile.obj:     $(COMMDIR)\ffile.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\file.obj:     $(COMMDIR)\file.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\fileconf.obj:     $(COMMDIR)\fileconf.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\filefn.obj:     $(COMMDIR)\filefn.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\filename.obj:     $(COMMDIR)\filename.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\filesys.obj:     $(COMMDIR)\filesys.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\fontmap.obj:     $(COMMDIR)\fontmap.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\fs_inet.obj:     $(COMMDIR)\fs_inet.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\fs_mem.obj:     $(COMMDIR)\fs_mem.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\fs_zip.obj:     $(COMMDIR)\fs_zip.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\ftp.obj:     $(COMMDIR)\ftp.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\hash.obj:     $(COMMDIR)\hash.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\hashmap.obj:     $(COMMDIR)\hashmap.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\http.obj:     $(COMMDIR)\http.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\intl.obj:     $(COMMDIR)\intl.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\ipcbase.obj:     $(COMMDIR)\ipcbase.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\list.obj:     $(COMMDIR)\list.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\log.obj:     $(COMMDIR)\log.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\longlong.obj:     $(COMMDIR)\longlong.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\memory.obj:     $(COMMDIR)\memory.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\mimecmn.obj:     $(COMMDIR)\mimecmn.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\module.obj:     $(COMMDIR)\module.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\msgout.obj:     $(COMMDIR)\msgout.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\mstream.obj:     $(COMMDIR)\mstream.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\object.obj:     $(COMMDIR)\object.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\process.obj:     $(COMMDIR)\process.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\protocol.obj:     $(COMMDIR)\protocol.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\regex.obj:     $(COMMDIR)\regex.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\sckaddr.obj:     $(COMMDIR)\sckaddr.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\sckfile.obj:     $(COMMDIR)\sckfile.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\sckipc.obj:     $(COMMDIR)\sckipc.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\sckstrm.obj:     $(COMMDIR)\sckstrm.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\socket.obj:     $(COMMDIR)\socket.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\strconv.obj:     $(COMMDIR)\strconv.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\stream.obj:     $(COMMDIR)\stream.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\string.obj:     $(COMMDIR)\string.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\sysopt.obj:     $(COMMDIR)\sysopt.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\textbuf.obj:     $(COMMDIR)\textbuf.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\textfile.obj:     $(COMMDIR)\textfile.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\timercmn.obj:     $(COMMDIR)\timercmn.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\tokenzr.obj:     $(COMMDIR)\tokenzr.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\txtstrm.obj:     $(COMMDIR)\txtstrm.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\unzip.obj:     $(COMMDIR)\unzip.c
  *$(CC) $(CFLAGS) $<

$(OUTPUTDIR)\url.obj:     $(COMMDIR)\url.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\utilscmn.obj:     $(COMMDIR)\utilscmn.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\variant.obj:     $(COMMDIR)\variant.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\wfstream.obj:     $(COMMDIR)\wfstream.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\wxchar.obj:     $(COMMDIR)\wxchar.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\zipstrm.obj:     $(COMMDIR)\zipstrm.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\zstream.obj:     $(COMMDIR)\zstream.cpp
  *$(CXX) $(CXXFLAGS) $<

$(OUTPUTDIR)\init.obj:     $(COMMDIR)\init.cpp
  *$(CXX) $(CXXFLAGS) $<



zlib:   .SYMBOLIC
    cd $(WXDIR)\src\zlib
    wmake -f makefile.wat all
    cd $(WXDIR)\src\msw

clean_zlib:   .SYMBOLIC
    cd $(WXDIR)\src\zlib
    wmake -f makefile.wat clean
    cd $(WXDIR)\src\msw

regex:    .SYMBOLIC
    cd $(WXDIR)\src\regex
    wmake -f makefile.wat all
    cd $(WXDIR)\src\msw

clean_regex:   .SYMBOLIC
    cd $(WXDIR)\src\regex
    wmake -f makefile.wat clean
    cd $(WXDIR)\src\msw

MFTYPE=watbase
self : .SYMBOLIC $(WXDIR)\distrib\msw\tmake\filelist.txt $(WXDIR)\distrib\msw\tmake\$(MFTYPE).t
	cd $(WXDIR)\distrib\msw\tmake
	tmake -t $(MFTYPE) wxwin.pro -o makebase.wat
	copy makebase.wat $(WXDIR)\src\msw
