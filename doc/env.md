# 获取系统环境变量
可以使用Lua自带的函数：**os.getenv**，例如：
```lua
print(os.getenv('PATH'))
```

# 添加路径到系统环境变量
- **star.addenv(string dirpath)**

例如添加Python安装目录到环境变量中：
```lua
star.addenv([[D:\Python\Python37]]) star.addenv([[D:\Python\Python37\Scripts]])
```
