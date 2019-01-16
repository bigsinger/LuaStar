# star.file
```lua
star.file(string cmd, string filepath)
```

## 参数cmd
指示要获取什么路径，有以下选项：
- size: 获取文件大小，返回值3个：低32bit 高32bit 64bit
- read: 返回全部文件内容，适合比较小的纯文本文件
- cp: 复制一个文件为另一个文件（参数3），参数4表示是否覆盖（默认false）
- exist： 判断文件或路径是否存在，boolean
- md5:  获取文件MD5值字符串（默认全大写）
- time: 获取文件时间，返回：创建时间，修改时间，访问时间。10位整数，单位s，例如：1537789456
- rn: 重命名文件，参数3为新文件名。
- verify:签名校验文件，返回整数：1表示系统文件，2表示签名文件
- attr:返回整数（GetFileAttributes的返回值）
- delete: 删除文件，成功返回true，失败返回false
- deldir: 递归删除目录（目录路径长度小于5的不执行删除操作），成功返回true，失败返回false
- desc: 获取文件详情信息，返回值5个：文件描述，原始文件名，产品版本号，文件版本号，版权信息
- select: 打开资源管理器并在文件夹中定位文件
- ls：不递归列举目录下的文件。如果目录下没有文件返回nil，否则返回一个table，可以根据索引遍历。

## 参数 filepath
文件全路径


# star.read
返回全部文件内容，适合比较小的纯文本文件。
```lua
s = star.read(string filepath)
```
等效于：

```lua
s = star.file('read', string filepath)
```