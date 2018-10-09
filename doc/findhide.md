# findhide 查找隐藏模块/隐藏代码

获取两个时间的差：
```lua
star.findhide('', pid)
```

调用示例：
```lua
pid = star.getpid('hypprotect.exe') t = star.findhide('', pid)
```

输出示例：
```lua
start find abnormal mem page( enum abnormal VirtualQueryEx )
find nothing abnormal!

start find abnormal page: ZwQueryVirtualMemory diff VirtualQueryEx
start find abnormal page: MEM_IMAGE without Image Path
find nothing abnormal!


start find memory page: EXECUTE without MEM_IMAGE(ZwQueryVirtualMemory)
base:0000000000BD0000 Abase:0000000000BD0000 size:00042000 t:00040000 p:00000040        8B55
base:0000000010001000 Abase:0000000010000000 size:00021000 t:00020000 p:00000020        find MZ!

start find memory page: EXECUTE without MEM_IMAGE(VirtualQueryEx)
base:0000000000BD0000 Abase:0000000000BD0000 size:00042000 t:00040000 p:00000040        8B55
base:0000000010001000 Abase:0000000010000000 size:00021000 t:00020000 p:00000020        find MZ!

start find abnormal thread
tid: 5916       addr: 00000000002DF57E base: 00000000002D0000 file: E:\xxx\game\xxx.exe
tid: 2504       addr: 0000000077364BF0 base: 0000000077330000 file: C:\windows\SYSTEM32\ntdll.dll
tid: 4256       addr: 0000000077364BF0 base: 0000000077330000 file: C:\windows\SYSTEM32\ntdll.dll
tid: 5896       addr: 0000000077364BF0 base: 0000000077330000 file: C:\windows\SYSTEM32\ntdll.dll
tid: 2768       addr: 0000000077364BF0 base: 0000000077330000 file: C:\windows\SYSTEM32\ntdll.dll
tid: 372        addr: 000000001000AE35 base: 0000000010000000 file:     hidden thread! file:
tid: 3032       addr: 000000001000AE1D base: 0000000010000000 file:     hidden thread! file:
tid: 4092       addr: 000000006EF3ED90 base: 000000006EE10000 file: C:\windows\system32\d3d9.dll
tid: 5308       addr: 0000000010005900 base: 0000000010000000 file:     hidden thread! file:
tid: 4272       addr: 000000001000AE11 base: 0000000010000000 file:     hidden thread! file:
tid: 888        addr: 000000006DE4305D base: 000000006D470000 file: C:\windows\System32\DriverStore\FileRepository\nv_dispi.inf_amd64_52ac7eb8f32780d5\nvd3dum.dll

DOS_STUB find begin
find DOS_STUB! maybe hidden code, base addr: 0000000010000000 (align size: 00001000) file:

DOS_STUB find end
```

通过以上几种方法可以看出，模块0000000010000000为隐藏的代码。