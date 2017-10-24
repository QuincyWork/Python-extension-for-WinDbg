@set CL=/D "WIN32" /D "_WINDOWS" /D "_USRDLL" /D "PYKD_EXPORTS" /D "_WINDLL" /D "_UNICODE" /D "UNICODE"
@set CL=%CL% /nologo /EHsc /LD /MD /W3 /D "NDEBUG" /Ox /I%BOOST_ROOT% /I%PYTHON_ROOT%\include /I%DBG_SDK_ROOT%\inc
@rc /fo resource.res pykd.rc > nul
@cvtres /out:resource.obj /nologo /machine:x86 resource.res
@cl pykd.cpp       ^
    dbgcmd.cpp     ^
    dbgdump.cpp    ^
    dbgeventcb.cpp ^
    dbgext.cpp     ^
    dbgmem.cpp     ^
    dbgmodule.cpp  ^
    dbgpath.cpp    ^
    dbgprint.cpp   ^
    dbgprocess.cpp ^
    dbgreg.cpp     ^
    dbgsession.cpp ^
    dbgsym.cpp     ^
    dbgsynsym.cpp  ^
    dbgsystem.cpp  ^
    dbgtype.cpp    ^
    stdafx.cpp     ^
    pykd.def       ^
    %DBG_SDK_ROOT%\lib\i386\dbgeng.lib ^
    /link /out:pykd.pyd
@mt -nologo -manifest pykd.pyd.manifest  -outputresource:pykd.pyd;1
@if exist *.obj del *.obj
@if exist *.res del *.res
@if exist *.lib del *.lib
@if exist *.exp del *.exp
@if exist *.manifest del *.manifest
