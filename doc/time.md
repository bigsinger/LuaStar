# os.time

获取两个时间的差：
```lua
t1 = os.time() s = os.difftime(t1, tw) h = s/(60*60) m = s/60%60 S = s%60 print(s,h,m,S)
```