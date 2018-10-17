# 查找某游戏可疑外挂
```lua
function check()
	t = star.getwindows() for k,v in pairs(t) do if v.visible and v.width > 1000 and v.height > 900 then print(string.format('%X',v.hwnd), v.left, v.top, v.width, v.height, v.pid, v.text, v.class) end end
	pid = star.getpid('game.exe')
	if pid <= 0 then return end
	 print('\nfind sus dll:\n')
	t = star.ms(pid, 'sus') if t then for k, v in pairs(t) do print(v) end else print('not found') end
	star.d3d('check', pid)
	 print('\nfind hide dll:\n')
	t = star.ms(pid, 'hide')
	 print('\nfind hide dll and code:\n')
	t = star.findhide(pid, '', true)
	 print('\nlist modules:\n')
	t = star.ms(pid, '', true) for k, v in pairs(t) do print(v) end print('')
end
check()
```

