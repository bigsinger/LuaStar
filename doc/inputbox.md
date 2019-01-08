# star.inputbox

获取用户输入，支持拖放文件，如果是拖放文件则返回两个结果，第一个为拖放的第一个文件全路径，第二个为table，包含所有拖放进来的文件。

## 原型

```lua
s,[t] = star.inputbox([string tip], [string defaultValue])
```

## 参数
- 提示文本，可选
- 默认输入值，可选

## 示例

```lua
s = star.inputbox('请输入数值：', '100')
```

获取拖放的文件路径：
```lua
f,t = star.inputbox()
print(f)
if t then
	for k,v in pairs(t) do
		print(v)
	end
end
```