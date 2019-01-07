# 注册表

## 读取注册表字符串
- **getregstr**
- **getregstrexpand**
- **getregstrmulti**

例如读取关于Python路径的注册表字符串：
```lua
s = star.getregstr([[HKEY_LOCAL_MACHINE\SOFTWARE\Wow6432Node\Microsoft\Windows\CurrentVersion\App Paths\Python.exe]]) print(s)
s = star.getregstr([[HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\Python.exe]]) print(s)

s = star.getregstr([[HKEY_CLASSES_ROOT\Python.File\Shell\editwithidle\shell\edit37\command]]) print(s)
s = star.getregstr([[HKEY_CLASSES_ROOT\Python.NoConFile\Shell\editwithidle\shell\edit37\command]]) print(s)
s = star.getregstr([[HKEY_LOCAL_MACHINE\SOFTWARE\Classes\Python.File\Shell\editwithidle\shell\edit37\command]]) print(s)
s = star.getregstr([[HKEY_LOCAL_MACHINE\SOFTWARE\Classes\Python.NoConFile\Shell\editwithidle\shell\edit37\command
]]) print(s)
```

读取系统环境变量的替代方案：
```lua
s = star.getregstrexpand([[HKEY_LOCAL_MACHINE\SYSTEM\ControlSet001\Control\Session Manager\Environment]], 'Path') print(s)
s = star.getregstrexpand([[HKEY_LOCAL_MACHINE\SYSTEM\ControlSet002\Control\Session Manager\Environment]], 'Path') print(s)
s = star.getregstrexpand([[HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\Session Manager\Environment]], 'Path') print(s)
```

等价于：
```lua
print(os.getenv('PATH'))
```

## 读取注册表数值
- **getregint**

```lua
d = star.getregint([[HKEY_CURRENT_USER\Software\Local AppWizard-Generated Applications\xxxsoft\main]], 'keyname') print(d)
```