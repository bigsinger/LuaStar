# 枚举窗口列表
```lua
t = star.getwindows()
```
返回的表元素字段：
- hwnd: 	窗口句柄
- visible: 	是否可见
- left: 	窗口坐标
- top: 		窗口坐标
- width: 	窗口尺寸宽
- height: 	窗口尺寸高
- pid: 		窗口所在进程PID
- class: 	窗口类名
- text: 	窗口文本


一行脚本获取所有尺寸在1000以上的可见窗口：
```lua
t = star.getwindows() for k,v in pairs(t) do if v.visible and v.width > 1000 and v.height > 1000 then print(v.hwnd, v.left, v.top, v.width, v.height, v.pid, v.text, v.class) end end
```

输出示例：
```none
```
