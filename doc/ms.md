# 枚举模块列表
```lua
t = star.ms(integer pid, [string cmdtype])
```
pid为-1时获取本进程的模块信息。


cmdtype可选字符串：
- sys: 获取内核模块
- hide: 获取隐藏模块，仅仅是隐藏模块，隐藏代码请使用[findhide 查找隐藏模块/隐藏代码](./findhide.md)
- sus:	获取可疑模块，策略：模块路径有问题，时间有问题。 技术实现：TH32CS_SNAPMODULE ModuleFirst  ModuleNext
- 不填或其他：R3模块列表

## 获取模块列表
```lua
pid = star.getpid('notepad.exe') t = star.ms(pid) for k, v in pairs(t) do print(v) end
```

## 获取内核模块
```lua
pid = star.getpid('notepad.exe')
t = star.ms(pid, 'sys')
```

一行脚本获取当前进程的内核模块：
```lua
t = star.ms(-1, 'sys') for k, v in pairs(t) do if star.file('exist', v) then print(v) else print(v, 'not exist') end end
```

输出示例：
```none
C:\Windows\system32\ntoskrnl.exe
C:\Windows\system32\hal.dll
C:\Windows\system32\kdcom.dll
C:\Windows\system32\mcupdate_genuineintel.dll
C:\Windows\system32\pshed.dll
C:\Windows\system32\clfs.sys
C:\Windows\system32\ci.dll
C:\Windows\system32\drivers\wdf01000.sys
C:\Windows\system32\drivers\wdfldr.sys
C:\Windows\system32\drivers\acpi.sys
C:\Windows\system32\drivers\wmilib.sys
C:\Windows\system32\drivers\msisadrv.sys
C:\Windows\system32\drivers\pci.sys
C:\Windows\system32\drivers\vdrvroot.sys
C:\Windows\system32\drivers\partmgr.sys
C:\Windows\system32\drivers\compbatt.sys
C:\Windows\system32\drivers\battc.sys
C:\Windows\system32\drivers\volmgr.sys
C:\Windows\system32\drivers\volmgrx.sys
C:\Windows\system32\drivers\intelide.sys
C:\Windows\system32\drivers\pciidex.sys
C:\Windows\system32\drivers\vmci.sys
C:\Windows\system32\drivers\vsock.sys
C:\Windows\system32\drivers\mountmgr.sys
C:\Windows\system32\drivers\atapi.sys
C:\Windows\system32\drivers\ataport.sys
C:\Windows\system32\drivers\lsi_sas.sys
C:\Windows\system32\drivers\storport.sys
C:\Windows\system32\drivers\msahci.sys
C:\Windows\system32\drivers\amdxata.sys
C:\Windows\system32\drivers\fltmgr.sys
C:\Windows\system32\drivers\fileinfo.sys
C:\Windows\system32\drivers\ntfs.sys
C:\Windows\system32\drivers\msrpc.sys
C:\Windows\system32\drivers\ksecdd.sys
C:\Windows\system32\drivers\cng.sys
C:\Windows\system32\drivers\pcw.sys
C:\Windows\system32\drivers\fs_rec.sys
C:\Windows\system32\drivers\ndis.sys
C:\Windows\system32\drivers\netio.sys
C:\Windows\system32\drivers\ksecpkg.sys
C:\Windows\system32\drivers\tcpip.sys
C:\Windows\system32\drivers\fwpkclnt.sys
C:\Windows\system32\drivers\vmstorfl.sys
C:\Windows\system32\drivers\volsnap.sys
C:\Windows\system32\drivers\spldr.sys
C:\Windows\system32\drivers\rdyboost.sys
C:\Windows\system32\drivers\mup.sys
C:\Windows\system32\drivers\hwpolicy.sys
C:\Windows\system32\drivers\fvevol.sys
C:\Windows\system32\drivers\disk.sys
C:\Windows\system32\drivers\classpnp.sys
C:\Windows\system32\drivers\cdrom.sys
C:\Windows\system32\drivers\null.sys
C:\Windows\system32\drivers\beep.sys
C:\Windows\system32\drivers\vmrawdsk.sys
C:\Windows\system32\drivers\vga.sys
C:\Windows\system32\drivers\videoprt.sys
C:\Windows\system32\drivers\watchdog.sys
C:\Windows\system32\drivers\rdpcdd.sys
C:\Windows\system32\drivers\rdpencdd.sys
C:\Windows\system32\drivers\rdprefmp.sys
C:\Windows\system32\drivers\msfs.sys
C:\Windows\system32\drivers\npfs.sys
C:\Windows\system32\drivers\tdx.sys
C:\Windows\system32\drivers\tdi.sys
C:\Windows\system32\drivers\afd.sys
C:\Windows\system32\drivers\netbt.sys
C:\Windows\system32\drivers\ws2ifsl.sys
C:\Windows\system32\drivers\wfplwf.sys
C:\Windows\system32\drivers\pacer.sys
C:\Windows\system32\drivers\netbios.sys
C:\Windows\system32\drivers\serial.sys
C:\Windows\system32\drivers\wanarp.sys
C:\Windows\system32\drivers\termdd.sys
C:\Windows\system32\drivers\rdbss.sys
C:\Windows\system32\drivers\nsiproxy.sys
C:\Windows\system32\drivers\mssmbios.sys
C:\Windows\system32\drivers\discache.sys
C:\Windows\system32\drivers\csc.sys
C:\Windows\system32\drivers\dfsc.sys
C:\Windows\system32\drivers\blbdrive.sys
C:\Windows\system32\drivers\tunnel.sys
C:\Windows\system32\drivers\i8042prt.sys
C:\Windows\system32\drivers\kbdclass.sys
C:\Windows\system32\drivers\vmmouse.sys
C:\Windows\system32\drivers\mouclass.sys
C:\Windows\system32\drivers\serenum.sys
C:\Windows\system32\drivers\vm3dmp_loader.sys
C:\Windows\system32\drivers\vm3dmp.sys
C:\Windows\system32\drivers\dxgkrnl.sys
C:\Windows\system32\drivers\dxgmms1.sys
C:\Windows\system32\drivers\usbuhci.sys
C:\Windows\system32\drivers\usbport.sys
C:\Windows\system32\drivers\e1g6032e.sys
C:\Windows\system32\drivers\hdaudbus.sys
C:\Windows\system32\drivers\usbehci.sys
C:\Windows\system32\drivers\cmbatt.sys
C:\Windows\system32\drivers\intelppm.sys
C:\Windows\system32\drivers\compositebus.sys
C:\Windows\system32\drivers\agilevpn.sys
C:\Windows\system32\drivers\rasl2tp.sys
C:\Windows\system32\drivers\ndistapi.sys
C:\Windows\system32\drivers\ndiswan.sys
C:\Windows\system32\drivers\raspppoe.sys
C:\Windows\system32\drivers\raspptp.sys
C:\Windows\system32\drivers\rassstp.sys
C:\Windows\system32\drivers\rdpbus.sys
C:\Windows\system32\drivers\swenum.sys
C:\Windows\system32\drivers\ks.sys
C:\Windows\system32\drivers\umbus.sys
C:\Windows\system32\drivers\usbhub.sys
C:\Windows\system32\drivers\ndproxy.sys
C:\Windows\system32\drivers\hdaudio.sys
C:\Windows\system32\drivers\portcls.sys
C:\Windows\system32\drivers\drmk.sys
C:\Windows\system32\drivers\ksthunk.sys
C:\Windows\system32\drivers\crashdmp.sys
C:\Windows\system32\drivers\dump_diskdump.sys   not exist
C:\Windows\system32\drivers\dump_lsi_sas.sys    not exist
C:\Windows\system32\drivers\dump_dumpfve.sys    not exist
C:\Windows\system32\win32k.sys
C:\Windows\system32\drivers\dxapi.sys
C:\Windows\system32\drivers\monitor.sys
C:\Windows\system32\tsddd.dll
C:\Windows\system32\cdd.dll
C:\Windows\system32\drivers\luafv.sys
C:\Windows\system32\drivers\usbccgp.sys
C:\Windows\system32\drivers\usbd.sys
C:\Windows\system32\drivers\hidusb.sys
C:\Windows\system32\drivers\hidclass.sys
C:\Windows\system32\drivers\hidparse.sys
C:\Windows\system32\drivers\mouhid.sys
C:\Windows\system32\drivers\vmusbmouse.sys
C:\Windows\system32\drivers\lltdio.sys
C:\Windows\system32\drivers\rspndr.sys
C:\Windows\system32\drivers\http.sys
C:\Windows\system32\drivers\bowser.sys
C:\Windows\system32\drivers\mpsdrv.sys
C:\Windows\system32\drivers\mrxsmb.sys
C:\Windows\system32\drivers\mrxsmb10.sys
C:\Windows\system32\drivers\mrxsmb20.sys
C:\Windows\system32\drivers\vmmemctl.sys
C:\Windows\system32\drivers\peauth.sys
C:\Windows\system32\drivers\secdrv.sys
C:\Windows\system32\drivers\srvnet.sys
C:\Windows\system32\drivers\tcpipreg.sys
C:\Windows\system32\drivers\srv2.sys
C:\Windows\system32\drivers\srv.sys
C:\Windows\system32\drivers\vmhgfs.sys
C:\Windows\system32\drivers\spsys.sys
c:\program files\tools\hidetoolz\zljkkx.sys     not exist
c:\users\js\desktop\pchunter64ar.sys    not exist
C:\Windows\system32\ntdll.dll
C:\Windows\system32\smss.exe
C:\Windows\system32\apisetschema.dll
C:\Windows\system32\autochk.exe
C:\Windows\system32\normaliz.dll
C:\Windows\system32\usp10.dll
C:\Windows\system32\imm32.dll
C:\Windows\system32\wldap32.dll
C:\Windows\system32\comdlg32.dll
C:\Windows\system32\psapi.dll
C:\Windows\system32\difxapi.dll
C:\Windows\system32\shell32.dll
C:\Windows\system32\msctf.dll
C:\Windows\system32\kernel32.dll
C:\Windows\system32\lpk.dll
C:\Windows\system32\ole32.dll
C:\Windows\system32\nsi.dll
C:\Windows\system32\iertutil.dll
C:\Windows\system32\oleaut32.dll
C:\Windows\system32\setupapi.dll
C:\Windows\system32\ws2_32.dll
C:\Windows\system32\imagehlp.dll
C:\Windows\system32\rpcrt4.dll
C:\Windows\system32\user32.dll
C:\Windows\system32\wininet.dll
C:\Windows\system32\sechost.dll
C:\Windows\system32\advapi32.dll
C:\Windows\system32\msvcrt.dll
C:\Windows\system32\urlmon.dll
C:\Windows\system32\clbcatq.dll
C:\Windows\system32\shlwapi.dll
C:\Windows\system32\gdi32.dll
C:\Windows\system32\comctl32.dll
C:\Windows\system32\devobj.dll
C:\Windows\system32\kernelbase.dll
C:\Windows\system32\crypt32.dll
C:\Windows\system32\cfgmgr32.dll
C:\Windows\system32\wintrust.dll
C:\Windows\system32\msasn1.dll
C:\Windows\syswow64\normaliz.dll
```

测试发现这几个好像电脑上均有：
```
C:\Windows\system32\drivers\dump_diskdump.sys   not exist
C:\Windows\system32\drivers\dump_lsi_sas.sys    not exist
C:\Windows\system32\drivers\dump_iastora.sys    not exist
C:\Windows\system32\drivers\dump_dumpfve.sys    not exist
```

但是以下的驱动就比较明显了：
```
c:\program files\tools\hidetoolz\zljkkx.sys     not exist
c:\users\js\desktop\pchunter64ar.sys	not exist
```